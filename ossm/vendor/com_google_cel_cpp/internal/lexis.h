// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_LEXIS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_LEXIS_H_

#include "absl/strings/string_view.h"

namespace cel::internal {

// Returns true if the given text matches RESERVED per the lexis of the CEL
// specification.
bool LexisIsReserved(absl::string_view text);

// Returns true if the given text matches IDENT per the lexis of the CEL
// specification, fales otherwise.
bool LexisIsIdentifier(absl::string_view text);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_LEXIS_H_
