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

package com.github.bazelbuild.rules_jvm_external.jar;

import static org.junit.Assert.assertEquals;

import com.google.devtools.build.runfiles.Runfiles;
import com.google.gson.Gson;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.SortedSet;
import java.util.TreeMap;
import java.util.TreeSet;
import org.junit.Test;

public class IndexJarTest {
  @Test
  public void simplePackages() throws Exception {
    doTest(
        "hamcrest_core_for_test/file/hamcrest-core-1.3.jar",
        sortedSet("org.hamcrest", "org.hamcrest.core", "org.hamcrest.internal"),
        new TreeMap<>());
  }

  @Test
  public void hasModuleInfo() throws Exception {
    doTest(
        "gson_for_test/file/gson-2.9.0.jar",
        sortedSet(
            "com.google.gson",
            "com.google.gson.annotations",
            "com.google.gson.internal",
            "com.google.gson.internal.bind",
            "com.google.gson.internal.bind.util",
            "com.google.gson.internal.reflect",
            "com.google.gson.internal.sql",
            "com.google.gson.reflect",
            "com.google.gson.stream"),
        new TreeMap<>());
  }

  @Test
  public void multiVersioned() throws Exception {
    doTest(
        "junit_platform_commons_for_test/file/junit-platform-commons-1.8.2.jar",
        sortedSet(
            "org.junit.platform.commons",
            "org.junit.platform.commons.annotation",
            "org.junit.platform.commons.function",
            "org.junit.platform.commons.logging",
            "org.junit.platform.commons.support",
            "org.junit.platform.commons.util"),
        new TreeMap<>());
  }

  @Test
  public void noPackages() throws Exception {
    doTest(
        "hamcrest_core_srcs_for_test/file/hamcrest-core-1.3-sources.jar",
        sortedSet(),
        new TreeMap<>());
  }

  @Test
  public void serviceImplementations() throws Exception {
    doTest(
        "lombok_for_test/file/lombok-1.18.22.jar",
        sortedSet(
            "lombok",
            "lombok.delombok.ant",
            "lombok.experimental",
            "lombok.extern.apachecommons",
            "lombok.extern.flogger",
            "lombok.extern.jackson",
            "lombok.extern.java",
            "lombok.extern.jbosslog",
            "lombok.extern.log4j",
            "lombok.extern.slf4j",
            "lombok.javac.apt",
            "lombok.launch"),
        getLombokServiceImplementations());
  }

  @Test
  public void parseServiceImplementations_simple() throws Exception {
    try (InputStream inputStream = streamOf("com.example.Impl1\ncom.example.Impl2")) {
      SortedSet<String> impls = new IndexJar().parseServiceImplementations(inputStream);
      assertEquals(sortedSet("com.example.Impl1", "com.example.Impl2"), impls);
    }
  }

  @Test
  public void parseServiceImplementations_ignoresCommentsAndEmpty() throws Exception {
    String contents =
        "com.example.Impl1# end of line comment\n"
            + "# whole line comment\n"
            + " # whole line comment with leading space\n"
            + "\n"
            + "com.example .\tImpl2\n";
    try (InputStream inputStream = streamOf(contents)) {
      SortedSet<String> impls = new IndexJar().parseServiceImplementations(inputStream);
      assertEquals(sortedSet("com.example.Impl1", "com.example.Impl2"), impls);
    }
  }

  private InputStream streamOf(String string) {
    return new ByteArrayInputStream(string.getBytes(StandardCharsets.UTF_8));
  }

  @Test
  public void invalidCRC() throws Exception {
    doTest(
        "google_api_services_compute_javadoc_for_test/file/google-api-services-compute-v1-rev235-1.25.0-javadoc.jar",
        sortedSet(),
        new TreeMap<>());
  }

  private static class Lockfile {
    public TreeMap<String, TreeMap<String, TreeSet<String>>> services;
  }

  @Test
  public void testLockfile() throws Exception {
    Path lockfilePath =
        Paths.get(
            Runfiles.create()
                .rlocation(
                    "rules_jvm_external/tests/custom_maven_install/service_indexing_testing.json"));
    try (Reader reader = Files.newBufferedReader(lockfilePath)) {
      Gson gson = new Gson();
      Lockfile lockfile = gson.fromJson(reader, Lockfile.class);
      assertEquals(
          getLombokServiceImplementations(), lockfile.services.get("org.projectlombok:lombok"));
    }
  }

  private void doTest(
      String runfileJar,
      SortedSet<String> expectedPackages,
      TreeMap<String, TreeSet<String>> expectedServiceImplementations)
      throws IOException {
    Path jar = Paths.get(Runfiles.create().rlocation(runfileJar));
    PerJarIndexResults perJarIndexResults = new IndexJar().index(jar);
    assertEquals(expectedPackages, perJarIndexResults.getPackages());
    assertEquals(expectedServiceImplementations, perJarIndexResults.getServiceImplementations());
  }

  private TreeMap<String, TreeSet<String>> getLombokServiceImplementations() {
    TreeMap<String, TreeSet<String>> lombokServiceImplementations = new TreeMap<>();
    lombokServiceImplementations.put(
        "javax.annotation.processing.Processor",
        sortedSet(
            "lombok.launch.AnnotationProcessorHider$AnnotationProcessor",
            "lombok.launch.AnnotationProcessorHider$ClaimingProcessor"));
    lombokServiceImplementations.put(
        "lombok.core.LombokApp",
        sortedSet(
            "lombok.bytecode.PoolConstantsApp",
            "lombok.bytecode.PostCompilerApp",
            "lombok.core.Main$LicenseApp",
            "lombok.core.Main$VersionApp",
            "lombok.core.PublicApiCreatorApp",
            "lombok.core.configuration.ConfigurationApp",
            "lombok.core.runtimeDependencies.CreateLombokRuntimeApp",
            "lombok.delombok.DelombokApp",
            "lombok.installer.Installer$CommandLineInstallerApp",
            "lombok.installer.Installer$CommandLineUninstallerApp",
            "lombok.installer.Installer$GraphicalInstallerApp"));
    lombokServiceImplementations.put(
        "lombok.core.PostCompilerTransformation",
        sortedSet(
            "lombok.bytecode.PreventNullAnalysisRemover", "lombok.bytecode.SneakyThrowsRemover"));
    lombokServiceImplementations.put(
        "lombok.core.runtimeDependencies.RuntimeDependencyInfo",
        sortedSet("lombok.core.handlers.SneakyThrowsAndCleanupDependencyInfo"));
    lombokServiceImplementations.put(
        "lombok.eclipse.EclipseASTVisitor",
        sortedSet(
            "lombok.eclipse.handlers.HandleFieldDefaults", "lombok.eclipse.handlers.HandleVal"));
    lombokServiceImplementations.put(
        "lombok.eclipse.EclipseAnnotationHandler",
        sortedSet(
            "lombok.eclipse.handlers.HandleAccessors",
            "lombok.eclipse.handlers.HandleBuilder",
            "lombok.eclipse.handlers.HandleBuilderDefault",
            "lombok.eclipse.handlers.HandleCleanup",
            "lombok.eclipse.handlers.HandleConstructor$HandleAllArgsConstructor",
            "lombok.eclipse.handlers.HandleConstructor$HandleNoArgsConstructor",
            "lombok.eclipse.handlers.HandleConstructor$HandleRequiredArgsConstructor",
            "lombok.eclipse.handlers.HandleData",
            "lombok.eclipse.handlers.HandleDelegate",
            "lombok.eclipse.handlers.HandleEqualsAndHashCode",
            "lombok.eclipse.handlers.HandleExtensionMethod",
            "lombok.eclipse.handlers.HandleFieldNameConstants",
            "lombok.eclipse.handlers.HandleGetter",
            "lombok.eclipse.handlers.HandleHelper",
            "lombok.eclipse.handlers.HandleJacksonized",
            "lombok.eclipse.handlers.HandleLog$HandleCommonsLog",
            "lombok.eclipse.handlers.HandleLog$HandleCustomLog",
            "lombok.eclipse.handlers.HandleLog$HandleFloggerLog",
            "lombok.eclipse.handlers.HandleLog$HandleJBossLog",
            "lombok.eclipse.handlers.HandleLog$HandleJulLog",
            "lombok.eclipse.handlers.HandleLog$HandleLog4j2Log",
            "lombok.eclipse.handlers.HandleLog$HandleLog4jLog",
            "lombok.eclipse.handlers.HandleLog$HandleSlf4jLog",
            "lombok.eclipse.handlers.HandleLog$HandleXSlf4jLog",
            "lombok.eclipse.handlers.HandleNonNull",
            "lombok.eclipse.handlers.HandlePrintAST",
            "lombok.eclipse.handlers.HandleSetter",
            "lombok.eclipse.handlers.HandleSneakyThrows",
            "lombok.eclipse.handlers.HandleStandardException",
            "lombok.eclipse.handlers.HandleSuperBuilder",
            "lombok.eclipse.handlers.HandleSynchronized",
            "lombok.eclipse.handlers.HandleToString",
            "lombok.eclipse.handlers.HandleUtilityClass",
            "lombok.eclipse.handlers.HandleValue",
            "lombok.eclipse.handlers.HandleWith",
            "lombok.eclipse.handlers.HandleWithBy"));
    lombokServiceImplementations.put(
        "lombok.eclipse.handlers.EclipseSingularsRecipes$EclipseSingularizer",
        sortedSet(
            "lombok.eclipse.handlers.singulars.EclipseGuavaMapSingularizer",
            "lombok.eclipse.handlers.singulars.EclipseGuavaSetListSingularizer",
            "lombok.eclipse.handlers.singulars.EclipseGuavaTableSingularizer",
            "lombok.eclipse.handlers.singulars.EclipseJavaUtilListSingularizer",
            "lombok.eclipse.handlers.singulars.EclipseJavaUtilMapSingularizer",
            "lombok.eclipse.handlers.singulars.EclipseJavaUtilSetSingularizer"));
    lombokServiceImplementations.put(
        "lombok.installer.IdeLocationProvider",
        sortedSet(
            "lombok.installer.eclipse.AngularIDELocationProvider",
            "lombok.installer.eclipse.EclipseLocationProvider",
            "lombok.installer.eclipse.JbdsLocationProvider",
            "lombok.installer.eclipse.MyEclipseLocationProvider",
            "lombok.installer.eclipse.RhcrLocationProvider",
            "lombok.installer.eclipse.RhdsLocationProvider",
            "lombok.installer.eclipse.STS4LocationProvider",
            "lombok.installer.eclipse.STSLocationProvider"));
    lombokServiceImplementations.put(
        "lombok.javac.JavacASTVisitor",
        sortedSet("lombok.javac.handlers.HandleFieldDefaults", "lombok.javac.handlers.HandleVal"));
    lombokServiceImplementations.put(
        "lombok.javac.JavacAnnotationHandler",
        sortedSet(
            "lombok.javac.handlers.HandleAccessors",
            "lombok.javac.handlers.HandleBuilder",
            "lombok.javac.handlers.HandleBuilderDefault",
            "lombok.javac.handlers.HandleBuilderDefaultRemove",
            "lombok.javac.handlers.HandleBuilderRemove",
            "lombok.javac.handlers.HandleCleanup",
            "lombok.javac.handlers.HandleConstructor$HandleAllArgsConstructor",
            "lombok.javac.handlers.HandleConstructor$HandleNoArgsConstructor",
            "lombok.javac.handlers.HandleConstructor$HandleRequiredArgsConstructor",
            "lombok.javac.handlers.HandleData",
            "lombok.javac.handlers.HandleDelegate",
            "lombok.javac.handlers.HandleEqualsAndHashCode",
            "lombok.javac.handlers.HandleExtensionMethod",
            "lombok.javac.handlers.HandleFieldNameConstants",
            "lombok.javac.handlers.HandleGetter",
            "lombok.javac.handlers.HandleHelper",
            "lombok.javac.handlers.HandleJacksonized",
            "lombok.javac.handlers.HandleLog$HandleCommonsLog",
            "lombok.javac.handlers.HandleLog$HandleCustomLog",
            "lombok.javac.handlers.HandleLog$HandleFloggerLog",
            "lombok.javac.handlers.HandleLog$HandleJBossLog",
            "lombok.javac.handlers.HandleLog$HandleJulLog",
            "lombok.javac.handlers.HandleLog$HandleLog4j2Log",
            "lombok.javac.handlers.HandleLog$HandleLog4jLog",
            "lombok.javac.handlers.HandleLog$HandleSlf4jLog",
            "lombok.javac.handlers.HandleLog$HandleXSlf4jLog",
            "lombok.javac.handlers.HandleNonNull",
            "lombok.javac.handlers.HandlePrintAST",
            "lombok.javac.handlers.HandleSetter",
            "lombok.javac.handlers.HandleSneakyThrows",
            "lombok.javac.handlers.HandleStandardException",
            "lombok.javac.handlers.HandleSuperBuilder",
            "lombok.javac.handlers.HandleSuperBuilderRemove",
            "lombok.javac.handlers.HandleSynchronized",
            "lombok.javac.handlers.HandleToString",
            "lombok.javac.handlers.HandleUtilityClass",
            "lombok.javac.handlers.HandleValue",
            "lombok.javac.handlers.HandleWith",
            "lombok.javac.handlers.HandleWithBy"));
    lombokServiceImplementations.put(
        "lombok.javac.handlers.JavacSingularsRecipes$JavacSingularizer",
        sortedSet(
            "lombok.javac.handlers.singulars.JavacGuavaMapSingularizer",
            "lombok.javac.handlers.singulars.JavacGuavaSetListSingularizer",
            "lombok.javac.handlers.singulars.JavacGuavaTableSingularizer",
            "lombok.javac.handlers.singulars.JavacJavaUtilListSingularizer",
            "lombok.javac.handlers.singulars.JavacJavaUtilMapSingularizer",
            "lombok.javac.handlers.singulars.JavacJavaUtilSetSingularizer"));
    return lombokServiceImplementations;
  }

  private TreeSet<String> sortedSet(String... contents) {
    TreeSet<String> set = new TreeSet<>();
    for (String string : contents) {
      set.add(string);
    }
    return set;
  }
}
