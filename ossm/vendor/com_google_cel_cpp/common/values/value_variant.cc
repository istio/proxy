// Copyright 2025 Google LLC
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

#include "common/values/value_variant.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "common/values/bytes_value.h"
#include "common/values/error_value.h"
#include "common/values/string_value.h"
#include "common/values/unknown_value.h"
#include "common/values/values.h"

namespace cel::common_internal {

void ValueVariant::SlowCopyConstruct(const ValueVariant& other) noexcept {
  ABSL_DCHECK((flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNonTrivial);

  switch (index_) {
    case ValueIndex::kBytes:
      ::new (static_cast<void*>(&raw_[0])) BytesValue(*other.At<BytesValue>());
      break;
    case ValueIndex::kString:
      ::new (static_cast<void*>(&raw_[0]))
          StringValue(*other.At<StringValue>());
      break;
    case ValueIndex::kError:
      ::new (static_cast<void*>(&raw_[0])) ErrorValue(*other.At<ErrorValue>());
      break;
    case ValueIndex::kUnknown:
      ::new (static_cast<void*>(&raw_[0]))
          UnknownValue(*other.At<UnknownValue>());
      break;
    default:
      ABSL_UNREACHABLE();
  }
}

void ValueVariant::SlowMoveConstruct(ValueVariant& other) noexcept {
  ABSL_DCHECK((flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNonTrivial);

  switch (index_) {
    case ValueIndex::kBytes:
      ::new (static_cast<void*>(&raw_[0]))
          BytesValue(std::move(*other.At<BytesValue>()));
      break;
    case ValueIndex::kString:
      ::new (static_cast<void*>(&raw_[0]))
          StringValue(std::move(*other.At<StringValue>()));
      break;
    case ValueIndex::kError:
      ::new (static_cast<void*>(&raw_[0]))
          ErrorValue(std::move(*other.At<ErrorValue>()));
      break;
    case ValueIndex::kUnknown:
      ::new (static_cast<void*>(&raw_[0]))
          UnknownValue(std::move(*other.At<UnknownValue>()));
      break;
    default:
      ABSL_UNREACHABLE();
  }
}

void ValueVariant::SlowDestruct() noexcept {
  ABSL_DCHECK((flags_ & ValueFlags::kNonTrivial) == ValueFlags::kNonTrivial);

  switch (index_) {
    case ValueIndex::kBytes:
      At<BytesValue>()->~BytesValue();
      break;
    case ValueIndex::kString:
      At<StringValue>()->~StringValue();
      break;
    case ValueIndex::kError:
      At<ErrorValue>()->~ErrorValue();
      break;
    case ValueIndex::kUnknown:
      At<UnknownValue>()->~UnknownValue();
      break;
    default:
      ABSL_UNREACHABLE();
  }
}

void ValueVariant::SlowCopyAssign(const ValueVariant& other, bool trivial,
                                  bool other_trivial) noexcept {
  ABSL_DCHECK(!trivial || !other_trivial);

  if (trivial) {
    switch (other.index_) {
      case ValueIndex::kBytes:
        ::new (static_cast<void*>(&raw_[0]))
            BytesValue(*other.At<BytesValue>());
        break;
      case ValueIndex::kString:
        ::new (static_cast<void*>(&raw_[0]))
            StringValue(*other.At<StringValue>());
        break;
      case ValueIndex::kError:
        ::new (static_cast<void*>(&raw_[0]))
            ErrorValue(*other.At<ErrorValue>());
        break;
      case ValueIndex::kUnknown:
        ::new (static_cast<void*>(&raw_[0]))
            UnknownValue(*other.At<UnknownValue>());
        break;
      default:
        ABSL_UNREACHABLE();
    }
    index_ = other.index_;
    kind_ = other.kind_;
    flags_ = other.flags_;
  } else if (other_trivial) {
    switch (index_) {
      case ValueIndex::kBytes:
        At<BytesValue>()->~BytesValue();
        break;
      case ValueIndex::kString:
        At<StringValue>()->~StringValue();
        break;
      case ValueIndex::kError:
        At<ErrorValue>()->~ErrorValue();
        break;
      case ValueIndex::kUnknown:
        At<UnknownValue>()->~UnknownValue();
        break;
      default:
        ABSL_UNREACHABLE();
    }
    FastCopyAssign(other);
  } else {
    switch (index_) {
      case ValueIndex::kBytes:
        switch (other.index_) {
          case ValueIndex::kBytes:
            *At<BytesValue>() = *other.At<BytesValue>();
            break;
          case ValueIndex::kString:
            At<BytesValue>()->~BytesValue();
            ::new (static_cast<void*>(&raw_[0]))
                StringValue(*other.At<StringValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kError:
            At<BytesValue>()->~BytesValue();
            ::new (static_cast<void*>(&raw_[0]))
                ErrorValue(*other.At<ErrorValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kUnknown:
            At<BytesValue>()->~BytesValue();
            ::new (static_cast<void*>(&raw_[0]))
                UnknownValue(*other.At<UnknownValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          default:
            ABSL_UNREACHABLE();
        }
        break;
      case ValueIndex::kString:
        switch (other.index_) {
          case ValueIndex::kBytes:
            At<StringValue>()->~StringValue();
            ::new (static_cast<void*>(&raw_[0]))
                BytesValue(*other.At<BytesValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kString:
            *At<StringValue>() = *other.At<StringValue>();
            break;
          case ValueIndex::kError:
            At<StringValue>()->~StringValue();
            ::new (static_cast<void*>(&raw_[0]))
                ErrorValue(*other.At<ErrorValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kUnknown:
            At<StringValue>()->~StringValue();
            ::new (static_cast<void*>(&raw_[0]))
                UnknownValue(*other.At<UnknownValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          default:
            ABSL_UNREACHABLE();
        }
        break;
      case ValueIndex::kError:
        switch (other.index_) {
          case ValueIndex::kBytes:
            At<ErrorValue>()->~ErrorValue();
            ::new (static_cast<void*>(&raw_[0]))
                BytesValue(*other.At<BytesValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kString:
            At<ErrorValue>()->~ErrorValue();
            ::new (static_cast<void*>(&raw_[0]))
                StringValue(*other.At<StringValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kError:
            *At<ErrorValue>() = *other.At<ErrorValue>();
            break;
          case ValueIndex::kUnknown:
            At<ErrorValue>()->~ErrorValue();
            ::new (static_cast<void*>(&raw_[0]))
                UnknownValue(*other.At<UnknownValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          default:
            ABSL_UNREACHABLE();
        }
        break;
      case ValueIndex::kUnknown:
        switch (other.index_) {
          case ValueIndex::kBytes:
            At<UnknownValue>()->~UnknownValue();
            ::new (static_cast<void*>(&raw_[0]))
                BytesValue(*other.At<BytesValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kString:
            At<UnknownValue>()->~UnknownValue();
            ::new (static_cast<void*>(&raw_[0]))
                StringValue(*other.At<StringValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kError:
            At<UnknownValue>()->~UnknownValue();
            ::new (static_cast<void*>(&raw_[0]))
                ErrorValue(*other.At<ErrorValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kUnknown:
            At<UnknownValue>()->~UnknownValue();
            ::new (static_cast<void*>(&raw_[0]))
                UnknownValue(*other.At<UnknownValue>());
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          default:
            ABSL_UNREACHABLE();
        }
        break;
      default:
        ABSL_UNREACHABLE();
    }
    flags_ = other.flags_;
  }
}

void ValueVariant::SlowMoveAssign(ValueVariant& other, bool trivial,
                                  bool other_trivial) noexcept {
  ABSL_DCHECK(!trivial || !other_trivial);

  if (trivial) {
    switch (other.index_) {
      case ValueIndex::kBytes:
        ::new (static_cast<void*>(&raw_[0]))
            BytesValue(std::move(*other.At<BytesValue>()));
        break;
      case ValueIndex::kString:
        ::new (static_cast<void*>(&raw_[0]))
            StringValue(std::move(*other.At<StringValue>()));
        break;
      case ValueIndex::kError:
        ::new (static_cast<void*>(&raw_[0]))
            ErrorValue(std::move(*other.At<ErrorValue>()));
        break;
      case ValueIndex::kUnknown:
        ::new (static_cast<void*>(&raw_[0]))
            UnknownValue(std::move(*other.At<UnknownValue>()));
        break;
      default:
        ABSL_UNREACHABLE();
    }
    index_ = other.index_;
    kind_ = other.kind_;
    flags_ = other.flags_;
  } else if (other_trivial) {
    switch (index_) {
      case ValueIndex::kBytes:
        At<BytesValue>()->~BytesValue();
        break;
      case ValueIndex::kString:
        At<StringValue>()->~StringValue();
        break;
      case ValueIndex::kError:
        At<ErrorValue>()->~ErrorValue();
        break;
      case ValueIndex::kUnknown:
        At<UnknownValue>()->~UnknownValue();
        break;
      default:
        ABSL_UNREACHABLE();
    }
    FastMoveAssign(other);
  } else {
    switch (index_) {
      case ValueIndex::kBytes:
        switch (other.index_) {
          case ValueIndex::kBytes:
            *At<BytesValue>() = std::move(*other.At<BytesValue>());
            break;
          case ValueIndex::kString:
            At<BytesValue>()->~BytesValue();
            ::new (static_cast<void*>(&raw_[0]))
                StringValue(std::move(*other.At<StringValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kError:
            At<BytesValue>()->~BytesValue();
            ::new (static_cast<void*>(&raw_[0]))
                ErrorValue(std::move(*other.At<ErrorValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kUnknown:
            At<BytesValue>()->~BytesValue();
            ::new (static_cast<void*>(&raw_[0]))
                UnknownValue(std::move(*other.At<UnknownValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          default:
            ABSL_UNREACHABLE();
        }
        break;
      case ValueIndex::kString:
        switch (other.index_) {
          case ValueIndex::kBytes:
            At<StringValue>()->~StringValue();
            ::new (static_cast<void*>(&raw_[0]))
                BytesValue(std::move(*other.At<BytesValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kString:
            *At<StringValue>() = std::move(*other.At<StringValue>());
            break;
          case ValueIndex::kError:
            At<StringValue>()->~StringValue();
            ::new (static_cast<void*>(&raw_[0]))
                ErrorValue(std::move(*other.At<ErrorValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kUnknown:
            At<StringValue>()->~StringValue();
            ::new (static_cast<void*>(&raw_[0]))
                UnknownValue(std::move(*other.At<UnknownValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          default:
            ABSL_UNREACHABLE();
        }
        break;
      case ValueIndex::kError:
        switch (other.index_) {
          case ValueIndex::kBytes:
            At<ErrorValue>()->~ErrorValue();
            ::new (static_cast<void*>(&raw_[0]))
                BytesValue(std::move(*other.At<BytesValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kString:
            At<ErrorValue>()->~ErrorValue();
            ::new (static_cast<void*>(&raw_[0]))
                StringValue(std::move(*other.At<StringValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kError:
            *At<ErrorValue>() = std::move(*other.At<ErrorValue>());
            break;
          case ValueIndex::kUnknown:
            At<ErrorValue>()->~ErrorValue();
            ::new (static_cast<void*>(&raw_[0]))
                UnknownValue(std::move(*other.At<UnknownValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          default:
            ABSL_UNREACHABLE();
        }
        break;
      case ValueIndex::kUnknown:
        switch (other.index_) {
          case ValueIndex::kBytes:
            At<UnknownValue>()->~UnknownValue();
            ::new (static_cast<void*>(&raw_[0]))
                BytesValue(std::move(*other.At<BytesValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kString:
            At<UnknownValue>()->~UnknownValue();
            ::new (static_cast<void*>(&raw_[0]))
                StringValue(std::move(*other.At<StringValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kError:
            At<UnknownValue>()->~UnknownValue();
            ::new (static_cast<void*>(&raw_[0]))
                ErrorValue(std::move(*other.At<ErrorValue>()));
            index_ = other.index_;
            kind_ = other.kind_;
            break;
          case ValueIndex::kUnknown:
            *At<UnknownValue>() = std::move(*other.At<UnknownValue>());
            break;
          default:
            ABSL_UNREACHABLE();
        }
        break;
      default:
        ABSL_UNREACHABLE();
    }
    flags_ = other.flags_;
  }
}

void ValueVariant::SlowSwap(ValueVariant& lhs, ValueVariant& rhs,
                            bool lhs_trivial, bool rhs_trivial) noexcept {
  using std::swap;
  ABSL_DCHECK(!lhs_trivial || !rhs_trivial);

  if (lhs_trivial) {
    alignas(ValueVariant) std::byte tmp[sizeof(ValueVariant)];
    // This is acceptable. We know that both are trivially copyable at runtime.
    // NOLINTNEXTLINE(bugprone-undefined-memory-manipulation)
    std::memcpy(tmp, std::addressof(lhs), sizeof(ValueVariant));
    switch (rhs.index_) {
      case ValueIndex::kBytes:
        ::new (static_cast<void*>(&lhs.raw_[0]))
            BytesValue(*rhs.At<BytesValue>());
        rhs.At<BytesValue>()->~BytesValue();
        break;
      case ValueIndex::kString:
        ::new (static_cast<void*>(&lhs.raw_[0]))
            StringValue(*rhs.At<StringValue>());
        rhs.At<StringValue>()->~StringValue();
        break;
      case ValueIndex::kError:
        ::new (static_cast<void*>(&lhs.raw_[0]))
            ErrorValue(*rhs.At<ErrorValue>());
        rhs.At<ErrorValue>()->~ErrorValue();
        break;
      case ValueIndex::kUnknown:
        ::new (static_cast<void*>(&lhs.raw_[0]))
            UnknownValue(*rhs.At<UnknownValue>());
        rhs.At<UnknownValue>()->~UnknownValue();
        break;
      default:
        ABSL_UNREACHABLE();
    }
    lhs.index_ = rhs.index_;
    lhs.kind_ = rhs.kind_;
    lhs.flags_ = rhs.flags_;
    // This is acceptable. We know that both are trivially copyable at runtime.
    // NOLINTNEXTLINE(bugprone-undefined-memory-manipulation)
    std::memcpy(std::addressof(rhs), tmp, sizeof(ValueVariant));
  } else if (rhs_trivial) {
    alignas(ValueVariant) std::byte tmp[sizeof(ValueVariant)];
    // This is acceptable. We know that both are trivially copyable at runtime.
    // NOLINTNEXTLINE(bugprone-undefined-memory-manipulation)
    std::memcpy(tmp, std::addressof(rhs), sizeof(ValueVariant));
    switch (lhs.index_) {
      case ValueIndex::kBytes:
        ::new (static_cast<void*>(&rhs.raw_[0]))
            BytesValue(*lhs.At<BytesValue>());
        lhs.At<BytesValue>()->~BytesValue();
        break;
      case ValueIndex::kString:
        ::new (static_cast<void*>(&rhs.raw_[0]))
            StringValue(*lhs.At<StringValue>());
        lhs.At<StringValue>()->~StringValue();
        break;
      case ValueIndex::kError:
        ::new (static_cast<void*>(&rhs.raw_[0]))
            ErrorValue(*lhs.At<ErrorValue>());
        lhs.At<ErrorValue>()->~ErrorValue();
        break;
      case ValueIndex::kUnknown:
        ::new (static_cast<void*>(&rhs.raw_[0]))
            UnknownValue(*lhs.At<UnknownValue>());
        lhs.At<UnknownValue>()->~UnknownValue();
        break;
      default:
        ABSL_UNREACHABLE();
    }
    rhs.index_ = lhs.index_;
    rhs.kind_ = lhs.kind_;
    rhs.flags_ = lhs.flags_;
    // This is acceptable. We know that both are trivially copyable at runtime.
    // NOLINTNEXTLINE(bugprone-undefined-memory-manipulation)
    std::memcpy(std::addressof(lhs), tmp, sizeof(ValueVariant));
  } else {
    ValueVariant tmp = std::move(lhs);
    lhs = std::move(rhs);
    rhs = std::move(tmp);
  }
}

}  // namespace cel::common_internal
