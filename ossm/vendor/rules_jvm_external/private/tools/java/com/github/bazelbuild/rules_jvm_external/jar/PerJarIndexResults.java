package com.github.bazelbuild.rules_jvm_external.jar;

import java.util.SortedMap;
import java.util.SortedSet;

public class PerJarIndexResults {
  private final SortedSet<String> packages;
  private final SortedSet<String> classes;
  private final SortedMap<String, SortedSet<String>> serviceImplementations;

  public PerJarIndexResults(
      SortedSet<String> packages,
      SortedSet<String> classes,
      SortedMap<String, SortedSet<String>> serviceImplementations) {
    this.packages = packages;
    this.classes = classes;
    this.serviceImplementations = serviceImplementations;
  }

  public SortedSet<String> getPackages() {
    return this.packages;
  }

  public SortedSet<String> getClasses() {
    return this.classes;
  }

  public SortedMap<String, SortedSet<String>> getServiceImplementations() {
    return this.serviceImplementations;
  }
}
