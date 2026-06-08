package com.github.bazelbuild.rules_jvm_external.jar;

import static com.github.bazelbuild.rules_jvm_external.jar.AddJarManifestEntry.AUTOMATIC_MODULE_NAME;
import static java.nio.charset.StandardCharsets.UTF_8;
import static java.util.jar.Attributes.Name.CLASS_PATH;
import static java.util.jar.Attributes.Name.MANIFEST_VERSION;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.common.io.ByteStreams;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.nio.file.attribute.FileTime;
import java.time.Instant;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TimeZone;
import java.util.jar.Attributes;
import java.util.jar.JarFile;
import java.util.jar.JarInputStream;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class AddJarManifestEntryTest {

  @Rule public TemporaryFolder temp = new TemporaryFolder();

  @Test
  public void shouldAddEntryToManifest() throws IOException {
    Path inputOne = temp.newFile("first.jar").toPath();

    Manifest manifest = new Manifest();
    manifest.getMainAttributes().put(new Attributes.Name("Hello-World"), "hello");

    createJar(
        inputOne,
        manifest,
        new ImmutableMap.Builder<String, String>()
            .put("com/example/A.class", "Hello, World!")
            .put("com/example/B.class", "Hello, World Again!")
            .build());

    Path outputJar = temp.newFile("out.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--source", inputOne.toAbsolutePath().toString(),
          "--manifest-entry", "Target-Label:@maven//:com_google_guava_guava"
        });

    Map<String, String> contents = readJar(outputJar);
    assertEquals(3, contents.size());
    assertTrue(contents.get("META-INF/MANIFEST.MF").contains("Manifest-Version: 1.0"));
    assertTrue(contents.get("META-INF/MANIFEST.MF").contains("Hello-World: hello"));
    assertTrue(
        contents
            .get("META-INF/MANIFEST.MF")
            .contains("Target-Label: @maven//:com_google_guava_guava"));
  }

  @Test
  public void shouldCreateManifestIfManifestIsMissing() throws IOException {
    Path inputOne = temp.newFile("first.jar").toPath();
    createJar(inputOne, null, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--source", inputOne.toAbsolutePath().toString(),
          "--manifest-entry", "Target-Label:@maven//:com_google_guava_guava"
        });

    Map<String, String> contents = readJar(outputJar);
    assertEquals(2, contents.size());
    assertTrue(contents.get("META-INF/MANIFEST.MF").contains("Manifest-Version: 1.0"));
    assertTrue(
        contents
            .get("META-INF/MANIFEST.MF")
            .contains("Target-Label: @maven//:com_google_guava_guava"));
    assertTrue(contents.get("META-INF/MANIFEST.MF").contains("Created-By: AddJarManifestEntry"));
  }

  @Test
  public void shouldCreateManifestAsFirstEntryInJarIfManifestIsMissing() throws IOException {
    Path inputOne = temp.newFile("first.jar").toPath();
    createJar(inputOne, null, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path outputJar = temp.newFile("out.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--source", inputOne.toAbsolutePath().toString(),
          "--manifest-entry", "Target-Label:@maven//:com_google_guava_guava"
        });

    List<String> entries = readJarEntries(outputJar);
    assertEquals(2, entries.size());
    assertEquals("META-INF/MANIFEST.MF", entries.get(0));
    assertEquals("com/example/A.class", entries.get(1));
  }

  @Test
  public void shouldOverwriteManifestEntryIfItAlreadyExistsInJar() throws IOException {
    Path inputOne = temp.newFile("first.jar").toPath();

    Manifest manifest = new Manifest();
    manifest
        .getMainAttributes()
        .put(new Attributes.Name("Target-Label"), "@maven//:org_hamcrest_hamcrest");

    createJar(
        inputOne,
        manifest,
        new ImmutableMap.Builder<String, String>()
            .put("com/example/A.class", "Hello, World!")
            .build());

    Path outputJar = temp.newFile("out.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--source", inputOne.toAbsolutePath().toString(),
          "--manifest-entry", "Target-Label:@maven//:com_google_guava_guava"
        });

    Map<String, String> contents = readJar(outputJar);
    assertEquals(2, contents.size());
    assertTrue(contents.get("META-INF/MANIFEST.MF").contains("Manifest-Version: 1.0"));
    assertTrue(
        contents
            .get("META-INF/MANIFEST.MF")
            .contains("Target-Label: @maven//:com_google_guava_guava"));
  }

  @Test
  public void doesNotAlterSignedJars() throws IOException {
    Path input = temp.newFile("in.jar").toPath();
    Path output = temp.newFile("out.jar").toPath();

    Manifest manifest = new Manifest();
    manifest.getMainAttributes().put(Attributes.Name.MANIFEST_VERSION, "1.0");
    createJar(input, manifest, ImmutableMap.of("META-INF/SOME_FILE.SF", ""));

    AddJarManifestEntry.main(
        new String[] {
          "--output", output.toAbsolutePath().toString(),
          "--source", input.toAbsolutePath().toString(),
          "--manifest-entry", "Target-Label:@boo//:scary"
        });

    byte[] in = Files.readAllBytes(input);
    byte[] out = Files.readAllBytes(output);

    assertArrayEquals(in, out);
  }

  @Test
  public void shouldLeaveOriginalManifestInPlaceIfNoClassPathIsThere() throws IOException {
    Path inJar = temp.newFile("input.jar").toPath();

    Manifest manifest = new Manifest();
    manifest.getMainAttributes().put(MANIFEST_VERSION, "1.0.0");
    Attributes.Name name = new Attributes.Name("Cheese");

    manifest.getMainAttributes().put(name, "Roquefort");
    createJar(inJar, manifest, new HashMap<>());

    Path outJar = temp.newFile("output.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--source", inJar.toAbsolutePath().toString(),
          "--output", outJar.toAbsolutePath().toString(),
          "--remove-entry", "Class-Path"
        });

    try (InputStream is = Files.newInputStream(outJar);
        JarInputStream jis = new JarInputStream(is)) {
      Manifest read = jis.getManifest();

      assertEquals("Roquefort", read.getMainAttributes().get(name));
    }
  }

  @Test
  public void shouldRemoveClassPathFromManifestIfPresent() throws IOException {
    Path inJar = temp.newFile("input.jar").toPath();

    Manifest manifest = new Manifest();
    manifest.getMainAttributes().put(MANIFEST_VERSION, "1.0.0");

    manifest.getMainAttributes().put(CLASS_PATH, "Brie");
    createJar(inJar, manifest, new HashMap<>());

    Path outJar = temp.newFile("output.jar").toPath();
    AddJarManifestEntry.main(
        new String[] {
          "--source", inJar.toAbsolutePath().toString(),
          "--output", outJar.toAbsolutePath().toString(),
          "--remove-entry", "Class-Path"
        });

    try (InputStream is = Files.newInputStream(outJar);
        JarInputStream jis = new JarInputStream(is)) {
      Manifest read = jis.getManifest();

      assertNull(read.getMainAttributes().get(CLASS_PATH));
    }
  }

  @Test
  public void shouldNotBeAffectedByTimezone() throws IOException {
    Path inputJar = temp.newFile("first.jar").toPath();

    createJar(inputJar, null, ImmutableMap.of("com/example/A.class", "Hello, World!"));

    Path outputJarOne = temp.newFile("out1.jar").toPath();
    Path outputJarTwo = temp.newFile("out2.jar").toPath();

    writeJarInTimezone(inputJar, outputJarOne, "EST");
    writeJarInTimezone(inputJar, outputJarTwo, "PST");

    assertArrayEquals(Files.readAllBytes(outputJarOne), Files.readAllBytes(outputJarTwo));
  }

  @Test
  public void shouldNotBeAffectedByTimezoneWithExtendedTimestamps() throws IOException {
    Path inputJar = temp.newFile("first.jar").toPath();

    createJar(inputJar, null, ImmutableMap.of("com/example/A.class", "Hello, World!"), true);

    Path outputJarOne = temp.newFile("out1.jar").toPath();
    Path outputJarTwo = temp.newFile("out2.jar").toPath();

    writeJarInTimezone(inputJar, outputJarOne, "EST");
    writeJarInTimezone(inputJar, outputJarTwo, "PST");

    assertArrayEquals(Files.readAllBytes(outputJarOne), Files.readAllBytes(outputJarTwo));
  }

  /**
   * There are jars in the wild which cannot be unpacked by zip or bazel's own zipper. These fail to
   * unpack because the jar contains a directory and a file that would unpack to the same path. Make
   * sure our header jar creator handles this.
   */
  @Test
  public void shouldBeAbleToCopeWithUnpackableJars() throws IOException {
    Path inJar = temp.newFile("input.jar").toPath();

    try (OutputStream is = Files.newOutputStream(inJar);
        JarOutputStream jos = new JarOutputStream(is)) {
      ZipEntry entry = new ZipEntry("foo");
      jos.putNextEntry(entry);
      jos.write("Hello, World!".getBytes(UTF_8));

      entry = new ZipEntry("foo/");
      jos.putNextEntry(entry);
    }

    Path outJar = temp.newFile("output.jar").toPath();

    // No exception is a Good Thing
    AddJarManifestEntry.main(
        new String[] {
          "--source", inJar.toAbsolutePath().toString(),
          "--output", outJar.toAbsolutePath().toString(),
          "--remove-entry", "Class-Path"
        });
  }

  /**
   * One pattern for making "executable jars" is to use a preamble to the zip archive. When this is
   * done `ZipInputStream` wrongly indicates that there are no entries in the jar, which means that
   * the header jar generated is empty, which leads to obvious issues.
   */
  @Test
  public void shouldStillIncludeClassesIfThereIsAShellPreambleToTheJar() throws IOException {
    Path tempJar = temp.newFile("regular.jar").toPath();

    createJar(
        tempJar,
        null,
        ImmutableMap.of(
            "Foo.class", "0xDECAFBAD",
            "Bar.class", "0xDEADBEEF"));

    // Write the preamble. We do things this way because a plain text
    // file is not a valid zip file.
    Path inJar = temp.newFile("input.jar").toPath();
    Files.write(inJar, "#!/bin/bash\necho Hello, World\n\n".getBytes(UTF_8));

    try (InputStream is = Files.newInputStream(tempJar);
        OutputStream os = Files.newOutputStream(inJar, StandardOpenOption.APPEND)) {
      ByteStreams.copy(is, os);
    }

    Path outJar = temp.newFile("output.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--source", inJar.toAbsolutePath().toString(),
          "--output", outJar.toAbsolutePath().toString(),
          "--manifest-entry", "Target-Label:@maven//:com_google_guava_guava"
        });

    try (JarFile jarFile = new JarFile(outJar.toFile())) {
      assertNotNull(jarFile.getEntry("Foo.class"));
      assertNotNull(jarFile.getEntry("Bar.class"));
    }
  }

  @Test
  public void aValidAutomaticModuleNameIsLeftAlone() throws IOException {
    Path inJar = createJarWithAutomaticModuleName("it.will.be.fine");
    Path outJar = temp.newFile("output.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--source", inJar.toAbsolutePath().toString(),
          "--output", outJar.toAbsolutePath().toString(),
          "--make-safe"
        });

    Manifest manifest = readManifest(outJar);
    String name = manifest.getMainAttributes().getValue(AUTOMATIC_MODULE_NAME);

    assertEquals("it.will.be.fine", name);
  }

  @Test
  public void anEmptyAutomaticModuleNameIsRemoved() throws IOException {
    Path inJar = createJarWithAutomaticModuleName("");
    Path outJar = temp.newFile("output.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--source", inJar.toAbsolutePath().toString(),
          "--output", outJar.toAbsolutePath().toString(),
          "--make-safe"
        });

    Manifest manifest = readManifest(outJar);

    assertFalse(manifest.getMainAttributes().containsKey(AUTOMATIC_MODULE_NAME));
  }

  @Test
  public void anAutomaticModuleNameThatIsNotAValidJavaIdentifierIsRemoved() throws IOException {
    // The `-` means that this isn't a valid package name
    Path inJar = createJarWithAutomaticModuleName("some.invalid.package-name");
    Path outJar = temp.newFile("output.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--source", inJar.toAbsolutePath().toString(),
          "--output", outJar.toAbsolutePath().toString(),
          "--make-safe"
        });

    Manifest manifest = readManifest(outJar);

    assertFalse(manifest.getMainAttributes().containsKey(AUTOMATIC_MODULE_NAME));
  }

  @Test
  public void aJavaReservedKeywordIsNotAllowedAsPartOfAnAutomaticModuleName() throws IOException {
    // Both `boolean` and `package` are reserved words
    Path inJar = createJarWithAutomaticModuleName("some.boolean.package.name");
    Path outJar = temp.newFile("output.jar").toPath();

    AddJarManifestEntry.main(
        new String[] {
          "--source", inJar.toAbsolutePath().toString(),
          "--output", outJar.toAbsolutePath().toString(),
          "--make-safe"
        });

    Manifest manifest = readManifest(outJar);

    assertFalse(manifest.getMainAttributes().containsKey(AUTOMATIC_MODULE_NAME));
  }

  private Manifest readManifest(Path fromJar) throws IOException {
    try (InputStream is = Files.newInputStream(fromJar);
        JarInputStream jis = new JarInputStream(is)) {
      return jis.getManifest();
    }
  }

  private Path createJarWithAutomaticModuleName(String name) throws IOException {
    Manifest manifest = new Manifest();
    manifest.getMainAttributes().put(MANIFEST_VERSION, "1.0");
    manifest.getMainAttributes().put(AUTOMATIC_MODULE_NAME, name);

    Path jar = temp.newFile("automatic-module.jar").toPath();
    try (OutputStream os = Files.newOutputStream(jar)) {
      new JarOutputStream(os, manifest).close();
    }
    return jar;
  }

  private void createJar(Path outputTo, Manifest manifest, Map<String, String> pathToContents)
      throws IOException {
    createJar(outputTo, manifest, pathToContents, false);
  }

  private void createJar(
      Path outputTo,
      Manifest manifest,
      Map<String, String> pathToContents,
      boolean setExtendedTimestamps)
      throws IOException {
    try (OutputStream os = Files.newOutputStream(outputTo);
        ZipOutputStream zos = new JarOutputStream(os)) {

      if (manifest != null) {
        manifest.getMainAttributes().put(Attributes.Name.MANIFEST_VERSION, "1.0");
        ZipEntry manifestEntry = new ZipEntry(JarFile.MANIFEST_NAME);

        zos.putNextEntry(manifestEntry);
        manifest.write(zos);
      }

      for (Map.Entry<String, String> entry : pathToContents.entrySet()) {
        ZipEntry ze = new ZipEntry(entry.getKey());
        if (setExtendedTimestamps) {
          ze.setLastModifiedTime(FileTime.from(Instant.now()));
        }
        zos.putNextEntry(ze);
        zos.write(entry.getValue().getBytes(UTF_8));
        zos.closeEntry();
      }
    }
  }

  private void writeJarInTimezone(Path inputJar, Path outputJar, String timeZone)
      throws IOException {
    final TimeZone previousTimeZone = TimeZone.getDefault();
    TimeZone.setDefault(TimeZone.getTimeZone(timeZone));
    AddJarManifestEntry.main(
        new String[] {
          "--output", outputJar.toAbsolutePath().toString(),
          "--source", inputJar.toAbsolutePath().toString(),
          "--manifest-entry", "Target-Label:@maven//:com_google_guava_guava"
        });
    TimeZone.setDefault(previousTimeZone);
  }

  private Map<String, String> readJar(Path jar) throws IOException {
    ImmutableMap.Builder<String, String> builder = ImmutableMap.builder();
    try (InputStream is = Files.newInputStream(jar);
        ZipInputStream zis = new ZipInputStream(is)) {
      for (ZipEntry entry = zis.getNextEntry(); entry != null; entry = zis.getNextEntry()) {
        if (entry.isDirectory()) {
          continue;
        }
        builder.put(entry.getName(), new String(ByteStreams.toByteArray(zis), UTF_8));
      }
    }
    return builder.build();
  }

  private List<String> readJarEntries(Path jar) throws IOException {
    ImmutableList.Builder<String> builder = ImmutableList.builder();
    try (InputStream is = Files.newInputStream(jar);
        ZipInputStream zis = new ZipInputStream(is)) {
      for (ZipEntry entry = zis.getNextEntry(); entry != null; entry = zis.getNextEntry()) {
        if (entry.isDirectory()) {
          continue;
        }
        builder.add(entry.getName());
      }
    }
    return builder.build();
  }
}
