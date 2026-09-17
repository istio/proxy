package com.github.bazelbuild.rules_jvm_external.examples.bzlmod;

import com.google.common.collect.ImmutableList;
import java.util.List;
import org.openqa.selenium.WebDriver;

/** A basic class with nothing special about it. */
public class Main {

  public static void main(String[] args) {
    var fromGuava = ImmutableList.of("Hello", "World!");
    var fromJre = List.of("Hello", "World!");

    if (!fromJre.equals(fromGuava)) {
      throw new RuntimeException("This is less than ideal");
    }

    // Make sure we can import something from the selenium-api jar
    // We don't want to fire up a full driver, because we might not
    // have access to the binaries.
    WebDriver driver = null;
  }
}
