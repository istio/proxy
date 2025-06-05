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

import static com.github.bazelbuild.rules_jvm_external.ZipUtils.createJar;
import static com.github.bazelbuild.rules_jvm_external.ZipUtils.readJar;
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.jar.Attributes;
import java.util.jar.JarFile;
import java.util.jar.Manifest;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class MergeJarsTest {

  @Rule public TemporaryFolder temp = new TemporaryFolder();

  @Test
  public void shouldGenerateAnEmptyJarIfNoSourcesAreGiven() throws IOException {
    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(new String[] {"--output", outputJar.toAbsolutePath().toString()});

    assertTrue(Files.exists(outputJar));
    assertTrue(Files.size(outputJar) > 0);
  }

  @Test
  public void shouldGenerateAJarContainingAllTheClassesFromASingleSource() throws IOException {
    Path inputOne = temp.newFile("first.jar").toPath();
    createJar(inputOne, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);
    // We expect the manifest and one file
    assertEquals(2, contents.size());
    assertEquals("Hello, World!", contents.get("com/example/A.class"));
  }

  @Test
  public void shouldMergeMultipleSourceJarsIntoASingleOutputJar() throws IOException {
    Path inputOne = temp.newFile("first.jar").toPath();
    createJar(inputOne, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path inputTwo = temp.newFile("second.jar").toPath();
    createJar(inputTwo, ImmutableMap.of("com/example/foo/B.class", "Also hello"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);
    // We expect the manifest and one file
    assertEquals(3, contents.size());
    assertEquals("Hello, World!", contents.get("com/example/A.class"));
    assertEquals("Also hello", contents.get("com/example/foo/B.class"));
  }

  @Test
  public void shouldAllowDuplicateClassesByDefaultAndLastOneInWins() throws IOException {
    // Create jars with names such that the first is sorted after the second
    Path inputOne = temp.newFile("beta.jar").toPath();
    createJar(inputOne, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path inputTwo = temp.newFile("alpha.jar").toPath();
    createJar(inputTwo, ImmutableMap.of("com/example/A.class", "Farewell!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);
    // We expect the manifest and one file
    assertEquals(2, contents.size());
    assertEquals("Farewell!", contents.get("com/example/A.class"));
  }

  @Test
  public void shouldBeAbleToSpecifyThatFirstSeenClassShouldBeIncludedInMergedJar()
      throws IOException {
    // Create jars with names such that the first is sorted after the second
    Path inputOne = temp.newFile("beta.jar").toPath();
    createJar(inputOne, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path inputTwo = temp.newFile("alpha.jar").toPath();
    createJar(inputTwo, ImmutableMap.of("com/example/A.class", "Farewell!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString(),
          "--duplicates", "first-wins"
        });

    Map<String, String> contents = readJar(outputJar);
    // We expect the manifest and one file
    assertEquals(2, contents.size());
    assertEquals("Hello, World!", contents.get("com/example/A.class"));
  }

  @Test(expected = IOException.class)
  public void duplicateClassesCanBeDeclaredAsErrors() throws IOException {
    // Create jars with names such that the first is sorted after the second
    Path inputOne = temp.newFile("beta.jar").toPath();
    createJar(inputOne, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path inputTwo = temp.newFile("alpha.jar").toPath();
    createJar(inputTwo, ImmutableMap.of("com/example/A.class", "Farewell!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString(),
          "--duplicates", "are-errors"
        });
  }

  @Test
  public void identicalDuplicateClassesAreFine() throws IOException {
    // Create jars with names such that the first is sorted after the second
    Path inputOne = temp.newFile("beta.jar").toPath();
    createJar(inputOne, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path inputTwo = temp.newFile("alpha.jar").toPath();
    createJar(inputTwo, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString(),
          "--duplicates", "are-errors"
        });

    Map<String, String> contents = readJar(outputJar);
    // We expect the manifest and one file
    assertEquals(2, contents.size());
    assertEquals("Hello, World!", contents.get("com/example/A.class"));
  }

  @Test
  public void shouldUseDifferentTimesForSourceAndClassFiles() throws IOException {
    Path inputOne = temp.newFile("first.jar").toPath();
    createJar(
        inputOne,
        new ImmutableMap.Builder<String, String>()
            .put("com/example/A.class", "Hello, Class!")
            .put("com/example/A.java", "Hello, Source!")
            .build());

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString()
        });

    Map<String, Long> entryTimestamps = readJarTimeStamps(outputJar);
    assertEquals(3, entryTimestamps.size());
    assertTrue(
        entryTimestamps.get("com/example/A.class") > entryTimestamps.get("com/example/A.java"));
  }

  @Test
  public void shouldBeAbleToExcludeClassesFromMergedJar() throws IOException {
    // Create jars with names such that the first is sorted after the second
    Path includeFrom = temp.newFile("include.jar").toPath();
    createJar(
        includeFrom,
        ImmutableMap.of(
            "com/example/A.class", "Hello, World!",
            "com/example/B.class", "I like cheese!"));

    Path excludeFrom = temp.newFile("exclude.jar").toPath();
    createJar(excludeFrom, ImmutableMap.of("com/example/A.class", "Something else!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", includeFrom.toAbsolutePath().toString(),
          "--exclude", excludeFrom.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);
    // We expect the manifest and one file
    assertEquals(2, contents.size());
    assertEquals("I like cheese!", contents.get("com/example/B.class"));
  }

  @Test
  public void shouldNotExcludeLicenseFiles() throws IOException {
    // Create jars with names such that the first is sorted after the second
    Path includeFrom = temp.newFile("include.jar").toPath();
    createJar(includeFrom, ImmutableMap.of("LICENSE", "Hello, World!"));

    Path excludeFrom = temp.newFile("exclude.jar").toPath();
    createJar(excludeFrom, ImmutableMap.of("LICENSE", "Something else!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", includeFrom.toAbsolutePath().toString(),
          "--exclude", excludeFrom.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);
    // We expect the manifest and one file
    assertEquals(2, contents.size());
    assertEquals("Hello, World!", contents.get("LICENSE"));
  }

  @Test
  public void shouldNotIncludeManifestOrMetaInfEntriesFromExclusions() throws IOException {
    // Create jars with names such that the first is sorted after the second
    Path includeFrom = temp.newFile("include.jar").toPath();
    createJar(includeFrom, ImmutableMap.of("META-INF/foo", "Hello, World!"));

    Path excludeFrom = temp.newFile("exclude.jar").toPath();
    createJar(excludeFrom, ImmutableMap.of("META-INF/foo", "Something else!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", includeFrom.toAbsolutePath().toString(),
          "--exclude", excludeFrom.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);
    // We expect the manifest and the one meta inf entry
    assertEquals(2, contents.size());
    assertEquals("Hello, World!", contents.get("META-INF/foo"));
  }

  @Test
  public void canMergeJarsWhereADirectoryAndFileShareTheSamePath() throws IOException {
    Path inputOne = temp.newFile("one.jar").toPath();
    createJar(inputOne, ImmutableMap.of("example/file.txt", "Yellow!"));

    Path inputTwo = temp.newFile("two.jar").toPath();
    createJar(inputTwo, ImmutableMap.of("example", "Purple!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);

    // One entry for the manifest, one for the file "example", and one for "example/file.txt"
    assertEquals("Yellow!", contents.get("example/file.txt"));
    assertEquals("Purple!", contents.get("example"));
  }

  @Test
  public void canMergeJarsWithDirectoriesWithTheSameName() throws IOException {
    Path inputOne = temp.newFile("one.jar").toPath();

    // We actually need the directory entries this time
    try (OutputStream os = Files.newOutputStream(inputOne);
        ZipOutputStream zos = new ZipOutputStream(os)) {
      ZipEntry e = new ZipEntry("META-INF/services/");
      zos.putNextEntry(e);
      zos.closeEntry();
      e = new ZipEntry("META-INF/services/one.txt");
      zos.putNextEntry(e);
      zos.write("Hello".getBytes(UTF_8));
      zos.closeEntry();
    }

    Path inputTwo = temp.newFile("two.jar").toPath();
    try (OutputStream os = Files.newOutputStream(inputTwo);
        ZipOutputStream zos = new ZipOutputStream(os)) {
      ZipEntry e = new ZipEntry("META-INF/services/");
      zos.putNextEntry(e);
      zos.closeEntry();
      e = new ZipEntry("META-INF/services/two.txt");
      zos.putNextEntry(e);
      zos.write("Hello".getBytes(UTF_8));
      zos.closeEntry();
    }

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);
    assertTrue(contents.containsKey("META-INF/services/one.txt"));
    assertTrue(contents.containsKey("META-INF/services/two.txt"));
  }

  @Test
  public void aMergedJarShouldHaveTheManifestAsTheFirstOrSecondEntry() throws IOException {
    // This is required to allow JarInputStream to read the manifest properly
    Path inputOne = temp.newFile("one.jar").toPath();

    Manifest firstManifest = new Manifest();
    firstManifest.getMainAttributes().put(Attributes.Name.MANIFEST_VERSION, "1.0");
    firstManifest.getMainAttributes().put(new Attributes.Name("First"), "foo");
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    firstManifest.write(bos);

    // Note: none of these jars have the manifest as one of the first two entries
    createJar(
        inputOne,
        ImmutableMap.of(
            "META-INF/MANA", "Yellow!",
            "META-INF/MANB", "Red!",
            "META-INF/MANIFEST.MF", bos.toString("UTF-8")));

    Path inputTwo = temp.newFile("two.jar").toPath();

    Manifest secondManifest = new Manifest();
    secondManifest.getMainAttributes().put(Attributes.Name.MANIFEST_VERSION, "1.0");
    secondManifest.getMainAttributes().put(new Attributes.Name("Second"), "bar");
    bos = new ByteArrayOutputStream();
    secondManifest.write(bos);

    createJar(
        inputTwo,
        ImmutableMap.of(
            "META-INF/MANC", "Purple!",
            "META-INF/MAND", "Green!",
            "META-INF/MANIFEST.MF", bos.toString("UTF-8")));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString()
        });

    try (InputStream is = Files.newInputStream(outputJar);
        ZipInputStream zis = new ZipInputStream(is)) {
      Set<String> names = new HashSet<>();
      names.add(zis.getNextEntry().getName());
      names.add(zis.getNextEntry().getName());

      assertTrue(
          "Manifest is not one of the first entries.", names.contains("META-INF/MANIFEST.MF"));
    }
  }

  @Test
  public void shouldAddMissingDirectories() throws IOException {
    Path input = temp.newFile("example.jar").toPath();
    createJar(input, ImmutableMap.of("foo/bar/baz.txt", "Hello, World!"));

    Path output = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", output.toAbsolutePath().toString(),
          "--sources", input.toAbsolutePath().toString(),
        });

    List<String> dirNames = readDirNames(output);

    assertTrue(dirNames.toString(), dirNames.contains("foo/"));
    assertTrue(dirNames.toString(), dirNames.contains("foo/bar/"));
  }

  @Test
  public void shouldNotBeConfusedBySimilarNamesWhenCreatingDirectories() throws IOException {
    Path input = temp.newFile("example.jar").toPath();
    createJar(
        input,
        ImmutableMap.of(
            "foo/bar/baz.txt", "Hello, World!",
            "foo/bar/baz/qux.txt", "Goodbye, cruel World!"));

    Path output = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", output.toAbsolutePath().toString(),
          "--sources", input.toAbsolutePath().toString(),
        });

    List<String> dirNames = readDirNames(output);

    assertTrue(dirNames.toString(), dirNames.contains("foo/"));
    assertTrue(dirNames.toString(), dirNames.contains("foo/bar/"));
    assertTrue(dirNames.toString(), dirNames.contains("foo/bar/baz/"));

    Map<String, String> contents = readJar(output);
    assertEquals("Hello, World!", contents.get("foo/bar/baz.txt"));
    assertEquals("Goodbye, cruel World!", contents.get("foo/bar/baz/qux.txt"));
  }

  @Test
  public void orderingOfAutomaticallyCreatedDirectoriesIsConduciveToSensibleUnpacking()
      throws IOException {
    Path input = temp.newFile("example.jar").toPath();
    createJar(
        input,
        ImmutableMap.of(
            "foo/bar/baz/qux/quux.txt", "Greetings, fellow mortal",
            "foo/bar/baz.txt", "Hello, World!"));

    Path output = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", output.toAbsolutePath().toString(),
          "--sources", input.toAbsolutePath().toString(),
        });

    // We want `foo/` to appear before `foo/bar/` and so on so that a simple unzipper can
    // just walk the zip, creating directories as it goes, and have everything work the
    // way we expect.
    List<String> dirNames = readDirNames(output);

    int indexOfFoo = dirNames.indexOf("foo/");
    int indexOfBar = dirNames.indexOf("foo/bar/");

    assertTrue(indexOfBar > indexOfFoo);

    int indexOfBaz = dirNames.indexOf("foo/bar/baz/");

    assertTrue(indexOfBaz > indexOfBar);

    int indexOfQux = dirNames.indexOf("foo/bar/baz/qux/");

    assertTrue(indexOfQux > indexOfBaz);
  }

  @Test
  public void shouldCreateIntermediateDirectoriesEvenIfTheyExistInTheSourceJar()
      throws IOException {
    Path input = temp.newFile("example.jar").toPath();
    createJar(
        input,
        ImmutableMap.of(
            "foo/", "",
            "foo/bar", "",
            "foo/bar/baz.txt", "cheese"));

    Path output = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", output.toAbsolutePath().toString(),
          "--sources", input.toAbsolutePath().toString(),
        });

    List<String> dirNames = readDirNames(output);
    assertTrue(dirNames.contains("foo/"));
    assertTrue(dirNames.contains("foo/bar/"));
  }

  @Test
  public void mergedJarManifestSpecialAttributesAreHandled() throws IOException {
    // This is required to allow JarInputStream to read the manifest properly
    Path inputOne = temp.newFile("one.jar").toPath();

    Manifest firstManifest = new Manifest();
    firstManifest.getMainAttributes().put(Attributes.Name.MANIFEST_VERSION, "1.0");
    firstManifest.getMainAttributes().put(new Attributes.Name("First"), "foo");
    firstManifest
        .getMainAttributes()
        .put(new Attributes.Name("Target-Label"), "@secret_corp//:foo");
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    firstManifest.write(bos);

    createJar(inputOne, ImmutableMap.of("META-INF/MANIFEST.MF", bos.toString("UTF-8")));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString()
        });

    try (JarFile jar = new JarFile(outputJar.toFile())) {
      assertTrue(
          jar.getManifest().getMainAttributes().containsKey(new Attributes.Name("Created-By")));
      assertFalse(
          jar.getManifest().getMainAttributes().containsKey(new Attributes.Name("Target-Label")));
    }
  }

  @Test
  public void mergedJarServiceProviderFilePreservesComments() throws IOException {
    Path inputOne = temp.newFile("one.jar").toPath();
    String inputOneContents = "# This is a comment\n# This is another comment\ncom.example.Foo";
    createJar(
        inputOne,
        ImmutableMap.of("META-INF/services/com.example.ServiceProvider", inputOneContents));

    Path inputTwo = temp.newFile("two.jar").toPath();
    String inputTwoContents = "# My License\ncom.example.Bar";
    createJar(
        inputTwo,
        ImmutableMap.of("META-INF/services/com.example.ServiceProvider", inputTwoContents));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString(),
        });

    Map<String, String> contents = readJar(outputJar);

    String expected = String.join("\n\n", inputOneContents, inputTwoContents) + "\n";

    assertEquals(expected, contents.get("META-INF/services/com.example.ServiceProvider"));
  }

  @Test
  public void mergedJarKeepsNonClassFiles() throws IOException {
    Path inputOne = temp.newFile("one.jar").toPath();
    createJar(inputOne, ImmutableMap.of("log4j.properties", "log4j.rootLogger=ERROR,stdout"));

    Path excludeOne = temp.newFile("two.jar").toPath();
    createJar(excludeOne, ImmutableMap.of("log4j.properties", "log4j.rootLogger=ERROR"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--exclude", excludeOne.toAbsolutePath().toString(),
        });

    Map<String, String> contents = readJar(outputJar);

    assertEquals("log4j.rootLogger=ERROR,stdout", contents.get("log4j.properties"));
  }

  @Test
  public void mergedJarKeepsNonClassFilesDefaultDuplicateStrategy() throws IOException {
    Path inputOne = temp.newFile("one.jar").toPath();
    createJar(inputOne, ImmutableMap.of("log4j.properties", "log4j.rootLogger=ERROR,stdout"));
    Path inputTwo = temp.newFile("two.jar").toPath();
    createJar(
        inputTwo, ImmutableMap.of("log4j.properties", "log4j.rootLogger=ERROR,stdout,stderr"));

    Path excludeOne = temp.newFile("three.jar").toPath();
    createJar(excludeOne, ImmutableMap.of("log4j.properties", "log4j.rootLogger=ERROR"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString(),
          "--exclude", excludeOne.toAbsolutePath().toString(),
        });

    Map<String, String> contents = readJar(outputJar);

    assertEquals("log4j.rootLogger=ERROR,stdout,stderr", contents.get("log4j.properties"));
  }

  @Test
  public void mergedJarKeepsNonClassFilesFirstWinsStrategy() throws IOException {
    Path inputOne = temp.newFile("one.jar").toPath();
    createJar(inputOne, ImmutableMap.of("log4j.properties", "log4j.rootLogger=ERROR,stdout"));
    Path inputTwo = temp.newFile("two.jar").toPath();
    createJar(
        inputTwo, ImmutableMap.of("log4j.properties", "log4j.rootLogger=ERROR,stdout,stderr"));

    Path excludeOne = temp.newFile("three.jar").toPath();
    createJar(excludeOne, ImmutableMap.of("log4j.properties", "log4j.rootLogger=ERROR"));

    Path outputJar = temp.newFile("out.jar").toPath();

    MergeJars.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--sources", inputOne.toAbsolutePath().toString(),
          "--sources", inputTwo.toAbsolutePath().toString(),
          "--duplicates", "first-wins",
          "--exclude", excludeOne.toAbsolutePath().toString(),
        });

    Map<String, String> contents = readJar(outputJar);

    assertEquals("log4j.rootLogger=ERROR,stdout", contents.get("log4j.properties"));
  }

  private Map<String, Long> readJarTimeStamps(Path jar) throws IOException {
    ImmutableMap.Builder<String, Long> builder = ImmutableMap.builder();

    try (InputStream is = Files.newInputStream(jar);
        ZipInputStream zis = new ZipInputStream(is)) {

      for (ZipEntry entry = zis.getNextEntry(); entry != null; entry = zis.getNextEntry()) {
        if (entry.isDirectory()) {
          continue;
        }

        builder.put(entry.getName(), entry.getTime());
      }
    }

    return builder.build();
  }

  private static List<String> readDirNames(Path output) throws IOException {
    // Ordering matters! Retain insertion order
    ImmutableList.Builder<String> dirNames = ImmutableList.builder();

    try (InputStream is = Files.newInputStream(output);
        ZipInputStream zis = new ZipInputStream(is)) {
      ZipEntry entry = zis.getNextEntry();
      while (entry != null) {
        if (entry.isDirectory()) {
          dirNames.add(entry.getName());
        }
        entry = zis.getNextEntry();
      }
    }

    return dirNames.build();
  }
}
