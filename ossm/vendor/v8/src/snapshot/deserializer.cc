// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/deserializer.h"

#include "src/base/logging.h"
#include "src/codegen/assembler-inl.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap-inl.h"
#include "src/logging/local-logger.h"
#include "src/logging/log.h"
#include "src/objects/backing-store.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/objects/string.h"
#include "src/roots/roots.h"
#include "src/snapshot/embedded/embedded-data-inl.h"
#include "src/snapshot/references.h"
#include "src/snapshot/serializer-deserializer.h"
#include "src/snapshot/shared-heap-serializer.h"
#include "src/snapshot/snapshot-data.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

// A SlotAccessor for a slot in a HeapObject, which abstracts the slot
// operations done by the deserializer in a way which is GC-safe. In particular,
// rather than an absolute slot address, this accessor holds a Handle to the
// HeapObject, which is updated if the HeapObject moves.
class SlotAccessorForHeapObject {
 public:
  static SlotAccessorForHeapObject ForSlotIndex(Handle<HeapObject> object,
                                                int index) {
    return SlotAccessorForHeapObject(object, index * kTaggedSize);
  }
  static SlotAccessorForHeapObject ForSlotOffset(Handle<HeapObject> object,
                                                 int offset) {
    return SlotAccessorForHeapObject(object, offset);
  }

  MaybeObjectSlot slot() const { return object_->RawMaybeWeakField(offset_); }
  ExternalPointerSlot external_pointer_slot() const {
    return object_->RawExternalPointerField(offset_);
  }
  Handle<HeapObject> object() const { return object_; }
  int offset() const { return offset_; }

  // Writes the given value to this slot, optionally with an offset (e.g. for
  // repeat writes). Returns the number of slots written (which is one).
  int Write(MaybeObject value, int slot_offset = 0) {
    MaybeObjectSlot current_slot = slot() + slot_offset;
    current_slot.Relaxed_Store(value);
    CombinedWriteBarrier(*object_, current_slot, value, UPDATE_WRITE_BARRIER);
    return 1;
  }
  int Write(HeapObject value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    return Write(HeapObjectReference::From(value, ref_type), slot_offset);
  }
  int Write(Handle<HeapObject> value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    return Write(*value, ref_type, slot_offset);
  }

 private:
  SlotAccessorForHeapObject(Handle<HeapObject> object, int offset)
      : object_(object), offset_(offset) {}

  const Handle<HeapObject> object_;
  const int offset_;
};

// A SlotAccessor for absolute full slot addresses.
class SlotAccessorForRootSlots {
 public:
  explicit SlotAccessorForRootSlots(FullMaybeObjectSlot slot) : slot_(slot) {}

  FullMaybeObjectSlot slot() const { return slot_; }
  ExternalPointerSlot external_pointer_slot() const { UNREACHABLE(); }
  Handle<HeapObject> object() const { UNREACHABLE(); }
  int offset() const { UNREACHABLE(); }

  // Writes the given value to this slot, optionally with an offset (e.g. for
  // repeat writes). Returns the number of slots written (which is one).
  int Write(MaybeObject value, int slot_offset = 0) {
    FullMaybeObjectSlot current_slot = slot() + slot_offset;
    current_slot.Relaxed_Store(value);
    return 1;
  }
  int Write(HeapObject value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    return Write(HeapObjectReference::From(value, ref_type), slot_offset);
  }
  int Write(Handle<HeapObject> value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    return Write(*value, ref_type, slot_offset);
  }

 private:
  const FullMaybeObjectSlot slot_;
};

// A SlotAccessor for creating a Handle, which saves a Handle allocation when
// a Handle already exists.
template <typename IsolateT>
class SlotAccessorForHandle {
 public:
  SlotAccessorForHandle(Handle<HeapObject>* handle, IsolateT* isolate)
      : handle_(handle), isolate_(isolate) {}

  MaybeObjectSlot slot() const { UNREACHABLE(); }
  ExternalPointerSlot external_pointer_slot() const { UNREACHABLE(); }
  Handle<HeapObject> object() const { UNREACHABLE(); }
  int offset() const { UNREACHABLE(); }

  int Write(MaybeObject value, int slot_offset = 0) { UNREACHABLE(); }
  int Write(HeapObject value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    DCHECK_EQ(slot_offset, 0);
    DCHECK_EQ(ref_type, HeapObjectReferenceType::STRONG);
    *handle_ = handle(value, isolate_);
    return 1;
  }
  int Write(Handle<HeapObject> value, HeapObjectReferenceType ref_type,
            int slot_offset = 0) {
    DCHECK_EQ(slot_offset, 0);
    DCHECK_EQ(ref_type, HeapObjectReferenceType::STRONG);
    *handle_ = value;
    return 1;
  }

 private:
  Handle<HeapObject>* handle_;
  IsolateT* isolate_;
};

template <typename IsolateT>
template <typename TSlot>
int Deserializer<IsolateT>::WriteAddress(TSlot dest, Address value) {
  DCHECK(!next_reference_is_weak_);
  memcpy(dest.ToVoidPtr(), &value, kSystemPointerSize);
  static_assert(IsAligned(kSystemPointerSize, TSlot::kSlotDataSize));
  return (kSystemPointerSize / TSlot::kSlotDataSize);
}

template <typename IsolateT>
int Deserializer<IsolateT>::WriteExternalPointer(ExternalPointerSlot dest,
                                                 Address value,
                                                 ExternalPointerTag tag) {
  DCHECK(!next_reference_is_weak_);
  dest.init(main_thread_isolate(), value, tag);
  // ExternalPointers can only be written into HeapObject fields, therefore they
  // cover (kExternalPointerSlotSize / kTaggedSize) slots.
  return (kExternalPointerSlotSize / kTaggedSize);
}

namespace {
#ifdef DEBUG
int GetNumApiReferences(Isolate* isolate) {
  int num_api_references = 0;
  // The read-only deserializer is run by read-only heap set-up before the
  // heap is fully set up. External reference table relies on a few parts of
  // this set-up (like old-space), so it may be uninitialized at this point.
  if (isolate->isolate_data()->external_reference_table()->is_initialized()) {
    // Count the number of external references registered through the API.
    if (isolate->api_external_references() != nullptr) {
      while (isolate->api_external_references()[num_api_references] != 0) {
        num_api_references++;
      }
    }
  }
  return num_api_references;
}
int GetNumApiReferences(LocalIsolate* isolate) { return 0; }
#endif
}  // namespace

template <typename IsolateT>
Deserializer<IsolateT>::Deserializer(IsolateT* isolate,
                                     base::Vector<const byte> payload,
                                     uint32_t magic_number,
                                     bool deserializing_user_code,
                                     bool can_rehash)
    : isolate_(isolate),
      source_(payload),
      magic_number_(magic_number),
      deserializing_user_code_(deserializing_user_code),
      should_rehash_((v8_flags.rehash_snapshot && can_rehash) ||
                     deserializing_user_code) {
  DCHECK_NOT_NULL(isolate);
  isolate->RegisterDeserializerStarted();

  // We start the indices here at 1, so that we can distinguish between an
  // actual index and an empty backing store (serialized as
  // kEmptyBackingStoreRefSentinel) in a deserialized object requiring fix-up.
  static_assert(kEmptyBackingStoreRefSentinel == 0);
  backing_stores_.push_back({});

#ifdef DEBUG
  num_api_references_ = GetNumApiReferences(isolate);
#endif  // DEBUG
  CHECK_EQ(magic_number_, SerializedData::kMagicNumber);
}

template <typename IsolateT>
void Deserializer<IsolateT>::Rehash() {
  DCHECK(should_rehash());
  for (Handle<HeapObject> item : to_rehash_) {
    item->RehashBasedOnMap(isolate());
  }
}

template <typename IsolateT>
Deserializer<IsolateT>::~Deserializer() {
#ifdef DEBUG
  // Do not perform checks if we aborted deserialization.
  if (source_.position() == 0) return;
  // Check that we only have padding bytes remaining.
  while (source_.HasMore()) DCHECK_EQ(kNop, source_.Get());
  // Check that there are no remaining forward refs.
  DCHECK_EQ(num_unresolved_forward_refs_, 0);
  DCHECK(unresolved_forward_refs_.empty());
#endif  // DEBUG
  isolate_->RegisterDeserializerFinished();
}

// This is called on the roots.  It is the driver of the deserialization
// process.  It is also called on the body of each function.
template <typename IsolateT>
void Deserializer<IsolateT>::VisitRootPointers(Root root,
                                               const char* description,
                                               FullObjectSlot start,
                                               FullObjectSlot end) {
  ReadData(FullMaybeObjectSlot(start), FullMaybeObjectSlot(end));
}

template <typename IsolateT>
void Deserializer<IsolateT>::Synchronize(VisitorSynchronization::SyncTag tag) {
  static const byte expected = kSynchronize;
  CHECK_EQ(expected, source_.Get());
}

template <typename IsolateT>
void Deserializer<IsolateT>::DeserializeDeferredObjects() {
  for (int code = source_.Get(); code != kSynchronize; code = source_.Get()) {
    SnapshotSpace space = NewObject::Decode(code);
    ReadObject(space);
  }
}

template <typename IsolateT>
void Deserializer<IsolateT>::LogNewMapEvents() {
  if (V8_LIKELY(!v8_flags.log_maps)) return;
  DisallowGarbageCollection no_gc;
  for (Handle<Map> map : new_maps_) {
    DCHECK(v8_flags.log_maps);
    LOG(isolate(), MapCreate(*map));
    LOG(isolate(), MapDetails(*map));
  }
}

template <typename IsolateT>
void Deserializer<IsolateT>::WeakenDescriptorArrays() {
  DisallowGarbageCollection no_gc;
  Map descriptor_array_map = ReadOnlyRoots(isolate()).descriptor_array_map();
  for (Handle<DescriptorArray> descriptor_array : new_descriptor_arrays_) {
    DescriptorArray raw = *descriptor_array;
    DCHECK(raw.IsStrongDescriptorArray());
    raw.set_map_safe_transition(descriptor_array_map);
    WriteBarrier::Marking(raw, raw.number_of_descriptors());
  }
}

template <typename IsolateT>
void Deserializer<IsolateT>::LogScriptEvents(Script script) {
  DisallowGarbageCollection no_gc;
  LOG(isolate(),
      ScriptEvent(V8FileLogger::ScriptEventType::kDeserialize, script.id()));
  LOG(isolate(), ScriptDetails(script));
}

namespace {
template <typename IsolateT>
uint32_t ComputeRawHashField(IsolateT* isolate, String string) {
  // Make sure raw_hash_field() is computed.
  string.EnsureHash(SharedStringAccessGuardIfNeeded(isolate));
  return string.raw_hash_field();
}
}  // namespace

StringTableInsertionKey::StringTableInsertionKey(
    Isolate* isolate, Handle<String> string,
    DeserializingUserCodeOption deserializing_user_code)
    : StringTableKey(ComputeRawHashField(isolate, *string), string->length()),
      string_(string) {
#ifdef DEBUG
  deserializing_user_code_ = deserializing_user_code;
#endif
  DCHECK(string->IsInternalizedString());
}

StringTableInsertionKey::StringTableInsertionKey(
    LocalIsolate* isolate, Handle<String> string,
    DeserializingUserCodeOption deserializing_user_code)
    : StringTableKey(ComputeRawHashField(isolate, *string), string->length()),
      string_(string) {
#ifdef DEBUG
  deserializing_user_code_ = deserializing_user_code;
#endif
  DCHECK(string->IsInternalizedString());
}

template <typename IsolateT>
bool StringTableInsertionKey::IsMatch(IsolateT* isolate, String string) {
  // We want to compare the content of two strings here.
  return string_->SlowEquals(string, SharedStringAccessGuardIfNeeded(isolate));
}
template bool StringTableInsertionKey::IsMatch(Isolate* isolate, String string);
template bool StringTableInsertionKey::IsMatch(LocalIsolate* isolate,
                                               String string);

namespace {

void NoExternalReferencesCallback() {
  // The following check will trigger if a function or object template
  // with references to native functions have been deserialized from
  // snapshot, but no actual external references were provided when the
  // isolate was created.
  FATAL("No external references provided via API");
}

void PostProcessExternalString(ExternalString string, Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  uint32_t index = string.GetResourceRefForDeserialization();
  Address address =
      static_cast<Address>(isolate->api_external_references()[index]);
  string.InitExternalPointerFields(isolate);
  string.set_address_as_resource(isolate, address);
  isolate->heap()->UpdateExternalString(string, 0,
                                        string.ExternalPayloadSize());
  isolate->heap()->RegisterExternalString(string);
}

}  // namespace

template <typename IsolateT>
void Deserializer<IsolateT>::PostProcessNewJSReceiver(
    Map map, Handle<JSReceiver> obj, InstanceType instance_type,
    SnapshotSpace space) {
  DCHECK_EQ(map.instance_type(), instance_type);

  if (InstanceTypeChecker::IsJSDataView(instance_type)) {
    auto data_view = JSDataView::cast(*obj);
    auto buffer = JSArrayBuffer::cast(data_view.buffer());
    if (buffer.was_detached()) {
      // Directly set the data pointer to point to the EmptyBackingStoreBuffer.
      // Otherwise, we might end up setting it to EmptyBackingStoreBuffer() +
      // byte_offset() which would result in an invalid pointer.
      data_view.set_data_pointer(main_thread_isolate(),
                                 EmptyBackingStoreBuffer());
    } else {
      void* backing_store = buffer.backing_store();
      data_view.set_data_pointer(
          main_thread_isolate(),
          reinterpret_cast<uint8_t*>(backing_store) + data_view.byte_offset());
    }
  } else if (InstanceTypeChecker::IsJSTypedArray(instance_type)) {
    auto typed_array = JSTypedArray::cast(*obj);
    // Note: ByteArray objects must not be deferred s.t. they are
    // available here for is_on_heap(). See also: CanBeDeferred.
    // Fixup typed array pointers.
    if (typed_array.is_on_heap()) {
      typed_array.AddExternalPointerCompensationForDeserialization(
          main_thread_isolate());
    } else {
      // Serializer writes backing store ref as a DataPtr() value.
      uint32_t store_index =
          typed_array.GetExternalBackingStoreRefForDeserialization();
      auto backing_store = backing_stores_[store_index];
      void* start = backing_store ? backing_store->buffer_start() : nullptr;
      if (!start) start = EmptyBackingStoreBuffer();
      typed_array.SetOffHeapDataPtr(main_thread_isolate(), start,
                                    typed_array.byte_offset());
    }
  } else if (InstanceTypeChecker::IsJSArrayBuffer(instance_type)) {
    auto buffer = JSArrayBuffer::cast(*obj);
    uint32_t store_index = buffer.GetBackingStoreRefForDeserialization();
    if (store_index == kEmptyBackingStoreRefSentinel) {
      buffer.set_backing_store(main_thread_isolate(),
                               EmptyBackingStoreBuffer());
    } else {
      auto bs = backing_store(store_index);
      SharedFlag shared =
          bs && bs->is_shared() ? SharedFlag::kShared : SharedFlag::kNotShared;
      DCHECK_IMPLIES(bs, buffer.is_resizable() == bs->is_resizable());
      ResizableFlag resizable = bs && bs->is_resizable()
                                    ? ResizableFlag::kResizable
                                    : ResizableFlag::kNotResizable;
      buffer.Setup(shared, resizable, bs);
    }
  }
}

template <typename IsolateT>
void Deserializer<IsolateT>::PostProcessNewObject(Handle<Map> map,
                                                  Handle<HeapObject> obj,
                                                  SnapshotSpace space) {
  DisallowGarbageCollection no_gc;
  Map raw_map = *map;
  DCHECK_EQ(raw_map, obj->map(isolate_));
  InstanceType instance_type = raw_map.instance_type();
  HeapObject raw_obj = *obj;
  DCHECK_IMPLIES(deserializing_user_code(), should_rehash());
  if (should_rehash()) {
    if (InstanceTypeChecker::IsString(instance_type)) {
      // Uninitialize hash field as we need to recompute the hash.
      String string = String::cast(raw_obj);
      string.set_raw_hash_field(String::kEmptyHashField);
      // Rehash strings before read-only space is sealed. Strings outside
      // read-only space are rehashed lazily. (e.g. when rehashing dictionaries)
      if (space == SnapshotSpace::kReadOnlyHeap) {
        to_rehash_.push_back(obj);
      }
    } else if (raw_obj.NeedsRehashing(instance_type)) {
      to_rehash_.push_back(obj);
    }

    if (deserializing_user_code()) {
      if (InstanceTypeChecker::IsInternalizedString(instance_type)) {
        // Canonicalize the internalized string. If it already exists in the
        // string table, set the string to point to the existing one and patch
        // the deserialized string handle to point to the existing one.
        // TODO(leszeks): This handle patching is ugly, consider adding an
        // explicit internalized string bytecode. Also, the new thin string
        // should be dead, try immediately freeing it.
        Handle<String> string = Handle<String>::cast(obj);

        StringTableInsertionKey key(
            isolate(), string,
            DeserializingUserCodeOption::kIsDeserializingUserCode);
        String result = *isolate()->string_table()->LookupKey(isolate(), &key);

        if (result != raw_obj) {
          String::cast(raw_obj).MakeThin(isolate(), result);
          // Mutate the given object handle so that the backreference entry is
          // also updated.
          obj.PatchValue(result);
        }
        return;
      } else if (InstanceTypeChecker::IsScript(instance_type)) {
        new_scripts_.push_back(Handle<Script>::cast(obj));
      } else if (InstanceTypeChecker::IsAllocationSite(instance_type)) {
        // We should link new allocation sites, but we can't do this immediately
        // because |AllocationSite::HasWeakNext()| internally accesses
        // |Heap::roots_| that may not have been initialized yet. So defer this
        // to |ObjectDeserializer::CommitPostProcessedObjects()|.
        new_allocation_sites_.push_back(Handle<AllocationSite>::cast(obj));
      } else {
        // We dont defer ByteArray because JSTypedArray needs the base_pointer
        // ByteArray immediately if it's on heap.
        DCHECK(CanBeDeferred(*obj) ||
               InstanceTypeChecker::IsByteArray(instance_type));
      }
    }
  }

  if (InstanceTypeChecker::IsCode(instance_type)) {
    // We flush all code pages after deserializing the startup snapshot.
    // Hence we only remember each individual code object when deserializing
    // user code.
    if (deserializing_user_code()) {
      new_code_objects_.push_back(Handle<Code>::cast(obj));
    }
  } else if (V8_EXTERNAL_CODE_SPACE_BOOL &&
             InstanceTypeChecker::IsCodeDataContainer(instance_type)) {
    auto code_data_container = CodeDataContainer::cast(raw_obj);
    code_data_container.set_code_cage_base(isolate()->code_cage_base());
    code_data_container.init_code_entry_point(main_thread_isolate(),
                                              kNullAddress);
#ifdef V8_EXTERNAL_CODE_SPACE
    if (V8_REMOVE_BUILTINS_CODE_OBJECTS &&
        code_data_container.is_off_heap_trampoline()) {
      Address entry = OffHeapInstructionStart(code_data_container,
                                              code_data_container.builtin_id());
      code_data_container.SetEntryPointForOffHeapBuiltin(main_thread_isolate(),
                                                         entry);
    } else {
      code_data_container.UpdateCodeEntryPoint(main_thread_isolate(),
                                               code_data_container.code());
    }
#endif
  } else if (InstanceTypeChecker::IsMap(instance_type)) {
    if (v8_flags.log_maps) {
      // Keep track of all seen Maps to log them later since they might be only
      // partially initialized at this point.
      new_maps_.push_back(Handle<Map>::cast(obj));
    }
  } else if (InstanceTypeChecker::IsAccessorInfo(instance_type)) {
#ifdef USE_SIMULATOR
    accessor_infos_.push_back(Handle<AccessorInfo>::cast(obj));
#endif
  } else if (InstanceTypeChecker::IsCallHandlerInfo(instance_type)) {
#ifdef USE_SIMULATOR
    call_handler_infos_.push_back(Handle<CallHandlerInfo>::cast(obj));
#endif
  } else if (InstanceTypeChecker::IsExternalString(instance_type)) {
    PostProcessExternalString(ExternalString::cast(raw_obj),
                              main_thread_isolate());
  } else if (InstanceTypeChecker::IsJSReceiver(instance_type)) {
    // PostProcessNewJSReceiver may trigger GC.
    no_gc.Release();
    return PostProcessNewJSReceiver(raw_map, Handle<JSReceiver>::cast(obj),
                                    instance_type, space);
  } else if (InstanceTypeChecker::IsDescriptorArray(instance_type)) {
    DCHECK(InstanceTypeChecker::IsStrongDescriptorArray(instance_type));
    Handle<DescriptorArray> descriptors = Handle<DescriptorArray>::cast(obj);
    new_descriptor_arrays_.push_back(descriptors);
  } else if (InstanceTypeChecker::IsNativeContext(instance_type)) {
    NativeContext::cast(raw_obj).init_microtask_queue(main_thread_isolate(),
                                                      nullptr);
  } else if (InstanceTypeChecker::IsScript(instance_type)) {
    LogScriptEvents(Script::cast(*obj));
  }
}

template <typename IsolateT>
HeapObjectReferenceType Deserializer<IsolateT>::GetAndResetNextReferenceType() {
  HeapObjectReferenceType type = next_reference_is_weak_
                                     ? HeapObjectReferenceType::WEAK
                                     : HeapObjectReferenceType::STRONG;
  next_reference_is_weak_ = false;
  return type;
}

template <typename IsolateT>
Handle<HeapObject> Deserializer<IsolateT>::GetBackReferencedObject() {
  Handle<HeapObject> obj = back_refs_[source_.GetInt()];

  // We don't allow ThinStrings in backreferences -- if internalization produces
  // a thin string, then it should also update the backref handle.
  DCHECK(!obj->IsThinString(isolate()));

  hot_objects_.Add(obj);
  DCHECK(!HasWeakHeapObjectTag(*obj));
  return obj;
}

template <typename IsolateT>
Handle<HeapObject> Deserializer<IsolateT>::ReadObject() {
  Handle<HeapObject> ret;
  CHECK_EQ(ReadSingleBytecodeData(
               source_.Get(), SlotAccessorForHandle<IsolateT>(&ret, isolate())),
           1);
  return ret;
}

namespace {
AllocationType SpaceToAllocation(SnapshotSpace space) {
  switch (space) {
    case SnapshotSpace::kCode:
      return AllocationType::kCode;
    case SnapshotSpace::kMap:
      return AllocationType::kMap;
    case SnapshotSpace::kOld:
      return AllocationType::kOld;
    case SnapshotSpace::kReadOnlyHeap:
      return AllocationType::kReadOnly;
  }
}
}  // namespace

template <typename IsolateT>
Handle<HeapObject> Deserializer<IsolateT>::ReadObject(SnapshotSpace space) {
  const int size_in_tagged = source_.GetInt();
  const int size_in_bytes = size_in_tagged * kTaggedSize;

  // The map can't be a forward ref. If you want the map to be a forward ref,
  // then you're probably serializing the meta-map, in which case you want to
  // use the kNewMetaMap bytecode.
  DCHECK_NE(source()->Peek(), kRegisterPendingForwardRef);
  Handle<Map> map = Handle<Map>::cast(ReadObject());

  AllocationType allocation = SpaceToAllocation(space);

  // When sharing a string table, all in-place internalizable and internalized
  // strings internalized strings are allocated in the shared heap.
  //
  // TODO(12007): When shipping, add a new SharedOld SnapshotSpace.
  if (v8_flags.shared_string_table) {
    InstanceType instance_type = map->instance_type();
    if (InstanceTypeChecker::IsInternalizedString(instance_type) ||
        String::IsInPlaceInternalizable(instance_type)) {
      allocation = isolate()
                       ->factory()
                       ->RefineAllocationTypeForInPlaceInternalizableString(
                           allocation, *map);
    }
  }

  // Filling an object's fields can cause GCs and heap walks, so this object has
  // to be in a 'sufficiently initialised' state by the time the next allocation
  // can happen. For this to be the case, the object is carefully deserialized
  // as follows:
  //   * The space for the object is allocated.
  //   * The map is set on the object so that the GC knows what type the object
  //     has.
  //   * The rest of the object is filled with a fixed Smi value
  //     - This is a Smi so that tagged fields become initialized to a valid
  //       tagged value.
  //     - It's a fixed value, "Smi::uninitialized_deserialization_value()", so
  //       that we can DCHECK for it when reading objects that are assumed to be
  //       partially initialized objects.
  //   * The fields of the object are deserialized in order, under the
  //     assumption that objects are laid out in such a way that any fields
  //     required for object iteration (e.g. length fields) are deserialized
  //     before fields with objects.
  //     - We ensure this is the case by DCHECKing on object allocation that the
  //       previously allocated object has a valid size (see `Allocate`).
  HeapObject raw_obj =
      Allocate(allocation, size_in_bytes, HeapObject::RequiredAlignment(*map));
  raw_obj.set_map_after_allocation(*map);
  MemsetTagged(raw_obj.RawField(kTaggedSize),
               Smi::uninitialized_deserialization_value(), size_in_tagged - 1);
  DCHECK(raw_obj.CheckRequiredAlignment(isolate()));

  // Make sure BytecodeArrays have a valid age, so that the marker doesn't
  // break when making them older.
  if (raw_obj.IsBytecodeArray(isolate())) {
    BytecodeArray::cast(raw_obj).set_bytecode_age(0);
  } else if (raw_obj.IsEphemeronHashTable()) {
    // Make sure EphemeronHashTables have valid HeapObject keys, so that the
    // marker does not break when marking EphemeronHashTable, see
    // MarkingVisitorBase::VisitEphemeronHashTable.
    EphemeronHashTable table = EphemeronHashTable::cast(raw_obj);
    MemsetTagged(table.RawField(table.kElementsStartOffset),
                 ReadOnlyRoots(isolate()).undefined_value(),
                 (size_in_bytes - table.kElementsStartOffset) / kTaggedSize);
  }

#ifdef DEBUG
  PtrComprCageBase cage_base(isolate());
  // We want to make sure that all embedder pointers are initialized to null.
  if (raw_obj.IsJSObject(cage_base) &&
      JSObject::cast(raw_obj).MayHaveEmbedderFields()) {
    JSObject js_obj = JSObject::cast(raw_obj);
    for (int i = 0; i < js_obj.GetEmbedderFieldCount(); ++i) {
      void* pointer;
      CHECK(EmbedderDataSlot(js_obj, i).ToAlignedPointer(main_thread_isolate(),
                                                         &pointer));
      CHECK_NULL(pointer);
    }
  } else if (raw_obj.IsEmbedderDataArray(cage_base)) {
    EmbedderDataArray array = EmbedderDataArray::cast(raw_obj);
    EmbedderDataSlot start(array, 0);
    EmbedderDataSlot end(array, array.length());
    for (EmbedderDataSlot slot = start; slot < end; ++slot) {
      void* pointer;
      CHECK(slot.ToAlignedPointer(main_thread_isolate(), &pointer));
      CHECK_NULL(pointer);
    }
  }
#endif

  Handle<HeapObject> obj = handle(raw_obj, isolate());
  back_refs_.push_back(obj);

  ReadData(obj, 1, size_in_tagged);
  PostProcessNewObject(map, obj, space);

#ifdef DEBUG
  if (obj->IsCode(cage_base)) {
    DCHECK(space == SnapshotSpace::kCode ||
           space == SnapshotSpace::kReadOnlyHeap);
  } else {
    DCHECK_NE(space, SnapshotSpace::kCode);
  }
#endif  // DEBUG

  return obj;
}

template <typename IsolateT>
Handle<HeapObject> Deserializer<IsolateT>::ReadMetaMap() {
  const SnapshotSpace space = SnapshotSpace::kReadOnlyHeap;
  const int size_in_bytes = Map::kSize;
  const int size_in_tagged = size_in_bytes / kTaggedSize;

  HeapObject raw_obj =
      Allocate(SpaceToAllocation(space), size_in_bytes, kTaggedAligned);
  raw_obj.set_map_after_allocation(Map::unchecked_cast(raw_obj));
  MemsetTagged(raw_obj.RawField(kTaggedSize),
               Smi::uninitialized_deserialization_value(), size_in_tagged - 1);
  DCHECK(raw_obj.CheckRequiredAlignment(isolate()));

  Handle<HeapObject> obj = handle(raw_obj, isolate());
  back_refs_.push_back(obj);

  // Set the instance-type manually, to allow backrefs to read it.
  Map::unchecked_cast(*obj).set_instance_type(MAP_TYPE);

  ReadData(obj, 1, size_in_tagged);
  PostProcessNewObject(Handle<Map>::cast(obj), obj, space);

  return obj;
}

class DeserializerRelocInfoVisitor {
 public:
  DeserializerRelocInfoVisitor(Deserializer<Isolate>* deserializer,
                               const std::vector<Handle<HeapObject>>* objects)
      : deserializer_(deserializer), objects_(objects), current_object_(0) {}

  DeserializerRelocInfoVisitor(Deserializer<LocalIsolate>* deserializer,
                               const std::vector<Handle<HeapObject>>* objects) {
    UNREACHABLE();
  }

  ~DeserializerRelocInfoVisitor() {
    DCHECK_EQ(current_object_, objects_->size());
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo);
  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo);
  void VisitRuntimeEntry(Code host, RelocInfo* rinfo);
  void VisitExternalReference(Code host, RelocInfo* rinfo);
  void VisitInternalReference(Code host, RelocInfo* rinfo);
  void VisitOffHeapTarget(Code host, RelocInfo* rinfo);

 private:
  Isolate* isolate() { return deserializer_->isolate(); }
  SnapshotByteSource& source() { return deserializer_->source_; }

  Deserializer<Isolate>* deserializer_;
  const std::vector<Handle<HeapObject>>* objects_;
  int current_object_;
};

void DeserializerRelocInfoVisitor::VisitCodeTarget(Code host,
                                                   RelocInfo* rinfo) {
  HeapObject object = *objects_->at(current_object_++);
  rinfo->set_target_address(Code::cast(object).raw_instruction_start());
}

void DeserializerRelocInfoVisitor::VisitEmbeddedPointer(Code host,
                                                        RelocInfo* rinfo) {
  HeapObject object = *objects_->at(current_object_++);
  // Embedded object reference must be a strong one.
  rinfo->set_target_object(isolate()->heap(), object);
}

void DeserializerRelocInfoVisitor::VisitRuntimeEntry(Code host,
                                                     RelocInfo* rinfo) {
  // We no longer serialize code that contains runtime entries.
  UNREACHABLE();
}

void DeserializerRelocInfoVisitor::VisitExternalReference(Code host,
                                                          RelocInfo* rinfo) {
  byte data = source().Get();
  CHECK_EQ(data, Deserializer<Isolate>::kExternalReference);

  Address address = deserializer_->ReadExternalReferenceCase();

  if (rinfo->IsCodedSpecially()) {
    Address location_of_branch_data = rinfo->pc();
    Assembler::deserialization_set_special_target_at(location_of_branch_data,
                                                     host, address);
  } else {
    WriteUnalignedValue(rinfo->target_address_address(), address);
  }
}

void DeserializerRelocInfoVisitor::VisitInternalReference(Code host,
                                                          RelocInfo* rinfo) {
  byte data = source().Get();
  CHECK_EQ(data, Deserializer<Isolate>::kInternalReference);

  // Internal reference target is encoded as an offset from code entry.
  int target_offset = source().GetInt();
  // TODO(jgruber,v8:11036): We are being permissive for this DCHECK, but
  // consider using raw_instruction_size() instead of raw_body_size() in the
  // future.
  static_assert(Code::kOnHeapBodyIsContiguous);
  DCHECK_LT(static_cast<unsigned>(target_offset),
            static_cast<unsigned>(host.raw_body_size()));
  Address target = host.entry() + target_offset;
  Assembler::deserialization_set_target_internal_reference_at(
      rinfo->pc(), target, rinfo->rmode());
}

void DeserializerRelocInfoVisitor::VisitOffHeapTarget(Code host,
                                                      RelocInfo* rinfo) {
  // Currently we don't serialize code that contains near builtin entries.
  DCHECK_NE(rinfo->rmode(), RelocInfo::NEAR_BUILTIN_ENTRY);

  byte data = source().Get();
  CHECK_EQ(data, Deserializer<Isolate>::kOffHeapTarget);

  Builtin builtin = Builtins::FromInt(source().GetInt());

  CHECK_NOT_NULL(isolate()->embedded_blob_code());
  EmbeddedData d = EmbeddedData::FromBlob(isolate());
  Address address = d.InstructionStartOfBuiltin(builtin);
  CHECK_NE(kNullAddress, address);

  // TODO(ishell): implement RelocInfo::set_target_off_heap_target()
  if (RelocInfo::OffHeapTargetIsCodedSpecially()) {
    Address location_of_branch_data = rinfo->pc();
    Assembler::deserialization_set_special_target_at(location_of_branch_data,
                                                     host, address);
  } else {
    WriteUnalignedValue(rinfo->target_address_address(), address);
  }
}

template <typename IsolateT>
template <typename SlotAccessor>
int Deserializer<IsolateT>::ReadRepeatedObject(SlotAccessor slot_accessor,
                                               int repeat_count) {
  CHECK_LE(2, repeat_count);

  Handle<HeapObject> heap_object = ReadObject();
  DCHECK(!Heap::InYoungGeneration(*heap_object));
  for (int i = 0; i < repeat_count; i++) {
    // TODO(leszeks): Use a ranged barrier here.
    slot_accessor.Write(heap_object, HeapObjectReferenceType::STRONG, i);
  }
  return repeat_count;
}

namespace {

// Template used by the below CASE_RANGE macro to statically verify that the
// given number of cases matches the number of expected cases for that bytecode.
template <int byte_code_count, int expected>
constexpr byte VerifyBytecodeCount(byte bytecode) {
  static_assert(byte_code_count == expected);
  return bytecode;
}

}  // namespace

// Helper macro (and its implementation detail) for specifying a range of cases.
// Use as "case CASE_RANGE(byte_code, num_bytecodes):"
#define CASE_RANGE(byte_code, num_bytecodes) \
  CASE_R##num_bytecodes(                     \
      (VerifyBytecodeCount<byte_code##Count, num_bytecodes>(byte_code)))
#define CASE_R1(byte_code) byte_code
#define CASE_R2(byte_code) CASE_R1(byte_code) : case CASE_R1(byte_code + 1)
#define CASE_R3(byte_code) CASE_R2(byte_code) : case CASE_R1(byte_code + 2)
#define CASE_R4(byte_code) CASE_R2(byte_code) : case CASE_R2(byte_code + 2)
#define CASE_R8(byte_code) CASE_R4(byte_code) : case CASE_R4(byte_code + 4)
#define CASE_R16(byte_code) CASE_R8(byte_code) : case CASE_R8(byte_code + 8)
#define CASE_R32(byte_code) CASE_R16(byte_code) : case CASE_R16(byte_code + 16)

// This generates a case range for all the spaces.
#define CASE_RANGE_ALL_SPACES(bytecode)                           \
  SpaceEncoder<bytecode>::Encode(SnapshotSpace::kOld)             \
      : case SpaceEncoder<bytecode>::Encode(SnapshotSpace::kCode) \
      : case SpaceEncoder<bytecode>::Encode(SnapshotSpace::kMap)  \
      : case SpaceEncoder<bytecode>::Encode(SnapshotSpace::kReadOnlyHeap)

template <typename IsolateT>
void Deserializer<IsolateT>::ReadData(Handle<HeapObject> object,
                                      int start_slot_index,
                                      int end_slot_index) {
  int current = start_slot_index;
  while (current < end_slot_index) {
    byte data = source_.Get();
    current += ReadSingleBytecodeData(
        data, SlotAccessorForHeapObject::ForSlotIndex(object, current));
  }
  CHECK_EQ(current, end_slot_index);
}

template <typename IsolateT>
void Deserializer<IsolateT>::ReadData(FullMaybeObjectSlot start,
                                      FullMaybeObjectSlot end) {
  FullMaybeObjectSlot current = start;
  while (current < end) {
    byte data = source_.Get();
    current += ReadSingleBytecodeData(data, SlotAccessorForRootSlots(current));
  }
  CHECK_EQ(current, end);
}

template <typename IsolateT>
template <typename SlotAccessor>
int Deserializer<IsolateT>::ReadSingleBytecodeData(byte data,
                                                   SlotAccessor slot_accessor) {
  using TSlot = decltype(slot_accessor.slot());

  switch (data) {
    // Deserialize a new object and write a pointer to it to the current
    // object.
    case CASE_RANGE_ALL_SPACES(kNewObject): {
      SnapshotSpace space = NewObject::Decode(data);
      // Save the reference type before recursing down into reading the object.
      HeapObjectReferenceType ref_type = GetAndResetNextReferenceType();
      Handle<HeapObject> heap_object = ReadObject(space);
      return slot_accessor.Write(heap_object, ref_type);
    }

    // Find a recently deserialized object using its offset from the current
    // allocation point and write a pointer to it to the current object.
    case kBackref: {
      Handle<HeapObject> heap_object = GetBackReferencedObject();
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Reference an object in the read-only heap. This should be used when an
    // object is read-only, but is not a root.
    case kReadOnlyHeapRef: {
      DCHECK(isolate()->heap()->deserialization_complete());
      uint32_t chunk_index = source_.GetInt();
      uint32_t chunk_offset = source_.GetInt();

      ReadOnlySpace* read_only_space = isolate()->heap()->read_only_space();
      ReadOnlyPage* page = read_only_space->pages()[chunk_index];
      Address address = page->OffsetToAddress(chunk_offset);
      HeapObject heap_object = HeapObject::FromAddress(address);

      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Find an object in the roots array and write a pointer to it to the
    // current object.
    case kRootArray: {
      int id = source_.GetInt();
      RootIndex root_index = static_cast<RootIndex>(id);
      Handle<HeapObject> heap_object =
          Handle<HeapObject>::cast(isolate()->root_handle(root_index));
      hot_objects_.Add(heap_object);
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Find an object in the startup object cache and write a pointer to it to
    // the current object.
    case kStartupObjectCache: {
      int cache_index = source_.GetInt();
      // TODO(leszeks): Could we use the address of the startup_object_cache
      // entry as a Handle backing?
      HeapObject heap_object = HeapObject::cast(
          main_thread_isolate()->startup_object_cache()->at(cache_index));
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Find an object in the read-only object cache and write a pointer to it
    // to the current object.
    case kReadOnlyObjectCache: {
      int cache_index = source_.GetInt();
      // TODO(leszeks): Could we use the address of the cached_read_only_object
      // entry as a Handle backing?
      HeapObject heap_object = HeapObject::cast(
          isolate()->read_only_heap()->cached_read_only_object(cache_index));
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Find an object in the shared heap object cache and write a pointer to it
    // to the current object.
    case kSharedHeapObjectCache: {
      int cache_index = source_.GetInt();
      // TODO(leszeks): Could we use the address of the
      // shared_heap_object_cache entry as a Handle backing?
      HeapObject heap_object = HeapObject::cast(
          main_thread_isolate()->shared_heap_object_cache()->at(cache_index));
      DCHECK(
          SharedHeapSerializer::ShouldBeInSharedHeapObjectCache(heap_object));
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    // Deserialize a new meta-map and write a pointer to it to the current
    // object.
    case kNewMetaMap: {
      Handle<HeapObject> heap_object = ReadMetaMap();
      return slot_accessor.Write(heap_object, HeapObjectReferenceType::STRONG);
    }

    // Find an external reference and write a pointer to it to the current
    // object.
    case kSandboxedExternalReference:
    case kExternalReference: {
      DCHECK_IMPLIES(data == kSandboxedExternalReference,
                     V8_ENABLE_SANDBOX_BOOL);
      Address address = ReadExternalReferenceCase();
      ExternalPointerTag tag = kExternalPointerNullTag;
      if (data == kSandboxedExternalReference) {
        tag = ReadExternalPointerTag();
      }
      return WriteExternalPointer(slot_accessor.external_pointer_slot(),
                                  address, tag);
    }

    case kSandboxedRawExternalReference:
    case kRawExternalReference: {
      DCHECK_IMPLIES(data == kSandboxedExternalReference,
                     V8_ENABLE_SANDBOX_BOOL);
      Address address;
      source_.CopyRaw(&address, kSystemPointerSize);
      ExternalPointerTag tag = kExternalPointerNullTag;
      if (data == kSandboxedRawExternalReference) {
        tag = ReadExternalPointerTag();
      }
      return WriteExternalPointer(slot_accessor.external_pointer_slot(),
                                  address, tag);
    }

    case kInternalReference:
    case kOffHeapTarget:
      // These bytecodes are expected only during RelocInfo iteration.
      UNREACHABLE();

    // Find an object in the attached references and write a pointer to it to
    // the current object.
    case kAttachedReference: {
      int index = source_.GetInt();
      Handle<HeapObject> heap_object = attached_objects_[index];
      return slot_accessor.Write(heap_object, GetAndResetNextReferenceType());
    }

    case kNop:
      return 0;

    case kRegisterPendingForwardRef: {
      HeapObjectReferenceType ref_type = GetAndResetNextReferenceType();
      unresolved_forward_refs_.emplace_back(slot_accessor.object(),
                                            slot_accessor.offset(), ref_type);
      num_unresolved_forward_refs_++;
      return 1;
    }

    case kResolvePendingForwardRef: {
      // Pending forward refs can only be resolved after the heap object's map
      // field is deserialized; currently they only appear immediately after
      // the map field.
      DCHECK_EQ(slot_accessor.offset(), HeapObject::kHeaderSize);
      Handle<HeapObject> obj = slot_accessor.object();
      int index = source_.GetInt();
      auto& forward_ref = unresolved_forward_refs_[index];
      SlotAccessorForHeapObject::ForSlotOffset(forward_ref.object,
                                               forward_ref.offset)
          .Write(*obj, forward_ref.ref_type);
      num_unresolved_forward_refs_--;
      if (num_unresolved_forward_refs_ == 0) {
        // If there's no more pending fields, clear the entire pending field
        // vector.
        unresolved_forward_refs_.clear();
      } else {
        // Otherwise, at least clear the pending field.
        forward_ref.object = Handle<HeapObject>();
      }
      return 0;
    }

    case kSynchronize:
      // If we get here then that indicates that you have a mismatch between
      // the number of GC roots when serializing and deserializing.
      UNREACHABLE();

    // Deserialize raw data of variable length.
    case kVariableRawData: {
      // This operation is only supported for tagged-size slots, else we might
      // become misaligned.
      DCHECK_EQ(TSlot::kSlotDataSize, kTaggedSize);
      int size_in_tagged = source_.GetInt();
      // TODO(leszeks): Only copy slots when there are Smis in the serialized
      // data.
      source_.CopySlots(slot_accessor.slot().location(), size_in_tagged);
      return size_in_tagged;
    }

    // Deserialize raw code directly into the body of the code object.
    case kCodeBody: {
      // This operation is only supported for tagged-size slots, else we might
      // become misaligned.
      DCHECK_EQ(TSlot::kSlotDataSize, kTaggedSize);
      // CodeBody can only occur right after the heap object header.
      DCHECK_EQ(slot_accessor.offset(), HeapObject::kHeaderSize);

      int size_in_tagged = source_.GetInt();
      int size_in_bytes = size_in_tagged * kTaggedSize;

      {
        DisallowGarbageCollection no_gc;
        Code code = Code::cast(*slot_accessor.object());

        // First deserialize the code itself.
        source_.CopyRaw(
            reinterpret_cast<void*>(code.address() + Code::kDataStart),
            size_in_bytes);
      }

      // Then deserialize the code header
      ReadData(slot_accessor.object(), HeapObject::kHeaderSize / kTaggedSize,
               Code::kDataStart / kTaggedSize);

      // Then deserialize the pre-serialized RelocInfo objects.
      std::vector<Handle<HeapObject>> preserialized_objects;
      while (source_.Peek() != kSynchronize) {
        Handle<HeapObject> obj = ReadObject();
        preserialized_objects.push_back(obj);
      }
      // Skip the synchronize bytecode.
      source_.Advance(1);

      // Finally iterate RelocInfos (the same way it was done by the serializer)
      // and deserialize respective data into RelocInfos. The RelocIterator
      // holds a raw pointer to the code, so we have to disable garbage
      // collection here. It's ok though, any objects it would have needed are
      // in the preserialized_objects vector.
      {
        DisallowGarbageCollection no_gc;

        Code code = Code::cast(*slot_accessor.object());
        if (V8_EXTERNAL_CODE_SPACE_BOOL) {
          code.set_main_cage_base(isolate()->cage_base(), kRelaxedStore);
        }
        DeserializerRelocInfoVisitor visitor(this, &preserialized_objects);
        for (RelocIterator it(code, Code::BodyDescriptor::kRelocModeMask);
             !it.done(); it.next()) {
          it.rinfo()->Visit(&visitor);
        }
      }

      // Advance to the end of the code object.
      return (int{Code::kDataStart} - HeapObject::kHeaderSize) / kTaggedSize +
             size_in_tagged;
    }

    case kVariableRepeat: {
      int repeats = VariableRepeatCount::Decode(source_.GetInt());
      return ReadRepeatedObject(slot_accessor, repeats);
    }

    case kOffHeapBackingStore:
    case kOffHeapResizableBackingStore: {
      int byte_length = source_.GetInt();
      std::unique_ptr<BackingStore> backing_store;
      if (data == kOffHeapBackingStore) {
        backing_store = BackingStore::Allocate(
            main_thread_isolate(), byte_length, SharedFlag::kNotShared,
            InitializedFlag::kUninitialized);
      } else {
        int max_byte_length = source_.GetInt();
        size_t page_size, initial_pages, max_pages;
        Maybe<bool> result =
            JSArrayBuffer::GetResizableBackingStorePageConfiguration(
                nullptr, byte_length, max_byte_length, kDontThrow, &page_size,
                &initial_pages, &max_pages);
        DCHECK(result.FromJust());
        USE(result);
        backing_store = BackingStore::TryAllocateAndPartiallyCommitMemory(
            main_thread_isolate(), byte_length, max_byte_length, page_size,
            initial_pages, max_pages, WasmMemoryFlag::kNotWasm,
            SharedFlag::kNotShared);
      }
      CHECK_NOT_NULL(backing_store);
      source_.CopyRaw(backing_store->buffer_start(), byte_length);
      backing_stores_.push_back(std::move(backing_store));
      return 0;
    }

    case kSandboxedApiReference:
    case kApiReference: {
      DCHECK_IMPLIES(data == kSandboxedExternalReference,
                     V8_ENABLE_SANDBOX_BOOL);
      uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());
      Address address;
      if (main_thread_isolate()->api_external_references()) {
        DCHECK_WITH_MSG(reference_id < num_api_references_,
                        "too few external references provided through the API");
        address = static_cast<Address>(
            main_thread_isolate()->api_external_references()[reference_id]);
      } else {
        address = reinterpret_cast<Address>(NoExternalReferencesCallback);
      }
      ExternalPointerTag tag = kExternalPointerNullTag;
      if (data == kSandboxedApiReference) {
        tag = ReadExternalPointerTag();
      }
      return WriteExternalPointer(slot_accessor.external_pointer_slot(),
                                  address, tag);
    }

    case kClearedWeakReference:
      return slot_accessor.Write(HeapObjectReference::ClearedValue(isolate()));

    case kWeakPrefix: {
      // We shouldn't have two weak prefixes in a row.
      DCHECK(!next_reference_is_weak_);
      // We shouldn't have weak refs without a current object.
      DCHECK_NE(slot_accessor.object()->address(), kNullAddress);
      next_reference_is_weak_ = true;
      return 0;
    }

    case CASE_RANGE(kRootArrayConstants, 32): {
      // First kRootArrayConstantsCount roots are guaranteed to be in
      // the old space.
      static_assert(static_cast<int>(RootIndex::kFirstImmortalImmovableRoot) ==
                    0);
      static_assert(kRootArrayConstantsCount <=
                    static_cast<int>(RootIndex::kLastImmortalImmovableRoot));

      RootIndex root_index = RootArrayConstant::Decode(data);
      Handle<HeapObject> heap_object =
          Handle<HeapObject>::cast(isolate()->root_handle(root_index));
      return slot_accessor.Write(heap_object, HeapObjectReferenceType::STRONG);
    }

    case CASE_RANGE(kHotObject, 8): {
      int index = HotObject::Decode(data);
      Handle<HeapObject> hot_object = hot_objects_.Get(index);
      return slot_accessor.Write(hot_object, GetAndResetNextReferenceType());
    }

    case CASE_RANGE(kFixedRawData, 32): {
      // Deserialize raw data of fixed length from 1 to 32 times kTaggedSize.
      int size_in_tagged = FixedRawDataWithSize::Decode(data);
      static_assert(TSlot::kSlotDataSize == kTaggedSize ||
                    TSlot::kSlotDataSize == 2 * kTaggedSize);
      int size_in_slots = size_in_tagged / (TSlot::kSlotDataSize / kTaggedSize);
      // kFixedRawData can have kTaggedSize != TSlot::kSlotDataSize when
      // serializing Smi roots in pointer-compressed builds. In this case, the
      // size in bytes is unconditionally the (full) slot size.
      DCHECK_IMPLIES(kTaggedSize != TSlot::kSlotDataSize, size_in_slots == 1);
      // TODO(leszeks): Only copy slots when there are Smis in the serialized
      // data.
      source_.CopySlots(slot_accessor.slot().location(), size_in_slots);
      return size_in_slots;
    }

    case CASE_RANGE(kFixedRepeat, 16): {
      int repeats = FixedRepeatWithCount::Decode(data);
      return ReadRepeatedObject(slot_accessor, repeats);
    }

#ifdef DEBUG
#define UNUSED_CASE(byte_code) \
  case byte_code:              \
    UNREACHABLE();
      UNUSED_SERIALIZER_BYTE_CODES(UNUSED_CASE)
#endif
#undef UNUSED_CASE
  }

  // The above switch, including UNUSED_SERIALIZER_BYTE_CODES, covers all
  // possible bytecodes; but, clang doesn't realize this, so we have an explicit
  // UNREACHABLE here too.
  UNREACHABLE();
}

#undef CASE_RANGE_ALL_SPACES
#undef CASE_RANGE
#undef CASE_R32
#undef CASE_R16
#undef CASE_R8
#undef CASE_R4
#undef CASE_R3
#undef CASE_R2
#undef CASE_R1

template <typename IsolateT>
Address Deserializer<IsolateT>::ReadExternalReferenceCase() {
  uint32_t reference_id = static_cast<uint32_t>(source_.GetInt());
  return main_thread_isolate()->external_reference_table()->address(
      reference_id);
}

template <typename IsolateT>
ExternalPointerTag Deserializer<IsolateT>::ReadExternalPointerTag() {
  uint64_t shifted_tag = static_cast<uint64_t>(source_.GetInt());
  return static_cast<ExternalPointerTag>(shifted_tag
                                         << kExternalPointerTagShift);
}

template <typename IsolateT>
HeapObject Deserializer<IsolateT>::Allocate(AllocationType allocation, int size,
                                            AllocationAlignment alignment) {
#ifdef DEBUG
  if (!previous_allocation_obj_.is_null()) {
    // Make sure that the previous object is initialized sufficiently to
    // be iterated over by the GC.
    int object_size = previous_allocation_obj_->Size(isolate_);
    DCHECK_LE(object_size, previous_allocation_size_);
  }
#endif

  HeapObject obj = HeapObject::FromAddress(isolate()->heap()->AllocateRawOrFail(
      size, allocation, AllocationOrigin::kRuntime, alignment));

#ifdef DEBUG
  previous_allocation_obj_ = handle(obj, isolate());
  previous_allocation_size_ = size;
#endif

  return obj;
}

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) Deserializer<Isolate>;
template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Deserializer<LocalIsolate>;

}  // namespace internal
}  // namespace v8
