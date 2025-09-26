package com.github.bazelbuild.rules_jvm_external.duplicates_in_java_export;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashSet;
import java.util.Set;
import java.util.jar.JarInputStream;
import java.util.zip.ZipEntry;
import org.junit.Test;

public class AllowedDuplicatesTest {

  @Test
  public void shouldNormallyDisallowDuplicates() {
    Path childWithoutParent = Paths.get(System.getProperty("child.without.parent"));
    assertTrue(Files.exists(childWithoutParent));

    // Find all the `.proto` files in the jar
    Set<String> fileNames = getProtoFileNames(childWithoutParent);
    assertEquals(Set.of("child.proto"), fileNames);
  }

  @Test
  public void shouldAllowNamedDuplicates() {
    Path childWithParent = Paths.get(System.getProperty("child.with.parent"));
    assertTrue(Files.exists(childWithParent));

    // Find all the `.proto` files in the jar
    Set<String> fileNames = getProtoFileNames(childWithParent);
    assertEquals(Set.of("child.proto", "parent.proto"), fileNames);
  }

  Set<String> getProtoFileNames(Path jar) {
    Set<String> fileNames = new HashSet<>();
    try (InputStream is = Files.newInputStream(jar);
        InputStream bis = new BufferedInputStream(is);
        JarInputStream jis = new JarInputStream(is)) {

      for (ZipEntry entry = jis.getNextEntry(); entry != null; entry = jis.getNextEntry()) {
        if (entry.getName().endsWith(".proto")) {
          fileNames.add(entry.getName());
        }
      }

      return Set.copyOf(fileNames);
    } catch (IOException e) {
      fail(e.getMessage());
      throw new RuntimeException(e);
    }
  }
}
