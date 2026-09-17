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
import static com.google.common.io.Files.getFileExtension;
import static java.nio.charset.StandardCharsets.US_ASCII;
import static java.nio.charset.StandardCharsets.UTF_8;
import static java.util.concurrent.TimeUnit.MINUTES;

import com.github.bazelbuild.rules_jvm_external.ByteStreams;
import com.github.bazelbuild.rules_jvm_external.maven.MavenSigning.SigningMetadata;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import com.github.bazelbuild.rules_jvm_external.resolver.remote.HttpDownloader;
import com.google.auth.Credentials;
import com.google.auth.oauth2.GoogleCredentials;
import com.google.cloud.WriteChannel;
import com.google.cloud.storage.BlobInfo;
import com.google.cloud.storage.Storage;
import com.google.cloud.storage.StorageOptions;
import com.google.common.base.Splitter;
import com.google.common.base.Strings;
import com.google.common.io.CharStreams;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.StringReader;
import java.io.UncheckedIOException;
import java.math.BigInteger;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URL;
import java.nio.channels.Channels;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.Callable;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.logging.Logger;
import java.util.stream.Collectors;
import javax.annotation.Nullable;
import org.apache.maven.artifact.repository.metadata.Metadata;
import org.apache.maven.artifact.repository.metadata.Versioning;
import org.apache.maven.artifact.repository.metadata.io.xpp3.MetadataXpp3Reader;
import org.apache.maven.artifact.repository.metadata.io.xpp3.MetadataXpp3Writer;
import software.amazon.awssdk.core.ResponseInputStream;
import software.amazon.awssdk.services.s3.S3Client;
import software.amazon.awssdk.services.s3.model.GetObjectRequest;
import software.amazon.awssdk.services.s3.model.GetObjectResponse;
import software.amazon.awssdk.services.s3.model.PutObjectRequest;
import software.amazon.awssdk.services.s3.model.S3Exception;

public class MavenPublisher {

  private static final Logger LOG = Logger.getLogger(MavenPublisher.class.getName());

  private static final String[] SUPPORTED_SCHEMES = {
    "file:/", "http://", "https://", "gs://", "s3://", "artifactregistry://",
  };
  private static final String[] SUPPORTED_UPLOAD_SCHEMES = {
    "file:/", "http://", "https://", "s3://"
  };

  public static void main(String[] args) throws Exception {

    if (args.length < 4) {
      throw new IllegalArgumentException(
          "Expected at least 4 arguments: <coordinates> <path to pom> <path to main artifact>"
              + " <publish maven metadata> [<extra artifacts>]");
    }

    final String repo = System.getenv("MAVEN_REPO");
    if (Strings.isNullOrEmpty(repo)) {
      throw new IllegalArgumentException("MAVEN_REPO environment variable must be set");
    }

    if (!"true".equalsIgnoreCase(args[3]) && !"false".equalsIgnoreCase(args[3])) {
      throw new IllegalArgumentException(
          String.format(
              "Fourth argument <publish maven metadata> should be true or false. Found: %s.",
              args[3]));
    }
    boolean publishMavenMetadata = Boolean.parseBoolean(args[3]);

    boolean gpgSign = Boolean.parseBoolean(System.getenv("GPG_SIGN"));
    Credentials credentials =
        new BasicAuthCredentials(System.getenv("MAVEN_USER"), System.getenv("MAVEN_PASSWORD"));
    boolean useInMemoryPgpKeys = Boolean.parseBoolean(System.getenv("USE_IN_MEMORY_PGP_KEYS"));
    String signingKey = System.getenv("PGP_SIGNING_KEY");
    String signingPassword = System.getenv("PGP_SIGNING_PWD");
    SigningMetadata signingMetadata =
        new SigningMetadata(gpgSign, useInMemoryPgpKeys, signingKey, signingPassword);

    final ExecutorService executorService =
        Executors.newFixedThreadPool(
            Optional.ofNullable(System.getenv("RJE_MAX_THREADS")).map(Integer::parseInt).orElse(8));

    try {
      run(
          args[0],
          args[1],
          args[2],
          publishMavenMetadata,
          args.length > 4 ? args[4] : null,
          repo,
          credentials,
          signingMetadata,
          executorService);
    } finally {
      executorService.shutdown();
    }
  }

  protected static void run(
      String coordinates,
      String pomPath,
      String mainArtifactPath,
      boolean publishMavenMetadata,
      @Nullable String extraArtifacts,
      String repo,
      @Nullable Credentials credentials,
      SigningMetadata signingMetadata,
      Executor executor)
      throws Exception {

    if (!isSchemeSupported(repo)) {
      throw new IllegalArgumentException(
          "Repository must be accessed via the supported schemes: "
              + Arrays.toString(SUPPORTED_SCHEMES));
    }

    if (!isUploadSchemeSupported(repo) && publishMavenMetadata) {
      throw new IllegalArgumentException(
          "publishMavenMetadata enabled. Repository must be uploaded to via the supported schemes: "
              + Arrays.toString(SUPPORTED_UPLOAD_SCHEMES));
    }

    final Coordinates coords = Coordinates.fromString(coordinates);

    // Calculate md5 and sha1 for each of the inputs
    Path pom = Paths.get(pomPath);

    List<CompletableFuture<Void>> futures = new ArrayList<>();
    futures.add(upload(repo, credentials, coords, ".pom", pom, signingMetadata, executor));

    futures.add(
        upload(
            repo,
            credentials,
            coords,
            "." + getFileExtension(mainArtifactPath),
            Paths.get(mainArtifactPath),
            signingMetadata,
            executor));

    if (!Strings.isNullOrEmpty(extraArtifacts)) {
      List<String> extraArtifactTuples = Splitter.onPattern(",").splitToList(extraArtifacts);
      for (String artifactTuple : extraArtifactTuples) {
        String[] splits = artifactTuple.split("=");
        String classifier = splits[0];
        Path artifact = Paths.get(splits[1]);
        String ext = getFileExtension(splits[1]);
        futures.add(
            upload(
                repo,
                credentials,
                coords,
                String.format("-%s.%s", classifier, ext),
                artifact,
                signingMetadata,
                executor));
      }
    }

    CompletableFuture<Void> all =
        CompletableFuture.allOf(futures.toArray(new CompletableFuture[0]));

    // uploading the maven-metadata.xml signals to cut over to the new version, so it must be at
    // the end.
    // publishing the file is opt-in for remote repositories, but always done for local file
    // repositories.
    if (publishMavenMetadata || repo.startsWith("file:/")) {
      all = all.thenCompose(Void -> uploadMavenMetadata(repo, credentials, coords, executor));
    }

    all.get(30, MINUTES);
  }

  private static boolean isSchemeSupported(String repo) {
    for (String scheme : SUPPORTED_SCHEMES) {
      if (repo.startsWith(scheme)) {
        return true;
      }
    }
    return false;
  }

  private static boolean isUploadSchemeSupported(String repo) {
    for (String scheme : SUPPORTED_UPLOAD_SCHEMES) {
      if (repo.startsWith(scheme)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Download the pre-existing maven-metadata.xml file if it exists. If no such file exists, create
   * a default Metadata with the Coordinates provided.
   */
  private static CompletableFuture<Metadata> downloadExistingMavenMetadata(
      String repo, Coordinates coords) {
    String mavenMetadataUrl =
        String.format(
            "%s/%s/%s/maven-metadata.xml",
            repo.replaceAll("/$", ""), coords.groupId.replace('.', '/'), coords.artifactId);

    return download(mavenMetadataUrl)
        .thenApply(
            optionalFileContents -> {
              try {
                if (optionalFileContents.isEmpty()) {
                  // no file so just upload a new one
                  // we must bootstrap
                  Metadata metadata = new Metadata();
                  metadata.setGroupId(coords.groupId);
                  metadata.setArtifactId(coords.artifactId);
                  metadata.setVersioning(new Versioning());
                  return metadata;
                }
                return new MetadataXpp3Reader()
                    .read(new StringReader(optionalFileContents.get()), false);
              } catch (Exception e) {
                throw new RuntimeException(e);
              }
            });
  }

  /**
   * Upload the new maven-metadata.xml with the new version included in the version list & set the
   * latest and release tags in the Metadata XML object. This function will first download the
   * pre-existing metadata-xml and augment. If no maven-metadata.xml exists, a new one will be
   * hydrated.
   */
  private static CompletableFuture<Void> uploadMavenMetadata(
      String repo, Credentials credentials, Coordinates coords, Executor executor) {

    String mavenMetadataUrl =
        String.format(
            "%s/%s/%s/maven-metadata.xml",
            repo.replaceAll("/$", ""), coords.groupId.replace('.', '/'), coords.artifactId);
    return downloadExistingMavenMetadata(repo, coords)
        .thenCompose(
            metadata -> {
              try {

                // There is a chance versioning is null; handle it by creating the empty object.
                Versioning versioning =
                    Optional.ofNullable(metadata.getVersioning()).orElse(new Versioning());
                versioning.setLatest(coords.version);
                versioning.setRelease(coords.version);
                // This may be needed for SNAPSHOT support
                String timestamp = new SimpleDateFormat("yyyyMMddHHmmss").format(new Date());
                versioning.setLastUpdated(timestamp);
                versioning.getVersions().add(coords.version);
                // Let's handle adding multiple versions many times by turning it back to a set
                versioning.setVersions(
                    versioning.getVersions().stream().distinct().collect(Collectors.toList()));
                metadata.setVersioning(versioning);

                Path newMavenMetadataXml = Files.createTempFile("maven-metadata", ".xml");
                ByteArrayOutputStream os = new ByteArrayOutputStream();
                new MetadataXpp3Writer().write(os, metadata);
                Files.write(newMavenMetadataXml, os.toByteArray());
                return upload(mavenMetadataUrl, credentials, newMavenMetadataXml, executor);
              } catch (Exception e) {
                throw new RuntimeException(e);
              }
            });
  }

  private static CompletableFuture<Void> upload(
      String repo,
      Credentials credentials,
      Coordinates coords,
      String append,
      Path item,
      SigningMetadata signingMetadata,
      Executor executor)
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
    uploads.add(upload(String.format("%s%s", base, append), credentials, item, executor));
    uploads.add(upload(String.format("%s%s.md5", base, append), credentials, md5, executor));
    uploads.add(upload(String.format("%s%s.sha1", base, append), credentials, sha1, executor));
    uploads.add(upload(String.format("%s%s.sha256", base, append), credentials, sha256, executor));
    uploads.add(upload(String.format("%s%s.sha512", base, append), credentials, sha512, executor));

    MavenSigning.SigningMethod signingMethod = signingMetadata.signingMethod;
    if (signingMethod.equals(MavenSigning.SigningMethod.GPG)) {
      uploads.add(
          upload(String.format("%s%s.asc", base, append), credentials, gpg_sign(item), executor));
    } else if (signingMethod.equals(MavenSigning.SigningMethod.PGP)) {
      uploads.add(
          upload(
              String.format("%s%s.asc", base, append),
              credentials,
              in_memory_pgp_sign(
                  item, signingMetadata.getSigningKey(), signingMetadata.getSigningPassword()),
              executor));
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

  /**
   * Attempts to download the file at the given targetUrl. Valid protocols are: http(s), file, and
   * s3 at the moment.
   */
  private static CompletableFuture<Optional<String>> download(String targetUrl) {
    if (targetUrl.startsWith("http")) {
      return httpDownload(targetUrl);
    } else if (targetUrl.startsWith("file:/")) {
      return fileDownload(targetUrl);
    } else if (targetUrl.startsWith("s3://")) {
      return s3Download(targetUrl);
    } else {
      throw new IllegalArgumentException("Unsupported protocol for download: " + targetUrl);
    }
  }

  private static CompletableFuture<Optional<String>> s3Download(String targetUrl) {
    return CompletableFuture.supplyAsync(
        () -> {
          try (S3Client s3Client = S3Client.create()) {
            URI s3Uri = URI.create(targetUrl);
            String bucketName = s3Uri.getHost();
            String key = s3Uri.getPath().substring(1);
            GetObjectRequest request =
                GetObjectRequest.builder().bucket(bucketName).key(key).build();
            ResponseInputStream<GetObjectResponse> s3Object = s3Client.getObject(request);
            return Optional.of(
                CharStreams.toString(new InputStreamReader(s3Object, StandardCharsets.UTF_8)));
          } catch (IOException e) {
            throw new UncheckedIOException(e);
          } catch (S3Exception e) {
            if (e.statusCode() == 404) {
              return Optional.empty();
            } else {
              throw new RuntimeException(e);
            }
          }
        });
  }

  private static CompletableFuture<Optional<String>> fileDownload(String targetUrl) {
    return CompletableFuture.supplyAsync(
        () -> {
          try {
            Path path = Paths.get(URI.create(targetUrl));
            if (!Files.exists(path)) {
              return Optional.empty();
            }
            return Optional.of(Files.readString(path, StandardCharsets.UTF_8));
          } catch (IOException e) {
            throw new UncheckedIOException(e);
          }
        });
  }

  private static CompletableFuture<Optional<String>> httpDownload(String targetUrl) {
    return CompletableFuture.supplyAsync(
        () -> {
          try (HttpDownloader downloader = new HttpDownloader(Netrc.fromUserHome())) {
            Path path = downloader.get(URI.create(targetUrl));
            if (path == null || !Files.exists(path)) {
              return Optional.empty();
            }

            return Optional.of(Files.readString(path, StandardCharsets.UTF_8));
          } catch (Exception e) {
            throw new RuntimeException(e);
          }
        });
  }

  private static CompletableFuture<Void> upload(
      String targetUrl, Credentials credentials, Path toUpload, Executor executor) {
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

    return CompletableFuture.supplyAsync(
        () -> {
          try {
            return callable.call();
          } catch (Exception e) {
            throw new RuntimeException(e);
          }
        },
        executor);
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
      Path path = Paths.get(URI.create(targetUrl));
      Files.createDirectories(path.getParent());
      Files.deleteIfExists(path);
      Files.copy(toUpload, path);

      return null;
    };
  }

  private static Callable<Void> gcsUpload(String targetUrl, Path toUpload) {
    return () -> {
      Storage storage = StorageOptions.getDefaultInstance().getService();
      URI gsUri = URI.create(targetUrl);
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
      try (S3Client s3Client = S3Client.create()) {
        URI s3Uri = URI.create(targetUrl);
        String bucketName = s3Uri.getHost();
        String path = s3Uri.getPath().substring(1);

        LOG.info(String.format("Copying %s to s3://%s/%s", toUpload, bucketName, path));
        s3Client.putObject(
            PutObjectRequest.builder().bucket(bucketName).key(path).build(), toUpload);
      }
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

    private static Coordinates fromString(String coordinates) {
      String[] parts = coordinates.split(":");
      if (parts.length != 3) {
        throw new IllegalArgumentException(
            "Coordinates must be a triplet: " + Arrays.toString(parts));
      }
      return new Coordinates(parts[0], parts[1], parts[2]);
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
