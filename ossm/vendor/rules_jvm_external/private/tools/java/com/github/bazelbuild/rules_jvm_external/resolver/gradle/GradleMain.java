package com.github.bazelbuild.rules_jvm_external.resolver.gradle;

import com.github.bazelbuild.rules_jvm_external.resolver.Resolver;
import com.github.bazelbuild.rules_jvm_external.resolver.cmd.AbstractMain;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;

public class GradleMain extends AbstractMain {

  public static void main(String[] args) {
    new GradleMain().doMain(args);
  }

  @Override
  public Resolver getResolver(Netrc netrc, int maxThreads, EventListener listener) {
    return new GradleResolver(netrc, maxThreads, listener);
  }
}
