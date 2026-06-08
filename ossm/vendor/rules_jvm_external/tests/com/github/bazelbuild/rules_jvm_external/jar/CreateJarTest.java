package com.github.bazelbuild.rules_jvm_external.jar;

import static com.github.bazelbuild.rules_jvm_external.ZipUtils.readJar;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class CreateJarTest {

  @Rule public TemporaryFolder temp = new TemporaryFolder();

  @Test
  public void testCreateJar() throws Exception {
    // Create temporary directory for test files
    File tempDirFile = temp.newFolder("jar-test-input");
    Path tempDir = tempDirFile.toPath();

    // Create test files with known content
    Path file1 = tempDir.resolve("file1.txt");
    Files.writeString(file1, "content1");
    Path subDir = tempDir.resolve("subdir");
    Files.createDirectory(subDir);
    Path file2 = subDir.resolve("file2.txt");
    Files.writeString(file2, "content2");

    // Create output jar path
    Path outputJar = temp.newFile("output.jar").toPath();
    Files.delete(outputJar); // Delete so createJar can create it

    CreateJar.main(
        new String[] {outputJar.toAbsolutePath().toString(), tempDir.toAbsolutePath().toString()});

    // Verify jar was created
    assertTrue(Files.exists(outputJar));

    // Verify jar contents
    Map<String, String> jarContents = readJar(outputJar);
    assertEquals("content1", jarContents.get("file1.txt"));
    assertEquals("content2", jarContents.get("subdir/file2.txt"));
  }
}
