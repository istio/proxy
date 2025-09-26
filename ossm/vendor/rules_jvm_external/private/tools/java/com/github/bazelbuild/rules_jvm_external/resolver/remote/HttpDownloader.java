// Copyright 2024 The Bazel Authors. All rights reserved.
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

package com.github.bazelbuild.rules_jvm_external.resolver.remote;

import static com.github.bazelbuild.rules_jvm_external.resolver.events.DownloadEvent.Stage.COMPLETE;
import static com.github.bazelbuild.rules_jvm_external.resolver.events.DownloadEvent.Stage.STARTING;
import static java.net.HttpURLConnection.HTTP_NOT_FOUND;
import static java.net.http.HttpClient.Redirect.ALWAYS;

import com.github.bazelbuild.rules_jvm_external.resolver.events.DownloadEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.events.EventListener;
import com.github.bazelbuild.rules_jvm_external.resolver.events.LogEvent;
import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.Authenticator;
import java.net.ConnectException;
import java.net.PasswordAuthentication;
import java.net.ProxySelector;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Duration;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.logging.Logger;

public class HttpDownloader {

  private static final int MAX_RETRY_COUNT = 3;
  private static final Set<Integer> RETRY_RESPONSE_CODES = Set.of(500, 502, 503, 504);
  private static final Set<Integer> UNAUTHENTICATED_RESPONSE_CODES = Set.of(401, 403, 407);
  private static final Logger LOG = Logger.getLogger(HttpDownloader.class.getName());
  private final HttpClient client;
  private final EventListener listener;
  private final Set<String> authenticationFailed = Collections.synchronizedSet(new HashSet<>());

  public HttpDownloader(Netrc netrc, EventListener listener) {
    this.listener = listener;

    HttpClient.Builder builder =
        HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(300))
            .followRedirects(ALWAYS)
            .proxy(ProxySelector.getDefault());
    Authenticator authenticator =
        new Authenticator() {
          @Override
          protected PasswordAuthentication getPasswordAuthentication() {
            String host = getRequestingHost();
            Netrc.Credential credential = netrc.getCredential(host);
            if (credential == null) {
              // By returning `null` here, we will cause the `AuthenticationFilter` to throw
              // an exception, which we can catch and deal with later. If we don't do this,
              // then the JDK HttpClient will attempt to retry the authentication request up
              // to (by default) 3 times, which is not the behaviour we want.
              return null;
            }

            String userName = credential.account();
            if (userName == null || userName.isEmpty()) {
              userName = credential.login();
            }

            return new PasswordAuthentication(userName, credential.password().toCharArray());
          }
        };
    builder = builder.authenticator(authenticator);
    this.client = builder.build();
  }

  public Path get(URI uriToGet) {
    if ("file".equals(uriToGet.getScheme())) {
      Path path = Paths.get(uriToGet);
      if (Files.exists(path)) {
        return path;
      }
      return null;
    }

    HttpRequest request = startPreparingRequest(uriToGet).GET().build();

    try {
      Path path = Files.createTempFile("resolver", "download");

      HttpResponse<Path> response = makeRequest(request, HttpResponse.BodyHandlers.ofFile(path));

      if (!isSuccessful(response)) {
        return null;
      }

      return response.body();
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }

  public boolean head(URI uri) {
    if ("file".equals(uri.getScheme())) {
      Path path = Paths.get(uri);
      return Files.exists(path);
    }

    HttpRequest request =
        startPreparingRequest(uri).method("HEAD", HttpRequest.BodyPublishers.noBody()).build();

    HttpResponse<Void> response = makeRequest(request, HttpResponse.BodyHandlers.discarding());

    return isSuccessful(response);
  }

  private HttpRequest.Builder startPreparingRequest(URI uri) {
    return HttpRequest.newBuilder()
        .uri(uri)
        .header("User-Agent", "rules_jvm_external resolver")
        .timeout(Duration.ofMinutes(10));
  }

  private <X> HttpResponse<X> makeRequest(
      HttpRequest request, HttpResponse.BodyHandler<X> handler) {
    return doRequest(0, request, handler);
  }

  private <X> HttpResponse<X> doRequest(
      int attemptCount, HttpRequest request, HttpResponse.BodyHandler<X> handler) {
    listener.onEvent(new DownloadEvent(STARTING, request.uri().toString()));
    LOG.fine(String.format("Downloading (attempt %d): %s", attemptCount, request.uri()));

    // Slight pause, in case a previous attempt overwhelmed a server. We may be about to do it
    // again,
    // but this might just help a little.
    try {
      Thread.sleep(attemptCount * 500L);
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      throw new RuntimeException(e);
    }

    try {
      HttpResponse<X> response = client.send(request, handler);
      LOG.fine(String.format("%s -> Got response %d%n", request.uri(), response.statusCode()));

      // Do we want to retry the request?
      if (RETRY_RESPONSE_CODES.contains(response.statusCode())) {
        return doRequest(++attemptCount, request, handler);
      }

      if (UNAUTHENTICATED_RESPONSE_CODES.contains(response.statusCode())) {
        logFailedAuthenticationIfRequired(request);
        return new EmptyResponse<>(request, response.statusCode());
      }

      return response;
    } catch (ConnectException e) {
      // Unable to connect to the remote server. Report the URL as not being found
      LOG.fine(String.format("Unable to connect to remote server: %s", request.uri()));
      return new EmptyResponse<>(request, HTTP_NOT_FOUND);
    } catch (IOException e) {
      LOG.fine(String.format("Attempt %d failed for %s", attemptCount, request.uri()));

      // We may have failed because of an authentication error. The `AuthenticationFilter`
      // doesn't make it easy to detect this case (because it will only return a response
      // with the unauthenticated error code if we'd not used _any_ authenticator in our
      // HttpClient, so we have to examine the exception's error message and hope for the
      // best. This is very, very nasty
      if ("No credentials provided".equals(e.getMessage())) {
        logFailedAuthenticationIfRequired(request);
        return new EmptyResponse<>(request, 401);
      }

      // There are many reasons we may have seen an IOException. One is when an HTTP/2 server sends
      // a `GOAWAY` frame.
      // Don't panic. Just have another go.

      if (attemptCount < MAX_RETRY_COUNT) {
        listener.onEvent(new DownloadEvent(COMPLETE, request.uri().toString()));
        return doRequest(++attemptCount, request, handler);
      }

      // But in all other cases, get very upset.
      throw new UncheckedIOException(e);
    } catch (InterruptedException e) {
      LOG.fine(String.format("Attempt %d interrupted for %s", attemptCount, request.uri()));
      Thread.currentThread().interrupt();
      throw new RuntimeException(e);
    } finally {
      LOG.fine(String.format("Downloaded (attempt %d): %s", attemptCount, request.uri()));
      listener.onEvent(new DownloadEvent(COMPLETE, request.uri().toString()));
    }
  }

  private void logFailedAuthenticationIfRequired(HttpRequest request) {
    if (authenticationFailed.add(request.uri().getHost())) {
      listener.onEvent(
          new LogEvent(
              "downloader",
              String.format(
                  "Authentication failed for %s. Results from this host will be missing from the"
                      + " generated lock file.",
                  request.uri().getHost()),
              null));
    }
  }

  private boolean isSuccessful(HttpResponse<?> response) {
    return response.statusCode() > 199 && response.statusCode() < 300;
  }
}
