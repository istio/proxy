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

public class LogEvent implements Event {

  private final String source;
  private final String message;
  private final String detail;

  public LogEvent(String source, String message, String detail) {
    this.source = source;
    this.message = message;
    this.detail = detail;
  }

  @Override
  public String toString() {
    StringBuilder str = new StringBuilder(source).append(": ").append(message);
    if (detail != null && System.getenv("RJE_VERBOSE") != null) {
      str.append("\n").append(detail);
    }
    return str.toString();
  }
}
