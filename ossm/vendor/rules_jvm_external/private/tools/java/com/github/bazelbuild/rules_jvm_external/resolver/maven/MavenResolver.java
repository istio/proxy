// Copyright 2024 The Bazel Authors. All rights reserved.
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

package com.github.bazelbuild.rules_jvm_external.resolver.maven;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.Conflict;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolutionRequest;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolutionResult;
import com.github.bazelbuild.rules_jvm_external.resolver.Resolver;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.events.LogEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.Sets;
import com.google.common.graph.Graph;
import com.google.common.graph.GraphBuilder;
import com.google.common.graph.ImmutableGraph;
import com.google.common.graph.MutableGraph;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Function;
import java.util.stream.Collectors;
import org.apache.maven.artifact.versioning.ComparableVersion;
import org.apache.maven.model.building.ModelBuildingException;
import org.apache.maven.repository.internal.MavenRepositorySystemUtils;
import org.eclipse.aether.DefaultRepositorySystemSession;
import org.eclipse.aether.RepositoryListener;
import org.eclipse.aether.RepositorySystem;
import org.eclipse.aether.RepositorySystemSession;
import org.eclipse.aether.artifact.Artifact;
import org.eclipse.aether.artifact.DefaultArtifact;
import org.eclipse.aether.collection.CollectRequest;
import org.eclipse.aether.collection.CollectResult;
import org.eclipse.aether.collection.DependencyCollectionContext;
import org.eclipse.aether.collection.DependencyCollectionException;
import org.eclipse.aether.collection.DependencyManager;
import org.eclipse.aether.connector.basic.BasicRepositoryConnectorFactory;
import org.eclipse.aether.graph.Dependency;
import org.eclipse.aether.graph.DependencyCycle;
import org.eclipse.aether.graph.DependencyNode;
import org.eclipse.aether.graph.DependencyVisitor;
import org.eclipse.aether.graph.Exclusion;
import org.eclipse.aether.impl.DefaultServiceLocator;
import org.eclipse.aether.internal.impl.collect.DefaultDependencyCollectionContext;
import org.eclipse.aether.repository.LocalRepository;
import org.eclipse.aether.repository.RemoteRepository;
import org.eclipse.aether.resolution.ArtifactDescriptorException;
import org.eclipse.aether.resolution.ArtifactDescriptorRequest;
import org.eclipse.aether.resolution.ArtifactDescriptorResult;
import org.eclipse.aether.spi.connector.RepositoryConnectorFactory;
import org.eclipse.aether.spi.connector.transport.TransporterFactory;
import org.eclipse.aether.transfer.ArtifactNotFoundException;
import org.eclipse.aether.transport.file.FileTransporterFactory;
import org.eclipse.aether.transport.http.HttpTransporterFactory;
import org.eclipse.aether.util.artifact.JavaScopes;
import org.eclipse.aether.util.graph.manager.ClassicDependencyManager;
import org.eclipse.aether.util.graph.manager.DependencyManagerUtils;
import org.eclipse.aether.util.graph.transformer.ConflictResolver;
import org.eclipse.aether.util.graph.traverser.StaticDependencyTraverser;
import org.eclipse.aether.util.graph.visitor.DependencyGraphDumper;
import org.eclipse.aether.util.graph.visitor.TreeDependencyVisitor;

public class MavenResolver implements Resolver {

  private final RemoteRepositoryFactory remoteRepositoryFactory;
  private final int maxThreads;
  private final EventListener listener;

  public MavenResolver(Netrc netrc, int maxThreads, EventListener listener) {
    this.remoteRepositoryFactory = new RemoteRepositoryFactory(netrc);
    this.maxThreads = maxThreads;
    this.listener = listener;
  }

  public String getName() {
    return "maven";
  }

  private Dependency createBom(
      com.github.bazelbuild.rules_jvm_external.resolver.Artifact artifact) {
    Coordinates coordinates = artifact.getCoordinates();

    Dependency bom =
        new Dependency(
            new DefaultArtifact(
                coordinates.getGroupId(),
                coordinates.getArtifactId(),
                "pom",
                "",
                coordinates.getVersion()),
            JavaScopes.RUNTIME);

    Set<Exclusion> exclusions =
        artifact.getExclusions().stream().map(this::createExclusion).collect(Collectors.toSet());

    return bom.setScope("import").setExclusions(exclusions);
  }

  private Dependency createDependency(
      com.github.bazelbuild.rules_jvm_external.resolver.Artifact source) {
    Coordinates coords = source.getCoordinates();
    Artifact artifact =
        new DefaultArtifact(
            coords.getGroupId(),
            coords.getArtifactId(),
            coords.getClassifier(),
            coords.getExtension(),
            coords.getVersion());

    Set<Exclusion> excluded =
        source.getExclusions().stream().map(this::createExclusion).collect(Collectors.toSet());

    return new Dependency(artifact, JavaScopes.RUNTIME).setExclusions(excluded);
  }

  private Exclusion createExclusion(Coordinates coordinates) {
    return new Exclusion(coordinates.getGroupId(), coordinates.getArtifactId(), "*", "*");
  }

  @Override
  public ResolutionResult resolve(ResolutionRequest request) {
    List<RemoteRepository> repos =
        request.getRepositories().stream()
            .map(remoteRepositoryFactory::createFor)
            .collect(Collectors.toList());

    List<Dependency> boms =
        request.getBoms().stream().map(this::createBom).collect(Collectors.toList());
    List<Dependency> dependencies =
        request.getDependencies().stream().map(this::createDependency).collect(Collectors.toList());
    Set<Exclusion> globalExclusions =
        request.getGlobalExclusions().stream()
            .map(this::createExclusion)
            .collect(Collectors.toSet());

    RepositorySystem system = createRepositorySystem();
    ConsoleRepositoryListener consoleLogListener = new ConsoleRepositoryListener(listener);
    ErrorReportingListener errorListener = new ErrorReportingListener();
    CoordinateGatheringListener coordinatesListener = new CoordinateGatheringListener();
    RepositorySystemSession session =
        prepareSession(
            system,
            new ClassicDependencyManager(),
            new CompoundListener(consoleLogListener, errorListener, coordinatesListener),
            request.getLocalCache("maven"));

    List<RemoteRepository> repositories = new ArrayList<>(repos.size());
    repositories.add(createRemoteRepoFromLocalM2Cache(request.getLocalCache("maven")));
    repositories.addAll(repos);

    List<Dependency> bomsWithGlobalExclusions = addGlobalExclusions(globalExclusions, boms);
    consoleLogListener.setPhase("Resolving " + bomsWithGlobalExclusions.size() + " BOM artifacts");
    List<Dependency> bomDependencies =
        resolveArtifactsFromBoms(system, session, repositories, bomsWithGlobalExclusions);

    List<Dependency> managedDependencies = createManagedDependencies(bomDependencies, dependencies);

    // In a regular maven project, the root `pom.xml` defines what has the
    // dependencies --- they can't just "float in space". We simulate the
    // same kind of structure by defining a `fakeRoot`. This is the thing
    // to which we add dependencies. It won't appear in any of our outputs.
    Artifact fakeRoot = new DefaultArtifact("com.example:bazel-dep-resolver:1.0.0");

    Dependency dep = new Dependency(fakeRoot, JavaScopes.COMPILE);
    DependencyCollectionContext depCollectionContext =
        new DefaultDependencyCollectionContext(session, null, dep, managedDependencies);
    DependencyManager derived =
        new ClassicDependencyManager().deriveChildManager(depCollectionContext);
    session =
        prepareSession(
            system,
            derived,
            new CompoundListener(consoleLogListener, errorListener, coordinatesListener),
            request.getLocalCache("maven"));

    List<Dependency> depsWithGlobalExclusions = addGlobalExclusions(globalExclusions, dependencies);
    consoleLogListener.setPhase(
        "Resolving and fetching the transitive closure of "
            + depsWithGlobalExclusions.size()
            + " artifact(s)");
    List<DependencyNode> resolvedDependencies =
        resolveBaseDependencies(
            system, session, repositories, fakeRoot, managedDependencies, depsWithGlobalExclusions);

    if (System.getenv("RJE_VERBOSE") != null) {
      DependencyGraphDumper graphDumper = new DependencyGraphDumper(System.err::println);
      System.err.println("\nDependency Graph:");
      resolvedDependencies.forEach(node -> node.accept(graphDumper));

      Map<Coordinates, Coordinates> remappings = coordinatesListener.getRemappings();
      if (remappings.isEmpty()) {
        System.err.println("\nNo dependency remappings");
      } else {
        System.err.println("\nDependency remappings:");
        for (Map.Entry<Coordinates, Coordinates> entry : remappings.entrySet()) {
          System.err.println(entry.getKey() + " -> " + entry.getValue());
        }
      }
    }

    List<Exception> exceptions = errorListener.getExceptions();
    if (!exceptions.isEmpty()) {
      for (Exception e : exceptions) {
        if (e instanceof ModelBuildingException) {
          ModelBuildingException mbe = (ModelBuildingException) e;
          String message =
              "The POM for "
                  + mbe.getModelId()
                  + " is invalid. Transitive dependencies will not be available.";
          String detail =
              mbe.getProblems().stream()
                  .map(p -> "[WARNING]:    " + p.getModelId() + " -> " + p.getMessage())
                  .collect(Collectors.joining("\n"));
          listener.onEvent(new LogEvent("maven", message, detail));
        } else if (e instanceof ArtifactNotFoundException) {
          ArtifactNotFoundException anfe = (ArtifactNotFoundException) e;
          // Re-throw if we are missing a top-level dependency
          if (dependencies.stream()
              .map(Dependency::getArtifact)
              .noneMatch(
                  artifact ->
                      artifact.getArtifactId().equals(anfe.getArtifact().getArtifactId())
                          && artifact.getGroupId().equals(anfe.getArtifact().getGroupId())
                          && artifact.getVersion().equals(anfe.getArtifact().getVersion()))) {
            String message =
                "The POM for "
                    + anfe.getArtifact()
                    + " is missing, no dependency information available.";
            String detail = "[WARNING]:    " + anfe.getMessage();
            listener.onEvent(new LogEvent("maven", message, detail));
          } else {
            throw new RuntimeException(e);
          }
        } else if (e instanceof RuntimeException) {
          throw (RuntimeException) e;
        } else {
          throw new RuntimeException(e);
        }
      }
    }

    Graph<Coordinates> dependencyGraph =
        buildDependencyGraph(coordinatesListener.getRemappings(), resolvedDependencies);
    GraphNormalizationResult graphNormalizationResult = makeVersionsConsistent(dependencyGraph);

    Set<Conflict> conflicts =
        Sets.union(
            getConflicts(request.getDependencies(), resolvedDependencies),
            graphNormalizationResult.getConflicts());

    return new ResolutionResult(graphNormalizationResult.getNormalizedGraph(), conflicts);
  }

  private GraphNormalizationResult makeVersionsConsistent(Graph<Coordinates> dependencyGraph) {
    Set<Coordinates> nodes = dependencyGraph.nodes();

    Map<Coordinates, Coordinates> mappedVersions = gatherExpectedVersions(nodes);

    // Reconstruct the dependency graph
    MutableGraph<Coordinates> toReturn = GraphBuilder.directed().allowsSelfLoops(true).build();

    for (Coordinates node : nodes) {
      Coordinates replacement = mappedVersions.get(node);
      toReturn.addNode(replacement);
      Set<Coordinates> successors = dependencyGraph.successors(node);
      for (Coordinates successor : successors) {
        Coordinates successorReplacement = mappedVersions.get(successor);
        toReturn.addNode(successorReplacement);
        toReturn.putEdge(replacement, successorReplacement);
      }
    }

    Set<Conflict> conflicts =
        mappedVersions.entrySet().stream()
            .filter(e -> !e.getKey().equals(e.getValue()))
            .map(e -> new Conflict(e.getValue(), e.getKey()))
            .collect(Collectors.toSet());

    return new GraphNormalizationResult(ImmutableGraph.copyOf(toReturn), conflicts);
  }

  private Map<Coordinates, Coordinates> gatherExpectedVersions(Set<Coordinates> allCoords) {
    Function<Coordinates, String> keyify = c -> c.getGroupId() + ":" + c.getArtifactId();

    // Populate our map of expected versions
    Map<String, String> keyToVersion = new HashMap<>();
    for (Coordinates coord : allCoords) {
      String key = keyify.apply(coord);

      String existing = keyToVersion.get(key);
      if (existing == null) {
        keyToVersion.put(key, coord.getVersion());
        continue;
      }

      ComparableVersion coordVersion = new ComparableVersion(coord.getVersion());
      ComparableVersion existingVersion = new ComparableVersion(existing);

      if (coordVersion.compareTo(existingVersion) > 0) {
        keyToVersion.put(key, coord.getVersion());
      }
    }

    // Now prepare the map to return
    Map<Coordinates, Coordinates> toReturn = new HashMap<>();
    for (Coordinates coord : allCoords) {
      String key = keyify.apply(coord);
      toReturn.put(coord, coord.setVersion(keyToVersion.get(key)));
    }

    return Map.copyOf(toReturn);
  }

  private Set<Conflict> getConflicts(
      List<com.github.bazelbuild.rules_jvm_external.resolver.Artifact> artifacts,
      List<DependencyNode> resolvedDependencies) {
    Set<Conflict> conflicts = new HashSet<>();

    Set<Coordinates> artifactsCoordinates =
        artifacts.stream()
            .map(com.github.bazelbuild.rules_jvm_external.resolver.Artifact::getCoordinates)
            .collect(Collectors.toSet());

    DependencyVisitor collector =
        new TreeDependencyVisitor(
            new DependencyNodeVisitor(
                node -> {
                  Object winner = node.getData().get(ConflictResolver.NODE_DATA_WINNER);
                  if (!(winner instanceof DependencyNode)) {
                    return;
                  }

                  Artifact winningArtifact = ((DependencyNode) winner).getArtifact();
                  Coordinates winningCoords = MavenCoordinates.asCoordinates(winningArtifact);
                  Artifact artifact = node.getArtifact();
                  Coordinates nodeCoords = MavenCoordinates.asCoordinates(artifact);

                  if (!winningCoords.equals(nodeCoords)) {
                    if (!artifactsCoordinates.contains(winningCoords)) {
                      conflicts.add(new Conflict(winningCoords, nodeCoords));
                    }
                  }
                }));

    resolvedDependencies.forEach(node -> node.accept(collector));

    return Set.copyOf(conflicts);
  }

  private List<Dependency> createManagedDependencies(
      List<Dependency> bomDependencies, List<Dependency> dependencies) {
    // Add artifacts people have requested to the managed dependencies
    // Without this, the versions requested in BOMs will be preferred to the
    // one that the user requested
    // First add all the dependencies that have versions
    Map<String, String> artifactVersions = new HashMap<>();
    Set<String> artifactKeys = new HashSet<>();
    dependencies.stream()
        .map(Dependency::getArtifact)
        .filter(artifact -> artifact.getVersion() != null)
        .filter(artifact -> !artifact.getVersion().isEmpty())
        .forEach(
            artifact -> {
              artifactVersions.put(getArtifactKey(artifact), artifact.getVersion());
              artifactKeys.add(getArtifactKey(artifact));
            });

    ImmutableList.Builder<Dependency> managedDependencies = ImmutableList.builder();
    managedDependencies.addAll(dependencies);

    // Then add all the BOM dependencies that are not in the declared dependency list
    boolean rjeVerbose = System.getenv("RJE_VERBOSE") != null;
    bomDependencies.stream()
        .peek(
            dep -> {
              if (rjeVerbose) {
                String artifactKey = getArtifactKey(dep.getArtifact());
                if (artifactKeys.contains(artifactKey)) {
                  System.err.println(
                      "BOM version for "
                          + dep.getArtifact()
                          + " has been overridden by an explicit dependency version: "
                          + artifactVersions.get(artifactKey));
                }
              }
            })
        .filter(dep -> !artifactKeys.contains(getArtifactKey(dep.getArtifact())))
        .forEach(managedDependencies::add);

    // Last, normalize versions across different classifiers of an artifact
    // The rules_jvm_external json version file only supports one version for an artifact even when
    // the artifact has multiple classifiers e.g.
    //
    //  "artifacts": {
    //    "io.netty:netty-tcnative-boringssl-static": {
    //      "shasums": {
    //        "jar": "b6f974972c44cd6f9cecabc255290286faac40b6393c66c3c3c0db7f421cc28e",
    //        "linux-aarch_64": "3614395218ae379cec22ccaa089c4f27b9329a660e0d53c93e7cb12b7a2cee46",
    //        "linux-x86_64": "4ff9d14f1ec6ccee35b78f53a6f3d9c7c54535aa2a76138311c2f619c5e150e1",
    //        "osx-aarch_64": "58e0302c9fde3db984c3ff7ee7ec7159dc0320bdb91533cc290e12e40911cd1a",
    //        "osx-x86_64": "fcfea887f4f0802d363c699b444d504b7109a7cb198ae6845eeff63745e5b0ba",
    //        "windows-x86_64": "17cd2fa3c63b7ed23edea01c945e55cb7baed1faa0f553732c3f5f56da90b3e0"
    //      },
    //      "version": "2.0.61.Final"
    //    },
    //
    // In order to keep the version consistent across all classifiers we use the first version for
    // the artifact in the dependency list as the version for all of the classifiers of the
    // artifact.
    //
    // Note: This is different from the coursier resolver which will use the last artifact in the
    // dependency list for the version of all of that dependency with classifiers.
    Map<String, String> dependencyVersions = new HashMap<>();
    return managedDependencies.build().stream()
        .map(
            dependency -> {
              Artifact artifact = dependency.getArtifact();
              String groupIdArtifactId = artifact.getGroupId() + ":" + artifact.getArtifactId();
              if (dependencyVersions.containsKey(groupIdArtifactId)) {
                Artifact artifactWithNewVersion =
                    new DefaultArtifact(
                        artifact.getGroupId(),
                        artifact.getArtifactId(),
                        artifact.getClassifier(),
                        artifact.getExtension(),
                        dependencyVersions.get(groupIdArtifactId));
                return new Dependency(
                    artifactWithNewVersion,
                    dependency.getScope(),
                    dependency.getOptional(),
                    dependency.getExclusions());
              } else {
                if (artifact.getVersion() != null && !artifact.getVersion().isEmpty()) {
                  dependencyVersions.put(groupIdArtifactId, artifact.getVersion());
                }
                return dependency;
              }
            })
        .collect(ImmutableList.toImmutableList());
  }

  private static String getArtifactKey(Artifact artifact) {
    // Note: We're not using MavenCoordinates.asCoordinates(artifact).asKey() because these
    // dependencies are inputs to the maven resolver, so we don't want to change the classifier
    // or extension before sending them to the resolver.
    return new Coordinates(
            artifact.getGroupId(),
            artifact.getArtifactId(),
            artifact.getExtension(),
            artifact.getClassifier(),
            artifact.getVersion())
        .asKey();
  }

  private List<Dependency> resolveArtifactsFromBoms(
      RepositorySystem system,
      RepositorySystemSession session,
      List<RemoteRepository> repositories,
      List<Dependency> boms) {
    // Use LinkedHashSet to maintain order of how BOMS were declared
    Set<Dependency> managedDependencies = new LinkedHashSet<>();

    for (Dependency bom : boms) {
      ArtifactDescriptorRequest request =
          new ArtifactDescriptorRequest(bom.getArtifact(), repositories, JavaScopes.COMPILE);
      try {
        ArtifactDescriptorResult result = system.readArtifactDescriptor(session, request);
        // NOTE: BOM dependencies are added in order so dependencies from eariler BOMs will
        // take precedence over dependencies from later BOMs
        managedDependencies.addAll(result.getManagedDependencies());
      } catch (ArtifactDescriptorException e) {
        throw new RuntimeException(e);
      }
    }

    return ImmutableList.copyOf(managedDependencies);
  }

  private List<Dependency> addGlobalExclusions(
      Set<Exclusion> globalExclusions, List<Dependency> dependencies) {
    return dependencies.stream()
        .map(
            dep -> {
              Set<Exclusion> allExclusions = new HashSet<>(globalExclusions);
              allExclusions.addAll(dep.getExclusions());

              return new Dependency(
                  dep.getArtifact(), dep.getScope(), dep.getOptional(), allExclusions);
            })
        .collect(ImmutableList.toImmutableList());
  }

  private Graph<Coordinates> buildDependencyGraph(
      Map<Coordinates, Coordinates> remappings, Collection<DependencyNode> resolvedDependencies) {
    MutableGraph<Coordinates> dependencyGraph =
        GraphBuilder.directed().allowsSelfLoops(true).build();
    DependencyVisitor collector =
        new TreeDependencyVisitor(
            new DependencyNodeVisitor(
                node -> {
                  final DependencyNode actualNode = getDependencyNode(node);

                  Artifact artifact = amendArtifact(actualNode.getArtifact());
                  Coordinates from = MavenCoordinates.asCoordinates(artifact);
                  Coordinates remapped = remappings.getOrDefault(from, from);
                  dependencyGraph.addNode(remapped);

                  actualNode.getChildren().stream()
                      .map(this::getDependencyNode)
                      .map(DependencyNode::getArtifact)
                      .map(this::amendArtifact)
                      .map(MavenCoordinates::asCoordinates)
                      .map(c -> remappings.getOrDefault(c, c))
                      .forEach(
                          to -> {
                            dependencyGraph.addNode(to);
                            dependencyGraph.putEdge(remapped, to);
                          });
                }));
    resolvedDependencies.forEach(node -> node.accept(collector));

    return ImmutableGraph.copyOf(dependencyGraph);
  }

  private DependencyNode getDependencyNode(DependencyNode node) {
    Map<?, ?> data = node.getData();
    if (data != null) {
      // By default, aether will trim duplicate dependencies from the graph
      // of returned nodes. That's fine for Maven, as it means that each dep
      // will appear on the classpath once, but not for us. To make sure we
      // get the full dependency graph, we set
      // `ConflictResolver.CONFIG_PROP_VERBOSE` to `true` earlier. When aether
      // detects a duplicate node, it places a marker with no dependencies in
      // the returned `DependencyNode`, and a link to the actual node in the
      // `NODE_DATA_WINNER` data. We need to use _that_ node in order to
      // properly construct the dependency graph.
      Object winner = data.get(ConflictResolver.NODE_DATA_WINNER);
      if (winner instanceof DependencyNode) {
        return (DependencyNode) winner;
      }
    }
    return node;
  }

  private Artifact amendArtifact(Artifact artifact) {
    // If someone has depended on an aggregating pom, the `type` or `extension` will be `pom`.
    // However, in `rules_jvm_external`, we pretend that these are actually `jar` files, and
    // then make things up from there.
    if (!"pom".equals(artifact.getExtension())) {
      return artifact;
    }

    return new DefaultArtifact(
        artifact.getGroupId(),
        artifact.getArtifactId(),
        artifact.getClassifier(),
        null,
        artifact.getVersion());
  }

  private DefaultRepositorySystemSession prepareSession(
      RepositorySystem system,
      DependencyManager dependencyManager,
      RepositoryListener listener,
      Path localCache) {
    DefaultRepositorySystemSession session = MavenRepositorySystemUtils.newSession();

    LocalRepository localRepository = new LocalRepository(localCache.toAbsolutePath().toString());
    session.setLocalRepositoryManager(system.newLocalRepositoryManager(session, localRepository));

    session.setDependencyManager(dependencyManager);

    session.setDependencyTraverser(new StaticDependencyTraverser(true));

    session.setRepositoryListener(listener);

    // Only resolve from repos that we have been asked to use.
    session.setIgnoreArtifactDescriptorRepositories(true);

    Map<Object, Object> configProperties = new HashMap<>();
    configProperties.putAll(System.getProperties());

    // Use the breadth-first dependency collector. This allows us to work out the dependency graph
    // in parallel. This only applies to the step where we calculate the dependency graph, not the
    // actual downloading of those dependencies (which is handled by the `Downloader`).
    // For reference, the set of config properties is drawn from:
    // https://github.com/apache/maven-resolver/blob/master/src/site/markdown/configuration.md
    configProperties.put("aether.dependencyCollector.impl", "bf");
    configProperties.put("aether.dependencyCollector.bf.threads", String.valueOf(maxThreads));
    // Set the number of threads to use when figuring out how many dependencies to download in
    // parallel.
    configProperties.put("maven.artifact.threads", String.valueOf(maxThreads));

    // Add flags so we keep the version conflict information in the resolved graph
    //
    // By default, the graph transformer will turn the dependency graph into a tree without
    // duplicate artifacts. Using the configuration property CONFIG_PROP_VERBOSE, a verbose
    // mode can be enabled where the graph is still turned into a tree but all nodes participating
    // in a conflict are retained. The nodes that were rejected during conflict resolution have no
    // children and link back to the winner node via the NODE_DATA_WINNER key in their custom data.
    // Additionally, the keys NODE_DATA_ORIGINAL_SCOPE and NODE_DATA_ORIGINAL_OPTIONALITY are used
    // to store the original scope and optionality of each node. Obviously, the resulting dependency
    // tree is not suitable for artifact resolution unless a filter is employed to exclude the
    // duplicate dependencies.
    configProperties.put(ConflictResolver.CONFIG_PROP_VERBOSE, true);
    // If enabled, the original attributes of a dependency before its update due to dependency
    // managemnent will be recorded in the node's custom data when building a dependency graph.
    configProperties.put(DependencyManagerUtils.CONFIG_PROP_VERBOSE, true);

    session.setConfigProperties(Map.copyOf(configProperties));

    session.setSystemProperties(System.getProperties());

    return session;
  }

  private static RepositorySystem createRepositorySystem() {
    DefaultServiceLocator locator = MavenRepositorySystemUtils.newServiceLocator();
    locator.addService(RepositoryConnectorFactory.class, BasicRepositoryConnectorFactory.class);
    locator.addService(TransporterFactory.class, FileTransporterFactory.class);
    locator.addService(TransporterFactory.class, HttpTransporterFactory.class);

    return locator.getService(RepositorySystem.class);
  }

  private List<DependencyNode> resolveBaseDependencies(
      RepositorySystem system,
      RepositorySystemSession session,
      Collection<RemoteRepository> repositories,
      Artifact root,
      List<Dependency> managedDependencies,
      List<Dependency> allDependencies) {
    CollectRequest collectRequest = new CollectRequest();
    collectRequest.setRootArtifact(root);
    collectRequest.setRequestContext(JavaScopes.RUNTIME);

    collectRequest.setManagedDependencies(managedDependencies);
    collectRequest.setDependencies(allDependencies);

    for (RemoteRepository repository : repositories) {
      collectRequest.addRepository(repository);
    }

    CollectResult collectResult;
    try {
      collectResult = system.collectDependencies(session, collectRequest);
    } catch (DependencyCollectionException e) {
      throw new RuntimeException(e);
    }

    if (!collectResult.getExceptions().isEmpty()) {
      Exception exception = collectResult.getExceptions().get(0);
      if (exception instanceof RuntimeException) {
        throw (RuntimeException) exception;
      }
      throw new RuntimeException(exception);
    }

    List<DependencyCycle> cycles = collectResult.getCycles();
    if (!cycles.isEmpty()) {
      Set<DependencyCycle> illegalCycles = new LinkedHashSet<>();
      for (DependencyCycle cycle : cycles) {
        // If the cycle is to the same group and coordinate, it's probably just fine
        List<Dependency> precedingDeps = cycle.getPrecedingDependencies();
        Artifact parent = precedingDeps.get(precedingDeps.size() - 1).getArtifact();

        for (Dependency dep : cycle.getCyclicDependencies()) {
          Artifact artifact = dep.getArtifact();
          if (!parent.getGroupId().equals(artifact.getGroupId())
              || !parent.getArtifactId().equals(artifact.getArtifactId())) {
            illegalCycles.add(cycle);
            break;
          }
        }
      }
      if (!illegalCycles.isEmpty()) {
        throw new RuntimeException(
            "Cycles detected: \n"
                + illegalCycles.stream().map(c -> "  " + c).collect(Collectors.joining("\n")));
      }
    }

    return collectResult.getRoot().getChildren();
  }

  private RemoteRepository createRemoteRepoFromLocalM2Cache(Path localCache) {
    return remoteRepositoryFactory.createFor(localCache.toUri());
  }

  private static class GraphNormalizationResult {
    private final Graph<Coordinates> normalizedGraph;
    private final Set<Conflict> conflicts;

    public GraphNormalizationResult(Graph<Coordinates> normalizedGraph, Set<Conflict> conflicts) {
      this.normalizedGraph = normalizedGraph;
      this.conflicts = conflicts;
    }

    public Graph<Coordinates> getNormalizedGraph() {
      return normalizedGraph;
    }

    public Set<Conflict> getConflicts() {
      return conflicts;
    }
  }
}
