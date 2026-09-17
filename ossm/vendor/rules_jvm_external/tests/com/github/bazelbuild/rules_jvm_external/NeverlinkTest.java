package com.github.bazelbuild.rules_jvm_external;

import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.StringContains.containsString;
import static org.junit.Assert.*;

import com.google.common.reflect.ClassPath;
import com.google.common.reflect.ClassPath.ClassInfo;
import java.io.IOException;
import org.junit.Test;

public class NeverlinkTest {

  @Test
  public void test_neverlinkArtifacts_notOnRuntimeClassPath() throws IOException {
    ClassPath classPath = ClassPath.from(ClassLoader.getSystemClassLoader());
    for (ClassInfo ci : classPath.getTopLevelClasses()) {
      assertThat(ci.getName(), not(containsString("com.squareup.javapoet")));
    }
  }
}
