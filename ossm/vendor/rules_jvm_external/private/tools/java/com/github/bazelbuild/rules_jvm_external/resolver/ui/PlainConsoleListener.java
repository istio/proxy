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

package com.github.bazelbuild.rules_jvm_external.resolver.ui;

import static com.github.bazelbuild.rules_jvm_external.resolver.events.DownloadEvent.Stage.STARTING;

import com.github.bazelbuild.rules_jvm_external.resolver.events.DownloadEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.events.Event;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.events.LogEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.events.PhaseEvent;
import java.util.Locale;

public class PlainConsoleListener implements EventListener {
  @Override
  public void onEvent(Event event) {
    if (event instanceof DownloadEvent) {
      DownloadEvent de = (DownloadEvent) event;
      if (de.getStage() == STARTING) {
        String method = de.getMethod();
        String prefix = method != null ? method + " " : "";
        System.err.println(prefix + de.getTarget());
      }
    }

    if (event instanceof LogEvent) {
      System.err.println("[WARNING]: " + event);
    }

    if (event instanceof PhaseEvent) {
      System.err.println(
          "Currently: " + ((PhaseEvent) event).getPhaseName().toLowerCase(Locale.ENGLISH));
    }
  }

  @Override
  public void close() {}
}
