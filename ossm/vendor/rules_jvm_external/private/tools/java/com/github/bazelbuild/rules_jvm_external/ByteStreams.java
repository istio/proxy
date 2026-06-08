package com.github.bazelbuild.rules_jvm_external;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class ByteStreams {

  private static final int COPY_BUFFER_SIZE = 1024 * 100;

  private ByteStreams() {
    // Utility methods
  }

  public static byte[] toByteArray(InputStream source) throws IOException {
    try (ByteArrayOutputStream bos = new ByteArrayOutputStream()) {
      copy(source, bos);
      return bos.toByteArray();
    }
  }

  public static void copy(InputStream source, OutputStream sink) throws IOException {
    byte[] buffer = new byte[COPY_BUFFER_SIZE];

    for (int read = source.read(buffer); read != -1; read = source.read(buffer)) {
      sink.write(buffer, 0, read);
    }
  }
}
