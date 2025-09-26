package com.github.bazelbuild.rules_jvm_external.javadoc;

import static com.github.bazelbuild.rules_jvm_external.ZipUtils.createJar;
import static com.github.bazelbuild.rules_jvm_external.ZipUtils.readJar;
import static java.nio.charset.StandardCharsets.UTF_8;
import static org.junit.Assert.assertEquals;

import com.google.common.collect.ImmutableMap;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class ResourceTest {
  @Rule public TemporaryFolder temp = new TemporaryFolder();

  @Test
  public void shouldIncludeResourceFiles() throws Exception {
    Path inputJar = temp.newFile("in.jar").toPath();
    Path outputJar = temp.newFile("out.jar").toPath();
    Path elementList = temp.newFile("element-list").toPath();
    // deleting the file since JavadocJarMaker fails on existing files, we just need to supply the
    // path.
    elementList.toFile().delete();

    Path license = temp.newFile("LICENSE").toPath();
    Files.write(license, List.of("Apache License 2.0"), UTF_8);

    createJar(
        inputJar,
        ImmutableMap.of(
            "com/example/Main.java",
            "public class Main { public static void main(String[] args) {} }"));

    JavadocJarMaker.main(
        new String[] {
          "--resources",
          license.toAbsolutePath().toString(),
          "--in",
          inputJar.toAbsolutePath().toString(),
          "--out",
          outputJar.toAbsolutePath().toString(),
          "--element-list",
          elementList.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);
    assertEquals("Apache License 2.0".strip(), contents.get("LICENSE").strip());
  }
}
