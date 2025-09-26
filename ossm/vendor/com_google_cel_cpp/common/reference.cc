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

#include "common/reference.h"

#include "absl/base/no_destructor.h"

namespace cel {

const VariableReference& VariableReference::default_instance() {
  static const absl::NoDestructor<VariableReference> instance;
  return *instance;
}

const FunctionReference& FunctionReference::default_instance() {
  static const absl::NoDestructor<FunctionReference> instance;
  return *instance;
}

}  // namespace cel
