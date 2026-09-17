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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.Conflict;
import com.github.bazelbuild.rules_jvm_external.resolver.DependencyInfo;
import com.github.bazelbuild.rules_jvm_external.resolver.cmd.AbstractMain;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import java.io.IOException;
import java.net.URI;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import org.junit.Test;

public class V3LockFileTest {

  private final URI defaultRepo = URI.create("http://localhost/m2/repository/");
  private final Set<URI> repos = Set.of(defaultRepo);

  @Test
  public void shouldRenderAggregatingJarsAsJarWithNullShasum() {
    DependencyInfo aggregator =
        new DependencyInfo(
            new Coordinates("com.example:aggregator:1.0.0"),
            repos,
            Optional.empty(),
            Optional.empty(),
            Set.of(),
            Set.of(),
            Set.of(),
            new TreeMap<>());

    Map<String, Object> rendered =
        new V3LockFile(repos, Set.of(aggregator), Set.of(), true).render();

    Map<?, ?> artifacts = (Map<?, ?>) rendered.get("artifacts");
    Map<?, ?> data = (Map<?, ?>) artifacts.get("com.example:aggregator");
    Map<?, ?> shasums = (Map<?, ?>) data.get("shasums");

    HashMap<Object, Object> expected = new HashMap<>();
    expected.put("jar", null);
    assertEquals(expected, shasums);
  }

  @Test
  public void shouldRoundTripASimpleSetOfDependencies() {
    V3LockFile roundTripped = roundTrip(new V3LockFile(repos, Set.of(), Set.of(), true));

    assertEquals(repos, roundTripped.getRepositories());
    assertEquals(Set.of(), roundTripped.getDependencyInfos());
    assertEquals(Set.of(), roundTripped.getConflicts());
  }

  @Test
  public void shouldRoundTripM2Local() {
    V3LockFile lockFile = new V3LockFile(repos, Set.of(), Set.of(), true);
    Map<String, Object> rendered = lockFile.render();
    rendered.put("m2local", true);

    V3LockFile roundTripped =
        V3LockFile.create(
            new GsonBuilder().setPrettyPrinting().serializeNulls().create().toJson(rendered));

    assertEquals(Set.of(defaultRepo, V3LockFile.M2_LOCAL_URI), roundTripped.getRepositories());
  }

  @Test
  public void shouldRoundTripASingleArtifact() {
    DependencyInfo info =
        new DependencyInfo(
            new Coordinates("com.example:item:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("c2c97a708be197aae5fee64dcc8b5e8a09c76c79a44c0e8e5b48b235084ec395"),
            Set.of(),
            Set.of(),
            Set.of(),
            new TreeMap<>());

    V3LockFile lockFile = roundTrip(new V3LockFile(repos, Set.of(info), Set.of(), true));

    assertEquals(Set.of(info), lockFile.getDependencyInfos());
  }

  @Test
  public void shouldRoundTripASingleArtifactWithADependency() {
    Coordinates depCoords = new Coordinates("com.example:has-deps:1.0.0");

    DependencyInfo dep =
        new DependencyInfo(
            depCoords,
            repos,
            Optional.empty(),
            Optional.of("cafebad08be197aae5fee64dcc8b5e8a09c76c79a44c0e8e5b48b235084ec395"),
            Set.of(),
            Set.of(),
            Set.of(),
            new TreeMap<>());

    DependencyInfo info =
        new DependencyInfo(
            new Coordinates("com.example:item:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("c2c97a708be197aae5fee64dcc8b5e8a09c76c79a44c0e8e5b48b235084ec395"),
            Set.of(depCoords),
            Set.of(),
            Set.of(),
            new TreeMap<>());

    V3LockFile lockFile = roundTrip(new V3LockFile(repos, Set.of(info, dep), Set.of(), true));

    assertEquals(Set.of(info, dep), lockFile.getDependencyInfos());
  }

  @Test
  public void shouldRoundTripConflicts() {
    Set<Conflict> conflicts =
        Set.of(
            new Conflict(
                new Coordinates("com.foo:bar:1.2.3"), new Coordinates("com.foo:bar:1.0.0")),
            new Conflict(
                new Coordinates("com.foo:bar:1.2.3"), new Coordinates("com.foo:bar:1.2.1")));

    V3LockFile lockFile = roundTrip(new V3LockFile(repos, Set.of(), conflicts, true));

    assertEquals(conflicts, lockFile.getConflicts());
  }

  @Test
  public void shouldIncludePackagesWhenIncludePackagesIsTrue() {
    DependencyInfo info =
        new DependencyInfo(
            new Coordinates("com.example:item:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("abc123"),
            Set.of(),
            Set.of("com.example", "com.example.sub"),
            Set.of(),
            new TreeMap<>());

    Map<String, Object> rendered = new V3LockFile(repos, Set.of(info), Set.of(), true).render();

    assertNotNull(rendered.get("packages"));
    @SuppressWarnings("unchecked")
    Map<String, Set<String>> packages = (Map<String, Set<String>>) rendered.get("packages");
    assertFalse(packages.isEmpty());
    assertEquals(Set.of("com.example", "com.example.sub"), packages.get("com.example:item"));
  }

  @Test
  public void shouldExcludePackagesWhenIncludePackagesIsFalse() {
    DependencyInfo info =
        new DependencyInfo(
            new Coordinates("com.example:item:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("abc123"),
            Set.of(),
            Set.of("com.example", "com.example.sub"),
            Set.of(),
            new TreeMap<>());

    Map<String, Object> rendered = new V3LockFile(repos, Set.of(info), Set.of(), false).render();

    assertNull(rendered.get("packages"));
  }

  @Test
  public void shouldStillIncludeOtherFieldsWhenPackagesExcluded() {
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

    Map<String, Object> rendered = new V3LockFile(repos, Set.of(info), Set.of(), false).render();

    // Packages should be excluded
    assertNull(rendered.get("packages"));

    // But other fields should still be present
    assertNotNull(rendered.get("artifacts"));
    assertNotNull(rendered.get("dependencies"));
    assertNotNull(rendered.get("services"));
    assertNotNull(rendered.get("repositories"));
    assertEquals("3", rendered.get("version"));
  }

  @Test
  @SuppressWarnings("unchecked")
  public void testCalculateArtifactHashMatchesStoredHash() throws IOException {
    String lockFileContent =
        new String(
            getClass().getClassLoader().getResourceAsStream("maven_install.json").readAllBytes());

    Gson gson = new GsonBuilder().create();
    Map<String, Object> lockFileData = gson.fromJson(lockFileContent, Map.class);
    Map<String, Double> storedHash =
        (Map<String, Double>) lockFileData.get("__RESOLVED_ARTIFACTS_HASH");

    Map<String, Object> dependencies = (Map<String, Object>) lockFileData.remove("dependencies");
    Map<String, Set<String>> convertedDeps = new TreeMap<>();
    for (Map.Entry<String, Object> entry : dependencies.entrySet()) {
      java.util.List<String> depList = (java.util.List<String>) entry.getValue();
      convertedDeps.put(entry.getKey(), new TreeSet<>(depList));
    }
    lockFileData.put("dependencies", convertedDeps);

    Map<String, Integer> calculatedHash = AbstractMain.calculateArtifactHash(lockFileData);

    assertEquals(
        "Hash mismatch: calculated hash does not match stored hash",
        storedHash.size(),
        calculatedHash.size());

    for (Map.Entry<String, Double> entry : storedHash.entrySet()) {
      String key = entry.getKey();
      int expectedHash = entry.getValue().intValue();
      int actualHash = calculatedHash.get(key);

      assertEquals(String.format("Hash mismatch for artifact '%s'", key), expectedHash, actualHash);
    }
  }

  private V3LockFile roundTrip(V3LockFile lockFile) {
    Map<String, Object> rendered = lockFile.render();
    String converted =
        new GsonBuilder().setPrettyPrinting().serializeNulls().create().toJson(rendered) + "\n";
    return V3LockFile.create(converted);
  }
}
