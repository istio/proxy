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

import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleDependencyModel;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;
import org.gradle.tooling.GradleConnector;
import org.gradle.tooling.ProjectConnection;

/**
 * Used for building a fake gradle project with dependencies we have to run a custom gradle plugin
 * to build the dependency and artifact graph.
 */
public class GradleProject implements AutoCloseable {

  private final Path projectDir;
  private final Path gradleCacheDir;
  private final Path gradleJavaHome;
  private final EventListener eventListener;
  private final Path initScript;

  private ProjectConnection connection;

  public GradleProject(
      Path projectDir,
      Path gradleCacheDir,
      Path gradleJavaHome,
      Path initScript,
      EventListener eventListener) {
    this.projectDir = Objects.requireNonNull(projectDir);
    this.gradleCacheDir = Objects.requireNonNull(gradleCacheDir);
    this.gradleJavaHome = gradleJavaHome;
    this.eventListener = Objects.requireNonNull(eventListener);
    this.initScript = Objects.requireNonNull(initScript);
  }

  public void setupProject() throws IOException {
    Files.createDirectories(projectDir);

    Files.writeString(
        projectDir.resolve("settings.gradle"),
        "rootProject.name = 'rules_jvm_external'\n",
        StandardOpenOption.CREATE,
        StandardOpenOption.TRUNCATE_EXISTING);
  }

  public void connect(Path gradlePath) {
    System.setProperty("gradle.user.home", gradleCacheDir.toAbsolutePath().toString());
    System.setProperty("org.gradle.parallel", "true");
    connection =
        GradleConnector.newConnector()
            .forProjectDirectory(projectDir.toFile())
            .useInstallation(gradlePath.toFile())
            .connect();
  }

  /** Triggers dependency resolution by running the custom task to resolve gradle dependencies */
  public GradleDependencyModel resolveDependencies(Map<String, String> gradleProperties) {
    if (connection == null) {
      throw new IllegalStateException("Gradle connection not established. Call connect() first.");
    }

    // This allows us to pass sensitive information like repository credentials without
    // leaking it into the actual build file
    List<String> arguments =
        gradleProperties.entrySet().stream()
            .map(entry -> "-P" + entry.getKey() + "=" + entry.getValue())
            .collect(Collectors.toList());
    arguments.add("--init-script=" + this.initScript);

    return connection
        .model(GradleDependencyModel.class)
        .addProgressListener(new GradleProgressListener(eventListener))
        .setStandardError(System.err)
        .withArguments(arguments)
        // .setJvmArguments("-agentlib:jdwp=transport=dt_socket,server=y,suspend=y,address=*:5005")
        // uncomment if you want to debug the plugin itself
        .get();
  }

  @Override
  public void close() throws Exception {
    if (connection != null) {
      connection.close();
      connection = null;
    }
  }

  public Path getProjectDir() {
    return projectDir;
  }
}
