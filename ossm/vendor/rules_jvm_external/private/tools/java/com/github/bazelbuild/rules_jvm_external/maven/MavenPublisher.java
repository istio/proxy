// Licensed to the Software Freedom Conservancy (SFC) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The SFC licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package com.github.bazelbuild.rules_jvm_external.maven;

import static com.github.bazelbuild.rules_jvm_external.maven.MavenSigning.gpg_sign;
import static com.github.bazelbuild.rules_jvm_external.maven.MavenSigning.in_memory_pgp_sign;
import static java.nio.charset.StandardCharsets.US_ASCII;
import static java.nio.charset.StandardCharsets.UTF_8;
import static java.util.concurrent.TimeUnit.MINUTES;

import com.github.bazelbuild.rules_jvm_external.ByteStreams;
import com.google.auth.Credentials;
import com.google.auth.oauth2.GoogleCredentials;
import com.google.cloud.WriteChannel;
import com.google.cloud.storage.BlobInfo;
import com.google.cloud.storage.Storage;
import com.google.cloud.storage.StorageOptions;
import com.google.common.base.Splitter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.math.BigInteger;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.nio.channels.Channels;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeoutException;
import java.util.logging.Logger;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.model.PutObjectRequest;

public class MavenPublisher {

  private static final Logger LOG = Logger.getLogger(MavenPublisher.class.getName());
  private static final ExecutorService EXECUTOR = Executors.newFixedThreadPool(1);
  private static final String[] SUPPORTED_SCHEMES = {
    "file:/", "https://", "gs://", "s3://", "artifactregistry://"
  };

  public static void main(String[] args)
      throws IOException, InterruptedException, ExecutionException, TimeoutException {
    String repo = System.getenv("MAVEN_REPO");
    if (!isSchemeSupported(repo)) {
      throw new IllegalArgumentException(
          "Repository must be accessed via the supported schemes: "
              + Arrays.toString(SUPPORTED_SCHEMES));
    }

    boolean gpgSign = Boolean.parseBoolean(System.getenv("GPG_SIGN"));
    Credentials credentials =
        new BasicAuthCredentials(System.getenv("MAVEN_USER"), System.getenv("MAVEN_PASSWORD"));
    boolean useInMemoryPgpKeys = Boolean.parseBoolean(System.getenv("USE_IN_MEMORY_PGP_KEYS"));
    String signingKey = System.getenv("PGP_SIGNING_KEY");
    String signingPassword = System.getenv("PGP_SIGNING_PWD");
    MavenSigning.SigningMetadata signingMetadata =
        new MavenSigning.SigningMetadata(gpgSign, useInMemoryPgpKeys, signingKey, signingPassword);

    List<String> parts = Arrays.asList(args[0].split(":"));
    if (parts.size() != 3) {
      throw new IllegalArgumentException(
          "Coordinates must be a triplet: " + Arrays.toString(parts.toArray()));
    }

    Coordinates coords = new Coordinates(parts.get(0), parts.get(1), parts.get(2));

    // Calculate md5 and sha1 for each of the inputs
    Path pom = Paths.get(args[1]);
    Path mainArtifact = getPathIfSet(args[2]);

    try {
      List<CompletableFuture<Void>> futures = new ArrayList<>();
      futures.add(upload(repo, credentials, coords, ".pom", pom, signingMetadata));

      if (mainArtifact != null) {
        String ext =
            com.google.common.io.Files.getFileExtension(mainArtifact.getFileName().toString());
        futures.add(upload(repo, credentials, coords, "." + ext, mainArtifact, signingMetadata));
      }

      if (args.length > 3 && !args[3].isEmpty()) {
        List<String> extraArtifactTuples = Splitter.onPattern(",").splitToList(args[3]);
        for (String artifactTuple : extraArtifactTuples) {
          String[] splits = artifactTuple.split("=");
          String classifier = splits[0];
          Path artifact = Paths.get(splits[1]);
          String ext = com.google.common.io.Files.getFileExtension(splits[1]);
          futures.add(
              upload(
                  repo,
                  credentials,
                  coords,
                  String.format("-%s.%s", classifier, ext),
                  artifact,
                  signingMetadata));
        }
      }

      CompletableFuture<Void> all =
          CompletableFuture.allOf(futures.toArray(new CompletableFuture[0]));
      all.get(30, MINUTES);
    } finally {
      EXECUTOR.shutdown();
    }
  }

  private static Path getPathIfSet(String arg) {
    if (!arg.isEmpty()) {
      return Paths.get(arg);
    }
    return null;
  }

  private static boolean isSchemeSupported(String repo) {
    for (String scheme : SUPPORTED_SCHEMES) {
      if (repo.startsWith(scheme)) {
        return true;
      }
    }
    return false;
  }

  private static CompletableFuture<Void> upload(
      String repo,
      Credentials credentials,
      Coordinates coords,
      String append,
      Path item,
      MavenSigning.SigningMetadata signingMetadata)
      throws IOException, InterruptedException {

    String base =
        String.format(
            "%s/%s/%s/%s/%s-%s",
            repo.replaceAll("/$", ""),
            coords.groupId.replace('.', '/'),
            coords.artifactId,
            coords.version,
            coords.artifactId,
            coords.version);

    byte[] toHash = Files.readAllBytes(item);
    Path md5 = Files.createTempFile(item.getFileName().toString(), ".md5");
    Files.write(md5, toMd5(toHash).getBytes(UTF_8));

    Path sha1 = Files.createTempFile(item.getFileName().toString(), ".sha1");
    Files.write(sha1, toSha1(toHash).getBytes(UTF_8));

    Path sha256 = Files.createTempFile(item.getFileName().toString(), ".sha256");
    Files.write(sha256, toSha256(toHash).getBytes(UTF_8));

    Path sha512 = Files.createTempFile(item.getFileName().toString(), ".sha512");
    Files.write(sha512, toSha512(toHash).getBytes(UTF_8));

    List<CompletableFuture<?>> uploads = new ArrayList<>();
    uploads.add(upload(String.format("%s%s", base, append), credentials, item));
    uploads.add(upload(String.format("%s%s.md5", base, append), credentials, md5));
    uploads.add(upload(String.format("%s%s.sha1", base, append), credentials, sha1));
    uploads.add(upload(String.format("%s%s.sha256", base, append), credentials, sha256));
    uploads.add(upload(String.format("%s%s.sha512", base, append), credentials, sha512));

    MavenSigning.SigningMethod signingMethod = signingMetadata.signingMethod;
    if (signingMethod.equals(MavenSigning.SigningMethod.GPG)) {
      uploads.add(upload(String.format("%s%s.asc", base, append), credentials, gpg_sign(item)));
      uploads.add(upload(String.format("%s%s.md5.asc", base, append), credentials, gpg_sign(md5)));
      uploads.add(
          upload(String.format("%s%s.sha1.asc", base, append), credentials, gpg_sign(sha1)));
      uploads.add(
          upload(String.format("%s%s.sha256.asc", base, append), credentials, gpg_sign(sha256)));
      uploads.add(
          upload(String.format("%s%s.sha512.asc", base, append), credentials, gpg_sign(sha512)));
    } else if (signingMethod.equals(MavenSigning.SigningMethod.PGP)) {
      uploads.add(
          upload(
              String.format("%s%s.asc", base, append),
              credentials,
              in_memory_pgp_sign(
                  item, signingMetadata.signingKey, signingMetadata.signingPassword)));
      uploads.add(
          upload(
              String.format("%s%s.md5.asc", base, append),
              credentials,
              in_memory_pgp_sign(
                  md5, signingMetadata.signingKey, signingMetadata.signingPassword)));
      uploads.add(
          upload(
              String.format("%s%s.sha1.asc", base, append),
              credentials,
              in_memory_pgp_sign(
                  sha1, signingMetadata.signingKey, signingMetadata.signingPassword)));
      uploads.add(
          upload(
              String.format("%s%s.sha256.asc", base, append),
              credentials,
              in_memory_pgp_sign(
                  sha256, signingMetadata.signingKey, signingMetadata.signingPassword)));
      uploads.add(
          upload(
              String.format("%s%s.sha512.asc", base, append),
              credentials,
              in_memory_pgp_sign(
                  sha512, signingMetadata.signingKey, signingMetadata.signingPassword)));
    }

    return CompletableFuture.allOf(uploads.toArray(new CompletableFuture<?>[0]));
  }

  private static String toSha1(byte[] toHash) {
    return toHexS("%040x", "SHA-1", toHash);
  }

  private static String toSha256(byte[] toHash) {
    return toHexS("%064x", "SHA-256", toHash);
  }

  private static String toSha512(byte[] toHash) {
    return toHexS("%0128x", "SHA-512", toHash);
  }

  private static String toMd5(byte[] toHash) {
    return toHexS("%032x", "MD5", toHash);
  }

  private static String toHexS(String fmt, String algorithm, byte[] toHash) {
    try {
      MessageDigest digest = MessageDigest.getInstance(algorithm);
      digest.update(toHash);
      return String.format(fmt, new BigInteger(1, digest.digest()));
    } catch (NoSuchAlgorithmException e) {
      throw new RuntimeException(e);
    }
  }

  private static CompletableFuture<Void> upload(
      String targetUrl, Credentials credentials, Path toUpload) {
    Callable<Void> callable;
    if (targetUrl.startsWith("http://") || targetUrl.startsWith("https://")) {
      callable = httpUpload(targetUrl, credentials, toUpload);
    } else if (targetUrl.startsWith("gs://")) {
      callable = gcsUpload(targetUrl, toUpload);
    } else if (targetUrl.startsWith("s3://")) {
      callable = s3upload(targetUrl, toUpload);
    } else if (targetUrl.startsWith("artifactregistry://")) {
      callable = arUpload(targetUrl, toUpload);
    } else {
      callable = writeFile(targetUrl, toUpload);
    }

    CompletableFuture<Void> toReturn = new CompletableFuture<>();
    EXECUTOR.submit(
        () -> {
          try {
            callable.call();
            toReturn.complete(null);
          } catch (Exception e) {
            toReturn.completeExceptionally(e);
          }
        });
    return toReturn;
  }

  private static Callable<Void> httpUpload(
      String targetUrl, Credentials credentials, Path toUpload) {
    return () -> {
      LOG.info(String.format("Uploading to %s", targetUrl));
      URL url = new URL(targetUrl);

      HttpURLConnection connection = (HttpURLConnection) url.openConnection();
      connection.setRequestMethod("PUT");
      connection.setDoOutput(true);
      if (credentials != null) {
        if (!credentials.hasRequestMetadataOnly()) {
          throw new RuntimeException("Unsupported credentials");
        }
        if (credentials.hasRequestMetadata()) {
          credentials
              .getRequestMetadata()
              .forEach(
                  (k, l) -> {
                    l.forEach(v -> connection.addRequestProperty(k, v));
                  });
        }
      }
      connection.setRequestProperty("Content-Length", "" + Files.size(toUpload));

      try (OutputStream out = connection.getOutputStream()) {
        try (InputStream is = Files.newInputStream(toUpload)) {
          ByteStreams.copy(is, out);
        }

        int code = connection.getResponseCode();

        if (code == HttpURLConnection.HTTP_UNAUTHORIZED) {
          throw new RuntimeException(connection.getHeaderField("WWW-Authenticate"));
        }

        if (code < 200 || code > 299) {
          try (InputStream in = connection.getErrorStream()) {
            String message;
            if (in != null) {
              String body = new String(ByteStreams.toByteArray(in));
              message = String.format("Unable to upload %s (%s) %s", targetUrl, code, body);
            } else {
              message = String.format("Unable to upload %s (%s)", targetUrl, code);
            }

            throw new IOException(message);
          }
        }
      }
      LOG.info(String.format("Upload to %s complete.", targetUrl));
      return null;
    };
  }

  private static Callable<Void> writeFile(String targetUrl, Path toUpload) {
    return () -> {
      LOG.info(String.format("Copying %s to %s", toUpload, targetUrl));
      Path path = Paths.get(new URL(targetUrl).toURI());
      Files.createDirectories(path.getParent());
      Files.deleteIfExists(path);
      Files.copy(toUpload, path);

      return null;
    };
  }

  private static Callable<Void> gcsUpload(String targetUrl, Path toUpload) {
    return () -> {
      Storage storage = StorageOptions.getDefaultInstance().getService();
      URI gsUri = new URI(targetUrl);
      String bucketName = gsUri.getHost();
      String path = gsUri.getPath().substring(1);

      LOG.info(String.format("Copying %s to gs://%s/%s", toUpload, bucketName, path));
      BlobInfo blobInfo = BlobInfo.newBuilder(bucketName, path).build();
      try (WriteChannel writer = storage.writer(blobInfo);
          InputStream is = Files.newInputStream(toUpload)) {
        ByteStreams.copy(is, Channels.newOutputStream(writer));
      }

      return null;
    };
  }

  private static Callable<Void> s3upload(String targetUrl, Path toUpload) {
    return () -> {
      S3Client s3Client = S3Client.create();
      URI s3Uri = new URI(targetUrl);
      String bucketName = s3Uri.getHost();
      String path = s3Uri.getPath().substring(1);

      LOG.info(String.format("Copying %s to s3://%s/%s", toUpload, bucketName, path));
      s3Client.putObject(PutObjectRequest.builder().bucket(bucketName).key(path).build(), toUpload);

      return null;
    };
  }

  private static Callable<Void> arUpload(String targetUrl, Path toUpload) {
    return () -> {
      Credentials cred = GoogleCredentials.getApplicationDefault();
      String url = "https://" + targetUrl.substring(19);
      return httpUpload(url, cred, toUpload).call();
    };
  }

  private static class Coordinates {
    private final String groupId;
    private final String artifactId;
    private final String version;

    public Coordinates(String groupId, String artifactId, String version) {
      this.groupId = groupId;
      this.artifactId = artifactId;
      this.version = version;
    }
  }

  private static class BasicAuthCredentials extends Credentials {
    private final String user;
    private final String password;

    public BasicAuthCredentials(String user, String password) {
      this.user = user == null || user.isEmpty() ? null : user;
      this.password = password == null || password.isEmpty() ? null : password;
    }

    @Override
    public String getAuthenticationType() {
      return "Basic";
    }

    @Override
    public Map<String, List<String>> getRequestMetadata() {
      return Collections.singletonMap(
          "Authorization",
          Collections.singletonList(
              "Basic "
                  + Base64.getEncoder()
                      .encodeToString(String.format("%s:%s", user, password).getBytes(US_ASCII))));
    }

    @Override
    public Map<String, List<String>> getRequestMetadata(URI uri) {
      return getRequestMetadata();
    }

    @Override
    public boolean hasRequestMetadata() {
      return true;
    }

    @Override
    public boolean hasRequestMetadataOnly() {
      return true;
    }

    @Override
    public void refresh() {}
  }
}
