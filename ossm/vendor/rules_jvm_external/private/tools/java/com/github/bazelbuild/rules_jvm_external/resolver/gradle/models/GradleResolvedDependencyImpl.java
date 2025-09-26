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
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Represents a single dependency resolved by gradle */
public class GradleResolvedDependencyImpl implements Serializable, GradleResolvedDependency {
  private String group;
  private String name;
  private String version;
  private Set<String> requestedVersions;
  private boolean conflict;
  private List<GradleResolvedDependency> children;
  private boolean fromBom;
  private List<GradleResolvedArtifact> artifacts;

  public GradleResolvedDependencyImpl() {
    this.artifacts = new ArrayList<>();
    this.children = new ArrayList<>();
    this.requestedVersions = new HashSet<>();
  }

  public String getGroup() {
    return group;
  }

  public void setGroup(String group) {
    this.group = group;
  }

  public String getName() {
    return name;
  }

  public void setName(String name) {
    this.name = name;
  }

  public String getVersion() {
    return version;
  }

  public void setVersion(String version) {
    this.version = version;
  }

  public Set<String> getRequestedVersions() {
    return requestedVersions;
  }

  public void addRequestedVersion(String requestedVersion) {
    this.requestedVersions.add(requestedVersion);
  }

  public boolean isConflict() {
    return conflict;
  }

  public void setConflict(boolean conflict) {
    this.conflict = conflict;
  }

  public List<GradleResolvedDependency> getChildren() {
    return children;
  }

  public void setChildren(List<GradleResolvedDependency> children) {
    this.children = children;
  }

  public boolean isFromBom() {
    return fromBom;
  }

  public void setFromBom(boolean fromBom) {
    this.fromBom = fromBom;
  }

  @Override
  public List<GradleResolvedArtifact> getArtifacts() {
    return artifacts;
  }

  @Override
  public void setArtifacts(List<GradleResolvedArtifact> artifacts) {
    this.artifacts = artifacts;
  }

  @Override
  public void addArtifact(GradleResolvedArtifact artifact) {
    this.artifacts.add(artifact);
  }
}
