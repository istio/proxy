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

#include "common/internal/data_interface.h"

#include <memory>

#include "common/native_type.h"
#include "internal/testing.h"

namespace cel::common_internal {
namespace {

namespace data_interface_test {

class TestInterface final : public DataInterface {
 private:
  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<TestInterface>();
  }
};

}  // namespace data_interface_test

TEST(DataInterface, GetNativeTypeId) {
  auto data = std::make_unique<data_interface_test::TestInterface>();
  EXPECT_EQ(NativeTypeId::Of(*data),
            NativeTypeId::For<data_interface_test::TestInterface>());
}

}  // namespace
}  // namespace cel::common_internal
