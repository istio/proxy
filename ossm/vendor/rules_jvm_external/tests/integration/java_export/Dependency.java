package com.jvm.external.jvm_export;

import com.google.common.collect.ImmutableList;
import java.util.List;

public class Dependency {

  public List<String> createImmutableList(String... of) {
    return ImmutableList.copyOf(of);
  }
}
