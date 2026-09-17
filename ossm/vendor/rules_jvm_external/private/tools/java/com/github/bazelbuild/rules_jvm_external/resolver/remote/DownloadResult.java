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

package com.github.bazelbuild.rules_jvm_external.resolver.remote;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.google.common.collect.ImmutableSet;
import java.net.URI;
import java.nio.file.Path;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;

public class DownloadResult {

  private final Coordinates coordinates;
  private final Set<URI> repos;
  private final Optional<Path> path;
  private final Optional<String> sha256;

  public DownloadResult(Coordinates coordinates, Set<URI> repos, Path path, String sha256) {
    this.coordinates = Objects.requireNonNull(coordinates);
    this.repos = ImmutableSet.copyOf(repos);
    this.path = Optional.ofNullable(path);
    this.sha256 = Optional.ofNullable(sha256);
  }

  public Coordinates getCoordinates() {
    return coordinates;
  }

  public Set<URI> getRepositories() {
    return repos;
  }

  public Optional<Path> getPath() {
    return path;
  }

  public Optional<String> getSha256() {
    return sha256;
  }
}
