// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PROTOBUF_VERSION_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PROTOBUF_VERSION_H_

#ifdef __has_include
#if __has_include("third_party/protobuf/runtime_version.h")
#include "google/protobuf/runtime_version.h"  // IWYU pragma: keep
#endif
#endif

#ifdef PROTOBUF_OSS_VERSION
#define CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(major, minor, patch) \
  ((major) * 1000000 + (minor) * 1000 + (patch) <= PROTOBUF_OSS_VERSION)
#else
// Older versions of protobuf did not have the macro.
#define CEL_INTERNAL_PROTOBUF_OSS_VERSION_PREREQ(major, minor, patch) 0
#endif

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PROTOBUF_VERSION_H_
