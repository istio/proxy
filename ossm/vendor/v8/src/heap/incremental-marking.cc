// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/incremental-marking.h"

#include "src/codegen/compilation-cache.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/global-handles.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/incremental-marking-job.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-barrier.h"
#include "src/heap/marking-visitor-inl.h"
#include "src/heap/marking-visitor.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/safepoint.h"
#include "src/init/v8.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/numbers/conversions.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/slots-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/objects/visitors.h"
#include "src/tracing/trace-event.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

void IncrementalMarking::Observer::Step(int bytes_allocated, Address addr,
                                        size_t size) {
  Heap* heap = incremental_marking_->heap();
  VMState<GC> state(heap->isolate());
  RCS_SCOPE(heap->isolate(),
            RuntimeCallCounterId::kGC_Custom_IncrementalMarkingObserver);
  incremental_marking_->AdvanceOnAllocation();
}

IncrementalMarking::IncrementalMarking(Heap* heap, WeakObjects* weak_objects)
    : heap_(heap),
      major_collector_(heap->mark_compact_collector()),
      minor_collector_(heap->minor_mark_compact_collector()),
      weak_objects_(weak_objects),
      incremental_marking_job_(heap),
      new_generation_observer_(this, kYoungGenerationAllocatedThreshold),
      old_generation_observer_(this, kOldGenerationAllocatedThreshold),
      marking_state_(heap->isolate()),
      atomic_marking_state_(heap->isolate()),
      non_atomic_marking_state_(heap->isolate()) {}

void IncrementalMarking::MarkBlackAndVisitObjectDueToLayoutChange(
    HeapObject obj) {
  // TODO(v8:13012): Add scope for MinorMC.
  TRACE_EVENT0("v8", "V8.GCIncrementalMarkingLayoutChange");
  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_INCREMENTAL_LAYOUT_CHANGE);
  marking_state()->WhiteToGrey(obj);
  if (IsMajorMarking()) {
    major_collector_->VisitObject(obj);
  } else {
    // Not covered by tests.
    minor_collector_->VisitObject(obj);
  }
}

void IncrementalMarking::MarkBlackBackground(HeapObject obj, int object_size) {
  MarkBit mark_bit = atomic_marking_state()->MarkBitFrom(obj);
  Marking::MarkBlack<AccessMode::ATOMIC>(mark_bit);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(obj);
  IncrementLiveBytesBackground(chunk, static_cast<intptr_t>(object_size));
}

void IncrementalMarking::NotifyLeftTrimming(HeapObject from, HeapObject to) {
  if (!IsMarking()) return;

  DCHECK(MemoryChunk::FromHeapObject(from)->SweepingDone());
  DCHECK_EQ(MemoryChunk::FromHeapObject(from), MemoryChunk::FromHeapObject(to));
  DCHECK_NE(from, to);

  MarkBit new_mark_bit = marking_state()->MarkBitFrom(to);

  if (black_allocation() &&
      Marking::IsBlack<AccessMode::ATOMIC>(new_mark_bit)) {
    // Nothing to do if the object is in black area.
    return;
  }
  MarkBlackAndVisitObjectDueToLayoutChange(from);
  DCHECK(marking_state()->IsBlack(from));
  // Mark the new address as black.
  if (from.address() + kTaggedSize == to.address()) {
    // The old and the new markbits overlap. The |to| object has the
    // grey color. To make it black, we need to set the second bit.
    DCHECK(new_mark_bit.Get<AccessMode::ATOMIC>());
    new_mark_bit.Next().Set<AccessMode::ATOMIC>();
  } else {
    bool success = Marking::WhiteToBlack<AccessMode::ATOMIC>(new_mark_bit);
    DCHECK(success);
    USE(success);
  }
  DCHECK(marking_state()->IsBlack(to));
}

bool IncrementalMarking::CanBeStarted() const {
  // Only start incremental marking in a safe state:
  //   1) when incremental marking is turned on
  //   2) when we are currently not in a GC, and
  //   3) when we are currently not serializing or deserializing the heap, and
  //   4) not a shared heap.
  return v8_flags.incremental_marking && heap_->gc_state() == Heap::NOT_IN_GC &&
         heap_->deserialization_complete() &&
         !heap_->isolate()->serializer_enabled() && !heap_->IsShared();
}

bool IncrementalMarking::IsBelowActivationThresholds() const {
  return heap_->OldGenerationSizeOfObjects() <= kV8ActivationThreshold &&
         heap_->EmbedderSizeOfObjects() <= kEmbedderActivationThreshold;
}

void IncrementalMarking::Start(GarbageCollector garbage_collector,
                               GarbageCollectionReason gc_reason) {
  DCHECK(!major_collector_->sweeping_in_progress());
  DCHECK(!heap_->IsShared());

  if (v8_flags.trace_incremental_marking) {
    const size_t old_generation_size_mb =
        heap()->OldGenerationSizeOfObjects() / MB;
    const size_t old_generation_limit_mb =
        heap()->old_generation_allocation_limit() / MB;
    const size_t global_size_mb = heap()->GlobalSizeOfObjects() / MB;
    const size_t global_limit_mb = heap()->global_allocation_limit() / MB;
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Start (%s): (size/limit/slack) v8: %zuMB / %zuMB "
        "/ %zuMB global: %zuMB / %zuMB / %zuMB\n",
        Heap::GarbageCollectionReasonToString(gc_reason),
        old_generation_size_mb, old_generation_limit_mb,
        old_generation_size_mb > old_generation_limit_mb
            ? 0
            : old_generation_limit_mb - old_generation_size_mb,
        global_size_mb, global_limit_mb,
        global_size_mb > global_limit_mb ? 0
                                         : global_limit_mb - global_size_mb);
  }
  DCHECK(v8_flags.incremental_marking);
  DCHECK(IsStopped());
  DCHECK_EQ(heap_->gc_state(), Heap::NOT_IN_GC);
  DCHECK(!heap_->isolate()->serializer_enabled());

  Counters* counters = heap_->isolate()->counters();

  counters->incremental_marking_reason()->AddSample(
      static_cast<int>(gc_reason));
  NestedTimedHistogramScope incremental_marking_scope(
      counters->gc_incremental_marking_start());
  TRACE_EVENT1(
      "v8", "V8.GCIncrementalMarkingStart", "epoch",
      heap_->tracer()->CurrentEpoch(GCTracer::Scope::MC_INCREMENTAL_START));
  TRACE_GC_EPOCH(heap()->tracer(), GCTracer::Scope::MC_INCREMENTAL_START,
                 ThreadKind::kMain);
  heap_->tracer()->NotifyIncrementalMarkingStart();

  start_time_ms_ = heap()->MonotonicallyIncreasingTimeInMs();
  completion_task_scheduled_ = false;
  completion_task_timeout_ = 0.0;
  initial_old_generation_size_ = heap_->OldGenerationSizeOfObjects();
  old_generation_allocation_counter_ = heap_->OldGenerationAllocationCounter();
  bytes_marked_ = 0;
  scheduled_bytes_to_mark_ = 0;
  schedule_update_time_ms_ = start_time_ms_;
  bytes_marked_concurrently_ = 0;

  if (garbage_collector == GarbageCollector::MARK_COMPACTOR) {
    current_collector_ = CurrentCollector::kMajorMC;
    StartMarkingMajor();
    heap_->AddAllocationObserversToAllSpaces(&old_generation_observer_,
                                             &new_generation_observer_);
    incremental_marking_job()->ScheduleTask();
  } else {
    current_collector_ = CurrentCollector::kMinorMC;
    // Allocation observers are not currently used by MinorMC because we don't
    // do incremental marking.
    StartMarkingMinor();
  }
}

bool IncrementalMarking::WhiteToGreyAndPush(HeapObject obj) {
  if (marking_state()->WhiteToGrey(obj)) {
    local_marking_worklists()->Push(obj);
    return true;
  }
  return false;
}

class IncrementalMarking::IncrementalMarkingRootMarkingVisitor final
    : public RootVisitor {
 public:
  explicit IncrementalMarkingRootMarkingVisitor(Heap* heap)
      : heap_(heap), incremental_marking_(heap->incremental_marking()) {}

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) override {
    DCHECK(!MapWord::IsPacked((*p).ptr()));
    MarkObjectByPointer(root, p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p) {
      DCHECK(!MapWord::IsPacked((*p).ptr()));
      MarkObjectByPointer(root, p);
    }
  }

 private:
  void MarkObjectByPointer(Root root, FullObjectSlot p) {
    Object object = *p;
    if (!object.IsHeapObject()) return;
    DCHECK(!MapWord::IsPacked(object.ptr()));
    HeapObject heap_object = HeapObject::cast(object);

    if (heap_object.InSharedHeap()) return;

    if (incremental_marking_->IsMajorMarking()) {
      if (incremental_marking_->WhiteToGreyAndPush(heap_object)) {
        if (V8_UNLIKELY(v8_flags.track_retaining_path)) {
          heap_->AddRetainingRoot(root, heap_object);
        }
      }
    } else if (Heap::InYoungGeneration(heap_object)) {
      incremental_marking_->WhiteToGreyAndPush(heap_object);
    }
  }

  Heap* const heap_;
  IncrementalMarking* const incremental_marking_;
};

void IncrementalMarking::MarkRoots() {
  IncrementalMarkingRootMarkingVisitor visitor(heap_);
  CodePageHeaderModificationScope rwx_write_scope(
      "Marking of builtins table entries require write access to Code page "
      "header");
  if (IsMajorMarking()) {
    heap_->IterateRoots(
        &visitor,
        base::EnumSet<SkipRoot>{SkipRoot::kStack, SkipRoot::kMainThreadHandles,
                                SkipRoot::kWeak});
  } else {
    heap_->IterateRoots(
        &visitor, base::EnumSet<SkipRoot>{
                      SkipRoot::kStack, SkipRoot::kMainThreadHandles,
                      SkipRoot::kWeak, SkipRoot::kExternalStringTable,
                      SkipRoot::kGlobalHandles, SkipRoot::kOldGeneration});

    heap()->isolate()->global_handles()->IterateYoungStrongAndDependentRoots(
        &visitor);
    // TODO(v8:13012): Do PageMarkingItem processing.
  }
}

void IncrementalMarking::MarkRootsForTesting() { MarkRoots(); }

void IncrementalMarking::StartMarkingMajor() {
  if (heap_->isolate()->serializer_enabled()) {
    // Black allocation currently starts when we start incremental marking,
    // but we cannot enable black allocation while deserializing. Hence, we
    // have to delay the start of incremental marking in that case.
    if (v8_flags.trace_incremental_marking) {
      heap()->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Start delayed - serializer\n");
    }
    return;
  }
  if (v8_flags.trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Start marking\n");
  }

  heap_->InvokeIncrementalMarkingPrologueCallbacks();

  is_compacting_ = major_collector_->StartCompaction(
      MarkCompactCollector::StartCompactionMode::kIncremental);

#ifdef V8_COMPRESS_POINTERS
  heap_->isolate()->external_pointer_table().StartCompactingIfNeeded();
#endif  // V8_COMPRESS_POINTERS

  auto embedder_flags = heap_->flags_for_embedder_tracer();
  {
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_INCREMENTAL_EMBEDDER_PROLOGUE);
    // PrepareForTrace should be called before visitor initialization in
    // StartMarking. It is only used with CppHeap.
    heap_->local_embedder_heap_tracer()->PrepareForTrace(embedder_flags);
  }

  major_collector_->StartMarking();
  current_local_marking_worklists = major_collector_->local_marking_worklists();

  is_marking_ = true;
  heap_->SetIsMarkingFlag(true);

  MarkingBarrier::ActivateAll(heap(), is_compacting_,
                              MarkingBarrierType::kMajor);
  GlobalHandles::EnableMarkingBarrier(heap()->isolate());

  heap_->isolate()->compilation_cache()->MarkCompactPrologue();

  StartBlackAllocation();

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_MARK_ROOTS);
    MarkRoots();
  }

  if (v8_flags.concurrent_marking && !heap_->IsTearingDown()) {
    heap_->concurrent_marking()->ScheduleJob(GarbageCollector::MARK_COMPACTOR);
  }

  // Ready to start incremental marking.
  if (v8_flags.trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp("[IncrementalMarking] Running\n");
  }

  {
    // TracePrologue may call back into V8 in corner cases, requiring that
    // marking (including write barriers) is fully set up.
    TRACE_GC(heap()->tracer(),
             GCTracer::Scope::MC_INCREMENTAL_EMBEDDER_PROLOGUE);
    heap_->local_embedder_heap_tracer()->TracePrologue(embedder_flags);
  }

  heap_->InvokeIncrementalMarkingEpilogueCallbacks();
}

void IncrementalMarking::StartMarkingMinor() {
  // Removed serializer_enabled() check because we don't do black allocation.

  if (v8_flags.trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] (MinorMC) Start marking\n");
  }

  minor_collector_->StartMarking();
  current_local_marking_worklists = minor_collector_->local_marking_worklists();

  is_marking_ = true;
  heap_->SetIsMarkingFlag(true);
  heap_->SetIsMinorMarkingFlag(true);

  MarkingBarrier::ActivateAll(heap(), false, MarkingBarrierType::kMinor);

  {
    TRACE_GC(heap()->tracer(), GCTracer::Scope::MINOR_MC_MARK_ROOTS);
    MarkRoots();
  }

  if (v8_flags.concurrent_marking && !heap_->IsTearingDown()) {
    heap_->concurrent_marking()->ScheduleJob(
        GarbageCollector::MINOR_MARK_COMPACTOR);
  }

  if (v8_flags.trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] (MinorMC) Running\n");
  }
}

void IncrementalMarking::StartBlackAllocation() {
  DCHECK(!black_allocation_);
  DCHECK(IsMarking());
  black_allocation_ = true;
  heap()->old_space()->MarkLinearAllocationAreaBlack();
  if (heap()->map_space()) heap()->map_space()->MarkLinearAllocationAreaBlack();
  {
    CodePageHeaderModificationScope rwx_write_scope(
        "Marking Code objects requires write access to the Code page header");
    heap()->code_space()->MarkLinearAllocationAreaBlack();
  }
  heap()->safepoint()->IterateLocalHeaps([](LocalHeap* local_heap) {
    local_heap->MarkLinearAllocationAreaBlack();
  });
  if (v8_flags.trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Black allocation started\n");
  }
}

void IncrementalMarking::PauseBlackAllocation() {
  DCHECK(IsMarking());
  heap()->old_space()->UnmarkLinearAllocationArea();
  if (heap()->map_space()) heap()->map_space()->UnmarkLinearAllocationArea();
  {
    CodePageHeaderModificationScope rwx_write_scope(
        "Marking Code objects requires write access to the Code page header");
    heap()->code_space()->UnmarkLinearAllocationArea();
  }
  heap()->safepoint()->IterateLocalHeaps(
      [](LocalHeap* local_heap) { local_heap->UnmarkLinearAllocationArea(); });
  if (v8_flags.trace_incremental_marking) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Black allocation paused\n");
  }
  black_allocation_ = false;
}

void IncrementalMarking::FinishBlackAllocation() {
  if (black_allocation_) {
    black_allocation_ = false;
    if (v8_flags.trace_incremental_marking) {
      heap()->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Black allocation finished\n");
    }
  }
}

void IncrementalMarking::UpdateMarkingWorklistAfterYoungGenGC() {
  if (!IsMarking()) return;
  DCHECK(!v8_flags.separate_gc_phases);
  DCHECK(IsMajorMarking());

  Map filler_map = ReadOnlyRoots(heap_).one_pointer_filler_map();

  MarkingState* minor_marking_state =
      heap()->minor_mark_compact_collector()->marking_state();

  major_collector_->local_marking_worklists()->Publish();
  MarkingBarrier::PublishAll(heap());
  PtrComprCageBase cage_base(heap_->isolate());
  major_collector_->marking_worklists()->Update([this, minor_marking_state,
                                                 cage_base, filler_map](
                                                    HeapObject obj,
                                                    HeapObject* out) -> bool {
    DCHECK(obj.IsHeapObject());
    // Only pointers to from space have to be updated.
    if (Heap::InFromPage(obj)) {
      DCHECK(!v8_flags.minor_mc);
      MapWord map_word = obj.map_word(cage_base, kRelaxedLoad);
      if (!map_word.IsForwardingAddress()) {
        // There may be objects on the marking deque that do not exist
        // anymore, e.g. left trimmed objects or objects from the root set
        // (frames). If these object are dead at scavenging time, their
        // marking deque entries will not point to forwarding addresses.
        // Hence, we can discard them.
        return false;
      }
      HeapObject dest = map_word.ToForwardingAddress();
      USE(this);
      DCHECK_IMPLIES(marking_state()->IsWhite(obj), obj.IsFreeSpaceOrFiller());
      if (dest.InSharedHeap()) {
        // Object got promoted into the shared heap. Drop it from the client
        // heap marking worklist.
        return false;
      }
      *out = dest;
      return true;
    } else if (Heap::InToPage(obj)) {
      // The object may be on a large page or on a page that was moved in
      // new space.
      DCHECK(Heap::IsLargeObject(obj) || Page::FromHeapObject(obj)->IsFlagSet(
                                             Page::PAGE_NEW_NEW_PROMOTION));
      DCHECK_IMPLIES(v8_flags.minor_mc, !Page::FromHeapObject(obj)->IsFlagSet(
                                            Page::PAGE_NEW_NEW_PROMOTION));
      DCHECK_IMPLIES(
          v8_flags.minor_mc,
          !obj.map_word(cage_base, kRelaxedLoad).IsForwardingAddress());
      if (minor_marking_state->IsWhite(obj)) {
        return false;
      }
      // Either a large object or an object marked by the minor
      // mark-compactor.
      *out = obj;
      return true;
    } else {
      // The object may be on a page that was moved from new to old space.
      // Only applicable during minor MC garbage collections.
      if (!Heap::IsLargeObject(obj) &&
          Page::FromHeapObject(obj)->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION)) {
        if (minor_marking_state->IsWhite(obj)) {
          return false;
        }
        *out = obj;
        return true;
      }
      DCHECK_IMPLIES(marking_state()->IsWhite(obj),
                     obj.IsFreeSpaceOrFiller(cage_base));
      // Skip one word filler objects that appear on the
      // stack when we perform in place array shift.
      if (obj.map(cage_base) != filler_map) {
        *out = obj;
        return true;
      }
      return false;
    }
  });

  major_collector_->local_weak_objects()->Publish();
  weak_objects_->UpdateAfterScavenge();
}

void IncrementalMarking::UpdateMarkedBytesAfterScavenge(
    size_t dead_bytes_in_new_space) {
  if (!IsMarking()) return;
  bytes_marked_ -= std::min(bytes_marked_, dead_bytes_in_new_space);
}

void IncrementalMarking::EmbedderStep(double expected_duration_ms,
                                      double* duration_ms) {
  DCHECK(IsMarking());
  if (!heap_->local_embedder_heap_tracer()
           ->SupportsIncrementalEmbedderSteps()) {
    *duration_ms = 0.0;
    return;
  }

  constexpr size_t kObjectsToProcessBeforeDeadlineCheck = 500;

  TRACE_GC(heap()->tracer(), GCTracer::Scope::MC_INCREMENTAL_EMBEDDER_TRACING);
  LocalEmbedderHeapTracer* local_tracer = heap_->local_embedder_heap_tracer();
  const double start = heap_->MonotonicallyIncreasingTimeInMs();
  const double deadline = start + expected_duration_ms;
  bool empty_worklist = true;
  if (local_marking_worklists()->PublishWrapper()) {
    DCHECK(local_marking_worklists()->IsWrapperEmpty());
  } else {
    // Cannot directly publish wrapper objects.
    LocalEmbedderHeapTracer::ProcessingScope scope(local_tracer);
    HeapObject object;
    size_t cnt = 0;
    while (local_marking_worklists()->PopWrapper(&object)) {
      scope.TracePossibleWrapper(JSObject::cast(object));
      if (++cnt == kObjectsToProcessBeforeDeadlineCheck) {
        if (deadline <= heap_->MonotonicallyIncreasingTimeInMs()) {
          empty_worklist = false;
          break;
        }
        cnt = 0;
      }
    }
  }
  // |deadline - heap_->MonotonicallyIncreasingTimeInMs()| could be negative,
  // which means |local_tracer| won't do any actual tracing, so there is no
  // need to check for |deadline <= heap_->MonotonicallyIncreasingTimeInMs()|.
  local_tracer->Trace(deadline - heap_->MonotonicallyIncreasingTimeInMs());
  double current = heap_->MonotonicallyIncreasingTimeInMs();
  local_tracer->SetEmbedderWorklistEmpty(empty_worklist);
  *duration_ms = current - start;
}

bool IncrementalMarking::Stop() {
  if (IsStopped()) return false;

  if (v8_flags.trace_incremental_marking) {
    int old_generation_size_mb =
        static_cast<int>(heap()->OldGenerationSizeOfObjects() / MB);
    int old_generation_limit_mb =
        static_cast<int>(heap()->old_generation_allocation_limit() / MB);
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Stopping: old generation %dMB, limit %dMB, "
        "overshoot %dMB\n",
        old_generation_size_mb, old_generation_limit_mb,
        std::max(0, old_generation_size_mb - old_generation_limit_mb));
  }

  if (IsMajorMarking()) {
    for (SpaceIterator it(heap_); it.HasNext();) {
      Space* space = it.Next();
      if (space == heap_->new_space()) {
        space->RemoveAllocationObserver(&new_generation_observer_);
      } else {
        space->RemoveAllocationObserver(&old_generation_observer_);
      }
    }
  }

  collection_requested_via_stack_guard_ = false;
  heap_->isolate()->stack_guard()->ClearGC();

  is_marking_ = false;
  heap_->SetIsMarkingFlag(false);
  heap_->SetIsMinorMarkingFlag(false);
  is_compacting_ = false;
  FinishBlackAllocation();

  // Merge live bytes counters of background threads
  for (const auto& pair : background_live_bytes_) {
    MemoryChunk* memory_chunk = pair.first;
    intptr_t live_bytes = pair.second;
    if (live_bytes) {
      marking_state()->IncrementLiveBytes(memory_chunk, live_bytes);
    }
  }
  background_live_bytes_.clear();
  current_collector_ = CurrentCollector::kNone;

  return true;
}

double IncrementalMarking::CurrentTimeToMarkingTask() const {
  const double recorded_time_to_marking_task =
      heap_->tracer()->AverageTimeToIncrementalMarkingTask();
  const double current_time_to_marking_task =
      incremental_marking_job_.CurrentTimeToTask();
  if (recorded_time_to_marking_task == 0.0) return 0.0;
  return std::max(recorded_time_to_marking_task, current_time_to_marking_task);
}

bool IncrementalMarking::ShouldWaitForTask() {
  if (!completion_task_scheduled_) {
    incremental_marking_job_.ScheduleTask();
    completion_task_scheduled_ = true;
  }

  if (completion_task_timeout_ == 0.0) {
    if (!TryInitializeTaskTimeout()) {
      return false;
    }
  }

  const double current_time = heap()->MonotonicallyIncreasingTimeInMs();
  const bool wait_for_task = current_time < completion_task_timeout_;

  if (v8_flags.trace_incremental_marking && wait_for_task) {
    heap()->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Delaying GC via stack guard. time left: "
        "%fms\n",
        completion_task_timeout_ - current_time);
  }

  return wait_for_task;
}

bool IncrementalMarking::TryInitializeTaskTimeout() {
  // Allowed overshoot percentage of incremental marking walltime.
  constexpr double kAllowedOvershoot = 0.1;
  // Minimum overshoot in ms. This is used to allow moving away from stack
  // when marking was fast.
  constexpr double kMinOvershootMs = 50;

  const double now = heap_->MonotonicallyIncreasingTimeInMs();
  const double overshoot_ms =
      std::max(kMinOvershootMs, (now - start_time_ms_) * kAllowedOvershoot);
  const double time_to_marking_task = CurrentTimeToMarkingTask();

  if (time_to_marking_task == 0.0 || time_to_marking_task > overshoot_ms) {
    if (v8_flags.trace_incremental_marking) {
      heap()->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Not delaying marking completion. time to "
          "task: %fms allowed overshoot: %fms\n",
          time_to_marking_task, overshoot_ms);
    }

    return false;
  } else {
    completion_task_timeout_ = now + overshoot_ms;

    if (v8_flags.trace_incremental_marking) {
      heap()->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Delaying GC via stack guard. time to task: "
          "%fms "
          "allowed overshoot: %fms\n",
          time_to_marking_task, overshoot_ms);
    }

    return true;
  }
}

void IncrementalMarking::FastForwardSchedule() {
  DCHECK(v8_flags.fast_forward_schedule);

  if (scheduled_bytes_to_mark_ < bytes_marked_) {
    scheduled_bytes_to_mark_ = bytes_marked_;
    if (v8_flags.trace_incremental_marking) {
      heap_->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Fast-forwarded schedule\n");
    }
  }
}

void IncrementalMarking::FastForwardScheduleIfCloseToFinalization() {
  // Consider marking close to finalization if 75% of the initial old
  // generation was marked.
  if (bytes_marked_ > 3 * (initial_old_generation_size_ / 4)) {
    FastForwardSchedule();
  }
}

void IncrementalMarking::ScheduleBytesToMarkBasedOnTime(double time_ms) {
  // Time interval that should be sufficient to complete incremental marking.
  constexpr double kTargetMarkingWallTimeInMs = 500;
  constexpr double kMinTimeBetweenScheduleInMs = 10;
  if (schedule_update_time_ms_ + kMinTimeBetweenScheduleInMs > time_ms) return;
  double delta_ms =
      std::min(time_ms - schedule_update_time_ms_, kTargetMarkingWallTimeInMs);
  schedule_update_time_ms_ = time_ms;

  size_t bytes_to_mark =
      (delta_ms / kTargetMarkingWallTimeInMs) * initial_old_generation_size_;
  AddScheduledBytesToMark(bytes_to_mark);

  if (v8_flags.trace_incremental_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Scheduled %zuKB to mark based on time delta "
        "%.1fms\n",
        bytes_to_mark / KB, delta_ms);
  }
}

void IncrementalMarking::AdvanceAndFinalizeIfComplete() {
  ScheduleBytesToMarkBasedOnTime(heap()->MonotonicallyIncreasingTimeInMs());
  if (v8_flags.fast_forward_schedule) {
    FastForwardScheduleIfCloseToFinalization();
  }
  Step(kStepSizeInMs, StepOrigin::kTask);
  heap()->FinalizeIncrementalMarkingIfComplete(
      GarbageCollectionReason::kFinalizeMarkingViaTask);
}

void IncrementalMarking::AdvanceAndFinalizeIfNecessary() {
  DCHECK(!heap_->always_allocate());
  AdvanceOnAllocation();

  if (collection_requested_via_stack_guard_) {
    heap()->FinalizeIncrementalMarkingIfComplete(
        GarbageCollectionReason::kFinalizeMarkingViaStackGuard);
  }
}

void IncrementalMarking::AdvanceForTesting(double max_step_size_in_ms) {
  Step(max_step_size_in_ms, StepOrigin::kV8);
}

void IncrementalMarking::AdvanceOnAllocation() {
  DCHECK_EQ(heap_->gc_state(), Heap::NOT_IN_GC);
  DCHECK(v8_flags.incremental_marking);
  DCHECK(IsMarking());

  // Code using an AlwaysAllocateScope assumes that the GC state does not
  // change; that implies that no marking steps must be performed.
  if (heap_->always_allocate()) {
    return;
  }

  ScheduleBytesToMarkBasedOnAllocation();
  Step(kMaxStepSizeInMs, StepOrigin::kV8);

  if (IsMajorMarkingComplete()) {
    // Marking cannot be finalized here. Schedule a completion task instead.
    if (!ShouldWaitForTask()) {
      // When task isn't run soon enough, fall back to stack guard to force
      // completion.
      collection_requested_via_stack_guard_ = true;
      heap_->isolate()->stack_guard()->RequestGC();
    }
  }
}

bool IncrementalMarking::ShouldFinalize() const {
  DCHECK(IsMarking());

  return heap()
             ->mark_compact_collector()
             ->local_marking_worklists()
             ->IsEmpty() &&
         heap()
             ->local_embedder_heap_tracer()
             ->ShouldFinalizeIncrementalMarking();
}

size_t IncrementalMarking::StepSizeToKeepUpWithAllocations() {
  // Update bytes_allocated_ based on the allocation counter.
  size_t current_counter = heap_->OldGenerationAllocationCounter();
  size_t result = current_counter - old_generation_allocation_counter_;
  old_generation_allocation_counter_ = current_counter;
  return result;
}

size_t IncrementalMarking::StepSizeToMakeProgress() {
  const size_t kTargetStepCount = 256;
  const size_t kTargetStepCountAtOOM = 32;
  const size_t kMaxStepSizeInByte = 256 * KB;
  size_t oom_slack = heap()->new_space()->Capacity() + 64 * MB;

  if (!heap()->CanExpandOldGeneration(oom_slack)) {
    return heap()->OldGenerationSizeOfObjects() / kTargetStepCountAtOOM;
  }

  return std::min(std::max({initial_old_generation_size_ / kTargetStepCount,
                            IncrementalMarking::kMinStepSizeInBytes}),
                  kMaxStepSizeInByte);
}

void IncrementalMarking::AddScheduledBytesToMark(size_t bytes_to_mark) {
  if (scheduled_bytes_to_mark_ + bytes_to_mark < scheduled_bytes_to_mark_) {
    // The overflow case.
    scheduled_bytes_to_mark_ = std::numeric_limits<std::size_t>::max();
  } else {
    scheduled_bytes_to_mark_ += bytes_to_mark;
  }
}

void IncrementalMarking::ScheduleBytesToMarkBasedOnAllocation() {
  size_t progress_bytes = StepSizeToMakeProgress();
  size_t allocation_bytes = StepSizeToKeepUpWithAllocations();
  size_t bytes_to_mark = progress_bytes + allocation_bytes;
  AddScheduledBytesToMark(bytes_to_mark);

  if (v8_flags.trace_incremental_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Scheduled %zuKB to mark based on allocation "
        "(progress=%zuKB, allocation=%zuKB)\n",
        bytes_to_mark / KB, progress_bytes / KB, allocation_bytes / KB);
  }
}

void IncrementalMarking::FetchBytesMarkedConcurrently() {
  if (v8_flags.concurrent_marking) {
    size_t current_bytes_marked_concurrently =
        heap()->concurrent_marking()->TotalMarkedBytes();
    // The concurrent_marking()->TotalMarkedBytes() is not monotonic for a
    // short period of time when a concurrent marking task is finishing.
    if (current_bytes_marked_concurrently > bytes_marked_concurrently_) {
      bytes_marked_ +=
          current_bytes_marked_concurrently - bytes_marked_concurrently_;
      bytes_marked_concurrently_ = current_bytes_marked_concurrently;
    }
    if (v8_flags.trace_incremental_marking) {
      heap_->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Marked %zuKB on background threads\n",
          heap_->concurrent_marking()->TotalMarkedBytes() / KB);
    }
  }
}

size_t IncrementalMarking::ComputeStepSizeInBytes(StepOrigin step_origin) {
  FetchBytesMarkedConcurrently();
  if (v8_flags.trace_incremental_marking) {
    if (scheduled_bytes_to_mark_ > bytes_marked_) {
      heap_->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Marker is %zuKB behind schedule\n",
          (scheduled_bytes_to_mark_ - bytes_marked_) / KB);
    } else {
      heap_->isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Marker is %zuKB ahead of schedule\n",
          (bytes_marked_ - scheduled_bytes_to_mark_) / KB);
    }
  }
  // Allow steps on allocation to get behind the schedule by small amount.
  // This gives higher priority to steps in tasks.
  size_t kScheduleMarginInBytes = step_origin == StepOrigin::kV8 ? 1 * MB : 0;
  if (bytes_marked_ + kScheduleMarginInBytes > scheduled_bytes_to_mark_)
    return 0;
  return scheduled_bytes_to_mark_ - bytes_marked_ - kScheduleMarginInBytes;
}

void IncrementalMarking::Step(double max_step_size_in_ms,
                              StepOrigin step_origin) {
  NestedTimedHistogramScope incremental_marking_scope(
      heap_->isolate()->counters()->gc_incremental_marking());
  TRACE_EVENT1("v8", "V8.GCIncrementalMarking", "epoch",
               heap_->tracer()->CurrentEpoch(GCTracer::Scope::MC_INCREMENTAL));
  TRACE_GC_EPOCH(heap_->tracer(), GCTracer::Scope::MC_INCREMENTAL,
                 ThreadKind::kMain);
  DCHECK(IsMajorMarking());
  double start = heap_->MonotonicallyIncreasingTimeInMs();

  size_t bytes_to_process = 0;
  size_t v8_bytes_processed = 0;
  double embedder_duration = 0.0;
  double embedder_deadline = 0.0;

  if (v8_flags.concurrent_marking) {
    // It is safe to merge back all objects that were on hold to the shared
    // work list at Step because we are at a safepoint where all objects
    // are properly initialized.
    local_marking_worklists()->MergeOnHold();
  }

// Only print marking worklist in debug mode to save ~40KB of code size.
#ifdef DEBUG
  if (v8_flags.trace_incremental_marking && v8_flags.trace_concurrent_marking &&
      v8_flags.trace_gc_verbose) {
    major_collector_->marking_worklists()->Print();
  }
#endif
  if (v8_flags.trace_incremental_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Marking speed %.fKB/ms\n",
        heap()->tracer()->IncrementalMarkingSpeedInBytesPerMillisecond());
  }
  // The first step after Scavenge will see many allocated bytes.
  // Cap the step size to distribute the marking work more uniformly.
  const double marking_speed =
      heap()->tracer()->IncrementalMarkingSpeedInBytesPerMillisecond();
  size_t max_step_size = GCIdleTimeHandler::EstimateMarkingStepSize(
      max_step_size_in_ms, marking_speed);
  bytes_to_process =
      std::min(ComputeStepSizeInBytes(step_origin), max_step_size);
  bytes_to_process = std::max({bytes_to_process, kMinStepSizeInBytes});

  // Perform a single V8 and a single embedder step. In case both have been
  // observed as empty back to back, we can finalize.
  //
  // This ignores that case where the embedder finds new V8-side objects. The
  // assumption is that large graphs are well connected and can mostly be
  // processed on their own. For small graphs, helping is not necessary.
  std::tie(v8_bytes_processed, std::ignore) =
      major_collector_->ProcessMarkingWorklist(bytes_to_process);
  if (heap_->local_embedder_heap_tracer()->InUse()) {
    embedder_deadline =
        std::min(max_step_size_in_ms,
                 static_cast<double>(bytes_to_process) / marking_speed);
    // TODO(chromium:1056170): Replace embedder_deadline with bytes_to_process
    // after migrating blink to the cppgc library and after v8 can directly
    // push objects to Oilpan.
    EmbedderStep(embedder_deadline, &embedder_duration);
  }
  bytes_marked_ += v8_bytes_processed;

  if (v8_flags.concurrent_marking) {
    local_marking_worklists()->ShareWork();
    heap_->concurrent_marking()->RescheduleJobIfNeeded(
        GarbageCollector::MARK_COMPACTOR);
  }

  const double current_time = heap_->MonotonicallyIncreasingTimeInMs();
  const double v8_duration = current_time - start - embedder_duration;
  heap_->tracer()->AddIncrementalMarkingStep(v8_duration, v8_bytes_processed);

  if (v8_flags.trace_incremental_marking) {
    heap_->isolate()->PrintWithTimestamp(
        "[IncrementalMarking] Step %s V8: %zuKB (%zuKB), embedder: %fms "
        "(%fms) "
        "in %.1f\n",
        step_origin == StepOrigin::kV8 ? "in v8" : "in task",
        v8_bytes_processed / KB, bytes_to_process / KB, embedder_duration,
        embedder_deadline, current_time - start);
  }
}

}  // namespace internal
}  // namespace v8
