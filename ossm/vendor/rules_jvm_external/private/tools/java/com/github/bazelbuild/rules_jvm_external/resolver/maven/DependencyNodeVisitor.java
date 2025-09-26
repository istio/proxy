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

import java.util.function.Consumer;
import org.eclipse.aether.graph.DependencyNode;
import org.eclipse.aether.graph.DependencyVisitor;

class DependencyNodeVisitor implements DependencyVisitor {

  private final Consumer<DependencyNode> onNode;

  public DependencyNodeVisitor(Consumer<DependencyNode> onNode) {
    this.onNode = onNode;
  }

  @Override
  public boolean visitEnter(DependencyNode node) {
    return !node.getDependency().isOptional();
  }

  @Override
  public boolean visitLeave(DependencyNode node) {
    onNode.accept(node);
    return true;
  }
}
