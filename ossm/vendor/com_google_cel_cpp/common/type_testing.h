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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_TESTING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_TESTING_H_

#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/memory_testing.h"
#include "common/type_factory.h"
#include "common/type_introspector.h"
#include "common/type_manager.h"

namespace cel::common_internal {

template <typename... Ts>
class ThreadCompatibleTypeTest : public ThreadCompatibleMemoryTest<Ts...> {
 private:
  using Base = ThreadCompatibleMemoryTest<Ts...>;

 public:
  void SetUp() override {
    Base::SetUp();
    type_manager_ = NewThreadCompatibleTypeManager(
        this->memory_manager(), NewTypeIntrospector(this->memory_manager()));
  }

  void TearDown() override {
    type_manager_.reset();
    Base::TearDown();
  }

  TypeManager& type_manager() const { return **type_manager_; }

  TypeFactory& type_factory() const { return type_manager(); }

 private:
  virtual Shared<TypeIntrospector> NewTypeIntrospector(
      MemoryManagerRef memory_manager) {
    return NewThreadCompatibleTypeIntrospector(memory_manager);
  }

  absl::optional<Shared<TypeManager>> type_manager_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_TESTING_H_
