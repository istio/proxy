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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_NEW_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_NEW_H_

#include <cstddef>
#include <new>
#include <utility>

namespace cel::internal {

inline constexpr size_t kDefaultNewAlignment =
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
    __STDCPP_DEFAULT_NEW_ALIGNMENT__
#else
    alignof(std::max_align_t)
#endif
    ;  // NOLINT(whitespace/semicolon)

// Allocates memory which has a size of at least `size` and a minimum alignment
// of `kDefaultNewAlignment`.
void* New(size_t size);

// Allocates memory which has a size of at least `size` and a minimum alignment
// of `alignment`. To deallocate, the caller must use `AlignedDelete` or
// `SizedAlignedDelete`.
void* AlignedNew(size_t size, std::align_val_t alignment);

std::pair<void*, size_t> SizeReturningNew(size_t size);

// Allocates memory which has a size of at least `size` and a minimum alignment
// of `alignment`, returns a pointer to the allocated memory and the actual
// usable allocation size. To deallocate, the caller must use `AlignedDelete` or
// `SizedAlignedDelete`.
std::pair<void*, size_t> SizeReturningAlignedNew(size_t size,
                                                 std::align_val_t alignment);

void Delete(void* ptr) noexcept;

void SizedDelete(void* ptr, size_t size) noexcept;

void AlignedDelete(void* ptr, std::align_val_t alignment) noexcept;

void SizedAlignedDelete(void* ptr, size_t size,
                        std::align_val_t alignment) noexcept;

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_NEW_H_
