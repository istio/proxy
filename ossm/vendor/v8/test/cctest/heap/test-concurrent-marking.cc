// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/heap/concurrent-marking.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap.h"
#include "src/heap/mark-compact.h"
#include "src/heap/marking-worklist-inl.h"
#include "src/heap/marking-worklist.h"
#include "src/init/v8.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

namespace v8 {
namespace internal {
namespace heap {

void PublishSegment(MarkingWorklist& worklist, HeapObject object) {
  MarkingWorklist::Local local(worklist);
  for (size_t i = 0; i < MarkingWorklist::kMinSegmentSizeForTesting; i++) {
    local.Push(object);
  }
  local.Publish();
}

TEST(ConcurrentMarking) {
  if (!i::v8_flags.concurrent_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  CcTest::CollectAllGarbage();
  if (!heap->incremental_marking()->IsStopped()) return;
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
  }

  WeakObjects weak_objects;
  ConcurrentMarking* concurrent_marking =
      new ConcurrentMarking(heap, &weak_objects);
  PublishSegment(*collector->marking_worklists()->shared(),
                 ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->ScheduleJob(GarbageCollector::MARK_COMPACTOR);
  concurrent_marking->Join();
  delete concurrent_marking;
}

TEST(ConcurrentMarkingReschedule) {
  if (!i::v8_flags.concurrent_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  CcTest::CollectAllGarbage();
  if (!heap->incremental_marking()->IsStopped()) return;
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
  }

  WeakObjects weak_objects;
  ConcurrentMarking* concurrent_marking =
      new ConcurrentMarking(heap, &weak_objects);
  PublishSegment(*collector->marking_worklists()->shared(),
                 ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->ScheduleJob(GarbageCollector::MARK_COMPACTOR);
  concurrent_marking->Join();
  PublishSegment(*collector->marking_worklists()->shared(),
                 ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->RescheduleJobIfNeeded(GarbageCollector::MARK_COMPACTOR);
  concurrent_marking->Join();
  delete concurrent_marking;
}

TEST(ConcurrentMarkingPreemptAndReschedule) {
  if (!i::v8_flags.concurrent_marking) return;
  CcTest::InitializeVM();
  Heap* heap = CcTest::heap();
  CcTest::CollectAllGarbage();
  if (!heap->incremental_marking()->IsStopped()) return;
  MarkCompactCollector* collector = CcTest::heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->EnsureSweepingCompleted(
        MarkCompactCollector::SweepingForcedFinalizationMode::kV8Only);
  }

  WeakObjects weak_objects;
  ConcurrentMarking* concurrent_marking =
      new ConcurrentMarking(heap, &weak_objects);
  for (int i = 0; i < 5000; i++)
    PublishSegment(*collector->marking_worklists()->shared(),
                   ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->ScheduleJob(GarbageCollector::MARK_COMPACTOR);
  concurrent_marking->Pause();
  for (int i = 0; i < 5000; i++)
    PublishSegment(*collector->marking_worklists()->shared(),
                   ReadOnlyRoots(heap).undefined_value());
  concurrent_marking->RescheduleJobIfNeeded(GarbageCollector::MARK_COMPACTOR);
  concurrent_marking->Join();
  delete concurrent_marking;
}

TEST(ConcurrentMarkingMarkedBytes) {
  if (!v8_flags.incremental_marking) return;
  if (!i::v8_flags.concurrent_marking) return;
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  Heap* heap = CcTest::heap();
  HandleScope sc(isolate);
  Handle<FixedArray> root = isolate->factory()->NewFixedArray(1000000);
  CcTest::CollectAllGarbage();
  if (!heap->incremental_marking()->IsStopped()) return;

  // Store array in Global such that it is part of the root set when
  // starting incremental marking.
  v8::Global<Value> global_root(CcTest::isolate(),
                                Utils::ToLocal(Handle<Object>::cast(root)));

  heap::SimulateIncrementalMarking(heap, false);
  heap->concurrent_marking()->Join();
  CHECK_GE(heap->concurrent_marking()->TotalMarkedBytes(), root->Size());
}

UNINITIALIZED_TEST(ConcurrentMarkingStoppedOnTeardown) {
  if (!v8_flags.incremental_marking) return;
  if (!i::v8_flags.concurrent_marking) return;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
    Factory* factory = i_isolate->factory();

    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::New(isolate)->Enter();

    for (int i = 0; i < 10000; i++) {
      factory->NewJSWeakMap();
    }

    Heap* heap = i_isolate->heap();
    heap::SimulateIncrementalMarking(heap, false);
  }

  isolate->Dispose();
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
