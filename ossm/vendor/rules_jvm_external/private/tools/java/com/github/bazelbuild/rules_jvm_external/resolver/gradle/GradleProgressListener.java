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

package com.github.bazelbuild.rules_jvm_external.resolver.gradle;

import com.github.bazelbuild.rules_jvm_external.resolver.events.DownloadEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.events.LogEvent;
import java.util.ArrayList;
import java.util.List;
import org.gradle.tooling.Failure;
import org.gradle.tooling.events.ProgressEvent;
import org.gradle.tooling.events.ProgressListener;
import org.gradle.tooling.events.configuration.ProjectConfigurationFailureResult;
import org.gradle.tooling.events.configuration.ProjectConfigurationFinishEvent;
import org.gradle.tooling.events.download.FileDownloadFinishEvent;
import org.gradle.tooling.events.download.FileDownloadStartEvent;

/**
 * Listens to download start/ending events for artifacts from the Gradle daemon while resolving
 * dependencies.
 */
public class GradleProgressListener implements ProgressListener {
  private static final String DOWNLOAD = "Download ";
  private final EventListener listener;
  private final List<Exception> exceptions = new ArrayList<>();

  public GradleProgressListener(EventListener listener) {
    this.listener = listener;
  }

  @Override
  public void statusChanged(ProgressEvent progressEvent) {
    if (progressEvent instanceof FileDownloadStartEvent) {
      String name = progressEvent.getDescriptor().getName();
      if (name == null) {
        return;
      }
      if (name.startsWith(DOWNLOAD)) {
        name = name.substring(DOWNLOAD.length());
      }

      listener.onEvent(new DownloadEvent(DownloadEvent.Stage.STARTING, name));
    } else if (progressEvent instanceof FileDownloadFinishEvent) {
      String name = progressEvent.getDescriptor().getName();
      if (name == null) {
        return;
      }
      if (name.startsWith(DOWNLOAD)) {
        name = name.substring(DOWNLOAD.length());
      }
      listener.onEvent(new DownloadEvent(DownloadEvent.Stage.COMPLETE, name));
    } else if (progressEvent instanceof ProjectConfigurationFailureResult) {
      String name = progressEvent.getDescriptor().getName();
      if (name == null) {
        return;
      }
      List<Failure> failures =
          (List<Failure>) ((ProjectConfigurationFailureResult) progressEvent).getFailures();
      StringBuilder message = new StringBuilder("Gradle project configuration failed: \n");
      for (Failure failure : failures) {
        message.append(failure.getMessage()).append("\n");
      }
      listener.onEvent(new LogEvent("gradle", name, message.toString()));
    } else if (progressEvent instanceof ProjectConfigurationFinishEvent) {
      String name = progressEvent.getDescriptor().getName();
      if (name == null) {
        return;
      }
      listener.onEvent(new LogEvent("gradle", name, "Gradle project configuration finished"));
    }
  }
}
