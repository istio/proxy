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

#include "eval/public/containers/internal_field_backed_list_impl.h"

#include "eval/public/cel_value.h"
#include "eval/public/structs/field_access_impl.h"

namespace google::api::expr::runtime::internal {

int FieldBackedListImpl::size() const {
  return reflection_->FieldSize(*message_, descriptor_);
}

CelValue FieldBackedListImpl::operator[](int index) const {
  auto result = CreateValueFromRepeatedField(message_, descriptor_, index,
                                             factory_, arena_);
  if (!result.ok()) {
    CreateErrorValue(arena_, result.status().ToString());
  }

  return *result;
}

}  // namespace google::api::expr::runtime::internal
