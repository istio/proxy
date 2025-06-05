// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/concurrent-allocator.h"

#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/concurrent-allocator-inl.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/parked-scope.h"

namespace v8 {
namespace internal {

void StressConcurrentAllocatorTask::RunInternal() {
  Heap* heap = isolate_->heap();
  LocalHeap local_heap(heap, ThreadKind::kBackground);
  UnparkedScope unparked_scope(&local_heap);

  const int kNumIterations = 2000;
  const int kSmallObjectSize = 10 * kTaggedSize;
  const int kMediumObjectSize = 8 * KB;
  const int kLargeObjectSize =
      static_cast<int>(MemoryChunk::kPageSize -
                       MemoryChunkLayout::ObjectStartOffsetInDataPage());

  for (int i = 0; i < kNumIterations; i++) {
    // Isolate tear down started, stop allocation...
    if (heap->gc_state() == Heap::TEAR_DOWN) return;

    AllocationResult result = local_heap.AllocateRaw(
        kSmallObjectSize, AllocationType::kOld, AllocationOrigin::kRuntime,
        AllocationAlignment::kTaggedAligned);
    if (!result.IsFailure()) {
      heap->CreateFillerObjectAtBackground(result.ToAddress(),
                                           kSmallObjectSize);
    } else {
      local_heap.TryPerformCollection();
    }

    result = local_heap.AllocateRaw(kMediumObjectSize, AllocationType::kOld,
                                    AllocationOrigin::kRuntime,
                                    AllocationAlignment::kTaggedAligned);
    if (!result.IsFailure()) {
      heap->CreateFillerObjectAtBackground(result.ToAddress(),
                                           kMediumObjectSize);
    } else {
      local_heap.TryPerformCollection();
    }

    result = local_heap.AllocateRaw(kLargeObjectSize, AllocationType::kOld,
                                    AllocationOrigin::kRuntime,
                                    AllocationAlignment::kTaggedAligned);
    if (!result.IsFailure()) {
      heap->CreateFillerObjectAtBackground(result.ToAddress(),
                                           kLargeObjectSize);
    } else {
      local_heap.TryPerformCollection();
    }
    local_heap.Safepoint();
  }

  Schedule(isolate_);
}

// static
void StressConcurrentAllocatorTask::Schedule(Isolate* isolate) {
  auto task = std::make_unique<StressConcurrentAllocatorTask>(isolate);
  const double kDelayInSeconds = 0.1;
  V8::GetCurrentPlatform()->CallDelayedOnWorkerThread(std::move(task),
                                                      kDelayInSeconds);
}

void ConcurrentAllocator::FreeLinearAllocationArea() {
  // The code page of the linear allocation area needs to be unprotected
  // because we are going to write a filler into that memory area below.
  base::Optional<CodePageMemoryModificationScope> optional_scope;
  if (lab_.IsValid() && space_->identity() == CODE_SPACE) {
    optional_scope.emplace(MemoryChunk::FromAddress(lab_.top()));
  }
  lab_.CloseAndMakeIterable();
}

void ConcurrentAllocator::MakeLinearAllocationAreaIterable() {
  // The code page of the linear allocation area needs to be unprotected
  // because we are going to write a filler into that memory area below.
  base::Optional<CodePageMemoryModificationScope> optional_scope;
  if (lab_.IsValid() && space_->identity() == CODE_SPACE) {
    optional_scope.emplace(MemoryChunk::FromAddress(lab_.top()));
  }
  lab_.MakeIterable();
}

void ConcurrentAllocator::MarkLinearAllocationAreaBlack() {
  Address top = lab_.top();
  Address limit = lab_.limit();

  if (top != kNullAddress && top != limit) {
    base::Optional<CodePageHeaderModificationScope> optional_rwx_write_scope;
    if (space_->identity() == CODE_SPACE) {
      optional_rwx_write_scope.emplace(
          "Marking Code objects requires write access to the Code page header");
    }
    Page::FromAllocationAreaAddress(top)->CreateBlackAreaBackground(top, limit);
  }
}

void ConcurrentAllocator::UnmarkLinearAllocationArea() {
  Address top = lab_.top();
  Address limit = lab_.limit();

  if (top != kNullAddress && top != limit) {
    base::Optional<CodePageHeaderModificationScope> optional_rwx_write_scope;
    if (space_->identity() == CODE_SPACE) {
      optional_rwx_write_scope.emplace(
          "Marking Code objects requires write access to the Code page header");
    }
    Page::FromAllocationAreaAddress(top)->DestroyBlackAreaBackground(top,
                                                                     limit);
  }
}

AllocationResult ConcurrentAllocator::AllocateInLabSlow(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin) {
  if (!EnsureLab(origin)) {
    return AllocationResult::Failure();
  }
  AllocationResult allocation =
      lab_.AllocateRawAligned(size_in_bytes, alignment);
  DCHECK(!allocation.IsFailure());
  return allocation;
}

bool ConcurrentAllocator::EnsureLab(AllocationOrigin origin) {
  auto result = space_->RawAllocateBackground(local_heap_, kMinLabSize,
                                              kMaxLabSize, origin);
  if (!result) return false;

  if (IsBlackAllocationEnabled()) {
    Address top = result->first;
    Address limit = top + result->second;
    Page::FromAllocationAreaAddress(top)->CreateBlackAreaBackground(top, limit);
  }

  HeapObject object = HeapObject::FromAddress(result->first);
  LocalAllocationBuffer saved_lab = std::move(lab_);
  lab_ = LocalAllocationBuffer::FromResult(
      space_->heap(), AllocationResult::FromObject(object), result->second);
  DCHECK(lab_.IsValid());
  if (!lab_.TryMerge(&saved_lab)) {
    saved_lab.CloseAndMakeIterable();
  }
  return true;
}

AllocationResult ConcurrentAllocator::AllocateOutsideLab(
    int size_in_bytes, AllocationAlignment alignment, AllocationOrigin origin) {
  // Conservative estimate as we don't know the alignment of the allocation.
  const int requested_filler_size = Heap::GetMaximumFillToAlign(alignment);
  const int aligned_size_in_bytes = size_in_bytes + requested_filler_size;
  auto result = space_->RawAllocateBackground(
      local_heap_, aligned_size_in_bytes, aligned_size_in_bytes, origin);

  if (!result) return AllocationResult::Failure();
  DCHECK_GE(result->second, aligned_size_in_bytes);

  HeapObject object =
      (requested_filler_size)
          ? owning_heap()->AlignWithFiller(
                HeapObject::FromAddress(result->first), size_in_bytes,
                static_cast<int>(result->second), alignment)
          : HeapObject::FromAddress(result->first);
  if (IsBlackAllocationEnabled()) {
    owning_heap()->incremental_marking()->MarkBlackBackground(object,
                                                              size_in_bytes);
  }
  return AllocationResult::FromObject(object);
}

bool ConcurrentAllocator::IsBlackAllocationEnabled() const {
  return owning_heap()->incremental_marking()->black_allocation();
}

Heap* ConcurrentAllocator::owning_heap() const { return space_->heap(); }

}  // namespace internal
}  // namespace v8
