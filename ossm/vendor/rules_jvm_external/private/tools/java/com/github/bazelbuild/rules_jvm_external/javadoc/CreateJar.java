package com.github.bazelbuild.rules_jvm_external.javadoc;

import com.github.bazelbuild.rules_jvm_external.zip.StableZipEntry;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UncheckedIOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Comparator;
import java.util.stream.Stream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public class CreateJar {

  public static void createJar(Path out, Path inputDir) throws IOException {
    try (OutputStream os = Files.newOutputStream(out);
        ZipOutputStream zos = new ZipOutputStream(os);
        Stream<Path> walk = Files.walk(inputDir)) {

      walk.sorted(Comparator.naturalOrder())
          .forEachOrdered(
              path -> {
                if (path.equals(inputDir)) {
                  return;
                }

                try {
                  String name =
                      inputDir.relativize(path).toString().replace(File.separatorChar, '/');
                  if (Files.isDirectory(path)) {
                    name += "/";
                    ZipEntry entry = new StableZipEntry(name);
                    zos.putNextEntry(entry);
                    zos.closeEntry();
                  } else {
                    ZipEntry entry = new StableZipEntry(name);
                    zos.putNextEntry(entry);
                    try (InputStream is = Files.newInputStream(path)) {
                      com.github.bazelbuild.rules_jvm_external.ByteStreams.copy(is, zos);
                    }
                    zos.closeEntry();
                  }
                } catch (IOException e) {
                  throw new UncheckedIOException(e);
                }
              });
    }
  }
}
