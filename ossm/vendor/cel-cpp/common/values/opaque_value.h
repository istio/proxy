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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"
// IWYU pragma: friend "common/values/optional_value.h"

// `OpaqueValue` represents values of the `opaque` type. `OpaqueValueView`
// is a non-owning view of `OpaqueValue`. `OpaqueValueInterface` is the abstract
// base class of implementations. `OpaqueValue` and `OpaqueValueView` act as
// smart pointers to `OpaqueValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/custom_value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class OpaqueValueInterface;
class OpaqueValueInterfaceIterator;
class OpaqueValue;
class TypeFactory;
using OpaqueValueContent = CustomValueContent;

struct OpaqueValueDispatcher {
  using GetTypeId =
      NativeTypeId (*)(const OpaqueValueDispatcher* absl_nonnull dispatcher,
                       OpaqueValueContent content);

  using GetArena = google::protobuf::Arena* absl_nullable (*)(
      const OpaqueValueDispatcher* absl_nonnull dispatcher,
      OpaqueValueContent content);

  using GetTypeName = absl::string_view (*)(
      const OpaqueValueDispatcher* absl_nonnull dispatcher,
      OpaqueValueContent content);

  using DebugString =
      std::string (*)(const OpaqueValueDispatcher* absl_nonnull dispatcher,
                      OpaqueValueContent content);

  using GetRuntimeType =
      OpaqueType (*)(const OpaqueValueDispatcher* absl_nonnull dispatcher,
                     OpaqueValueContent content);

  using Equal = absl::Status (*)(
      const OpaqueValueDispatcher* absl_nonnull dispatcher,
      OpaqueValueContent content, const OpaqueValue& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result);

  using Clone = OpaqueValue (*)(
      const OpaqueValueDispatcher* absl_nonnull dispatcher,
      OpaqueValueContent content, google::protobuf::Arena* absl_nonnull arena);

  absl_nonnull GetTypeId get_type_id;

  absl_nonnull GetArena get_arena;

  absl_nonnull GetTypeName get_type_name;

  absl_nonnull DebugString debug_string;

  absl_nonnull GetRuntimeType get_runtime_type;

  absl_nonnull Equal equal;

  absl_nonnull Clone clone;
};

class OpaqueValueInterface {
 public:
  OpaqueValueInterface() = default;
  OpaqueValueInterface(const OpaqueValueInterface&) = delete;
  OpaqueValueInterface(OpaqueValueInterface&&) = delete;

  virtual ~OpaqueValueInterface() = default;

  OpaqueValueInterface& operator=(const OpaqueValueInterface&) = delete;
  OpaqueValueInterface& operator=(OpaqueValueInterface&&) = delete;

 private:
  friend class OpaqueValue;

  virtual std::string DebugString() const = 0;

  virtual absl::string_view GetTypeName() const = 0;

  virtual OpaqueType GetRuntimeType() const = 0;

  virtual absl::Status Equal(
      const OpaqueValue& other,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const = 0;

  virtual OpaqueValue Clone(google::protobuf::Arena* absl_nonnull arena) const = 0;

  virtual NativeTypeId GetNativeTypeId() const = 0;

  struct Content {
    const OpaqueValueInterface* absl_nonnull interface;
    google::protobuf::Arena* absl_nonnull arena;
  };
};

// Creates an opaque value from a manual dispatch table `dispatcher` and
// opaque data `content` whose format is only know to functions in the manual
// dispatch table. The dispatch table should probably be valid for the lifetime
// of the process, but at a minimum must outlive all instances of the resulting
// value.
//
// IMPORTANT: This approach to implementing OpaqueValue should only be
// used when you know exactly what you are doing. When in doubt, just implement
// OpaqueValueInterface.
OpaqueValue UnsafeOpaqueValue(const OpaqueValueDispatcher* absl_nonnull
                              dispatcher ABSL_ATTRIBUTE_LIFETIME_BOUND,
                              OpaqueValueContent content);

class OpaqueValue : private common_internal::OpaqueValueMixin<OpaqueValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kOpaque;

  // Constructs an opaque value from an implementation of
  // `OpaqueValueInterface` `interface` whose lifetime is tied to that of
  // the arena `arena`.
  OpaqueValue(const OpaqueValueInterface* absl_nonnull
              interface ABSL_ATTRIBUTE_LIFETIME_BOUND,
              google::protobuf::Arena* absl_nonnull arena ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    ABSL_DCHECK(interface != nullptr);
    ABSL_DCHECK(arena != nullptr);
    content_ = OpaqueValueContent::From(
        OpaqueValueInterface::Content{.interface = interface, .arena = arena});
  }

  OpaqueValue() = default;
  OpaqueValue(const OpaqueValue&) = default;
  OpaqueValue(OpaqueValue&&) = default;
  OpaqueValue& operator=(const OpaqueValue&) = default;
  OpaqueValue& operator=(OpaqueValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  NativeTypeId GetTypeId() const;

  OpaqueType GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using OpaqueValueMixin::Equal;

  bool IsZeroValue() const { return false; }

  OpaqueValue Clone(google::protobuf::Arena* absl_nonnull arena) const;

  // Returns `true` if this opaque value is an instance of an optional value.
  bool IsOptional() const;

  // Convenience method for use with template metaprogramming. See
  // `IsOptional()`.
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, bool> Is() const {
    return IsOptional();
  }

  // Performs a checked cast from an opaque value to an optional value,
  // returning a non-empty optional with either a value or reference to the
  // optional value. Otherwise an empty optional is returned.
  optional_ref<const OptionalValue> AsOptional() &
      ABSL_ATTRIBUTE_LIFETIME_BOUND;
  optional_ref<const OptionalValue> AsOptional()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  absl::optional<OptionalValue> AsOptional() &&;
  absl::optional<OptionalValue> AsOptional() const&&;

  // Convenience method for use with template metaprogramming. See
  // `AsOptional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>,
                       optional_ref<const OptionalValue>>
      As() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   optional_ref<const OptionalValue>>
  As() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() &&;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>,
                   absl::optional<OptionalValue>>
  As() const&&;

  // Performs an unchecked cast from an opaque value to an optional value. In
  // debug builds a best effort is made to crash. If `IsOptional()` would return
  // false, calling this method is undefined behavior.
  const OptionalValue& GetOptional() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  const OptionalValue& GetOptional() const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  OptionalValue GetOptional() &&;
  OptionalValue GetOptional() const&&;

  // Convenience method for use with template metaprogramming. See
  // `Optional()`.
  template <typename T>
      std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&>
      Get() & ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, const OptionalValue&> Get()
      const& ABSL_ATTRIBUTE_LIFETIME_BOUND;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get() &&;
  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalValue, T>, OptionalValue> Get()
      const&&;

  const OpaqueValueDispatcher* absl_nullable dispatcher() const {
    return dispatcher_;
  }

  OpaqueValueContent content() const {
    ABSL_DCHECK(dispatcher_ != nullptr);
    return content_;
  }

  const OpaqueValueInterface* absl_nullable interface() const {
    if (dispatcher_ == nullptr) {
      return content_.To<OpaqueValueInterface::Content>().interface;
    }
    return nullptr;
  }

  friend void swap(OpaqueValue& lhs, OpaqueValue& rhs) noexcept {
    using std::swap;
    swap(lhs.dispatcher_, rhs.dispatcher_);
    swap(lhs.content_, rhs.content_);
  }

  explicit operator bool() const {
    if (dispatcher_ == nullptr) {
      return content_.To<OpaqueValueInterface::Content>().interface != nullptr;
    }
    return true;
  }

 protected:
  OpaqueValue(const OpaqueValueDispatcher* absl_nonnull dispatcher
                  ABSL_ATTRIBUTE_LIFETIME_BOUND,
              OpaqueValueContent content)
      : dispatcher_(dispatcher), content_(content) {
    ABSL_DCHECK(dispatcher != nullptr);
    ABSL_DCHECK(dispatcher->get_type_id != nullptr);
    ABSL_DCHECK(dispatcher->get_type_name != nullptr);
    ABSL_DCHECK(dispatcher->clone != nullptr);
  }

 private:
  friend class common_internal::ValueMixin<OpaqueValue>;
  friend class common_internal::OpaqueValueMixin<OpaqueValue>;
  friend OpaqueValue UnsafeOpaqueValue(const OpaqueValueDispatcher* absl_nonnull
                                       dispatcher ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                       OpaqueValueContent content);

  const OpaqueValueDispatcher* absl_nullable dispatcher_ = nullptr;
  OpaqueValueContent content_ = OpaqueValueContent::Zero();
};

inline std::ostream& operator<<(std::ostream& out, const OpaqueValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<OpaqueValue> final {
  static NativeTypeId Id(const OpaqueValue& type) { return type.GetTypeId(); }
};

inline OpaqueValue UnsafeOpaqueValue(const OpaqueValueDispatcher* absl_nonnull
                                     dispatcher ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                     OpaqueValueContent content) {
  return OpaqueValue(dispatcher, content);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_
