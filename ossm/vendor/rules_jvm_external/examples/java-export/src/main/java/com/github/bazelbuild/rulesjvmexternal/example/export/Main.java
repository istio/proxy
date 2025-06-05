package com.github.bazelbuild.rulesjvmexternal.example.export;

import com.google.common.collect.ImmutableList;
import java.util.List;

/** A basic class with nothing special about it. */
public class Main {

  public static void main(String[] args) {
    var fromGuava = ImmutableList.of("Hello", "World!");
    var fromJre = List.of("Hello", "World!");

    if (!fromJre.equals(fromGuava)) {
      throw new RuntimeException("This is less than ideal");
    }
  }
}
