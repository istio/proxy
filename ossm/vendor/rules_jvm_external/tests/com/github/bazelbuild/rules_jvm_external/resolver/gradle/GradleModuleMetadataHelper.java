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

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.MavenRepo;
import java.io.IOException;
import java.io.Reader;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.Objects;
import javax.xml.stream.XMLEventFactory;
import javax.xml.stream.XMLEventReader;
import javax.xml.stream.XMLEventWriter;
import javax.xml.stream.XMLInputFactory;
import javax.xml.stream.XMLOutputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.events.XMLEvent;

/**
 * Helper class to manage gradle module metadata within ephemeral local maven repos for tests
 * related to the gradle resolver
 */
public class GradleModuleMetadataHelper {
  private final MavenRepo mavenRepo;
  private final String GRADLE_POM_MARKER = "do_not_remove: published-with-gradle-metadata";

  public GradleModuleMetadataHelper(MavenRepo mavenRepo) throws IOException {
    Objects.requireNonNull(mavenRepo);
    this.mavenRepo = mavenRepo;
  }

  public void addToMavenRepo(Coordinates coordinates, String moduleMetadata)
      throws IOException, XMLStreamException {
    // Add this to the maven repo with a POM file
    this.mavenRepo.add(coordinates);

    Path dir = this.mavenRepo.getPath().resolve(coordinates.toRepoPath()).getParent();

    Path moduleFile =
        dir.resolve(coordinates.getArtifactId() + "-" + coordinates.getVersion() + ".module");
    Files.write(moduleFile, moduleMetadata.getBytes());

    Path pomFile =
        dir.resolve(coordinates.getArtifactId() + "-" + coordinates.getVersion() + ".pom");

    injectGradleMarkerInPom(pomFile);
  }

  private void injectGradleMarkerInPom(Path pomPath) throws IOException, XMLStreamException {
    // To resolve artifacts with gradle metadata, gradle looks for a marker comment in pom.xml
    // e.g https://repo1.maven.org/maven2/com/squareup/okio/okio/3.6.0/okio-3.6.0.pom
    // so we insert them for the same reason for the pom.xml in these test cases

    String content = Files.readString(pomPath, StandardCharsets.UTF_8);
    if (content.contains(GRADLE_POM_MARKER)) {
      return; // already has marker, nothing to do
    }

    XMLInputFactory inFactory = XMLInputFactory.newInstance();
    XMLOutputFactory outFactory = XMLOutputFactory.newInstance();
    XMLEventFactory eventFactory = XMLEventFactory.newInstance();

    // Use a temp buffer to hold rewritten XML
    StringWriter buffer = new StringWriter();
    try (Reader in = Files.newBufferedReader(pomPath, StandardCharsets.UTF_8)) {
      XMLEventReader r = inFactory.createXMLEventReader(in);
      XMLEventWriter w = outFactory.createXMLEventWriter(buffer);

      boolean injected = false;
      while (r.hasNext()) {
        XMLEvent e = r.nextEvent();
        w.add(e);

        if (!injected
            && e.isStartElement()
            && e.asStartElement().getName().getLocalPart().equals("project")) {
          w.add(eventFactory.createCharacters("\n"));
          w.add(
              eventFactory.createComment(
                  "  This module was also published with a richer model, Gradle metadata,   "));
          w.add(eventFactory.createCharacters("\n"));
          w.add(
              eventFactory.createComment(
                  "  which should be used instead. Do not delete the following line which   "));
          w.add(eventFactory.createCharacters("\n"));
          w.add(
              eventFactory.createComment(
                  "  is to indicate to Gradle or any Gradle module metadata file consumer   "));
          w.add(eventFactory.createCharacters("\n"));
          w.add(
              eventFactory.createComment(
                  "  that they should prefer consuming it instead.                          "));
          w.add(eventFactory.createCharacters("\n"));
          w.add(
              eventFactory.createComment("  " + GRADLE_POM_MARKER + "                           "));
          w.add(eventFactory.createCharacters("\n"));
          injected = true;
        }
      }
      w.close();
      r.close();
    }

    Files.writeString(
        pomPath,
        buffer.toString(),
        StandardCharsets.UTF_8,
        StandardOpenOption.TRUNCATE_EXISTING,
        StandardOpenOption.WRITE);
  }
}
