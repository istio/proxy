// Copyright 2022 Google LLC
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

#include "eval/public/cel_number.h"

#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

absl::optional<CelNumber> GetNumberFromCelValue(const CelValue& value) {
  if (int64_t val; value.GetValue(&val)) {
    return CelNumber(val);
  } else if (uint64_t val; value.GetValue(&val)) {
    return CelNumber(val);
  } else if (double val; value.GetValue(&val)) {
    return CelNumber(val);
  }
  return absl::nullopt;
}
}  // namespace google::api::expr::runtime
