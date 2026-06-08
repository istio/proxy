package com.github.bazelbuild.rules_jvm_external.javadoc;

import static com.github.bazelbuild.rules_jvm_external.ZipUtils.createJar;
import static com.github.bazelbuild.rules_jvm_external.ZipUtils.readJar;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.google.common.collect.ImmutableMap;
import java.io.IOException;
import java.nio.file.Path;
import java.util.Map;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class ExclusionTest {

  private Path inputJar;
  private Path outputJar;
  private Path elementList;

  @Rule public TemporaryFolder temp = new TemporaryFolder();

  @Before
  public void setUp() throws IOException {
    this.inputJar = temp.newFile("in.jar").toPath();
    this.outputJar = temp.newFile("out.jar").toPath();
    this.elementList = temp.newFile("element-list").toPath();
    // deleting the file since JavadocJarMaker fails on existing files, we just need to supply the
    // path.
    elementList.toFile().delete();
  }

  @Test
  public void testJavadocPackageExclusion() throws IOException {
    createJar(
        inputJar,
        ImmutableMap.of(
            "com/example/Main.java",
            "package com.example; public class Main { public static void main(String[] args) {} }",
            "com/example/processor/Processor.java",
            "package com.example.processor; public class Processor {}",
            "com/example/processor/internal/InternalProcessor.java",
            "package com.example.processor.internal; public class InternalProcessor {}",
            "com/example/state/internal/InternalThing.java",
            "package com.example.state.internal; public class InternalThing {}",
            "com/example/state/Thing.java",
            "package com.example.state; public class Thing {}",
            "com/example/query/Query.java",
            "package com.example.query; public class Query {}",
            "com/example/query/internal/InternalQuery.java",
            "package com.example.query.internal; public class InternalQuery {}"));

    JavadocJarMaker.main(
        new String[] {
          "--in",
          inputJar.toAbsolutePath().toString(),
          "--out",
          outputJar.toAbsolutePath().toString(),
          "--exclude-packages",
          "com.example.processor.internal",
          "--exclude-packages",
          "com.example.state.internal",
          "--exclude-packages",
          "com.example.query.internal",
          "--element-list",
          elementList.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);

    assertTrue(contents.containsKey("com/example/Main.html"));

    assertTrue(contents.containsKey("com/example/processor/Processor.html"));
    assertTrue(!contents.containsKey("com/example/processor/internal/InternalProcessor.html"));

    assertTrue(contents.containsKey("com/example/state/Thing.html"));
    assertTrue(!contents.containsKey("com/example/state/internal/InternalThing.html"));

    assertTrue(contents.containsKey("com/example/query/Query.html"));
    assertTrue(!contents.containsKey("com/example/query/internal/InternalQuery.html"));
  }

  @Test
  public void testJavadocPackageExclusionWithAsterisk() throws IOException {
    createJar(
        inputJar,
        ImmutableMap.of(
            "com/example/Main.java",
            "package com.example; public class Main { public static void main(String[] args) {} }",
            "com/example/processor/Processor.java",
            "package com.example.processor; public class Processor {}",
            "com/example/processor/internal/InternalProcessor.java",
            "package com.example.processor.internal; public class InternalProcessor {}",
            "com/example/processor/internal/other/OtherProcessor.java",
            "package com.example.processor.internal.other; public class OtherProcessor {}"));

    JavadocJarMaker.main(
        new String[] {
          "--in",
          inputJar.toAbsolutePath().toString(),
          "--out",
          outputJar.toAbsolutePath().toString(),
          "--exclude-packages",
          "com.example.processor.internal.*",
          "--element-list",
          elementList.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);

    // With asterisk, the "other" subpackage should be excluded as well.
    assertTrue(contents.containsKey("com/example/Main.html"));
    assertTrue(contents.containsKey("com/example/processor/Processor.html"));

    assertFalse(contents.containsKey("com/example/processor/internal/InternalProcessor.html"));
    assertFalse(contents.containsKey("com/example/processor/internal/other/OtherProcessor.html"));
  }

  @Test
  public void testJavadocPackageToplevelExcluded() throws IOException {
    createJar(
        inputJar,
        ImmutableMap.of(
            "com/example/Main.java",
            "package com.example; public class Main { public static void main(String[] args) {} }",
            "io/example/processor/Processor.java",
            "package io.example.processor; public class Processor {}"));

    JavadocJarMaker.main(
        new String[] {
          "--in",
          inputJar.toAbsolutePath().toString(),
          "--out",
          outputJar.toAbsolutePath().toString(),
          "--exclude-packages",
          "io.example.*",
          "--element-list",
          elementList.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);

    // Checking that the toplevel package "io" is excluded. If it wasn't, the javadoc command
    // would throw an error for -subpackage containing a package that doesn't exist.
    assertTrue(contents.containsKey("com/example/Main.html"));
    assertFalse(contents.containsKey("io/example/processor/Processor.html"));
  }

  @Test
  public void testIncludeCombinedWithExclude() throws IOException {
    createJar(
        inputJar,
        ImmutableMap.of(
            "com/example/Main.java",
            "package com.example; public class Main { public static void main(String[] args) {} }",
            "io/example/Thing.java",
            "package io.example; public class Thing {}",
            "com/example/internal/InternalThing.java",
            "package com.example.internal; public class InternalThing {}"));

    JavadocJarMaker.main(
        new String[] {
          "--in",
          inputJar.toAbsolutePath().toString(),
          "--out",
          outputJar.toAbsolutePath().toString(),
          "--exclude-packages",
          "com.example.internal",
          "--include-packages",
          "com.example.*",
          "--element-list",
          elementList.toAbsolutePath().toString()
        });

    Map<String, String> contents = readJar(outputJar);

    // The include gets applied before the exclude.
    // io.example is not explicitely excluded, but its not in the include list, so it should not
    // appear.
    assertTrue(contents.containsKey("com/example/Main.html"));
    assertFalse(contents.containsKey("io/example/Thing.html"));
    assertFalse(contents.containsKey("com/example/internal/InternalThing.html"));
  }
}
