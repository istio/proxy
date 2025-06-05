// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_LOCAL_ISOLATE_H_
#define V8_EXECUTION_LOCAL_ISOLATE_H_

#include "src/base/macros.h"
#include "src/execution/shared-mutex-guard-if-off-thread.h"
#include "src/execution/thread-id.h"
#include "src/handles/handles.h"
#include "src/handles/local-handles.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/local-factory.h"
#include "src/heap/local-heap.h"
#include "src/logging/runtime-call-stats.h"

namespace v8 {

namespace bigint {
class Processor;
}

namespace internal {

class Isolate;
class LocalLogger;
class RuntimeCallStats;

// HiddenLocalFactory parallels Isolate's HiddenFactory
class V8_EXPORT_PRIVATE HiddenLocalFactory : private LocalFactory {
 public:
  // Forward constructors.
  using LocalFactory::LocalFactory;
};

// And Isolate-like class that can be passed in to templated methods that need
// an isolate syntactically, but are usable off-thread.
//
// This class holds an LocalFactory, but is otherwise effectively a stub
// implementation of an Isolate. In particular, it doesn't allow throwing
// exceptions, and hard crashes if you try.
class V8_EXPORT_PRIVATE LocalIsolate final : private HiddenLocalFactory {
 public:
  using HandleScopeType = LocalHandleScope;

  explicit LocalIsolate(Isolate* isolate, ThreadKind kind);
  ~LocalIsolate();

  // Kinda sketchy.
  static LocalIsolate* FromHeap(LocalHeap* heap) {
    return reinterpret_cast<LocalIsolate*>(reinterpret_cast<Address>(heap) -
                                           OFFSET_OF(LocalIsolate, heap_));
  }

  bool is_main_thread() { return heap()->is_main_thread(); }

  LocalHeap* heap() { return &heap_; }

  inline Address cage_base() const;
  inline Address code_cage_base() const;
  inline ReadOnlyHeap* read_only_heap() const;
  inline Object root(RootIndex index) const;
  inline Handle<Object> root_handle(RootIndex index) const;

  StringTable* string_table() const { return isolate_->string_table(); }
  base::SharedMutex* internalized_string_access() {
    return isolate_->internalized_string_access();
  }
  const AstStringConstants* ast_string_constants() {
    return isolate_->ast_string_constants();
  }
  LazyCompileDispatcher* lazy_compile_dispatcher() {
    return isolate_->lazy_compile_dispatcher();
  }
  V8FileLogger* main_thread_logger() {
    // TODO(leszeks): This is needed for logging in ParseInfo. Figure out a way
    // to use the LocalLogger for this instead.
    return isolate_->v8_file_logger();
  }

  v8::internal::LocalFactory* factory() {
    // Upcast to the privately inherited base-class using c-style casts to avoid
    // undefined behavior (as static_cast cannot cast across private bases).
    return (v8::internal::LocalFactory*)this;
  }

  AccountingAllocator* allocator() { return isolate_->allocator(); }

  bool has_pending_exception() const { return false; }

  void RegisterDeserializerStarted();
  void RegisterDeserializerFinished();
  bool has_active_deserializer() const;

  template <typename T>
  Handle<T> Throw(Handle<Object> exception) {
    UNREACHABLE();
  }
  [[noreturn]] void FatalProcessOutOfHeapMemory(const char* location) {
    UNREACHABLE();
  }

  int GetNextScriptId();
#if V8_SFI_HAS_UNIQUE_ID
  int GetNextUniqueSharedFunctionInfoId();
#endif  // V8_SFI_HAS_UNIQUE_ID

  bool is_collecting_type_profile() const;

  // TODO(cbruni): rename this back to logger() once the V8FileLogger
  // refactoring is completed.
  LocalLogger* v8_file_logger() const { return logger_.get(); }
  ThreadId thread_id() const { return thread_id_; }
  Address stack_limit() const { return stack_limit_; }
#ifdef V8_RUNTIME_CALL_STATS
  RuntimeCallStats* runtime_call_stats() const { return runtime_call_stats_; }
#else
  RuntimeCallStats* runtime_call_stats() const { return nullptr; }
#endif
  bigint::Processor* bigint_processor() {
    if (!bigint_processor_) InitializeBigIntProcessor();
    return bigint_processor_;
  }

  bool is_main_thread() const { return heap_.is_main_thread(); }

  // AsIsolate is only allowed on the main-thread.
  Isolate* AsIsolate() {
    DCHECK(is_main_thread());
    DCHECK_EQ(ThreadId::Current(), isolate_->thread_id());
    return isolate_;
  }
  LocalIsolate* AsLocalIsolate() { return this; }

  // TODO(victorgomes): Remove this when/if MacroAssembler supports LocalIsolate
  // only constructor.
  Isolate* GetMainThreadIsolateUnsafe() const { return isolate_; }

  Object* pending_message_address() {
    return isolate_->pending_message_address();
  }

#ifdef V8_INTL_SUPPORT
  // WARNING: This might be out-of-sync with the main-thread.
  const std::string& DefaultLocale();
#endif

 private:
  friend class v8::internal::LocalFactory;
  friend class LocalIsolateFactory;

  void InitializeBigIntProcessor();

  LocalHeap heap_;

  // TODO(leszeks): Extract out the fields of the Isolate we want and store
  // those instead of the whole thing.
  Isolate* const isolate_;

  std::unique_ptr<LocalLogger> logger_;
  ThreadId const thread_id_;
  Address const stack_limit_;

  bigint::Processor* bigint_processor_{nullptr};

#ifdef V8_RUNTIME_CALL_STATS
  base::Optional<WorkerThreadRuntimeCallStatsScope> rcs_scope_;
  RuntimeCallStats* runtime_call_stats_;
#endif
#ifdef V8_INTL_SUPPORT
  std::string default_locale_;
#endif
};

template <base::MutexSharedType kIsShared>
class V8_NODISCARD SharedMutexGuardIfOffThread<LocalIsolate, kIsShared> final {
 public:
  SharedMutexGuardIfOffThread(base::SharedMutex* mutex, LocalIsolate* isolate) {
    DCHECK_NOT_NULL(mutex);
    DCHECK_NOT_NULL(isolate);
    if (!isolate->is_main_thread()) mutex_guard_.emplace(mutex);
  }

  SharedMutexGuardIfOffThread(const SharedMutexGuardIfOffThread&) = delete;
  SharedMutexGuardIfOffThread& operator=(const SharedMutexGuardIfOffThread&) =
      delete;

 private:
  base::Optional<base::SharedMutexGuard<kIsShared>> mutex_guard_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_LOCAL_ISOLATE_H_
