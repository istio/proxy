package com.github.bazelbuild.rules_jvm_external.maven;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.core.Is.is;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.hamcrest.core.IsNot.not;
import static org.hamcrest.core.IsNull.notNullValue;
import static org.hamcrest.core.IsNull.nullValue;
import static org.junit.Assert.assertThat;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.w3c.dom.Document;

public class OutdatedTest {
  private static final String OS = System.getProperty("os.name").toLowerCase();

  private final PrintStream originalOut = System.out;

  @Rule public TemporaryFolder temp = new TemporaryFolder();

  @Test
  public void shouldFindUpdatedVersionForGuava() throws IOException {
    Path artifactsFile = temp.newFile("outdated.artifacts").toPath();
    Files.write(
        artifactsFile, Arrays.asList("com.google.guava:guava:27.0-jre"), StandardCharsets.UTF_8);

    Path repositoriesFile = temp.newFile("outdated.repositories").toPath();
    Files.write(
        repositoriesFile, Arrays.asList("https://repo1.maven.org/maven2"), StandardCharsets.UTF_8);

    ByteArrayOutputStream outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString()
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(outdatedOutput.toString(), containsString("https://repo1.maven.org/maven2"));
    assertThat(outdatedOutput.toString(), containsString("com.google.guava:guava [27.0-jre -> "));
    assertThat(outdatedOutput.toString(), not(containsString("No updates found")));
    assertThat(outdatedOutput.toString(), not(containsString("BOMs")));
  }

  @Test
  public void shouldPrintNoUpdatesIfInputFileIsEmpty() throws IOException {
    Path artifactsFile = temp.newFile("outdated.artifacts").toPath();
    Files.write(artifactsFile, Arrays.asList(""), StandardCharsets.UTF_8);

    Path repositoriesFile = temp.newFile("outdated.repositories").toPath();
    Files.write(
        repositoriesFile, Arrays.asList("https://repo1.maven.org/maven2"), StandardCharsets.UTF_8);

    ByteArrayOutputStream outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString()
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(outdatedOutput.toString(), containsString("https://repo1.maven.org/maven2"));
    assertThat(outdatedOutput.toString(), containsString("No updates found"));
  }

  @Test
  public void shouldWorkWithMultipleArtifactsAndRepositories() throws IOException {
    Path artifactsFile = temp.newFile("outdated.artifacts").toPath();
    Files.write(
        artifactsFile,
        Arrays.asList(
            "com.google.guava:guava:27.0-jre", "junit:junit:4.12", "com.squareup:javapoet:1.11.1"),
        StandardCharsets.UTF_8);

    Path repositoriesFile = temp.newFile("outdated.repositories").toPath();
    Files.write(
        repositoriesFile,
        Arrays.asList("https://repo1.maven.org/maven2", "https://maven.google.com"),
        StandardCharsets.UTF_8);

    ByteArrayOutputStream outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString()
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 3 artifacts against the following repositories"));
    assertThat(outdatedOutput.toString(), containsString("https://repo1.maven.org/maven2"));
    assertThat(outdatedOutput.toString(), containsString("https://maven.google.com"));
    assertThat(outdatedOutput.toString(), containsString("com.google.guava:guava [27.0-jre -> "));
    assertThat(outdatedOutput.toString(), containsString("junit:junit [4.12 -> "));
    assertThat(outdatedOutput.toString(), containsString("com.squareup:javapoet [1.11.1 -> "));
  }

  @Test
  public void artifactHasUpdateAndPreReleaseUpdate() throws IOException {
    if (OS.indexOf("win") != -1) {
      // On Windows we get a java.io.FileNotFoundException for the maven-metadata.xml specified as
      // 'data' to the test
      return;
    }
    Path artifactsFile = temp.newFile("outdated.artifacts").toPath();
    Files.write(
        artifactsFile,
        Arrays.asList("com.example.release-and-prerelease:test-artifact:11.0.0"),
        StandardCharsets.UTF_8);

    Path repositoriesFile = temp.newFile("outdated.repositories").toPath();
    Files.write(
        repositoriesFile,
        Arrays.asList(
            new File("tests/com/github/bazelbuild/rules_jvm_external/maven/resources")
                .toURI()
                .toURL()
                .toString()),
        StandardCharsets.UTF_8);

    ByteArrayOutputStream outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString()
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(
        outdatedOutput.toString(),
        containsString(
            "com.example.release-and-prerelease:test-artifact [11.0.0 -> 11.0.12] (pre-release:"
                + " 12.0.1.alpha4)"));
    assertThat(outdatedOutput.toString(), not(containsString("No updates found")));

    // Test with legacy output
    outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString(),
            "--legacy-output"
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(
        outdatedOutput.toString(),
        containsString(
            "com.example.release-and-prerelease:test-artifact [11.0.0 -> 12.0.1.alpha4]"));
    assertThat(outdatedOutput.toString(), not(containsString("pre-release")));
    assertThat(outdatedOutput.toString(), not(containsString("No updates found")));
  }

  @Test
  public void artifactHasOnlyPreReleaseUpdate() throws IOException {
    if (OS.indexOf("win") != -1) {
      // On Windows we get a java.io.FileNotFoundException for the maven-metadata.xml specified as
      // 'data' to the test
      return;
    }
    Path artifactsFile = temp.newFile("outdated.artifacts").toPath();
    Files.write(
        artifactsFile,
        Arrays.asList("com.example.prerelease:test-artifact:11.0.0"),
        StandardCharsets.UTF_8);

    Path repositoriesFile = temp.newFile("outdated.repositories").toPath();
    Files.write(
        repositoriesFile,
        Arrays.asList(
            new File("tests/com/github/bazelbuild/rules_jvm_external/maven/resources")
                .toURI()
                .toURL()
                .toString()),
        StandardCharsets.UTF_8);

    ByteArrayOutputStream outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString()
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(
        outdatedOutput.toString(),
        containsString(
            "com.example.prerelease:test-artifact [11.0.0] (pre-release:" + " 12.0.1.alpha4)"));
    assertThat(outdatedOutput.toString(), not(containsString("No updates found")));

    // Test with legacy output
    outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString(),
            "--legacy-output"
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(
        outdatedOutput.toString(),
        containsString("com.example.prerelease:test-artifact [11.0.0 -> 12.0.1.alpha4]"));
    assertThat(outdatedOutput.toString(), not(containsString("pre-release")));
    assertThat(outdatedOutput.toString(), not(containsString("No updates found")));
  }

  @Test
  public void artifactOnlyHasNonPreReleaseUpdate() throws IOException {
    if (OS.indexOf("win") != -1) {
      // On Windows we get a java.io.FileNotFoundException for the maven-metadata.xml specified as
      // 'data' to the test
      return;
    }
    Path artifactsFile = temp.newFile("outdated.artifacts").toPath();
    Files.write(
        artifactsFile,
        Arrays.asList("com.example.release:test-artifact:11.0.0"),
        StandardCharsets.UTF_8);

    Path repositoriesFile = temp.newFile("outdated.repositories").toPath();
    Files.write(
        repositoriesFile,
        Arrays.asList(
            new File("tests/com/github/bazelbuild/rules_jvm_external/maven/resources")
                .toURI()
                .toURL()
                .toString()),
        StandardCharsets.UTF_8);

    ByteArrayOutputStream outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString()
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(
        outdatedOutput.toString(),
        containsString("com.example.release:test-artifact [11.0.0 -> 11.0.12]"));
    assertThat(outdatedOutput.toString(), not(containsString("pre-release")));
    assertThat(outdatedOutput.toString(), not(containsString("No updates found")));

    // Test with legacy output
    outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString(),
            "--legacy-output"
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(
        outdatedOutput.toString(),
        containsString("com.example.release:test-artifact [11.0.0 -> 11.0.12]"));
    assertThat(outdatedOutput.toString(), not(containsString("pre-release")));
    assertThat(outdatedOutput.toString(), not(containsString("No updates found")));
  }

  @Test
  public void artifactHasNoUpdateAndNoPreReleaseUpdate() throws IOException {
    Path artifactsFile = temp.newFile("outdated.artifacts").toPath();
    Files.write(
        artifactsFile, Arrays.asList("com.example:test-artifact:11.0.0"), StandardCharsets.UTF_8);

    Path repositoriesFile = temp.newFile("outdated.repositories").toPath();
    Files.write(
        repositoriesFile,
        Arrays.asList(
            new File("com/github/bazelbuild/rules_jvm_external/maven/resources")
                .toURI()
                .toURL()
                .toString()),
        StandardCharsets.UTF_8);

    ByteArrayOutputStream outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString()
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(outdatedOutput.toString(), not(containsString("test-artifact")));
    assertThat(outdatedOutput.toString(), not(containsString("pre-release")));
    assertThat(outdatedOutput.toString(), containsString("No updates found"));

    // Test with legacy output
    outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString(),
            "--legacy-output"
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(outdatedOutput.toString(), not(containsString("test-artifact")));
    assertThat(outdatedOutput.toString(), not(containsString("pre-release")));
    assertThat(outdatedOutput.toString(), containsString("No updates found"));
  }

  private static Document getTestDocument(String name) {
    String resourceName = "tests/com/github/bazelbuild/rules_jvm_external/maven/resources/" + name;
    try (InputStream in =
        Thread.currentThread().getContextClassLoader().getResourceAsStream(resourceName)) {
      DocumentBuilderFactory documentBuilderFactory = DocumentBuilderFactory.newInstance();
      DocumentBuilder documentBuilder = documentBuilderFactory.newDocumentBuilder();
      return documentBuilder.parse(in);
    } catch (Exception e) {
      throw new AssertionError(e);
    }
  }

  // https://github.com/bazelbuild/rules_jvm_external/issues/507
  @Test
  public void shouldWorkForAnArtifactMissingReleaseMetadata() throws IOException {
    Document testDocument = getTestDocument("maven-metadata-javax-inject.xml");
    Outdated.ArtifactReleaseInfo releaseInfo = Outdated.getReleaseVersion(testDocument, "testDoc");
    assertThat(releaseInfo, is(notNullValue()));
    assertThat(releaseInfo.releaseVersion, equalTo("1"));
    assertThat(releaseInfo.preReleaseVersion, is(nullValue()));
  }

  // https://github.com/bazelbuild/rules_jvm_external/issues/507
  @Test
  public void grabsLastVersionWhenArtifactMissingReleaseMetadata() throws IOException {
    Document testDocument = getTestDocument("maven-metadata-multiple-versions.xml");
    Outdated.ArtifactReleaseInfo releaseInfo = Outdated.getReleaseVersion(testDocument, "testDoc");
    assertThat(releaseInfo, is(notNullValue()));
    assertThat(releaseInfo.releaseVersion, equalTo("2"));
    assertThat(releaseInfo.preReleaseVersion, is(nullValue()));
  }

  @Test
  public void grabsLastVersionWorksWhenReleaseIsPreRelease() throws IOException {
    Document testDocument = getTestDocument("maven-metadata-unsorted-versions.xml");
    Outdated.ArtifactReleaseInfo releaseInfo = Outdated.getReleaseVersion(testDocument, "testDoc");
    assertThat(releaseInfo, is(notNullValue()));
    assertThat(releaseInfo.releaseVersion, equalTo("11.0.12"));
    assertThat(releaseInfo.preReleaseVersion, equalTo("12.0.1.alpha4"));
  }

  @Test
  public void worksWithCommonsVersions() throws IOException {
    Document testDocument = getTestDocument("maven-metadata-commons-collections.xml");
    Outdated.ArtifactReleaseInfo releaseInfo = Outdated.getReleaseVersion(testDocument, "testDoc");
    assertThat(releaseInfo, is(notNullValue()));
    assertThat(releaseInfo.releaseVersion, equalTo("20040616"));
    assertThat(releaseInfo.preReleaseVersion, is(nullValue()));
  }

  @Test
  public void shouldFindUpdatedVersionForBOMs() throws IOException {
    Path artifactsFile = temp.newFile("outdated.artifacts").toPath();
    Files.write(
        artifactsFile, Arrays.asList("com.google.guava:guava:27.0-jre"), StandardCharsets.UTF_8);

    Path bomsFile = temp.newFile("outdated.boms").toPath();
    Files.write(
        bomsFile, Arrays.asList("com.google.cloud:libraries-bom:26.0.0"), StandardCharsets.UTF_8);

    Path repositoriesFile = temp.newFile("outdated.repositories").toPath();
    Files.write(
        repositoriesFile, Arrays.asList("https://repo1.maven.org/maven2"), StandardCharsets.UTF_8);

    ByteArrayOutputStream outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--boms-file", bomsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString()
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString(
            "Checking for updates of 1 boms and 1 artifacts against the following repositories"));
    assertThat(outdatedOutput.toString(), containsString("https://repo1.maven.org/maven2"));
    assertThat(outdatedOutput.toString(), containsString("BOMs"));
    assertThat(
        outdatedOutput.toString(), containsString("com.google.cloud:libraries-bom [26.0.0 -> "));
    assertThat(outdatedOutput.toString(), containsString("com.google.guava:guava [27.0-jre -> "));
    assertThat(outdatedOutput.toString(), not(containsString("No updates found")));
  }

  @Test
  public void shouldPrintNoUpdatesIfBOMInputFileIsEmpty() throws IOException {
    Path artifactsFile = temp.newFile("outdated.artifacts").toPath();
    Files.write(
        artifactsFile, Arrays.asList("com.google.guava:guava:27.0-jre"), StandardCharsets.UTF_8);

    Path bomsFile = temp.newFile("outdated.boms").toPath();
    Files.write(bomsFile, Arrays.asList(""), StandardCharsets.UTF_8);

    Path repositoriesFile = temp.newFile("outdated.repositories").toPath();
    Files.write(
        repositoriesFile, Arrays.asList("https://repo1.maven.org/maven2"), StandardCharsets.UTF_8);

    ByteArrayOutputStream outdatedOutput = new ByteArrayOutputStream();
    try {
      System.setOut(new PrintStream(outdatedOutput));
      Outdated.main(
          new String[] {
            "--artifacts-file", artifactsFile.toAbsolutePath().toString(),
            "--boms-file", bomsFile.toAbsolutePath().toString(),
            "--repositories-file", repositoriesFile.toAbsolutePath().toString()
          });
    } finally {
      System.setOut(originalOut);
    }

    assertThat(
        outdatedOutput.toString(),
        containsString("Checking for updates of 1 artifacts against the following repositories"));
    assertThat(outdatedOutput.toString(), containsString("https://repo1.maven.org/maven2"));
    assertThat(outdatedOutput.toString(), not(containsString("BOMs")));
    assertThat(outdatedOutput.toString(), containsString("com.google.guava:guava [27.0-jre -> "));
    assertThat(outdatedOutput.toString(), not(containsString("No updates found")));
  }
}
