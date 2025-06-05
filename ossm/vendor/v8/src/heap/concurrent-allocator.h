// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CONCURRENT_ALLOCATOR_H_
#define V8_HEAP_CONCURRENT_ALLOCATOR_H_

#include "src/common/globals.h"
#include "src/heap/heap.h"
#include "src/heap/spaces.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class LocalHeap;

class StressConcurrentAllocatorTask : public CancelableTask {
 public:
  explicit StressConcurrentAllocatorTask(Isolate* isolate)
      : CancelableTask(isolate), isolate_(isolate) {}

  void RunInternal() override;

  // Schedules task on background thread
  static void Schedule(Isolate* isolate);

 private:
  Isolate* isolate_;
};

// Concurrent allocator for allocation from background threads/tasks.
// Allocations are served from a TLAB if possible.
class ConcurrentAllocator {
 public:
  static constexpr int kMinLabSize = 4 * KB;
  static constexpr int kMaxLabSize = 32 * KB;
  static constexpr int kMaxLabObjectSize = 2 * KB;

  ConcurrentAllocator(LocalHeap* local_heap, PagedSpace* space)
      : local_heap_(local_heap),
        space_(space),
        lab_(LocalAllocationBuffer::InvalidBuffer()) {}

  inline AllocationResult AllocateRaw(int object_size,
                                      AllocationAlignment alignment,
                                      AllocationOrigin origin);

  void FreeLinearAllocationArea();
  void MakeLinearAllocationAreaIterable();
  void MarkLinearAllocationAreaBlack();
  void UnmarkLinearAllocationArea();

 private:
  static_assert(
      kMinLabSize > kMaxLabObjectSize,
      "LAB size must be larger than max LAB object size as the fast "
      "paths do not consider alignment. The assumption is that any object with "
      "size <= kMaxLabObjectSize will fit into a newly allocated LAB of size "
      "kLabSize after computing the alignment requirements.");

  V8_EXPORT_PRIVATE AllocationResult
  AllocateInLabSlow(int size_in_bytes, AllocationAlignment alignment,
                    AllocationOrigin origin);
  bool EnsureLab(AllocationOrigin origin);

  V8_EXPORT_PRIVATE AllocationResult
  AllocateOutsideLab(int size_in_bytes, AllocationAlignment alignment,
                     AllocationOrigin origin);

  bool IsBlackAllocationEnabled() const;

  // Returns the Heap of space_. This might differ from the LocalHeap's Heap for
  // shared spaces.
  Heap* owning_heap() const;

  LocalHeap* const local_heap_;
  PagedSpace* const space_;
  LocalAllocationBuffer lab_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CONCURRENT_ALLOCATOR_H_
