package com.jvm.external.jvm_export;

import static com.google.common.base.StandardSystemProperty.OS_NAME;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeFalse;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.stream.Collectors;
import java.util.zip.ZipFile;
import org.junit.BeforeClass;
import org.junit.Test;

public class ProjectJarTest {

  @BeforeClass
  public static void checkPlatform() {
    assumeFalse(OS_NAME.value().toLowerCase().contains("window"));
  }

  @Test
  public void projectJarShouldContainClassDirectlyIncluded() throws IOException {
    Path projectJar = getProjectJar();
    assertTrue(jarContains(projectJar, "com.jvm.external.jvm_export.Main"));
  }

  @Test
  public void projectJarShouldContainTransitiveDep() throws IOException {
    Path projectJar = getProjectJar();
    assertTrue(jarContains(projectJar, "com.jvm.external.jvm_export.Dependency"));
  }

  @Test
  public void projectJarShouldNotContainClassesFromMavenDependencies() throws IOException {
    Path projectJar = getProjectJar();
    assertFalse(jarContains(projectJar, "com.google.common.collect.ImmutableList"));
  }

  private boolean jarContains(Path jarFile, String className) throws IOException {
    String pathWithinJar = className.replace(".", "/") + ".class";

    try (ZipFile zipFile = new ZipFile(jarFile.toFile())) {
      return zipFile.getEntry(pathWithinJar) != null;
    }
  }

  private Path getProjectJar() {
    Path projectJar = Paths.get(System.getProperty("location"));

    String errorInfo = "Project jar: " + projectJar + "\n" + list(projectJar.getParent());
    assertTrue(errorInfo, Files.exists(projectJar));
    return projectJar;
  }

  private String list(Path path) {
    try {
      return "Contents of "
          + path
          + ": "
          + Files.list(path).map(Object::toString).collect(Collectors.joining(", ", "", "\n"));
    } catch (Exception e) {
      return "Unable to list contents of " + path + "\n";
    }
  }
}
