package com.github.bazelbuild.rulesjvmexternal.example.io;

import com.google.common.collect.ImmutableList;
import java.util.List;

public class Printer {

  public void showGreeting(String personToGreet) {
    // We take a dep on guava so that we can show off BOM generation.
    // No-one sensible would write this code normally.
    List<String> messages = ImmutableList.of("Hello, ", personToGreet, "!");

    for (String item : messages) {
      System.out.print(item);
    }
    System.out.print("\n");
  }
}
