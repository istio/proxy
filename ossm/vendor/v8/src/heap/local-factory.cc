// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/local-factory.h"

#include "src/common/globals.h"
#include "src/execution/local-isolate.h"
#include "src/handles/handles.h"
#include "src/heap/concurrent-allocator-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/heap/local-heap-inl.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/string.h"
#include "src/roots/roots-inl.h"
#include "src/strings/string-hasher.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX
LocalFactory::LocalFactory(Isolate* isolate)
    : roots_(isolate), isolate_for_sandbox_(isolate) {}
#else
LocalFactory::LocalFactory(Isolate* isolate) : roots_(isolate) {}
#endif

void LocalFactory::AddToScriptList(Handle<Script> shared) {
// TODO(leszeks): Actually add the script to the main Isolate's script list,
// in a thread-safe way.
//
// At the moment, we have to do one final fix-up during off-thread
// finalization, where we add the created script to the script list, but this
// relies on there being exactly one script created during the lifetime of
// this LocalFactory.
//
// For now, prevent accidentaly creating more scripts that don't get added to
// the script list with a simple DCHECK.
#ifdef DEBUG
  DCHECK(!a_script_was_added_to_the_script_list_);
  a_script_was_added_to_the_script_list_ = true;
#endif
}

HeapObject LocalFactory::AllocateRaw(int size, AllocationType allocation,
                                     AllocationAlignment alignment) {
  DCHECK(allocation == AllocationType::kOld ||
         allocation == AllocationType::kSharedOld);
  return HeapObject::FromAddress(isolate()->heap()->AllocateRawOrFail(
      size, allocation, AllocationOrigin::kRuntime, alignment));
}

int LocalFactory::NumberToStringCacheHash(Smi) { return 0; }

int LocalFactory::NumberToStringCacheHash(double) { return 0; }

void LocalFactory::NumberToStringCacheSet(Handle<Object>, int, Handle<String>) {
}

Handle<Object> LocalFactory::NumberToStringCacheGet(Object, int) {
  return undefined_value();
}

}  // namespace internal
}  // namespace v8
