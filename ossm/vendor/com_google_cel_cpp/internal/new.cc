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

#include "internal/new.h"

#include <cstddef>
#include <cstdlib>
#include <new>
#include <utility>

#ifdef _MSC_VER
#include <malloc.h>
#endif

#include "absl/base/config.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/numeric/bits.h"
#include "internal/align.h"

#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606L
#define CEL_INTERNAL_HAVE_ALIGNED_NEW 1
#endif

#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
#define CEL_INTERNAL_HAVE_SIZED_DELETE 1
#endif

namespace cel::internal {

namespace {

[[noreturn, maybe_unused]] void ThrowStdBadAlloc() {
#ifdef ABSL_HAVE_EXCEPTIONS
  throw std::bad_alloc();
#else
  std::abort();
#endif
}

}  // namespace

void* New(size_t size) { return ::operator new(size); }

void* AlignedNew(size_t size, std::align_val_t alignment) {
  ABSL_DCHECK(absl::has_single_bit(static_cast<size_t>(alignment)));
#ifdef CEL_INTERNAL_HAVE_ALIGNED_NEW
  return ::operator new(size, alignment);
#else
  if (static_cast<size_t>(alignment) <= kDefaultNewAlignment) {
    return New(size);
  }
#if defined(_MSC_VER)
  void* ptr = _aligned_malloc(size, static_cast<size_t>(alignment));
  if (ABSL_PREDICT_FALSE(size != 0 && ptr == nullptr)) {
    ThrowStdBadAlloc();
  }
  return ptr;
#else
  void* ptr = std::aligned_alloc(static_cast<size_t>(alignment), size);
  if (ABSL_PREDICT_FALSE(size != 0 && ptr == nullptr)) {
    ThrowStdBadAlloc();
  }
  return ptr;
#endif
#endif
}

std::pair<void*, size_t> SizeReturningNew(size_t size) {
  return std::pair{::operator new(size), size};
}

std::pair<void*, size_t> SizeReturningAlignedNew(size_t size,
                                                 std::align_val_t alignment) {
  ABSL_DCHECK(absl::has_single_bit(static_cast<size_t>(alignment)));
#ifdef CEL_INTERNAL_HAVE_ALIGNED_NEW
  return std::pair{::operator new(size, alignment), size};
#else
  return std::pair{AlignedNew(size, alignment), size};
#endif
}

void Delete(void* ptr) noexcept { ::operator delete(ptr); }

void SizedDelete(void* ptr, size_t size) noexcept {
#ifdef CEL_INTERNAL_HAVE_SIZED_DELETE
  ::operator delete(ptr, size);
#else
  ::operator delete(ptr);
#endif
}

void AlignedDelete(void* ptr, std::align_val_t alignment) noexcept {
  ABSL_DCHECK(absl::has_single_bit(static_cast<size_t>(alignment)));
#ifdef CEL_INTERNAL_HAVE_ALIGNED_NEW
  ::operator delete(ptr, alignment);
#else
  if (static_cast<size_t>(alignment) <= kDefaultNewAlignment) {
    Delete(ptr, size);
  } else {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
  }
#endif
}

void SizedAlignedDelete(void* ptr, size_t size,
                        std::align_val_t alignment) noexcept {
  ABSL_DCHECK(absl::has_single_bit(static_cast<size_t>(alignment)));
#ifdef CEL_INTERNAL_HAVE_ALIGNED_NEW
#ifdef CEL_INTERNAL_HAVE_SIZED_DELETE
  ::operator delete(ptr, size, alignment);
#else
  ::operator delete(ptr, alignment);
#endif
#else
  AlignedDelete(ptr, alignment);
#endif
}

}  // namespace cel::internal
