// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "jwt_verify_lib/check_audience.h"

#include "absl/strings/match.h"

namespace google {
namespace jwt_verify {
namespace {

// HTTP Protocol scheme prefix in JWT aud claim.
constexpr absl::string_view HTTPSchemePrefix("http://");

// HTTPS Protocol scheme prefix in JWT aud claim.
constexpr absl::string_view HTTPSSchemePrefix("https://");

std::string sanitizeAudience(const std::string& aud) {
  if (aud.empty()) {
    return aud;
  }

  size_t beg_pos = 0;
  bool sanitized = false;
  // Point beg to first character after protocol scheme prefix in audience.
  if (absl::StartsWith(aud, HTTPSchemePrefix)) {
    beg_pos = HTTPSchemePrefix.size();
    sanitized = true;
  } else if (absl::StartsWith(aud, HTTPSSchemePrefix)) {
    beg_pos = HTTPSSchemePrefix.size();
    sanitized = true;
  }

  // Point end to trailing slash in aud.
  size_t end_pos = aud.length();
  if (aud[end_pos - 1] == '/') {
    --end_pos;
    sanitized = true;
  }
  if (sanitized) {
    return aud.substr(beg_pos, end_pos - beg_pos);
  }
  return aud;
}

}  // namespace

CheckAudience::CheckAudience(const std::vector<std::string>& config_audiences) {
  for (const auto& aud : config_audiences) {
    config_audiences_.insert(sanitizeAudience(aud));
  }
}

bool CheckAudience::areAudiencesAllowed(
    const std::vector<std::string>& jwt_audiences) const {
  if (config_audiences_.empty()) {
    return true;
  }
  for (const auto& aud : jwt_audiences) {
    if (config_audiences_.find(sanitizeAudience(aud)) !=
        config_audiences_.end()) {
      return true;
    }
  }
  return false;
}

}  // namespace jwt_verify
}  // namespace google
