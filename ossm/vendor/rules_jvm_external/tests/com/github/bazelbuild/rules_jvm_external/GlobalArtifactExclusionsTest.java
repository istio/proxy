package com.github.bazelbuild.rules_jvm_external;

import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.StringContains.containsString;
import static org.junit.Assert.*;

import com.google.common.reflect.ClassPath;
import com.google.common.reflect.ClassPath.ClassInfo;
import java.io.IOException;
import org.junit.Test;

public class GlobalArtifactExclusionsTest {

  @Test
  public void test_globallyExcludedArtifacts_notOnClassPath() throws IOException {
    ClassPath classPath = ClassPath.from(ClassLoader.getSystemClassLoader());
    for (ClassInfo ci : classPath.getTopLevelClasses()) {
      assertThat(ci.getName(), not(containsString("org.codehaus.mojo.animal_sniffer")));
      assertThat(ci.getName(), not(containsString("com.google.j2objc.annotations")));
    }
  }
}
