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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_MEMORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_MEMORY_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/numeric/bits.h"
#include "common/allocator.h"
#include "common/arena.h"
#include "common/data.h"
#include "common/internal/metadata.h"
#include "common/internal/reference_count.h"
#include "common/reference_count.h"
#include "internal/exceptions.h"
#include "internal/to_address.h"  // IWYU pragma: keep
#include "google/protobuf/arena.h"

namespace cel {

// Obtain the address of the underlying element from a raw pointer or "fancy"
// pointer.
using internal::to_address;

// MemoryManagement is an enumeration of supported memory management forms
// underlying `cel::MemoryManager`.
enum class MemoryManagement {
  // Region-based (a.k.a. arena). Memory is allocated in fixed size blocks and
  // deallocated all at once upon destruction of the `cel::MemoryManager`.
  kPooling = 1,
  // Reference counting. Memory is allocated with an associated reference
  // counter. When the reference counter hits 0, it is deallocated.
  kReferenceCounting,
};

std::ostream& operator<<(std::ostream& out, MemoryManagement memory_management);

class ABSL_ATTRIBUTE_TRIVIAL_ABI [[nodiscard]] Owner;
class Borrower;
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI [[nodiscard]] Unique;
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI [[nodiscard]] Owned;
template <typename T>
class Borrowed;
template <typename T>
struct Ownable;
template <typename T>
struct Borrowable;

class MemoryManager;
class ReferenceCountingMemoryManager;
class PoolingMemoryManager;

namespace common_internal {
template <typename T>
inline constexpr bool kNotMessageLiteAndNotData =
    std::conjunction_v<std::negation<std::is_base_of<google::protobuf::MessageLite, T>>,
                       std::negation<std::is_base_of<Data, T>>>;
template <typename To, typename From>
inline constexpr bool kIsPointerConvertible = std::is_convertible_v<From*, To*>;
template <typename To, typename From>
inline constexpr bool kNotSameAndIsPointerConvertible =
    std::conjunction_v<std::negation<std::is_same<To, From>>,
                       std::bool_constant<kIsPointerConvertible<To, From>>>;

// Clears the contents of `owner`, and returns the reference count if in use.
const ReferenceCount* absl_nullable OwnerRelease(Owner owner) noexcept;
const ReferenceCount* absl_nullable BorrowerRelease(Borrower borrower) noexcept;
template <typename T>
Owned<const T> WrapEternal(const T* value);

// Pointer tag used by `cel::Unique` to indicate that the destructor needs to be
// registered with the arena, but it has not been done yet. Must be done when
// releasing.
inline constexpr uintptr_t kUniqueArenaUnownedBit = uintptr_t{1} << 0;
inline constexpr uintptr_t kUniqueArenaBits = kUniqueArenaUnownedBit;
inline constexpr uintptr_t kUniqueArenaPointerMask = ~kUniqueArenaBits;
}  // namespace common_internal

template <typename T, typename... Args>
Owned<T> AllocateShared(Allocator<> allocator, Args&&... args);

template <typename T>
Owned<T> WrapShared(T* object, Allocator<> allocator);

// `Owner` represents a reference to some co-owned data, of which this owner is
// one of the co-owners. When using reference counting, `Owner` performs
// increment/decrement where appropriate similar to `std::shared_ptr`.
// `Borrower` is similar to `Owner`, except that it is always trivially
// copyable/destructible. In that sense, `Borrower` is similar to
// `std::reference_wrapper<const Owner>`.
class ABSL_ATTRIBUTE_TRIVIAL_ABI [[nodiscard]] Owner final {
 private:
  static constexpr uintptr_t kNone = common_internal::kMetadataOwnerNone;
  static constexpr uintptr_t kReferenceCountBit =
      common_internal::kMetadataOwnerReferenceCountBit;
  static constexpr uintptr_t kArenaBit =
      common_internal::kMetadataOwnerArenaBit;
  static constexpr uintptr_t kBits = common_internal::kMetadataOwnerBits;
  static constexpr uintptr_t kPointerMask =
      common_internal::kMetadataOwnerPointerMask;

 public:
  static Owner None() noexcept { return Owner(); }

  static Owner Allocator(Allocator<> allocator) noexcept {
    auto* arena = allocator.arena();
    return arena != nullptr ? Arena(arena) : None();
  }

  static Owner Arena(google::protobuf::Arena* absl_nonnull arena
                         ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    ABSL_DCHECK(arena != nullptr);
    return Owner(reinterpret_cast<uintptr_t>(arena) | kArenaBit);
  }

  static Owner Arena(std::nullptr_t) = delete;

  static Owner ReferenceCount(const ReferenceCount* absl_nonnull reference_count
                                  ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    ABSL_DCHECK(reference_count != nullptr);
    common_internal::StrongRef(*reference_count);
    return Owner(reinterpret_cast<uintptr_t>(reference_count) |
                 kReferenceCountBit);
  }

  static Owner ReferenceCount(std::nullptr_t) = delete;

  Owner() = default;

  Owner(const Owner& other) noexcept : Owner(CopyFrom(other.ptr_)) {}

  Owner(Owner&& other) noexcept : Owner(MoveFrom(other.ptr_)) {}

  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owner(const Owned<T>& owned) noexcept;

  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owner(Owned<T>&& owned) noexcept;

  explicit Owner(Borrower borrower) noexcept;

  template <typename T>
  explicit Owner(Borrowed<T> borrowed) noexcept;

  ~Owner() { Destroy(ptr_); }

  Owner& operator=(const Owner& other) noexcept {
    if (ptr_ != other.ptr_) {
      Destroy(ptr_);
      ptr_ = CopyFrom(other.ptr_);
    }
    return *this;
  }

  Owner& operator=(Owner&& other) noexcept {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      Destroy(ptr_);
      ptr_ = MoveFrom(other.ptr_);
    }
    return *this;
  }

  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owner& operator=(const Owned<T>& owned) noexcept;

  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owner& operator=(Owned<T>&& owned) noexcept;

  explicit operator bool() const noexcept { return !IsNone(ptr_); }

  google::protobuf::Arena* absl_nullable arena() const noexcept {
    return (ptr_ & Owner::kBits) == Owner::kArenaBit
               ? reinterpret_cast<google::protobuf::Arena*>(ptr_ & Owner::kPointerMask)
               : nullptr;
  }

  void reset() noexcept {
    Destroy(ptr_);
    ptr_ = 0;
  }

  // Tests whether two owners have ownership over the same data, that is they
  // are co-owners.
  friend bool operator==(const Owner& lhs, const Owner& rhs) noexcept {
    // A reference count and arena can never occupy the same memory address, so
    // we can compare for equality without masking off the bits.
    return lhs.ptr_ == rhs.ptr_;
  }

 private:
  template <typename T>
  friend class Unique;
  friend class Borrower;
  template <typename T, typename... Args>
  friend Owned<T> AllocateShared(cel::Allocator<> allocator, Args&&... args);
  template <typename T>
  friend Owned<T> WrapShared(T* object, cel::Allocator<> allocator);
  template <typename U>
  friend struct Ownable;
  friend const common_internal::ReferenceCount* absl_nullable
  common_internal::OwnerRelease(Owner owner) noexcept;
  friend const common_internal::ReferenceCount* absl_nullable
  common_internal::BorrowerRelease(Borrower borrower) noexcept;
  friend struct ArenaTraits<Owner>;

  constexpr explicit Owner(uintptr_t ptr) noexcept : ptr_(ptr) {}

  static constexpr bool IsNone(uintptr_t ptr) noexcept { return ptr == kNone; }

  static constexpr bool IsArena(uintptr_t ptr) noexcept {
    return (ptr & kArenaBit) != kNone;
  }

  static constexpr bool IsReferenceCount(uintptr_t ptr) noexcept {
    return (ptr & kReferenceCountBit) != kNone;
  }

  ABSL_ATTRIBUTE_RETURNS_NONNULL
  static google::protobuf::Arena* absl_nonnull AsArena(uintptr_t ptr) noexcept {
    ABSL_ASSERT(IsArena(ptr));
    return reinterpret_cast<google::protobuf::Arena*>(ptr & kPointerMask);
  }

  ABSL_ATTRIBUTE_RETURNS_NONNULL
  static const common_internal::ReferenceCount* absl_nonnull AsReferenceCount(
      uintptr_t ptr) noexcept {
    ABSL_ASSERT(IsReferenceCount(ptr));
    return reinterpret_cast<const common_internal::ReferenceCount*>(
        ptr & kPointerMask);
  }

  static uintptr_t CopyFrom(uintptr_t other) noexcept { return Own(other); }

  static uintptr_t MoveFrom(uintptr_t& other) noexcept {
    return std::exchange(other, kNone);
  }

  static void Destroy(uintptr_t ptr) noexcept { Unown(ptr); }

  static uintptr_t Own(uintptr_t ptr) noexcept {
    if (IsReferenceCount(ptr)) {
      const auto* refcount = Owner::AsReferenceCount(ptr);
      ABSL_ASSUME(refcount != nullptr);
      common_internal::StrongRef(refcount);
    }
    return ptr;
  }

  static void Unown(uintptr_t ptr) noexcept {
    if (IsReferenceCount(ptr)) {
      const auto* reference_count = AsReferenceCount(ptr);
      ABSL_ASSUME(reference_count != nullptr);
      common_internal::StrongUnref(reference_count);
    }
  }

  uintptr_t ptr_ = kNone;
};

inline bool operator!=(const Owner& lhs, const Owner& rhs) noexcept {
  return !operator==(lhs, rhs);
}

namespace common_internal {

inline const ReferenceCount* absl_nullable OwnerRelease(Owner owner) noexcept {
  uintptr_t ptr = std::exchange(owner.ptr_, kMetadataOwnerNone);
  if (Owner::IsReferenceCount(ptr)) {
    return Owner::AsReferenceCount(ptr);
  }
  return nullptr;
}

}  // namespace common_internal

template <>
struct ArenaTraits<Owner> {
  static bool trivially_destructible(const Owner& owner) {
    return !Owner::IsReferenceCount(owner.ptr_);
  }
};

// `Borrower` represents a reference to some borrowed data, where the data has
// at least one owner. When using reference counting, `Borrower` does not
// participate in incrementing/decrementing the reference count. Thus `Borrower`
// will not keep the underlying data alive.
class Borrower final {
 public:
  static Borrower None() noexcept { return Borrower(); }

  static Borrower Allocator(Allocator<> allocator) noexcept {
    auto* arena = allocator.arena();
    return arena != nullptr ? Arena(arena) : None();
  }

  static Borrower Arena(google::protobuf::Arena* absl_nonnull arena
                            ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    ABSL_DCHECK(arena != nullptr);
    return Borrower(reinterpret_cast<uintptr_t>(arena) | Owner::kArenaBit);
  }

  static Borrower Arena(std::nullptr_t) = delete;

  static Borrower ReferenceCount(
      const ReferenceCount* absl_nonnull reference_count
          ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    ABSL_DCHECK(reference_count != nullptr);
    return Borrower(reinterpret_cast<uintptr_t>(reference_count) |
                    Owner::kReferenceCountBit);
  }

  static Borrower ReferenceCount(std::nullptr_t) = delete;

  Borrower() = default;
  Borrower(const Borrower&) = default;
  Borrower(Borrower&&) = default;
  Borrower& operator=(const Borrower&) = default;
  Borrower& operator=(Borrower&&) = default;

  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrower(const Owned<T>& owned ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrower(Borrowed<T> borrowed) noexcept;

  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrower(const Owner& owner ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : ptr_(owner.ptr_) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrower& operator=(
      const Owner& owner ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    ptr_ = owner.ptr_;
    return *this;
  }

  Borrower& operator=(Owner&&) = delete;

  template <typename T>
  Borrower& operator=(
      const Owned<T>& owned ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  template <typename T>
  Borrower& operator=(Owned<T>&&) = delete;

  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrower& operator=(Borrowed<T> borrowed) noexcept;

  explicit operator bool() const noexcept { return !Owner::IsNone(ptr_); }

  google::protobuf::Arena* absl_nullable arena() const noexcept {
    return (ptr_ & Owner::kBits) == Owner::kArenaBit
               ? reinterpret_cast<google::protobuf::Arena*>(ptr_ & Owner::kPointerMask)
               : nullptr;
  }

  void reset() noexcept { ptr_ = 0; }

  // Tests whether two borrowers are borrowing the same data.
  friend bool operator==(Borrower lhs, Borrower rhs) noexcept {
    // A reference count and arena can never occupy the same memory address, so
    // we can compare for equality without masking off the bits.
    return lhs.ptr_ == rhs.ptr_;
  }

 private:
  friend class Owner;
  template <typename U>
  friend struct Borrowable;
  friend const common_internal::ReferenceCount* absl_nullable
  common_internal::BorrowerRelease(Borrower borrower) noexcept;

  constexpr explicit Borrower(uintptr_t ptr) noexcept : ptr_(ptr) {}

  uintptr_t ptr_ = Owner::kNone;
};

inline bool operator!=(Borrower lhs, Borrower rhs) noexcept {
  return !operator==(lhs, rhs);
}

inline bool operator==(Borrower lhs, const Owner& rhs) noexcept {
  return operator==(lhs, Borrower(rhs));
}

inline bool operator==(const Owner& lhs, Borrower rhs) noexcept {
  return operator==(Borrower(lhs), rhs);
}

inline bool operator!=(Borrower lhs, const Owner& rhs) noexcept {
  return !operator==(lhs, rhs);
}

inline bool operator!=(const Owner& lhs, Borrower rhs) noexcept {
  return !operator==(lhs, rhs);
}

inline Owner::Owner(Borrower borrower) noexcept
    : ptr_(Owner::Own(borrower.ptr_)) {}

namespace common_internal {

inline const ReferenceCount* absl_nullable BorrowerRelease(
    Borrower borrower) noexcept {
  uintptr_t ptr = borrower.ptr_;
  if (Owner::IsReferenceCount(ptr)) {
    return Owner::AsReferenceCount(ptr);
  }
  return nullptr;
}

}  // namespace common_internal

template <typename T, typename... Args>
Unique<T> AllocateUnique(Allocator<> allocator, Args&&... args);

// Wrap an already created `T` in `Unique`. Requires that `T` is not const,
// otherwise `GetArena()` may return slightly unexpected results depending on if
// it is the default value.
template <typename T>
std::enable_if_t<!std::is_const_v<T>, Unique<T>> WrapUnique(T* object);

template <typename T>
Unique<T> WrapUnique(T* object, Allocator<> allocator);

// `Unique<T>` points to an object which was allocated using `Allocator<>` or
// `Allocator<T>`. It has ownership over the object, and will perform any
// destruction and deallocation required. `Unique` must not outlive the
// underlying arena, if any. Unlike `Owned` and `Borrowed`, `Unique` supports
// arena incompatible objects. It is very similar to `std::unique_ptr` when
// using a custom deleter.
//
// IMPLEMENTATION NOTES:
// When utilizing arenas, we optionally perform a risky optimization via
// `AllocateUnique`. We do not use `Arena::Create`, instead we directly allocate
// the bytes and construct it in place ourselves. This avoids registering the
// destructor when required. Instead we register the destructor ourselves, if
// required, during `Unique::release`. This allows us to avoid deferring
// destruction of the object until the arena is destroyed, avoiding the cost
// involved in doing so.
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI [[nodiscard]] Unique final {
 public:
  using element_type = T;

  static_assert(!std::is_array_v<T>, "T must not be an array");
  static_assert(!std::is_reference_v<T>, "T must not be a reference");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");

  Unique() = default;
  Unique(const Unique&) = delete;
  Unique& operator=(const Unique&) = delete;

  explicit Unique(T* ptr) noexcept
      : Unique(ptr, common_internal::GetArena(ptr)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Unique(std::nullptr_t) noexcept : Unique() {}

  Unique(Unique&& other) noexcept : Unique(other.ptr_, other.arena_) {
    other.ptr_ = nullptr;
  }

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Unique(Unique<U>&& other) noexcept : Unique(other.ptr_, other.arena_) {
    other.ptr_ = nullptr;
  }

  ~Unique() { Delete(); }

  Unique& operator=(Unique&& other) noexcept {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      Delete();
      ptr_ = other.ptr_;
      arena_ = other.arena_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Unique& operator=(U* other) noexcept {
    reset(other);
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Unique& operator=(Unique<U>&& other) noexcept {
    Delete();
    ptr_ = other.ptr_;
    arena_ = other.arena_;
    other.ptr_ = nullptr;
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Unique& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  T& operator*() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(static_cast<bool>(*this));
    return *get();
  }

  T* absl_nonnull operator->() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(static_cast<bool>(*this));
    return get();
  }

  // Relinquishes ownership of `T*`, returning it. If `T` was allocated and
  // constructed using an arena, no further action is required. If `T` was
  // allocated and constructed without an arena, the caller must eventually call
  // `delete`.
  ABSL_MUST_USE_RESULT T* release() noexcept {
    PreRelease();
    return std::exchange(ptr_, nullptr);
  }

  void reset() noexcept { reset(nullptr); }

  void reset(T* ptr) noexcept {
    Delete();
    ptr_ = ptr;
    arena_ = reinterpret_cast<uintptr_t>(common_internal::GetArena(ptr));
  }

  void reset(std::nullptr_t) noexcept {
    Delete();
    ptr_ = nullptr;
    arena_ = 0;
  }

  explicit operator bool() const noexcept { return get() != nullptr; }

  google::protobuf::Arena* absl_nullable arena() const noexcept {
    return reinterpret_cast<google::protobuf::Arena*>(
        arena_ & common_internal::kUniqueArenaPointerMask);
  }

  friend void swap(Unique& lhs, Unique& rhs) noexcept {
    using std::swap;
    swap(lhs.ptr_, rhs.ptr_);
    swap(lhs.arena_, rhs.arena_);
  }

 private:
  template <typename U>
  friend class Unique;
  template <typename U>
  friend class Owned;
  template <typename U, typename... Args>
  friend Unique<U> AllocateUnique(Allocator<> allocator, Args&&... args);
  template <typename U>
  friend Unique<U> WrapUnique(U* object, Allocator<> allocator);
  friend class ReferenceCountingMemoryManager;
  friend class PoolingMemoryManager;
  friend struct std::pointer_traits<Unique<T>>;
  friend struct ArenaTraits<Unique<T>>;

  Unique(T* ptr, uintptr_t arena) noexcept : ptr_(ptr), arena_(arena) {}

  Unique(T* ptr, google::protobuf::Arena* arena, bool unowned = false) noexcept
      : Unique(ptr,
               reinterpret_cast<uintptr_t>(arena) |
                   (unowned ? common_internal::kUniqueArenaUnownedBit : 0)) {
    ABSL_ASSERT(!unowned || (unowned && arena != nullptr));
  }

  Unique(google::protobuf::Arena* arena, T* ptr, bool unowned = false) noexcept
      : Unique(ptr, arena, unowned) {}

  T* get() const noexcept { return ptr_; }

  void Delete() const noexcept {
    if (static_cast<bool>(*this)) {
      if (arena_ != 0) {
        if ((arena_ & common_internal::kUniqueArenaBits) ==
            common_internal::kUniqueArenaUnownedBit) {
          // We never registered the destructor, call it if necessary.
          if constexpr (!std::is_trivially_destructible_v<T> &&
                        !google::protobuf::Arena::is_destructor_skippable<T>::value) {
            std::destroy_at(ptr_);
          }
        }
      } else {
        google::protobuf::Arena::Destroy(ptr_);
      }
    }
  }

  void PreRelease() noexcept {
    if constexpr (!std::is_trivially_destructible_v<T> &&
                  !google::protobuf::Arena::is_destructor_skippable<T>::value) {
      if (static_cast<bool>(*this) &&
          (arena_ & common_internal::kUniqueArenaBits) ==
              common_internal::kUniqueArenaUnownedBit) {
        // We never registered the destructor, call it if necessary.
        arena()->OwnDestructor(const_cast<std::remove_const_t<T>*>(ptr_));
        arena_ &= common_internal::kUniqueArenaPointerMask;
      }
    }
  }

  void Release(T** ptr, Owner* owner) noexcept {
    if (ptr_ == nullptr) {
      *ptr = nullptr;
      return;
    }
    PreRelease();
    *ptr = std::exchange(ptr_, nullptr);
    if (arena_ == 0) {
      owner->ptr_ = reinterpret_cast<uintptr_t>(
                        common_internal::MakeDeletingReferenceCount(*ptr)) |
                    common_internal::kMetadataOwnerReferenceCountBit;
    } else {
      owner->ptr_ = reinterpret_cast<uintptr_t>(arena()) |
                    common_internal::kMetadataOwnerArenaBit;
    }
  }

  T* ptr_ = nullptr;
  // Potentially tagged pointer to `google::protobuf::Arena`. The tag is used to determine
  // whether we still need to register the destructor with the `google::protobuf::Arena`.
  uintptr_t arena_ = 0;
};

template <typename T>
Unique(T*) -> Unique<T>;

template <typename T, typename... Args>
Unique<T> AllocateUnique(Allocator<> allocator, Args&&... args) {
  using U = std::remove_cv_t<T>;
  static_assert(!std::is_reference_v<U>, "T must not be a reference");
  static_assert(!std::is_array_v<U>, "T must not be an array");

  U* object;
  google::protobuf::Arena* absl_nullable arena = allocator.arena();
  bool unowned;
  if constexpr (google::protobuf::Arena::is_arena_constructable<U>::value) {
    object = google::protobuf::Arena::Create<U>(arena, std::forward<Args>(args)...);
    // For arena-compatible proto types, let the Arena::Create handle
    // registering the destructor call.
    // Otherwise, Unique<T> retains a pointer to the owning arena so it may
    // conditionally register T::~T depending on usage.
    unowned = false;
  } else {
    void* p = allocator.allocate_bytes(sizeof(U), alignof(U));
    CEL_INTERNAL_TRY {
      if constexpr (ArenaTraits<>::constructible<U>()) {
        object = ::new (p) U(arena, std::forward<Args>(args)...);
      } else {
        object = ::new (p) U(std::forward<Args>(args)...);
      }
    }
    CEL_INTERNAL_CATCH_ANY {
      allocator.deallocate_bytes(p, sizeof(U), alignof(U));
      CEL_INTERNAL_RETHROW;
    }
    unowned =
        arena != nullptr && !ArenaTraits<>::trivially_destructible(*object);
  }
  return Unique<T>(object, arena, unowned);
}

template <typename T>
std::enable_if_t<!std::is_const_v<T>, Unique<T>> WrapUnique(T* object) {
  return Unique<T>(object);
}

template <typename T>
Unique<T> WrapUnique(T* object, Allocator<> allocator) {
  return Unique<T>(object, allocator.arena());
}

template <typename T>
inline bool operator==(const Unique<T>& lhs, std::nullptr_t) {
  return !static_cast<bool>(lhs);
}

template <typename T>
inline bool operator==(std::nullptr_t, const Unique<T>& rhs) {
  return !static_cast<bool>(rhs);
}

template <typename T>
inline bool operator!=(const Unique<T>& lhs, std::nullptr_t) {
  return static_cast<bool>(lhs);
}

template <typename T>
inline bool operator!=(std::nullptr_t, const Unique<T>& rhs) {
  return static_cast<bool>(rhs);
}

}  // namespace cel

namespace std {

template <typename T>
struct pointer_traits<cel::Unique<T>> {
  using pointer = cel::Unique<T>;
  using element_type = typename cel::Unique<T>::element_type;
  using difference_type = ptrdiff_t;

  template <typename U>
  using rebind = cel::Unique<U>;

  static element_type* to_address(const pointer& p) noexcept { return p.ptr_; }
};

}  // namespace std

namespace cel {

template <typename T>
struct ArenaTraits<Unique<T>> {
  static bool trivially_destructible(const Unique<T>& unique) {
    return unique.arena_ != 0 &&
           (unique.arena_ & common_internal::kUniqueArenaBits) == 0;
  }
};

// `Owned<T>` points to an object which was allocated using `Allocator<>` or
// `Allocator<T>`. It has co-ownership over the object. `T` must meet the named
// requirement `ArenaConstructable`.
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI [[nodiscard]] Owned final {
 public:
  using element_type = T;

  static_assert(!std::is_array_v<T>, "T must not be an array");
  static_assert(!std::is_reference_v<T>, "T must not be a reference");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");
  static_assert(!std::is_void_v<T>, "T must not be void");

  Owned() = default;
  Owned(const Owned&) = default;
  Owned& operator=(const Owned&) = default;

  Owned(Owned&& other) noexcept
      : Owned(std::exchange(other.value_, nullptr), std::move(other.owner_)) {}

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owned(const Owned<U>& other) noexcept : Owned(other.value_, other.owner_) {}

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owned(Owned<U>&& other) noexcept
      : Owned(std::exchange(other.value_, nullptr), std::move(other.owner_)) {}

  template <typename U, typename = std::enable_if_t<
                            common_internal::kIsPointerConvertible<T, U>>>
  explicit Owned(Borrowed<U> other) noexcept;

  template <typename U, typename = std::enable_if_t<
                            common_internal::kIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owned(Unique<U>&& other) : Owned() {
    other.Release(&value_, &owner_);
  }

  Owned(Owner owner, T* value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : Owned(value, std::move(owner)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Owned(std::nullptr_t) noexcept : Owned() {}

  Owned& operator=(Owned&& other) noexcept {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      value_ = std::exchange(other.value_, nullptr);
      owner_ = std::move(other.owner_);
    }
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owned& operator=(const Owned<U>& other) noexcept {
    value_ = other.value_;
    owner_ = other.owner_;
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owned& operator=(Owned<U>&& other) noexcept {
    value_ = std::exchange(other.value_, nullptr);
    owner_ = std::move(other.owner_);
    return *this;
  }

  template <typename U, typename = std::enable_if_t<
                            common_internal::kIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owned& operator=(Borrowed<U> other) noexcept;

  template <typename U, typename = std::enable_if_t<
                            common_internal::kIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Owned& operator=(Unique<U>&& other) {
    owner_.reset();
    other.Release(&value_, &owner_);
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Owned& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  T& operator*() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(static_cast<bool>(*this));
    return *get();
  }

  T* absl_nonnull operator->() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(static_cast<bool>(*this));
    return get();
  }

  void reset() noexcept {
    value_ = nullptr;
    owner_.reset();
  }

  google::protobuf::Arena* absl_nullable arena() const noexcept { return owner_.arena(); }

  explicit operator bool() const noexcept { return get() != nullptr; }

  friend void swap(Owned& lhs, Owned& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
    swap(lhs.owner_, rhs.owner_);
  }

 private:
  friend class Owner;
  friend class Borrower;
  template <typename U>
  friend class Owned;
  template <typename U>
  friend class Borrowed;
  template <typename U>
  friend struct Ownable;
  template <typename U, typename... Args>
  friend Owned<U> AllocateShared(Allocator<> allocator, Args&&... args);
  template <typename U>
  friend Owned<U> WrapShared(U* object, Allocator<> allocator);
  template <typename U>
  friend Owned<const U> common_internal::WrapEternal(const U* value);
  friend struct std::pointer_traits<Owned<T>>;
  friend struct ArenaTraits<Owned<T>>;

  Owned(T* value, Owner owner) noexcept
      : value_(value), owner_(std::move(owner)) {}

  T* get() const noexcept { return value_; }

  T* value_ = nullptr;
  Owner owner_;
};

template <typename T>
Owned(T*) -> Owned<T>;
template <typename T>
Owned(Unique<T>) -> Owned<T>;
template <typename T>
Owned(Owner, T*) -> Owned<T>;
template <typename T>
Owned(Borrowed<T>) -> Owned<T>;

}  // namespace cel

namespace std {

template <typename T>
struct pointer_traits<cel::Owned<T>> {
  using pointer = cel::Owned<T>;
  using element_type = typename cel::Owned<T>::element_type;
  using difference_type = ptrdiff_t;

  template <typename U>
  using rebind = cel::Owned<U>;

  static element_type* to_address(const pointer& p) noexcept {
    return p.value_;
  }
};

}  // namespace std

namespace cel {

template <typename T>
struct ArenaTraits<Owned<T>> {
  static bool trivially_destructible(const Owned<T>& owned) {
    return ArenaTraits<>::trivially_destructible(owned.owner_);
  }
};

template <typename T>
Owner::Owner(const Owned<T>& owned) noexcept : Owner(owned.owner_) {}

template <typename T>
Owner::Owner(Owned<T>&& owned) noexcept : Owner(std::move(owned.owner_)) {
  owned.value_ = nullptr;
}

template <typename T>
Owner& Owner::operator=(const Owned<T>& owned) noexcept {
  *this = owned.owner_;
  return *this;
}

template <typename T>
Owner& Owner::operator=(Owned<T>&& owned) noexcept {
  *this = std::move(owned.owner_);
  owned.value_ = nullptr;
  return *this;
}

template <typename T>
bool operator==(const Owned<T>& lhs, std::nullptr_t) noexcept {
  return !static_cast<bool>(lhs);
}

template <typename T>
bool operator==(std::nullptr_t, const Owned<T>& rhs) noexcept {
  return rhs == nullptr;
}

template <typename T>
bool operator!=(const Owned<T>& lhs, std::nullptr_t) noexcept {
  return !operator==(lhs, nullptr);
}

template <typename T>
bool operator!=(std::nullptr_t, const Owned<T>& rhs) noexcept {
  return !operator==(nullptr, rhs);
}

template <typename T, typename... Args>
Owned<T> AllocateShared(Allocator<> allocator, Args&&... args) {
  using U = std::remove_cv_t<T>;
  static_assert(!std::is_reference_v<U>, "T must not be a reference");
  static_assert(!std::is_array_v<U>, "T must not be an array");

  U* object;
  Owner owner;
  if (google::protobuf::Arena* absl_nullable arena = allocator.arena();
      arena != nullptr) {
    object = ArenaAllocator(arena).template new_object<U>(
        std::forward<Args>(args)...);
    owner.ptr_ = reinterpret_cast<uintptr_t>(arena) |
                 common_internal::kMetadataOwnerArenaBit;
  } else {
    const common_internal::ReferenceCount* refcount;
    std::tie(object, refcount) = common_internal::MakeEmplacedReferenceCount<U>(
        std::forward<Args>(args)...);
    owner.ptr_ = reinterpret_cast<uintptr_t>(refcount) |
                 common_internal::kMetadataOwnerReferenceCountBit;
  }
  return Owned<U>(object, std::move(owner));
}

template <typename T>
Owned<T> WrapShared(T* object, Allocator<> allocator) {
  Owner owner;
  if (object == nullptr) {
  } else if (allocator.arena() != nullptr) {
    owner.ptr_ = reinterpret_cast<uintptr_t>(
                     static_cast<google::protobuf::Arena*>(allocator.arena())) |
                 common_internal::kMetadataOwnerArenaBit;
  } else {
    owner.ptr_ = reinterpret_cast<uintptr_t>(
                     common_internal::MakeDeletingReferenceCount(object)) |
                 common_internal::kMetadataOwnerReferenceCountBit;
  }
  return Owned<T>(object, std::move(owner));
}

template <typename T>
std::enable_if_t<!std::is_const_v<T>, Owned<T>> WrapShared(T* object) {
  return WrapShared(object, object->GetArena());
}

namespace common_internal {

template <typename T>
Owned<const T> WrapEternal(const T* value) {
  return Owned<const T>(value, Owner::None());
}

}  // namespace common_internal

// `Borrowed<T>` points to an object which was allocated using `Allocator<>` or
// `Allocator<T>`. It has no ownership over the object, and is only valid so
// long as one or more owners of the object exist. `T` must meet the named
// requirement `ArenaConstructable`.
template <typename T>
class Borrowed final {
 public:
  using element_type = T;

  static_assert(!std::is_array_v<T>, "T must not be an array");
  static_assert(!std::is_reference_v<T>, "T must not be a reference");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");
  static_assert(!std::is_void_v<T>, "T must not be void");

  Borrowed() = default;
  Borrowed(const Borrowed&) = default;
  Borrowed(Borrowed&&) = default;
  Borrowed& operator=(const Borrowed&) = default;
  Borrowed& operator=(Borrowed&&) = default;

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrowed(const Borrowed<U>& other) noexcept
      : Borrowed(other.value_, other.borrower_) {}

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrowed(Borrowed<U>&& other) noexcept
      : Borrowed(other.value_, other.borrower_) {}

  template <typename U, typename = std::enable_if_t<
                            common_internal::kIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrowed(const Owned<U>& other ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : Borrowed(other.value_, other.owner_) {}

  Borrowed(Borrower borrower, T* ptr) noexcept : Borrowed(ptr, borrower) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrowed(std::nullptr_t) noexcept : Borrowed() {}

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrowed& operator=(const Borrowed<U>& other) noexcept {
    value_ = other.value_;
    borrower_ = other.borrower_;
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<
                common_internal::kNotSameAndIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrowed& operator=(Borrowed<U>&& other) noexcept {
    value_ = other.value_;
    borrower_ = other.borrower_;
    return *this;
  }

  template <typename U, typename = std::enable_if_t<
                            common_internal::kIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrowed& operator=(
      const Owned<U>& other ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    value_ = other.value_;
    borrower_ = other.borrower_;
    return *this;
  }

  template <typename U, typename = std::enable_if_t<
                            common_internal::kIsPointerConvertible<T, U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrowed& operator=(Owned<U>&&) = delete;

  // NOLINTNEXTLINE(google-explicit-constructor)
  Borrowed& operator=(std::nullptr_t) noexcept {
    reset();
    return *this;
  }

  T& operator*() const noexcept {
    ABSL_DCHECK(static_cast<bool>(*this));
    return *get();
  }

  T* absl_nonnull operator->() const noexcept {
    ABSL_DCHECK(static_cast<bool>(*this));
    return get();
  }

  void reset() noexcept {
    value_ = nullptr;
    borrower_.reset();
  }

  google::protobuf::Arena* absl_nullable arena() const noexcept {
    return borrower_.arena();
  }

  explicit operator bool() const noexcept { return get() != nullptr; }

 private:
  friend class Owner;
  friend class Borrower;
  template <typename U>
  friend class Owned;
  template <typename U>
  friend class Borrowed;
  template <typename U>
  friend struct Borrowable;
  friend struct std::pointer_traits<Borrowed<T>>;

  constexpr Borrowed(T* value, Borrower borrower) noexcept
      : value_(value), borrower_(borrower) {}

  T* get() const noexcept { return value_; }

  T* value_ = nullptr;
  Borrower borrower_;
};

template <typename T>
Borrowed(T*) -> Borrowed<T>;
template <typename T>
Borrowed(Borrower, T*) -> Borrowed<T>;
template <typename T>
Borrowed(Owned<T>) -> Borrowed<T>;

}  // namespace cel

namespace std {

template <typename T>
struct pointer_traits<cel::Borrowed<T>> {
  using pointer = cel::Borrowed<T>;
  using element_type = typename cel::Borrowed<T>::element_type;
  using difference_type = ptrdiff_t;

  template <typename U>
  using rebind = cel::Borrowed<U>;

  static element_type* to_address(pointer p) noexcept { return p.value_; }
};

}  // namespace std

namespace cel {

template <typename T>
Owner::Owner(Borrowed<T> borrowed) noexcept : Owner(borrowed.borrower_) {}

template <typename T>
Borrower::Borrower(const Owned<T>& owned ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
    : Borrower(owned.owner_) {}

template <typename T>
Borrower::Borrower(Borrowed<T> borrowed) noexcept
    : Borrower(borrowed.borrower_) {}

template <typename T>
Borrower& Borrower::operator=(
    const Owned<T>& owned ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  *this = owned.owner_;
  return *this;
}

template <typename T>
Borrower& Borrower::operator=(Borrowed<T> borrowed) noexcept {
  *this = borrowed.borrower_;
  return *this;
}

template <typename T>
bool operator==(Borrowed<T> lhs, std::nullptr_t) noexcept {
  return !static_cast<bool>(lhs);
}

template <typename T>
bool operator==(std::nullptr_t, Borrowed<T> rhs) noexcept {
  return rhs == nullptr;
}

template <typename T>
bool operator!=(Borrowed<T> lhs, std::nullptr_t) noexcept {
  return !operator==(lhs, nullptr);
}

template <typename T>
bool operator!=(std::nullptr_t, Borrowed<T> rhs) noexcept {
  return !operator==(nullptr, rhs);
}

template <typename T>
template <typename U, typename>
Owned<T>::Owned(Borrowed<U> other) noexcept
    : Owned(other.value_, Owner(other.borrower_)) {}

template <typename T>
template <typename U, typename>
Owned<T>& Owned<T>::operator=(Borrowed<U> other) noexcept {
  value_ = other.value_;
  owner_ = Owner(other.borrower_);
  return *this;
}

// `Ownable<T>` is a mixin for enabling the ability to get `Owned` that refer to
// this.
template <typename T>
struct Ownable {
 protected:
  Owned<const T> Own() const noexcept {
    static_assert(std::is_base_of_v<Data, T>, "T must be derived from Data");
    const T* const that = static_cast<const T*>(this);
    return Owned<const T>(
        Owner(Owner::Own(static_cast<const Data*>(that)->owner_)), that);
  }

  Owned<T> Own() noexcept {
    static_assert(std::is_base_of_v<Data, T>, "T must be derived from Data");
    T* const that = static_cast<T*>(this);
    return Owned<T>(Owner(Owner::Own(static_cast<Data*>(that)->owner_)), that);
  }

  ABSL_DEPRECATED("Use Own")
  Owned<const T> shared_from_this() const noexcept { return Own(); }

  ABSL_DEPRECATED("Use Own")
  Owned<T> shared_from_this() noexcept { return Own(); }
};

// `Borrowable<T>` is a mixin for enabling the ability to get `Borrowed` that
// refer to this.
template <typename T>
struct Borrowable {
 protected:
  Borrowed<const T> Borrow() const noexcept {
    static_assert(std::is_base_of_v<Data, T>, "T must be derived from Data");
    const T* const that = static_cast<const T*>(this);
    return Borrowed<const T>(Borrower(static_cast<const Data*>(that)->owner_),
                             that);
  }

  Borrowed<T> Borrow() noexcept {
    static_assert(std::is_base_of_v<Data, T>, "T must be derived from Data");
    T* const that = static_cast<T*>(this);
    return Borrowed<T>(Borrower(static_cast<Data*>(that)->owner_), that);
  }
};

// `ReferenceCountingMemoryManager` is a `MemoryManager` which employs automatic
// memory management through reference counting.
class ReferenceCountingMemoryManager final {
 public:
  ReferenceCountingMemoryManager(const ReferenceCountingMemoryManager&) =
      delete;
  ReferenceCountingMemoryManager(ReferenceCountingMemoryManager&&) = delete;
  ReferenceCountingMemoryManager& operator=(
      const ReferenceCountingMemoryManager&) = delete;
  ReferenceCountingMemoryManager& operator=(ReferenceCountingMemoryManager&&) =
      delete;

 private:
  static void* Allocate(size_t size, size_t alignment);

  static bool Deallocate(void* ptr, size_t size, size_t alignment) noexcept;

  explicit ReferenceCountingMemoryManager() = default;

  friend class MemoryManager;
};

// `PoolingMemoryManager` is a `MemoryManager` which employs automatic
// memory management through memory pooling.
class PoolingMemoryManager final {
 public:
  PoolingMemoryManager(const PoolingMemoryManager&) = delete;
  PoolingMemoryManager(PoolingMemoryManager&&) = delete;
  PoolingMemoryManager& operator=(const PoolingMemoryManager&) = delete;
  PoolingMemoryManager& operator=(PoolingMemoryManager&&) = delete;

 private:
  // Allocates memory directly from the allocator used by this memory manager.
  // If `memory_management()` returns `MemoryManagement::kReferenceCounting`,
  // this allocation *must* be explicitly deallocated at some point via
  // `Deallocate`. Otherwise deallocation is optional.
  ABSL_MUST_USE_RESULT static void* Allocate(google::protobuf::Arena* absl_nonnull arena,
                                             size_t size, size_t alignment) {
    ABSL_DCHECK(absl::has_single_bit(alignment))
        << "alignment must be a power of 2";
    if (size == 0) {
      return nullptr;
    }
    return arena->AllocateAligned(size, alignment);
  }

  // Attempts to deallocate memory previously allocated via `Allocate`, `size`
  // and `alignment` must match the values from the previous call to `Allocate`.
  // Returns `true` if the deallocation was successful and additional calls to
  // `Allocate` may re-use the memory, `false` otherwise. Returns `false` if
  // given `nullptr`.
  static bool Deallocate(google::protobuf::Arena* absl_nonnull, void*, size_t,
                         size_t alignment) noexcept {
    ABSL_DCHECK(absl::has_single_bit(alignment))
        << "alignment must be a power of 2";
    return false;
  }

  // Registers a custom destructor to be run upon destruction of the memory
  // management implementation. Return value is always `true`, indicating that
  // the destructor may be called at some point in the future.
  static bool OwnCustomDestructor(google::protobuf::Arena* absl_nonnull arena,
                                  void* object,
                                  void (*absl_nonnull destruct)(void*)) {
    ABSL_DCHECK(destruct != nullptr);
    arena->OwnCustomDestructor(object, destruct);
    return true;
  }

  template <typename T>
  static void DefaultDestructor(void* ptr) {
    static_assert(!std::is_trivially_destructible_v<T>);
    static_cast<T*>(ptr)->~T();
  }

  explicit PoolingMemoryManager() = default;

  friend class MemoryManager;
};

// `MemoryManager` is an abstraction for supporting automatic memory management.
// All objects created by the `MemoryManager` have a lifetime governed by the
// underlying memory management strategy. Currently `MemoryManager` is a
// composed type that holds either a reference to
// `ReferenceCountingMemoryManager` or owns a `PoolingMemoryManager`.
//
// ============================ Reference Counting ============================
// `Unique`: The object is valid until destruction of the `Unique`.
//
// `Shared`: The object is valid so long as one or more `Shared` managing the
// object exist.
//
// ================================= Pooling ==================================
// `Unique`: The object is valid until destruction of the underlying memory
// resources or of the `Unique`.
//
// `Shared`: The object is valid until destruction of the underlying memory
// resources.
class MemoryManager final {
 public:
  // Returns a `MemoryManager` which utilizes an arena but never frees its
  // memory. It is effectively a memory leak and should only be used for limited
  // use cases, such as initializing singletons which live for the life of the
  // program.
  static MemoryManager Unmanaged();

  // Returns a `MemoryManager` which utilizes reference counting.
  ABSL_MUST_USE_RESULT static MemoryManager ReferenceCounting() {
    return MemoryManager(nullptr);
  }

  // Returns a `MemoryManager` which utilizes an arena.
  ABSL_MUST_USE_RESULT static MemoryManager Pooling(
      google::protobuf::Arena* absl_nonnull arena) {
    return MemoryManager(arena);
  }

  explicit MemoryManager(Allocator<> allocator) : arena_(allocator.arena()) {}

  MemoryManager() = delete;
  MemoryManager(const MemoryManager&) = default;
  MemoryManager& operator=(const MemoryManager&) = default;

  MemoryManagement memory_management() const noexcept {
    return arena_ == nullptr ? MemoryManagement::kReferenceCounting
                             : MemoryManagement::kPooling;
  }

  // Allocates memory directly from the allocator used by this memory manager.
  // If `memory_management()` returns `MemoryManagement::kReferenceCounting`,
  // this allocation *must* be explicitly deallocated at some point via
  // `Deallocate`. Otherwise deallocation is optional.
  ABSL_MUST_USE_RESULT void* Allocate(size_t size, size_t alignment) {
    if (arena_ == nullptr) {
      return ReferenceCountingMemoryManager::Allocate(size, alignment);
    } else {
      return PoolingMemoryManager::Allocate(arena_, size, alignment);
    }
  }

  // Attempts to deallocate memory previously allocated via `Allocate`, `size`
  // and `alignment` must match the values from the previous call to `Allocate`.
  // Returns `true` if the deallocation was successful and additional calls to
  // `Allocate` may re-use the memory, `false` otherwise. Returns `false` if
  // given `nullptr`.
  bool Deallocate(void* ptr, size_t size, size_t alignment) noexcept {
    if (arena_ == nullptr) {
      return ReferenceCountingMemoryManager::Deallocate(ptr, size, alignment);
    } else {
      return PoolingMemoryManager::Deallocate(arena_, ptr, size, alignment);
    }
  }

  // Registers a custom destructor to be run upon destruction of the memory
  // management implementation. A return of `true` indicates the destructor may
  // be called at some point in the future, `false` if will definitely not be
  // called. All pooling memory managers return `true` while the reference
  // counting memory manager returns `false`.
  bool OwnCustomDestructor(void* object, void (*absl_nonnull destruct)(void*)) {
    ABSL_DCHECK(destruct != nullptr);
    if (arena_ == nullptr) {
      return false;
    } else {
      return PoolingMemoryManager::OwnCustomDestructor(arena_, object,
                                                       destruct);
    }
  }

  google::protobuf::Arena* absl_nullable arena() const noexcept { return arena_; }

  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator Allocator<T>() const {
    return arena();
  }

  friend void swap(MemoryManager& lhs, MemoryManager& rhs) noexcept {
    using std::swap;
    swap(lhs.arena_, rhs.arena_);
  }

 private:
  friend class PoolingMemoryManager;

  explicit MemoryManager(std::nullptr_t) : arena_(nullptr) {}

  explicit MemoryManager(google::protobuf::Arena* absl_nonnull arena) : arena_(arena) {}

  // If `nullptr`, we are using reference counting. Otherwise we are using
  // Pooling. We use `UnreachablePooling()` as a sentinel to detect use after
  // move otherwise the moved-from `MemoryManager` would be in a valid state and
  // utilize reference counting.
  google::protobuf::Arena* absl_nullable arena_;
};

using MemoryManagerRef = MemoryManager;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_MEMORY_H_
