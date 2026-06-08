// Copyright 2024 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.github.bazelbuild.rules_jvm_external.resolver.gradle;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.MavenRepo;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolutionResult;
import com.github.bazelbuild.rules_jvm_external.resolver.Resolver;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolverTestBase;
import com.github.bazelbuild.rules_jvm_external.resolver.cmd.ResolverConfig;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import com.google.common.graph.Graph;
import com.google.devtools.build.runfiles.AutoBazelRepository;
import com.google.devtools.build.runfiles.Runfiles;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;
import javax.xml.stream.XMLStreamException;
import org.junit.Test;

@AutoBazelRepository
public class GradleResolverTest extends ResolverTestBase {

  @Override
  protected Resolver getResolver(Netrc netrc, EventListener listener) {
    return new GradleResolver(netrc, ResolverConfig.DEFAULT_MAX_THREADS, listener);
  }

  @Test
  public void resolvesSimpleJvmVariant() throws IOException, XMLStreamException {
    // This test validates gradle can resolve a artifact using only gradle module metadata
    // In this case, there's a root artifact com.example.sample which points to
    // com.example.sample-jvm, which satisfies the runtimeClasspath configuration
    // as it has the JVM variant attributes by default.
    Coordinates baseCoordinates = new Coordinates("com.example:sample:1.0");
    Coordinates jvmCoordinates = new Coordinates("com.example:sample-jvm:1.0");
    MavenRepo mavenRepo = MavenRepo.create();
    GradleModuleMetadataHelper moduleMetadataHelper = new GradleModuleMetadataHelper(mavenRepo);

    Runfiles runfiles =
        Runfiles.preload().withSourceRepository(AutoBazelRepository_GradleResolverTest.NAME);
    Path baseMetadataPath =
        Paths.get(
            runfiles.rlocation(
                "rules_jvm_external/tests/com/github/bazelbuild/rules_jvm_external/resolver/gradle/fixtures/simpleJvmVariant/sample-1.0.module"));
    String baseMetadata = Files.readString(baseMetadataPath);
    moduleMetadataHelper.addToMavenRepo(baseCoordinates, baseMetadata);

    Path jvmMetadataPath =
        Paths.get(
            runfiles.rlocation(
                "rules_jvm_external/tests/com/github/bazelbuild/rules_jvm_external/resolver/gradle/fixtures/simpleJvmVariant/sample-jvm-1.0.module"));
    String jvmMetadata = Files.readString(jvmMetadataPath);
    moduleMetadataHelper.addToMavenRepo(jvmCoordinates, jvmMetadata);

    Graph<Coordinates> resolved =
        resolver
            .resolve(prepareRequestFor(mavenRepo.getPath().toUri(), baseCoordinates))
            .getResolution();

    assertEquals(2, resolved.nodes().size());
    // sample-jvm resolves indirectly through sample using the gradle module metadata redirect
    assertEquals(Set.of(baseCoordinates, jvmCoordinates), resolved.nodes());
  }

  @Test
  public void resolvesJvmButNotAndroidVariant() throws IOException, XMLStreamException {
    // This test validates a scenario similar to
    // https://repo1.maven.org/maven2/com/squareup/okhttp3/okhttp/5.1.0/okhttp-5.1.0.module
    // which supports 2 different coordinates with one base coordinate - one for JVM and android
    // using variant selection.
    // Right now, we only resolve the default runtime classpath configuration, so we'll only resolve
    // the JVM variant
    // and won't have the android variant
    Coordinates baseCoordinates = new Coordinates("com.example:sample:1.0");
    Coordinates jvmCoordinates = new Coordinates("com.example:sample-jvm:1.0");
    Coordinates androidCoordinates = new Coordinates("com.example:sample-android:1.0");
    MavenRepo mavenRepo = MavenRepo.create();
    GradleModuleMetadataHelper moduleMetadataHelper = new GradleModuleMetadataHelper(mavenRepo);

    Runfiles runfiles =
        Runfiles.preload().withSourceRepository(AutoBazelRepository_GradleResolverTest.NAME);
    Path baseMetadataPath =
        Paths.get(
            runfiles.rlocation(
                "rules_jvm_external/tests/com/github/bazelbuild/rules_jvm_external/resolver/gradle/fixtures/jvmAndAndroidVariants/sample-1.0.module"));
    String baseMetadata = Files.readString(baseMetadataPath);
    moduleMetadataHelper.addToMavenRepo(baseCoordinates, baseMetadata);

    Path jvmMetadataPath =
        Paths.get(
            runfiles.rlocation(
                "rules_jvm_external/tests/com/github/bazelbuild/rules_jvm_external/resolver/gradle/fixtures/jvmAndAndroidVariants/sample-jvm-1.0.module"));
    String jvmMetadata = Files.readString(jvmMetadataPath);
    moduleMetadataHelper.addToMavenRepo(jvmCoordinates, jvmMetadata);

    Path androidMetadataPath =
        Paths.get(
            runfiles.rlocation(
                "rules_jvm_external/tests/com/github/bazelbuild/rules_jvm_external/resolver/gradle/fixtures/jvmAndAndroidVariants/sample-android-1.0.module"));
    String androidMetadata = Files.readString(androidMetadataPath);
    moduleMetadataHelper.addToMavenRepo(androidCoordinates, androidMetadata);

    Graph<Coordinates> resolved =
        resolver
            .resolve(prepareRequestFor(mavenRepo.getPath().toUri(), baseCoordinates))
            .getResolution();

    // sample-jvm resolves indirectly through sample using the gradle module metadata redirect
    // but not sample-android as we don't resolve multiple variants currently.
    assertEquals(2, resolved.nodes().size());
    // Once we support resolving android variant, this test should be updated to ensure
    // sample-android is also resolved
    assertEquals(Set.of(baseCoordinates, jvmCoordinates), resolved.nodes());
  }

  @Test
  public void throwsAnExceptionIfASingleDependencyWasNotResolved() throws IOException {
    Coordinates validCoordinates = new Coordinates("com.example:sample:1.0");
    Coordinates invalidCoordinates = new Coordinates("com.example:does-not-exist:1.0");
    MavenRepo mavenRepo = MavenRepo.create().add(validCoordinates);
    // Junit 4 doesn't have a clean way to assert exceptions
    // so we handle the exception explicitly and assert on it
    try {
      resolver
          .resolve(
              prepareRequestFor(mavenRepo.getPath().toUri(), validCoordinates, invalidCoordinates))
          .getResolution();
      fail("Resolution shouldn't succeed if invalid coordinates are specified");

    } catch (Exception e) {
      assertTrue(e.getCause() instanceof GradleDependencyResolutionException);
      assertEquals(
          "Failed to resolve dependency: com.example:does-not-exist:1.0 (NOT_FOUND)",
          e.getCause().getMessage());
    }
  }

  @Test
  public void throwsAnExceptionIfMultipleDependenciesWereNotResolved() throws IOException {
    Coordinates validCoordinates = new Coordinates("com.example:sample:1.0");
    Coordinates invalidCoordinates1 = new Coordinates("com.example:does-not-exist:1.0");
    Coordinates invalidCoordinates2 = new Coordinates("com.example:does-not-exist-too:1.0");
    MavenRepo mavenRepo = MavenRepo.create().add(validCoordinates);
    // Junit 4 doesn't have a clean way to assert exceptions
    // so we handle the exception explicitly and assert on it
    try {
      resolver
          .resolve(
              prepareRequestFor(
                  mavenRepo.getPath().toUri(),
                  validCoordinates,
                  invalidCoordinates1,
                  invalidCoordinates2))
          .getResolution();
      fail("Resolution shouldn't succeed if invalid coordinates are specified");

    } catch (Exception e) {
      assertTrue(e.getCause() instanceof GradleDependencyResolutionException);
      assertEquals(
          "Multiple dependencies failed to resolve:\n"
              + "  - com.example:does-not-exist:1.0 (NOT_FOUND)\n"
              + "  - com.example:does-not-exist-too:1.0 (NOT_FOUND)",
          e.getCause().getMessage());
    }
  }

  @Test
  public void resolvesAggregatingDependencyWithOnlyClassifiedArtifacts()
      throws IOException, XMLStreamException {
    // This test validates that dependencies with only platform-specific classified artifacts
    // (e.g., native libraries like netty-transport-native-kqueue with -osx-aarch_64.jar)
    // are correctly marked as aggregating and resolution completes successfully
    // without trying to download a non-existent base JAR.
    Coordinates rootCoordinates = new Coordinates("com.example:app:1.0");
    Coordinates nativeLibCoordinates = new Coordinates("com.example:native-lib:1.0");
    MavenRepo mavenRepo = MavenRepo.create();
    GradleModuleMetadataHelper moduleMetadataHelper = new GradleModuleMetadataHelper(mavenRepo);

    Runfiles runfiles =
        Runfiles.preload().withSourceRepository(AutoBazelRepository_GradleResolverTest.NAME);

    // Add root artifact with dependency on native-lib
    Path rootMetadataPath =
        Paths.get(
            runfiles.rlocation(
                "rules_jvm_external/tests/com/github/bazelbuild/rules_jvm_external/resolver/gradle/fixtures/aggregatingDependency/app-1.0.module"));
    String rootMetadata = Files.readString(rootMetadataPath);
    moduleMetadataHelper.addToMavenRepo(rootCoordinates, rootMetadata);

    // Add native-lib with only classified artifacts (no base JAR)
    // Use POM extension to avoid creating a base JAR file
    Path nativeLibMetadataPath =
        Paths.get(
            runfiles.rlocation(
                "rules_jvm_external/tests/com/github/bazelbuild/rules_jvm_external/resolver/gradle/fixtures/aggregatingDependency/native-lib-1.0.module"));
    String nativeLibMetadata = Files.readString(nativeLibMetadataPath);
    Coordinates nativeLibPomCoordinates = nativeLibCoordinates.setExtension("pom");
    moduleMetadataHelper.addToMavenRepo(nativeLibPomCoordinates, nativeLibMetadata);

    // Add classified artifacts for native-lib
    Coordinates osxAarch64 = new Coordinates("com.example:native-lib:jar:osx-aarch_64:1.0");
    Coordinates osxX8664 = new Coordinates("com.example:native-lib:jar:osx-x86_64:1.0");
    mavenRepo.add(osxAarch64);
    mavenRepo.add(osxX8664);

    // This should complete successfully without trying to download the base JAR
    // (com.example:native-lib:1.0 without classifier)
    var result = resolver.resolve(prepareRequestFor(mavenRepo.getPath().toUri(), rootCoordinates));
    Graph<Coordinates> resolved = result.getResolution();

    // Verify the graph contains the root coordinate
    assertTrue(resolved.nodes().contains(rootCoordinates));

    // The base coordinate for native-lib should NOT be in the graph
    // because it only has a POM (it's an aggregating dependency with no base artifact)
    assertEquals(
        "Expected aggregating native-lib coordinate to be removed from graph",
        false,
        resolved.nodes().contains(nativeLibCoordinates));

    // The classified artifacts should be in the graph
    assertTrue(
        "Expected osx-aarch_64 classified artifact in graph",
        resolved.nodes().contains(osxAarch64));
    assertTrue(
        "Expected osx-x86_64 classified artifact in graph", resolved.nodes().contains(osxX8664));

    // The graph should contain: app + 2 classified native-lib artifacts
    assertEquals("Expected 3 coordinates in graph", 3, resolved.nodes().size());
  }

  @Test
  public void shouldRecordCorrectShaForResolvedVersionNotConflictingVersion() {
    // When there's a version conflict, the paths map should contain only the resolved version,
    // not the conflicting lower version. This ensures we record the correct SHA for the artifact.
    Coordinates lowerVersion = new Coordinates("com.example:conflicted:2.8");
    Coordinates higherVersion = new Coordinates("com.example:conflicted:3.0.0");
    Coordinates dependsOnLower = new Coordinates("com.example:uses-lower:1.0");
    Coordinates dependsOnHigher = new Coordinates("com.example:uses-higher:1.0");

    Path repo =
        MavenRepo.create()
            .add(lowerVersion)
            .add(higherVersion)
            .add(dependsOnLower, lowerVersion)
            .add(dependsOnHigher, higherVersion)
            .getPath();

    ResolutionResult result =
        resolver.resolve(prepareRequestFor(repo.toUri(), dependsOnLower, dependsOnHigher));

    // Verify there's a conflict
    assertFalse("Expected a conflict to be recorded", result.getConflicts().isEmpty());

    // Verify the resolution graph contains only the higher version
    Graph<Coordinates> graph = result.getResolution();
    Set<Coordinates> conflictedNodes =
        graph.nodes().stream()
            .filter(c -> "conflicted".equals(c.getArtifactId()))
            .collect(Collectors.toSet());
    assertEquals("Should resolve to exactly one version", 1, conflictedNodes.size());
    assertTrue("Should resolve to higher version", conflictedNodes.contains(higherVersion));

    // Verify paths map contains only the resolved (higher) version, not the lower version
    Map<Coordinates, Path> paths = result.getPaths();
    assertTrue("Paths should contain resolved version", paths.containsKey(higherVersion));
    assertFalse(
        "Paths should not contain conflicting lower version", paths.containsKey(lowerVersion));
  }
}
