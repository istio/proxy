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

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.google.common.base.MoreObjects;
import com.google.common.collect.ImmutableSet;
import java.net.URI;
import java.nio.file.Path;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;
import java.util.SortedMap;
import java.util.SortedSet;
import java.util.TreeSet;

public class DependencyInfo {

  private final Coordinates coordinates;
  private final Set<URI> repos;
  private final Optional<Path> path;
  private final Optional<String> sha256;
  private final Set<Coordinates> dependencies;
  private final Set<String> packages;
  private final SortedMap<String, SortedSet<String>> services;

  public DependencyInfo(
      Coordinates coordinates,
      Set<URI> repos,
      Optional<Path> path,
      Optional<String> sha256,
      Set<Coordinates> dependencies,
      Set<String> packages,
      SortedMap<String, SortedSet<String>> services) {
    this.coordinates = coordinates;
    this.repos = ImmutableSet.copyOf(repos);
    this.path = path;
    this.sha256 = sha256;
    this.dependencies = ImmutableSet.copyOf(new TreeSet<>(dependencies));

    this.packages = ImmutableSet.copyOf(new TreeSet<>(packages));
    this.services = services;
  }

  public Coordinates getCoordinates() {
    return coordinates;
  }

  public Set<URI> getRepositories() {
    return repos;
  }

  public Optional<String> getSha256() {
    return sha256;
  }

  public Set<Coordinates> getDependencies() {
    return dependencies;
  }

  public Set<String> getPackages() {
    return packages;
  }

  public Optional<Path> getPath() {
    return path;
  }

  public SortedMap<String, SortedSet<String>> getServices() {
    return services;
  }

  @Override
  public String toString() {
    return MoreObjects.toStringHelper(this)
        .add("coordinates", coordinates)
        .add("sha256", sha256.orElseGet(() -> ""))
        .add("dependencies", dependencies)
        .toString();
  }

  @Override
  public boolean equals(Object o) {
    if (this == o) {
      return true;
    }
    if (o == null || getClass() != o.getClass()) {
      return false;
    }
    DependencyInfo that = (DependencyInfo) o;
    return Objects.equals(coordinates, that.coordinates)
        && Objects.equals(sha256, that.sha256)
        && Objects.equals(dependencies, that.dependencies)
        && Objects.equals(packages, that.packages)
        && Objects.equals(services, that.services);
  }

  @Override
  public int hashCode() {
    return Objects.hash(coordinates, sha256, dependencies, packages, services);
  }
}
