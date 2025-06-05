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

package com.github.bazelbuild.rules_jvm_external.resolver.maven;

import static com.google.common.base.StandardSystemProperty.USER_HOME;
import static org.eclipse.aether.repository.RepositoryPolicy.CHECKSUM_POLICY_WARN;
import static org.eclipse.aether.repository.RepositoryPolicy.UPDATE_POLICY_DAILY;

import com.github.bazelbuild.rules_jvm_external.resolver.netrc.Netrc;
import java.net.URI;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Objects;
import org.apache.maven.settings.Profile;
import org.apache.maven.settings.Proxy;
import org.apache.maven.settings.Repository;
import org.apache.maven.settings.Server;
import org.apache.maven.settings.Settings;
import org.apache.maven.settings.building.DefaultSettingsBuilderFactory;
import org.apache.maven.settings.building.DefaultSettingsBuildingRequest;
import org.apache.maven.settings.building.SettingsBuildingException;
import org.apache.maven.settings.building.SettingsBuildingRequest;
import org.apache.maven.settings.building.SettingsBuildingResult;
import org.apache.maven.settings.crypto.DefaultSettingsDecrypter;
import org.apache.maven.settings.crypto.DefaultSettingsDecryptionRequest;
import org.apache.maven.settings.crypto.SettingsDecrypter;
import org.apache.maven.settings.crypto.SettingsDecryptionResult;
import org.eclipse.aether.repository.Authentication;
import org.eclipse.aether.repository.RemoteRepository;
import org.eclipse.aether.repository.RepositoryPolicy;
import org.eclipse.aether.util.repository.AuthenticationBuilder;
import org.sonatype.plexus.components.cipher.DefaultPlexusCipher;
import org.sonatype.plexus.components.cipher.PlexusCipher;
import org.sonatype.plexus.components.sec.dispatcher.DefaultSecDispatcher;
import org.sonatype.plexus.components.sec.dispatcher.SecDispatcher;

class RemoteRepositoryFactory {
  private final Netrc netrc;
  private final Settings settings;

  public RemoteRepositoryFactory(Netrc netrc) {
    this(netrc, getSettings());
  }

  public RemoteRepositoryFactory(Netrc netrc, Settings settings) {
    this.netrc = Objects.requireNonNull(netrc, "Netrc");
    this.settings = Objects.requireNonNull(settings, "Settings");
  }

  private static Settings getSettings() {
    SettingsBuildingRequest settingsBuildingRequest = new DefaultSettingsBuildingRequest();
    settingsBuildingRequest.setSystemProperties(System.getProperties());

    String userHome = USER_HOME.value();
    if (userHome != null) {
      Path settings = Paths.get(userHome).resolve(".m2").resolve("settings.xml");
      if (Files.exists(settings)) {
        settingsBuildingRequest.setUserSettingsFile(settings.toFile());
      }
    }

    SettingsBuildingResult result;
    try {
      result = new DefaultSettingsBuilderFactory().newInstance().build(settingsBuildingRequest);
      return result.getEffectiveSettings();
    } catch (SettingsBuildingException e) {
      throw new RuntimeException(e);
    }
  }

  public RemoteRepository createFor(URI uri) {
    Objects.requireNonNull(uri, "Repository");

    RemoteRepository.Builder repo =
        new RemoteRepository.Builder(uri.toString(), "default", uri.toString());
    repo.setSnapshotPolicy(new RepositoryPolicy(false, UPDATE_POLICY_DAILY, CHECKSUM_POLICY_WARN));
    repo.setReleasePolicy(new RepositoryPolicy(true, UPDATE_POLICY_DAILY, CHECKSUM_POLICY_WARN));

    amendWithNetrcCredentials(uri, repo);

    // If there's a mapping to the repository in the settings, attempt to grab authentication
    // credentials
    String serverId = getServerId(uri);
    Server server = null;
    if (serverId != null) {
      server = settings.getServer(serverId);
    }
    if (server != null) {
      amendWithDecryptedData(uri, repo, server);
    }

    return repo.build();
  }

  private void amendWithNetrcCredentials(URI serverUri, RemoteRepository.Builder repo) {
    Netrc.Credential credentials = netrc.getCredential(serverUri.getHost());
    if (credentials != null) {
      addAuthentication(repo, credentials);
    } else {
      credentials = netrc.defaultCredential();
      if (credentials != null) {
        addAuthentication(repo, credentials);
      }
    }
  }

  private void amendWithDecryptedData(URI serverUri, RemoteRepository.Builder repo, Server server) {
    PlexusCipher plexusCipher = new DefaultPlexusCipher();
    SecDispatcher dispatcher = new DefaultSecDispatcher(plexusCipher);
    SettingsDecrypter decrypter = new DefaultSettingsDecrypter(dispatcher);

    DefaultSettingsDecryptionRequest request = new DefaultSettingsDecryptionRequest(server);
    SettingsDecryptionResult result = decrypter.decrypt(request);

    Server decryptServer = result.getServer();
    String password = decryptServer.getPassword();
    if (password != null) {
      Authentication authentication =
          new AuthenticationBuilder()
              .addUsername(server.getUsername())
              .addPassword(password)
              .build();
      repo.setAuthentication(authentication);
    }

    Proxy proxy = result.getProxy();
    if (proxy != null) {
      if (proxy.getUsername() != null && proxy.getPassword() != null) {
        Authentication proxyAuth =
            new AuthenticationBuilder()
                .addUsername(proxy.getUsername())
                .addPassword(proxy.getPassword())
                .build();
        repo.setProxy(
            new org.eclipse.aether.repository.Proxy(
                null, proxy.getHost(), proxy.getPort(), proxyAuth));
      } else {
        repo.setProxy(
            new org.eclipse.aether.repository.Proxy(null, proxy.getHost(), proxy.getPort()));
      }
    }
  }

  private void addAuthentication(RemoteRepository.Builder repo, Netrc.Credential credential) {
    Authentication authentication =
        new AuthenticationBuilder()
            .addUsername(credential.login())
            .addPassword(credential.password())
            .build();
    repo.setAuthentication(authentication);
  }

  private String getServerId(URI uri) {
    String expected = uri.toString();

    for (Profile profile : settings.getProfiles()) {
      for (Repository repo : profile.getRepositories()) {
        if (expected.equals(repo.getUrl())) {
          return repo.getId();
        }
      }
    }
    return null;
  }
}
