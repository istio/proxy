// Copyright 2020 Google LLC
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

#include "fuzzing/replay/status_util.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace fuzzing {

namespace {

constexpr size_t kMaxErrorStringSize = 128;

// Borrowed from absl/base/internal/strerror.cc
const char* StrErrorAdaptor(int errnum, char* buf, size_t buflen) {
#if defined(_WIN32)
  int rc = strerror_s(buf, buflen, errnum);
  buf[buflen - 1] = '\0';  // guarantee NUL termination
  if (rc == 0 && strncmp(buf, "Unknown error", buflen) == 0) *buf = '\0';
  return buf;
#else
  // The type of `ret` is platform-specific; both of these branches must compile
  // either way but only one will execute on any given platform:
  auto ret = strerror_r(errnum, buf, buflen);
  if (std::is_same<decltype(ret), int>::value) {
    // XSI `strerror_r`; `ret` is `int`:
    if (ret) *buf = '\0';
    return buf;
  } else {
    // GNU `strerror_r`; `ret` is `char *`:
    return reinterpret_cast<const char*>(ret);
  }
#endif
}

// Borrowed from absl/base/internal/strerror.cc
std::string StrErrorInternal(int errnum) {
  char buf[kMaxErrorStringSize];
  const char* str = StrErrorAdaptor(errnum, buf, sizeof buf);
  if (*str == '\0') {
    snprintf(buf, sizeof buf, "Unknown error %d", errnum);
    str = buf;
  }
  return str;
}

}  // namespace

absl::Status ErrnoStatus(absl::string_view message, int errno_value) {
  if (errno_value == 0) {
    return absl::OkStatus();
  } else {
    return absl::UnknownError(
        absl::StrCat(message, " (", StrErrorInternal(errno_value), ")"));
  }
}

}  // namespace fuzzing
