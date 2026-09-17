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

package com.github.bazelbuild.rules_jvm_external.resolver.gradle.models;

import java.io.File;
import java.util.Map;

/** Represents a Maven artifact fetched by Gradle using the ArtifactView API */
public interface GradleResolvedArtifact {
  String getClassifier();

  String getExtension();

  File getFile();

  // Currently unused, but can be used in the future to model gradle variants associated with
  // artifacts
  // useful for KMP especially
  Map<String, String> getVariantAttributes();

  void setClassifier(String classifier);

  void setExtension(String extension);

  void setFile(File file);

  void setVariantAttributes(Map<String, String> variantAttributes);
}
