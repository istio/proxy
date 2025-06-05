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

#ifndef BUILD_BAZEL_RULES_SWIFT_TOOLS_COMMON_BAZEL_SUBSTITUTIONS_H_
#define BUILD_BAZEL_RULES_SWIFT_TOOLS_COMMON_BAZEL_SUBSTITUTIONS_H_

#include <functional>
#include <map>
#include <string>

namespace bazel_rules_swift {

// Manages the substitution of special Bazel placeholder strings in command line
// arguments that are used to defer the determination of Apple developer and SDK
// paths until execution time.
class BazelPlaceholderSubstitutions {
 public:
  // Initializes the substitutions by looking them up in the process's
  // environment when they are first requested.
  BazelPlaceholderSubstitutions();

  // Initializes the substitutions with the given fixed strings. Intended to be
  // used for testing.
  BazelPlaceholderSubstitutions(const std::string &developer_dir,
                                const std::string &sdk_root);

  // Applies any necessary substitutions to `arg` and returns true if this
  // caused the string to change.
  bool Apply(std::string &arg);

 private:
  // A resolver for a Bazel placeholder string that retrieves and caches the
  // value the first time it is requested.
  class PlaceholderResolver {
   public:
    explicit PlaceholderResolver(std::function<std::string()> fn)
        : function_(fn), initialized_(false) {}

    // Returns the requested placeholder value, caching it for future
    // retrievals.
    std::string get() {
      if (!initialized_) {
        value_ = function_();
        initialized_ = true;
      }
      return value_;
    }

   private:
    // The function that returns the value of the placeholder, or the empty
    // string if the placeholder should not be replaced.
    std::function<std::string()> function_;

    // Indicates whether the value of the placeholder has been requested yet and
    // and is therefore initialized.
    bool initialized_;

    // The cached value of the placeholder if `initialized_` is true.
    std::string value_;
  };

  // Finds and replaces all instances of `placeholder` with the value provided
  // by `resolver`, in-place on `str`. Returns true if the string was changed.
  bool FindAndReplace(const std::string &placeholder,
                      PlaceholderResolver &resolver, std::string &str);

  // A mapping from Bazel placeholder strings to resolvers that provide their
  // values.
  std::map<std::string, PlaceholderResolver> placeholder_resolvers_;
};

}  // namespace bazel_rules_swift

#endif  // BUILD_BAZEL_RULES_SWIFT_TOOLS_COMMON_BAZEL_SUBSTITUTIONS_H_
