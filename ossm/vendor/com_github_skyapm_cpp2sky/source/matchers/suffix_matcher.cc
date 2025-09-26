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

#include "suffix_matcher.h"

#include "absl/strings/match.h"

namespace cpp2sky {

bool SuffixMatcher::match(absl::string_view target) {
  for (const auto& ignore_suffix : target_suffixes_) {
    if (absl::EndsWith(target, ignore_suffix)) {
      return true;
    }
  }

  return false;
}

}  // namespace cpp2sky
