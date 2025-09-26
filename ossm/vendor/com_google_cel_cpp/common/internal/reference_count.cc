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

#include "common/internal/reference_count.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "common/data.h"
#include "internal/new.h"
#include "google/protobuf/message_lite.h"

namespace cel::common_internal {

template class DeletingReferenceCount<google::protobuf::MessageLite>;

namespace {

class ReferenceCountedStdString final : public ReferenceCounted {
 public:
  static std::pair<const ReferenceCount* absl_nonnull, absl::string_view> New(
      std::string&& string) {
    const auto* const refcount =
        new ReferenceCountedStdString(std::move(string));
    const auto* const refcount_string = std::launder(
        reinterpret_cast<const std::string*>(&refcount->string_[0]));
    return std::pair{static_cast<const ReferenceCount* absl_nonnull>(refcount),
                     absl::string_view(*refcount_string)};
  }

  explicit ReferenceCountedStdString(std::string&& string) {
    (::new (static_cast<void*>(&string_[0])) std::string(std::move(string)))
        ->shrink_to_fit();
  }

 private:
  void Finalize() noexcept override {
    std::destroy_at(std::launder(reinterpret_cast<std::string*>(&string_[0])));
  }

  alignas(std::string) char string_[sizeof(std::string)];
};

class ReferenceCountedString final : public ReferenceCounted {
 public:
  static std::pair<const ReferenceCount* absl_nonnull, absl::string_view> New(
      absl::string_view string) {
    const auto* const refcount =
        ::new (internal::New(Overhead() + string.size()))
            ReferenceCountedString(string);
    return std::pair{static_cast<const ReferenceCount* absl_nonnull>(refcount),
                     absl::string_view(refcount->data_, refcount->size_)};
  }

 private:
// ReferenceCountedString is non-standard-layout due to having virtual functions
// from a base class. This causes compilers to warn about the use of offsetof(),
// but it still works here, so silence the warning and proceed.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

  static size_t Overhead() { return offsetof(ReferenceCountedString, data_); }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

  explicit ReferenceCountedString(absl::string_view string)
      : size_(string.size()) {
    std::memcpy(data_, string.data(), size_);
  }

  void Delete() noexcept override {
    void* const that = this;
    const auto size = size_;
    std::destroy_at(this);
    internal::SizedDelete(that, Overhead() + size);
  }

  const size_t size_;
  char data_[];
};

}  // namespace

std::pair<const ReferenceCount* absl_nonnull, absl::string_view>
MakeReferenceCountedString(absl::string_view value) {
  ABSL_DCHECK(!value.empty());
  return ReferenceCountedString::New(value);
}

std::pair<const ReferenceCount* absl_nonnull, absl::string_view>
MakeReferenceCountedString(std::string&& value) {
  ABSL_DCHECK(!value.empty());
  return ReferenceCountedStdString::New(std::move(value));
}

}  // namespace cel::common_internal
