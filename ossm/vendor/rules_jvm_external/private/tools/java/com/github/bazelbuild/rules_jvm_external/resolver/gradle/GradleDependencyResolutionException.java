package com.github.bazelbuild.rules_jvm_external.resolver.gradle;

import com.github.bazelbuild.rules_jvm_external.resolver.gradle.models.GradleUnresolvedDependency;
import java.util.Collections;
import java.util.List;

public class GradleDependencyResolutionException extends Exception {

  private final List<GradleUnresolvedDependency> unresolved;

  public GradleDependencyResolutionException(List<GradleUnresolvedDependency> unresolved) {
    super(buildMessage(unresolved));
    this.unresolved = Collections.unmodifiableList(unresolved);
  }

  private static String buildMessage(List<GradleUnresolvedDependency> unresolved) {
    if (unresolved.size() == 1) {
      GradleUnresolvedDependency dep = unresolved.get(0);
      return "Failed to resolve dependency: " + gav(dep) + " (" + dep.getFailureReason() + ")";
    }
    StringBuilder sb = new StringBuilder("Multiple dependencies failed to resolve:");
    for (GradleUnresolvedDependency dep : unresolved) {
      sb.append("\n  - ").append(gav(dep)).append(" (").append(dep.getFailureReason()).append(")");
    }
    return sb.toString();
  }

  private static String gav(GradleUnresolvedDependency d) {
    return d.getGroup() + ":" + d.getName() + ":" + d.getVersion();
  }

  public List<GradleUnresolvedDependency> getUnresolved() {
    return unresolved;
  }

  @Override
  public String toString() {
    return getClass().getSimpleName() + ": " + getMessage();
  }
}
