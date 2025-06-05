// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "test/unittests/heap/heap-utils.h"
#include "v8-isolate.h"

namespace v8 {
namespace internal {
namespace heap {

// Tests don't work when --optimize-for-size is set.
#ifndef V8_LITE_MODE

namespace {

class PagePromotionTest : public TestWithHeapInternalsAndContext {};

Page* FindLastPageInNewSpace(const std::vector<Handle<FixedArray>>& handles) {
  for (auto rit = handles.rbegin(); rit != handles.rend(); ++rit) {
    // One deref gets the Handle, the second deref gets the FixedArray.
    Page* candidate = Page::FromHeapObject(**rit);
    if (candidate->InNewSpace()) return candidate;
  }
  return nullptr;
}

}  // namespace

TEST_F(PagePromotionTest, PagePromotion_NewToOld) {
  if (i::v8_flags.single_generation) return;
  if (!i::v8_flags.incremental_marking) return;
  if (!i::v8_flags.page_promotion) return;
  v8_flags.page_promotion_threshold = 0;
  // Parallel evacuation messes with fragmentation in a way that objects that
  // should be copied in semi space are promoted to old space because of
  // fragmentation.
  v8_flags.parallel_compaction = false;
  // Parallel scavenge introduces too much fragmentation.
  v8_flags.parallel_scavenge = false;
  // We cannot optimize for size as we require a new space with more than one
  // page.
  v8_flags.optimize_for_size = false;

  ManualGCScope manual_gc_scope(isolate());

  {
    v8::HandleScope handle_scope(reinterpret_cast<v8::Isolate*>(isolate()));
    Heap* heap = isolate()->heap();

    // Ensure that the new space is empty so that the page to be promoted
    // does not contain the age mark.
    CollectGarbage(OLD_SPACE);

    std::vector<Handle<FixedArray>> handles;
    SimulateFullSpace(heap->new_space(), &handles);
    CHECK_GT(handles.size(), 0u);
    Page* const to_be_promoted_page = FindLastPageInNewSpace(handles);
    CHECK_NOT_NULL(to_be_promoted_page);
    CHECK(heap->new_space()->IsPromotionCandidate(to_be_promoted_page));
    // To perform a sanity check on live bytes we need to mark the heap.
    SimulateIncrementalMarking(true);
    // Sanity check that the page meets the requirements for promotion.
    const int threshold_bytes = static_cast<int>(
        v8_flags.page_promotion_threshold *
        MemoryChunkLayout::AllocatableMemoryInDataPage() / 100);
    CHECK_GE(heap->incremental_marking()->marking_state()->live_bytes(
                 to_be_promoted_page),
             threshold_bytes);

    // Actual checks: The page is in new space first, but is moved to old space
    // during a full GC.
    CHECK(heap->new_space()->ContainsSlow(to_be_promoted_page->address()));
    CHECK(!heap->old_space()->ContainsSlow(to_be_promoted_page->address()));
    CollectGarbage(OLD_SPACE);
    CHECK(!heap->new_space()->ContainsSlow(to_be_promoted_page->address()));
    CHECK(heap->old_space()->ContainsSlow(to_be_promoted_page->address()));
  }
}

#endif  // V8_LITE_MODE

}  // namespace heap
}  // namespace internal
}  // namespace v8
