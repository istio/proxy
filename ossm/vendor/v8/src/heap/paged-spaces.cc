// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/paged-spaces.h"

#include <atomic>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/allocation-observer.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/heap.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/memory-allocator.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/paged-spaces-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/safepoint.h"
#include "src/heap/spaces.h"
#include "src/heap/sweeper.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/string.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// PagedSpaceObjectIterator

PagedSpaceObjectIterator::PagedSpaceObjectIterator(Heap* heap,
                                                   const PagedSpaceBase* space)
    : cur_addr_(kNullAddress),
      cur_end_(kNullAddress),
      space_(space),
      page_range_(space->first_page(), nullptr),
      current_page_(page_range_.begin())
#if V8_COMPRESS_POINTERS
      ,
      cage_base_(heap->isolate())
#endif  // V8_COMPRESS_POINTERS
{
  heap->MakeHeapIterable();
  USE(space_);
}

PagedSpaceObjectIterator::PagedSpaceObjectIterator(Heap* heap,
                                                   const PagedSpaceBase* space,
                                                   const Page* page)
    : cur_addr_(kNullAddress),
      cur_end_(kNullAddress),
      space_(space),
      page_range_(page),
      current_page_(page_range_.begin())
#if V8_COMPRESS_POINTERS
      ,
      cage_base_(heap->isolate())
#endif  // V8_COMPRESS_POINTERS
{
  heap->MakeHeapIterable();
}

PagedSpaceObjectIterator::PagedSpaceObjectIterator(Heap* heap,
                                                   const PagedSpace* space,
                                                   const Page* page,
                                                   Address start_address)
    : cur_addr_(start_address),
      cur_end_(page->area_end()),
      space_(space),
      page_range_(page, page),
      current_page_(page_range_.begin())
#if V8_COMPRESS_POINTERS
      ,
      cage_base_(heap->isolate())
#endif  // V8_COMPRESS_POINTERS
{
  heap->MakeHeapIterable();
  DCHECK_IMPLIES(space->IsInlineAllocationEnabled(),
                 !page->Contains(space->top()));
  DCHECK(page->Contains(start_address));
  DCHECK(page->SweepingDone());
}

// We have hit the end of the page and should advance to the next block of
// objects.  This happens at the end of the page.
bool PagedSpaceObjectIterator::AdvanceToNextPage() {
  DCHECK_EQ(cur_addr_, cur_end_);
  if (current_page_ == page_range_.end()) return false;
  const Page* cur_page = *(current_page_++);

  cur_addr_ = cur_page->area_start();
  cur_end_ = cur_page->area_end();
  DCHECK(cur_page->SweepingDone());
  return true;
}

Page* PagedSpaceBase::InitializePage(MemoryChunk* chunk) {
  Page* page = static_cast<Page*>(chunk);
  DCHECK_EQ(
      MemoryChunkLayout::AllocatableMemoryInMemoryChunk(page->owner_identity()),
      page->area_size());
  // Make sure that categories are initialized before freeing the area.
  page->ResetAllocationStatistics();
  page->SetOldGenerationPageFlags(heap()->incremental_marking()->IsMarking());
  page->AllocateFreeListCategories();
  page->InitializeFreeListCategories();
  page->list_node().Initialize();
  page->InitializationMemoryFence();
  return page;
}

PagedSpaceBase::PagedSpaceBase(
    Heap* heap, AllocationSpace space, Executability executable,
    FreeList* free_list, AllocationCounter& allocation_counter,
    LinearAllocationArea& allocation_info_,
    LinearAreaOriginalData& linear_area_original_data,
    CompactionSpaceKind compaction_space_kind)
    : SpaceWithLinearArea(heap, space, free_list, allocation_counter,
                          allocation_info_, linear_area_original_data),
      executable_(executable),
      compaction_space_kind_(compaction_space_kind) {
  area_size_ = MemoryChunkLayout::AllocatableMemoryInMemoryChunk(space);
  accounting_stats_.Clear();
}

void PagedSpaceBase::TearDown() {
  while (!memory_chunk_list_.Empty()) {
    MemoryChunk* chunk = memory_chunk_list_.front();
    memory_chunk_list_.Remove(chunk);
    heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kImmediately,
                                     chunk);
  }
  accounting_stats_.Clear();
}

void PagedSpaceBase::RefillFreeList(Sweeper* sweeper) {
  // Any PagedSpace might invoke RefillFreeList. We filter all but our old
  // generation spaces out.
  DCHECK(identity() == OLD_SPACE || identity() == CODE_SPACE ||
         identity() == MAP_SPACE || identity() == NEW_SPACE);

  size_t added = 0;

  {
    Page* p = nullptr;
    while ((p = sweeper->GetSweptPageSafe(this)) != nullptr) {
      // We regularly sweep NEVER_ALLOCATE_ON_PAGE pages. We drop the freelist
      // entries here to make them unavailable for allocations.
      if (p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE)) {
        p->ForAllFreeListCategories([this](FreeListCategory* category) {
          category->Reset(free_list());
        });
      }

      // Only during compaction pages can actually change ownership. This is
      // safe because there exists no other competing action on the page links
      // during compaction.
      if (is_compaction_space()) {
        DCHECK_NE(this, p->owner());
        DCHECK_NE(NEW_SPACE, identity());
        PagedSpaceBase* owner = reinterpret_cast<PagedSpaceBase*>(p->owner());
        base::MutexGuard guard(owner->mutex());
        owner->RefineAllocatedBytesAfterSweeping(p);
        owner->RemovePage(p);
        added += AddPage(p);
        added += p->wasted_memory();
      } else {
        base::MutexGuard guard(mutex());
        DCHECK_EQ(this, p->owner());
        RefineAllocatedBytesAfterSweeping(p);
        added += RelinkFreeListCategories(p);
        added += p->wasted_memory();
      }
      if (is_compaction_space() && (added > kCompactionMemoryWanted)) break;
    }
  }
}

void PagedSpaceBase::MergeCompactionSpace(CompactionSpace* other) {
  base::MutexGuard guard(mutex());

  DCHECK_NE(NEW_SPACE, identity());
  DCHECK_NE(NEW_SPACE, other->identity());
  DCHECK(identity() == other->identity());

  // Unmerged fields:
  //   area_size_
  other->FreeLinearAllocationArea();

  for (int i = static_cast<int>(AllocationOrigin::kFirstAllocationOrigin);
       i <= static_cast<int>(AllocationOrigin::kLastAllocationOrigin); i++) {
    allocations_origins_[i] += other->allocations_origins_[i];
  }

  // The linear allocation area of {other} should be destroyed now.
  DCHECK_EQ(kNullAddress, other->top());
  DCHECK_EQ(kNullAddress, other->limit());

  // Move over pages.
  for (auto it = other->begin(); it != other->end();) {
    Page* p = *(it++);

    // Ensure that pages are initialized before objects on it are discovered by
    // concurrent markers.
    p->InitializationMemoryFence();

    // Relinking requires the category to be unlinked.
    other->RemovePage(p);
    AddPage(p);
    DCHECK_IMPLIES(
        !p->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE),
        p->AvailableInFreeList() == p->AvailableInFreeListFromAllocatedBytes());

    // TODO(leszeks): Here we should allocation step, but:
    //   1. Allocation groups are currently not handled properly by the sampling
    //      allocation profiler, and
    //   2. Observers might try to take the space lock, which isn't reentrant.
    // We'll have to come up with a better solution for allocation stepping
    // before shipping, which will likely be using LocalHeap.
  }
  for (auto p : other->GetNewPages()) {
    heap()->NotifyOldGenerationExpansion(identity(), p);
  }

  DCHECK_EQ(0u, other->Size());
  DCHECK_EQ(0u, other->Capacity());
}

size_t PagedSpaceBase::CommittedPhysicalMemory() const {
  if (!base::OS::HasLazyCommits()) {
    DCHECK_EQ(0, committed_physical_memory());
    return CommittedMemory();
  }
  CodePageHeaderModificationScope rwx_write_scope(
      "Updating high water mark for Code pages requires write access to "
      "the Code page headers");
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  return committed_physical_memory();
}

void PagedSpaceBase::IncrementCommittedPhysicalMemory(size_t increment_value) {
  if (!base::OS::HasLazyCommits() || increment_value == 0) return;
  size_t old_value = committed_physical_memory_.fetch_add(
      increment_value, std::memory_order_relaxed);
  USE(old_value);
  DCHECK_LT(old_value, old_value + increment_value);
}

void PagedSpaceBase::DecrementCommittedPhysicalMemory(size_t decrement_value) {
  if (!base::OS::HasLazyCommits() || decrement_value == 0) return;
  size_t old_value = committed_physical_memory_.fetch_sub(
      decrement_value, std::memory_order_relaxed);
  USE(old_value);
  DCHECK_GT(old_value, old_value - decrement_value);
}

#if DEBUG
void PagedSpaceBase::VerifyCommittedPhysicalMemory() const {
  heap()->safepoint()->AssertActive();
  size_t size = 0;
  for (const Page* page : *this) {
    DCHECK(page->SweepingDone());
    size += page->CommittedPhysicalMemory();
  }
  // Ensure that the space's counter matches the sum of all page counters.
  DCHECK_EQ(size, CommittedPhysicalMemory());
}
#endif  // DEBUG

bool PagedSpaceBase::ContainsSlow(Address addr) const {
  Page* p = Page::FromAddress(addr);
  for (const Page* page : *this) {
    if (page == p) return true;
  }
  return false;
}

void PagedSpaceBase::RefineAllocatedBytesAfterSweeping(Page* page) {
  CHECK(page->SweepingDone());
  auto marking_state =
      heap()->mark_compact_collector()->non_atomic_marking_state();
  // The live_byte on the page was accounted in the space allocated
  // bytes counter. After sweeping allocated_bytes() contains the
  // accurate live byte count on the page.
  size_t old_counter = marking_state->live_bytes(page);
  size_t new_counter = page->allocated_bytes();
  DCHECK_GE(old_counter, new_counter);
  if (old_counter > new_counter) {
    DecreaseAllocatedBytes(old_counter - new_counter, page);
  }
  marking_state->SetLiveBytes(page, 0);
}

Page* PagedSpaceBase::RemovePageSafe(int size_in_bytes) {
  base::MutexGuard guard(mutex());
  Page* page = free_list()->GetPageForSize(size_in_bytes);
  if (!page) return nullptr;
  RemovePage(page);
  return page;
}

size_t PagedSpaceBase::AddPage(Page* page) {
  DCHECK_NOT_NULL(page);
  CHECK(page->SweepingDone());
  page->set_owner(this);
  DCHECK_IMPLIES(identity() == NEW_SPACE, page->IsFlagSet(Page::TO_PAGE));
  DCHECK_IMPLIES(identity() != NEW_SPACE, !page->IsFlagSet(Page::TO_PAGE));
  memory_chunk_list_.PushBack(page);
  AccountCommitted(page->size());
  IncreaseCapacity(page->area_size());
  IncreaseAllocatedBytes(page->allocated_bytes(), page);
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    IncrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
  IncrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
  return RelinkFreeListCategories(page);
}

void PagedSpaceBase::RemovePage(Page* page) {
  CHECK(page->SweepingDone());
  DCHECK_IMPLIES(identity() == NEW_SPACE, page->IsFlagSet(Page::TO_PAGE));
  memory_chunk_list_.Remove(page);
  UnlinkFreeListCategories(page);
  if (identity() == NEW_SPACE) {
    page->ReleaseFreeListCategories();
  }
  // Pages are only removed from new space when they are promoted to old space
  // during a GC. This happens after sweeping as started and the allocation
  // counters have been reset.
  DCHECK_IMPLIES(identity() == NEW_SPACE, Size() == 0);
  if (identity() != NEW_SPACE) {
    DecreaseAllocatedBytes(page->allocated_bytes(), page);
  }
  DecreaseCapacity(page->area_size());
  AccountUncommitted(page->size());
  for (size_t i = 0; i < ExternalBackingStoreType::kNumTypes; i++) {
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    DecrementExternalBackingStoreBytes(t, page->ExternalBackingStoreBytes(t));
  }
  DecrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
}

void PagedSpaceBase::SetTopAndLimit(Address top, Address limit) {
  DCHECK(top == limit ||
         Page::FromAddress(top) == Page::FromAddress(limit - 1));
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  allocation_info_.Reset(top, limit);

  base::Optional<base::SharedMutexGuard<base::kExclusive>> optional_guard;
  if (!is_compaction_space()) optional_guard.emplace(linear_area_lock());
  linear_area_original_data_.set_original_limit_relaxed(limit);
  linear_area_original_data_.set_original_top_release(top);
}

size_t PagedSpaceBase::ShrinkPageToHighWaterMark(Page* page) {
  size_t unused = page->ShrinkToHighWaterMark();
  accounting_stats_.DecreaseCapacity(static_cast<intptr_t>(unused));
  AccountUncommitted(unused);
  return unused;
}

void PagedSpaceBase::ResetFreeList() {
  for (Page* page : *this) {
    free_list_->EvictFreeListItems(page);
  }
  DCHECK(free_list_->IsEmpty());
  DCHECK_EQ(0, free_list_->Available());
}

void PagedSpaceBase::ShrinkImmortalImmovablePages() {
  DCHECK(!heap()->deserialization_complete());
  BasicMemoryChunk::UpdateHighWaterMark(allocation_info_.top());
  FreeLinearAllocationArea();
  ResetFreeList();
  for (Page* page : *this) {
    DCHECK(page->IsFlagSet(Page::NEVER_EVACUATE));
    ShrinkPageToHighWaterMark(page);
  }
}

Page* PagedSpaceBase::TryExpandImpl() {
  Page* page = heap()->memory_allocator()->AllocatePage(
      MemoryAllocator::AllocationMode::kRegular, this, executable());
  if (page == nullptr) return nullptr;
  ConcurrentAllocationMutex guard(this);
  AddPage(page);
  Free(page->area_start(), page->area_size(),
       SpaceAccountingMode::kSpaceAccounted);
  return page;
}

base::Optional<std::pair<Address, size_t>> PagedSpaceBase::TryExpandBackground(
    size_t size_in_bytes) {
  DCHECK_NE(NEW_SPACE, identity());
  Page* page = heap()->memory_allocator()->AllocatePage(
      MemoryAllocator::AllocationMode::kRegular, this, executable());
  if (page == nullptr) return {};
  base::MutexGuard lock(&space_mutex_);
  AddPage(page);
  if (identity() == CODE_SPACE || identity() == CODE_LO_SPACE) {
    heap()->isolate()->AddCodeMemoryChunk(page);
  }
  Address object_start = page->area_start();
  CHECK_LE(size_in_bytes, page->area_size());
  Free(page->area_start() + size_in_bytes, page->area_size() - size_in_bytes,
       SpaceAccountingMode::kSpaceAccounted);
  AddRangeToActiveSystemPages(page, object_start, object_start + size_in_bytes);
  return std::make_pair(object_start, size_in_bytes);
}

int PagedSpaceBase::CountTotalPages() const {
  int count = 0;
  for (const Page* page : *this) {
    count++;
    USE(page);
  }
  return count;
}

void PagedSpaceBase::SetLinearAllocationArea(Address top, Address limit) {
  SetTopAndLimit(top, limit);
  if (top != kNullAddress && top != limit && identity() != NEW_SPACE &&
      heap()->incremental_marking()->black_allocation()) {
    Page::FromAllocationAreaAddress(top)->CreateBlackArea(top, limit);
  }
}

void PagedSpaceBase::DecreaseLimit(Address new_limit) {
  Address old_limit = limit();
  DCHECK_LE(top(), new_limit);
  DCHECK_GE(old_limit, new_limit);
  if (new_limit != old_limit) {
    base::Optional<CodePageMemoryModificationScope> optional_scope;

    if (identity() == CODE_SPACE) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(new_limit);
      optional_scope.emplace(chunk);
    }

    ConcurrentAllocationMutex guard(this);
    SetTopAndLimit(top(), new_limit);
    Free(new_limit, old_limit - new_limit,
         SpaceAccountingMode::kSpaceAccounted);
    if (heap()->incremental_marking()->black_allocation() &&
        identity() != NEW_SPACE) {
      Page::FromAllocationAreaAddress(new_limit)->DestroyBlackArea(new_limit,
                                                                   old_limit);
    }
  }
}

void PagedSpaceBase::MarkLinearAllocationAreaBlack() {
  DCHECK(heap()->incremental_marking()->black_allocation());
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->CreateBlackArea(current_top, current_limit);
  }
}

void PagedSpaceBase::UnmarkLinearAllocationArea() {
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    Page::FromAllocationAreaAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }
}

void PagedSpaceBase::MakeLinearAllocationAreaIterable() {
  Address current_top = top();
  Address current_limit = limit();
  if (current_top != kNullAddress && current_top != current_limit) {
    base::Optional<CodePageMemoryModificationScope> optional_scope;

    if (identity() == CODE_SPACE) {
      MemoryChunk* chunk = MemoryChunk::FromAddress(current_top);
      optional_scope.emplace(chunk);
    }

    heap_->CreateFillerObjectAt(current_top,
                                static_cast<int>(current_limit - current_top));
  }
}

size_t PagedSpaceBase::Available() const {
  ConcurrentAllocationMutex guard(this);
  return free_list_->Available();
}

namespace {

UnprotectMemoryOrigin GetUnprotectMemoryOrigin(bool is_compaction_space) {
  return is_compaction_space ? UnprotectMemoryOrigin::kMaybeOffMainThread
                             : UnprotectMemoryOrigin::kMainThread;
}

}  // namespace

void PagedSpaceBase::FreeLinearAllocationArea() {
  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.
  Address current_top = top();
  Address current_limit = limit();
  if (current_top == kNullAddress) {
    DCHECK_EQ(kNullAddress, current_limit);
    return;
  }

  AdvanceAllocationObservers();

  if (identity() != NEW_SPACE && current_top != current_limit &&
      heap()->incremental_marking()->black_allocation()) {
    Page::FromAddress(current_top)
        ->DestroyBlackArea(current_top, current_limit);
  }

  SetTopAndLimit(kNullAddress, kNullAddress);
  DCHECK_GE(current_limit, current_top);

  // The code page of the linear allocation area needs to be unprotected
  // because we are going to write a filler into that memory area below.
  if (identity() == CODE_SPACE) {
    heap()->UnprotectAndRegisterMemoryChunk(
        MemoryChunk::FromAddress(current_top),
        GetUnprotectMemoryOrigin(is_compaction_space()));
  }

  DCHECK_IMPLIES(current_limit - current_top >= 2 * kTaggedSize,
                 heap()->incremental_marking()->marking_state()->IsWhite(
                     HeapObject::FromAddress(current_top)));
  Free(current_top, current_limit - current_top,
       SpaceAccountingMode::kSpaceAccounted);
}

void PagedSpaceBase::ReleasePage(Page* page) {
  DCHECK_EQ(
      0, heap()->incremental_marking()->non_atomic_marking_state()->live_bytes(
             page));
  DCHECK_EQ(page->owner(), this);

  DCHECK_IMPLIES(identity() == NEW_SPACE, page->IsFlagSet(Page::TO_PAGE));

  free_list_->EvictFreeListItems(page);

  if (Page::FromAllocationAreaAddress(allocation_info_.top()) == page) {
    SetTopAndLimit(kNullAddress, kNullAddress);
  }

  if (identity() == CODE_SPACE) {
    heap()->isolate()->RemoveCodeMemoryChunk(page);
  }

  AccountUncommitted(page->size());
  DecrementCommittedPhysicalMemory(page->CommittedPhysicalMemory());
  accounting_stats_.DecreaseCapacity(page->area_size());
  heap()->memory_allocator()->Free(MemoryAllocator::FreeMode::kConcurrently,
                                   page);
}

void PagedSpaceBase::SetReadable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    DCHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadable();
  }
}

void PagedSpaceBase::SetReadAndExecutable() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    DCHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetReadAndExecutable();
  }
}

void PagedSpaceBase::SetCodeModificationPermissions() {
  DCHECK(identity() == CODE_SPACE);
  for (Page* page : *this) {
    DCHECK(heap()->memory_allocator()->IsMemoryChunkExecutable(page));
    page->SetCodeModificationPermissions();
  }
}

std::unique_ptr<ObjectIterator> PagedSpaceBase::GetObjectIterator(Heap* heap) {
  return std::unique_ptr<ObjectIterator>(
      new PagedSpaceObjectIterator(heap, this));
}

bool PagedSpaceBase::TryAllocationFromFreeListMain(size_t size_in_bytes,
                                                   AllocationOrigin origin) {
  ConcurrentAllocationMutex guard(this);
  DCHECK(IsAligned(size_in_bytes, kTaggedSize));
  DCHECK_LE(top(), limit());
#ifdef DEBUG
  if (top() != limit()) {
    DCHECK_EQ(Page::FromAddress(top()), Page::FromAddress(limit() - 1));
  }
#endif
  // Don't free list allocate if there is linear space available.
  DCHECK_LT(static_cast<size_t>(limit() - top()), size_in_bytes);

  // Mark the old linear allocation area with a free space map so it can be
  // skipped when scanning the heap.  This also puts it back in the free list
  // if it is big enough.
  FreeLinearAllocationArea();

  size_t new_node_size = 0;
  FreeSpace new_node =
      free_list_->Allocate(size_in_bytes, &new_node_size, origin);
  if (new_node.is_null()) return false;
  DCHECK_GE(new_node_size, size_in_bytes);

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  Page* page = Page::FromHeapObject(new_node);
  IncreaseAllocatedBytes(new_node_size, page);

  DCHECK_EQ(allocation_info_.start(), allocation_info_.top());
  Address start = new_node.address();
  Address end = new_node.address() + new_node_size;
  Address limit = ComputeLimit(start, end, size_in_bytes);
  DCHECK_LE(limit, end);
  DCHECK_LE(size_in_bytes, limit - start);
  if (limit != end) {
    if (identity() == CODE_SPACE) {
      heap()->UnprotectAndRegisterMemoryChunk(
          page, GetUnprotectMemoryOrigin(is_compaction_space()));
    }
    Free(limit, end - limit, SpaceAccountingMode::kSpaceAccounted);
  }
  SetLinearAllocationArea(start, limit);
  AddRangeToActiveSystemPages(page, start, limit);

  return true;
}

base::Optional<std::pair<Address, size_t>>
PagedSpaceBase::RawAllocateBackground(LocalHeap* local_heap,
                                      size_t min_size_in_bytes,
                                      size_t max_size_in_bytes,
                                      AllocationOrigin origin) {
  DCHECK(!is_compaction_space());
  DCHECK(identity() == OLD_SPACE || identity() == CODE_SPACE ||
         identity() == MAP_SPACE);
  DCHECK(origin == AllocationOrigin::kRuntime ||
         origin == AllocationOrigin::kGC);
  DCHECK_IMPLIES(!local_heap, origin == AllocationOrigin::kGC);

  base::Optional<std::pair<Address, size_t>> result =
      TryAllocationFromFreeListBackground(min_size_in_bytes, max_size_in_bytes,
                                          origin);
  if (result) return result;

  MarkCompactCollector* collector = heap()->mark_compact_collector();
  // Sweeping is still in progress.
  if (collector->sweeping_in_progress()) {
    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    RefillFreeList(collector->sweeper());

    // Retry the free list allocation.
    result = TryAllocationFromFreeListBackground(min_size_in_bytes,
                                                 max_size_in_bytes, origin);
    if (result) return result;

    if (IsSweepingAllowedOnThread(local_heap)) {
      // Now contribute to sweeping from background thread and then try to
      // reallocate.
      const int kMaxPagesToSweep = 1;
      int max_freed = collector->sweeper()->ParallelSweepSpace(
          identity(), Sweeper::SweepingMode::kLazyOrConcurrent,
          static_cast<int>(min_size_in_bytes), kMaxPagesToSweep);

      // Keep new space sweeping atomic.
      RefillFreeList(collector->sweeper());

      if (static_cast<size_t>(max_freed) >= min_size_in_bytes) {
        result = TryAllocationFromFreeListBackground(min_size_in_bytes,
                                                     max_size_in_bytes, origin);
        if (result) return result;
      }
    }
  }

  if (heap()->ShouldExpandOldGenerationOnSlowAllocation(local_heap) &&
      heap()->CanExpandOldGenerationBackground(local_heap, AreaSize())) {
    result = TryExpandBackground(max_size_in_bytes);
    if (result) return result;
  }

  if (collector->sweeping_in_progress()) {
    // Complete sweeping for this space.
    if (IsSweepingAllowedOnThread(local_heap)) {
      collector->DrainSweepingWorklistForSpace(identity());
    }

    RefillFreeList(collector->sweeper());

    // Last try to acquire memory from free list.
    return TryAllocationFromFreeListBackground(min_size_in_bytes,
                                               max_size_in_bytes, origin);
  }

  return {};
}

base::Optional<std::pair<Address, size_t>>
PagedSpaceBase::TryAllocationFromFreeListBackground(size_t min_size_in_bytes,
                                                    size_t max_size_in_bytes,
                                                    AllocationOrigin origin) {
  base::MutexGuard lock(&space_mutex_);
  DCHECK_LE(min_size_in_bytes, max_size_in_bytes);
  DCHECK(identity() == OLD_SPACE || identity() == CODE_SPACE ||
         identity() == MAP_SPACE);

  size_t new_node_size = 0;
  FreeSpace new_node =
      free_list_->Allocate(min_size_in_bytes, &new_node_size, origin);
  if (new_node.is_null()) return {};
  DCHECK_GE(new_node_size, min_size_in_bytes);

  // The old-space-step might have finished sweeping and restarted marking.
  // Verify that it did not turn the page of the new node into an evacuation
  // candidate.
  DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_node));

  // Memory in the linear allocation area is counted as allocated.  We may free
  // a little of this again immediately - see below.
  Page* page = Page::FromHeapObject(new_node);
  IncreaseAllocatedBytes(new_node_size, page);

  heap()->StartIncrementalMarkingIfAllocationLimitIsReachedBackground();

  size_t used_size_in_bytes = std::min(new_node_size, max_size_in_bytes);

  Address start = new_node.address();
  Address end = new_node.address() + new_node_size;
  Address limit = new_node.address() + used_size_in_bytes;
  DCHECK_LE(limit, end);
  DCHECK_LE(min_size_in_bytes, limit - start);
  if (limit != end) {
    if (identity() == CODE_SPACE) {
      heap()->UnprotectAndRegisterMemoryChunk(
          page, UnprotectMemoryOrigin::kMaybeOffMainThread);
    }
    Free(limit, end - limit, SpaceAccountingMode::kSpaceAccounted);
  }
  AddRangeToActiveSystemPages(page, start, limit);

  return std::make_pair(start, used_size_in_bytes);
}

bool PagedSpaceBase::IsSweepingAllowedOnThread(LocalHeap* local_heap) const {
  // Code space sweeping is only allowed on main thread.
  return (local_heap && local_heap->is_main_thread()) ||
         identity() != CODE_SPACE;
}

#ifdef DEBUG
void PagedSpaceBase::Print() {}
#endif

#ifdef VERIFY_HEAP
void PagedSpaceBase::Verify(Isolate* isolate, ObjectVisitor* visitor) const {
  bool allocation_pointer_found_in_space =
      (allocation_info_.top() == allocation_info_.limit());
  size_t external_space_bytes[kNumTypes];
  size_t external_page_bytes[kNumTypes];

  for (int i = 0; i < kNumTypes; i++) {
    external_space_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
  }

  PtrComprCageBase cage_base(isolate);
  for (const Page* page : *this) {
    CHECK_EQ(page->owner(), this);

    for (int i = 0; i < kNumTypes; i++) {
      external_page_bytes[static_cast<ExternalBackingStoreType>(i)] = 0;
    }

    if (page == Page::FromAllocationAreaAddress(allocation_info_.top())) {
      allocation_pointer_found_in_space = true;
    }
    CHECK(page->SweepingDone());
    PagedSpaceObjectIterator it(isolate->heap(), this, page);
    Address end_of_previous_object = page->area_start();
    Address top = page->area_end();

    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      CHECK(end_of_previous_object <= object.address());

      // The first word should be a map, and we expect all map pointers to
      // be in map space.
      Map map = object.map(cage_base);
      CHECK(map.IsMap(cage_base));
      CHECK(ReadOnlyHeap::Contains(map) ||
            isolate->heap()->space_for_maps()->Contains(map));

      // Perform space-specific object verification.
      VerifyObject(object);

      // The object itself should look OK.
      object.ObjectVerify(isolate);

      if (identity() != RO_SPACE && !v8_flags.verify_heap_skip_remembered_set) {
        HeapVerifier::VerifyRememberedSetFor(isolate->heap(), object);
      }

      // All the interior pointers should be contained in the heap.
      int size = object.Size(cage_base);
      object.IterateBody(map, size, visitor);
      CHECK(object.address() + size <= top);
      end_of_previous_object = object.address() + size;

      if (object.IsExternalString(cage_base)) {
        ExternalString external_string = ExternalString::cast(object);
        size_t payload_size = external_string.ExternalPayloadSize();
        external_page_bytes[ExternalBackingStoreType::kExternalString] +=
            payload_size;
      }
    }
    for (int i = 0; i < kNumTypes; i++) {
      ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
      CHECK_EQ(external_page_bytes[t], page->ExternalBackingStoreBytes(t));
      external_space_bytes[t] += external_page_bytes[t];
    }

    CHECK(!page->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION));
    CHECK(!page->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION));

#ifdef V8_ENABLE_INNER_POINTER_RESOLUTION_OSB
    page->object_start_bitmap()->Verify();
#endif  // V8_ENABLE_INNER_POINTER_RESOLUTION_OSB
  }
  for (int i = 0; i < kNumTypes; i++) {
    if (i == ExternalBackingStoreType::kArrayBuffer) continue;
    ExternalBackingStoreType t = static_cast<ExternalBackingStoreType>(i);
    CHECK_EQ(external_space_bytes[t], ExternalBackingStoreBytes(t));
  }
  CHECK(allocation_pointer_found_in_space);

  if (identity() == OLD_SPACE && !v8_flags.concurrent_array_buffer_sweeping) {
    size_t bytes = heap()->array_buffer_sweeper()->old().BytesSlow();
    CHECK_EQ(bytes,
             ExternalBackingStoreBytes(ExternalBackingStoreType::kArrayBuffer));
  }

#ifdef DEBUG
  VerifyCountersAfterSweeping(isolate->heap());
#endif
}

void PagedSpaceBase::VerifyLiveBytes() const {
  MarkingState* marking_state = heap()->incremental_marking()->marking_state();
  PtrComprCageBase cage_base(heap()->isolate());
  for (const Page* page : *this) {
    CHECK(page->SweepingDone());
    PagedSpaceObjectIterator it(heap(), this, page);
    int black_size = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      // All the interior pointers should be contained in the heap.
      if (marking_state->IsBlack(object)) {
        black_size += object.Size(cage_base);
      }
    }
    CHECK_LE(black_size, marking_state->live_bytes(page));
  }
}
#endif  // VERIFY_HEAP

#ifdef DEBUG
void PagedSpaceBase::VerifyCountersAfterSweeping(Heap* heap) const {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  PtrComprCageBase cage_base(heap->isolate());
  for (const Page* page : *this) {
    DCHECK(page->SweepingDone());
    total_capacity += page->area_size();
    PagedSpaceObjectIterator it(heap, this, page);
    size_t real_allocated = 0;
    for (HeapObject object = it.Next(); !object.is_null(); object = it.Next()) {
      if (!object.IsFreeSpaceOrFiller()) {
        real_allocated += object.Size(cage_base);
      }
    }
    total_allocated += page->allocated_bytes();
    // The real size can be smaller than the accounted size if array trimming,
    // object slack tracking happened after sweeping.
    DCHECK_LE(real_allocated, accounting_stats_.AllocatedOnPage(page));
    DCHECK_EQ(page->allocated_bytes(), accounting_stats_.AllocatedOnPage(page));
  }
  DCHECK_EQ(total_capacity, accounting_stats_.Capacity());
  DCHECK_EQ(total_allocated, accounting_stats_.Size());
}

void PagedSpaceBase::VerifyCountersBeforeConcurrentSweeping() const {
  size_t total_capacity = 0;
  size_t total_allocated = 0;
  auto marking_state =
      heap()->incremental_marking()->non_atomic_marking_state();
  for (const Page* page : *this) {
    size_t page_allocated =
        page->SweepingDone()
            ? page->allocated_bytes()
            : static_cast<size_t>(marking_state->live_bytes(page));
    total_capacity += page->area_size();
    total_allocated += page_allocated;
    DCHECK_EQ(page_allocated, accounting_stats_.AllocatedOnPage(page));
  }
  DCHECK_EQ(total_capacity, accounting_stats_.Capacity());
  DCHECK_EQ(total_allocated, accounting_stats_.Size());
}
#endif

void PagedSpaceBase::UpdateInlineAllocationLimit(size_t min_size) {
  // Ensure there are no unaccounted allocations.
  DCHECK_EQ(allocation_info_.start(), allocation_info_.top());

  Address new_limit = ComputeLimit(top(), limit(), min_size);
  DCHECK_LE(top(), new_limit);
  DCHECK_LE(new_limit, limit());
  DecreaseLimit(new_limit);
}

// -----------------------------------------------------------------------------
// OldSpace implementation

void PagedSpaceBase::PrepareForMarkCompact() {
  // Clear the free list before a full GC---it will be rebuilt afterward.
  free_list_->Reset();
}

bool PagedSpaceBase::RefillLabMain(int size_in_bytes, AllocationOrigin origin) {
  VMState<GC> state(heap()->isolate());
  RCS_SCOPE(heap()->isolate(),
            RuntimeCallCounterId::kGC_Custom_SlowAllocateRaw);
  return RawRefillLabMain(size_in_bytes, origin);
}

Page* CompactionSpace::TryExpandImpl() {
  DCHECK_NE(NEW_SPACE, identity());
  Page* page = PagedSpaceBase::TryExpandImpl();
  new_pages_.push_back(page);
  return page;
}

bool CompactionSpace::RefillLabMain(int size_in_bytes,
                                    AllocationOrigin origin) {
  return RawRefillLabMain(size_in_bytes, origin);
}

bool PagedSpaceBase::TryExpand(int size_in_bytes, AllocationOrigin origin) {
  DCHECK_NE(NEW_SPACE, identity());
  Page* page = TryExpandImpl();
  if (!page) return false;
  if (!is_compaction_space() && identity() != NEW_SPACE) {
    heap()->NotifyOldGenerationExpansion(identity(), page);
  }
  return TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                       origin);
}

bool PagedSpaceBase::RawRefillLabMain(int size_in_bytes,
                                      AllocationOrigin origin) {
  // Allocation in this space has failed.
  DCHECK_GE(size_in_bytes, 0);
  const int kMaxPagesToSweep = 1;

  if (TryAllocationFromFreeListMain(size_in_bytes, origin)) return true;

  if (identity() == NEW_SPACE) {
    // New space should not allocate new pages when running out of space and it
    // is not currently swept.
    return false;
  }

  MarkCompactCollector* collector = heap()->mark_compact_collector();
  // Sweeping is still in progress.
  if (collector->sweeping_in_progress()) {
    // First try to refill the free-list, concurrent sweeper threads
    // may have freed some objects in the meantime.
    RefillFreeList(collector->sweeper());

    // Retry the free list allocation.
    if (TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                      origin))
      return true;

    if (ContributeToSweepingMain(size_in_bytes, kMaxPagesToSweep, size_in_bytes,
                                 origin))
      return true;
  }

  if (is_compaction_space()) {
    DCHECK_NE(NEW_SPACE, identity());
    // The main thread may have acquired all swept pages. Try to steal from
    // it. This can only happen during young generation evacuation.
    PagedSpaceBase* main_space = heap()->paged_space(identity());
    Page* page = main_space->RemovePageSafe(size_in_bytes);
    if (page != nullptr) {
      AddPage(page);
      if (TryAllocationFromFreeListMain(static_cast<size_t>(size_in_bytes),
                                        origin))
        return true;
    }
  }

  if (heap()->ShouldExpandOldGenerationOnSlowAllocation(
          heap()->main_thread_local_heap()) &&
      heap()->CanExpandOldGeneration(AreaSize())) {
    if (TryExpand(size_in_bytes, origin)) {
      return true;
    }
  }

  // Try sweeping all pages.
  if (ContributeToSweepingMain(0, 0, size_in_bytes, origin)) {
    return true;
  }

  if (heap()->gc_state() != Heap::NOT_IN_GC && !heap()->force_oom()) {
    // Avoid OOM crash in the GC in order to invoke NearHeapLimitCallback after
    // GC and give it a chance to increase the heap limit.
    return TryExpand(size_in_bytes, origin);
  }
  return false;
}

bool PagedSpaceBase::ContributeToSweepingMain(int required_freed_bytes,
                                              int max_pages, int size_in_bytes,
                                              AllocationOrigin origin) {
  // TODO(v8:12612): New space is not currently swept so new space allocation
  // shoudl not contribute to sweeping, Revisit this once sweeping for young gen
  // is implemented.
  DCHECK_NE(NEW_SPACE, identity());
  // Cleanup invalidated old-to-new refs for compaction space in the
  // final atomic pause.
  Sweeper::SweepingMode sweeping_mode =
      is_compaction_space() ? Sweeper::SweepingMode::kEagerDuringGC
                            : Sweeper::SweepingMode::kLazyOrConcurrent;

  MarkCompactCollector* collector = heap()->mark_compact_collector();
  if (collector->sweeping_in_progress()) {
    collector->sweeper()->ParallelSweepSpace(identity(), sweeping_mode,
                                             required_freed_bytes, max_pages);
    RefillFreeList(collector->sweeper());
    return TryAllocationFromFreeListMain(size_in_bytes, origin);
  }
  return false;
}

void PagedSpaceBase::AddRangeToActiveSystemPages(Page* page, Address start,
                                                 Address end) {
  DCHECK_LE(page->address(), start);
  DCHECK_LT(start, end);
  DCHECK_LE(end, page->address() + Page::kPageSize);

  const size_t added_pages = page->active_system_pages()->Add(
      start - page->address(), end - page->address(),
      MemoryAllocator::GetCommitPageSizeBits());

  IncrementCommittedPhysicalMemory(added_pages *
                                   MemoryAllocator::GetCommitPageSize());
}

void PagedSpaceBase::ReduceActiveSystemPages(
    Page* page, ActiveSystemPages active_system_pages) {
  const size_t reduced_pages =
      page->active_system_pages()->Reduce(active_system_pages);
  DecrementCommittedPhysicalMemory(reduced_pages *
                                   MemoryAllocator::GetCommitPageSize());
}

void PagedSpaceBase::UnlinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  page->ForAllFreeListCategories([this](FreeListCategory* category) {
    free_list()->RemoveCategory(category);
  });
}

size_t PagedSpaceBase::RelinkFreeListCategories(Page* page) {
  DCHECK_EQ(this, page->owner());
  size_t added = 0;
  page->ForAllFreeListCategories([this, &added](FreeListCategory* category) {
    added += category->available();
    category->Relink(free_list());
  });

  DCHECK_IMPLIES(!page->IsFlagSet(Page::NEVER_ALLOCATE_ON_PAGE),
                 page->AvailableInFreeList() ==
                     page->AvailableInFreeListFromAllocatedBytes());
  return added;
}

// -----------------------------------------------------------------------------
// MapSpace implementation

// TODO(dmercadier): use a heap instead of sorting like that.
// Using a heap will have multiple benefits:
//   - for now, SortFreeList is only called after sweeping, which is somewhat
//   late. Using a heap, sorting could be done online: FreeListCategories would
//   be inserted in a heap (ie, in a sorted manner).
//   - SortFreeList is a bit fragile: any change to FreeListMap (or to
//   MapSpace::free_list_) could break it.
void MapSpace::SortFreeList() {
  using LiveBytesPagePair = std::pair<size_t, Page*>;
  std::vector<LiveBytesPagePair> pages;
  pages.reserve(CountTotalPages());

  for (Page* p : *this) {
    free_list()->RemoveCategory(p->free_list_category(kFirstCategory));
    pages.push_back(std::make_pair(p->allocated_bytes(), p));
  }

  // Sorting by least-allocated-bytes first.
  std::sort(pages.begin(), pages.end(),
            [](const LiveBytesPagePair& a, const LiveBytesPagePair& b) {
              return a.first < b.first;
            });

  for (LiveBytesPagePair const& p : pages) {
    // Since AddCategory inserts in head position, it reverts the order produced
    // by the sort above: least-allocated-bytes will be Added first, and will
    // therefore be the last element (and the first one will be
    // most-allocated-bytes).
    free_list()->AddCategory(p.second->free_list_category(kFirstCategory));
  }
}

#ifdef VERIFY_HEAP
void MapSpace::VerifyObject(HeapObject object) const { CHECK(object.IsMap()); }
#endif

}  // namespace internal
}  // namespace v8
