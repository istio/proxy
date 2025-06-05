// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_
#define V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_

#include "include/cppgc/trace-trait.h"
#include "include/v8-cppgc.h"
#include "src/base/macros.h"
#include "src/heap/cppgc-js/unified-heap-marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"

namespace cppgc {

class SourceLocation;

namespace internal {
class ConcurrentMarkingState;
class BasicMarkingState;
class MutatorMarkingState;
}  // namespace internal
}  // namespace cppgc

namespace v8 {
namespace internal {

using cppgc::SourceLocation;
using cppgc::TraceDescriptor;
using cppgc::WeakCallback;
using cppgc::internal::HeapBase;
using cppgc::internal::MutatorMarkingState;

class UnifiedHeapMarker;

class V8_EXPORT_PRIVATE UnifiedHeapMarkingVisitorBase : public JSVisitor {
 public:
  UnifiedHeapMarkingVisitorBase(HeapBase&, cppgc::internal::BasicMarkingState&,
                                UnifiedHeapMarkingState&);
  ~UnifiedHeapMarkingVisitorBase() override = default;

 protected:
  // C++ handling.
  void Visit(const void*, TraceDescriptor) final;
  void VisitWeak(const void*, TraceDescriptor, WeakCallback, const void*) final;
  void VisitEphemeron(const void*, const void*, TraceDescriptor) final;
  void VisitWeakContainer(const void* self, TraceDescriptor strong_desc,
                          TraceDescriptor weak_desc, WeakCallback callback,
                          const void* data) final;
  void RegisterWeakCallback(WeakCallback, const void*) final;
  void HandleMovableReference(const void**) final;

  // JS handling.
  void Visit(const TracedReferenceBase& ref) override;

  cppgc::internal::BasicMarkingState& marking_state_;
  UnifiedHeapMarkingState& unified_heap_marking_state_;

  friend class UnifiedHeapMarker;
};

class V8_EXPORT_PRIVATE MutatorUnifiedHeapMarkingVisitor
    : public UnifiedHeapMarkingVisitorBase {
 public:
  MutatorUnifiedHeapMarkingVisitor(HeapBase&, MutatorMarkingState&,
                                   UnifiedHeapMarkingState&);
  ~MutatorUnifiedHeapMarkingVisitor() override = default;
};

class V8_EXPORT_PRIVATE MutatorMinorGCMarkingVisitor final
    : public MutatorUnifiedHeapMarkingVisitor {
 public:
  using MutatorUnifiedHeapMarkingVisitor::MutatorUnifiedHeapMarkingVisitor;
  ~MutatorMinorGCMarkingVisitor() override = default;

 protected:
  // Override and make the function empty, since we don't want to trace V8
  // reference during cppgc's minor GC.
  void Visit(const TracedReferenceBase&) final {}
};

class V8_EXPORT_PRIVATE ConcurrentUnifiedHeapMarkingVisitor
    : public UnifiedHeapMarkingVisitorBase {
 public:
  ConcurrentUnifiedHeapMarkingVisitor(HeapBase&, Heap*,
                                      cppgc::internal::ConcurrentMarkingState&);
  ~ConcurrentUnifiedHeapMarkingVisitor() override;

 protected:
  bool DeferTraceToMutatorThreadIfConcurrent(const void*, cppgc::TraceCallback,
                                             size_t) final;

 private:
  // Visitor owns the local worklist. All remaining items are published on
  // destruction of the visitor. This is good enough as concurrent visitation
  // ends before computing the rest of the transitive closure on the main
  // thread. Dynamically allocated as it is only present when the heaps are
  // attached.
  std::unique_ptr<MarkingWorklists::Local> local_marking_worklist_;
  UnifiedHeapMarkingState concurrent_unified_heap_marking_state_;
};

// Same visitor as for full GCs unified heap, but avoids visiting
// TracedReferences.
class V8_EXPORT_PRIVATE ConcurrentMinorGCMarkingVisitor final
    : public ConcurrentUnifiedHeapMarkingVisitor {
 public:
  using ConcurrentUnifiedHeapMarkingVisitor::
      ConcurrentUnifiedHeapMarkingVisitor;

 private:
  // Override and make the function empty, since we don't want to trace V8
  // reference during cppgc's minor GC.
  void Visit(const TracedReferenceBase&) final {}
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VISITOR_H_
