package com.github.bazelbuild.rules_jvm_external.manifest;

import java.io.IOException;
import java.util.jar.JarFile;
import java.util.jar.Manifest;

public class Print {

  public static void main(String[] args) throws IOException {
    try (JarFile jar = new JarFile(args[0])) {
      Manifest manifest = jar.getManifest();

      manifest.getMainAttributes().forEach((k, v) -> System.out.printf("%s: %s%n", k, v));
    }
  }
}
