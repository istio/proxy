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
import com.google.devtools.build.runfiles.AutoBazelRepository;
import com.google.devtools.build.runfiles.Runfiles;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Instant;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

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
      ResolutionResult result = parseDependencies(dependencies, resolved, boms);
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
      List<GradleDependency> requestedDeps,
      GradleDependencyModel resolved,
      List<GradleDependency> boms)
      throws IOException {
    MutableGraph<Coordinates> graph = GraphBuilder.directed().allowsSelfLoops(false).build();

    Set<Conflict> conflicts = new HashSet<>();
    List<GradleResolvedDependency> implementationDependencies = resolved.getResolvedDependencies();
    List<GradleUnresolvedDependency> unresolvedDependencies = resolved.getUnresolvedDependencies();
    if (implementationDependencies == null) {
      return new ResolutionResult(graph, null);
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
        if (extension != null && extension.equals("pom")) {
          extension = null;
        }
        String classifier = gradleCoordinates.getClassifier();
        Coordinates coordinates =
            new Coordinates(
                gradleCoordinates.getGroupId(),
                gradleCoordinates.getArtifactId(),
                extension,
                classifier,
                gradleCoordinates.getVersion());
        addDependency(graph, coordinates, dependency, conflicts, requestedDeps, visited);
        // if there's a conflict and the conflicting version isn't one that's actually requested
        // then it's an actual conflict we want to report
        if (dependency.isConflict() && !isRequestedDep(requestedDeps, dependency)) {
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

    return new ResolutionResult(graph, conflicts);
  }

  private boolean isRequestedDep(
      List<GradleDependency> requestedDeps, GradleResolvedDependency resolved) {
    return requestedDeps.stream()
        .anyMatch(
            dep ->
                dep.getArtifact().equals(resolved.getName())
                    && dep.getGroup().equals(resolved.getGroup())
                    && dep.getVersion().equals(resolved.getVersion()));
  }

  private void addDependency(
      MutableGraph<Coordinates> graph,
      Coordinates parent,
      GradleResolvedDependency parentInfo,
      Set<Conflict> conflicts,
      List<GradleDependency> requestedDeps,
      Set<Coordinates> visited) {
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
          if (extension != null && extension.equals("pom")) {
            extension = null;
          }
          Coordinates child =
              new Coordinates(
                  childCoordinates.getGroupId(),
                  childCoordinates.getArtifactId(),
                  extension,
                  childCoordinates.getClassifier(),
                  childCoordinates.getVersion());
          graph.addNode(child);
          graph.putEdge(parent, child);
          // if there's a conflict and the conflicting version isn't one that's actually requested
          // then it's an actual conflict we want to report
          if (childInfo.isConflict() && !isRequestedDep(requestedDeps, childInfo)) {
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
              requestedDeps,
              visited); // recursively traverse the graph
        }
      }
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
    return new GradleDependencyImpl(
        coordinates.getGroupId(),
        coordinates.getArtifactId(),
        coordinates.getVersion(),
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
      Files.createDirectories(gradleCacheDir);
      if (useUnsafeCache) {
        gradleCacheDir = Paths.get(System.getProperty("user.home"), ".gradle");
      }

      return new GradleProject(
          fakeProjectDirectory, gradleCacheDir, null, outputInitScript, eventListener);
    } catch (IOException e) {
      throw new RuntimeException(e);
    }
  }
}
