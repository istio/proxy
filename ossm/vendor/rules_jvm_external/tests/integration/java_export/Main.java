package com.jvm.external.jvm_export;

public class Main {

  public static void main(String[] args) {
    System.out.printf("List is: %s\n", new Dependency().createImmutableList("Hello", "World!"));
  }
}
