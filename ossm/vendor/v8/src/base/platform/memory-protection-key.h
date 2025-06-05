// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_MEMORY_PROTECTION_KEY_H_
#define V8_BASE_PLATFORM_MEMORY_PROTECTION_KEY_H_

#if V8_HAS_PKU_JIT_WRITE_PROTECT
#include <sys/mman.h>  // For static_assert of permission values.
#undef MAP_TYPE  // Conflicts with MAP_TYPE in Torque-generated instance-types.h
#endif

#include "include/v8-platform.h"
#include "src/base/address-region.h"

namespace v8 {
namespace base {

// ----------------------------------------------------------------------------
// MemoryProtectionKey
//
// This class has static methods for the different platform specific
// functions related to memory protection key support.

// TODO(dlehmann): Consider adding this to {base::PageAllocator} (higher-level,
// exported API) once the API is more stable and we have converged on a better
// design (e.g., typed class wrapper around int memory protection key).
class V8_BASE_EXPORT MemoryProtectionKey {
 public:
  // Sentinel value if there is no PKU support or allocation of a key failed.
  // This is also the return value on an error of pkey_alloc() and has the
  // benefit that calling pkey_mprotect() with -1 behaves the same as regular
  // mprotect().
  static constexpr int kNoMemoryProtectionKey = -1;

  // Permissions for memory protection keys on top of the page's permissions.
  // NOTE: Since there is no executable bit, the executable permission cannot be
  // withdrawn by memory protection keys.
  enum Permission {
    kNoRestrictions = 0,
    kDisableAccess = 1,
    kDisableWrite = 2,
  };

// If sys/mman.h has PKEY support (on newer Linux distributions), ensure that
// our definitions of the permissions is consistent with the ones in glibc.
#if defined(PKEY_DISABLE_ACCESS)
  static_assert(kDisableAccess == PKEY_DISABLE_ACCESS);
  static_assert(kDisableWrite == PKEY_DISABLE_WRITE);
#endif

  // Call exactly once per process to determine if PKU is supported on this
  // platform and initialize global data structures.
  static void InitializeMemoryProtectionKeySupport();

  // Allocates a memory protection key on platforms with PKU support, returns
  // {kNoMemoryProtectionKey} on platforms without support or when allocation
  // failed at runtime.
  static int AllocateKey();

  // Frees the given memory protection key, to make it available again for the
  // next call to {AllocateKey()}. Note that this does NOT
  // invalidate access rights to pages that are still tied to that key. That is,
  // if the key is reused and pages with that key are still accessable, this
  // might be a security issue. See
  // https://www.gnu.org/software/libc/manual/html_mono/libc.html#Memory-Protection-Keys
  static void FreeKey(int key);

  // Associates a memory protection {key} with the given {region}.
  // If {key} is {kNoMemoryProtectionKey} this behaves like "plain"
  // {SetPermissions()} and associates the default key to the region. That is,
  // explicitly calling with {kNoMemoryProtectionKey} can be used to
  // disassociate any protection key from a region. This also means "plain"
  // {SetPermissions()} disassociates the key from a region, making the key's
  // access restrictions irrelevant/inactive for that region. Returns true if
  // changing permissions and key was successful. (Returns a bool to be
  // consistent with {SetPermissions()}). The {page_permissions} are the
  // permissions of the page, not the key. For changing the permissions of the
  // key, use {SetPermissionsForKey()} instead.
  static bool SetPermissionsAndKey(
      v8::PageAllocator* page_allocator, base::AddressRegion region,
      v8::PageAllocator::Permission page_permissions, int key);

  // Set the key's permissions. {key} must be valid, i.e. not
  // {kNoMemoryProtectionKey}.
  static void SetPermissionsForKey(int key, Permission permissions);

  // Get the permissions of the protection key {key} for the current thread.
  static Permission GetKeyPermission(int key);
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_PLATFORM_MEMORY_PROTECTION_KEY_H_
