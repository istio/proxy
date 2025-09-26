// Copyright 2021 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <memory>

#include "absl/strings/string_view.h"

namespace cpp2sky {

class Matcher {
 public:
  virtual ~Matcher() = default;

  /**
   * Check whether to match rule.
   */
  virtual bool match(absl::string_view target) = 0;
};

using MatcherPtr = std::unique_ptr<Matcher>;

}  // namespace cpp2sky
