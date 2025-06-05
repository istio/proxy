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
import com.github.bazelbuild.rules_jvm_external.zip.StableZipEntry;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
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
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.stream.Collectors;
import java.util.stream.Stream;
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
          DocumentationTool.Location.DOCUMENTATION_OUTPUT, Arrays.asList(dir.toFile()));
      fileManager.setLocation(
          StandardLocation.CLASS_PATH,
          classpath.stream().map(Path::toFile).collect(Collectors.toSet()));

      Set<JavaFileObject> sources = new HashSet<>();
      Set<String> topLevelPackages = new HashSet<>();

      Path unpackTo = Files.createTempDirectory("unpacked-sources");
      tempDirs.add(unpackTo);
      Set<String> fileNames = new HashSet<>();
      readSourceFiles(unpackTo, fileManager, sourceJars, sources, topLevelPackages, fileNames);

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

      sources.forEach(obj -> options.add(obj.getName()));

      for (Path resource : resources) {
        Path target = outputTo.resolve(resource.getFileName());
        Files.createDirectories(target.getParent());
        Files.copy(resource, target);
      }

      Writer writer = new StringWriter();
      DocumentationTool.DocumentationTask task =
          tool.getTask(writer, fileManager, null, null, options, sources);
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

      try (OutputStream os = Files.newOutputStream(out);
          ZipOutputStream zos = new ZipOutputStream(os);
          Stream<Path> walk = Files.walk(outputTo)) {

        walk.sorted(Comparator.naturalOrder())
            .forEachOrdered(
                path -> {
                  if (path.equals(outputTo)) {
                    return;
                  }

                  try {
                    if (Files.isDirectory(path)) {
                      String name = outputTo.relativize(path) + "/";
                      ZipEntry entry = new StableZipEntry(name);
                      zos.putNextEntry(entry);
                      zos.closeEntry();
                    } else {
                      String name = outputTo.relativize(path).toString();
                      ZipEntry entry = new StableZipEntry(name);
                      zos.putNextEntry(entry);
                      try (InputStream is = Files.newInputStream(path)) {
                        ByteStreams.copy(is, zos);
                      }
                      zos.closeEntry();
                    }
                  } catch (IOException e) {
                    throw new UncheckedIOException(e);
                  }
                });
      }
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
      Set<JavaFileObject> sources,
      Set<String> topLevelPackages,
      Set<String> fileNames)
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

          fileManager.getJavaFileObjects(target.toFile()).forEach(sources::add);

          String[] segments = name.split("/");
          if (segments.length > 0 && !"META-INF".equals(segments[0])) {
            topLevelPackages.add(segments[0]);
          }

          fileNames.add(name);
        }
      }
    }
  }
}
