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

public class GradleUnresolvedDependencyImpl implements GradleUnresolvedDependency, Serializable {
  private final String group;
  private final String name;
  private final String version;
  private final FailureReason failureReason;
  private final String failureDetails;

  public GradleUnresolvedDependencyImpl(
      String group,
      String name,
      String version,
      FailureReason failureReason,
      String failureDetails) {
    this.group = group;
    this.name = name;
    this.version = version;
    this.failureReason = failureReason;
    this.failureDetails = failureDetails;
  }

  @Override
  public String getGroup() {
    return this.group;
  }

  @Override
  public String getName() {
    return this.name;
  }

  @Override
  public String getVersion() {
    return this.version;
  }

  @Override
  public FailureReason getFailureReason() {
    return this.failureReason;
  }

  @Override
  public String getFailureDetails() {
    return this.failureDetails;
  }
}
