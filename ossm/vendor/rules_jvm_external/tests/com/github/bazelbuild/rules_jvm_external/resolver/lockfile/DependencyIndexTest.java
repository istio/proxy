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

package com.github.bazelbuild.rules_jvm_external.resolver.lockfile;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.DependencyInfo;
import java.net.URI;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import org.junit.Test;

public class DependencyIndexTest {

  private final URI defaultRepo = URI.create("http://localhost/m2/repository/");
  private final Set<URI> repos = Set.of(defaultRepo);

  @Test
  public void shouldIncludeVersionNumber() {
    DependencyIndex index = new DependencyIndex(Set.of());
    Map<String, Object> rendered = index.render();

    assertEquals(1, rendered.get("version"));
  }

  @Test
  public void shouldRenderEmptySectionsForEmptyInfos() {
    DependencyIndex index = new DependencyIndex(Set.of());
    Map<String, Object> rendered = index.render();

    @SuppressWarnings("unchecked")
    Map<String, Set<String>> packages = (Map<String, Set<String>>) rendered.get("packages");
    assertTrue(packages.isEmpty());

    @SuppressWarnings("unchecked")
    Map<String, Map<String, Set<String>>> classes =
        (Map<String, Map<String, Set<String>>>) rendered.get("split_package_classes");
    assertTrue(classes.isEmpty());
  }

  @Test
  public void shouldPutUniquePackagesInPackagesSection() {
    Set<String> artifactClasses = new TreeSet<>();
    artifactClasses.add("com.example.Foo");
    artifactClasses.add("com.example.Bar");
    artifactClasses.add("com.example.sub.Baz");

    DependencyInfo info =
        new DependencyInfo(
            new Coordinates("com.example:item:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("abc123"),
            Set.of(),
            Set.of("com.example", "com.example.sub"),
            artifactClasses,
            new TreeMap<>());

    DependencyIndex index = new DependencyIndex(Set.of(info));
    Map<String, Object> rendered = index.render();

    // Unique packages go to "packages" section
    @SuppressWarnings("unchecked")
    Map<String, Set<String>> packages = (Map<String, Set<String>>) rendered.get("packages");
    assertEquals(1, packages.size());
    assertEquals(Set.of("com.example", "com.example.sub"), packages.get("com.example:item"));

    // No collisions, so "split_package_classes" section should be empty for this artifact
    @SuppressWarnings("unchecked")
    Map<String, Map<String, Set<String>>> classes =
        (Map<String, Map<String, Set<String>>>) rendered.get("split_package_classes");
    assertTrue(classes.isEmpty());
  }

  @Test
  public void shouldPutCollidingPackagesInClassesSection() {
    // Two artifacts share the same package "javax.annotation"
    Set<String> guavaClasses = new TreeSet<>();
    guavaClasses.add("javax.annotation.Nullable");
    guavaClasses.add("com.google.common.base.Optional");

    Set<String> jsr305Classes = new TreeSet<>();
    jsr305Classes.add("javax.annotation.Nonnull");
    jsr305Classes.add("javax.annotation.Nullable");

    DependencyInfo guava =
        new DependencyInfo(
            new Coordinates("com.google.guava:guava:33.0.0"),
            repos,
            Optional.empty(),
            Optional.of("abc123"),
            Set.of(),
            Set.of("javax.annotation", "com.google.common.base"),
            guavaClasses,
            new TreeMap<>());

    DependencyInfo jsr305 =
        new DependencyInfo(
            new Coordinates("com.google.code.findbugs:jsr305:3.0.2"),
            repos,
            Optional.empty(),
            Optional.of("def456"),
            Set.of(),
            Set.of("javax.annotation"),
            jsr305Classes,
            new TreeMap<>());

    DependencyIndex index = new DependencyIndex(Set.of(guava, jsr305));
    Map<String, Object> rendered = index.render();

    // Unique packages go to "packages" section
    @SuppressWarnings("unchecked")
    Map<String, Set<String>> packages = (Map<String, Set<String>>) rendered.get("packages");
    assertEquals(1, packages.size());
    assertEquals(Set.of("com.google.common.base"), packages.get("com.google.guava:guava"));
    assertNull(packages.get("com.google.code.findbugs:jsr305")); // jsr305 has no unique packages

    // Colliding package goes to "split_package_classes" section with full class listings
    @SuppressWarnings("unchecked")
    Map<String, Map<String, Set<String>>> classes =
        (Map<String, Map<String, Set<String>>>) rendered.get("split_package_classes");
    assertEquals(2, classes.size());

    Map<String, Set<String>> guavaCollisions = classes.get("com.google.guava:guava");
    assertEquals(1, guavaCollisions.size());
    assertEquals(Set.of("Nullable"), guavaCollisions.get("javax.annotation"));

    Map<String, Set<String>> jsr305Collisions = classes.get("com.google.code.findbugs:jsr305");
    assertEquals(1, jsr305Collisions.size());
    assertEquals(Set.of("Nonnull", "Nullable"), jsr305Collisions.get("javax.annotation"));
  }

  @Test
  public void shouldHandleArtifactWithBothUniqueAndCollidingPackages() {
    Set<String> artifact1Classes = new TreeSet<>();
    artifact1Classes.add("com.shared.Foo");
    artifact1Classes.add("com.unique1.Bar");

    Set<String> artifact2Classes = new TreeSet<>();
    artifact2Classes.add("com.shared.Baz");
    artifact2Classes.add("com.unique2.Qux");

    DependencyInfo artifact1 =
        new DependencyInfo(
            new Coordinates("com.example:artifact1:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("abc123"),
            Set.of(),
            Set.of("com.shared", "com.unique1"),
            artifact1Classes,
            new TreeMap<>());

    DependencyInfo artifact2 =
        new DependencyInfo(
            new Coordinates("com.example:artifact2:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("def456"),
            Set.of(),
            Set.of("com.shared", "com.unique2"),
            artifact2Classes,
            new TreeMap<>());

    DependencyIndex index = new DependencyIndex(Set.of(artifact1, artifact2));
    Map<String, Object> rendered = index.render();

    // Unique packages in "packages" section
    @SuppressWarnings("unchecked")
    Map<String, Set<String>> packages = (Map<String, Set<String>>) rendered.get("packages");
    assertEquals(2, packages.size());
    assertEquals(Set.of("com.unique1"), packages.get("com.example:artifact1"));
    assertEquals(Set.of("com.unique2"), packages.get("com.example:artifact2"));

    // Colliding package in "split_package_classes" section
    @SuppressWarnings("unchecked")
    Map<String, Map<String, Set<String>>> classes =
        (Map<String, Map<String, Set<String>>>) rendered.get("split_package_classes");
    assertEquals(2, classes.size());

    assertEquals(Set.of("Foo"), classes.get("com.example:artifact1").get("com.shared"));
    assertEquals(Set.of("Baz"), classes.get("com.example:artifact2").get("com.shared"));
  }

  @Test
  public void shouldHandleDefaultPackage() {
    Set<String> artifactClasses = new TreeSet<>();
    artifactClasses.add("Foo");

    DependencyInfo info =
        new DependencyInfo(
            new Coordinates("com.example:item:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("abc123"),
            Set.of(),
            Set.of(""),
            artifactClasses,
            new TreeMap<>());

    DependencyIndex index = new DependencyIndex(Set.of(info));
    Map<String, Object> rendered = index.render();

    // Default package (empty string) should be in packages section since it's unique
    @SuppressWarnings("unchecked")
    Map<String, Set<String>> packages = (Map<String, Set<String>>) rendered.get("packages");
    assertEquals(Set.of(""), packages.get("com.example:item"));
  }

  @Test
  public void shouldSkipSourcesArtifacts() {
    Set<String> artifactClasses = new TreeSet<>();
    artifactClasses.add("com.example.Foo");

    DependencyInfo sources =
        new DependencyInfo(
            new Coordinates("com.example:item:jar:sources:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("abc123"),
            Set.of(),
            Set.of(),
            artifactClasses,
            new TreeMap<>());

    DependencyIndex index = new DependencyIndex(Set.of(sources));
    Map<String, Object> rendered = index.render();

    @SuppressWarnings("unchecked")
    Map<String, Set<String>> packages = (Map<String, Set<String>>) rendered.get("packages");
    assertTrue(packages.isEmpty());

    @SuppressWarnings("unchecked")
    Map<String, Map<String, Set<String>>> classes =
        (Map<String, Map<String, Set<String>>>) rendered.get("split_package_classes");
    assertTrue(classes.isEmpty());
  }

  @Test
  public void shouldSkipJavadocArtifacts() {
    Set<String> artifactClasses = new TreeSet<>();
    artifactClasses.add("com.example.Foo");

    DependencyInfo javadoc =
        new DependencyInfo(
            new Coordinates("com.example:item:jar:javadoc:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("abc123"),
            Set.of(),
            Set.of(),
            artifactClasses,
            new TreeMap<>());

    DependencyIndex index = new DependencyIndex(Set.of(javadoc));
    Map<String, Object> rendered = index.render();

    @SuppressWarnings("unchecked")
    Map<String, Set<String>> packages = (Map<String, Set<String>>) rendered.get("packages");
    assertTrue(packages.isEmpty());

    @SuppressWarnings("unchecked")
    Map<String, Map<String, Set<String>>> classes =
        (Map<String, Map<String, Set<String>>>) rendered.get("split_package_classes");
    assertTrue(classes.isEmpty());
  }

  @Test
  public void shouldSkipArtifactsWithNoClasses() {
    DependencyInfo info =
        new DependencyInfo(
            new Coordinates("com.example:item:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("abc123"),
            Set.of(),
            Set.of("com.example"),
            Set.of(),
            new TreeMap<>());

    DependencyIndex index = new DependencyIndex(Set.of(info));
    Map<String, Object> rendered = index.render();

    @SuppressWarnings("unchecked")
    Map<String, Set<String>> packages = (Map<String, Set<String>>) rendered.get("packages");
    assertTrue(packages.isEmpty());

    @SuppressWarnings("unchecked")
    Map<String, Map<String, Set<String>>> classes =
        (Map<String, Map<String, Set<String>>>) rendered.get("split_package_classes");
    assertTrue(classes.isEmpty());
  }
}
