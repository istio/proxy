// Copyright 2019 The Bazel Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package com.github.bazelbuild.rules_jvm_external;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/** A tool to compute the sha256 hash of a file. */
public class Hasher {

  public static void main(String[] args) throws NoSuchAlgorithmException, IOException {
    Stream<String> fileStream = argsToStream(args);
    System.out.print(hashFiles(fileStream));
  }

  static Stream<String> argsToStream(String[] args) throws IOException {
    Stream<String> fileStream;

    if (args.length == 2 && args[0].equals("--argsfile")) {
      String argfile = args[1];
      fileStream = Files.lines(Paths.get(argfile));
    } else {
      throw new IllegalArgumentException("We require use of --argsfile");
    }

    return fileStream;
  }

  static String hashFiles(Stream<String> files) {
    return files
        .parallel()
        .map(
            arg -> {
              final File file = new File(arg);

              // Since this tool is for private usage, just do a simple assertion for the filename
              // argument.
              if (!file.exists() || !file.isFile()) {
                throw new IllegalArgumentException(
                    "File does not exist or is not a file: " + file.getAbsolutePath());
              }

              try {
                return sha256(file) + " " + file + "\n";
              } catch (Exception ex) {
                throw new RuntimeException(ex);
              }
            })
        .collect(Collectors.joining());
  }

  static String sha256(File file) throws NoSuchAlgorithmException, IOException {
    byte[] buffer = new byte[8192];
    int count;
    MessageDigest digest = MessageDigest.getInstance("SHA-256");
    try (BufferedInputStream bufferedInputStream =
        new BufferedInputStream(new FileInputStream(file))) {
      while ((count = bufferedInputStream.read(buffer)) > 0) {
        digest.update(buffer, 0, count);
      }
    }

    // sha256 is always 64 characters.
    StringBuilder hexString = new StringBuilder(64);

    // Convert digest byte array to a hex string.
    for (byte b : digest.digest()) {
      String hex = Integer.toHexString(0xff & b);
      if (hex.length() == 1) {
        hexString.append('0');
      }
      hexString.append(hex);
    }

    return hexString.toString();
  }
}
