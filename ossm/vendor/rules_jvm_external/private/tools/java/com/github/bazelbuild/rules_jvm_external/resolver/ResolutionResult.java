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
import com.google.common.graph.Graph;
import java.nio.file.Path;
import java.util.Map;
import java.util.Set;

/**
 * The result of a dependency resolution, containing not only the dependency graph, but also
 * metadata, such as conflicts that have been resolved.
 */
public class ResolutionResult {

  private final Graph<Coordinates> resolution;
  private final Set<Conflict> conflicts;
  private final Map<Coordinates, Path> paths;

  public ResolutionResult(
      Graph<Coordinates> resolution,
      Set<Conflict> conflicts,
      Map<Coordinates, Path> artifactPaths) {
    this.resolution = resolution;
    this.conflicts = Set.copyOf(conflicts);
    this.paths = artifactPaths != null ? Map.copyOf(artifactPaths) : Map.of();
  }

  public Graph<Coordinates> getResolution() {
    return resolution;
  }

  public Set<Conflict> getConflicts() {
    return conflicts;
  }

  public Map<Coordinates, Path> getPaths() {
    return paths;
  }
}
