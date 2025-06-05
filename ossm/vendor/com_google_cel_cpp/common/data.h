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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_DATA_H_
#define THIRD_PARTY_CEL_CPP_COMMON_DATA_H_

#include <cstdint>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "common/internal/metadata.h"
#include "google/protobuf/arena.h"

namespace cel {

class Data;
template <typename T>
struct Ownable;
template <typename T>
struct Borrowable;

namespace common_internal {

class ReferenceCount;

void SetDataReferenceCount(
    absl::Nonnull<const Data*> data,
    absl::Nonnull<const ReferenceCount*> refcount) noexcept;

absl::Nullable<const ReferenceCount*> GetDataReferenceCount(
    absl::Nonnull<const Data*> data) noexcept;

}  // namespace common_internal

// `Data` is one of the base classes of objects that can be managed by
// `MemoryManager`, the other is `google::protobuf::MessageLite`.
class Data {
 public:
  virtual ~Data() = default;

  absl::Nullable<google::protobuf::Arena*> GetArena() const noexcept {
    return (owner_ & kOwnerBits) == kOwnerArenaBit
               ? reinterpret_cast<google::protobuf::Arena*>(owner_ & kOwnerPointerMask)
               : nullptr;
  }

 protected:
  // At this point, the reference count has not been created. So we create it
  // unowned and set the reference count after. In theory we could create the
  // reference count ahead of time and then update it with the data it has to
  // delete, but that is a bit counter intuitive. Doing it this way is also
  // similar to how std::enable_shared_from_this works.
  Data() noexcept : Data(nullptr) {}

  Data(const Data&) = default;
  Data(Data&&) = default;
  Data& operator=(const Data&) = default;
  Data& operator=(Data&&) = default;

  explicit Data(absl::Nullable<google::protobuf::Arena*> arena) noexcept
      : owner_(reinterpret_cast<uintptr_t>(arena) |
               (arena != nullptr ? kOwnerArenaBit : kOwnerNone)) {}

 private:
  static constexpr uintptr_t kOwnerNone = common_internal::kMetadataOwnerNone;
  static constexpr uintptr_t kOwnerReferenceCountBit =
      common_internal::kMetadataOwnerReferenceCountBit;
  static constexpr uintptr_t kOwnerArenaBit =
      common_internal::kMetadataOwnerArenaBit;
  static constexpr uintptr_t kOwnerBits = common_internal::kMetadataOwnerBits;
  static constexpr uintptr_t kOwnerPointerMask =
      common_internal::kMetadataOwnerPointerMask;

  friend void common_internal::SetDataReferenceCount(
      absl::Nonnull<const Data*> data,
      absl::Nonnull<const common_internal::ReferenceCount*> refcount) noexcept;
  friend absl::Nullable<const common_internal::ReferenceCount*>
  common_internal::GetDataReferenceCount(
      absl::Nonnull<const Data*> data) noexcept;
  template <typename T>
  friend struct Ownable;
  template <typename T>
  friend struct Borrowable;

  mutable uintptr_t owner_ = kOwnerNone;
};

namespace common_internal {

inline void SetDataReferenceCount(
    absl::Nonnull<const Data*> data,
    absl::Nonnull<const ReferenceCount*> refcount) noexcept {
  ABSL_DCHECK_EQ(data->owner_, Data::kOwnerNone);
  data->owner_ =
      reinterpret_cast<uintptr_t>(refcount) | Data::kOwnerReferenceCountBit;
}

inline absl::Nullable<const ReferenceCount*> GetDataReferenceCount(
    absl::Nonnull<const Data*> data) noexcept {
  return (data->owner_ & Data::kOwnerBits) == Data::kOwnerReferenceCountBit
             ? reinterpret_cast<const ReferenceCount*>(data->owner_ &
                                                       Data::kOwnerPointerMask)
             : nullptr;
}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_DATA_H_
