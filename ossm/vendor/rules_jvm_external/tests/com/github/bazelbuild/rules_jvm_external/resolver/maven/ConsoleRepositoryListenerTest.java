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

import static org.junit.Assert.assertTrue;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import org.eclipse.aether.DefaultRepositorySystemSession;
import org.eclipse.aether.RepositoryEvent;
import org.eclipse.aether.artifact.DefaultArtifact;
import org.junit.Test;

public class ConsoleRepositoryListenerTest {
  @Test
  public void shouldHandleUndeclaredCharacterEntityInPOM() throws IOException {
    Coordinates coords = new Coordinates("com.example:characterentity:1.0");

    String pomContents =
        "<project>\n"
            + "  <modelVersion>4.0.0</modelVersion>\n"
            + "  <groupId>"
            + coords.getGroupId()
            + "</groupId>\n"
            + "  <artifactId>"
            + coords.getArtifactId()
            + "</artifactId>\n"
            + "  <packaging>pom</packaging>\n"
            + "  <version>"
            + coords.getVersion()
            + "</version>\n"
            + "  <developers>\n"
            + "    <developer>\n"
            + "      <name>First Las&oslash;t</name>\n"
            + "    </developer>\n"
            + "  </developers>\n"
            + "</project>\n";

    File pomFile = File.createTempFile("console-repository-listener-test", ".pom");
    try (BufferedWriter writer = Files.newBufferedWriter(pomFile.toPath())) {
      writer.write(pomContents);
    }

    CoordinateGatheringListener coordinateListener = new CoordinateGatheringListener();
    RepositoryEvent.Builder repositoryEventBuilder =
        new RepositoryEvent.Builder(
            new DefaultRepositorySystemSession(), RepositoryEvent.EventType.ARTIFACT_RESOLVED);
    repositoryEventBuilder.setArtifact(
        new DefaultArtifact(
            coords.getGroupId(), coords.getArtifactId(), "pom", coords.getVersion()));
    repositoryEventBuilder.setFile(pomFile);

    coordinateListener.artifactResolved(repositoryEventBuilder.build());

    assertTrue(coordinateListener.getRemappings().isEmpty());
  }
}
