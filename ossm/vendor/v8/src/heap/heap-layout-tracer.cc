// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-layout-tracer.h"

#include <iostream>

#include "src/heap/new-spaces.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/read-only-spaces.h"
#include "src/heap/spaces-inl.h"

namespace v8 {
namespace internal {

// static
void HeapLayoutTracer::GCProloguePrintHeapLayout(v8::Isolate* isolate,
                                                 v8::GCType gc_type,
                                                 v8::GCCallbackFlags flags,
                                                 void* data) {
  Heap* heap = reinterpret_cast<i::Isolate*>(isolate)->heap();
  // gc_count_ will increase after this callback, manually add 1.
  PrintF("Before GC:%d,", heap->gc_count() + 1);
  PrintF("collector_name:%s\n", Heap::CollectorName(gc_type));
  PrintHeapLayout(std::cout, heap);
}

// static
void HeapLayoutTracer::GCEpiloguePrintHeapLayout(v8::Isolate* isolate,
                                                 v8::GCType gc_type,
                                                 v8::GCCallbackFlags flags,
                                                 void* data) {
  Heap* heap = reinterpret_cast<i::Isolate*>(isolate)->heap();
  PrintF("After GC:%d,", heap->gc_count());
  PrintF("collector_name:%s\n", Heap::CollectorName(gc_type));
  PrintHeapLayout(std::cout, heap);
}

// static
void HeapLayoutTracer::PrintBasicMemoryChunk(std::ostream& os,
                                             const BasicMemoryChunk& chunk,
                                             const char* owner_name) {
  os << "{owner:" << owner_name << ","
     << "address:" << &chunk << ","
     << "size:" << chunk.size() << ","
     << "allocated_bytes:" << chunk.allocated_bytes() << ","
     << "wasted_memory:" << chunk.wasted_memory() << "}" << std::endl;
}

// static
void HeapLayoutTracer::PrintHeapLayout(std::ostream& os, Heap* heap) {
  if (v8_flags.minor_mc) {
    for (const Page* page : *heap->paged_new_space()) {
      PrintBasicMemoryChunk(os, *page, "new_space");
    }
  } else {
    const SemiSpaceNewSpace* semi_space_new_space =
        SemiSpaceNewSpace::From(heap->new_space());
    for (const Page* page : semi_space_new_space->to_space()) {
      PrintBasicMemoryChunk(os, *page, "to_space");
    }

    for (const Page* page : semi_space_new_space->from_space()) {
      PrintBasicMemoryChunk(os, *page, "from_space");
    }
  }

  OldGenerationMemoryChunkIterator it(heap);
  MemoryChunk* chunk;
  while ((chunk = it.next()) != nullptr) {
    PrintBasicMemoryChunk(os, *chunk, chunk->owner()->name());
  }

  for (ReadOnlyPage* page : heap->read_only_space()->pages()) {
    PrintBasicMemoryChunk(os, *page, "ro_space");
  }
}
}  // namespace internal
}  // namespace v8
