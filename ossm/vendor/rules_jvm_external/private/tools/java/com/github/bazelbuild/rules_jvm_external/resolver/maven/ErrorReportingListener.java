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

package com.github.bazelbuild.rules_jvm_external.resolver.maven;

import com.google.common.collect.ImmutableList;
import java.util.LinkedList;
import java.util.List;
import org.eclipse.aether.AbstractRepositoryListener;
import org.eclipse.aether.RepositoryEvent;

class ErrorReportingListener extends AbstractRepositoryListener {

  private final List<Exception> exceptions = new LinkedList<>();

  @Override
  public void artifactDescriptorInvalid(RepositoryEvent event) {
    if (event.getException() != null) {
      exceptions.add(event.getException());
    }
  }

  @Override
  public void artifactDescriptorMissing(RepositoryEvent event) {
    if (event.getException() != null) {
      exceptions.add(event.getException());
    }
  }

  @Override
  public void metadataInvalid(RepositoryEvent event) {
    if (event.getException() != null) {
      exceptions.add(event.getException());
    }
  }

  public List<Exception> getExceptions() {
    return ImmutableList.copyOf(exceptions);
  }
}
