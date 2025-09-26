// Copyright 2020 The Bazel Authors. All rights reserved.
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
package com.github.bazelbuild.rules_jvm_external.resolver.netrc;

// Originally com.google.devtools.build.lib.authandtls.Netrc

import com.google.common.base.MoreObjects;
import com.google.common.collect.ImmutableMap;
import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Map;

/** Container for the content of a .netrc file. */
public class Netrc {
  private final Credential defaultCredential;
  private final ImmutableMap<String, Credential> credentials;

  public Netrc(Credential defaultCredential, Map<String, Credential> credentials) {
    this.defaultCredential = defaultCredential;
    this.credentials = ImmutableMap.copyOf(credentials);
  }

  public static Netrc fromUserHome() {
    Path netrcPath = Paths.get(System.getProperty("user.home")).resolve(".netrc");
    if (Files.exists(netrcPath)) {
      try (InputStream is = Files.newInputStream(netrcPath);
          BufferedInputStream bis = new BufferedInputStream(is)) {
        return Netrc.fromStream(bis);
      } catch (IOException e) {
        // Fall through
      }
    }
    return new Netrc(null, ImmutableMap.of());
  }

  public static Netrc fromStream(InputStream inputStream) throws IOException {
    return NetrcParser.parseAndClose(inputStream);
  }

  public static Netrc create(Credential defaultCredential, Map<String, Credential> credentials) {
    return new Netrc(defaultCredential, credentials);
  }

  /**
   * Return a {@link Credential} for a given machine. If machine is not found and there isn't
   * default credential, return {@code null}.
   */
  public Credential getCredential(String machine) {
    return credentials().getOrDefault(machine, defaultCredential());
  }

  public Credential defaultCredential() {
    return defaultCredential;
  }

  public ImmutableMap<String, Credential> credentials() {
    return credentials;
  }

  /** Container for login, password and account of a machine in .netrc */
  public static class Credential {

    private final String machine;
    private final String login;
    private final String password;
    private final String account;

    public Credential(String machine, String login, String password, String account) {
      this.machine = machine;
      this.login = login;
      this.password = password;
      this.account = account;
    }

    public String machine() {
      return machine;
    }

    public String login() {
      return login;
    }

    public String password() {
      return password;
    }

    public String account() {
      return account;
    }

    /**
     * The generated toString method will leak the password. Override and replace the value of
     * password with constant string {@code <password>}.
     */
    @Override
    public final String toString() {
      return MoreObjects.toStringHelper(this)
          .add("machine", machine())
          .add("login", login())
          .add("password", "<password>")
          .add("account", account())
          .toString();
    }

    /** Create a {@link Builder} object for a given machine. */
    public static Builder builder(String machine) {
      return new Builder().setMachine(machine).setLogin("").setPassword("").setAccount("");
    }

    /** {@link Credential}Builder */
    public static class Builder {
      private String machine;
      private String login;
      private String password;
      private String account;

      public String machine() {
        return machine;
      }

      public Builder setMachine(String machine) {
        this.machine = machine;
        return this;
      }

      public String login() {
        return login;
      }

      public Builder setLogin(String login) {
        this.login = login;
        return this;
      }

      public String password() {
        return password;
      }

      public Builder setPassword(String password) {
        this.password = password;
        return this;
      }

      public String account() {
        return account;
      }

      public Builder setAccount(String account) {
        this.account = account;
        return this;
      }

      public Credential build() {
        return new Credential(machine, login, password, account);
      }
    }
  }
}
