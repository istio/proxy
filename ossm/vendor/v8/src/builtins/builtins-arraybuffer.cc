// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/handles/maybe-handles-inl.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/logging/counters.h"
#include "src/numbers/conversions.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

#define CHECK_SHARED(expected, name, method)                                \
  if (name->is_shared() != expected) {                                      \
    THROW_NEW_ERROR_RETURN_FAILURE(                                         \
        isolate,                                                            \
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,          \
                     isolate->factory()->NewStringFromAsciiChecked(method), \
                     name));                                                \
  }

#define CHECK_RESIZABLE(expected, name, method)                             \
  if (name->is_resizable() != expected) {                                   \
    THROW_NEW_ERROR_RETURN_FAILURE(                                         \
        isolate,                                                            \
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,          \
                     isolate->factory()->NewStringFromAsciiChecked(method), \
                     name));                                                \
  }

// -----------------------------------------------------------------------------
// ES#sec-arraybuffer-objects

namespace {

Object ConstructBuffer(Isolate* isolate, Handle<JSFunction> target,
                       Handle<JSReceiver> new_target, Handle<Object> length,
                       Handle<Object> max_length, InitializedFlag initialized) {
  SharedFlag shared = *target != target->native_context().array_buffer_fun()
                          ? SharedFlag::kShared
                          : SharedFlag::kNotShared;
  ResizableFlag resizable = max_length.is_null() ? ResizableFlag::kNotResizable
                                                 : ResizableFlag::kResizable;
  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::New(target, new_target, Handle<AllocationSite>::null()));
  auto array_buffer = Handle<JSArrayBuffer>::cast(result);
  // Ensure that all fields are initialized because BackingStore::Allocate is
  // allowed to GC. Note that we cannot move the allocation of the ArrayBuffer
  // after BackingStore::Allocate because of the spec.
  array_buffer->Setup(shared, resizable, nullptr);

  size_t byte_length;
  size_t max_byte_length = 0;
  if (!TryNumberToSize(*length, &byte_length) ||
      byte_length > JSArrayBuffer::kMaxByteLength) {
    // ToNumber failed.
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
  }

  std::unique_ptr<BackingStore> backing_store;
  if (resizable == ResizableFlag::kNotResizable) {
    backing_store =
        BackingStore::Allocate(isolate, byte_length, shared, initialized);
    max_byte_length = byte_length;
  } else {
    // We need to check the max length against both
    // JSArrayBuffer::kMaxByteLength and JSTypedArray::kMaxLength, since it's
    // possible to create length-tracking TypedArrays and resize the underlying
    // buffer. If the max byte length was larger than JSTypedArray::kMaxLength,
    // that'd result in having a TypedArray with length larger than
    // JSTypedArray::kMaxLength.
    if (!TryNumberToSize(*max_length, &max_byte_length) ||
        max_byte_length > JSArrayBuffer::kMaxByteLength ||
        max_byte_length > JSTypedArray::kMaxLength) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewRangeError(MessageTemplate::kInvalidArrayBufferMaxLength));
    }
    if (byte_length > max_byte_length) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewRangeError(MessageTemplate::kInvalidArrayBufferMaxLength));
    }

    size_t page_size, initial_pages, max_pages;
    MAYBE_RETURN(JSArrayBuffer::GetResizableBackingStorePageConfiguration(
                     isolate, byte_length, max_byte_length, kThrowOnError,
                     &page_size, &initial_pages, &max_pages),
                 ReadOnlyRoots(isolate).exception());

    backing_store = BackingStore::TryAllocateAndPartiallyCommitMemory(
        isolate, byte_length, max_byte_length, page_size, initial_pages,
        max_pages, WasmMemoryFlag::kNotWasm, shared);
  }
  if (!backing_store) {
    // Allocation of backing store failed.
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kArrayBufferAllocationFailed));
  }

  array_buffer->Attach(std::move(backing_store));
  array_buffer->set_max_byte_length(max_byte_length);
  return *array_buffer;
}

}  // namespace

// ES #sec-arraybuffer-constructor
BUILTIN(ArrayBufferConstructor) {
  HandleScope scope(isolate);
  Handle<JSFunction> target = args.target();
  DCHECK(*target == target->native_context().array_buffer_fun() ||
         *target == target->native_context().shared_array_buffer_fun());
  if (args.new_target()->IsUndefined(isolate)) {  // [[Call]]
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kConstructorNotFunction,
                              handle(target->shared().Name(), isolate)));
  }
  // [[Construct]]
  Handle<JSReceiver> new_target = Handle<JSReceiver>::cast(args.new_target());
  Handle<Object> length = args.atOrUndefined(isolate, 1);

  Handle<Object> number_length;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_length,
                                     Object::ToInteger(isolate, length));
  if (number_length->Number() < 0.0) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
  }

  Handle<Object> number_max_length;
  if (v8_flags.harmony_rab_gsab) {
    Handle<Object> max_length;
    Handle<Object> options = args.atOrUndefined(isolate, 2);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, max_length,
        JSObject::ReadFromOptionsBag(
            options, isolate->factory()->max_byte_length_string(), isolate));

    if (!max_length->IsUndefined(isolate)) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, number_max_length, Object::ToInteger(isolate, max_length));
    }
  }
  return ConstructBuffer(isolate, target, new_target, number_length,
                         number_max_length, InitializedFlag::kZeroInitialized);
}

// This is a helper to construct an ArrayBuffer with uinitialized memory.
// This means the caller must ensure the buffer is totally initialized in
// all cases, or we will expose uinitialized memory to user code.
BUILTIN(ArrayBufferConstructor_DoNotInitialize) {
  HandleScope scope(isolate);
  Handle<JSFunction> target(isolate->native_context()->array_buffer_fun(),
                            isolate);
  Handle<Object> length = args.atOrUndefined(isolate, 1);
  return ConstructBuffer(isolate, target, target, length, Handle<Object>(),
                         InitializedFlag::kUninitialized);
}

static Object SliceHelper(BuiltinArguments args, Isolate* isolate,
                          const char* kMethodName, bool is_shared) {
  HandleScope scope(isolate);
  Handle<Object> start = args.at(1);
  Handle<Object> end = args.atOrUndefined(isolate, 2);

  // * If Type(O) is not Object, throw a TypeError exception.
  // * If O does not have an [[ArrayBufferData]] internal slot, throw a
  //   TypeError exception.
  CHECK_RECEIVER(JSArrayBuffer, array_buffer, kMethodName);
  // * [AB] If IsSharedArrayBuffer(O) is true, throw a TypeError exception.
  // * [SAB] If IsSharedArrayBuffer(O) is false, throw a TypeError exception.
  CHECK_SHARED(is_shared, array_buffer, kMethodName);

  // * [AB] If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
  if (!is_shared && array_buffer->was_detached()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  kMethodName)));
  }

  // * [AB] Let len be O.[[ArrayBufferByteLength]].
  // * [SAB] Let len be O.[[ArrayBufferByteLength]].
  double const len = array_buffer->GetByteLength();

  // * Let relativeStart be ? ToInteger(start).
  Handle<Object> relative_start;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, relative_start,
                                     Object::ToInteger(isolate, start));

  // * If relativeStart < 0, let first be max((len + relativeStart), 0); else
  //   let first be min(relativeStart, len).
  double const first = (relative_start->Number() < 0)
                           ? std::max(len + relative_start->Number(), 0.0)
                           : std::min(relative_start->Number(), len);

  // * If end is undefined, let relativeEnd be len; else let relativeEnd be ?
  //   ToInteger(end).
  double relative_end;
  if (end->IsUndefined(isolate)) {
    relative_end = len;
  } else {
    Handle<Object> relative_end_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, relative_end_obj,
                                       Object::ToInteger(isolate, end));
    relative_end = relative_end_obj->Number();
  }

  // * If relativeEnd < 0, let final be max((len + relativeEnd), 0); else let
  //   final be min(relativeEnd, len).
  double const final_ = (relative_end < 0) ? std::max(len + relative_end, 0.0)
                                           : std::min(relative_end, len);

  // * Let newLen be max(final-first, 0).
  double const new_len = std::max(final_ - first, 0.0);
  Handle<Object> new_len_obj = isolate->factory()->NewNumber(new_len);

  // * [AB] Let ctor be ? SpeciesConstructor(O, %ArrayBuffer%).
  // * [SAB] Let ctor be ? SpeciesConstructor(O, %SharedArrayBuffer%).
  Handle<JSFunction> constructor_fun = is_shared
                                           ? isolate->shared_array_buffer_fun()
                                           : isolate->array_buffer_fun();
  Handle<Object> ctor;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, ctor,
      Object::SpeciesConstructor(
          isolate, Handle<JSReceiver>::cast(args.receiver()), constructor_fun));

  // * Let new be ? Construct(ctor, newLen).
  Handle<JSReceiver> new_;
  {
    const int argc = 1;

    base::ScopedVector<Handle<Object>> argv(argc);
    argv[0] = new_len_obj;

    Handle<Object> new_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, new_obj, Execution::New(isolate, ctor, argc, argv.begin()));

    new_ = Handle<JSReceiver>::cast(new_obj);
  }

  // * If new does not have an [[ArrayBufferData]] internal slot, throw a
  //   TypeError exception.
  if (!new_->IsJSArrayBuffer()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                     isolate->factory()->NewStringFromAsciiChecked(kMethodName),
                     new_));
  }

  // * [AB] If IsSharedArrayBuffer(new) is true, throw a TypeError exception.
  // * [SAB] If IsSharedArrayBuffer(new) is false, throw a TypeError exception.
  Handle<JSArrayBuffer> new_array_buffer = Handle<JSArrayBuffer>::cast(new_);
  CHECK_SHARED(is_shared, new_array_buffer, kMethodName);

  // The created ArrayBuffer might or might not be resizable, since the species
  // constructor might return a non-resizable or a resizable buffer.

  // * [AB] If IsDetachedBuffer(new) is true, throw a TypeError exception.
  if (!is_shared && new_array_buffer->was_detached()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  kMethodName)));
  }

  // * [AB] If SameValue(new, O) is true, throw a TypeError exception.
  if (!is_shared && new_->SameValue(*args.receiver())) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kArrayBufferSpeciesThis));
  }

  // * [SAB] If new.[[ArrayBufferData]] and O.[[ArrayBufferData]] are the same
  //         Shared Data Block values, throw a TypeError exception.
  if (is_shared &&
      new_array_buffer->backing_store() == array_buffer->backing_store()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kSharedArrayBufferSpeciesThis));
  }

  // * If new.[[ArrayBufferByteLength]] < newLen, throw a TypeError exception.
  size_t new_array_buffer_byte_length = new_array_buffer->GetByteLength();
  if (new_array_buffer_byte_length < new_len) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(is_shared ? MessageTemplate::kSharedArrayBufferTooShort
                               : MessageTemplate::kArrayBufferTooShort));
  }

  // * [AB] NOTE: Side-effects of the above steps may have detached O.
  // * [AB] If IsDetachedBuffer(O) is true, throw a TypeError exception.
  if (!is_shared && array_buffer->was_detached()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  kMethodName)));
  }

  // * Let fromBuf be O.[[ArrayBufferData]].
  // * Let toBuf be new.[[ArrayBufferData]].
  // * Perform CopyDataBlockBytes(toBuf, 0, fromBuf, first, newLen).
  size_t first_size = first;
  size_t new_len_size = new_len;
  DCHECK(new_array_buffer_byte_length >= new_len_size);

  if (new_len_size != 0) {
    size_t from_byte_length = array_buffer->GetByteLength();
    if (V8_UNLIKELY(!is_shared && array_buffer->is_resizable())) {
      // The above steps might have resized the underlying buffer. In that case,
      // only copy the still-accessible portion of the underlying data.
      if (first_size > from_byte_length) {
        return *new_;  // Nothing to copy.
      }
      if (new_len_size > from_byte_length - first_size) {
        new_len_size = from_byte_length - first_size;
      }
    }
    DCHECK(first_size <= from_byte_length);
    DCHECK(from_byte_length - first_size >= new_len_size);
    uint8_t* from_data =
        reinterpret_cast<uint8_t*>(array_buffer->backing_store()) + first_size;
    uint8_t* to_data =
        reinterpret_cast<uint8_t*>(new_array_buffer->backing_store());
    if (is_shared) {
      base::Relaxed_Memcpy(reinterpret_cast<base::Atomic8*>(to_data),
                           reinterpret_cast<base::Atomic8*>(from_data),
                           new_len_size);
    } else {
      CopyBytes(to_data, from_data, new_len_size);
    }
  }

  return *new_;
}

// ES #sec-sharedarraybuffer.prototype.slice
BUILTIN(SharedArrayBufferPrototypeSlice) {
  const char* const kMethodName = "SharedArrayBuffer.prototype.slice";
  return SliceHelper(args, isolate, kMethodName, true);
}

// ES #sec-arraybuffer.prototype.slice
// ArrayBuffer.prototype.slice ( start, end )
BUILTIN(ArrayBufferPrototypeSlice) {
  const char* const kMethodName = "ArrayBuffer.prototype.slice";
  return SliceHelper(args, isolate, kMethodName, false);
}

static Object ResizeHelper(BuiltinArguments args, Isolate* isolate,
                           const char* kMethodName, bool is_shared) {
  HandleScope scope(isolate);

  // 1 Let O be the this value.
  // 2. Perform ? RequireInternalSlot(O, [[ArrayBufferMaxByteLength]]).
  CHECK_RECEIVER(JSArrayBuffer, array_buffer, kMethodName);
  CHECK_RESIZABLE(true, array_buffer, kMethodName);

  // [RAB] 3. If IsSharedArrayBuffer(O) is true, throw a *TypeError* exception
  // [GSAB] 3. If IsSharedArrayBuffer(O) is false, throw a *TypeError* exception
  CHECK_SHARED(is_shared, array_buffer, kMethodName);

  // Let newByteLength to ? ToIntegerOrInfinity(newLength).
  Handle<Object> new_length = args.at(1);
  Handle<Object> number_new_byte_length;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_new_byte_length,
                                     Object::ToInteger(isolate, new_length));

  // [RAB] If IsDetachedBuffer(O) is true, throw a TypeError exception.
  if (!is_shared && array_buffer->was_detached()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  kMethodName)));
  }

  // [RAB] If newByteLength < 0 or newByteLength >
  // O.[[ArrayBufferMaxByteLength]], throw a RangeError exception.

  // [GSAB] If newByteLength < currentByteLength or newByteLength >
  // O.[[ArrayBufferMaxByteLength]], throw a RangeError exception.
  size_t new_byte_length;
  if (!TryNumberToSize(*number_new_byte_length, &new_byte_length)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferResizeLength,
                               isolate->factory()->NewStringFromAsciiChecked(
                                   kMethodName)));
  }

  if (is_shared && new_byte_length < array_buffer->byte_length()) {
    // GrowableSharedArrayBuffer is only allowed to grow.
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferResizeLength,
                               isolate->factory()->NewStringFromAsciiChecked(
                                   kMethodName)));
  }

  if (new_byte_length > array_buffer->max_byte_length()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferResizeLength,
                               isolate->factory()->NewStringFromAsciiChecked(
                                   kMethodName)));
  }

  // [RAB] Let hostHandled be ? HostResizeArrayBuffer(O, newByteLength).
  // [GSAB] Let hostHandled be ? HostGrowArrayBuffer(O, newByteLength).
  // If hostHandled is handled, return undefined.

  // TODO(v8:11111, v8:12746): Wasm integration.

  if (!is_shared) {
    // [RAB] Let oldBlock be O.[[ArrayBufferData]].
    // [RAB] Let newBlock be ? CreateByteDataBlock(newByteLength).
    // [RAB] Let copyLength be min(newByteLength, O.[[ArrayBufferByteLength]]).
    // [RAB] Perform CopyDataBlockBytes(newBlock, 0, oldBlock, 0, copyLength).
    // [RAB] NOTE: Neither creation of the new Data Block nor copying from the
    // old Data Block are observable. Implementations reserve the right to
    // implement this method as in-place growth or shrinkage.
    if (array_buffer->GetBackingStore()->ResizeInPlace(isolate,
                                                       new_byte_length) !=
        BackingStore::ResizeOrGrowResult::kSuccess) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewRangeError(MessageTemplate::kOutOfMemory,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     kMethodName)));
    }
    // [RAB] Set O.[[ArrayBufferByteLength]] to newLength.
    array_buffer->set_byte_length(new_byte_length);
  } else {
    // [GSAB] (Detailed description of the algorithm omitted.)
    auto result =
        array_buffer->GetBackingStore()->GrowInPlace(isolate, new_byte_length);
    if (result == BackingStore::ResizeOrGrowResult::kFailure) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewRangeError(MessageTemplate::kOutOfMemory,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     kMethodName)));
    }
    if (result == BackingStore::ResizeOrGrowResult::kRace) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewRangeError(
              MessageTemplate::kInvalidArrayBufferResizeLength,
              isolate->factory()->NewStringFromAsciiChecked(kMethodName)));
    }
    // Invariant: byte_length for a GSAB is 0 (it needs to be read from the
    // BackingStore).
    CHECK_EQ(0, array_buffer->byte_length());
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

// ES #sec-get-sharedarraybuffer.prototype.bytelength
// get SharedArrayBuffer.prototype.byteLength
BUILTIN(SharedArrayBufferPrototypeGetByteLength) {
  const char* const kMethodName = "get SharedArrayBuffer.prototype.byteLength";
  HandleScope scope(isolate);
  // 1. Let O be the this value.
  // 2. Perform ? RequireInternalSlot(O, [[ArrayBufferData]]).
  CHECK_RECEIVER(JSArrayBuffer, array_buffer, kMethodName);
  // 3. If IsSharedArrayBuffer(O) is false, throw a TypeError exception.
  CHECK_SHARED(true, array_buffer, kMethodName);

  DCHECK_EQ(array_buffer->max_byte_length(),
            array_buffer->GetBackingStore()->max_byte_length());

  // 4. Let length be ArrayBufferByteLength(O, SeqCst).
  size_t byte_length = array_buffer->GetByteLength();
  // 5. Return F(length).
  return *isolate->factory()->NewNumberFromSize(byte_length);
}

// ES #sec-arraybuffer.prototype.resize
// ArrayBuffer.prototype.resize(new_size)
BUILTIN(ArrayBufferPrototypeResize) {
  const char* const kMethodName = "ArrayBuffer.prototype.resize";
  constexpr bool kIsShared = false;
  return ResizeHelper(args, isolate, kMethodName, kIsShared);
}

// ES #sec-arraybuffer.prototype.transfer
// ArrayBuffer.prototype.transfer([new_length])
BUILTIN(ArrayBufferPrototypeTransfer) {
  const char kMethodName[] = "ArrayBuffer.prototype.transfer";
  HandleScope scope(isolate);

  Handle<Object> new_length = args.atOrUndefined(isolate, 1);

  // 1. Let O be the this value.
  // 2. Perform ? RequireInternalSlot(O, [[ArrayBufferData]]).
  CHECK_RECEIVER(JSArrayBuffer, array_buffer, kMethodName);

  // 3. If IsSharedArrayBuffer(O) is true, throw a TypeError exception.
  CHECK_SHARED(false, array_buffer, kMethodName);

  size_t new_byte_length;
  if (new_length->IsUndefined(isolate)) {
    // 4. If newLength is undefined,
    //   a. If IsDetachedBuffer(O) is *true*, throw a *TypeError* exception.
    if (array_buffer->was_detached()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                                isolate->factory()->NewStringFromAsciiChecked(
                                    kMethodName)));
    }

    //   b. Let newByteLength be O.[[ArrayBufferByteLength]].
    new_byte_length = array_buffer->GetByteLength();
  } else {
    // 5. Else,
    //   a. Let newByteLength be ? ToIndex(newLength).
    Handle<Object> number_new_byte_length;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, number_new_byte_length,
                                       Object::ToInteger(isolate, new_length));
    if (number_new_byte_length->Number() < 0.0) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewRangeError(MessageTemplate::kInvalidArrayBufferLength));
    }
    if (!TryNumberToSize(*number_new_byte_length, &new_byte_length) ||
        new_byte_length > JSArrayBuffer::kMaxByteLength) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewRangeError(
              MessageTemplate::kInvalidArrayBufferResizeLength,
              isolate->factory()->NewStringFromAsciiChecked(kMethodName)));
    }

    //   b. If IsDetachedBuffer(O) is *true*, throw a *TypeError* exception.
    if (array_buffer->was_detached()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kDetachedOperation,
                                isolate->factory()->NewStringFromAsciiChecked(
                                    kMethodName)));
    }
  }

  // After this point the steps are not observable and are performed out of
  // spec order.

  if (!array_buffer->is_detachable()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError(MessageTemplate::kDataCloneErrorNonDetachableArrayBuffer));
  }

  // Case 1: We don't need a BackingStore.
  if (new_byte_length == 0) {
    // Nothing to do for steps 6-12.

    // 13. Perform ? DetachArrayBuffer(O).
    array_buffer->Detach();

    // 14. Return new.
    return *isolate->factory()
                ->NewJSArrayBufferAndBackingStore(
                    0, InitializedFlag::kUninitialized)
                .ToHandleChecked();
  }

  // Case 2: We can reuse the same BackingStore.
  auto from_backing_store = array_buffer->GetBackingStore();
  if (!from_backing_store->is_resizable() &&
      (new_byte_length == array_buffer->GetByteLength() ||
       from_backing_store->CanReallocate())) {
    // Reallocate covers steps 6-12.
    if (new_byte_length != array_buffer->GetByteLength() &&
        !from_backing_store->Reallocate(isolate, new_byte_length)) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewRangeError(MessageTemplate::kArrayBufferAllocationFailed));
    }

    // 13. Perform ? DetachArrayBuffer(O).
    array_buffer->Detach();

    // 14. Return new.
    return *isolate->factory()->NewJSArrayBuffer(std::move(from_backing_store));
  }

  // Case 3: We can't reuse the same BackingStore. Copy the buffer.

  // 6. Let new be ? Construct(%ArrayBuffer%, « 𝔽(newByteLength) »).
  // 7. NOTE: This method returns a fixed-length ArrayBuffer.
  Handle<JSArrayBuffer> new_;
  MaybeHandle<JSArrayBuffer> result =
      isolate->factory()->NewJSArrayBufferAndBackingStore(
          new_byte_length, InitializedFlag::kUninitialized);
  if (!result.ToHandle(&new_)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kArrayBufferAllocationFailed));
  }

  // 8. Let copyLength be min(newByteLength, O.[[ArrayBufferByteLength]]).
  // (Size comparison is done manually below instead of using min.)

  // 9. Let fromBlock be O.[[ArrayBufferData]].
  uint8_t* from_data =
      reinterpret_cast<uint8_t*>(array_buffer->backing_store());

  // 10. Let toBlock be new.[[ArrayBufferData]].
  uint8_t* to_data = reinterpret_cast<uint8_t*>(new_->backing_store());

  // 11. Perform CopyDataBlockBytes(toBlock, 0, fromBlock, 0, copyLength).
  // 12. NOTE: Neither creation of the new Data Block nor copying from the old
  // Data Block are observable. Implementations reserve the right to implement
  // this method as a zero-copy move or a realloc.
  size_t from_byte_length = array_buffer->GetByteLength();
  if (new_byte_length <= from_byte_length) {
    CopyBytes(to_data, from_data, new_byte_length);
  } else {
    CopyBytes(to_data, from_data, from_byte_length);
    memset(to_data + from_byte_length, 0, new_byte_length - from_byte_length);
  }

  // 13. Perform ? DetachArrayBuffer(O).
  array_buffer->Detach();

  // 14. Return new.
  return *new_;
}

// ES #sec-sharedarraybuffer.prototype.grow
// SharedArrayBuffer.prototype.grow(new_size)
BUILTIN(SharedArrayBufferPrototypeGrow) {
  const char* const kMethodName = "SharedArrayBuffer.prototype.grow";
  constexpr bool kIsShared = true;
  return ResizeHelper(args, isolate, kMethodName, kIsShared);
}

}  // namespace internal
}  // namespace v8
