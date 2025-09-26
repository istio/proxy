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
import java.io.Serializable;
import java.util.Map;

public class GradleResolvedArtifactImpl implements Serializable, GradleResolvedArtifact {
  private String classifier;
  private String extension;
  private File file;
  private Map<String, String> variantAttributes;

  public GradleResolvedArtifactImpl() {}

  public String getClassifier() {
    return this.classifier;
  }

  public String getExtension() {
    return this.extension;
  }

  public File getFile() {
    return this.file;
  }

  public Map<String, String> getVariantAttributes() {
    return variantAttributes;
  }

  public void setClassifier(String classifier) {
    this.classifier = classifier;
  }

  public void setExtension(String extension) {
    this.extension = extension;
  }

  public void setFile(File file) {
    this.file = file;
  }

  public void setVariantAttributes(Map<String, String> variantAttributes) {
    this.variantAttributes = variantAttributes;
  }
}
