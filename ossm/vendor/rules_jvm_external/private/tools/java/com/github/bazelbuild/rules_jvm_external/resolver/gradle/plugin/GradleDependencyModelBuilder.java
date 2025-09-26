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

package com.github.bazelbuild.rules_jvm_external.resolver.gradle.plugin;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleDependency;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleDependencyImpl;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleDependencyModel;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleDependencyModelImpl;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleResolvedArtifact;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleResolvedArtifactImpl;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleResolvedDependency;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleResolvedDependencyImpl;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleUnresolvedDependency;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleUnresolvedDependencyImpl;
import com.google.common.collect.Streams;
import com.google.common.io.Files;
import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.stream.Collectors;
import org.gradle.api.Project;
import org.gradle.api.artifacts.ArtifactView;
import org.gradle.api.artifacts.Configuration;
import org.gradle.api.artifacts.Dependency;
import org.gradle.api.artifacts.DependencyArtifact;
import org.gradle.api.artifacts.ModuleDependency;
import org.gradle.api.artifacts.component.ComponentIdentifier;
import org.gradle.api.artifacts.component.ModuleComponentIdentifier;
import org.gradle.api.artifacts.component.ModuleComponentSelector;
import org.gradle.api.artifacts.result.ComponentSelectionReason;
import org.gradle.api.artifacts.result.DependencyResult;
import org.gradle.api.artifacts.result.ResolutionResult;
import org.gradle.api.artifacts.result.ResolvedArtifactResult;
import org.gradle.api.artifacts.result.ResolvedComponentResult;
import org.gradle.api.artifacts.result.ResolvedDependencyResult;
import org.gradle.api.artifacts.result.UnresolvedDependencyResult;
import org.gradle.api.attributes.Attribute;
import org.gradle.api.attributes.LibraryElements;
import org.gradle.api.attributes.Usage;
import org.gradle.tooling.provider.model.ToolingModelBuilder;

/**
 * GradleDependencyModelBuilder implenents the core functionality as part of a plugin to resolve the
 * dependency graph, fetch and associate relevant artifacts, track unresolved dependencies, report
 * any failures and the final tooling model back to the resolver
 */
public class GradleDependencyModelBuilder implements ToolingModelBuilder {

  @Override
  public boolean canBuild(String modelName) {
    return modelName.equals(GradleDependencyModel.class.getName());
  }

  @Override
  public Object buildAll(String modelName, Project project) {
    GradleDependencyModel gradleDependencyModel = new GradleDependencyModelImpl();
    // We only resolve dependencies and fetch artifacts for this configuration
    // similar to how we do in the Maven resolver
    Configuration cfg = project.getConfigurations().getByName("runtimeClasspath");

    List<GradleDependency> declaredDeps = collectDeclaredDependencies(cfg);
    // This stores the mapping between coordinates to the GradleResolvedDependency interface
    // which contains the resolved dependency information from the tooling API, additionally it'll
    // also
    // be used to attach the actual artifacts later
    ConcurrentHashMap<Coordinates, GradleResolvedDependency>
        coordinatesGradleResolvedDependencyMap = new ConcurrentHashMap<>();
    // We get the root nodes in the dependency graph (or rather forest here since there can be
    // disjoint trees)
    List<GradleResolvedDependency> resolvedRoots =
        collectResolvedDependencies(cfg, coordinatesGradleResolvedDependencyMap);

    // Collect any unresolved dependencies from the runtimeClasspath configuration
    List<GradleUnresolvedDependency> unresolvedDependenciesRuntimeClasspath =
        getUnresolvedDependencies(cfg);

    List<Dependency> unresolvedDependencies =
        unresolvedDependenciesRuntimeClasspath.stream()
            .map(
                dep -> {
                  String coords = dep.getGroup() + ":" + dep.getName() + ":" + dep.getVersion();
                  return project.getDependencies().create(coords);
                })
            .collect(Collectors.toList());

    // Create a configuration to resolve android dependencies (it can be used for other platforms
    // in the future as well like Kotlin multiplatform).
    Configuration detachedCfg =
        project
            .getConfigurations()
            .detachedConfiguration(unresolvedDependencies.toArray(new Dependency[0]));

    // build the updated dependency graph with the detached configuration for all the
    // dependencies that we couldn't resolve with the default configuration
    List<GradleResolvedDependency> resolvedDetachedRoots =
        resolveDetachedGraph(detachedCfg, coordinatesGradleResolvedDependencyMap);

    List<GradleResolvedDependency> roots =
        Streams.concat(resolvedRoots.stream(), resolvedDetachedRoots.stream())
            .collect(Collectors.toList());
    // Use the ArtifactView API to get all the resolved artifacts (jars, aars)
    // The ArtifactView API doesn't download some of the classifiers by default, so we handle that
    // here
    collectAllResolvedArtifacts(
        project, cfg, detachedCfg, roots, coordinatesGradleResolvedDependencyMap, declaredDeps);
    gradleDependencyModel.getResolvedDependencies().addAll(roots);

    // if anything is still unresolved, then add it for reporting
    gradleDependencyModel
        .getUnresolvedDependencies()
        .addAll(getUnresolvedDependencies(detachedCfg));
    return gradleDependencyModel;
  }

  private List<GradleResolvedDependency> resolveDetachedGraph(
      Configuration detachedCfg,
      ConcurrentHashMap<Coordinates, GradleResolvedDependency>
          coordinatesGradleResolvedDependencyMap) {
    return collectResolvedDependencies(detachedCfg, coordinatesGradleResolvedDependencyMap);
  }

  private List<GradleDependency> collectDeclaredDependencies(Configuration cfg) {
    List<GradleDependency> dependencies = new ArrayList<>();
    for (Dependency dependency : cfg.getAllDependencies()) {
      if (dependency instanceof ModuleDependency) {
        ModuleDependency externalDependency = (ModuleDependency) dependency;
        for (DependencyArtifact artifact : externalDependency.getArtifacts()) {
          GradleDependency dep =
              new GradleDependencyImpl(
                  externalDependency.getGroup(),
                  externalDependency.getName(),
                  externalDependency.getVersion(),
                  null,
                  artifact.getClassifier(),
                  artifact.getExtension());
          dependencies.add(dep);
        }
      }
    }
    return dependencies;
  }

  private boolean isBom(ResolvedDependencyResult result) {
    for (Attribute attribute : result.getRequested().getAttributes().keySet()) {
      // Gradle sets this attributes for BOMs
      if (attribute.getName().equals("org.gradle.category")) {
        return result.getRequested().getAttributes().getAttribute(attribute).equals("platform");
      }
    }
    return false;
  }

  private List<GradleResolvedDependency> collectResolvedDependencies(
      Configuration cfg,
      ConcurrentMap<Coordinates, GradleResolvedDependency> coordinatesGradleResolvedDependencyMap) {
    List<GradleResolvedDependency> resolvedRoots = new ArrayList<>();
    ResolutionResult result = cfg.getIncoming().getResolutionResult();
    ResolvedComponentResult root = result.getRoot();

    if (isVerbose()) {
      System.err.println("Dependency graph: ");
    }

    for (DependencyResult dep : root.getDependencies()) {
      if (dep instanceof ResolvedDependencyResult) {
        ResolvedDependencyResult rdep = (ResolvedDependencyResult) dep;
        ResolvedComponentResult selected = rdep.getSelected();
        // we don't want to recurse through a BOM's artifacts
        // as they wil be explicitly specified
        if (isBom(rdep)) {
          continue;
        }
        Set<ComponentIdentifier> visited = new HashSet<>();
        // walk the resolved component graph in depth-first manner
        // and collect all the resolved dependencies
        GradleResolvedDependency info =
            walkResolvedComponent(selected, visited, coordinatesGradleResolvedDependencyMap, 1);
        if (rdep.getRequested() instanceof ModuleComponentSelector) {
          String requested = ((ModuleComponentSelector) rdep.getRequested()).getVersion();
          info.addRequestedVersion(requested);
          info.setConflict(info.getRequestedVersions().size() > 1);
        }

        resolvedRoots.add(info);
      }
    }
    return resolvedRoots;
  }

  private List<GradleUnresolvedDependency> getUnresolvedDependencies(Configuration cfg) {
    List<GradleUnresolvedDependency> unresolvedDependencies = new ArrayList<>();
    ResolutionResult result = cfg.getIncoming().getResolutionResult();
    for (DependencyResult dep : result.getAllDependencies()) {
      if (dep instanceof UnresolvedDependencyResult) {
        UnresolvedDependencyResult rdep = (UnresolvedDependencyResult) dep;
        ModuleComponentSelector selector = (ModuleComponentSelector) rdep.getAttempted();
        Throwable failure = rdep.getFailure();

        GradleUnresolvedDependency.FailureReason reason =
            GradleUnresolvedDependency.FailureReason.INTERNAL;
        // This is an internal API, so we can't check for the type
        // but this maps to artifact not being found and not some other error
        if (failure
            .toString()
            .contains("org.gradle.internal.resolve.ModuleVersionNotFoundException")) {
          reason = GradleUnresolvedDependency.FailureReason.NOT_FOUND;
        }
        GradleUnresolvedDependency unresolved =
            new GradleUnresolvedDependencyImpl(
                selector.getGroup(),
                selector.getModule(),
                selector.getVersion(),
                reason,
                failure.getMessage());
        unresolvedDependencies.add(unresolved);
      }
    }
    return unresolvedDependencies;
  }

  private GradleResolvedDependency walkResolvedComponent(
      ResolvedComponentResult component,
      Set<ComponentIdentifier> visited,
      Map<Coordinates, GradleResolvedDependency> coordinatesGradleResolvedDependencyMap,
      int depth) {
    Coordinates coordinates =
        new Coordinates(
            component.getModuleVersion().getGroup()
                + ":"
                + component.getModuleVersion().getName()
                + ":"
                + component.getModuleVersion().getVersion());

    // Handle cycles as they can exist in resolution
    if (visited.contains(component.getId())) {
      return coordinatesGradleResolvedDependencyMap.get(coordinates);
    }

    visited.add(component.getId());
    if (isVerbose()) {
      System.err.println("  ".repeat(depth) + "→ " + coordinates);
    }

    // We might visit the same node multiple times through the graph, so check if we've visited
    // node before
    GradleResolvedDependency info = coordinatesGradleResolvedDependencyMap.get(coordinates);
    if (info == null) {
      // this is a new artifact yet unvisited
      info = new GradleResolvedDependencyImpl();
      info.setGroup(component.getModuleVersion().getGroup());
      info.setName(component.getModuleVersion().getName());
      info.setVersion(component.getModuleVersion().getVersion());
    }

    info.addRequestedVersion(info.getVersion()); // add a new version that may have been requested

    List<GradleResolvedDependency> children = new ArrayList<>();

    for (DependencyResult dep : component.getDependencies()) {
      if (!(dep instanceof ResolvedDependencyResult)) {
        continue;
      }

      ResolvedDependencyResult resolvedDep = (ResolvedDependencyResult) dep;
      ResolvedComponentResult selected = resolvedDep.getSelected();

      GradleResolvedDependency child =
          walkResolvedComponent(
              selected, visited, coordinatesGradleResolvedDependencyMap, depth + 1);
      if (child == null) {
        continue;
      }
      if (resolvedDep.getRequested() instanceof ModuleComponentSelector) {
        String requestedVersion =
            ((ModuleComponentSelector) resolvedDep.getRequested()).getVersion();
        child.addRequestedVersion(requestedVersion);
        child.setConflict(child.getRequestedVersions().size() > 1);
      }

      children.add(child);
    }

    ComponentSelectionReason reason = component.getSelectionReason();
    List<String> bomDescriptions =
        reason.getDescriptions().stream()
            .map(descriptor -> descriptor.getDescription().toLowerCase())
            .filter(
                description ->
                    description.contains("platform") || description.contains("constraint"))
            .collect(Collectors.toList());
    info.setFromBom(!bomDescriptions.isEmpty());

    info.setChildren(children);
    info.setConflict(info.getRequestedVersions().size() > 1);
    coordinatesGradleResolvedDependencyMap.put(coordinates, info);
    return info;
  }

  // The ArtifactView api doesn't provide a way to obtain the classifier (the legacy API does but it
  // doesn't provide a way to obtain additional artifacts
  // like javadoc/sources from what I can tell
  // so we interpolate the classifier based on the artifact file
  private String extractClassifier(File file, ComponentIdentifier id) {
    if (!(id instanceof ModuleComponentIdentifier)) {
      return null;
    }

    ModuleComponentIdentifier module = (ModuleComponentIdentifier) id;
    String version = module.getVersion();
    String artifact = module.getModule();

    String name = file.getName(); // e.g. lib-1.0-sources.jar, lib-1.0.pom

    // Strip extension
    int dotIndex = name.lastIndexOf('.');
    if (dotIndex <= 0) {
      return null;
    }
    String nameWithoutExt = name.substring(0, dotIndex);

    String expectedBase = artifact + "-" + version;

    if (!nameWithoutExt.startsWith(expectedBase)) {
      return null;
    }

    String suffix = nameWithoutExt.substring(expectedBase.length());
    return (suffix.startsWith("-")) ? suffix.substring(1) : null;
  }

  private void collectAllResolvedArtifacts(
      Project project,
      Configuration runtimeClassPathCfg,
      Configuration detachedCfg,
      List<GradleResolvedDependency> resolvedRoots,
      Map<Coordinates, GradleResolvedDependency> coordinatesMap,
      List<GradleDependency> declaredDeps) {

    ArtifactView jars =
        runtimeClassPathCfg
            .getIncoming()
            .artifactView(
                spec -> {
                  spec.setLenient(true);
                  spec.attributes(
                      attrs ->
                          attrs.attribute(
                              Usage.USAGE_ATTRIBUTE,
                              project.getObjects().named(Usage.class, Usage.JAVA_API)));
                });

    // collect JAR artifacts
    collectArtifactsFromArtifactView(jars, coordinatesMap);

    ArtifactView aarView =
        detachedCfg
            .getIncoming()
            .artifactView(
                spec -> {
                  spec.setLenient(true); // tolerate dependencies without AAR variants
                  spec.attributes(
                      attrs -> {
                        attrs.attribute(
                            Usage.USAGE_ATTRIBUTE,
                            project.getObjects().named(Usage.class, Usage.JAVA_RUNTIME));
                        attrs.attribute(
                            LibraryElements.LIBRARY_ELEMENTS_ATTRIBUTE,
                            project.getObjects().named(LibraryElements.class, "aar"));
                      });
                  spec.withVariantReselection();
                });

    // Collect Android artifacts  (AARs)
    collectArtifactsFromArtifactView(aarView, coordinatesMap);

    // Collect POM files explicitly as gradle doesn't fetch them unless requested
    collectPOMsForAllComponents(project, resolvedRoots, coordinatesMap, declaredDeps);
    // Request artifacts which have conflicts so asto record them in the graph
    collectConflictingVersionArtifacts(project, coordinatesMap);
  }

  private void collectConflictingVersionArtifacts(
      Project project,
      Map<Coordinates, GradleResolvedDependency> coordinatesGradleResolvedDependencyMap) {
    for (Map.Entry<Coordinates, GradleResolvedDependency> entry :
        coordinatesGradleResolvedDependencyMap.entrySet()) {
      GradleResolvedDependency dependency = entry.getValue();
      if (dependency.getRequestedVersions().size() > 1) {
        dependency
            .getRequestedVersions()
            .forEach(
                version -> {
                  Coordinates coordinates =
                      new Coordinates(
                          dependency.getGroup() + ":" + dependency.getName() + ":" + version);

                  Dependency dep = project.getDependencies().create(coordinates + "@jar");

                  Configuration jarCfg = project.getConfigurations().detachedConfiguration(dep);

                  ArtifactView view =
                      jarCfg
                          .getIncoming()
                          .artifactView(
                              spec -> {
                                spec.setLenient(true); // avoid failure if a POM is missing
                              });

                  for (ResolvedArtifactResult artifact : view.getArtifacts().getArtifacts()) {
                    GradleResolvedArtifact resolvedArtifact = new GradleResolvedArtifactImpl();
                    if (artifact.getFile() != null) {
                      resolvedArtifact.setFile(artifact.getFile());
                      GradleResolvedDependency resolvedDependency =
                          coordinatesGradleResolvedDependencyMap.get(entry.getKey());
                      synchronized (resolvedDependency) {
                        resolvedDependency.addArtifact(resolvedArtifact);
                      }
                    }
                  }
                });
      }
    }
  }

  private void collectArtifactsFromArtifactView(
      ArtifactView artifactView,
      Map<Coordinates, GradleResolvedDependency> coordinatesGradleResolvedDependencyMap) {
    Set<ResolvedArtifactResult> resolvedArtifactResults =
        artifactView.getArtifacts().getArtifacts();
    for (ResolvedArtifactResult artifact : resolvedArtifactResults) {
      ComponentIdentifier identifier = artifact.getId().getComponentIdentifier();
      ModuleComponentIdentifier module = (ModuleComponentIdentifier) identifier;
      if (artifact.getFile() != null) {
        GradleResolvedArtifact resolvedArtifact = new GradleResolvedArtifactImpl();
        resolvedArtifact.setFile(artifact.getFile());
        resolvedArtifact.setClassifier(extractClassifier(artifact.getFile(), identifier));
        resolvedArtifact.setExtension(Files.getFileExtension(artifact.getFile().getName()));

        Coordinates coordinates =
            new Coordinates(
                module.getGroup() + ":" + module.getModule() + ":" + module.getVersion());
        GradleResolvedDependency resolvedDependency =
            coordinatesGradleResolvedDependencyMap.get(coordinates);
        synchronized (resolvedDependency) {
          resolvedDependency.addArtifact(resolvedArtifact);
        }
      }
    }
  }

  private void collectPOMsForAllComponents(
      Project project,
      List<GradleResolvedDependency> resolvedRoots,
      Map<Coordinates, GradleResolvedDependency> coordinatesResolvedDependencyMap,
      List<GradleDependency> declaredDeps) {

    List<Dependency> pomDependencies = new ArrayList<>();
    Map<Coordinates, Coordinates> moduleToRequestedCoordinates = new HashMap<>();

    for (GradleResolvedDependency dependency : resolvedRoots) {
      Coordinates coordinates =
          new Coordinates(
              dependency.getGroup() + ":" + dependency.getName() + ":" + dependency.getVersion());

      boolean isDeclaredWithExplicitExtension =
          declaredDeps.stream()
              .anyMatch(
                  declaredDep ->
                      declaredDep.getGroup().equals(dependency.getGroup())
                          && declaredDep.getArtifact().equals(dependency.getName())
                          && declaredDep.getVersion().equals(dependency.getVersion())
                          && declaredDep.getExtension() != null);

      if (isDeclaredWithExplicitExtension) {
        continue;
      }

      Dependency dep = project.getDependencies().create(coordinates + "@pom");
      pomDependencies.add(dep);
      moduleToRequestedCoordinates.put(coordinates, coordinates);
    }

    if (pomDependencies.isEmpty()) {
      return;
    }

    Configuration batchedPomCfg =
        project
            .getConfigurations()
            .detachedConfiguration(pomDependencies.toArray(new Dependency[0]));

    ArtifactView view =
        batchedPomCfg
            .getIncoming()
            .artifactView(
                spec -> {
                  spec.setLenient(true); // tolerate missing POMs
                });

    for (ResolvedArtifactResult artifact : view.getArtifacts().getArtifacts()) {
      ComponentIdentifier id = artifact.getId().getComponentIdentifier();
      if (!(id instanceof ModuleComponentIdentifier)) {
        continue;
      }

      ModuleComponentIdentifier module = (ModuleComponentIdentifier) id;
      Coordinates moduleCoordinates =
          new Coordinates(module.getGroup() + ":" + module.getModule() + ":" + module.getVersion());

      Coordinates requestedKey = moduleToRequestedCoordinates.get(moduleCoordinates);
      if (requestedKey == null) {
        continue;
      }

      GradleResolvedDependency resolvedDependency =
          coordinatesResolvedDependencyMap.get(requestedKey);
      if (resolvedDependency == null) {
        continue;
      }

      GradleResolvedArtifact resolvedArtifact = new GradleResolvedArtifactImpl();
      resolvedArtifact.setFile(artifact.getFile());
      if (artifact.getFile() != null) {
        resolvedArtifact.setExtension(PomUtil.extractPackagingFromPom(artifact.getFile()));
      }
      resolvedDependency.addArtifact(resolvedArtifact);
    }
  }

  private boolean isVerbose() {
    return System.getenv("RJE_VERBOSE") != null;
  }
}
