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
import java.util.Set;

/**
 * GradleResolvedDependency models a gradle dependency that was successfully resolved, its children
 * in the graph and any artifacts associated with it.
 */
public interface GradleResolvedDependency {
  String getGroup();

  void setGroup(String group);

  String getName();

  void setName(String name);

  String getVersion();

  void setVersion(String version);

  Set<String> getRequestedVersions();

  void addRequestedVersion(String requestedVersion);

  boolean isConflict();

  void setConflict(boolean conflict);

  List<GradleResolvedDependency> getChildren();

  void setChildren(List<GradleResolvedDependency> children);

  boolean isFromBom();

  void setFromBom(boolean fromBom);

  List<GradleResolvedArtifact> getArtifacts();

  void setArtifacts(List<GradleResolvedArtifact> artifacts);

  void addArtifact(GradleResolvedArtifact artifact);
}
