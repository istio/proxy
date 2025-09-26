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

package com.github.bazelbuild.rules_jvm_external.resolver;

import static com.google.common.base.StandardSystemProperty.USER_HOME;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;
import java.util.stream.Stream;

public class ResolutionRequest {

  private final List<URI> repos = new ArrayList<>();
  private final List<Artifact> dependencies = new ArrayList<>();
  private final List<Artifact> boms = new ArrayList<>();
  private final Set<Coordinates> globalExclusions = new HashSet<>();
  private boolean useUnsafeSharedCache;
  private Path userHome;
  private boolean isUsingM2Local;

  public ResolutionRequest addRepository(String uri) {
    if ("m2local".equals(uri) || "m2Local".equals(uri)) {
      isUsingM2Local = true;
      Path m2local = Paths.get(USER_HOME.value()).resolve(".m2/repository");
      return addRepository(m2local.toUri());
    }

    return addRepository(URI.create(uri));
  }

  public ResolutionRequest addRepository(URI uri) {
    Objects.requireNonNull(uri, "Repository");
    repos.add(uri);
    return this;
  }

  public ResolutionRequest addBom(String coordinates, String... exclusions) {
    Objects.requireNonNull(coordinates, "BOM coordinates");
    return addBom(new Coordinates(coordinates), exclusions);
  }

  public ResolutionRequest addBom(Coordinates coordinates, String... exclusions) {
    Objects.requireNonNull(coordinates, "BOM coordinates");

    Coordinates bom =
        new Coordinates(
            coordinates.getGroupId(),
            coordinates.getArtifactId(),
            "pom",
            "",
            coordinates.getVersion());
    Artifact artifact =
        new Artifact(bom, Stream.of(exclusions).map(Coordinates::new).collect(Collectors.toSet()));

    return addBom(artifact);
  }

  public ResolutionRequest addBom(Artifact artifact) {
    Objects.requireNonNull(artifact, "Artifact");

    boms.add(artifact);

    return this;
  }

  public ResolutionRequest addArtifact(String coordinates, String... exclusions) {
    Objects.requireNonNull(coordinates, "Maven coordinates");

    Coordinates coords = new Coordinates(coordinates);
    Artifact artifact =
        new Artifact(
            coords, Stream.of(exclusions).map(Coordinates::new).collect(Collectors.toSet()));

    return addArtifact(artifact);
  }

  public ResolutionRequest addArtifact(Artifact artifact) {
    Objects.requireNonNull(artifact, "Artifact");

    dependencies.add(artifact);

    return this;
  }

  public ResolutionRequest exclude(String exclusion) {
    Objects.requireNonNull(exclusion, "Exclusion");
    return exclude(new Coordinates(exclusion));
  }

  public ResolutionRequest exclude(Coordinates exclusion) {
    Objects.requireNonNull(exclusion, "Exclusion");
    globalExclusions.add(exclusion);
    return this;
  }

  public ResolutionRequest useUnsafeSharedCache(boolean useUnsafeSharedCache) {
    this.useUnsafeSharedCache = useUnsafeSharedCache;
    return this;
  }

  public ResolutionRequest replaceDependencies(Collection<Artifact> amended) {
    ResolutionRequest toReturn = new ResolutionRequest();

    getRepositories().forEach(toReturn::addRepository);
    amended.forEach(toReturn::addArtifact);
    getBoms().stream().map(Objects::toString).forEach(toReturn::addBom);
    getGlobalExclusions().forEach(toReturn::exclude);
    toReturn.useUnsafeSharedCache = isUseUnsafeSharedCache();
    toReturn.userHome = userHome;
    toReturn.isUsingM2Local = isUsingM2Local();

    return toReturn;
  }

  public List<URI> getRepositories() {
    return Collections.unmodifiableList(repos);
  }

  public List<Artifact> getDependencies() {
    return Collections.unmodifiableList(dependencies);
  }

  public List<Artifact> getBoms() {
    return Collections.unmodifiableList(boms);
  }

  public Set<Coordinates> getGlobalExclusions() {
    return Collections.unmodifiableSet(globalExclusions);
  }

  public boolean isUseUnsafeSharedCache() {
    return useUnsafeSharedCache;
  }

  public boolean isUsingM2Local() {
    return isUsingM2Local;
  }

  public Path getUserHome() {
    if (userHome != null) {
      return userHome;
    }

    if (isUseUnsafeSharedCache()) {
      userHome = Paths.get(USER_HOME.value());
    } else {
      try {
        userHome = Files.createTempDirectory("resolver-home");
      } catch (IOException e) {
        throw new RuntimeException(e);
      }
    }

    return userHome;
  }

  private Path getGradleCachePath() {
    // https://docs.gradle.org/current/userguide/dependency_caching.html
    return getUserHome().resolve(".gradle").resolve("caches/modules-2/files-2.1");
  }

  private Path getM2CachePath() {
    return getUserHome().resolve(".m2").resolve("repository");
  }

  public Path getLocalCache(String resolver) {
    Path localRepo = getM2CachePath();
    // Gradle can download from m2local but never downloads to it
    // so we need to resolve to the gradle cache path here
    if (resolver.equals("gradle")) {
      localRepo = getGradleCachePath();
    }
    if (!Files.exists(localRepo)) {
      createDirectories(localRepo);
    }

    if (!Files.isDirectory(localRepo)) {
      throw new IllegalArgumentException(
          "Asked to use local repo, but it is not a directory: " + localRepo);
    }

    return localRepo;
  }

  private void createDirectories(Path path) {
    try {
      Files.createDirectories(path);
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }
}
