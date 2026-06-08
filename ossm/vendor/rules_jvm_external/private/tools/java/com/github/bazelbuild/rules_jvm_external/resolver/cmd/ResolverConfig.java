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

package com.github.bazelbuild.rules_jvm_external.resolver.cmd;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolutionRequest;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.events.PhaseEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Map;
import java.util.TreeMap;

public class ResolverConfig {

  // Limit the number of threads we use. The `5` is derived from the default number of threads Maven
  // uses
  // https://maven.apache.org/guides/mini/guide-configuring-maven.html#configuring-parallel-artifact-resolution
  public static final int DEFAULT_MAX_THREADS =
      Math.min(5, Runtime.getRuntime().availableProcessors());
  private final ResolutionRequest request;
  private final boolean fetchSources;
  private final boolean fetchJavadoc;
  private final Netrc netrc;
  private final Path output;
  private final Path dependencyIndexOutput;
  private final Map<String, Integer> inputHash;
  private final int maxThreads;

  public ResolverConfig(EventListener listener, String... args) throws IOException {
    Path configPath = null;
    this.netrc = Netrc.fromUserHome();

    ResolutionRequest request = new ResolutionRequest();
    boolean fetchSources = false;
    boolean fetchJavadoc = false;
    int maxThreads = DEFAULT_MAX_THREADS;
    Path output = null;
    Path dependencyIndexOutput = null;
    Path inputHashPath = null;

    if (System.getenv("RJE_MAX_THREADS") != null) {
      maxThreads = Integer.parseInt(System.getenv("RJE_MAX_THREADS"));
    }

    String envUseUnsafeCache = System.getenv("RJE_UNSAFE_CACHE");
    if (envUseUnsafeCache != null) {
      if ("1".equals(envUseUnsafeCache) || Boolean.parseBoolean(envUseUnsafeCache)) {
        request.useUnsafeSharedCache(true);
      }
    }

    for (int i = 0; i < args.length; i++) {
      String bazelWorkspaceDir = System.getenv("BUILD_WORKSPACE_DIRECTORY");
      switch (args[i]) {
        case "--argsfile":
          i++;
          configPath = Paths.get(args[i]);
          break;

        case "--bom":
          i++;
          request.addBom(args[i]);
          break;

        case "--input-hash-path":
          i++;
          inputHashPath = Paths.get(args[i]);
          break;

        case "--javadocs":
          fetchJavadoc = true;
          break;

        case "--output":
          i++;
          if (bazelWorkspaceDir == null) {
            output = Paths.get(args[i]);
          } else {
            output = Paths.get(bazelWorkspaceDir).resolve(args[i]);
          }
          break;

        case "--dependency-index-output":
          i++;
          String workspaceDir = System.getenv("BUILD_WORKSPACE_DIRECTORY");
          if (workspaceDir == null) {
            dependencyIndexOutput = Paths.get(args[i]);
          } else {
            dependencyIndexOutput = Paths.get(workspaceDir).resolve(args[i]);
          }
          break;

        case "--sources":
          fetchSources = true;
          break;

        case "--repository":
          i++;
          request.addRepository(args[i]);
          break;

        case "--max-threads":
          i++;
          maxThreads = Integer.parseInt(args[i]);
          break;

        case "--use_unsafe_shared_cache":
          request.useUnsafeSharedCache(true);
          break;

        default:
          request.addArtifact(args[i]);
          break;
      }
    }

    if (configPath != null) {
      listener.onEvent(new PhaseEvent("Reading parameter file"));
      String rawJson = Files.readString(configPath);
      ExternalResolverConfig config =
          new Gson().fromJson(rawJson, new TypeToken<ExternalResolverConfig>() {}.getType());

      fetchJavadoc |= config.isFetchJavadoc();
      fetchSources |= config.isFetchSources();

      request.useUnsafeSharedCache(
          request.isUseUnsafeSharedCache() || config.isUsingUnsafeSharedCache());

      config.getRepositories().forEach(request::addRepository);

      config.getGlobalExclusions().forEach(request::exclude);

      config
          .getBoms()
          .forEach(
              art -> {
                StringBuilder coords =
                    new StringBuilder()
                        .append(art.getGroupId())
                        .append(":")
                        .append(art.getArtifactId())
                        .append(":")
                        .append("pom")
                        .append(":")
                        .append(art.getVersion());
                request.addBom(
                    coords.toString(),
                    art.getExclusions().stream()
                        .map(c -> c.getGroupId() + ":" + c.getArtifactId())
                        .toArray(String[]::new));
              });

      config
          .getArtifacts()
          .forEach(
              art -> {
                Coordinates coords =
                    new Coordinates(
                        art.getGroupId(),
                        art.getArtifactId(),
                        art.getExtension(),
                        art.getClassifier(),
                        art.getVersion());
                com.github.bazelbuild.rules_jvm_external.resolver.Artifact artifact =
                    new com.github.bazelbuild.rules_jvm_external.resolver.Artifact(
                        coords, art.getExclusions(), art.isForceVersion());
                request.addArtifact(artifact);
              });
    }

    if (inputHashPath != null) {
      String rawJson = Files.readString(inputHashPath);
      Map<String, Integer> json =
          new Gson().fromJson(rawJson, new TypeToken<Map<String, Integer>>() {}.getType());
      this.inputHash = new TreeMap<>(json);
    } else {
      this.inputHash = null;
    }

    if (request.getRepositories().isEmpty()) {
      request.addRepository("https://repo1.maven.org/maven2/");
    }

    this.request = request;
    this.fetchSources = fetchSources;
    this.fetchJavadoc = fetchJavadoc;
    this.maxThreads = maxThreads;
    this.output = output;
    this.dependencyIndexOutput = dependencyIndexOutput;
  }

  public ResolutionRequest getResolutionRequest() {
    return request;
  }

  public boolean isFetchSources() {
    return fetchSources;
  }

  public boolean isFetchJavadoc() {
    return fetchJavadoc;
  }

  public Netrc getNetrc() {
    return netrc;
  }

  public int getMaxThreads() {
    return maxThreads;
  }

  public Map<String, Integer> getInputHash() {
    return inputHash;
  }

  public Path getOutput() {
    return output;
  }

  public Path getDependencyIndexOutput() {
    return dependencyIndexOutput;
  }
}
