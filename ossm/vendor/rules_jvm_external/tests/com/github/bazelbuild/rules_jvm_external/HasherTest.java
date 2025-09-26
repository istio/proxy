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

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.collection.IsArrayWithSize.arrayWithSize;
import static org.hamcrest.core.StringEndsWith.endsWith;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.security.NoSuchAlgorithmException;
import java.util.stream.Stream;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class HasherTest {

  @Rule public TemporaryFolder tmpDir = new TemporaryFolder();

  @Test
  public void sha256_emptyFile() throws IOException, NoSuchAlgorithmException {
    File file = tmpDir.newFile("test.file");

    // "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" is sha of null content.
    assertThat(
        Hasher.sha256(file),
        equalTo("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  }

  @Test
  public void sha256_helloWorldFile() throws IOException, NoSuchAlgorithmException {
    File file = writeFile("test.file", "Hello World!");
    assertThat(
        Hasher.sha256(file),
        equalTo("7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069"));
  }

  @Test
  public void sha256_multipleFiles() throws IOException, FileNotFoundException {
    File file1 = writeFile("test-1.file", "Hello World!");
    File file2 = writeFile("test-2.file", "Hello!");
    File file3 = writeFile("test-3.file", "Hello World!");
    File argsfile = writeFile("argsfile", "test-1.file\ntest-2.file\ntest-3.file");
    String[] files = new String[] {file1.getPath(), file2.getPath(), file3.getPath()};

    String result = Hasher.hashFiles(Stream.of(files));
    String[] lines = result.split("\n");
    assertThat(lines, arrayWithSize(3));

    checkLine(
        lines[0],
        "7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069",
        "test-1.file");
    checkLine(
        lines[1],
        "334d016f755cd6dc58c53a86e183882f8ec14f52fb05345887c8a5edd42c87b7",
        "test-2.file");
    checkLine(
        lines[2],
        "7f83b1657ff1fc53b92dc18148a1d65dfc2d4b1fa3d677284addd200126d9069",
        "test-3.file");
  }

  @Test
  public void argsToStream_throws_exception() throws IOException {
    String[] files = new String[] {};
    try {
      Stream<String> stream = Hasher.argsToStream(files);
      fail("Should not process args without --argsfile");
    } catch (IllegalArgumentException e) {
      // This is good
    }
  }

  @Test
  public void argsToStream_argsfile_empty() throws IOException, FileNotFoundException {
    File argsfile = writeFile("argsfile", "");
    String[] args = new String[] {"--argsfile", argsfile.getPath()};
    String[] expected = new String[] {};
    Stream<String> stream = Hasher.argsToStream(args);
    assertArrayEquals(stream.toArray(), expected);
  }

  @Test
  public void argsToStream_argsfile() throws IOException, FileNotFoundException {
    File argsfile = writeFile("argsfile", "file1");
    String[] args = new String[] {"--argsfile", argsfile.getPath()};
    String[] expected = new String[] {"file1"};
    Stream<String> stream = Hasher.argsToStream(args);
    assertArrayEquals(stream.toArray(), expected);

    argsfile = writeFile("argsfile2", "file1");
    args = new String[] {"--argsfile", argsfile.getPath()};
    expected = new String[] {"file1"};
    stream = Hasher.argsToStream(args);
    assertArrayEquals(stream.toArray(), expected);

    argsfile = writeFile("argsfile3", "file1\nfile2");
    args = new String[] {"--argsfile", argsfile.getPath()};
    expected = new String[] {"file1", "file2"};
    stream = Hasher.argsToStream(args);
    assertArrayEquals(stream.toArray(), expected);

    argsfile = writeFile("argsfile4", "file1\nfile2\nfile3");
    args = new String[] {"--argsfile", argsfile.getPath()};
    expected = new String[] {"file1", "file2", "file3"};
    stream = Hasher.argsToStream(args);
    assertArrayEquals(stream.toArray(), expected);
  }

  private File writeFile(String name, String contents) throws IOException {
    File file = tmpDir.newFile(name);

    try (PrintStream out = new PrintStream(new FileOutputStream(file))) {
      out.print(contents);
    }
    return file;
  }

  private void checkLine(String line, String hash, String file) {
    String[] parts = line.split("\\s+");
    assertThat(parts[0], equalTo(hash));
    assertThat(parts[1], endsWith(file));
  }
}
