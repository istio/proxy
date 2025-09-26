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

package com.github.bazelbuild.rules_jvm_external.resolver.cmd;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.google.common.collect.ImmutableSet;
import java.util.Set;

public class ExternalResolverConfig {

  private String resolver;
  private Set<String> repositories;
  private Set<String> globalExclusions;
  private Set<ConfigArtifact> artifacts;
  private Set<ConfigArtifact> boms;

  private boolean fetchSources;
  private boolean fetchJavadoc;
  private boolean useUnsafeSharedCache;

  public Set<String> getRepositories() {
    return repositories == null ? ImmutableSet.of() : ImmutableSet.copyOf(repositories);
  }

  public Set<Coordinates> getGlobalExclusions() {
    if (globalExclusions == null) {
      return Set.of();
    }

    return globalExclusions.stream().map(Coordinates::new).collect(ImmutableSet.toImmutableSet());
  }

  public Set<ConfigArtifact> getArtifacts() {
    return artifacts == null ? ImmutableSet.of() : ImmutableSet.copyOf(artifacts);
  }

  public Set<ConfigArtifact> getBoms() {
    return boms == null ? ImmutableSet.of() : ImmutableSet.copyOf(boms);
  }

  public boolean isFetchSources() {
    return fetchSources;
  }

  public boolean isFetchJavadoc() {
    return fetchJavadoc;
  }

  public boolean isUsingUnsafeSharedCache() {
    return useUnsafeSharedCache;
  }

  public String getResolver() {
    return resolver;
  }
}
