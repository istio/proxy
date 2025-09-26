// Licensed to the Software Freedom Conservancy (SFC) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The SFC licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package com.github.bazelbuild.rules_jvm_external.javadoc;

import static java.lang.Runtime.Version;
import static java.nio.charset.StandardCharsets.UTF_8;

import com.github.bazelbuild.rules_jvm_external.ByteStreams;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.OutputStream;
import java.io.Reader;
import java.io.StringWriter;
import java.io.UncheckedIOException;
import java.io.Writer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;
import javax.tools.DocumentationTool;
import javax.tools.JavaFileObject;
import javax.tools.StandardJavaFileManager;
import javax.tools.StandardLocation;
import javax.tools.ToolProvider;

public class JavadocJarMaker {

  private static final Version JAVA_9 = Version.parse("9");
  private static final Version JAVA_13 = Version.parse("13");

  public static void main(String[] args) throws IOException {
    Set<Path> sourceJars = new HashSet<>();
    Set<Path> resources = new HashSet<>();
    Path out = null;
    Path elementList = null;
    Set<Path> classpath = new HashSet<>();
    Set<String> excludedPackages = new HashSet<>();
    Set<String> includedPackages = new HashSet<>();
    List<String> options = new ArrayList<>();
    for (int i = 0; i < args.length; i++) {
      String flag = args[i];
      String next;

      switch (flag) {
        case "--cp":
          next = args[++i];
          classpath.add(Paths.get(next));
          break;

        case "--in":
          next = args[++i];
          sourceJars.add(Paths.get(next));
          break;

        case "--out":
          next = args[++i];
          out = Paths.get(next);
          break;

        case "--element-list":
          next = args[++i];
          elementList = Paths.get(next);
          break;

        case "--resources":
          next = args[++i];
          resources.add(Paths.get(next));
          break;

        case "--exclude-packages":
          next = args[++i];
          excludedPackages.add(next);
          break;

        case "--include-packages":
          next = args[++i];
          includedPackages.add(next);
          break;

        default:
          options.add(flag);
          break;
      }
    }

    if (sourceJars.isEmpty()) {
      throw new IllegalArgumentException(
          "At least one input just must be specified via the --in flag");
    }

    if (out == null) {
      throw new IllegalArgumentException(
          "The output jar location must be specified via the --out flag");
    }

    Path dir = Files.createTempDirectory("javadocs");
    Set<Path> tempDirs = new HashSet<>();
    tempDirs.add(dir);

    DocumentationTool tool = ToolProvider.getSystemDocumentationTool();
    try (StandardJavaFileManager fileManager =
        tool.getStandardFileManager(null, Locale.getDefault(), UTF_8)) {
      fileManager.setLocation(
          DocumentationTool.Location.DOCUMENTATION_OUTPUT, List.of(dir.toFile()));
      fileManager.setLocation(
          StandardLocation.CLASS_PATH,
          classpath.stream().map(Path::toFile).collect(Collectors.toSet()));

      Path unpackTo = Files.createTempDirectory("unpacked-sources");
      tempDirs.add(unpackTo);
      Map<String, List<JavaFileObject>> sources = new HashMap<>();
      readSourceFiles(unpackTo, fileManager, sourceJars, sources);
      Set<String> expandedExcludedPackages = expandPackages(excludedPackages, sources.keySet());
      Set<String> expandedIncludedPackages = expandPackages(includedPackages, sources.keySet());
      filterPackages(unpackTo, sources, expandedIncludedPackages, expandedExcludedPackages);
      Set<String> topLevelPackages =
          sources.keySet().stream().map(p -> p.split("\\.")[0]).collect(Collectors.toSet());

      // True if we're just exporting a set of modules
      if (sources.isEmpty()) {
        try (OutputStream os = Files.newOutputStream(out);
            ZipOutputStream zos = new ZipOutputStream(os)) {
          // It's enough to just create the thing
        }

        // We need to create the element list file so that the bazel rule calling us has the file
        // be created.
        if (elementList != null) {
          Files.write(
              elementList,
              "".getBytes(UTF_8),
              StandardOpenOption.CREATE,
              StandardOpenOption.TRUNCATE_EXISTING);
        }

        return;
      }

      if (!classpath.isEmpty()) {
        options.add("-cp");
        options.add(
            classpath.stream()
                .map(String::valueOf)
                .collect(Collectors.joining(File.pathSeparator)));
      }
      Version version = Runtime.version();

      // Generate frames if we can. Java prior to v9 generates frames automatically.
      // In Java 13, the flag was removed.
      if (version.compareTo(JAVA_9) > 0 && version.compareTo(JAVA_13) < 0) {
        options.add("--frames");
      }

      // If we can, generate HTML 5 documentation
      if (version.compareTo(JAVA_9) > 0) {
        options.add("-html5");
      }

      Path outputTo = Files.createTempDirectory("output-dir");
      tempDirs.add(outputTo);

      options.addAll(Arrays.asList("-d", outputTo.toAbsolutePath().toString()));

      // sourcepath and subpackages should work in most cases. A known edge case is when the package
      // names
      // don't match the directory structure. For example `Main.java` in
      // `tests/integration/java_export` has
      // a package of "com.jvm.external.jvm_export" but the file is in
      // `tests/integration/java_export/Main.java`.
      // The error comes from the javadoc tool itself. It seems that `-subpackage` looks at the
      // directory structure,
      // not the package name in the file. For this reason, include/exclude will not work when the
      // package name
      // doesn't match the directory structure.
      if (!expandedExcludedPackages.isEmpty()) {
        options.add("-sourcepath");
        options.add(unpackTo.toAbsolutePath().toString());

        options.add("-subpackages");
        options.add(String.join(":", topLevelPackages));

        // It might appear that -exclude is not needed since we
        // remove the source files, but without it
        // empty package info html files will still be generated.
        options.add("-exclude");
        options.add(String.join(":", expandedExcludedPackages));
      }

      sources.values().stream().flatMap(List::stream).forEach(s -> options.add(s.getName()));

      for (Path resource : resources) {
        Path target = outputTo.resolve(resource.getFileName());
        Files.createDirectories(target.getParent());
        Files.copy(resource, target);
      }

      Writer writer = new StringWriter();
      DocumentationTool.DocumentationTask task =
          tool.getTask(writer, fileManager, null, null, options, null);
      Boolean result = task.call();
      if (result == null || !result) {
        System.err.println("javadoc " + String.join(" ", options));
        System.err.println(writer);
        return;
      }

      Path generatedElementList = outputTo.resolve("element-list");
      try {
        Files.copy(generatedElementList, elementList);
      } catch (FileNotFoundException e) {
        // Do not fail the action if the generated element-list couldn't be found.
        Files.createFile(generatedElementList);
      }

      CreateJar.createJar(out, outputTo);
    }
    tempDirs.forEach(JavadocJarMaker::delete);
  }

  private static void delete(Path toDelete) {
    try {
      Files.walk(toDelete)
          .sorted(Comparator.reverseOrder())
          .map(Path::toFile)
          .forEach(File::delete);
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }

  private static void readSourceFiles(
      Path unpackTo,
      StandardJavaFileManager fileManager,
      Set<Path> sourceJars,
      Map<String, List<JavaFileObject>> sources)
      throws IOException {

    for (Path jar : sourceJars) {
      if (!Files.exists(jar)) {
        continue;
      }

      try (ZipInputStream zis = new ZipInputStream(Files.newInputStream(jar))) {
        for (ZipEntry entry = zis.getNextEntry(); entry != null; entry = zis.getNextEntry()) {
          String name = entry.getName();
          if (!name.endsWith(".java")) {
            continue;
          }

          Path target = unpackTo.resolve(name).normalize();
          if (!target.startsWith(unpackTo)) {
            throw new IOException("Attempt to write out of working directory");
          }

          Files.createDirectories(target.getParent());
          try (OutputStream out = Files.newOutputStream(target)) {
            ByteStreams.copy(zis, out);
          }

          fileManager
              .getJavaFileObjects(target.toFile())
              .forEach(
                  s -> {
                    String p = extractPackageName(s);
                    sources.computeIfAbsent(p, k -> new ArrayList<>()).add(s);
                  });
        }
      }
    }
  }

  // If the package ends in .* , then look for all subpackages in packages set
  private static Set<String> expandPackages(Set<String> wildcardedPackages, Set<String> packages) {
    Set<String> expandedPackages = new HashSet<>();

    for (String excludedPackage : wildcardedPackages) {
      if (excludedPackage.endsWith(".*")) {
        String basePackage = excludedPackage.substring(0, excludedPackage.length() - 2);
        for (String pkg : packages) {
          if (pkg.startsWith(basePackage)) {
            expandedPackages.add(pkg);
          }
        }
      } else {
        expandedPackages.add(excludedPackage);
      }
    }

    return expandedPackages;
  }

  // Extract the package name from the contents of the file
  private static String extractPackageName(JavaFileObject fileObject) {
    Set<String> keywords = Set.of("public", "class", "interface", "enum");
    try (Reader reader = fileObject.openReader(true);
        BufferedReader bufferedReader = new BufferedReader(reader)) {
      String line;
      while ((line = bufferedReader.readLine()) != null) {
        line = line.trim();
        if (line.startsWith("package ")) {
          return line.substring("package ".length(), line.indexOf(';')).trim();
        }

        // Stop looking if we hit the class or interface declaration
        if (keywords.stream().anyMatch(line::startsWith)) {
          break;
        }
      }
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }

    // default package
    return "";
  }

  private static void filterPackages(
      Path unpackTo,
      Map<String, List<JavaFileObject>> sources,
      Set<String> expandedIncludedPackages,
      Set<String> expandedExcludedPackages) {
    // If no "include" packages are specified, then include everything
    // minus the excluded packages.
    // If "include" packages are specified, then only include those packages
    // AND subtract the excluded packages.
    if (!expandedIncludedPackages.isEmpty()) {
      sources.keySet().retainAll(expandedIncludedPackages);
    }

    for (String excludedPackage : expandedExcludedPackages) {
      sources
          .getOrDefault(excludedPackage, new ArrayList<>())
          .forEach(
              s -> {
                try {
                  Files.deleteIfExists(unpackTo.resolve(s.getName()));
                } catch (IOException e) {
                  throw new UncheckedIOException(e);
                }
              });
      sources.remove(excludedPackage);
    }
  }
}
