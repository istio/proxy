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

import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.ExclusionImpl;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleDependency;
import com.github.jknack.handlebars.Context;
import com.github.jknack.handlebars.Handlebars;
import com.github.jknack.handlebars.Template;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

/**
 * GradleBuildScriptGenerator is responsible for generating the build.gradle.kts and init.gradle.kts
 * file that will allow us to setup a gradle project to resolve dependencies
 */
public class GradleBuildScriptGenerator {

  private static final Handlebars handlebars = new Handlebars();

  static {
    handlebars.registerHelper(
        "notEmpty",
        (context, options) -> {
          if (context != null && !context.toString().trim().isEmpty()) {
            return options.fn(context);
          } else {
            return options.inverse(context);
          }
        });
  }

  /**
   * Renders the init gradle script used for bootstrapping our custom plugin for resolving
   * dependencies with the gradle daemon.
   *
   * @param initScriptTemplatePath The handlebars template which has a placeholder for the path for
   *     the plugin jar
   * @param initScriptOutputPath The rendered template which will be used as init.gradle
   * @param pluginJarPath The actual path to the single jar which has the plugin.
   * @throws IOException
   */
  public static void generateInitScript(
      Path initScriptTemplatePath, Path initScriptOutputPath, Path pluginJarPath)
      throws IOException {
    String templateContent = Files.readString(initScriptTemplatePath);
    Template template = handlebars.compileInline(templateContent);

    Map<String, Object> contextMap = new HashMap<>();

    contextMap.put("pluginJarPath", pluginJarPath.toUri().toString());
    String output = template.apply(Context.newContext(contextMap)).trim();
    Files.writeString(initScriptOutputPath, output);
  }

  /**
   * Renders the build.gradle.kts for the fake project which has the dependencies, boms and
   * repositories requested after which resolution can be performed
   *
   * @param gradleBuildScriptTemplate - the handlebars template with placeholders for generating
   *     build.gradle
   * @param outputGradleBuildScript - the rendered build.gradle.kts to be used with the fake project
   *     for resolution
   * @param repositories - a list of Maven repositories to be used in resolution
   * @param dependencies - a list of dependencies to be resolved/requested
   * @param boms - a list of BOMs to be resolved
   * @param globalExclusions - a list of dependencies to be excluded in resolution
   */
  public static void generateBuildScript(
      Path gradleBuildScriptTemplate,
      Path outputGradleBuildScript,
      List<Repository> repositories,
      List<GradleDependency> dependencies,
      List<GradleDependency> boms,
      List<ExclusionImpl> globalExclusions,
      boolean isUsingM2Local)
      throws IOException {
    String templateContent = Files.readString(gradleBuildScriptTemplate);

    // Compile the template
    Template template = handlebars.compileInline(templateContent);

    // Build the Handlebars context
    Map<String, Object> contextMap = new HashMap<>();

    contextMap.put("isUsingM2Local", isUsingM2Local);
    contextMap.put(
        "repositories",
        repositories.stream()
            .map(
                repo -> {
                  Map<String, Object> map = new HashMap<>();
                  map.put("url", repo.getUrl());
                  if (repo.getUrl().startsWith("http://localhost")
                      || (repo.getUrl().startsWith("http://")
                          && repo.getUrl().contains("localhost:"))) {
                    map.put("allowInsecureProtocol", true);
                  } else {
                    map.put("allowInsecureProtocol", false);
                  }
                  map.put("requiresAuth", repo.requiresAuth);
                  if (repo.requiresAuth) {
                    map.put("usernameProperty", repo.usernameProperty);
                    map.put("passwordProperty", repo.passwordProperty);
                  }
                  return map;
                })
            .collect(Collectors.toList()));

    // If multiple versions of a BOM exists, we want to enforce the first one like maven
    // Unlike maven, gradle seems to choose the highest versions, so this emulates the maven
    // behavior by picking the first BOM version in the order we see.
    Map<String, String> enforcedBomVersions = new LinkedHashMap<>();
    boms.forEach(
        bom -> {
          String artifactKey = bom.getGroup() + ":" + bom.getArtifact();
          if (!enforcedBomVersions.containsKey(artifactKey)) {
            enforcedBomVersions.put(artifactKey, bom.getVersion());
          }
        });

    // Now get the boms filtered to include the "winning" boms if there are multiple versions of a
    // given one
    List<GradleDependency> actualBoms =
        boms.stream()
            .filter(
                bom -> {
                  String artifactKey = bom.getGroup() + ":" + bom.getArtifact();
                  return enforcedBomVersions.containsKey(artifactKey)
                      && enforcedBomVersions.get(artifactKey).equals(bom.getVersion());
                })
            .collect(Collectors.toList());

    contextMap.put(
        "boms",
        actualBoms.stream()
            .map(
                dep -> {
                  Map<String, Object> map = new HashMap<>();
                  map.put("group", dep.getGroup());
                  map.put("artifact", dep.getArtifact());
                  map.put("version", dep.getVersion());
                  return map;
                })
            .collect(Collectors.toList()));

    contextMap.put(
        "dependencies",
        dependencies.stream()
            .map(
                dep -> {
                  Map<String, Object> map = new HashMap<>();
                  map.put("group", dep.getGroup());
                  map.put("artifact", dep.getArtifact());
                  if (dep.getVersion() != null && !dep.getVersion().isEmpty()) {
                    map.put("version", ":" + dep.getVersion());
                  }
                  if (dep.getClassifier() != null && !dep.getClassifier().isEmpty()) {
                    map.put("classifier", ":" + dep.getClassifier());
                  } else {
                    map.put("classifier", "");
                  }
                  if (dep.getExtension() != null
                      && dep.getClassifier() != null
                      && !dep.getClassifier().isEmpty()) {
                    map.put("extension", "@" + dep.getExtension());
                  } else {
                    map.put("extension", "");
                  }

                  if (dep.getExclusions() != null && !dep.getExclusions().isEmpty()) {
                    contextMap.put(
                        "exclusions",
                        dep.getExclusions().stream()
                            .map(
                                exclusion -> {
                                  Map<String, Object> localExclusions = new HashMap<>();
                                  localExclusions.put("group", exclusion.getGroup());
                                  localExclusions.put("module", exclusion.getModule());
                                  return localExclusions;
                                })
                            .collect(Collectors.toList()));
                  }

                  return map;
                })
            .collect(Collectors.toList()));

    contextMap.put(
        "globalExclusions",
        globalExclusions.stream()
            .map(
                exclusion -> {
                  Map<String, Object> globalExcludes = new HashMap<>();
                  if (exclusion.getGroup() != null && !exclusion.getGroup().isEmpty()) {
                    globalExcludes.put("group", exclusion.getGroup());
                    globalExcludes.put("module", exclusion.getModule());
                  }
                  return globalExcludes;
                })
            .collect(Collectors.toList()));

    // Render the template and write the actual build file
    String output = template.apply(Context.newContext(contextMap)).trim();
    Files.writeString(outputGradleBuildScript, output);
  }
}
