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

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.Conflict;
import com.github.bazelbuild.rules_jvm_external.resolver.DependencyInfo;
import com.google.gson.GsonBuilder;
import java.net.URI;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.TreeMap;
import org.junit.Test;

public class V2LockFileTest {

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
            new TreeMap<>());

    Map<String, Object> rendered = new V2LockFile(repos, Set.of(aggregator), Set.of()).render();

    Map<?, ?> artifacts = (Map<?, ?>) rendered.get("artifacts");
    Map<?, ?> data = (Map<?, ?>) artifacts.get("com.example:aggregator");
    Map<?, ?> shasums = (Map<?, ?>) data.get("shasums");

    HashMap<Object, Object> expected = new HashMap<>();
    expected.put("jar", null);
    assertEquals(expected, shasums);
  }

  @Test
  public void shouldRoundTripASimpleSetOfDependencies() {
    V2LockFile roundTripped = roundTrip(new V2LockFile(repos, Set.of(), Set.of()));

    assertEquals(repos, roundTripped.getRepositories());
    assertEquals(Set.of(), roundTripped.getDependencyInfos());
    assertEquals(Set.of(), roundTripped.getConflicts());
  }

  @Test
  public void shouldRoundTripM2Local() {
    V2LockFile lockFile = new V2LockFile(repos, Set.of(), Set.of());
    Map<String, Object> rendered = lockFile.render();
    rendered.put("m2local", true);

    V2LockFile roundTripped =
        V2LockFile.create(
            new GsonBuilder().setPrettyPrinting().serializeNulls().create().toJson(rendered));

    assertEquals(Set.of(defaultRepo, V2LockFile.M2_LOCAL_URI), roundTripped.getRepositories());
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
            new TreeMap<>());

    V2LockFile lockFile = roundTrip(new V2LockFile(repos, Set.of(info), Set.of()));

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
            new TreeMap<>());

    DependencyInfo info =
        new DependencyInfo(
            new Coordinates("com.example:item:1.0.0"),
            repos,
            Optional.empty(),
            Optional.of("c2c97a708be197aae5fee64dcc8b5e8a09c76c79a44c0e8e5b48b235084ec395"),
            Set.of(depCoords),
            Set.of(),
            new TreeMap<>());

    V2LockFile lockFile = roundTrip(new V2LockFile(repos, Set.of(info, dep), Set.of()));

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

    V2LockFile lockFile = roundTrip(new V2LockFile(repos, Set.of(), conflicts));

    assertEquals(conflicts, lockFile.getConflicts());
  }

  private V2LockFile roundTrip(V2LockFile lockFile) {
    Map<String, Object> rendered = lockFile.render();
    String converted =
        new GsonBuilder().setPrettyPrinting().serializeNulls().create().toJson(rendered) + "\n";
    return V2LockFile.create(converted);
  }
}
