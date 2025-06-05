// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ATOMICS_SYNCHRONIZATION_INL_H_
#define V8_OBJECTS_JS_ATOMICS_SYNCHRONIZATION_INL_H_

#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/js-atomics-synchronization.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-atomics-synchronization-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(JSSynchronizationPrimitive)

std::atomic<JSSynchronizationPrimitive::StateT>*
JSSynchronizationPrimitive::AtomicStatePtr() {
  StateT* state_ptr = reinterpret_cast<StateT*>(field_address(kStateOffset));
  DCHECK(IsAligned(reinterpret_cast<uintptr_t>(state_ptr), sizeof(StateT)));
  return base::AsAtomicPtr(state_ptr);
}

TQ_OBJECT_CONSTRUCTORS_IMPL(JSAtomicsMutex)

CAST_ACCESSOR(JSAtomicsMutex)

JSAtomicsMutex::LockGuard::LockGuard(Isolate* isolate,
                                     Handle<JSAtomicsMutex> mutex)
    : isolate_(isolate), mutex_(mutex) {
  JSAtomicsMutex::Lock(isolate, mutex);
}

JSAtomicsMutex::LockGuard::~LockGuard() { mutex_->Unlock(isolate_); }

JSAtomicsMutex::TryLockGuard::TryLockGuard(Isolate* isolate,
                                           Handle<JSAtomicsMutex> mutex)
    : isolate_(isolate), mutex_(mutex), locked_(mutex->TryLock()) {}

JSAtomicsMutex::TryLockGuard::~TryLockGuard() {
  if (locked_) mutex_->Unlock(isolate_);
}

// static
void JSAtomicsMutex::Lock(Isolate* requester, Handle<JSAtomicsMutex> mutex) {
  DisallowGarbageCollection no_gc;
  // First try to lock an uncontended mutex, which should be the common case. If
  // this fails, then go to the slow path to possibly put the current thread to
  // sleep.
  //
  // The fast path is done using a weak CAS which may fail spuriously on
  // architectures with load-link/store-conditional instructions.
  std::atomic<StateT>* state = mutex->AtomicStatePtr();
  StateT expected = kUnlocked;
  if (V8_UNLIKELY(!state->compare_exchange_weak(expected, kLockedUncontended,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed))) {
    LockSlowPath(requester, mutex, state);
  }
  mutex->SetCurrentThreadAsOwner();
}

bool JSAtomicsMutex::TryLock() {
  DisallowGarbageCollection no_gc;
  StateT expected = kUnlocked;
  if (V8_LIKELY(AtomicStatePtr()->compare_exchange_strong(
          expected, kLockedUncontended, std::memory_order_acquire,
          std::memory_order_relaxed))) {
    SetCurrentThreadAsOwner();
    return true;
  }
  return false;
}

void JSAtomicsMutex::Unlock(Isolate* requester) {
  DisallowGarbageCollection no_gc;
  // First try to unlock an uncontended mutex, which should be the common
  // case. If this fails, then go to the slow path to wake a waiting thread.
  //
  // In contrast to Lock, the fast path is done using a strong CAS which does
  // not fail spuriously. This simplifies the slow path by guaranteeing that
  // there is at least one waiter to be notified.
  DCHECK(IsCurrentThreadOwner());
  ClearOwnerThread();
  std::atomic<StateT>* state = AtomicStatePtr();
  StateT expected = kLockedUncontended;
  if (V8_LIKELY(state->compare_exchange_strong(expected, kUnlocked,
                                               std::memory_order_release,
                                               std::memory_order_relaxed))) {
    return;
  }
  UnlockSlowPath(requester, state);
}

bool JSAtomicsMutex::IsHeld() {
  return AtomicStatePtr()->load(std::memory_order_relaxed) & kIsLockedBit;
}

bool JSAtomicsMutex::IsCurrentThreadOwner() {
  bool result = AtomicOwnerThreadIdPtr()->load(std::memory_order_relaxed) ==
                ThreadId::Current().ToInteger();
  DCHECK_IMPLIES(result, IsHeld());
  return result;
}

void JSAtomicsMutex::SetCurrentThreadAsOwner() {
  AtomicOwnerThreadIdPtr()->store(ThreadId::Current().ToInteger(),
                                  std::memory_order_relaxed);
}

void JSAtomicsMutex::ClearOwnerThread() {
  AtomicOwnerThreadIdPtr()->store(ThreadId::Invalid().ToInteger(),
                                  std::memory_order_relaxed);
}

std::atomic<int32_t>* JSAtomicsMutex::AtomicOwnerThreadIdPtr() {
  int32_t* owner_thread_id_ptr =
      reinterpret_cast<int32_t*>(field_address(kOwnerThreadIdOffset));
  return base::AsAtomicPtr(owner_thread_id_ptr);
}

TQ_OBJECT_CONSTRUCTORS_IMPL(JSAtomicsCondition)

CAST_ACCESSOR(JSAtomicsCondition)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ATOMICS_SYNCHRONIZATION_INL_H_
