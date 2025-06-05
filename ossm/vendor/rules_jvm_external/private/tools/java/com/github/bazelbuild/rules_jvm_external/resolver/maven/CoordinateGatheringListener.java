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

import static com.github.bazelbuild.rules_jvm_external.resolver.maven.MavenPackagingMappings.mapPackagingToExtension;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import org.apache.maven.model.Model;
import org.apache.maven.model.io.xpp3.MavenXpp3Reader;
import org.codehaus.plexus.util.ReaderFactory;
import org.codehaus.plexus.util.xml.pull.XmlPullParserException;
import org.eclipse.aether.AbstractRepositoryListener;
import org.eclipse.aether.RepositoryEvent;
import org.eclipse.aether.artifact.Artifact;

public class CoordinateGatheringListener extends AbstractRepositoryListener {

  private final Map<Coordinates, Coordinates> knownRewrittenCoordinates = new ConcurrentHashMap<>();

  @Override
  public void artifactResolved(RepositoryEvent event) {

    Artifact artifact = event.getArtifact();
    if (!"pom".equals(artifact.getExtension())) {
      return;
    }

    File file = event.getFile();
    if (file == null) {
      return;
    }

    try (InputStream is = new FileInputStream(file);
        BufferedInputStream bis = new BufferedInputStream(is);
        Reader reader = ReaderFactory.newXmlReader(bis)) {
      MavenXpp3Reader mavenXpp3Reader = new MavenXpp3Reader();
      Model model = mavenXpp3Reader.read(reader);
      String packaging = model.getPackaging();

      if (packaging == null) {
        return;
      }

      packaging = packaging.trim();

      String extension = mapPackagingToExtension(packaging);
      // The default packaging is "jar" anyway
      if (extension.isEmpty() || "jar".equals(extension)) {
        return;
      }

      Coordinates coords =
          new Coordinates(
              artifact.getGroupId(), artifact.getArtifactId(), null, null, artifact.getVersion());

      Coordinates actualCoords =
          new Coordinates(
              artifact.getGroupId(),
              artifact.getArtifactId(),
              extension,
              artifact.getClassifier(),
              artifact.getVersion());

      knownRewrittenCoordinates.put(coords, actualCoords);
    } catch (IOException | XmlPullParserException e) {
      throw new RuntimeException("Unable to determine packaging", e);
    }
  }

  public Map<Coordinates, Coordinates> getRemappings() {
    return Map.copyOf(knownRewrittenCoordinates);
  }
}
