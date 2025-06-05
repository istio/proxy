package com.github.bazelbuild.rules_jvm_external;

import static java.nio.charset.StandardCharsets.UTF_8;

import com.github.bazelbuild.rules_jvm_external.zip.StableZipEntry;
import com.google.common.collect.ImmutableMap;
import com.google.common.io.ByteStreams;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

public class ZipUtils {
  public static void createJar(Path outputTo, Map<String, String> pathToContents)
      throws IOException {
    try (OutputStream os = Files.newOutputStream(outputTo);
        ZipOutputStream zos = new ZipOutputStream(os)) {

      for (Map.Entry<String, String> entry : pathToContents.entrySet()) {
        ZipEntry ze = new StableZipEntry(entry.getKey());
        zos.putNextEntry(ze);
        if (!ze.isDirectory()) {
          zos.write(entry.getValue().getBytes(UTF_8));
        }
        zos.closeEntry();
      }
    }
  }

  public static Map<String, String> readJar(Path jar) throws IOException {
    ImmutableMap.Builder<String, String> builder = ImmutableMap.builder();

    try (InputStream is = Files.newInputStream(jar);
        ZipInputStream zis = new ZipInputStream(is)) {

      for (ZipEntry entry = zis.getNextEntry(); entry != null; entry = zis.getNextEntry()) {
        if (entry.isDirectory()) {
          continue;
        }

        builder.put(entry.getName(), new String(ByteStreams.toByteArray(zis), UTF_8));
      }
    }

    return builder.build();
  }
}
