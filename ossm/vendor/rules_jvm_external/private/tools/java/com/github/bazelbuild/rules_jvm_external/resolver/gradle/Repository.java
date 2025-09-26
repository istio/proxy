// Copyright 2025 The Bazel Authors. All rights reserved.
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

package com.github.bazelbuild.rules_jvm_external.resolver.gradle;

import java.net.URI;
import java.util.Objects;

/** Models a maven repository that is added to a gradle build script */
public class Repository {
  public final URI uri;
  public final boolean requiresAuth;
  public final String usernameProperty;
  public final String passwordProperty;
  private final String password;
  private final String username;

  public Repository(URI uri) {
    this(uri, false, null, null);
  }

  public Repository(URI uri, boolean requiresAuth, String username, String password) {
    this.uri = Objects.requireNonNull(uri);
    this.requiresAuth = requiresAuth;
    String host = URI.create(getUrl()).getHost();
    this.username = username;
    this.password = password;
    this.usernameProperty = host + "UserName";
    this.passwordProperty = host + "Password";
  }

  public String getPassword() {
    return password;
  }

  public String getUsername() {
    return username;
  }

  public String getUrl() {
    return uri.toString();
  }
}
