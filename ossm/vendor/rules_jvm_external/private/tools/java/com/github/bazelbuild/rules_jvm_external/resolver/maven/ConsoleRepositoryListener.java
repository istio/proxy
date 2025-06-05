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

import static com.github.bazelbuild.rules_jvm_external.resolver.events.DownloadEvent.Stage.COMPLETE;
import static com.github.bazelbuild.rules_jvm_external.resolver.events.DownloadEvent.Stage.STARTING;

import com.github.bazelbuild.rules_jvm_external.resolver.events.DownloadEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.events.PhaseEvent;
import java.io.Closeable;
import org.eclipse.aether.AbstractRepositoryListener;
import org.eclipse.aether.RepositoryEvent;

class ConsoleRepositoryListener extends AbstractRepositoryListener implements Closeable {

  private final EventListener listener;

  public ConsoleRepositoryListener(EventListener listener) {
    this.listener = listener;
  }

  @Override
  public void artifactDownloading(RepositoryEvent event) {
    listener.onEvent(
        new DownloadEvent(
            STARTING, MavenCoordinates.asCoordinates(event.getArtifact()).toString()));
  }

  @Override
  public void artifactDownloaded(RepositoryEvent event) {
    listener.onEvent(
        new DownloadEvent(
            COMPLETE, MavenCoordinates.asCoordinates(event.getArtifact()).toString()));
  }

  public void setPhase(String phaseDescription) {
    listener.onEvent(new PhaseEvent(phaseDescription));
  }

  @Override
  public void close() {
    listener.close();
  }
}
