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

package com.github.bazelbuild.rules_jvm_external.resolver;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import java.util.Objects;

/**
 * Represents a conflict that has been detected (and resolved) in the dependency resolution. The
 * requested and resolved versions are both stored to allow for display to the user.
 */
public class Conflict {

  private final Coordinates resolved;
  private final Coordinates requested;

  public Conflict(Coordinates resolved, Coordinates requested) {
    this.resolved = resolved;
    this.requested = requested;
  }

  public Coordinates getResolved() {
    return resolved;
  }

  public Coordinates getRequested() {
    return requested;
  }

  @Override
  public String toString() {
    return resolved + " -> " + requested;
  }

  @Override
  public boolean equals(Object o) {
    if (!(o instanceof Conflict)) {
      return false;
    }
    Conflict that = (Conflict) o;
    return Objects.equals(this.resolved, that.resolved)
        && Objects.equals(this.requested, that.requested);
  }

  @Override
  public int hashCode() {
    return Objects.hash(resolved, requested);
  }
}
