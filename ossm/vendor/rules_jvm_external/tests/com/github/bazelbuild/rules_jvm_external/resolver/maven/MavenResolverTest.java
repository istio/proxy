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

package com.github.bazelbuild.rules_jvm_external.resolver.maven;

import static org.junit.Assert.fail;

import com.github.bazelbuild.rules_jvm_external.Coordinates;
import com.github.bazelbuild.rules_jvm_external.resolver.MavenRepo;
import com.github.bazelbuild.rules_jvm_external.resolver.Resolver;
import com.github.bazelbuild.rules_jvm_external.resolver.ResolverTestBase;
import com.github.bazelbuild.rules_jvm_external.resolver.cmd.ResolverConfig;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import java.nio.file.Path;
import org.eclipse.aether.transfer.ArtifactNotFoundException;
import org.junit.Test;

public class MavenResolverTest extends ResolverTestBase {

  @Override
  protected Resolver getResolver(Netrc netrc, EventListener listener) {
    return new MavenResolver(netrc, ResolverConfig.DEFAULT_MAX_THREADS, listener);
  }

  @Test
  public void shouldSuccessfullyResolveNettyStaticClasses() {
    Coordinates main = new Coordinates("com.example:root:1.0.0");
    Coordinates x86Dep = new Coordinates("com.example", "root", null, "linux-x86_64", "1.0.0");

    Path repo = MavenRepo.create().add(main, x86Dep).getPath();

    // There should be no cycle detected by this dependency
    resolver.resolve(prepareRequestFor(repo.toUri(), main));
  }

  @Test
  public void shouldErrorOnMissingTopLevelDependency() {
    Coordinates missing = new Coordinates("com.example:missing:1.0.0");
    Path repo = MavenRepo.create().getPath();

    try {
      resolver.resolve(prepareRequestFor(repo.toUri(), missing)).getResolution();
      fail("Expected ArtifactNotFoundException");
    } catch (Exception e) {
      if (!(e.getCause() instanceof ArtifactNotFoundException)) {
        fail("Expected ArtifactNotFoundException");
      }
    }
  }
}
