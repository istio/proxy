// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_EXCEPTIONS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_EXCEPTIONS_H_

#include "absl/base/config.h"  // IWYU pragma: keep

#ifdef ABSL_HAVE_EXCEPTIONS
#define CEL_INTERNAL_TRY try
#define CEL_INTERNAL_CATCH_ANY catch (...)
#define CEL_INTERNAL_RETHROW \
  do {                       \
    throw;                   \
  } while (false)
#else
#define CEL_INTERNAL_TRY if (true)
#define CEL_INTERNAL_CATCH_ANY else if (false)
#define CEL_INTERNAL_RETHROW \
  do {                       \
  } while (false)
#endif

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_EXCEPTIONS_H_
