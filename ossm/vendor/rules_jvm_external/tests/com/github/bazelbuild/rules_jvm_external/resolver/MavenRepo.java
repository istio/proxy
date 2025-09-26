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

import static java.nio.charset.StandardCharsets.UTF_8;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.google.common.hash.Hashing;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import org.apache.maven.model.Dependency;
import org.apache.maven.model.Model;
import org.apache.maven.model.io.xpp3.MavenXpp3Writer;

public class MavenRepo {

  private final Path root;

  private MavenRepo(Path root) {
    this.root = root;
  }

  public static MavenRepo create() {
    Path root = null;
    try {
      root = Files.createTempDirectory("maven-repo");
      return new MavenRepo(root);
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }

  public MavenRepo add(Model item, Coordinates... deps) {
    // The coordinates we use need not be terribly complicated
    Coordinates coords =
        new Coordinates(
            item.getGroupId(), item.getArtifactId(), item.getPackaging(), null, item.getVersion());

    return add(coords, item, deps);
  }

  public MavenRepo add(Coordinates coords, Coordinates... deps) {
    Model model = new Model();
    model.setGroupId(coords.getGroupId());
    model.setArtifactId(coords.getArtifactId());
    model.setVersion(coords.getVersion());

    return add(coords, model, deps);
  }

  private MavenRepo add(Coordinates coords, Model ofCoordinates, Coordinates... deps) {
    try {
      writePomFile(ofCoordinates, deps);
      if (!"pom".equals(coords.getExtension())) {
        writeFile(coords);
      }

      return this;
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }

  public MavenRepo writePomFile(Model model) throws IOException {
    model.setModelVersion("4.0.0");

    Coordinates coords =
        new Coordinates(model.getGroupId(), model.getArtifactId(), null, null, model.getVersion());
    Path dir = root.resolve(coords.toRepoPath()).getParent();
    Files.createDirectories(dir);

    Path pomFile = dir.resolve(model.getArtifactId() + "-" + model.getVersion() + ".pom");
    try (BufferedWriter writer = Files.newBufferedWriter(pomFile)) {
      new MavenXpp3Writer().write(writer, model);
    }
    writeSha1File(pomFile);

    return this;
  }

  public MavenRepo writePomFile(Coordinates coords, String pomContents) throws IOException {
    Path dir = root.resolve(coords.toRepoPath()).getParent();
    Files.createDirectories(dir);

    Path pomFile = dir.resolve(coords.getArtifactId() + "-" + coords.getVersion() + ".pom");
    try (BufferedWriter writer = Files.newBufferedWriter(pomFile)) {
      writer.write(pomContents);
    }
    writeSha1File(pomFile);

    return this;
  }

  private void writePomFile(Model model, Coordinates... deps) throws IOException {
    for (Coordinates dep : deps) {
      Dependency mavenDep = new Dependency();
      mavenDep.setGroupId(dep.getGroupId());
      mavenDep.setArtifactId(dep.getArtifactId());
      mavenDep.setVersion(dep.getVersion());
      if (dep.getClassifier() != null && !dep.getClassifier().isEmpty()) {
        mavenDep.setClassifier(dep.getClassifier());
      }
      mavenDep.setType(dep.getExtension());

      model.addDependency(mavenDep);
    }

    writePomFile(model);
  }

  private void writeFile(Coordinates coords) throws IOException {
    Path output = root.resolve(coords.toRepoPath());
    // We don't read the contents, it just needs to exist
    Files.write(output, "Hello, World!".getBytes(UTF_8));

    writeSha1File(output);
  }

  private void writeSha1File(Path path) throws IOException {
    // Now write the checksum
    byte[] bytes = Files.readAllBytes(path);
    String hashCode = Hashing.sha1().hashBytes(bytes).toString();

    Path shaFile = Paths.get(path.toAbsolutePath() + ".sha1");
    Files.write(shaFile, hashCode.getBytes(UTF_8));
  }

  public Path getPath() {
    return root;
  }
}
