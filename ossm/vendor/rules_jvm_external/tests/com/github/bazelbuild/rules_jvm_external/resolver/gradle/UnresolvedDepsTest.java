// Copyright 2025 The Bazel Authors. All rights reserved.
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

import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleUnresolvedDependency;
import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleUnresolvedDependencyImpl;
import java.util.List;
import java.util.Set;
import org.junit.Test;

/**
 * Unit tests for the unresolved dependency filtering logic in GradleResolver.
 *
 * <p>This tests the fix for a bug where the resolver would incorrectly throw an exception when a
 * dependency version appeared as "unresolved" but the same artifact was successfully resolved at a
 * different version. This can happen when:
 *
 * <ul>
 *   <li>A BOM upgrades a dependency to a different version
 *   <li>Gradle's version conflict resolution picks a different version than requested
 *   <li>The detached configuration reports the originally-requested version as unresolved
 * </ul>
 *
 * <p>The fix ensures we only throw for truly unresolved dependencies - those where no version of
 * the same group:artifact was resolved.
 */
public class UnresolvedDepsTest {

  @Test
  public void shouldNotReportAsFailedWhenSameArtifactResolvedAtDifferentVersion() {
    // Scenario: User requests tink:1.12.0, BOM upgrades to tink:1.16.0
    // tink:1.12.0 appears in unresolved list, but tink:1.16.0 is resolved
    // The fix should skip tink:1.12.0 because com.example:tink was resolved

    Set<String> requestedDepKeys = Set.of("com.example:tink:1.12.0", "com.example:other:1.0.0");

    // The resolved graph has tink:1.16.0 (upgraded by BOM)
    Set<String> resolvedGroupArtifacts = Set.of("com.example:tink", "com.example:other");

    // The unresolved list has tink:1.12.0 (from detached config or version conflict)
    List<GradleUnresolvedDependency> unresolvedDeps =
        List.of(
            new GradleUnresolvedDependencyImpl(
                "com.example",
                "tink",
                "1.12.0",
                GradleUnresolvedDependency.FailureReason.INTERNAL,
                "Version conflict"));

    // Should NOT report tink:1.12.0 as failed because tink was resolved
    List<GradleUnresolvedDependency> filtered =
        GradleResolver.filterUnresolvedRequestedDeps(
            unresolvedDeps, requestedDepKeys, resolvedGroupArtifacts);

    assertTrue(
        "Should not report tink as failed because it was resolved at a different version",
        filtered.isEmpty());
  }

  @Test
  public void shouldReportAsFailedWhenArtifactTrulyNotResolved() {
    Set<String> requestedDepKeys = Set.of("com.example:missing:1.0.0", "com.example:other:1.0.0");

    Set<String> resolvedGroupArtifacts = Set.of("com.example:other");

    List<GradleUnresolvedDependency> unresolvedDeps =
        List.of(
            new GradleUnresolvedDependencyImpl(
                "com.example",
                "missing",
                "1.0.0",
                GradleUnresolvedDependency.FailureReason.NOT_FOUND,
                "Artifact not found"));

    List<GradleUnresolvedDependency> filtered =
        GradleResolver.filterUnresolvedRequestedDeps(
            unresolvedDeps, requestedDepKeys, resolvedGroupArtifacts);

    assertFalse(
        "Should report missing as failed because it was not resolved at any version",
        filtered.isEmpty());
    assertEquals(1, filtered.size());
    assertEquals("missing", filtered.get(0).getName());
  }

  @Test
  public void filterOnlyChecksDirectlyRequestedDependencies() {
    // This filter only checks dependencies that the user explicitly requested.
    // Transitive dependencies that appear in the unresolved list are ignored by this filter
    // because Gradle's own resolution handles transitive dependency failures - if a required
    // transitive dependency is truly missing, Gradle will fail the resolution before we
    // reach this check. This filter exists solely to catch false positives where Gradle
    // reports a requested version as "unresolved" even though a different version of the
    // same artifact was successfully resolved (e.g., due to BOM upgrades or conflict resolution).

    Set<String> requestedDepKeys = Set.of("com.example:direct:1.0.0");

    // The resolved graph has both direct and transitive
    Set<String> resolvedGroupArtifacts = Set.of("com.example:direct", "com.example:transitive");

    // The unresolved list has transitive:1.0.0 (some version that lost conflict resolution)
    List<GradleUnresolvedDependency> unresolvedDeps =
        List.of(
            new GradleUnresolvedDependencyImpl(
                "com.example",
                "transitive",
                "1.0.0",
                GradleUnresolvedDependency.FailureReason.INTERNAL,
                "Version conflict"));

    // Should be empty because transitive:1.0.0 was not in requestedDepKeys -
    // we only validate directly requested dependencies, not transitives
    List<GradleUnresolvedDependency> filtered =
        GradleResolver.filterUnresolvedRequestedDeps(
            unresolvedDeps, requestedDepKeys, resolvedGroupArtifacts);

    assertTrue(
        "Filter should ignore transitive deps - only directly requested deps are checked",
        filtered.isEmpty());
  }

  @Test
  public void shouldHandleMultipleUnresolvedEntriesForSameArtifact() {
    // The same artifact appears multiple times in the unresolved list.
    // If the artifact was resolved at any version, all entries should be skipped

    Set<String> requestedDepKeys = Set.of("com.example:tink:1.12.0");

    // The resolved graph has tink (at a different version)
    Set<String> resolvedGroupArtifacts = Set.of("com.example:tink");

    // The unresolved list has tink:1.12.0 appearing 3 times
    List<GradleUnresolvedDependency> unresolvedDeps =
        List.of(
            new GradleUnresolvedDependencyImpl(
                "com.example",
                "tink",
                "1.12.0",
                GradleUnresolvedDependency.FailureReason.INTERNAL,
                "Conflict 1"),
            new GradleUnresolvedDependencyImpl(
                "com.example",
                "tink",
                "1.12.0",
                GradleUnresolvedDependency.FailureReason.INTERNAL,
                "Conflict 2"),
            new GradleUnresolvedDependencyImpl(
                "com.example",
                "tink",
                "1.12.0",
                GradleUnresolvedDependency.FailureReason.INTERNAL,
                "Conflict 3"));

    // All entries should be skipped because tink was resolved
    List<GradleUnresolvedDependency> filtered =
        GradleResolver.filterUnresolvedRequestedDeps(
            unresolvedDeps, requestedDepKeys, resolvedGroupArtifacts);

    assertTrue(
        "Should not report any tink entries as failed because tink was resolved",
        filtered.isEmpty());
  }
}
