// Copyright 2025 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.github.bazelbuild.rules_jvm_external.resolver.gradle;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.Artifact;
import com.github.bazelbuild.rules_jvm_external.resolver.Conflict;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolutionRequest;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolutionResult;
import com.github.bazelbuild.rules_jvm_external.resolver.Resolver;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.events.LogEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.events.PhaseEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.Exclusion;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.ExclusionImpl;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleCoordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleCoordinatesImpl;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleDependency;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleDependencyImpl;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleDependencyModel;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleResolvedArtifact;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleResolvedDependency;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleUnresolvedDependency;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import com.google.common.graph.GraphBuilder;
import com.google.common.graph.MutableGraph;
import com.google.common.hash.Hashing;
import com.google.devtools.build.runfiles.AutoBazelRepository;
import com.google.devtools.build.runfiles.Runfiles;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Instant;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Collectors;
import org.apache.maven.model.Model;
import org.apache.maven.model.Relocation;
import org.apache.maven.model.io.xpp3.MavenXpp3Reader;
import org.codehaus.plexus.util.ReaderFactory;
import org.codehaus.plexus.util.xml.pull.XmlPullParserException;

/** The implementation for the Gradle resolver */
@AutoBazelRepository
public class GradleResolver implements Resolver {

  private final EventListener eventListener;
  private final Netrc netrc;
  private final int maxThreads;

  public GradleResolver(Netrc netrc, int maxThreads, EventListener eventListener) {
    this.netrc = netrc;
    this.eventListener = eventListener;
    this.maxThreads = maxThreads;
  }

  public String getName() {
    return "gradle";
  }

  private boolean isVerbose() {
    return System.getenv("RJE_VERBOSE") != null;
  }

  @Override
  public ResolutionResult resolve(ResolutionRequest request) {
    List<Repository> repositories =
        request.getRepositories().stream().map(this::createRepository).collect(Collectors.toList());
    List<GradleDependency> dependencies =
        request.getDependencies().stream().map(this::createDependency).collect(Collectors.toList());
    List<GradleDependency> boms =
        request.getBoms().stream().map(this::createDependency).collect(Collectors.toList());

    Path gradlePath = getGradleInstallationPath();
    try (GradleProject project =
        setupFakeGradleProject(
            repositories,
            dependencies,
            boms,
            request.getGlobalExclusions(),
            request.isUseUnsafeSharedCache(),
            request.isUsingM2Local())) {
      project.setupProject();
      eventListener.onEvent(new PhaseEvent("Gathering dependencies"));
      project.connect(gradlePath);
      if (isVerbose()) {
        eventListener.onEvent(
            new LogEvent(
                "gradle",
                "Resolving dependencies with Gradle",
                "Gradle Project Directory: " + project.getProjectDir().toUri()));
      }
      Instant start = Instant.now();
      GradleDependencyModel resolved =
          project.resolveDependencies(
              getGradleTaskProperties(repositories, project.getProjectDir()));
      Instant end = Instant.now();
      if (isVerbose()) {
        System.out.println(
            "Resolved dependencies and artifacts with Gradle resolver in "
                + (end.toEpochMilli() - start.toEpochMilli())
                + " ms");
      }
      start = Instant.now();
      ResolutionResult result = parseDependencies(dependencies, resolved);
      end = Instant.now();

      if (isVerbose()) {
        System.err.println(
            "Building dependency graph took "
                + (end.toEpochMilli() - start.toEpochMilli())
                + " ms");
      }
      return result;
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  private Map<String, String> getGradleTaskProperties(
      List<Repository> repositories, Path projectDir) throws MalformedURLException {
    Map<String, String> properties = new HashMap<>();
    for (Repository repository : repositories) {
      if (repository.requiresAuth) {
        properties.put(repository.usernameProperty, repository.getUsername());
        properties.put(repository.passwordProperty, repository.getPassword());
      }
    }

    if (isVerbose()) {
      properties.put("org.gradle.debug", "true");
    }
    return properties;
  }

  private ResolutionResult parseDependencies(
      List<GradleDependency> requestedDeps, GradleDependencyModel resolved)
      throws GradleDependencyResolutionException {
    MutableGraph<Coordinates> graph = GraphBuilder.directed().allowsSelfLoops(true).build();

    // Create lookup cache for requested dependencies
    Set<String> requestedDepKeys =
        requestedDeps.stream()
            .map(dep -> makeDepKey(dep.getGroup(), dep.getArtifact(), dep.getVersion()))
            .collect(Collectors.toSet());

    Set<Conflict> conflicts = new HashSet<>();
    Map<Coordinates, String> coordinateHashes = new HashMap<>();
    // Track artifacts per node so we can inspect POM files for relocation later
    Map<Coordinates, List<GradleResolvedArtifact>> artifactsByNode = new HashMap<>();
    Map<Coordinates, Path> paths = new HashMap<>();
    List<GradleResolvedDependency> implementationDependencies = resolved.getResolvedDependencies();
    List<GradleUnresolvedDependency> unresolvedDependencies = resolved.getUnresolvedDependencies();
    if (implementationDependencies == null) {
      return new ResolutionResult(graph, Set.of(), Map.of());
    }

    for (GradleResolvedDependency dependency : implementationDependencies) {
      Set<Coordinates> visited = new HashSet<>();
      for (GradleResolvedArtifact artifact : dependency.getArtifacts()) {
        GradleCoordinates gradleCoordinates =
            new GradleCoordinatesImpl(
                dependency.getGroup(),
                dependency.getName(),
                dependency.getVersion(),
                artifact.getClassifier(),
                artifact.getExtension());
        String extension = gradleCoordinates.getExtension();
        String classifier = gradleCoordinates.getClassifier();
        // POM artifacts should not generate their own node; coerce to JAR for node identity
        if ("pom".equals(extension)) {
          extension = null; // Coordinates() will default to jar
        }
        Coordinates coordinates =
            new Coordinates(
                gradleCoordinates.getGroupId(),
                gradleCoordinates.getArtifactId(),
                extension,
                classifier,
                gradleCoordinates.getVersion());
        // Track artifact for this node
        artifactsByNode.computeIfAbsent(coordinates, k -> new ArrayList<>()).add(artifact);

        File artifactFile = artifact.getFile();
        if (artifactFile != null && artifactFile.exists()) {
          paths.put(coordinates, artifactFile.toPath());
        }

        addDependency(
            graph, coordinates, dependency, conflicts, requestedDepKeys, visited, artifactsByNode);
        // if there's a conflict and the conflicting version isn't one that's actually requested
        // then it's an actual conflict we want to report
        if (dependency.isConflict()
            && !requestedDepKeys.contains(
                makeDepKey(dependency.getGroup(), dependency.getName(), dependency.getVersion()))) {
          String conflictingVersion =
              dependency.getRequestedVersions().stream()
                  .filter(x -> !x.equals(coordinates.getVersion()))
                  .findFirst()
                  .get();
          GradleCoordinates requestedCoordinates =
              new GradleCoordinatesImpl(
                  dependency.getGroup(),
                  dependency.getName(),
                  conflictingVersion,
                  artifact.getClassifier(),
                  artifact.getExtension());
          Coordinates requested =
              new Coordinates(
                  requestedCoordinates.getGroupId(),
                  requestedCoordinates.getArtifactId(),
                  requestedCoordinates.getExtension(),
                  requestedCoordinates.getClassifier(),
                  requestedCoordinates.getVersion());
          conflicts.add(new Conflict(coordinates, requested));
        }
      }
    }

    // Capture the set of successfully resolved group:artifact pairs BEFORE adding unresolved
    // dependencies to the graph. This allows us to determine if an "unresolved" dependency
    // was actually resolved at a different version (e.g., due to BOM constraints or version
    // conflict resolution).
    Set<String> resolvedGroupArtifacts =
        graph.nodes().stream()
            .map(c -> c.getGroupId() + ":" + c.getArtifactId())
            .collect(Collectors.toSet());

    for (GradleUnresolvedDependency dependency : unresolvedDependencies) {
      Coordinates coordinates =
          new Coordinates(
              dependency.getGroup() + ":" + dependency.getName() + ":" + dependency.getVersion());

      if (dependency.getFailureReason() == GradleUnresolvedDependency.FailureReason.NOT_FOUND) {
        Coordinates displayable = coordinates.setExtension("pom");
        String message =
            "The POM for " + displayable + " is missing, no dependency information available.";
        String detail = "[WARNING]:    " + dependency.getFailureDetails();
        eventListener.onEvent(new LogEvent("gradle", message, detail));
      } else {
        String message = "Could not resolve " + coordinates;
        String detail = "[WARNING]: " + dependency.getFailureDetails();
        eventListener.onEvent(new LogEvent("gradle", message, detail));
      }
      graph.addNode(coordinates);
    }

    // If any of the deps we requested failed to resolve, we should throw an exception.
    // For missing transitive deps, we only appear to log a warning in the maven, so keep that
    // behavior here as well.
    List<GradleUnresolvedDependency> unresolvedRequestedDeps =
        filterUnresolvedRequestedDeps(
            unresolvedDependencies, requestedDepKeys, resolvedGroupArtifacts);
    if (!unresolvedRequestedDeps.isEmpty()) {
      throw new GradleDependencyResolutionException(unresolvedRequestedDeps);
    }

    // After building the graph, contract relocation stubs (keep aggregating POMs)
    collapseRelocations(graph, coordinateHashes, conflicts, paths, artifactsByNode);

    // Collapse aggregating dependencies (dependencies with only classified artifacts)
    collapseAggregatingDependencies(graph, paths, artifactsByNode);

    // Populate paths for all nodes in the final graph from collected artifacts
    for (Map.Entry<Coordinates, List<GradleResolvedArtifact>> entry : artifactsByNode.entrySet()) {
      Coordinates coords = entry.getKey();
      if (!graph.nodes().contains(coords)) {
        continue; // Only process nodes in the final graph
      }

      File bestFile = null;
      // Prefer jar/aar with name matching artifactId-version to avoid picking wrong version
      for (GradleResolvedArtifact artifact : entry.getValue()) {
        File file = artifact.getFile();
        if (file == null || !file.exists()) {
          continue;
        }
        String name = file.getName();
        boolean isJarOrAar = name.endsWith(".jar") || name.endsWith(".aar");
        if (isJarOrAar && name.contains(coords.getArtifactId() + "-" + coords.getVersion())) {
          bestFile = file;
          break;
        }
      }
      // Fallback: any existing file (including pom)
      if (bestFile == null) {
        for (GradleResolvedArtifact artifact : entry.getValue()) {
          File file = artifact.getFile();
          if (file != null && file.exists()) {
            bestFile = file;
            break;
          }
        }
      }
      if (bestFile != null) {
        paths.put(coords, bestFile.toPath());
      }
    }

    // Only include paths for coordinates that are actually in the final resolved graph
    paths.keySet().retainAll(graph.nodes());

    return new ResolutionResult(graph, conflicts, paths);
  }

  private String makeDepKey(String group, String artifact, String version) {
    return group + ":" + artifact + ":" + version;
  }

  private void addDependency(
      MutableGraph<Coordinates> graph,
      Coordinates parent,
      GradleResolvedDependency parentInfo,
      Set<Conflict> conflicts,
      Set<String> requestedDepKeys,
      Set<Coordinates> visited,
      Map<Coordinates, List<GradleResolvedArtifact>> artifactsByNode) {
    if (visited.contains(parent)) {
      return;
    }
    visited.add(parent);
    graph.addNode(parent);

    if (parentInfo.getChildren() != null) {
      for (GradleResolvedDependency childInfo : parentInfo.getChildren()) {
        for (GradleResolvedArtifact childArtifact : childInfo.getArtifacts()) {
          GradleCoordinates childCoordinates =
              new GradleCoordinatesImpl(
                  childInfo.getGroup(),
                  childInfo.getName(),
                  childInfo.getVersion(),
                  childArtifact.getClassifier(),
                  childArtifact.getExtension());
          String extension = childArtifact.getExtension();
          // POM artifacts should not generate their own node; coerce to JAR for node identity
          if ("pom".equals(extension)) {
            extension = null; // Coordinates() will default to jar
          }
          Coordinates child =
              new Coordinates(
                  childCoordinates.getGroupId(),
                  childCoordinates.getArtifactId(),
                  extension,
                  childCoordinates.getClassifier(),
                  childCoordinates.getVersion());
          // Track artifact for child node
          artifactsByNode.computeIfAbsent(child, k -> new ArrayList<>()).add(childArtifact);
          graph.addNode(child);
          graph.putEdge(parent, child);
          // if there's a conflict and the conflicting version isn't one that's actually requested
          // then it's an actual conflict we want to report
          if (childInfo.isConflict()
              && !requestedDepKeys.contains(
                  makeDepKey(childInfo.getGroup(), childInfo.getName(), childInfo.getVersion()))) {
            String conflictingVersion =
                childInfo.getRequestedVersions().stream()
                    .filter(x -> !x.equals(child.getVersion()))
                    .findFirst()
                    .get();
            GradleCoordinates requestedCoordinates =
                new GradleCoordinatesImpl(
                    childInfo.getGroup(),
                    childInfo.getName(),
                    conflictingVersion,
                    childArtifact.getClassifier(),
                    childArtifact.getExtension());
            Coordinates requested =
                new Coordinates(
                    requestedCoordinates.getGroupId(),
                    requestedCoordinates.getArtifactId(),
                    requestedCoordinates.getExtension(),
                    requestedCoordinates.getClassifier(),
                    requestedCoordinates.getVersion());
            conflicts.add(new Conflict(child, requested));
          }
          addDependency(
              graph,
              child,
              childInfo,
              conflicts,
              requestedDepKeys,
              visited,
              artifactsByNode); // recursively traverse the graph
        }
      }
    }
  }

  private void collapseRelocations(
      MutableGraph<Coordinates> graph,
      Map<Coordinates, String> coordinateHashes,
      Set<Conflict> conflicts,
      Map<Coordinates, Path> paths,
      Map<Coordinates, List<GradleResolvedArtifact>> artifactsByNode) {
    List<Coordinates> toRemove = new ArrayList<>();

    for (Coordinates node : graph.nodes()) {
      List<GradleResolvedArtifact> artifacts = artifactsByNode.get(node);
      if (artifacts == null || artifacts.isEmpty()) {
        continue;
      }

      File pomFile = null;
      for (GradleResolvedArtifact a : artifacts) {
        File f = a.getFile();
        if (f != null && f.getName().endsWith(".pom")) {
          pomFile = f;
          break;
        }
      }
      if (pomFile == null) {
        continue; // no POM attached => cannot determine relocation
      }

      // Check for relocation in the POM
      Coordinates target = readRelocationTarget(pomFile, node);
      if (target == null) {
        continue; // aggregator or normal module, keep as-is
      }

      // Find the target node in the graph by matching G:A:V (ignore classifier/extension)
      // If the exact version isn't found, look for any version of the same artifact
      // since relocation means the artifact has moved regardless of version
      Coordinates targetNode = null;
      Coordinates targetNodeAnyVersion = null;

      for (Coordinates candidate : graph.nodes()) {
        if (candidate.getGroupId().equals(target.getGroupId())
            && candidate.getArtifactId().equals(target.getArtifactId())) {

          if (candidate.getVersion().equals(target.getVersion())) {
            // Exact version match - prefer non-POM node if possible
            if (targetNode == null) {
              targetNode = candidate;
            } else if (!"pom".equals(candidate.getExtension())
                && "pom".equals(targetNode.getExtension())) {
              targetNode = candidate;
            }
          } else {
            // Different version - keep track in case we need it
            if (targetNodeAnyVersion == null) {
              targetNodeAnyVersion = candidate;
            } else if (!"pom".equals(candidate.getExtension())
                && "pom".equals(targetNodeAnyVersion.getExtension())) {
              targetNodeAnyVersion = candidate;
            }
          }
        }
      }

      // If exact version not found, use any version of the target artifact
      if (targetNode == null) {
        targetNode = targetNodeAnyVersion;
      }

      if (targetNode == null) {
        // Could not find the relocation target in the graph; be conservative and skip
        continue;
      }

      // Rewire all predecessors of node to point to targetNode
      for (Coordinates pred : new HashSet<>(graph.predecessors(node))) {
        if (!pred.equals(targetNode)) {
          graph.putEdge(pred, targetNode);
        }
      }

      // Update conflicts that reference this node
      Set<Conflict> toAdd = new HashSet<>();
      Set<Conflict> toDrop = new HashSet<>();
      for (Conflict c : conflicts) {
        if (c.getResolved().equals(node)) {
          toDrop.add(c);
          toAdd.add(new Conflict(targetNode, c.getRequested()));
        } else if (c.getRequested().equals(node)) {
          toDrop.add(c);
          toAdd.add(new Conflict(c.getResolved(), targetNode));
        }
      }
      conflicts.removeAll(toDrop);
      conflicts.addAll(toAdd);

      // Remove any hash for the POM node
      coordinateHashes.remove(node);

      toRemove.add(node);
    }

    // Remove after iteration to avoid concurrent modification
    for (Coordinates n : toRemove) {
      graph.removeNode(n);
      paths.remove(n);
    }
  }

  private Coordinates readRelocationTarget(File pomFile, Coordinates fallback) {
    try (FileInputStream fis = new FileInputStream(pomFile);
        BufferedInputStream bis = new BufferedInputStream(fis)) {
      MavenXpp3Reader reader = new MavenXpp3Reader();
      Model model = reader.read(ReaderFactory.newXmlReader(bis));
      if (model.getDistributionManagement() != null) {
        Relocation relocation = model.getDistributionManagement().getRelocation();
        if (relocation != null) {
          String g =
              relocation.getGroupId() != null ? relocation.getGroupId() : fallback.getGroupId();
          String a =
              relocation.getArtifactId() != null
                  ? relocation.getArtifactId()
                  : fallback.getArtifactId();
          String v =
              relocation.getVersion() != null ? relocation.getVersion() : fallback.getVersion();
          return new Coordinates(g, a, fallback.getExtension(), fallback.getClassifier(), v);
        }
      }
    } catch (IOException | XmlPullParserException e) {
      // If parsing fails, treat as no relocation
      if (isVerbose()) {
        eventListener.onEvent(
            new LogEvent(
                "gradle", "Failed to parse POM for relocation: " + pomFile, e.getMessage()));
      }
    }
    return null;
  }

  private void collapseAggregatingDependencies(
      MutableGraph<Coordinates> graph,
      Map<Coordinates, Path> paths,
      Map<Coordinates, List<GradleResolvedArtifact>> artifactsByNode) {
    List<Coordinates> toRemove = new ArrayList<>();

    for (Coordinates node : graph.nodes()) {
      List<GradleResolvedArtifact> artifacts = artifactsByNode.get(node);
      if (artifacts == null || artifacts.isEmpty()) {
        continue;
      }

      // Check if this is an aggregating dependency:
      // A dependency is aggregating if it has only POM files (no JAR/AAR artifacts)
      // We check the actual filename since the extension field may not be reliable
      boolean hasNonPomArtifact = false;

      for (GradleResolvedArtifact artifact : artifacts) {
        File file = artifact.getFile();
        // Check the actual file name to see if it's a POM
        if (file != null && !file.getName().endsWith(".pom")) {
          hasNonPomArtifact = true;
          break;
        }
      }

      // If there are non-POM artifacts, this is not an aggregating dependency
      if (hasNonPomArtifact) {
        continue;
      }

      // This node only has POM artifacts. Check if it's an aggregating dependency.
      // An aggregating dependency is one where:
      // 1. It only has a POM file (checked above)
      // 2. Either:
      //    a) Its successors are all classified variants of the SAME artifact, OR
      //    b) There exist other nodes in the graph with the same G:A:V but with classifiers
      //       (indicating this is a base coordinate for classified variants)
      boolean isAggregating = false;
      Set<Coordinates> successors = graph.successors(node);

      if (!successors.isEmpty()) {
        // Check if all successors are classified variants of this node
        // (same group:artifact:version, but with classifiers)
        boolean allSuccessorsAreClassifiedVariants = true;
        for (Coordinates successor : successors) {
          boolean isClassifiedVariant =
              successor.getGroupId().equals(node.getGroupId())
                  && successor.getArtifactId().equals(node.getArtifactId())
                  && successor.getVersion().equals(node.getVersion())
                  && successor.getClassifier() != null
                  && !successor.getClassifier().isEmpty()
                  && !"javadoc".equals(successor.getClassifier())
                  && !"sources".equals(successor.getClassifier());
          if (!isClassifiedVariant) {
            allSuccessorsAreClassifiedVariants = false;
            break;
          }
        }

        isAggregating = allSuccessorsAreClassifiedVariants;
      } else {
        // No successors - check if there are classified variants in the graph
        for (Coordinates other : graph.nodes()) {
          if (other.getGroupId().equals(node.getGroupId())
              && other.getArtifactId().equals(node.getArtifactId())
              && other.getVersion().equals(node.getVersion())
              && other.getClassifier() != null
              && !other.getClassifier().isEmpty()
              && !"javadoc".equals(other.getClassifier())
              && !"sources".equals(other.getClassifier())) {
            isAggregating = true;
            break;
          }
        }
      }

      // Only remove if this is truly an aggregating dependency
      if (isAggregating) {
        toRemove.add(node);
      }
    }

    // Remove aggregating base coordinates after iteration
    for (Coordinates n : toRemove) {
      graph.removeNode(n);
      paths.remove(n);
    }
  }

  private Repository createRepository(URI uri) {
    Netrc.Credential credential = netrc.getCredential(uri.getHost());
    if (credential == null) {
      return new Repository(uri);
    }

    return new Repository(uri, true, credential.login(), credential.password());
  }

  private GradleDependencyImpl createDependency(Artifact artifact) {
    Coordinates coordinates = artifact.getCoordinates();
    List<Exclusion> exclusions = new ArrayList<>();
    artifact.getExclusions().stream()
        .forEach(
            exclusion -> {
              exclusions.add(new ExclusionImpl(exclusion.getGroupId(), exclusion.getArtifactId()));
            });

    // When force_version is true, use Gradle's !! shorthand which is equivalent to strictly()
    // This forces the exact version and prevents transitive dependencies from overriding it
    String version = coordinates.getVersion();
    if (artifact.isForceVersion() && version != null && !version.isEmpty()) {
      version = version + "!!";
    }

    return new GradleDependencyImpl(
        coordinates.getGroupId(),
        coordinates.getArtifactId(),
        version,
        exclusions,
        coordinates.getClassifier(),
        coordinates.getExtension());
  }

  private Path getGradleBuildScriptTemplate() throws IOException {
    try {
      Runfiles.Preloaded runfiles = Runfiles.preload();
      String gradleBuildPath =
          runfiles
              .withSourceRepository(AutoBazelRepository_GradleResolver.NAME)
              .rlocation(
                  "rules_jvm_external/private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/data/build.gradle.hbs");
      if (!Files.exists(Paths.get(gradleBuildPath))) {
        throw new IOException("Gradle build template not found at " + gradleBuildPath);
      }
      return Paths.get(gradleBuildPath);
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  private Path getGradleInitScriptTemplate() throws IOException {
    try {
      Runfiles.Preloaded runfiles = Runfiles.preload();
      String gradleBuildPath =
          runfiles
              .withSourceRepository(AutoBazelRepository_GradleResolver.NAME)
              .rlocation(
                  "rules_jvm_external/private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/data/init.gradle.hbs");
      if (!Files.exists(Paths.get(gradleBuildPath))) {
        throw new IOException("Gradle init template not found at " + gradleBuildPath);
      }
      return Paths.get(gradleBuildPath);
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  private Path getPluginJarPath() {
    try {
      Runfiles.Preloaded runfiles = Runfiles.preload();
      String pluginJarPath =
          runfiles
              .withSourceRepository(AutoBazelRepository_GradleResolver.NAME)
              .rlocation(
                  "rules_jvm_external/private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/plugin/plugin-single-jar.jar");
      if (!Files.exists(Paths.get(pluginJarPath))) {
        throw new IOException("Gradle Plugin jar not found at " + pluginJarPath);
      }
      return Paths.get(pluginJarPath);
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  private Path getGradleInstallationPath() {
    try {
      Runfiles.Preloaded runfiles = Runfiles.preload();
      String gradleReadmePath =
          runfiles
              .withSourceRepository(AutoBazelRepository_GradleResolver.NAME)
              .rlocation("gradle/gradle-bin/README");
      Path gradlePath = Paths.get(gradleReadmePath).getParent();
      if (!gradlePath.toFile().exists()) {
        throw new IllegalStateException(
            "Gradle installation path does not exist: " + gradleReadmePath);
      }
      return gradlePath;
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  private Path getPersistentGradleHomeForRepo() {
    // We check for BUILD_WORKSPACE_DIRECTORY which will be set for most usages
    // with bazel run. It won't be available with tests, so we fall back to TEST_SRCDIR
    // which will map to the root of the runfiles tree
    String workspaceRoot =
        Optional.ofNullable(System.getenv("BUILD_WORKSPACE_DIRECTORY"))
            .orElse(System.getenv("TEST_SRCDIR"));

    // If none are set, just return null so we fall back to the isolated cache
    if (workspaceRoot == null) {
      return null;
    }

    // We want gradle home to be persistent but unique for each repo under which we're running
    // so we compute a MD5 hash, similiar to Bazel's output base and use that in the persistent
    // directory nameaz
    String md5 = Hashing.md5().hashString(workspaceRoot, StandardCharsets.UTF_8).toString();

    return Paths.get(System.getProperty("java.io.tmpdir"), "rje-gradle-" + md5);
  }

  private GradleProject setupFakeGradleProject(
      List<Repository> repositories,
      List<GradleDependency> dependencies,
      List<GradleDependency> boms,
      Set<Coordinates> globalExclusions,
      boolean useUnsafeCache,
      boolean isUsingM2Local) {
    try {
      Path fakeProjectDirectory = Files.createTempDirectory("rules_jvm_external");
      Path gradleBuildScriptTemplate = getGradleBuildScriptTemplate();
      List<ExclusionImpl> exclusions =
          globalExclusions.stream()
              .map(
                  exclusion -> new ExclusionImpl(exclusion.getGroupId(), exclusion.getArtifactId()))
              .collect(Collectors.toList());
      Path outputBuildScript = fakeProjectDirectory.resolve("build.gradle");
      GradleBuildScriptGenerator.generateBuildScript(
          gradleBuildScriptTemplate,
          outputBuildScript,
          repositories,
          dependencies,
          boms,
          exclusions,
          isUsingM2Local);

      Path initScriptTemplate = getGradleInitScriptTemplate();
      Path outputInitScript = fakeProjectDirectory.resolve("init.gradle");
      GradleBuildScriptGenerator.generateInitScript(
          initScriptTemplate, outputInitScript, getPluginJarPath());

      if (isVerbose()) {
        eventListener.onEvent(
            new LogEvent(
                "gradle",
                "Gradle Build Script: (" + outputBuildScript + ")",
                Files.readString(outputBuildScript)));
      }

      Path gradleCacheDir = fakeProjectDirectory.resolve(".gradle");
      // Get a persistent directory under temp dir specific to the repo directory under which
      // we're running so that we use a gradle home that's persistent between invocations
      // to help improve performance
      Path persistentGradleHome = getPersistentGradleHomeForRepo();
      if (persistentGradleHome != null) {
        gradleCacheDir = persistentGradleHome.resolve(".gradle");
        if (isVerbose()) {
          eventListener.onEvent(
              new LogEvent(
                  "gradle",
                  "Using persistent directory for gradle home",
                  "Gradle Home: " + gradleCacheDir));
        }
      }
      Files.createDirectories(gradleCacheDir);
      if (useUnsafeCache) {
        // Instead of changing gradleCacheDir, symlink the user's caches directory
        // This avoids timing issues with gradle.user.home system property
        Path userCaches = Paths.get(System.getProperty("user.home"), ".gradle", "caches");
        if (Files.isDirectory(userCaches)) {
          try {
            Path cacheSymlink = gradleCacheDir.resolve("caches");
            Files.createSymbolicLink(cacheSymlink, userCaches);
            if (isVerbose()) {
              eventListener.onEvent(
                  new LogEvent(
                      "gradle",
                      "Using unsafe shared cache",
                      "Symlinked " + userCaches + " -> " + cacheSymlink));
            }
          } catch (IOException e) {
            // If symlinking fails, fall back to isolated cache
            if (isVerbose()) {
              eventListener.onEvent(
                  new LogEvent(
                      "gradle",
                      "Failed to create cache symlink, using isolated cache",
                      e.getMessage()));
            }
          }
        } else if (isVerbose()) {
          String reason = Files.exists(userCaches) ? "is not a directory" : "not found";
          eventListener.onEvent(
              new LogEvent(
                  "gradle",
                  "User gradle caches directory " + reason + ", using isolated cache",
                  "Expected: " + userCaches));
        }
      }

      return new GradleProject(
          fakeProjectDirectory, gradleCacheDir, null, outputInitScript, eventListener);
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }

  /**
   * Filters unresolved dependencies to find those that should cause resolution to fail.
   *
   * <p>A dependency should cause failure only if:
   *
   * <ol>
   *   <li>It was directly requested by the user (in requestedDepKeys)
   *   <li>No version of the same group:artifact was successfully resolved
   * </ol>
   *
   * <p>This handles the case where a BOM or version conflict resolution upgrades a dependency to a
   * different version. The originally-requested version may appear as "unresolved" in Gradle's
   * internal resolution, but as long as some version of the artifact was resolved, we should not
   * fail.
   *
   * @param unresolvedDependencies List of all unresolved dependencies from Gradle
   * @param requestedDepKeys Set of "group:artifact:version" strings for user-requested deps
   * @param resolvedGroupArtifacts Set of "group:artifact" strings for successfully resolved deps
   * @return List of unresolved dependencies that should cause resolution to fail
   */
  // Visible for testing
  static List<GradleUnresolvedDependency> filterUnresolvedRequestedDeps(
      List<GradleUnresolvedDependency> unresolvedDependencies,
      Set<String> requestedDepKeys,
      Set<String> resolvedGroupArtifacts) {
    return unresolvedDependencies.stream()
        .filter(
            dep ->
                requestedDepKeys.contains(
                    dep.getGroup() + ":" + dep.getName() + ":" + dep.getVersion()))
        .filter(dep -> !resolvedGroupArtifacts.contains(dep.getGroup() + ":" + dep.getName()))
        .collect(Collectors.toList());
  }
}
