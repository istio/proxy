// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_REMEMBERED_SET_INL_H_
#define V8_HEAP_REMEMBERED_SET_INL_H_

#include "src/codegen/assembler-inl.h"
#include "src/common/ptr-compr-inl.h"
#include "src/heap/remembered-set.h"
#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

template <typename Callback>
SlotCallbackResult UpdateTypedSlotHelper::UpdateTypedSlot(Heap* heap,
                                                          SlotType slot_type,
                                                          Address addr,
                                                          Callback callback) {
  switch (slot_type) {
    case SlotType::kCodeEntry: {
      RelocInfo rinfo(addr, RelocInfo::CODE_TARGET, 0, Code());
      return UpdateCodeTarget(&rinfo, callback);
    }
    case SlotType::kConstPoolCodeEntry: {
      return UpdateCodeEntry(addr, callback);
    }
    case SlotType::kEmbeddedObjectCompressed: {
      RelocInfo rinfo(addr, RelocInfo::COMPRESSED_EMBEDDED_OBJECT, 0, Code());
      return UpdateEmbeddedPointer(heap, &rinfo, callback);
    }
    case SlotType::kEmbeddedObjectFull: {
      RelocInfo rinfo(addr, RelocInfo::FULL_EMBEDDED_OBJECT, 0, Code());
      return UpdateEmbeddedPointer(heap, &rinfo, callback);
    }
    case SlotType::kEmbeddedObjectData: {
      RelocInfo rinfo(addr, RelocInfo::DATA_EMBEDDED_OBJECT, 0, Code());
      return UpdateEmbeddedPointer(heap, &rinfo, callback);
    }
    case SlotType::kConstPoolEmbeddedObjectCompressed: {
      HeapObject old_target = HeapObject::cast(Object(
          DecompressTaggedAny(heap->isolate(), base::Memory<Tagged_t>(addr))));
      HeapObject new_target = old_target;
      SlotCallbackResult result = callback(FullMaybeObjectSlot(&new_target));
      DCHECK(!HasWeakHeapObjectTag(new_target));
      if (new_target != old_target) {
        base::Memory<Tagged_t>(addr) = CompressTagged(new_target.ptr());
      }
      return result;
    }
    case SlotType::kConstPoolEmbeddedObjectFull: {
      return callback(FullMaybeObjectSlot(addr));
    }
    case SlotType::kCleared:
      break;
  }
  UNREACHABLE();
}

HeapObject UpdateTypedSlotHelper::GetTargetObject(Heap* heap,
                                                  SlotType slot_type,
                                                  Address addr) {
  switch (slot_type) {
    case SlotType::kCodeEntry: {
      RelocInfo rinfo(addr, RelocInfo::CODE_TARGET, 0, Code());
      return Code::GetCodeFromTargetAddress(rinfo.target_address());
    }
    case SlotType::kConstPoolCodeEntry: {
      return Code::GetObjectFromEntryAddress(addr);
    }
    case SlotType::kEmbeddedObjectCompressed: {
      RelocInfo rinfo(addr, RelocInfo::COMPRESSED_EMBEDDED_OBJECT, 0, Code());
      return rinfo.target_object(heap->isolate());
    }
    case SlotType::kEmbeddedObjectFull: {
      RelocInfo rinfo(addr, RelocInfo::FULL_EMBEDDED_OBJECT, 0, Code());
      return rinfo.target_object(heap->isolate());
    }
    case SlotType::kEmbeddedObjectData: {
      RelocInfo rinfo(addr, RelocInfo::DATA_EMBEDDED_OBJECT, 0, Code());
      return rinfo.target_object(heap->isolate());
    }
    case SlotType::kConstPoolEmbeddedObjectCompressed: {
      Address full =
          DecompressTaggedAny(heap->isolate(), base::Memory<Tagged_t>(addr));
      return HeapObject::cast(Object(full));
    }
    case SlotType::kConstPoolEmbeddedObjectFull: {
      FullHeapObjectSlot slot(addr);
      return (*slot).GetHeapObjectAssumeStrong(heap->isolate());
    }
    case SlotType::kCleared:
      break;
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
#endif  // V8_HEAP_REMEMBERED_SET_INL_H_
