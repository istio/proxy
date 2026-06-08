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

import java.util.List;
import org.eclipse.aether.AbstractRepositoryListener;
import org.eclipse.aether.RepositoryEvent;
import org.eclipse.aether.RepositoryListener;

class CompoundListener extends AbstractRepositoryListener {
  private final List<RepositoryListener> listeners;

  public CompoundListener(RepositoryListener... listeners) {
    this.listeners = List.of(listeners);
  }

  @Override
  public void artifactDownloading(RepositoryEvent event) {
    listeners.forEach(listener -> listener.artifactDownloading(event));
  }

  @Override
  public void artifactDownloaded(RepositoryEvent event) {
    listeners.forEach(listener -> listener.artifactDownloaded(event));
  }

  @Override
  public void artifactResolving(RepositoryEvent event) {
    listeners.forEach(listener -> listener.artifactResolving(event));
  }

  @Override
  public void artifactResolved(RepositoryEvent event) {
    listeners.forEach(listener -> listener.artifactResolved(event));
  }

  @Override
  public void artifactDescriptorInvalid(RepositoryEvent event) {
    listeners.forEach(listener -> listener.artifactDescriptorInvalid(event));
  }

  @Override
  public void artifactDescriptorMissing(RepositoryEvent event) {
    listeners.forEach(listener -> listener.artifactDescriptorMissing(event));
  }

  @Override
  public void metadataInvalid(RepositoryEvent event) {
    listeners.forEach(listener -> listener.metadataInvalid(event));
  }

  @Override
  public void metadataResolved(RepositoryEvent event) {
    listeners.forEach(listener -> listener.metadataResolved(event));
  }
}
