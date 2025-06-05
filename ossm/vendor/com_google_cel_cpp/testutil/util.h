// Copyright 2018 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_TESTUTIL_UTIL_H_
#define THIRD_PARTY_CEL_CPP_TESTUTIL_UTIL_H_

#include "internal/proto_matchers.h"

namespace google::api::expr::testutil {

// alias for old namespace
// prefer using cel::internal::test::EqualsProto.
using ::cel::internal::test::EqualsProto;

}  // namespace google::api::expr::testutil

#endif  // THIRD_PARTY_CEL_CPP_TESTUTIL_UTIL_H_
