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

package com.github.bazelbuild.rules_jvm_external.resolver.events;

import java.util.Objects;

public class DownloadEvent implements Event {

  private final Stage stage;
  private final String target;

  public DownloadEvent(Stage stage, String target) {
    this.stage = stage;
    this.target = Objects.requireNonNull(target);
  }

  public Stage getStage() {
    return stage;
  }

  public String getTarget() {
    return target;
  }

  public enum Stage {
    STARTING,
    COMPLETE,
  }
}
