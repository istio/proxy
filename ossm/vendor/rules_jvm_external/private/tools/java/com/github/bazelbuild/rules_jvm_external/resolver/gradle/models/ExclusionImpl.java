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

/** An exclusion declared to be excluded in the gradle resolution process */
public class ExclusionImpl implements Serializable, Exclusion {
  public final String group;
  public final String module;

  public ExclusionImpl(String group, String module) {
    this.group = group;
    this.module = module;
  }

  public String getGroup() {
    return group;
  }

  public String getModule() {
    return module;
  }
}
