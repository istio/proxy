package com.github.bazelbuild.rules_jvm_external.jar;

import static java.nio.file.StandardCopyOption.REPLACE_EXISTING;
import static java.util.jar.Attributes.Name.MANIFEST_VERSION;

import com.github.bazelbuild.rules_jvm_external.ByteStreams;
import com.github.bazelbuild.rules_jvm_external.zip.StableZipEntry;
import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;
import java.util.Objects;
import java.util.StringTokenizer;
import java.util.jar.Attributes;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.jar.JarInputStream;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipOutputStream;

/*
* A class that will add an entry to the manifest and keep the same modification
# times of the jar entries.
*/
public class AddJarManifestEntry {

  // Visible for testing
  public static final Attributes.Name AUTOMATIC_MODULE_NAME =
      new Attributes.Name("Automatic-Module-Name");
  // Collected from https://docs.oracle.com/javase/specs/jls/se11/html/jls-3.html#jls-Keyword
  private static final Collection<String> ILLEGAL_PACKAGE_NAMES =
      Collections.unmodifiableCollection(
          Arrays.asList(
              "abstract",
              "assert",
              "boolean",
              "break",
              "byte",
              "case",
              "catch",
              "char",
              "class",
              "const",
              "continue",
              "default",
              "do",
              "double",
              "else",
              "enum",
              "extends",
              "final",
              "finally",
              "float",
              "for",
              "goto",
              "if",
              "implements",
              "import",
              "instanceof",
              "int",
              "interface",
              "long",
              "native",
              "new",
              "package",
              "private",
              "protected",
              "public",
              "return",
              "short",
              "static",
              "strictfp",
              "super",
              "switch",
              "synchronized",
              "this",
              "throw",
              "throws",
              "transient",
              "try",
              "void",
              "volatile",
              "while",
              "_",

              // Additionally, these can't be used in identifiers:
              // https://docs.oracle.com/javase/specs/jls/se11/html/jls-3.html#jls-Identifier
              "false",
              "true",
              "null",
              "var"));

  public static void verboseLog(String logline) {
    // To make this work you need to add 'use_default_shell_env = True' to the
    // rule and specify '--action_env=RJE_VERBOSE=true' to the bazel build command.
    if (System.getenv("RJE_VERBOSE") != null) {
      System.out.println(logline);
    }
  }

  public static void main(String[] args) throws IOException {
    Path out = null;
    Path source = null;
    boolean makeSafe = false;
    List<String> toAdd = new ArrayList<>();
    List<String> toRemove = new ArrayList<>();

    for (int i = 0; i < args.length; i++) {
      switch (args[i]) {
        case "--make-safe":
          makeSafe = true;
          break;

        case "--manifest-entry":
          toAdd.add(args[++i]);
          break;

        case "--output":
          out = Paths.get(args[++i]);
          break;

        case "--remove-entry":
          toRemove.add(args[++i]);
          break;

        case "--source":
          source = Paths.get(args[++i]);
          break;

        default:
          throw new IllegalArgumentException(
              "Unable to parse command line: " + Arrays.toString(args));
      }
    }

    Objects.requireNonNull(source, "Source jar must be set.");
    Objects.requireNonNull(out, "Output path must be set.");

    if (isJarSigned(source)) {
      verboseLog("Signed jar. Will not modify: " + source);
      Files.createDirectories(out.getParent());
      Files.copy(source, out, REPLACE_EXISTING);
      return;
    }

    new AddJarManifestEntry().addEntryToManifest(out, source, toAdd, toRemove, makeSafe);
  }

  private static boolean isJarSigned(Path source) throws IOException {
    try (InputStream is = Files.newInputStream(source);
        JarInputStream jis = new JarInputStream(is)) {
      for (ZipEntry entry = jis.getNextEntry(); entry != null; entry = jis.getNextJarEntry()) {
        if (entry.isDirectory()) {
          continue;
        }
        if (entry.getName().startsWith("META-INF/") && entry.getName().endsWith(".SF")) {
          return true;
        }
      }
    }
    return false;
  }

  public void addEntryToManifest(
      Path out, Path source, List<String> toAdd, List<String> toRemove, boolean makeSafe)
      throws IOException {
    try (JarFile jarFile = new JarFile(source.toFile(), false)) {
      try (OutputStream fos = Files.newOutputStream(out);
          ZipOutputStream zos = new JarOutputStream(new BufferedOutputStream(fos))) {

        // Rewrite the manifest first
        Manifest manifest = jarFile.getManifest();
        if (manifest == null) {
          manifest = new Manifest();
          manifest.getMainAttributes().put(MANIFEST_VERSION, "1.0");
        }
        amendManifest(source, manifest, toAdd, toRemove, makeSafe);

        ZipEntry newManifestEntry = new StableZipEntry(JarFile.MANIFEST_NAME);
        zos.putNextEntry(newManifestEntry);
        manifest.write(zos);

        Enumeration<JarEntry> entries = jarFile.entries();
        while (entries.hasMoreElements()) {
          JarEntry sourceEntry = entries.nextElement();
          String name = sourceEntry.getName();

          if (JarFile.MANIFEST_NAME.equals(name)) {
            continue;
          }

          ZipEntry outEntry = new ZipEntry(sourceEntry);

          // Force compressed size recalculation as it might differ
          outEntry.setCompressedSize(-1);

          try (InputStream in = jarFile.getInputStream(sourceEntry)) {
            zos.putNextEntry(outEntry);
            ByteStreams.copy(in, zos);
          } catch (ZipException e) {
            if (e.getMessage().contains("duplicate entry:")) {
              // If there is a duplicate entry we keep the first one we saw.
              verboseLog(
                  "WARN: Skipping duplicate jar entry " + outEntry.getName() + " in " + source);
              continue;
            } else {
              throw e;
            }
          }
        }
      }
    }
  }

  private void amendManifest(
      Path jar, Manifest manifest, List<String> toAdd, List<String> toRemove, boolean makeSafe) {
    manifest.getMainAttributes().put(new Attributes.Name("Created-By"), "AddJarManifestEntry");
    toAdd.forEach(
        manifestEntry -> {
          String[] manifestEntryParts = manifestEntry.split(":", 2);
          manifest
              .getMainAttributes()
              .put(new Attributes.Name(manifestEntryParts[0]), manifestEntryParts[1]);
        });
    toRemove.forEach(name -> manifest.getMainAttributes().remove(new Attributes.Name(name)));

    if (makeSafe) {
      checkAutomaticModuleName(jar, manifest);
    }
  }

  private Manifest checkAutomaticModuleName(Path jar, Manifest manifest) {
    if (!manifest.getMainAttributes().containsKey(AUTOMATIC_MODULE_NAME)) {
      return manifest;
    }

    // The automatic module name must be a valid java package name. What is a valid java package
    // name?
    // https://docs.oracle.com/javase/specs/jls/se11/html/jls-7.html#jls-7.4 has the answer
    String name = manifest.getMainAttributes().getValue(AUTOMATIC_MODULE_NAME);
    if (name == null || name.isEmpty()) {
      return removeEntryAndPrintWarning(
          manifest,
          AUTOMATIC_MODULE_NAME,
          "An empty automatic module was detected. This is not allowed by the java module system: "
              + jar.getFileName());
    }

    StringTokenizer tokenizer = new StringTokenizer(name, ".");
    while (tokenizer.hasMoreTokens()) {
      String part = tokenizer.nextToken().trim();
      if (part.isEmpty()) {
        return removeEntryAndPrintWarning(
            manifest,
            AUTOMATIC_MODULE_NAME,
            String.format(
                "Automatic module name '%s' contains an empty part: %s", name, jar.getFileName()));
      }

      if (!Character.isJavaIdentifierStart(part.charAt(0))) {
        return removeEntryAndPrintWarning(
            manifest,
            AUTOMATIC_MODULE_NAME,
            String.format(
                "Automatic module name '%s' does not start with a character a java package name can"
                    + " start with ('%s'): %s",
                name, part, jar.getFileName()));
      }

      for (int i = 1; i < part.length(); i++) {
        if (ILLEGAL_PACKAGE_NAMES.contains(part)) {
          return removeEntryAndPrintWarning(
              manifest,
              AUTOMATIC_MODULE_NAME,
              String.format(
                  "Automatic module name '%s' contains reserved java keyword ('%s'): %s",
                  name, part, jar.getFileName()));
        }

        if (!Character.isJavaIdentifierPart(part.charAt(i))) {
          return removeEntryAndPrintWarning(
              manifest,
              AUTOMATIC_MODULE_NAME,
              String.format(
                  "Automatic module name '%s' contains a character ('%s') that may not be used in a"
                      + " java identifier: %s",
                  name, part.charAt(i), jar.getFileName()));
        }
      }
    }

    return manifest;
  }

  private Manifest removeEntryAndPrintWarning(
      Manifest manifest, Attributes.Name key, String warning) {
    manifest.getMainAttributes().remove(key);
    // We want this warning to be printed to the screen
    System.err.println(warning);
    return manifest;
  }
}
