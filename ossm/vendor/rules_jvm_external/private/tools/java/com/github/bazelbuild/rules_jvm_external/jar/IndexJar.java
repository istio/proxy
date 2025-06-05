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

import com.google.gson.Gson;
import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.AbstractMap;
import java.util.Arrays;
import java.util.Map;
import java.util.SortedMap;
import java.util.SortedSet;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.function.Predicate;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipInputStream;

public class IndexJar {

  private static final Predicate<String> IS_NUMERIC_VERSION =
      Pattern.compile("[1-9][0-9]*").asPredicate();

  private static final String SERVICES_DIRECTORY_PREFIX = "META-INF/services/";

  public static void main(String[] args) throws IOException {
    if (args.length != 2 || !"--argsfile".equals(args[0])) {
      System.err.printf("Required args: --argsfile /path/to/argsfile%n");
      System.exit(1);
    }

    Path argsFile = Paths.get(args[1]);
    Map<String, PerJarIndexResults> index = new IndexJar().index(Files.lines(argsFile));
    System.out.println(new Gson().toJson(index));
  }

  public Map<String, PerJarIndexResults> index(Stream<String> source) {
    TreeMap<String, PerJarIndexResults> index =
        source
            .parallel()
            .map(
                path -> {
                  try {
                    PerJarIndexResults results = index(Paths.get(path));
                    return new AbstractMap.SimpleEntry<>(path, results);
                  } catch (IOException e) {
                    throw new UncheckedIOException(e);
                  }
                })
            .collect(
                Collectors.toMap(
                    Map.Entry::getKey,
                    Map.Entry::getValue,
                    (left, right) -> {
                      throw new RuntimeException("Duplicate keys detected but not expected");
                    },
                    TreeMap::new));

    return index;
  }

  public PerJarIndexResults index(Path path) throws IOException {
    SortedSet<String> packages = new TreeSet<>();
    SortedMap<String, SortedSet<String>> serviceImplementations = new TreeMap<>();
    try (InputStream fis = new BufferedInputStream(Files.newInputStream(path));
        ZipInputStream zis = new ZipInputStream(fis)) {
      try {
        ZipEntry entry;
        while ((entry = zis.getNextEntry()) != null) {
          if (entry.getName().startsWith(SERVICES_DIRECTORY_PREFIX)
              && !SERVICES_DIRECTORY_PREFIX.equals(entry.getName())) {
            String serviceInterface = entry.getName().substring(SERVICES_DIRECTORY_PREFIX.length());
            SortedSet<String> implementingClasses = parseServiceImplementations(zis);
            serviceImplementations.put(serviceInterface, implementingClasses);
          }
          if (!entry.getName().endsWith(".class")) {
            continue;
          }
          if ("module-info.class".equals(entry.getName())
              || entry.getName().endsWith("/module-info.class")) {
            continue;
          }
          packages.add(extractPackageName(entry.getName()));
        }
      } catch (ZipException e) {
        System.err.printf("Caught ZipException: %s%n", e);
      }
      return new PerJarIndexResults(packages, serviceImplementations);
    }
  }

  // Visible for testing
  // Note that parseServiceImplementation does not close the passed InputStream, the caller is
  // responsible for doing this.
  // Implements as per https://docs.oracle.com/javase/8/docs/technotes/guides/jar/jar.html
  SortedSet<String> parseServiceImplementations(InputStream inputStream) throws IOException {
    SortedSet<String> implementingClasses = new TreeSet<>();
    // We can't close the inputStream here or if we're given a ZipInputStream it will also prevent
    // the caller from reading subsequent entries.
    BufferedReader bufferedReader = new BufferedReader(new InputStreamReader(inputStream));
    String implementingClass = bufferedReader.readLine();
    while (implementingClass != null) {
      implementingClass = implementingClass.replaceAll("[ \\t]", "").replaceFirst("#.*", "");
      if (!implementingClass.isEmpty()) {
        implementingClasses.add(implementingClass);
      }
      implementingClass = bufferedReader.readLine();
    }
    return implementingClasses;
  }

  private String extractPackageName(String zipEntryName) {
    String[] parts = zipEntryName.split("/");
    if (parts.length == 1) {
      return "";
    }
    int skip = 0;
    // As per https://docs.oracle.com/en/java/javase/13/docs/specs/jar/jar.html
    if (parts.length > 3
        && "META-INF".equals(parts[0])
        && "versions".equals(parts[1])
        && isNumericVersion(parts[2])) {
      skip = 3;
    }

    // -1 for the class name, -skip for the skipped META-INF prefix.
    int limit = parts.length - 1 - skip;
    return Arrays.stream(parts).skip(skip).limit(limit).collect(Collectors.joining("."));
  }

  private boolean isNumericVersion(String part) {
    return IS_NUMERIC_VERSION.test(part);
  }
}
