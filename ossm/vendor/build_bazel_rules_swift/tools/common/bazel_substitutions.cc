// Copyright 2021 The Bazel Authors. All rights reserved.
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

#include "tools/common/bazel_substitutions.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include "tools/common/process.h"

namespace bazel_rules_swift {
namespace {

// The placeholder string used by Bazel that should be replaced by
// `DEVELOPER_DIR` at runtime.
static const char kBazelXcodeDeveloperDir[] = "__BAZEL_XCODE_DEVELOPER_DIR__";

// The placeholder string used by Bazel that should be replaced by `SDKROOT`
// at runtime.
static const char kBazelXcodeSdkRoot[] = "__BAZEL_XCODE_SDKROOT__";

// The placeholder string used by the Apple and Swift rules to be replaced with
// the absolute path to the custom toolchain being used
static const char kBazelToolchainPath[] =
    "__BAZEL_CUSTOM_XCODE_TOOLCHAIN_PATH__";

// Returns the value of the given environment variable, or the empty string if
// it wasn't set.
std::string GetAppleEnvironmentVariable(const char *name) {
#if !defined(__APPLE__)
  return "";
#endif

  char *env_value = getenv(name);
  if (env_value == nullptr) {
    std::cerr << "error: required Apple environment variable '" << name << "' was not set. Please file an issue on bazelbuild/rules_swift.\n";
    exit(EXIT_FAILURE);
  }
  return env_value;
}

std::string GetToolchainPath() {
#if !defined(__APPLE__)
  return "";
#endif

  char *toolchain_id = getenv("TOOLCHAINS");
  if (toolchain_id == nullptr) {
    return "";
  }

  std::ostringstream output_stream;
  int exit_code =
      RunSubProcess({"/usr/bin/xcrun", "--find", "clang", "--toolchain", toolchain_id},
                    /*env=*/nullptr, &output_stream, /*stdout_to_stderr=*/true);
  if (exit_code != 0) {
    std::cerr << output_stream.str() << "Error: TOOLCHAINS was set to '"
              << toolchain_id << "' but xcrun failed when searching for that ID"
              << std::endl;
    exit(EXIT_FAILURE);
  }

  if (output_stream.str().empty()) {
    std::cerr << "Error: TOOLCHAINS was set to '" << toolchain_id
              << "' but no toolchain with that ID was found" << std::endl;
    exit(EXIT_FAILURE);
  } else if (output_stream.str().find("XcodeDefault.xctoolchain") !=
             std::string::npos) {
    // NOTE: Ideally xcrun would fail if the toolchain we asked for didn't exist
    // but it falls back to the DEVELOPER_DIR instead, so we have to check the
    // output ourselves.
    std::cerr << "Error: TOOLCHAINS was set to '" << toolchain_id
              << "' but the default toolchain was found, that likely means a "
                 "matching "
              << "toolchain isn't installed" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::filesystem::path toolchain_path(output_stream.str());
  // Remove usr/bin/clang components to get the root of the custom toolchain
  return toolchain_path.parent_path().parent_path().parent_path().string();
}

}  // namespace

BazelPlaceholderSubstitutions::BazelPlaceholderSubstitutions() {
  // When targeting Apple platforms, replace the magic Bazel placeholders with
  // the path in the corresponding environment variable. These should be set by
  // the build rules; only attempt to retrieve them if they're actually seen in
  // the argument list.
  placeholder_resolvers_ = {
      {kBazelXcodeDeveloperDir, PlaceholderResolver([]() {
         return GetAppleEnvironmentVariable("DEVELOPER_DIR");
       })},
      {kBazelXcodeSdkRoot, PlaceholderResolver([]() {
         return GetAppleEnvironmentVariable("SDKROOT");
       })},
      {kBazelToolchainPath,
       PlaceholderResolver([]() { return GetToolchainPath(); })},
  };
}

bool BazelPlaceholderSubstitutions::Apply(std::string &arg) {
  bool changed = false;

  // Replace placeholders in the string with their actual values.
  for (auto &pair : placeholder_resolvers_) {
    changed |= FindAndReplace(pair.first, pair.second, arg);
  }

  return changed;
}

bool BazelPlaceholderSubstitutions::FindAndReplace(
    const std::string &placeholder,
    BazelPlaceholderSubstitutions::PlaceholderResolver &resolver,
    std::string &str) {
  int start = 0;
  bool changed = false;
  while ((start = str.find(placeholder, start)) != std::string::npos) {
    std::string resolved_value = resolver.get();
    if (resolved_value.empty()) {
      return false;
    }
    changed = true;
    str.replace(start, placeholder.length(), resolved_value);
    start += resolved_value.length();
  }
  return changed;
}

}  // namespace bazel_rules_swift
