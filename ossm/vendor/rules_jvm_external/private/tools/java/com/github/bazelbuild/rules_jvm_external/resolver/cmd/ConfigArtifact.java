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

package com.github.bazelbuild.rules_jvm_external.resolver.cmd;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.google.common.collect.ImmutableSet;
import java.util.Set;

public class ConfigArtifact {

  // Field names are derived from the values used in `specs.bzl`
  // Getters are named after the equivalent fields in Maven coordinates
  private String group;
  private String artifact;
  private String classifier;
  private String packaging;
  private String version;
  private Set<String> exclusions;
  private boolean force_version;

  public String getGroupId() {
    return group;
  }

  public String getArtifactId() {
    return artifact;
  }

  public String getClassifier() {
    return classifier;
  }

  public String getExtension() {
    return packaging;
  }

  public String getVersion() {
    return version;
  }

  public Set<Coordinates> getExclusions() {
    if (exclusions == null) {
      return Set.of();
    }

    return exclusions.stream().map(Coordinates::new).collect(ImmutableSet.toImmutableSet());
  }

  public boolean isForceVersion() {
    return force_version;
  }
}
