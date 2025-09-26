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
import java.util.List;

/** Represents a Gradle dependency in build.gradle.kts */
public class GradleDependencyImpl implements Serializable, GradleDependency {
  public final String group;
  public final String artifact;
  public final String version;
  public final List<Exclusion> exclusions;
  private final String classifier;
  private final String extension;

  public GradleDependencyImpl(
      String group,
      String artifact,
      String version,
      List<Exclusion> exclusions,
      String classifier,
      String extension) {
    this.group = group;
    this.artifact = artifact;
    this.version = version;
    this.exclusions = exclusions != null ? exclusions : List.of();
    this.classifier = classifier;
    this.extension = extension;
  }

  public String getGroup() {
    return group;
  }

  public String getArtifact() {
    return artifact;
  }

  public String getVersion() {
    return version;
  }

  public String getClassifier() {
    return classifier;
  }

  public String getExtension() {
    return extension;
  }

  public List<Exclusion> getExclusions() {
    return exclusions;
  }
}
