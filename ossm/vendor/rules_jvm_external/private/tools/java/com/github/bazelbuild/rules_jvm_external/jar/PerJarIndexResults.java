package com.github.bazelbuild.rules_jvm_external.jar;

import java.util.SortedMap;
import java.util.SortedSet;

public class PerJarIndexResults {
  private final SortedSet<String> packages;
  private final SortedMap<String, SortedSet<String>> serviceImplementations;

  public PerJarIndexResults(
      SortedSet<String> packages, SortedMap<String, SortedSet<String>> serviceImplementations) {
    this.packages = packages;
    this.serviceImplementations = serviceImplementations;
  }

  public SortedSet<String> getPackages() {
    return this.packages;
  }

  public SortedMap<String, SortedSet<String>> getServiceImplementations() {
    return this.serviceImplementations;
  }
}
