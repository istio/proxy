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

package com.github.bazelbuild.rules_jvm_external.resolver.gradle.models;

import java.util.List;

/**
 * Gradle dependency information collected from our GradleDependencyModelPlugin. It provides all the
 * necessary information to build the lockfile and report conflicts.
 */
public interface GradleDependencyModel {
  /**
   * This will need to build the dependency graph and resolve all the associated artifacts
   *
   * @return a list of resolved gradle dependencies
   */
  List<GradleResolvedDependency> getResolvedDependencies();

  /**
   * This will fetch and report any unresolved dependencies after the resolution process.
   *
   * @return A list of unresolved gradle dependencies
   */
  List<GradleUnresolvedDependency> getUnresolvedDependencies();
}
