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

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

public class GradleDependencyModelImpl implements Serializable, GradleDependencyModel {
  private final List<GradleResolvedDependency> resolved = new ArrayList<>();
  private final List<GradleDependency> boms = new ArrayList<>();
  private final List<GradleUnresolvedDependency> unresolved = new ArrayList<>();

  public List<GradleResolvedDependency> getResolvedDependencies() {
    return resolved;
  }

  @Override
  public List<GradleUnresolvedDependency> getUnresolvedDependencies() {
    return unresolved;
  }

  public List<GradleDependency> getBoms() {
    return boms;
  }
}
