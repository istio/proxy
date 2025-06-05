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

package com.github.bazelbuild.rules_jvm_external.jar;

import static com.github.bazelbuild.rules_jvm_external.jar.DuplicateEntryStrategy.LAST_IN_WINS;
import static java.util.zip.Deflater.BEST_COMPRESSION;
import static java.util.zip.ZipOutputStream.DEFLATED;

import com.github.bazelbuild.rules_jvm_external.ByteStreams;
import com.github.bazelbuild.rules_jvm_external.zip.StableZipEntry;
import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.TreeMap;
import java.util.jar.Attributes;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;

public class MergeJars {

  public static void main(String[] args) throws IOException {
    Path out = null;
    // Insertion order may matter
    Set<Path> sources = new LinkedHashSet<>();
    Set<Path> excludes = new HashSet<>();
    DuplicateEntryStrategy onDuplicate = LAST_IN_WINS;

    for (int i = 0; i < args.length; i++) {
      switch (args[i]) {
        case "--compression":
        case "--normalize":
          // ignore
          break;

        case "--duplicates":
          onDuplicate = DuplicateEntryStrategy.fromShortName(args[++i]);
          break;

        case "--exclude":
          excludes.add(isValid(Paths.get(args[++i])));
          break;

        case "--output":
          out = Paths.get(args[++i]);
          break;

        case "--sources":
          sources.add(isValid(Paths.get(args[++i])));
          break;

        default:
          throw new IllegalArgumentException(
              "Unable to parse command line: " + Arrays.toString(args));
      }
    }

    Objects.requireNonNull(out, "Output path must be set.");
    if (sources.isEmpty()) {
      // Just write an empty jar and leave
      try (OutputStream fos = Files.newOutputStream(out);
          JarOutputStream jos = new JarOutputStream(fos)) {
        // This space left blank deliberately
      }
      return;
    }

    // Remove any jars from sources that we've been told to exclude
    sources.removeIf(excludes::contains);

    // We would love to keep things simple by expanding all the input jars into
    // a single directory, but this isn't possible since one jar may contain a
    // file with the same name as a directory in another. *sigh* Instead, what
    // we'll do is create a list of contents from each jar, and where we should
    // pull the contents from. Whee!

    Manifest manifest = new Manifest();
    manifest.getMainAttributes().put(Attributes.Name.MANIFEST_VERSION, "1.0");

    Map<String, List<String>> allServices = new TreeMap<>();
    Set<String> excludedPaths = readExcludedClassNames(excludes);

    // Ultimately, we want the entries in the output zip to be sorted
    // so that we have a deterministic output.
    Map<String, Path> fileToSourceJar = new TreeMap<>();
    Map<String, byte[]> fileHashCodes = new HashMap<>();

    for (Path source : sources) {
      try (InputStream fis = Files.newInputStream(source);
          ZipInputStream zis = new ZipInputStream(fis)) {

        ZipEntry entry;
        while ((entry = zis.getNextEntry()) != null) {
          if ("META-INF/MANIFEST.MF".equals(entry.getName())) {
            Manifest other = new Manifest(zis);
            manifest = merge(manifest, other);
            continue;
          }

          if ("META-INF/".equals(entry.getName())
              || (!entry.getName().startsWith("META-INF/")
                  && excludedPaths.contains(entry.getName()))) {
            continue;
          }

          if (entry.getName().startsWith("META-INF/services/") && !entry.isDirectory()) {
            String servicesName = entry.getName().substring("META-INF/services/".length());
            List<String> services =
                allServices.computeIfAbsent(servicesName, key -> new ArrayList<>());
            services.add(new String(ByteStreams.toByteArray(zis)));
            continue;
          }

          if (!entry.isDirectory()) {
            // Duplicate files, however may not be. We need the hash to determine
            // whether we should do anything.
            byte[] hash = hash(zis);

            if (!fileToSourceJar.containsKey(entry.getName())) {
              fileToSourceJar.put(entry.getName(), source);
              fileHashCodes.put(entry.getName(), hash);
            } else {
              byte[] originalHashCode = fileHashCodes.get(entry.getName());
              boolean replace =
                  onDuplicate.isReplacingCurrent(entry.getName(), originalHashCode, hash);
              if (replace) {
                fileToSourceJar.put(entry.getName(), source);
                fileHashCodes.put(entry.getName(), hash);
              }
            }
          }
        }
      }
    }

    manifest.getMainAttributes().put(new Attributes.Name("Created-By"), "mergejars");
    // Bazel labels are an internal detail of the project producing the merged
    // jar and not useful for consumers.
    manifest.getMainAttributes().remove(new Attributes.Name("Target-Label"));

    // Now create the output jar
    Files.createDirectories(out.getParent());

    Set<String> createdDirectories = new HashSet<>();

    try (OutputStream os = Files.newOutputStream(out);
        JarOutputStream jos = new JarOutputStream(os)) {
      jos.setMethod(DEFLATED);
      jos.setLevel(BEST_COMPRESSION);

      // Write the manifest by hand to ensure the date is good
      ZipEntry entry = new StableZipEntry("META-INF/");
      jos.putNextEntry(entry);
      jos.closeEntry();
      createdDirectories.add(entry.getName());

      entry = new StableZipEntry("META-INF/MANIFEST.MF");
      ByteArrayOutputStream bos = new ByteArrayOutputStream();
      manifest.write(bos);
      entry.setSize(bos.size());
      jos.putNextEntry(entry);
      jos.write(bos.toByteArray());
      jos.closeEntry();

      if (!allServices.isEmpty()) {
        if (!createdDirectories.contains("META-INF/services/")) {
          entry = new StableZipEntry("META-INF/services/");
          jos.putNextEntry(entry);
          jos.closeEntry();
          createdDirectories.add(entry.getName());
        }
        for (Map.Entry<String, List<String>> kv : allServices.entrySet()) {
          entry = new StableZipEntry("META-INF/services/" + kv.getKey());
          bos = new ByteArrayOutputStream();

          bos.write(String.join("\n\n", kv.getValue()).getBytes());
          bos.write("\n".getBytes());
          entry.setSize(bos.size());
          jos.putNextEntry(entry);
          jos.write(bos.toByteArray());
          jos.closeEntry();
        }
      }

      Path previousSource = sources.isEmpty() ? null : sources.iterator().next();
      ZipFile source = previousSource == null ? null : new ZipFile(previousSource.toFile());

      // We should never enter this loop without there being any sources
      for (Map.Entry<String, Path> pathAndSource : fileToSourceJar.entrySet()) {
        // Get the original entry
        String name = pathAndSource.getKey();

        // Make sure we're not trying to create root entries.
        if (name.startsWith("/")) {
          if (name.length() == 1) {
            continue;
          }
          name = name.substring(1);
        }

        createDirectories(jos, name, createdDirectories);

        if (createdDirectories.contains(name)) {
          continue;
        }

        if (!Objects.equals(previousSource, pathAndSource.getValue())) {
          if (source != null) {
            source.close();
          }
          source = new ZipFile(pathAndSource.getValue().toFile());
          previousSource = pathAndSource.getValue();
        }

        ZipEntry original = source.getEntry(name);
        if (original == null) {
          continue;
        }

        ZipEntry je = new StableZipEntry(name);
        jos.putNextEntry(je);

        try (InputStream is = source.getInputStream(original)) {
          ByteStreams.copy(is, jos);
        }
        jos.closeEntry();
      }
      if (source != null) {
        source.close();
      }
    }
  }

  private static void createDirectories(JarOutputStream jos, String name, Set<String> createdDirs)
      throws IOException {
    if (!name.endsWith("/")) {
      int slashIndex = name.lastIndexOf('/');
      if (slashIndex != -1) {
        createDirectories(jos, name.substring(0, slashIndex + 1), createdDirs);
      }
      return;
    }

    if (createdDirs.contains(name)) {
      return;
    }

    String[] segments = name.split("/");
    StringBuilder path = new StringBuilder();
    for (String segment : segments) {
      path.append(segment).append('/');

      String newPath = path.toString();
      if (createdDirs.contains(newPath)) {
        continue;
      }

      ZipEntry entry = new StableZipEntry(newPath);
      jos.putNextEntry(entry);
      jos.closeEntry();
      createdDirs.add(newPath);
    }
  }

  private static Set<String> readExcludedClassNames(Set<Path> excludes) throws IOException {
    Set<String> paths = new HashSet<>();

    for (Path exclude : excludes) {
      try (InputStream is = Files.newInputStream(exclude);
          BufferedInputStream bis = new BufferedInputStream(is);
          ZipInputStream jis = new ZipInputStream(bis)) {
        ZipEntry entry;
        while ((entry = jis.getNextEntry()) != null) {
          if (entry.isDirectory()) {
            continue;
          }
          if (!entry.getName().endsWith(".class")) {
            continue;
          }

          String name = entry.getName();
          paths.add(name);
        }
      }
    }
    return paths;
  }

  private static Path isValid(Path path) {
    if (!Files.exists(path)) {
      throw new IllegalArgumentException("File does not exist: " + path);
    }

    if (!Files.isReadable(path)) {
      throw new IllegalArgumentException("File is not readable: " + path);
    }

    return path;
  }

  private static Manifest merge(Manifest into, Manifest from) {
    Attributes attributes = from.getMainAttributes();
    if (attributes != null) {
      attributes.forEach((key, value) -> into.getMainAttributes().put(key, value));
    }

    from.getEntries()
        .forEach(
            (key, value) -> {
              Attributes attrs = into.getAttributes(key);
              if (attrs == null) {
                attrs = new Attributes();
                into.getEntries().put(key, attrs);
              }
              attrs.putAll(value);
            });

    return into;
  }

  private static byte[] hash(InputStream inputStream) {
    try {
      MessageDigest digest = MessageDigest.getInstance("SHA-1");

      byte[] buf = new byte[100 * 1024];
      int read;

      while ((read = inputStream.read(buf)) != -1) {
        digest.update(buf, 0, read);
      }
      return digest.digest();
    } catch (IOException | NoSuchAlgorithmException e) {
      throw new RuntimeException(e);
    }
  }
}
