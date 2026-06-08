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

// This header contains primitives for reference counting, roughly equivalent to
// the primitives used to implement `std::shared_ptr`. These primitives should
// not be used directly in most cases, instead `cel::ManagedMemory` should be
// used instead.

#include "common/data.h"

#include "absl/base/nullability.h"
#include "common/internal/reference_count.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::testing::IsNull;

class DataTest final : public Data {
 public:
  DataTest() noexcept : Data() {}

  explicit DataTest(google::protobuf::Arena* absl_nullable arena) noexcept
      : Data(arena) {}
};

class DataReferenceCount final : public common_internal::ReferenceCounted {
 public:
  explicit DataReferenceCount(const Data* data) : data_(data) {}

 private:
  void Finalize() noexcept override { delete data_; }

  const Data* data_;
};

TEST(Data, Arena) {
  google::protobuf::Arena arena;
  DataTest data(&arena);
  EXPECT_EQ(data.GetArena(), &arena);
  EXPECT_THAT(common_internal::GetDataReferenceCount(&data), IsNull());
}

TEST(Data, ReferenceCount) {
  auto* data = new DataTest();
  EXPECT_THAT(data->GetArena(), IsNull());
  auto* refcount = new DataReferenceCount(data);
  common_internal::SetDataReferenceCount(data, refcount);
  EXPECT_EQ(common_internal::GetDataReferenceCount(data), refcount);
  common_internal::StrongUnref(refcount);
}

}  // namespace
}  // namespace cel
