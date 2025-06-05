// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/frames.h"

#include <cstdint>
#include <memory>
#include <sstream>

#include "src/base/bits.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/maglev-safepoint-table.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/safepoint-table.h"
#include "src/common/globals.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/arguments.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames-inl.h"
#include "src/execution/vm-state-inl.h"
#include "src/ic/ic-stats.h"
#include "src/logging/counters.h"
#include "src/objects/code.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/visitors.h"
#include "src/snapshot/embedded/embedded-data-inl.h"
#include "src/strings/string-stream.h"
#include "src/zone/zone-containers.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/debug/debug-wasm-objects.h"
#include "src/wasm/stacks.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects-inl.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

ReturnAddressLocationResolver StackFrame::return_address_location_resolver_ =
    nullptr;

namespace {

Address AddressOf(const StackHandler* handler) {
  Address raw = handler->address();
#ifdef V8_USE_ADDRESS_SANITIZER
  // ASan puts C++-allocated StackHandler markers onto its fake stack.
  // We work around that by storing the real stack address in the "padding"
  // field. StackHandlers allocated from generated code have 0 as padding.
  Address padding =
      base::Memory<Address>(raw + StackHandlerConstants::kPaddingOffset);
  if (padding != 0) return padding;
#endif
  return raw;
}

}  // namespace

// Iterator that supports traversing the stack handlers of a
// particular frame. Needs to know the top of the handler chain.
class StackHandlerIterator {
 public:
  StackHandlerIterator(const StackFrame* frame, StackHandler* handler)
      : limit_(frame->fp()), handler_(handler) {
#if V8_ENABLE_WEBASSEMBLY
    // Make sure the handler has already been unwound to this frame. With stack
    // switching this is not equivalent to the inequality below, because the
    // frame and the handler could be in different stacks.
    DCHECK_IMPLIES(!v8_flags.experimental_wasm_stack_switching,
                   frame->sp() <= AddressOf(handler));
    // For CWasmEntry frames, the handler was registered by the last C++
    // frame (Execution::CallWasm), so even though its address is already
    // beyond the limit, we know we always want to unwind one handler.
    if (frame->is_c_wasm_entry()) handler_ = handler_->next();
#else
    // Make sure the handler has already been unwound to this frame.
    DCHECK_LE(frame->sp(), AddressOf(handler));
#endif  // V8_ENABLE_WEBASSEMBLY
  }

  StackHandler* handler() const { return handler_; }

  bool done() { return handler_ == nullptr || AddressOf(handler_) > limit_; }
  void Advance() {
    DCHECK(!done());
    handler_ = handler_->next();
  }

 private:
  const Address limit_;
  StackHandler* handler_;
};

// -------------------------------------------------------------------------

#define INITIALIZE_SINGLETON(type, field) field##_(this),
StackFrameIteratorBase::StackFrameIteratorBase(Isolate* isolate,
                                               bool can_access_heap_objects)
    : isolate_(isolate),
      STACK_FRAME_TYPE_LIST(INITIALIZE_SINGLETON) frame_(nullptr),
      handler_(nullptr),
      can_access_heap_objects_(can_access_heap_objects) {}
#undef INITIALIZE_SINGLETON

StackFrameIterator::StackFrameIterator(Isolate* isolate)
    : StackFrameIterator(isolate, isolate->thread_local_top()) {}

StackFrameIterator::StackFrameIterator(Isolate* isolate, ThreadLocalTop* t)
    : StackFrameIteratorBase(isolate, true) {
  Reset(t);
}
#if V8_ENABLE_WEBASSEMBLY
StackFrameIterator::StackFrameIterator(Isolate* isolate,
                                       wasm::StackMemory* stack)
    : StackFrameIteratorBase(isolate, true) {
  Reset(isolate->thread_local_top(), stack);
}
#endif

void StackFrameIterator::Advance() {
  DCHECK(!done());
  // Compute the state of the calling frame before restoring
  // callee-saved registers and unwinding handlers. This allows the
  // frame code that computes the caller state to access the top
  // handler and the value of any callee-saved register if needed.
  StackFrame::State state;
  StackFrame::Type type = frame_->GetCallerState(&state);

  // Unwind handlers corresponding to the current frame.
  StackHandlerIterator it(frame_, handler_);
  while (!it.done()) it.Advance();
  handler_ = it.handler();

  // Advance to the calling frame.
  frame_ = SingletonFor(type, &state);

  // When we're done iterating over the stack frames, the handler
  // chain must have been completely unwound. Except for wasm stack-switching:
  // we stop at the end of the current segment.
#if V8_ENABLE_WEBASSEMBLY
  DCHECK_IMPLIES(done() && !v8_flags.experimental_wasm_stack_switching,
                 handler_ == nullptr);
#else
  DCHECK_IMPLIES(done(), handler_ == nullptr);
#endif
}

StackFrame* StackFrameIterator::Reframe() {
  StackFrame::Type type = frame_->ComputeType(this, &frame_->state_);
  frame_ = SingletonFor(type, &frame_->state_);
  return frame();
}

void StackFrameIterator::Reset(ThreadLocalTop* top) {
  StackFrame::State state;
  StackFrame::Type type =
      ExitFrame::GetStateForFramePointer(Isolate::c_entry_fp(top), &state);
  handler_ = StackHandler::FromAddress(Isolate::handler(top));
  frame_ = SingletonFor(type, &state);
}

#if V8_ENABLE_WEBASSEMBLY
void StackFrameIterator::Reset(ThreadLocalTop* top, wasm::StackMemory* stack) {
  if (stack->jmpbuf()->state == wasm::JumpBuffer::Retired) {
    return;
  }
  StackFrame::State state;
  StackSwitchFrame::GetStateForJumpBuffer(stack->jmpbuf(), &state);
  handler_ = StackHandler::FromAddress(Isolate::handler(top));
  frame_ = SingletonFor(StackFrame::STACK_SWITCH, &state);
}
#endif

StackFrame* StackFrameIteratorBase::SingletonFor(StackFrame::Type type,
                                                 StackFrame::State* state) {
  StackFrame* result = SingletonFor(type);
  DCHECK((!result) == (type == StackFrame::NO_FRAME_TYPE));
  if (result) result->state_ = *state;
  return result;
}

StackFrame* StackFrameIteratorBase::SingletonFor(StackFrame::Type type) {
#define FRAME_TYPE_CASE(type, field) \
  case StackFrame::type:             \
    return &field##_;

  switch (type) {
    case StackFrame::NO_FRAME_TYPE:
      return nullptr;
      STACK_FRAME_TYPE_LIST(FRAME_TYPE_CASE)
    default:
      break;
  }
  return nullptr;

#undef FRAME_TYPE_CASE
}

// -------------------------------------------------------------------------

void TypedFrameWithJSLinkage::Iterate(RootVisitor* v) const {
  IterateExpressions(v);
  IteratePc(v, pc_address(), constant_pool_address(), LookupCodeT());
}

// -------------------------------------------------------------------------

void JavaScriptFrameIterator::Advance() {
  do {
    iterator_.Advance();
  } while (!iterator_.done() && !iterator_.frame()->is_java_script());
}

// -------------------------------------------------------------------------

StackTraceFrameIterator::StackTraceFrameIterator(Isolate* isolate)
    : iterator_(isolate) {
  if (!done() && !IsValidFrame(iterator_.frame())) Advance();
}

StackTraceFrameIterator::StackTraceFrameIterator(Isolate* isolate,
                                                 StackFrameId id)
    : StackTraceFrameIterator(isolate) {
  while (!done() && frame()->id() != id) Advance();
}

void StackTraceFrameIterator::Advance() {
  do {
    iterator_.Advance();
  } while (!done() && !IsValidFrame(iterator_.frame()));
}

int StackTraceFrameIterator::FrameFunctionCount() const {
  DCHECK(!done());
  if (!iterator_.frame()->is_optimized()) return 1;
  std::vector<SharedFunctionInfo> infos;
  TurbofanFrame::cast(iterator_.frame())->GetFunctions(&infos);
  return static_cast<int>(infos.size());
}

FrameSummary StackTraceFrameIterator::GetTopValidFrame() const {
  DCHECK(!done());
  // Like FrameSummary::GetTop, but additionally observes
  // StackTraceFrameIterator filtering semantics.
  std::vector<FrameSummary> frames;
  frame()->Summarize(&frames);
  if (is_javascript()) {
    for (int i = static_cast<int>(frames.size()) - 1; i >= 0; i--) {
      if (!IsValidJSFunction(*frames[i].AsJavaScript().function())) continue;
      return frames[i];
    }
    UNREACHABLE();
  }
#if V8_ENABLE_WEBASSEMBLY
  if (is_wasm()) return frames.back();
#endif  // V8_ENABLE_WEBASSEMBLY
  UNREACHABLE();
}

// static
bool StackTraceFrameIterator::IsValidFrame(StackFrame* frame) {
  if (frame->is_java_script()) {
    return IsValidJSFunction(static_cast<JavaScriptFrame*>(frame)->function());
  }
#if V8_ENABLE_WEBASSEMBLY
  if (frame->is_wasm()) return true;
#endif  // V8_ENABLE_WEBASSEMBLY
  return false;
}

// static
bool StackTraceFrameIterator::IsValidJSFunction(JSFunction f) {
  if (!f.IsJSFunction()) return false;
  return f.shared().IsSubjectToDebugging();
}

// -------------------------------------------------------------------------

namespace {

bool IsInterpreterFramePc(Isolate* isolate, Address pc,
                          StackFrame::State* state) {
  Builtin builtin = OffHeapInstructionStream::TryLookupCode(isolate, pc);
  if (builtin != Builtin::kNoBuiltinId &&
      (builtin == Builtin::kInterpreterEntryTrampoline ||
       builtin == Builtin::kInterpreterEnterAtBytecode ||
       builtin == Builtin::kInterpreterEnterAtNextBytecode ||
       builtin == Builtin::kBaselineOrInterpreterEnterAtBytecode ||
       builtin == Builtin::kBaselineOrInterpreterEnterAtNextBytecode)) {
    return true;
  } else if (v8_flags.interpreted_frames_native_stack) {
    intptr_t marker = Memory<intptr_t>(
        state->fp + CommonFrameConstants::kContextOrFrameTypeOffset);
    MSAN_MEMORY_IS_INITIALIZED(
        state->fp + StandardFrameConstants::kFunctionOffset,
        kSystemPointerSize);
    Object maybe_function = Object(
        Memory<Address>(state->fp + StandardFrameConstants::kFunctionOffset));
    // There's no need to run a full ContainsSlow if we know the frame can't be
    // an InterpretedFrame,  so we do these fast checks first
    if (StackFrame::IsTypeMarker(marker) || maybe_function.IsSmi()) {
      return false;
    } else if (!isolate->heap()->InSpaceSlow(pc, CODE_SPACE)) {
      return false;
    }
    CodeLookupResult interpreter_entry_trampoline =
        isolate->heap()->GcSafeFindCodeForInnerPointer(pc);
    return interpreter_entry_trampoline.code()
        .is_interpreter_trampoline_builtin();
  } else {
    return false;
  }
}

}  // namespace

bool SafeStackFrameIterator::IsNoFrameBytecodeHandlerPc(Isolate* isolate,
                                                        Address pc,
                                                        Address fp) const {
  // Return false for builds with non-embedded bytecode handlers.
  if (Isolate::CurrentEmbeddedBlobCode() == nullptr) return false;

  EmbeddedData d = EmbeddedData::FromBlob(isolate);
  if (pc < d.InstructionStartOfBytecodeHandlers() ||
      pc >= d.InstructionEndOfBytecodeHandlers()) {
    // Not a bytecode handler pc address.
    return false;
  }

  if (!IsValidStackAddress(fp +
                           CommonFrameConstants::kContextOrFrameTypeOffset)) {
    return false;
  }

  // Check if top stack frame is a bytecode handler stub frame.
  MSAN_MEMORY_IS_INITIALIZED(
      fp + CommonFrameConstants::kContextOrFrameTypeOffset, kSystemPointerSize);
  intptr_t marker =
      Memory<intptr_t>(fp + CommonFrameConstants::kContextOrFrameTypeOffset);
  if (StackFrame::IsTypeMarker(marker) &&
      StackFrame::MarkerToType(marker) == StackFrame::STUB) {
    // Bytecode handler built a frame.
    return false;
  }
  return true;
}

SafeStackFrameIterator::SafeStackFrameIterator(Isolate* isolate, Address pc,
                                               Address fp, Address sp,
                                               Address lr, Address js_entry_sp)
    : StackFrameIteratorBase(isolate, false),
      low_bound_(sp),
      high_bound_(js_entry_sp),
      top_frame_type_(StackFrame::NO_FRAME_TYPE),
      top_context_address_(kNullAddress),
      external_callback_scope_(isolate->external_callback_scope()),
      top_link_register_(lr) {
  StackFrame::State state;
  StackFrame::Type type;
  ThreadLocalTop* top = isolate->thread_local_top();
  bool advance_frame = true;

  Address fast_c_fp = isolate->isolate_data()->fast_c_call_caller_fp();
  uint8_t stack_is_iterable = isolate->isolate_data()->stack_is_iterable();
  if (!stack_is_iterable) {
    frame_ = nullptr;
    return;
  }
  // 'Fast C calls' are a special type of C call where we call directly from
  // JS to C without an exit frame inbetween. The CEntryStub is responsible
  // for setting Isolate::c_entry_fp, meaning that it won't be set for fast C
  // calls. To keep the stack iterable, we store the FP and PC of the caller
  // of the fast C call on the isolate. This is guaranteed to be the topmost
  // JS frame, because fast C calls cannot call back into JS. We start
  // iterating the stack from this topmost JS frame.
  if (fast_c_fp) {
    DCHECK_NE(kNullAddress, isolate->isolate_data()->fast_c_call_caller_pc());
    type = StackFrame::Type::TURBOFAN;
    top_frame_type_ = type;
    state.fp = fast_c_fp;
    state.sp = sp;
    state.pc_address = reinterpret_cast<Address*>(
        isolate->isolate_data()->fast_c_call_caller_pc_address());
    advance_frame = false;
  } else if (IsValidTop(top)) {
    type = ExitFrame::GetStateForFramePointer(Isolate::c_entry_fp(top), &state);
    top_frame_type_ = type;
  } else if (IsValidStackAddress(fp)) {
    DCHECK_NE(fp, kNullAddress);
    state.fp = fp;
    state.sp = sp;
    state.pc_address = StackFrame::ResolveReturnAddressLocation(
        reinterpret_cast<Address*>(CommonFrame::ComputePCAddress(fp)));

    // If the current PC is in a bytecode handler, the top stack frame isn't
    // the bytecode handler's frame and the top of stack or link register is a
    // return address into the interpreter entry trampoline, then we are likely
    // in a bytecode handler with elided frame. In that case, set the PC
    // properly and make sure we do not drop the frame.
    bool is_no_frame_bytecode_handler = false;
    if (IsNoFrameBytecodeHandlerPc(isolate, pc, fp)) {
      Address* tos_location = nullptr;
      if (top_link_register_) {
        tos_location = &top_link_register_;
      } else if (IsValidStackAddress(sp)) {
        MSAN_MEMORY_IS_INITIALIZED(sp, kSystemPointerSize);
        tos_location = reinterpret_cast<Address*>(sp);
      }

      if (IsInterpreterFramePc(isolate, *tos_location, &state)) {
        state.pc_address = tos_location;
        is_no_frame_bytecode_handler = true;
        advance_frame = false;
      }
    }

    // StackFrame::ComputeType will read both kContextOffset and kMarkerOffset,
    // we check only that kMarkerOffset is within the stack bounds and do
    // compile time check that kContextOffset slot is pushed on the stack before
    // kMarkerOffset.
    static_assert(StandardFrameConstants::kFunctionOffset <
                  StandardFrameConstants::kContextOffset);
    Address frame_marker = fp + StandardFrameConstants::kFunctionOffset;
    if (IsValidStackAddress(frame_marker)) {
      if (is_no_frame_bytecode_handler) {
        type = StackFrame::INTERPRETED;
      } else {
        type = StackFrame::ComputeType(this, &state);
      }
      top_frame_type_ = type;
      MSAN_MEMORY_IS_INITIALIZED(
          fp + CommonFrameConstants::kContextOrFrameTypeOffset,
          kSystemPointerSize);
      Address type_or_context_address =
          Memory<Address>(fp + CommonFrameConstants::kContextOrFrameTypeOffset);
      if (!StackFrame::IsTypeMarker(type_or_context_address))
        top_context_address_ = type_or_context_address;
    } else {
      // Mark the frame as TURBOFAN if we cannot determine its type.
      // We chose TURBOFAN rather than INTERPRETED because it's closer to
      // the original value of StackFrame::JAVA_SCRIPT here, in that JAVA_SCRIPT
      // referred to full-codegen frames (now removed from the tree), and
      // TURBOFAN refers to turbofan frames, both of which are generated
      // code. INTERPRETED frames refer to bytecode.
      // The frame anyways will be skipped.
      type = StackFrame::TURBOFAN;
      // Top frame is incomplete so we cannot reliably determine its type.
      top_frame_type_ = StackFrame::NO_FRAME_TYPE;
    }
  } else {
    return;
  }
  frame_ = SingletonFor(type, &state);
  if (advance_frame && frame_) Advance();
}

bool SafeStackFrameIterator::IsValidTop(ThreadLocalTop* top) const {
  Address c_entry_fp = Isolate::c_entry_fp(top);
  if (!IsValidExitFrame(c_entry_fp)) return false;
  // There should be at least one JS_ENTRY stack handler.
  Address handler = Isolate::handler(top);
  if (handler == kNullAddress) return false;
  // Check that there are no js frames on top of the native frames.
  return c_entry_fp < handler;
}

void SafeStackFrameIterator::AdvanceOneFrame() {
  DCHECK(!done());
  StackFrame* last_frame = frame_;
  Address last_sp = last_frame->sp(), last_fp = last_frame->fp();

  // Before advancing to the next stack frame, perform pointer validity tests.
  if (!IsValidFrame(last_frame) || !IsValidCaller(last_frame)) {
    frame_ = nullptr;
    return;
  }

  // Advance to the previous frame.
  StackFrame::State state;
  StackFrame::Type type = frame_->GetCallerState(&state);
  frame_ = SingletonFor(type, &state);
  if (!frame_) return;

  // Check that we have actually moved to the previous frame in the stack.
  if (frame_->sp() <= last_sp || frame_->fp() <= last_fp) {
    frame_ = nullptr;
  }
}

bool SafeStackFrameIterator::IsValidFrame(StackFrame* frame) const {
  return IsValidStackAddress(frame->sp()) && IsValidStackAddress(frame->fp());
}

bool SafeStackFrameIterator::IsValidCaller(StackFrame* frame) {
  StackFrame::State state;
  if (frame->is_entry() || frame->is_construct_entry()) {
    // See EntryFrame::GetCallerState. It computes the caller FP address
    // and calls ExitFrame::GetStateForFramePointer on it. We need to be
    // sure that caller FP address is valid.
    Address caller_fp =
        Memory<Address>(frame->fp() + EntryFrameConstants::kCallerFPOffset);
    if (!IsValidExitFrame(caller_fp)) return false;
  }
  frame->ComputeCallerState(&state);
  return IsValidStackAddress(state.sp) && IsValidStackAddress(state.fp) &&
         SingletonFor(frame->GetCallerState(&state)) != nullptr;
}

bool SafeStackFrameIterator::IsValidExitFrame(Address fp) const {
  if (!IsValidStackAddress(fp)) return false;
  Address sp = ExitFrame::ComputeStackPointer(fp);
  if (!IsValidStackAddress(sp)) return false;
  StackFrame::State state;
  ExitFrame::FillState(fp, sp, &state);
  MSAN_MEMORY_IS_INITIALIZED(state.pc_address, sizeof(state.pc_address));
  return *state.pc_address != kNullAddress;
}

void SafeStackFrameIterator::Advance() {
  while (true) {
    AdvanceOneFrame();
    if (done()) break;
    ExternalCallbackScope* last_callback_scope = nullptr;
    while (external_callback_scope_ != nullptr &&
           external_callback_scope_->scope_address() < frame_->fp()) {
      // As long as the setup of a frame is not atomic, we may happen to be
      // in an interval where an ExternalCallbackScope is already created,
      // but the frame is not yet entered. So we are actually observing
      // the previous frame.
      // Skip all the ExternalCallbackScope's that are below the current fp.
      last_callback_scope = external_callback_scope_;
      external_callback_scope_ = external_callback_scope_->previous();
    }
    if (frame_->is_java_script()) break;
#if V8_ENABLE_WEBASSEMBLY
    if (frame_->is_wasm() || frame_->is_wasm_to_js() ||
        frame_->is_js_to_wasm()) {
      break;
    }
#endif  // V8_ENABLE_WEBASSEMBLY
    if (frame_->is_exit() || frame_->is_builtin_exit()) {
      // Some of the EXIT frames may have ExternalCallbackScope allocated on
      // top of them. In that case the scope corresponds to the first EXIT
      // frame beneath it. There may be other EXIT frames on top of the
      // ExternalCallbackScope, just skip them as we cannot collect any useful
      // information about them.
      if (last_callback_scope) {
        frame_->state_.pc_address =
            last_callback_scope->callback_entrypoint_address();
      }
      break;
    }
  }
}

// -------------------------------------------------------------------------

namespace {
CodeLookupResult GetContainingCode(Isolate* isolate, Address pc) {
  return isolate->inner_pointer_to_code_cache()->GetCacheEntry(pc)->code;
}
}  // namespace

CodeLookupResult StackFrame::LookupCodeT() const {
  CodeLookupResult result = GetContainingCode(isolate(), pc());
  if (DEBUG_BOOL) {
    CHECK(result.IsFound());
    if (result.IsCode()) {
      Code code = result.code();
      CHECK_GE(pc(), code.InstructionStart(isolate(), pc()));
      CHECK_LT(pc(), code.InstructionEnd(isolate(), pc()));
    } else {
#ifdef V8_EXTERNAL_CODE_SPACE
      CodeDataContainer code = result.code_data_container();
      CHECK_GE(pc(), code.InstructionStart(isolate(), pc()));
      CHECK_LT(pc(), code.InstructionEnd(isolate(), pc()));
#endif
    }
  }
  return result;
}

void StackFrame::IteratePc(RootVisitor* v, Address* pc_address,
                           Address* constant_pool_address,
                           CodeLookupResult lookup_result) const {
  if (lookup_result.IsCodeDataContainer()) {
    // The embeded builtins are immovable, so there's no need to update PCs on
    // the stack, just visit the CodeT object.
    Object code = lookup_result.code_data_container();
    v->VisitRunningCode(FullObjectSlot(&code));
    return;
  }
  Code holder = lookup_result.code();
  Address old_pc = ReadPC(pc_address);
  DCHECK(ReadOnlyHeap::Contains(holder) ||
         holder.GetHeap()->GcSafeCodeContains(holder, old_pc));
  unsigned pc_offset = holder.GetOffsetFromInstructionStart(isolate_, old_pc);
  Object code = holder;
  v->VisitRunningCode(FullObjectSlot(&code));
  if (code == holder) return;
  holder = Code::unchecked_cast(code);
  Address pc = holder.InstructionStart(isolate_, old_pc) + pc_offset;
  // TODO(v8:10026): avoid replacing a signed pointer.
  PointerAuthentication::ReplacePC(pc_address, pc, kSystemPointerSize);
  if (v8_flags.enable_embedded_constant_pool && constant_pool_address) {
    *constant_pool_address = holder.constant_pool();
  }
}

void StackFrame::SetReturnAddressLocationResolver(
    ReturnAddressLocationResolver resolver) {
  DCHECK_NULL(return_address_location_resolver_);
  return_address_location_resolver_ = resolver;
}

namespace {

template <typename CodeOrCodeT>
inline StackFrame::Type ComputeBuiltinFrameType(CodeOrCodeT code) {
  if (code.is_interpreter_trampoline_builtin() ||
      // Frames for baseline entry trampolines on the stack are still
      // interpreted frames.
      code.is_baseline_trampoline_builtin()) {
    return StackFrame::INTERPRETED;
  }
  if (code.is_baseline_leave_frame_builtin()) {
    return StackFrame::BASELINE;
  }
  if (code.is_turbofanned()) {
    // TODO(bmeurer): We treat frames for BUILTIN Code objects as
    // OptimizedFrame for now (all the builtins with JavaScript
    // linkage are actually generated with TurboFan currently, so
    // this is sound).
    return StackFrame::TURBOFAN;
  }
  return StackFrame::BUILTIN;
}

}  // namespace

StackFrame::Type StackFrame::ComputeType(const StackFrameIteratorBase* iterator,
                                         State* state) {
#if V8_ENABLE_WEBASSEMBLY
  if (state->fp == kNullAddress) {
    DCHECK(v8_flags.experimental_wasm_stack_switching);
    return NO_FRAME_TYPE;
  }
#endif

  MSAN_MEMORY_IS_INITIALIZED(
      state->fp + CommonFrameConstants::kContextOrFrameTypeOffset,
      kSystemPointerSize);
  intptr_t marker = Memory<intptr_t>(
      state->fp + CommonFrameConstants::kContextOrFrameTypeOffset);
  Address pc = StackFrame::ReadPC(state->pc_address);
  if (!iterator->can_access_heap_objects_) {
    // TODO(titzer): "can_access_heap_objects" is kind of bogus. It really
    // means that we are being called from the profiler, which can interrupt
    // the VM with a signal at any arbitrary instruction, with essentially
    // anything on the stack. So basically none of these checks are 100%
    // reliable.
    MSAN_MEMORY_IS_INITIALIZED(
        state->fp + StandardFrameConstants::kFunctionOffset,
        kSystemPointerSize);
    Object maybe_function = Object(
        Memory<Address>(state->fp + StandardFrameConstants::kFunctionOffset));
    if (!StackFrame::IsTypeMarker(marker)) {
      if (maybe_function.IsSmi()) {
        return NATIVE;
      } else if (IsInterpreterFramePc(iterator->isolate(), pc, state)) {
        return INTERPRETED;
      } else {
        return TURBOFAN;
      }
    }
  } else {
#if V8_ENABLE_WEBASSEMBLY
    // If the {pc} does not point into WebAssembly code we can rely on the
    // returned {wasm_code} to be null and fall back to {GetContainingCode}.
    wasm::WasmCodeRefScope code_ref_scope;
    if (wasm::WasmCode* wasm_code =
            wasm::GetWasmCodeManager()->LookupCode(pc)) {
      switch (wasm_code->kind()) {
        case wasm::WasmCode::kWasmFunction:
          return WASM;
        case wasm::WasmCode::kWasmToCapiWrapper:
          return WASM_EXIT;
        case wasm::WasmCode::kWasmToJsWrapper:
          return WASM_TO_JS;
        default:
          UNREACHABLE();
      }
    }
#endif  // V8_ENABLE_WEBASSEMBLY

    // Look up the code object to figure out the type of the stack frame.
    CodeLookupResult lookup_result = GetContainingCode(iterator->isolate(), pc);
    if (lookup_result.IsFound()) {
      switch (lookup_result.kind()) {
        case CodeKind::BUILTIN: {
          if (StackFrame::IsTypeMarker(marker)) break;
          // We can't use lookup_result.ToCodeT() because we might in the
          // middle of GC.
          if (lookup_result.IsCodeDataContainer()) {
            return ComputeBuiltinFrameType(
                CodeT::cast(lookup_result.code_data_container()));
          }
          return ComputeBuiltinFrameType(lookup_result.code());
        }
        case CodeKind::BASELINE:
          return BASELINE;
        case CodeKind::MAGLEV:
          if (IsTypeMarker(marker)) {
            // An INTERNAL frame can be set up with an associated Maglev code
            // object when calling into runtime to handle tiering. In this case,
            // all stack slots are tagged pointers and should be visited through
            // the usual logic.
            DCHECK_EQ(MarkerToType(marker), StackFrame::INTERNAL);
            return StackFrame::INTERNAL;
          }
          return MAGLEV;
        case CodeKind::TURBOFAN:
          return TURBOFAN;
#if V8_ENABLE_WEBASSEMBLY
        case CodeKind::JS_TO_WASM_FUNCTION:
          if (lookup_result.builtin_id() == Builtin::kGenericJSToWasmWrapper) {
            return JS_TO_WASM;
          } else {
            return TURBOFAN_STUB_WITH_CONTEXT;
          }
        case CodeKind::JS_TO_JS_FUNCTION:
          return TURBOFAN_STUB_WITH_CONTEXT;
        case CodeKind::C_WASM_ENTRY:
          return C_WASM_ENTRY;
        case CodeKind::WASM_TO_JS_FUNCTION:
          return WASM_TO_JS_FUNCTION;
        case CodeKind::WASM_FUNCTION:
        case CodeKind::WASM_TO_CAPI_FUNCTION:
          // Never appear as on-heap {Code} objects.
          UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
        default:
          // All other types should have an explicit marker
          break;
      }
    } else {
      return NATIVE;
    }
  }
  DCHECK(StackFrame::IsTypeMarker(marker));
  StackFrame::Type candidate = StackFrame::MarkerToType(marker);
  switch (candidate) {
    case ENTRY:
    case CONSTRUCT_ENTRY:
    case EXIT:
    case BUILTIN_CONTINUATION:
    case JAVA_SCRIPT_BUILTIN_CONTINUATION:
    case JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH:
    case BUILTIN_EXIT:
    case STUB:
    case INTERNAL:
    case CONSTRUCT:
#if V8_ENABLE_WEBASSEMBLY
    case WASM_TO_JS:
    case WASM:
    case WASM_COMPILE_LAZY:
    case WASM_EXIT:
    case WASM_DEBUG_BREAK:
    case JS_TO_WASM:
    case STACK_SWITCH:
#endif  // V8_ENABLE_WEBASSEMBLY
      return candidate;

    // Any other marker value is likely to be a bogus stack frame when being
    // called from the profiler (in particular, JavaScript frames, including
    // interpreted frames, should never have a StackFrame::Type
    // marker). Consider these frames "native".
    default:
      return NATIVE;
  }
}

#ifdef DEBUG
bool StackFrame::can_access_heap_objects() const {
  return iterator_->can_access_heap_objects_;
}
#endif

StackFrame::Type StackFrame::GetCallerState(State* state) const {
  ComputeCallerState(state);
  return ComputeType(iterator_, state);
}

Address CommonFrame::GetCallerStackPointer() const {
  return fp() + CommonFrameConstants::kCallerSPOffset;
}

void NativeFrame::ComputeCallerState(State* state) const {
  state->sp = caller_sp();
  state->fp = Memory<Address>(fp() + CommonFrameConstants::kCallerFPOffset);
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(fp() + CommonFrameConstants::kCallerPCOffset));
  state->callee_pc_address = nullptr;
  state->constant_pool_address = nullptr;
}

HeapObject EntryFrame::unchecked_code() const {
  return isolate()->builtins()->code(Builtin::kJSEntry);
}

void EntryFrame::ComputeCallerState(State* state) const {
  GetCallerState(state);
}

StackFrame::Type EntryFrame::GetCallerState(State* state) const {
  const int offset = EntryFrameConstants::kCallerFPOffset;
  Address fp = Memory<Address>(this->fp() + offset);
  return ExitFrame::GetStateForFramePointer(fp, state);
}

#if V8_ENABLE_WEBASSEMBLY
StackFrame::Type CWasmEntryFrame::GetCallerState(State* state) const {
  const int offset = CWasmEntryFrameConstants::kCEntryFPOffset;
  Address fp = Memory<Address>(this->fp() + offset);
  return ExitFrame::GetStateForFramePointer(fp, state);
}
#endif  // V8_ENABLE_WEBASSEMBLY

HeapObject ConstructEntryFrame::unchecked_code() const {
  return isolate()->builtins()->code(Builtin::kJSConstructEntry);
}

void ExitFrame::ComputeCallerState(State* state) const {
  // Set up the caller state.
  state->sp = caller_sp();
  state->fp = Memory<Address>(fp() + ExitFrameConstants::kCallerFPOffset);
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(fp() + ExitFrameConstants::kCallerPCOffset));
  state->callee_pc_address = nullptr;
  if (v8_flags.enable_embedded_constant_pool) {
    state->constant_pool_address = reinterpret_cast<Address*>(
        fp() + ExitFrameConstants::kConstantPoolOffset);
  }
}

void ExitFrame::Iterate(RootVisitor* v) const {
  // The arguments are traversed as part of the expression stack of
  // the calling frame.
  IteratePc(v, pc_address(), constant_pool_address(), LookupCodeT());
}

StackFrame::Type ExitFrame::GetStateForFramePointer(Address fp, State* state) {
  if (fp == 0) return NO_FRAME_TYPE;
  StackFrame::Type type = ComputeFrameType(fp);
#if V8_ENABLE_WEBASSEMBLY
  Address sp = type == WASM_EXIT ? WasmExitFrame::ComputeStackPointer(fp)
                                 : ExitFrame::ComputeStackPointer(fp);
#else
  Address sp = ExitFrame::ComputeStackPointer(fp);
#endif  // V8_ENABLE_WEBASSEMBLY
  FillState(fp, sp, state);
  DCHECK_NE(*state->pc_address, kNullAddress);
  return type;
}

StackFrame::Type ExitFrame::ComputeFrameType(Address fp) {
  // Distinguish between between regular and builtin exit frames.
  // Default to EXIT in all hairy cases (e.g., when called from profiler).
  const int offset = ExitFrameConstants::kFrameTypeOffset;
  Object marker(Memory<Address>(fp + offset));

  if (!marker.IsSmi()) {
    return EXIT;
  }

  intptr_t marker_int = base::bit_cast<intptr_t>(marker);

  StackFrame::Type frame_type = static_cast<StackFrame::Type>(marker_int >> 1);
  switch (frame_type) {
    case BUILTIN_EXIT:
#if V8_ENABLE_WEBASSEMBLY
    case WASM_EXIT:
    case STACK_SWITCH:
#endif  // V8_ENABLE_WEBASSEMBLY
      return frame_type;
    default:
      return EXIT;
  }
}

Address ExitFrame::ComputeStackPointer(Address fp) {
  MSAN_MEMORY_IS_INITIALIZED(fp + ExitFrameConstants::kSPOffset,
                             kSystemPointerSize);
  return Memory<Address>(fp + ExitFrameConstants::kSPOffset);
}

#if V8_ENABLE_WEBASSEMBLY
Address WasmExitFrame::ComputeStackPointer(Address fp) {
  // For WASM_EXIT frames, {sp} is only needed for finding the PC slot,
  // everything else is handled via safepoint information.
  Address sp = fp + WasmExitFrameConstants::kWasmInstanceOffset;
  DCHECK_EQ(sp - 1 * kPCOnStackSize,
            fp + WasmExitFrameConstants::kCallingPCOffset);
  return sp;
}
#endif  // V8_ENABLE_WEBASSEMBLY

void ExitFrame::FillState(Address fp, Address sp, State* state) {
  state->sp = sp;
  state->fp = fp;
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(sp - 1 * kPCOnStackSize));
  state->callee_pc_address = nullptr;
  // The constant pool recorded in the exit frame is not associated
  // with the pc in this state (the return address into a C entry
  // stub).  ComputeCallerState will retrieve the constant pool
  // together with the associated caller pc.
  state->constant_pool_address = nullptr;
}

void BuiltinExitFrame::Summarize(std::vector<FrameSummary>* frames) const {
  DCHECK(frames->empty());
  Handle<FixedArray> parameters = GetParameters();
  DisallowGarbageCollection no_gc;
  CodeLookupResult code = LookupCodeT();
  int code_offset = code.GetOffsetFromInstructionStart(isolate(), pc());
  FrameSummary::JavaScriptFrameSummary summary(
      isolate(), receiver(), function(), code.ToAbstractCode(), code_offset,
      IsConstructor(), *parameters);
  frames->push_back(summary);
}

JSFunction BuiltinExitFrame::function() const {
  return JSFunction::cast(target_slot_object());
}

Object BuiltinExitFrame::receiver() const { return receiver_slot_object(); }

Object BuiltinExitFrame::GetParameter(int i) const {
  DCHECK(i >= 0 && i < ComputeParametersCount());
  int offset =
      BuiltinExitFrameConstants::kFirstArgumentOffset + i * kSystemPointerSize;
  return Object(Memory<Address>(fp() + offset));
}

int BuiltinExitFrame::ComputeParametersCount() const {
  Object argc_slot = argc_slot_object();
  DCHECK(argc_slot.IsSmi());
  // Argc also counts the receiver, target, new target, and argc itself as args,
  // therefore the real argument count is argc - 4.
  int argc = Smi::ToInt(argc_slot) - 4;
  DCHECK_GE(argc, 0);
  return argc;
}

Handle<FixedArray> BuiltinExitFrame::GetParameters() const {
  if (V8_LIKELY(!v8_flags.detailed_error_stack_trace)) {
    return isolate()->factory()->empty_fixed_array();
  }
  int param_count = ComputeParametersCount();
  auto parameters = isolate()->factory()->NewFixedArray(param_count);
  for (int i = 0; i < param_count; i++) {
    parameters->set(i, GetParameter(i));
  }
  return parameters;
}

bool BuiltinExitFrame::IsConstructor() const {
  return !new_target_slot_object().IsUndefined(isolate());
}

namespace {
void PrintIndex(StringStream* accumulator, StackFrame::PrintMode mode,
                int index) {
  accumulator->Add((mode == StackFrame::OVERVIEW) ? "%5d: " : "[%d]: ", index);
}

const char* StringForStackFrameType(StackFrame::Type type) {
  switch (type) {
#define CASE(value, name) \
  case StackFrame::value: \
    return #name;
    STACK_FRAME_TYPE_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}
}  // namespace

void StackFrame::Print(StringStream* accumulator, PrintMode mode,
                       int index) const {
  DisallowGarbageCollection no_gc;
  PrintIndex(accumulator, mode, index);
  accumulator->Add(StringForStackFrameType(type()));
  accumulator->Add(" [pc: %p]\n", reinterpret_cast<void*>(pc()));
}

void BuiltinExitFrame::Print(StringStream* accumulator, PrintMode mode,
                             int index) const {
  DisallowGarbageCollection no_gc;
  Object receiver = this->receiver();
  JSFunction function = this->function();

  accumulator->PrintSecurityTokenIfChanged(function);
  PrintIndex(accumulator, mode, index);
  accumulator->Add("builtin exit frame: ");
  if (IsConstructor()) accumulator->Add("new ");
  accumulator->PrintFunction(function, receiver);

  accumulator->Add("(this=%o", receiver);

  // Print the parameters.
  int parameters_count = ComputeParametersCount();
  for (int i = 0; i < parameters_count; i++) {
    accumulator->Add(",%o", GetParameter(i));
  }

  accumulator->Add(")\n\n");
}

Address CommonFrame::GetExpressionAddress(int n) const {
  const int offset = StandardFrameConstants::kExpressionsOffset;
  return fp() + offset - n * kSystemPointerSize;
}

Address UnoptimizedFrame::GetExpressionAddress(int n) const {
  const int offset = UnoptimizedFrameConstants::kExpressionsOffset;
  return fp() + offset - n * kSystemPointerSize;
}

Object CommonFrame::context() const {
  return ReadOnlyRoots(isolate()).undefined_value();
}

int CommonFrame::position() const {
  CodeLookupResult code = LookupCodeT();
  int code_offset = code.GetOffsetFromInstructionStart(isolate(), pc());
  return code.ToAbstractCode().SourcePosition(isolate(), code_offset);
}

int CommonFrame::ComputeExpressionsCount() const {
  Address base = GetExpressionAddress(0);
  Address limit = sp() - kSystemPointerSize;
  DCHECK(base >= limit);  // stack grows downwards
  // Include register-allocated locals in number of expressions.
  return static_cast<int>((base - limit) / kSystemPointerSize);
}

void CommonFrame::ComputeCallerState(State* state) const {
  state->fp = caller_fp();
#if V8_ENABLE_WEBASSEMBLY
  if (state->fp == kNullAddress) {
    // An empty FP signals the first frame of a stack segment. The caller is
    // on a different stack, or is unbound (suspended stack).
    DCHECK(v8_flags.experimental_wasm_stack_switching);
    return;
  }
#endif
  state->sp = caller_sp();
  state->pc_address = ResolveReturnAddressLocation(
      reinterpret_cast<Address*>(ComputePCAddress(fp())));
  state->callee_fp = fp();
  state->callee_pc_address = pc_address();
  state->constant_pool_address =
      reinterpret_cast<Address*>(ComputeConstantPoolAddress(fp()));
}

void CommonFrame::Summarize(std::vector<FrameSummary>* functions) const {
  // This should only be called on frames which override this method.
  UNREACHABLE();
}

namespace {
void VisitSpillSlot(Isolate* isolate, RootVisitor* v,
                    FullObjectSlot spill_slot) {
#ifdef V8_COMPRESS_POINTERS
  PtrComprCageBase cage_base(isolate);
  bool was_compressed = false;

  // Spill slots may contain compressed values in which case the upper
  // 32-bits will contain zeros. In order to simplify handling of such
  // slots in GC we ensure that the slot always contains full value.

  // The spill slot may actually contain weak references so we load/store
  // values using spill_slot.location() in order to avoid dealing with
  // FullMaybeObjectSlots here.
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    // When external code space is enabled the spill slot could contain both
    // Code and non-Code references, which have different cage bases. So
    // unconditional decompression of the value might corrupt Code pointers.
    // However, given that
    // 1) the Code pointers are never compressed by design (because
    //    otherwise we wouldn't know which cage base to apply for
    //    decompression, see respective DCHECKs in
    //    RelocInfo::target_object()),
    // 2) there's no need to update the upper part of the full pointer
    //    because if it was there then it'll stay the same,
    // we can avoid updating upper part of the spill slot if it already
    // contains full value.
    // TODO(v8:11880): Remove this special handling by enforcing builtins
    // to use CodeTs instead of Code objects.
    Address value = *spill_slot.location();
    if (!HAS_SMI_TAG(value) && value <= 0xffffffff) {
      // We don't need to update smi values or full pointers.
      was_compressed = true;
      *spill_slot.location() =
          DecompressTaggedPointer(cage_base, static_cast<Tagged_t>(value));
      if (DEBUG_BOOL) {
        // Ensure that the spill slot contains correct heap object.
        HeapObject raw = HeapObject::cast(Object(*spill_slot.location()));
        MapWord map_word = raw.map_word(cage_base, kRelaxedLoad);
        HeapObject forwarded = map_word.IsForwardingAddress()
                                   ? map_word.ToForwardingAddress()
                                   : raw;
        bool is_self_forwarded =
            forwarded.map_word(cage_base, kRelaxedLoad).ptr() ==
            forwarded.address();
        if (is_self_forwarded) {
          // The object might be in a self-forwarding state if it's located
          // in new large object space. GC will fix this at a later stage.
          CHECK(BasicMemoryChunk::FromHeapObject(forwarded)
                    ->InNewLargeObjectSpace());
        } else {
          HeapObject forwarded_map = forwarded.map(cage_base);
          // The map might be forwarded as well.
          MapWord fwd_map_map_word =
              forwarded_map.map_word(cage_base, kRelaxedLoad);
          if (fwd_map_map_word.IsForwardingAddress()) {
            forwarded_map = fwd_map_map_word.ToForwardingAddress();
          }
          CHECK(forwarded_map.IsMap(cage_base));
        }
      }
    }
  } else {
    Address slot_contents = *spill_slot.location();
    Tagged_t compressed_value = static_cast<Tagged_t>(slot_contents);
    if (!HAS_SMI_TAG(compressed_value)) {
      was_compressed = slot_contents <= 0xFFFFFFFF;
      // We don't need to update smi values.
      *spill_slot.location() =
          DecompressTaggedPointer(cage_base, compressed_value);
    }
  }
#endif
  v->VisitRootPointer(Root::kStackRoots, nullptr, spill_slot);
#if V8_COMPRESS_POINTERS
  if (was_compressed) {
    // Restore compression. Generated code should be able to trust that
    // compressed spill slots remain compressed.
    *spill_slot.location() = CompressTagged(*spill_slot.location());
  }
#endif
}

void VisitSpillSlots(Isolate* isolate, RootVisitor* v,
                     FullObjectSlot first_slot_offset,
                     base::Vector<const uint8_t> tagged_slots) {
  FullObjectSlot slot_offset = first_slot_offset;
  for (uint8_t bits : tagged_slots) {
    while (bits) {
      const int bit = base::bits::CountTrailingZeros(bits);
      bits &= ~(1 << bit);
      FullObjectSlot spill_slot = slot_offset + bit;
      VisitSpillSlot(isolate, v, spill_slot);
    }
    slot_offset += kBitsPerByte;
  }
}

SafepointEntry GetSafepointEntryFromCodeCache(
    Isolate* isolate, Address inner_pointer,
    InnerPointerToCodeCache::InnerPointerToCodeCacheEntry* entry) {
  if (!entry->safepoint_entry.is_initialized()) {
    entry->safepoint_entry =
        entry->code.GetSafepointEntry(isolate, inner_pointer);
    DCHECK(entry->safepoint_entry.is_initialized());
  } else {
    DCHECK_EQ(entry->safepoint_entry,
              entry->code.GetSafepointEntry(isolate, inner_pointer));
  }
  return entry->safepoint_entry;
}

MaglevSafepointEntry GetMaglevSafepointEntryFromCodeCache(
    Isolate* isolate, Address inner_pointer,
    InnerPointerToCodeCache::InnerPointerToCodeCacheEntry* entry) {
  if (!entry->maglev_safepoint_entry.is_initialized()) {
    entry->maglev_safepoint_entry =
        entry->code.GetMaglevSafepointEntry(isolate, inner_pointer);
    DCHECK(entry->maglev_safepoint_entry.is_initialized());
  } else {
    DCHECK_EQ(entry->maglev_safepoint_entry,
              entry->code.GetMaglevSafepointEntry(isolate, inner_pointer));
  }
  return entry->maglev_safepoint_entry;
}

}  // namespace

#ifdef V8_ENABLE_WEBASSEMBLY
void WasmFrame::Iterate(RootVisitor* v) const {
  // Make sure that we're not doing "safe" stack frame iteration. We cannot
  // possibly find pointers in optimized frames in that state.
  DCHECK(can_access_heap_objects());

  //  ===  WasmFrame ===
  //  +-----------------+-----------------------------------------
  //  |   out_param n   |  <-- parameters_base / sp
  //  |       ...       |
  //  |   out_param 0   |  (these can be tagged or untagged)
  //  +-----------------+-----------------------------------------
  //  |   spill_slot n  |  <-- parameters_limit          ^
  //  |       ...       |                          spill_slot_space
  //  |   spill_slot 0  |                                v
  //  +-----------------+-----------------------------------------
  //  | WasmFeedback(*) |  <-- frame_header_base         ^
  //  |- - - - - - - - -|                                |
  //  |   WasmInstance  |                                |
  //  |- - - - - - - - -|                                |
  //  |   Type Marker   |                                |
  //  |- - - - - - - - -|                         frame_header_size
  //  | [Constant Pool] |                                |
  //  |- - - - - - - - -|                                |
  //  | saved frame ptr |  <-- fp                        |
  //  |- - - - - - - - -|                                |
  //  |  return addr    |  <- tagged_parameter_limit     v
  //  +-----------------+-----------------------------------------
  //  |    in_param n   |
  //  |       ...       |
  //  |    in_param 0   |  <-- first_tagged_parameter_slot
  //  +-----------------+-----------------------------------------
  //
  // (*) Only if compiled by liftoff and with --wasm-speculative-inlining

  auto* wasm_code = wasm::GetWasmCodeManager()->LookupCode(pc());
  DCHECK(wasm_code);
  SafepointTable table(wasm_code);
  SafepointEntry safepoint_entry = table.FindEntry(pc());

#ifdef DEBUG
  intptr_t marker =
      Memory<intptr_t>(fp() + CommonFrameConstants::kContextOrFrameTypeOffset);
  DCHECK(StackFrame::IsTypeMarker(marker));
  StackFrame::Type type = StackFrame::MarkerToType(marker);
  DCHECK(type == WASM_TO_JS || type == WASM || type == WASM_EXIT);
#endif

  // Determine the fixed header and spill slot area size.
  // The last value in the frame header is the calling PC, which should
  // not be visited.
  static_assert(WasmExitFrameConstants::kFixedSlotCountFromFp ==
                    WasmFrameConstants::kFixedSlotCountFromFp + 1,
                "WasmExitFrame has one slot more than WasmFrame");

  int frame_header_size = WasmFrameConstants::kFixedFrameSizeFromFp;
  if (wasm_code->is_liftoff() && v8_flags.wasm_speculative_inlining) {
    // Frame has Wasm feedback slot.
    frame_header_size += kSystemPointerSize;
  }
  int spill_slot_space =
      wasm_code->stack_slots() * kSystemPointerSize -
      (frame_header_size + StandardFrameConstants::kFixedFrameSizeAboveFp);

  // Fixed frame slots.
  FullObjectSlot frame_header_base(&Memory<Address>(fp() - frame_header_size));
  FullObjectSlot frame_header_limit(
      &Memory<Address>(fp() - StandardFrameConstants::kCPSlotSize));
  // Parameters passed to the callee.
  FullObjectSlot parameters_base(&Memory<Address>(sp()));
  FullObjectSlot parameters_limit(frame_header_base.address() -
                                  spill_slot_space);

  // Visit the rest of the parameters if they are tagged.
  bool has_tagged_outgoing_params =
      wasm_code->kind() != wasm::WasmCode::kWasmFunction &&
      wasm_code->kind() != wasm::WasmCode::kWasmToCapiWrapper;
  if (has_tagged_outgoing_params) {
    v->VisitRootPointers(Root::kStackRoots, nullptr, parameters_base,
                         parameters_limit);
  }

  // Visit pointer spill slots and locals.
  DCHECK_GE((wasm_code->stack_slots() + kBitsPerByte) / kBitsPerByte,
            safepoint_entry.tagged_slots().size());
  VisitSpillSlots(isolate(), v, parameters_limit,
                  safepoint_entry.tagged_slots());

  // Visit tagged parameters that have been passed to the function of this
  // frame. Conceptionally these parameters belong to the parent frame. However,
  // the exact count is only known by this frame (in the presence of tail calls,
  // this information cannot be derived from the call site).
  if (wasm_code->num_tagged_parameter_slots() > 0) {
    FullObjectSlot tagged_parameter_base(&Memory<Address>(caller_sp()));
    tagged_parameter_base += wasm_code->first_tagged_parameter_slot();
    FullObjectSlot tagged_parameter_limit =
        tagged_parameter_base + wasm_code->num_tagged_parameter_slots();

    v->VisitRootPointers(Root::kStackRoots, nullptr, tagged_parameter_base,
                         tagged_parameter_limit);
  }

  // Visit the instance object.
  v->VisitRootPointers(Root::kStackRoots, nullptr, frame_header_base,
                       frame_header_limit);
}
#endif  // V8_ENABLE_WEBASSEMBLY

void TypedFrame::Iterate(RootVisitor* v) const {
  // Make sure that we're not doing "safe" stack frame iteration. We cannot
  // possibly find pointers in optimized frames in that state.
  DCHECK(can_access_heap_objects());

  //  ===  TypedFrame ===
  //  +-----------------+-----------------------------------------
  //  |   out_param n   |  <-- parameters_base / sp
  //  |       ...       |
  //  |   out_param 0   |
  //  +-----------------+-----------------------------------------
  //  |   spill_slot n  |  <-- parameters_limit          ^
  //  |       ...       |                          spill_slot_count
  //  |   spill_slot 0  |                                v
  //  +-----------------+-----------------------------------------
  //  |   Type Marker   |  <-- frame_header_base         ^
  //  |- - - - - - - - -|                                |
  //  | [Constant Pool] |                                |
  //  |- - - - - - - - -|                           kFixedSlotCount
  //  | saved frame ptr |  <-- fp                        |
  //  |- - - - - - - - -|                                |
  //  |  return addr    |                                v
  //  +-----------------+-----------------------------------------

  // Find the code and compute the safepoint information.
  Address inner_pointer = pc();
  InnerPointerToCodeCache::InnerPointerToCodeCacheEntry* entry =
      isolate()->inner_pointer_to_code_cache()->GetCacheEntry(inner_pointer);
  CHECK(entry->code.IsFound());
  DCHECK(entry->code.is_turbofanned());
  SafepointEntry safepoint_entry =
      GetSafepointEntryFromCodeCache(isolate(), inner_pointer, entry);

#ifdef DEBUG
  intptr_t marker =
      Memory<intptr_t>(fp() + CommonFrameConstants::kContextOrFrameTypeOffset);
  DCHECK(StackFrame::IsTypeMarker(marker));
#endif  // DEBUG

  // Determine the fixed header and spill slot area size.
  int frame_header_size = TypedFrameConstants::kFixedFrameSizeFromFp;
  int spill_slots_size =
      entry->code.stack_slots() * kSystemPointerSize -
      (frame_header_size + StandardFrameConstants::kFixedFrameSizeAboveFp);

  // Fixed frame slots.
  FullObjectSlot frame_header_base(&Memory<Address>(fp() - frame_header_size));
  FullObjectSlot frame_header_limit(
      &Memory<Address>(fp() - StandardFrameConstants::kCPSlotSize));
  // Parameters passed to the callee.
  FullObjectSlot parameters_base(&Memory<Address>(sp()));
  FullObjectSlot parameters_limit(frame_header_base.address() -
                                  spill_slots_size);

  // Visit the rest of the parameters.
  if (HasTaggedOutgoingParams(entry->code)) {
    v->VisitRootPointers(Root::kStackRoots, nullptr, parameters_base,
                         parameters_limit);
  }

  // Visit pointer spill slots and locals.
  DCHECK_GE((entry->code.stack_slots() + kBitsPerByte) / kBitsPerByte,
            safepoint_entry.tagged_slots().size());
  VisitSpillSlots(isolate(), v, parameters_limit,
                  safepoint_entry.tagged_slots());

  // Visit fixed header region.
  v->VisitRootPointers(Root::kStackRoots, nullptr, frame_header_base,
                       frame_header_limit);

  // Visit the return address in the callee and incoming arguments.
  IteratePc(v, pc_address(), constant_pool_address(), entry->code);
}

void MaglevFrame::Iterate(RootVisitor* v) const {
  // Make sure that we're not doing "safe" stack frame iteration. We cannot
  // possibly find pointers in optimized frames in that state.
  DCHECK(can_access_heap_objects());

  //  ===  MaglevFrame ===
  //  +-----------------+-----------------------------------------
  //  |   out_param n   |  <-- parameters_base / sp
  //  |       ...       |
  //  |   out_param 0   |
  //  +-----------------+-----------------------------------------
  //  | pushed_double n |  <-- parameters_limit          ^
  //  |       ...       |                                |
  //  | pushed_double 0 |                                |
  //  +- - - - - - - - -+                     num_pushed_registers
  //  |   pushed_reg n  |                                |
  //  |       ...       |                                |
  //  |   pushed_reg 0  |  <-- pushed_register_base      v
  //  +-----------------+-----------------------------------------
  //  | untagged_slot n |                                ^
  //  |       ...       |                                |
  //  | untagged_slot 0 |                                |
  //  +- - - - - - - - -+                         spill_slot_count
  //  |  tagged_slot n  |                                |
  //  |       ...       |                                |
  //  |  tagged_slot 0  |                                v
  //  +-----------------+-----------------------------------------
  //  |      argc       |  <-- frame_header_base         ^
  //  |- - - - - - - - -|                                |
  //  |   JSFunction    |                                |
  //  |- - - - - - - - -|                                |
  //  |    Context      |                                |
  //  |- - - - - - - - -|                          kFixedSlotCount
  //  | [Constant Pool] |                                |
  //  |- - - - - - - - -|                                |
  //  | saved frame ptr |  <-- fp                        |
  //  |- - - - - - - - -|                                |
  //  |  return addr    |                                v
  //  +-----------------+-----------------------------------------

  // Find the code and compute the safepoint information.
  Address inner_pointer = pc();
  InnerPointerToCodeCache::InnerPointerToCodeCacheEntry* entry =
      isolate()->inner_pointer_to_code_cache()->GetCacheEntry(inner_pointer);
  CHECK(entry->code.IsFound());
  DCHECK(entry->code.is_maglevved());
  MaglevSafepointEntry maglev_safepoint_entry =
      GetMaglevSafepointEntryFromCodeCache(isolate(), inner_pointer, entry);

#ifdef DEBUG
  // Assert that it is a JS frame and it has a context.
  intptr_t marker =
      Memory<intptr_t>(fp() + CommonFrameConstants::kContextOrFrameTypeOffset);
  DCHECK(!StackFrame::IsTypeMarker(marker));
#endif  // DEBUG

  // Fixed frame slots.
  FullObjectSlot frame_header_base(
      &Memory<Address>(fp() - StandardFrameConstants::kFixedFrameSizeFromFp));
  FullObjectSlot frame_header_limit(
      &Memory<Address>(fp() - StandardFrameConstants::kCPSlotSize));

  // Determine spill slot area count.
  uint32_t tagged_slot_count = maglev_safepoint_entry.num_tagged_slots();
  uint32_t spill_slot_count =
      tagged_slot_count + maglev_safepoint_entry.num_untagged_slots();
  DCHECK_EQ(entry->code.stack_slots(),
            StandardFrameConstants::kFixedSlotCount +
                maglev_safepoint_entry.num_tagged_slots() +
                maglev_safepoint_entry.num_untagged_slots());

  // Check that our frame size is big enough for our spill slots and pushed
  // registers.
  intptr_t actual_frame_size = static_cast<intptr_t>(fp() - sp());
  intptr_t expected_frame_size_excl_outgoing_params =
      StandardFrameConstants::kFixedFrameSizeFromFp +
      (spill_slot_count + maglev_safepoint_entry.num_pushed_registers()) *
          kSystemPointerSize;
  if (actual_frame_size < expected_frame_size_excl_outgoing_params) {
    // If the frame size is smaller than the expected size, then we must be in
    // the stack guard in the prologue of the maglev function. This means that
    // we've set up the frame header, but not the spill slots yet.

    // DCHECK the frame setup under the above assumption. Include one extra slot
    // for the single argument into StackGuardWithGap, and another for the saved
    // new.target register.
    DCHECK_EQ(actual_frame_size, StandardFrameConstants::kFixedFrameSizeFromFp +
                                     2 * kSystemPointerSize);
    DCHECK_EQ(isolate()->c_function(),
              Runtime::FunctionForId(Runtime::kStackGuardWithGap)->entry);
    DCHECK_EQ(maglev_safepoint_entry.num_pushed_registers(), 0);
    spill_slot_count = 0;
    tagged_slot_count = 0;
  }

  // Visit the outgoing parameters if they are tagged.
  DCHECK(entry->code.has_tagged_outgoing_params());
  FullObjectSlot parameters_base(&Memory<Address>(sp()));
  FullObjectSlot parameters_limit =
      frame_header_base - spill_slot_count -
      maglev_safepoint_entry.num_pushed_registers();
  v->VisitRootPointers(Root::kStackRoots, nullptr, parameters_base,
                       parameters_limit);

  // Maglev can also spill registers, tagged and untagged, just before making
  // a call. These are distinct from normal spill slots and live between the
  // normal spill slots and the pushed parameters. Some of these are tagged,
  // as indicated by the tagged register indexes, and should be visited too.
  if (maglev_safepoint_entry.num_pushed_registers() > 0) {
    FullObjectSlot pushed_register_base =
        frame_header_base - spill_slot_count - 1;
    uint32_t tagged_register_indexes =
        maglev_safepoint_entry.tagged_register_indexes();
    while (tagged_register_indexes != 0) {
      int index = base::bits::CountTrailingZeros(tagged_register_indexes);
      tagged_register_indexes &= ~(1 << index);
      FullObjectSlot spill_slot = pushed_register_base - index;
      VisitSpillSlot(isolate(), v, spill_slot);
    }
  }

  // Visit tagged spill slots.
  for (uint32_t i = 0; i < tagged_slot_count; ++i) {
    FullObjectSlot spill_slot = frame_header_base - 1 - i;
    VisitSpillSlot(isolate(), v, spill_slot);
  }

  // Visit fixed header region (the context and JSFunction), skipping the
  // argument count since it is stored untagged.
  v->VisitRootPointers(Root::kStackRoots, nullptr, frame_header_base + 1,
                       frame_header_limit);

  // Visit the return address in the callee and incoming arguments.
  IteratePc(v, pc_address(), constant_pool_address(), entry->code);
}

BytecodeOffset MaglevFrame::GetBytecodeOffsetForOSR() const {
  int deopt_index = SafepointEntry::kNoDeoptIndex;
  const DeoptimizationData data = GetDeoptimizationData(&deopt_index);
  if (deopt_index == SafepointEntry::kNoDeoptIndex) {
    CHECK(data.is_null());
    FATAL("Missing deoptimization information for OptimizedFrame::Summarize.");
  }

  return data.GetBytecodeOffset(deopt_index);
}

bool CommonFrame::HasTaggedOutgoingParams(CodeLookupResult& code_lookup) const {
#if V8_ENABLE_WEBASSEMBLY
  // With inlined JS-to-Wasm calls, we can be in an OptimizedFrame and
  // directly call a Wasm function from JavaScript. In this case the
  // parameters we pass to the callee are not tagged.
  wasm::WasmCode* wasm_callee =
      wasm::GetWasmCodeManager()->LookupCode(callee_pc());
  return (wasm_callee == nullptr) && code_lookup.has_tagged_outgoing_params();
#else
  return code_lookup.has_tagged_outgoing_params();
#endif  // V8_ENABLE_WEBASSEMBLY
}

HeapObject TurbofanStubWithContextFrame::unchecked_code() const {
  CodeLookupResult code_lookup = isolate()->FindCodeObject(pc());
  if (code_lookup.IsCodeDataContainer()) {
    return code_lookup.code_data_container();
  }
  if (code_lookup.IsCode()) {
    return code_lookup.code();
  }
  return {};
}

void CommonFrame::IterateTurbofanOptimizedFrame(RootVisitor* v) const {
  // Make sure that we're not doing "safe" stack frame iteration. We cannot
  // possibly find pointers in optimized frames in that state.
  DCHECK(can_access_heap_objects());

  //  ===  TurbofanFrame ===
  //  +-----------------+-----------------------------------------
  //  |   out_param n   |  <-- parameters_base / sp
  //  |       ...       |
  //  |   out_param 0   |
  //  +-----------------+-----------------------------------------
  //  |   spill_slot n  | <-- parameters_limit           ^
  //  |       ...       |                          spill_slot_count
  //  |   spill_slot 0  |                                v
  //  +-----------------+-----------------------------------------
  //  |      argc       |  <-- frame_header_base         ^
  //  |- - - - - - - - -|                                |
  //  |   JSFunction    |                                |
  //  |- - - - - - - - -|                                |
  //  |    Context      |                                |
  //  |- - - - - - - - -|                           kFixedSlotCount
  //  | [Constant Pool] |                                |
  //  |- - - - - - - - -|                                |
  //  | saved frame ptr |  <-- fp                        |
  //  |- - - - - - - - -|                                |
  //  |  return addr    |                                v
  //  +-----------------+-----------------------------------------

  // Find the code and compute the safepoint information.
  Address inner_pointer = pc();
  InnerPointerToCodeCache::InnerPointerToCodeCacheEntry* entry =
      isolate()->inner_pointer_to_code_cache()->GetCacheEntry(inner_pointer);
  CHECK(entry->code.IsFound());
  DCHECK(entry->code.is_turbofanned());
  SafepointEntry safepoint_entry =
      GetSafepointEntryFromCodeCache(isolate(), inner_pointer, entry);

#ifdef DEBUG
  // Assert that it is a JS frame and it has a context.
  intptr_t marker =
      Memory<intptr_t>(fp() + CommonFrameConstants::kContextOrFrameTypeOffset);
  DCHECK(!StackFrame::IsTypeMarker(marker));
#endif  // DEBUG

  // Determine the fixed header and spill slot area size.
  int frame_header_size = StandardFrameConstants::kFixedFrameSizeFromFp;
  int spill_slot_count =
      entry->code.stack_slots() - StandardFrameConstants::kFixedSlotCount;

  // Fixed frame slots.
  FullObjectSlot frame_header_base(&Memory<Address>(fp() - frame_header_size));
  FullObjectSlot frame_header_limit(
      &Memory<Address>(fp() - StandardFrameConstants::kCPSlotSize));
  // Parameters passed to the callee.
  FullObjectSlot parameters_base(&Memory<Address>(sp()));
  FullObjectSlot parameters_limit = frame_header_base - spill_slot_count;

  // Visit the outgoing parameters if they are tagged.
  if (HasTaggedOutgoingParams(entry->code)) {
    v->VisitRootPointers(Root::kStackRoots, nullptr, parameters_base,
                         parameters_limit);
  }

  // Spill slots are in the region ]frame_header_base, parameters_limit];
  // Visit pointer spill slots and locals.
  DCHECK_GE((entry->code.stack_slots() + kBitsPerByte) / kBitsPerByte,
            safepoint_entry.tagged_slots().size());
  VisitSpillSlots(isolate(), v, parameters_limit,
                  safepoint_entry.tagged_slots());

  // Visit fixed header region (the context and JSFunction), skipping the
  // argument count since it is stored untagged.
  v->VisitRootPointers(Root::kStackRoots, nullptr, frame_header_base + 1,
                       frame_header_limit);

  // Visit the return address in the callee and incoming arguments.
  IteratePc(v, pc_address(), constant_pool_address(), entry->code);
}

void TurbofanStubWithContextFrame::Iterate(RootVisitor* v) const {
  return IterateTurbofanOptimizedFrame(v);
}

void TurbofanFrame::Iterate(RootVisitor* v) const {
  return IterateTurbofanOptimizedFrame(v);
}

HeapObject StubFrame::unchecked_code() const {
  CodeLookupResult code_lookup = isolate()->FindCodeObject(pc());
  if (code_lookup.IsCodeDataContainer()) {
    return code_lookup.code_data_container();
  }
  if (code_lookup.IsCode()) {
    return code_lookup.code();
  }
  return {};
}

int StubFrame::LookupExceptionHandlerInTable() {
  CodeLookupResult code = LookupCodeT();
  DCHECK(code.is_turbofanned());
  DCHECK_EQ(code.kind(), CodeKind::BUILTIN);
  HandlerTable table(code.codet());
  int pc_offset = code.GetOffsetFromInstructionStart(isolate(), pc());
  return table.LookupReturn(pc_offset);
}

void JavaScriptFrame::SetParameterValue(int index, Object value) const {
  Memory<Address>(GetParameterSlot(index)) = value.ptr();
}

bool JavaScriptFrame::IsConstructor() const {
  return IsConstructFrame(caller_fp());
}

bool JavaScriptFrame::HasInlinedFrames() const {
  std::vector<SharedFunctionInfo> functions;
  GetFunctions(&functions);
  return functions.size() > 1;
}

HeapObject CommonFrameWithJSLinkage::unchecked_code() const {
  return function().code();
}

int TurbofanFrame::ComputeParametersCount() const {
  CodeLookupResult code = LookupCodeT();
  if (code.kind() == CodeKind::BUILTIN) {
    return static_cast<int>(
               Memory<intptr_t>(fp() + StandardFrameConstants::kArgCOffset)) -
           kJSArgcReceiverSlots;
  } else {
    return JavaScriptFrame::ComputeParametersCount();
  }
}

Address JavaScriptFrame::GetCallerStackPointer() const {
  return fp() + StandardFrameConstants::kCallerSPOffset;
}

void JavaScriptFrame::GetFunctions(
    std::vector<SharedFunctionInfo>* functions) const {
  DCHECK(functions->empty());
  functions->push_back(function().shared());
}

void JavaScriptFrame::GetFunctions(
    std::vector<Handle<SharedFunctionInfo>>* functions) const {
  DCHECK(functions->empty());
  std::vector<SharedFunctionInfo> raw_functions;
  GetFunctions(&raw_functions);
  for (const auto& raw_function : raw_functions) {
    functions->push_back(
        Handle<SharedFunctionInfo>(raw_function, function().GetIsolate()));
  }
}

bool CommonFrameWithJSLinkage::IsConstructor() const {
  return IsConstructFrame(caller_fp());
}

void CommonFrameWithJSLinkage::Summarize(
    std::vector<FrameSummary>* functions) const {
  DCHECK(functions->empty());
  CodeLookupResult code = LookupCodeT();
  int offset = code.GetOffsetFromInstructionStart(isolate(), pc());
  Handle<AbstractCode> abstract_code(code.ToAbstractCode(), isolate());
  Handle<FixedArray> params = GetParameters();
  FrameSummary::JavaScriptFrameSummary summary(
      isolate(), receiver(), function(), *abstract_code, offset,
      IsConstructor(), *params);
  functions->push_back(summary);
}

JSFunction JavaScriptFrame::function() const {
  return JSFunction::cast(function_slot_object());
}

Object JavaScriptFrame::unchecked_function() const {
  // During deoptimization of an optimized function, we may have yet to
  // materialize some closures on the stack. The arguments marker object
  // marks this case.
  DCHECK(function_slot_object().IsJSFunction() ||
         ReadOnlyRoots(isolate()).arguments_marker() == function_slot_object());
  return function_slot_object();
}

Object CommonFrameWithJSLinkage::receiver() const { return GetParameter(-1); }

Object JavaScriptFrame::context() const {
  const int offset = StandardFrameConstants::kContextOffset;
  Object maybe_result(Memory<Address>(fp() + offset));
  DCHECK(!maybe_result.IsSmi());
  return maybe_result;
}

Script JavaScriptFrame::script() const {
  return Script::cast(function().shared().script());
}

int CommonFrameWithJSLinkage::LookupExceptionHandlerInTable(
    int* stack_depth, HandlerTable::CatchPrediction* prediction) {
  if (DEBUG_BOOL) {
    CodeLookupResult code_lookup_result = LookupCodeT();
    CHECK(!code_lookup_result.has_handler_table());
    CHECK(!code_lookup_result.is_optimized_code() ||
          code_lookup_result.kind() == CodeKind::BASELINE);
  }
  return -1;
}

void JavaScriptFrame::PrintFunctionAndOffset(JSFunction function,
                                             AbstractCode code, int code_offset,
                                             FILE* file,
                                             bool print_line_number) {
  PtrComprCageBase cage_base = GetPtrComprCageBase(function);
  PrintF(file, "%s", CodeKindToMarker(code.kind(cage_base)));
  function.PrintName(file);
  PrintF(file, "+%d", code_offset);
  if (print_line_number) {
    SharedFunctionInfo shared = function.shared();
    int source_pos = code.SourcePosition(cage_base, code_offset);
    Object maybe_script = shared.script();
    if (maybe_script.IsScript()) {
      Script script = Script::cast(maybe_script);
      int line = script.GetLineNumber(source_pos) + 1;
      Object script_name_raw = script.name();
      if (script_name_raw.IsString()) {
        String script_name = String::cast(script.name());
        std::unique_ptr<char[]> c_script_name =
            script_name.ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
        PrintF(file, " at %s:%d", c_script_name.get(), line);
      } else {
        PrintF(file, " at <unknown>:%d", line);
      }
    } else {
      PrintF(file, " at <unknown>:<unknown>");
    }
  }
}

void JavaScriptFrame::PrintTop(Isolate* isolate, FILE* file, bool print_args,
                               bool print_line_number) {
  // constructor calls
  DisallowGarbageCollection no_gc;
  JavaScriptFrameIterator it(isolate);
  while (!it.done()) {
    if (it.frame()->is_java_script()) {
      JavaScriptFrame* frame = it.frame();
      if (frame->IsConstructor()) PrintF(file, "new ");
      JSFunction function = frame->function();
      int code_offset = 0;
      AbstractCode abstract_code = function.abstract_code(isolate);
      if (frame->is_interpreted()) {
        InterpretedFrame* iframe = reinterpret_cast<InterpretedFrame*>(frame);
        code_offset = iframe->GetBytecodeOffset();
      } else if (frame->is_baseline()) {
        // TODO(pthier): AbstractCode should fully support Baseline code.
        BaselineFrame* baseline_frame = BaselineFrame::cast(frame);
        code_offset = baseline_frame->GetBytecodeOffset();
        abstract_code = AbstractCode::cast(baseline_frame->GetBytecodeArray());
      } else {
        CodeLookupResult code = frame->LookupCodeT();
        code_offset = code.GetOffsetFromInstructionStart(isolate, frame->pc());
      }
      PrintFunctionAndOffset(function, abstract_code, code_offset, file,
                             print_line_number);
      if (print_args) {
        // function arguments
        // (we are intentionally only printing the actually
        // supplied parameters, not all parameters required)
        PrintF(file, "(this=");
        frame->receiver().ShortPrint(file);
        const int length = frame->ComputeParametersCount();
        for (int i = 0; i < length; i++) {
          PrintF(file, ", ");
          frame->GetParameter(i).ShortPrint(file);
        }
        PrintF(file, ")");
      }
      break;
    }
    it.Advance();
  }
}

// static
void JavaScriptFrame::CollectFunctionAndOffsetForICStats(JSFunction function,
                                                         AbstractCode code,
                                                         int code_offset) {
  auto ic_stats = ICStats::instance();
  ICInfo& ic_info = ic_stats->Current();
  PtrComprCageBase cage_base = GetPtrComprCageBase(function);
  SharedFunctionInfo shared = function.shared(cage_base);

  ic_info.function_name = ic_stats->GetOrCacheFunctionName(function);
  ic_info.script_offset = code_offset;

  int source_pos = code.SourcePosition(cage_base, code_offset);
  Object maybe_script = shared.script(cage_base);
  if (maybe_script.IsScript(cage_base)) {
    Script script = Script::cast(maybe_script);
    ic_info.line_num = script.GetLineNumber(source_pos) + 1;
    ic_info.column_num = script.GetColumnNumber(source_pos);
    ic_info.script_name = ic_stats->GetOrCacheScriptName(script);
  }
}

Object CommonFrameWithJSLinkage::GetParameter(int index) const {
  return Object(Memory<Address>(GetParameterSlot(index)));
}

int CommonFrameWithJSLinkage::ComputeParametersCount() const {
  DCHECK(can_access_heap_objects() &&
         isolate()->heap()->gc_state() == Heap::NOT_IN_GC);
  return function().shared().internal_formal_parameter_count_without_receiver();
}

int JavaScriptFrame::GetActualArgumentCount() const {
  return static_cast<int>(
             Memory<intptr_t>(fp() + StandardFrameConstants::kArgCOffset)) -
         kJSArgcReceiverSlots;
}

Handle<FixedArray> CommonFrameWithJSLinkage::GetParameters() const {
  if (V8_LIKELY(!v8_flags.detailed_error_stack_trace)) {
    return isolate()->factory()->empty_fixed_array();
  }
  int param_count = ComputeParametersCount();
  Handle<FixedArray> parameters =
      isolate()->factory()->NewFixedArray(param_count);
  for (int i = 0; i < param_count; i++) {
    parameters->set(i, GetParameter(i));
  }

  return parameters;
}

JSFunction JavaScriptBuiltinContinuationFrame::function() const {
  const int offset = BuiltinContinuationFrameConstants::kFunctionOffset;
  return JSFunction::cast(Object(base::Memory<Address>(fp() + offset)));
}

int JavaScriptBuiltinContinuationFrame::ComputeParametersCount() const {
  // Assert that the first allocatable register is also the argument count
  // register.
  DCHECK_EQ(RegisterConfiguration::Default()->GetAllocatableGeneralCode(0),
            kJavaScriptCallArgCountRegister.code());
  Object argc_object(
      Memory<Address>(fp() + BuiltinContinuationFrameConstants::kArgCOffset));
  return Smi::ToInt(argc_object) - kJSArgcReceiverSlots;
}

intptr_t JavaScriptBuiltinContinuationFrame::GetSPToFPDelta() const {
  Address height_slot =
      fp() + BuiltinContinuationFrameConstants::kFrameSPtoFPDeltaAtDeoptimize;
  intptr_t height = Smi::ToInt(Smi(Memory<Address>(height_slot)));
  return height;
}

Object JavaScriptBuiltinContinuationFrame::context() const {
  return Object(Memory<Address>(
      fp() + BuiltinContinuationFrameConstants::kBuiltinContextOffset));
}

void JavaScriptBuiltinContinuationWithCatchFrame::SetException(
    Object exception) {
  int argc = ComputeParametersCount();
  Address exception_argument_slot =
      fp() + BuiltinContinuationFrameConstants::kFixedFrameSizeAboveFp +
      (argc - 1) * kSystemPointerSize;

  // Only allow setting exception if previous value was the hole.
  CHECK_EQ(ReadOnlyRoots(isolate()).the_hole_value(),
           Object(Memory<Address>(exception_argument_slot)));
  Memory<Address>(exception_argument_slot) = exception.ptr();
}

FrameSummary::JavaScriptFrameSummary::JavaScriptFrameSummary(
    Isolate* isolate, Object receiver, JSFunction function,
    AbstractCode abstract_code, int code_offset, bool is_constructor,
    FixedArray parameters)
    : FrameSummaryBase(isolate, FrameSummary::JAVA_SCRIPT),
      receiver_(receiver, isolate),
      function_(function, isolate),
      abstract_code_(abstract_code, isolate),
      code_offset_(code_offset),
      is_constructor_(is_constructor),
      parameters_(parameters, isolate) {
  DCHECK(!CodeKindIsOptimizedJSFunction(abstract_code.kind(isolate)));
}

void FrameSummary::EnsureSourcePositionsAvailable() {
  if (IsJavaScript()) {
    java_script_summary_.EnsureSourcePositionsAvailable();
  }
}

bool FrameSummary::AreSourcePositionsAvailable() const {
  if (IsJavaScript()) {
    return java_script_summary_.AreSourcePositionsAvailable();
  }
  return true;
}

void FrameSummary::JavaScriptFrameSummary::EnsureSourcePositionsAvailable() {
  Handle<SharedFunctionInfo> shared(function()->shared(), isolate());
  SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate(), shared);
}

bool FrameSummary::JavaScriptFrameSummary::AreSourcePositionsAvailable() const {
  return !v8_flags.enable_lazy_source_positions ||
         function()
             ->shared()
             .GetBytecodeArray(isolate())
             .HasSourcePositionTable();
}

bool FrameSummary::JavaScriptFrameSummary::is_subject_to_debugging() const {
  return function()->shared().IsSubjectToDebugging();
}

int FrameSummary::JavaScriptFrameSummary::SourcePosition() const {
  return abstract_code()->SourcePosition(isolate(), code_offset());
}

int FrameSummary::JavaScriptFrameSummary::SourceStatementPosition() const {
  return abstract_code()->SourceStatementPosition(isolate(), code_offset());
}

Handle<Object> FrameSummary::JavaScriptFrameSummary::script() const {
  return handle(function_->shared().script(), isolate());
}

Handle<Context> FrameSummary::JavaScriptFrameSummary::native_context() const {
  return handle(function_->native_context(), isolate());
}

Handle<StackFrameInfo>
FrameSummary::JavaScriptFrameSummary::CreateStackFrameInfo() const {
  Handle<SharedFunctionInfo> shared(function_->shared(), isolate());
  Handle<Script> script(Script::cast(shared->script()), isolate());
  Handle<String> function_name = JSFunction::GetDebugName(function_);
  if (function_name->length() == 0 &&
      script->compilation_type() == Script::COMPILATION_TYPE_EVAL) {
    function_name = isolate()->factory()->eval_string();
  }
  int bytecode_offset = code_offset();
  if (bytecode_offset == kFunctionEntryBytecodeOffset) {
    // For the special function entry bytecode offset (-1), which signals
    // that the stack trace was captured while the function entry was
    // executing (i.e. during the interrupt check), we cannot store this
    // sentinel in the bit field, so we just eagerly lookup the source
    // position within the script.
    SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate(), shared);
    int source_position =
        abstract_code()->SourcePosition(isolate(), bytecode_offset);
    return isolate()->factory()->NewStackFrameInfo(
        script, source_position, function_name, is_constructor());
  }
  return isolate()->factory()->NewStackFrameInfo(
      shared, bytecode_offset, function_name, is_constructor());
}

#if V8_ENABLE_WEBASSEMBLY
FrameSummary::WasmFrameSummary::WasmFrameSummary(
    Isolate* isolate, Handle<WasmInstanceObject> instance, wasm::WasmCode* code,
    int code_offset, bool at_to_number_conversion)
    : FrameSummaryBase(isolate, WASM),
      wasm_instance_(instance),
      at_to_number_conversion_(at_to_number_conversion),
      code_(code),
      code_offset_(code_offset) {}

Handle<Object> FrameSummary::WasmFrameSummary::receiver() const {
  return wasm_instance_->GetIsolate()->global_proxy();
}

uint32_t FrameSummary::WasmFrameSummary::function_index() const {
  return code()->index();
}

int FrameSummary::WasmFrameSummary::byte_offset() const {
  return code_->GetSourcePositionBefore(code_offset());
}

int FrameSummary::WasmFrameSummary::SourcePosition() const {
  const wasm::WasmModule* module = wasm_instance()->module_object().module();
  return GetSourcePosition(module, function_index(), byte_offset(),
                           at_to_number_conversion());
}

Handle<Script> FrameSummary::WasmFrameSummary::script() const {
  return handle(wasm_instance()->module_object().script(),
                wasm_instance()->GetIsolate());
}

Handle<Context> FrameSummary::WasmFrameSummary::native_context() const {
  return handle(wasm_instance()->native_context(), isolate());
}

Handle<StackFrameInfo> FrameSummary::WasmFrameSummary::CreateStackFrameInfo()
    const {
  Handle<String> function_name =
      GetWasmFunctionDebugName(isolate(), wasm_instance(), function_index());
  return isolate()->factory()->NewStackFrameInfo(script(), SourcePosition(),
                                                 function_name, false);
}
#endif  // V8_ENABLE_WEBASSEMBLY

FrameSummary::~FrameSummary() {
#define FRAME_SUMMARY_DESTR(kind, type, field, desc) \
  case kind:                                         \
    field.~type();                                   \
    break;
  switch (base_.kind()) {
    FRAME_SUMMARY_VARIANTS(FRAME_SUMMARY_DESTR)
    default:
      UNREACHABLE();
  }
#undef FRAME_SUMMARY_DESTR
}

FrameSummary FrameSummary::GetTop(const CommonFrame* frame) {
  std::vector<FrameSummary> frames;
  frame->Summarize(&frames);
  DCHECK_LT(0, frames.size());
  return frames.back();
}

FrameSummary FrameSummary::GetBottom(const CommonFrame* frame) {
  return Get(frame, 0);
}

FrameSummary FrameSummary::GetSingle(const CommonFrame* frame) {
  std::vector<FrameSummary> frames;
  frame->Summarize(&frames);
  DCHECK_EQ(1, frames.size());
  return frames.front();
}

FrameSummary FrameSummary::Get(const CommonFrame* frame, int index) {
  DCHECK_LE(0, index);
  std::vector<FrameSummary> frames;
  frame->Summarize(&frames);
  DCHECK_GT(frames.size(), index);
  return frames[index];
}

#if V8_ENABLE_WEBASSEMBLY
#define FRAME_SUMMARY_DISPATCH(ret, name)   \
  ret FrameSummary::name() const {          \
    switch (base_.kind()) {                 \
      case JAVA_SCRIPT:                     \
        return java_script_summary_.name(); \
      case WASM:                            \
        return wasm_summary_.name();        \
      default:                              \
        UNREACHABLE();                      \
    }                                       \
  }
#else
#define FRAME_SUMMARY_DISPATCH(ret, name) \
  ret FrameSummary::name() const {        \
    DCHECK_EQ(JAVA_SCRIPT, base_.kind()); \
    return java_script_summary_.name();   \
  }
#endif  // V8_ENABLE_WEBASSEMBLY

FRAME_SUMMARY_DISPATCH(Handle<Object>, receiver)
FRAME_SUMMARY_DISPATCH(int, code_offset)
FRAME_SUMMARY_DISPATCH(bool, is_constructor)
FRAME_SUMMARY_DISPATCH(bool, is_subject_to_debugging)
FRAME_SUMMARY_DISPATCH(Handle<Object>, script)
FRAME_SUMMARY_DISPATCH(int, SourcePosition)
FRAME_SUMMARY_DISPATCH(int, SourceStatementPosition)
FRAME_SUMMARY_DISPATCH(Handle<Context>, native_context)
FRAME_SUMMARY_DISPATCH(Handle<StackFrameInfo>, CreateStackFrameInfo)

#undef FRAME_SUMMARY_DISPATCH

void OptimizedFrame::Summarize(std::vector<FrameSummary>* frames) const {
  DCHECK(frames->empty());
  DCHECK(is_optimized());

  // Delegate to JS frame in absence of deoptimization info.
  // TODO(turbofan): Revisit once we support deoptimization across the board.
  CodeLookupResult code = LookupCodeT();
  if (code.kind() == CodeKind::BUILTIN) {
    return JavaScriptFrame::Summarize(frames);
  }

  int deopt_index = SafepointEntry::kNoDeoptIndex;
  DeoptimizationData const data = GetDeoptimizationData(&deopt_index);
  if (deopt_index == SafepointEntry::kNoDeoptIndex) {
    // Hack: For maglevved function entry, we don't emit lazy deopt information,
    // so create an extra special summary here.
    //
    // TODO(leszeks): Remove this hack, by having a maglev-specific frame
    // summary which is a bit more aware of maglev behaviour and can e.g. handle
    // more compact safepointed frame information for both function entry and
    // loop stack checks.
    if (code.is_maglevved()) {
      DCHECK(frames->empty());
      Handle<AbstractCode> abstract_code(
          AbstractCode::cast(function().shared().GetBytecodeArray(isolate())),
          isolate());
      Handle<FixedArray> params = GetParameters();
      FrameSummary::JavaScriptFrameSummary summary(
          isolate(), receiver(), function(), *abstract_code,
          kFunctionEntryBytecodeOffset, IsConstructor(), *params);
      frames->push_back(summary);
      return;
    }

    CHECK(data.is_null());
    FATAL("Missing deoptimization information for OptimizedFrame::Summarize.");
  }

  // Prepare iteration over translation. We must not materialize values here
  // because we do not deoptimize the function.
  TranslatedState translated(this);
  translated.Prepare(fp());

  // We create the summary in reverse order because the frames
  // in the deoptimization translation are ordered bottom-to-top.
  bool is_constructor = IsConstructor();
  for (auto it = translated.begin(); it != translated.end(); it++) {
    if (it->kind() == TranslatedFrame::kUnoptimizedFunction ||
        it->kind() == TranslatedFrame::kJavaScriptBuiltinContinuation ||
        it->kind() ==
            TranslatedFrame::kJavaScriptBuiltinContinuationWithCatch) {
      Handle<SharedFunctionInfo> shared_info = it->shared_info();

      // The translation commands are ordered and the function is always
      // at the first position, and the receiver is next.
      TranslatedFrame::iterator translated_values = it->begin();

      // Get the correct function in the optimized frame.
      CHECK(!translated_values->IsMaterializedObject());
      Handle<JSFunction> function =
          Handle<JSFunction>::cast(translated_values->GetValue());
      translated_values++;

      // Get the correct receiver in the optimized frame.
      CHECK(!translated_values->IsMaterializedObject());
      Handle<Object> receiver = translated_values->GetValue();
      translated_values++;

      // Determine the underlying code object and the position within it from
      // the translation corresponding to the frame type in question.
      Handle<AbstractCode> abstract_code;
      unsigned code_offset;
      if (it->kind() == TranslatedFrame::kJavaScriptBuiltinContinuation ||
          it->kind() ==
              TranslatedFrame::kJavaScriptBuiltinContinuationWithCatch) {
        code_offset = 0;
        abstract_code = ToAbstractCode(
            isolate()->builtins()->code_handle(
                Builtins::GetBuiltinFromBytecodeOffset(it->bytecode_offset())),
            isolate());
      } else {
        DCHECK_EQ(it->kind(), TranslatedFrame::kUnoptimizedFunction);
        code_offset = it->bytecode_offset().ToInt();
        abstract_code =
            handle(shared_info->abstract_code(isolate()), isolate());
      }

      // Append full summary of the encountered JS frame.
      Handle<FixedArray> params = GetParameters();
      FrameSummary::JavaScriptFrameSummary summary(
          isolate(), *receiver, *function, *abstract_code, code_offset,
          is_constructor, *params);
      frames->push_back(summary);
      is_constructor = false;
    } else if (it->kind() == TranslatedFrame::kConstructStub) {
      // The next encountered JS frame will be marked as a constructor call.
      DCHECK(!is_constructor);
      is_constructor = true;
    }
  }
}

int OptimizedFrame::LookupExceptionHandlerInTable(
    int* data, HandlerTable::CatchPrediction* prediction) {
  // We cannot perform exception prediction on optimized code. Instead, we need
  // to use FrameSummary to find the corresponding code offset in unoptimized
  // code to perform prediction there.
  DCHECK_NULL(prediction);
  CodeT code = LookupCodeT().ToCodeT();

  HandlerTable table(code);
  if (table.NumberOfReturnEntries() == 0) return -1;

  int pc_offset = code.GetOffsetFromInstructionStart(isolate(), pc());
  DCHECK_NULL(data);  // Data is not used and will not return a value.

  // When the return pc has been replaced by a trampoline there won't be
  // a handler for this trampoline. Thus we need to use the return pc that
  // _used to be_ on the stack to get the right ExceptionHandler.
  if (CodeKindCanDeoptimize(code.kind()) && code.marked_for_deoptimization()) {
    pc_offset = FindReturnPCForTrampoline(code, pc_offset);
  }
  return table.LookupReturn(pc_offset);
}

int MaglevFrame::FindReturnPCForTrampoline(CodeT code,
                                           int trampoline_pc) const {
  DCHECK_EQ(code.kind(), CodeKind::MAGLEV);
  DCHECK(code.marked_for_deoptimization());
  MaglevSafepointTable safepoints(isolate(), pc(), code);
  return safepoints.find_return_pc(trampoline_pc);
}

int TurbofanFrame::FindReturnPCForTrampoline(CodeT code,
                                             int trampoline_pc) const {
  DCHECK_EQ(code.kind(), CodeKind::TURBOFAN);
  DCHECK(code.marked_for_deoptimization());
  SafepointTable safepoints(isolate(), pc(), code);
  return safepoints.find_return_pc(trampoline_pc);
}

DeoptimizationData OptimizedFrame::GetDeoptimizationData(
    int* deopt_index) const {
  DCHECK(is_optimized());

  JSFunction opt_function = function();
  CodeT code = opt_function.code();

  // The code object may have been replaced by lazy deoptimization. Fall
  // back to a slow search in this case to find the original optimized
  // code object.
  if (!code.contains(isolate(), pc())) {
    CodeLookupResult lookup_result =
        isolate()->heap()->GcSafeFindCodeForInnerPointer(pc());
    CHECK(lookup_result.IsFound());
    code = lookup_result.ToCodeT();
  }
  DCHECK(!code.is_null());
  DCHECK(CodeKindCanDeoptimize(code.kind()));

  if (code.is_maglevved()) {
    MaglevSafepointEntry safepoint_entry =
        code.GetMaglevSafepointEntry(isolate(), pc());
    if (safepoint_entry.has_deoptimization_index()) {
      *deopt_index = safepoint_entry.deoptimization_index();
      return DeoptimizationData::cast(code.deoptimization_data());
    }
  } else {
    SafepointEntry safepoint_entry = code.GetSafepointEntry(isolate(), pc());
    if (safepoint_entry.has_deoptimization_index()) {
      *deopt_index = safepoint_entry.deoptimization_index();
      return DeoptimizationData::cast(code.deoptimization_data());
    }
  }
  *deopt_index = SafepointEntry::kNoDeoptIndex;
  return DeoptimizationData();
}

void OptimizedFrame::GetFunctions(
    std::vector<SharedFunctionInfo>* functions) const {
  DCHECK(functions->empty());
  DCHECK(is_optimized());

  // Delegate to JS frame in absence of turbofan deoptimization.
  // TODO(turbofan): Revisit once we support deoptimization across the board.
  CodeLookupResult code = LookupCodeT();
  if (code.kind() == CodeKind::BUILTIN) {
    return JavaScriptFrame::GetFunctions(functions);
  }

  DisallowGarbageCollection no_gc;
  int deopt_index = SafepointEntry::kNoDeoptIndex;
  DeoptimizationData const data = GetDeoptimizationData(&deopt_index);
  DCHECK(!data.is_null());
  DCHECK_NE(SafepointEntry::kNoDeoptIndex, deopt_index);
  DeoptimizationLiteralArray const literal_array = data.LiteralArray();

  TranslationArrayIterator it(data.TranslationByteArray(),
                              data.TranslationIndex(deopt_index).value());
  TranslationOpcode opcode = TranslationOpcodeFromInt(it.NextUnsigned());
  DCHECK_EQ(TranslationOpcode::BEGIN, opcode);
  it.Next();  // Skip frame count.
  int jsframe_count = it.Next();
  it.Next();  // Skip update feedback count.

  // We insert the frames in reverse order because the frames
  // in the deoptimization translation are ordered bottom-to-top.
  while (jsframe_count != 0) {
    opcode = TranslationOpcodeFromInt(it.NextUnsigned());
    if (opcode == TranslationOpcode::INTERPRETED_FRAME ||
        opcode == TranslationOpcode::JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME ||
        opcode == TranslationOpcode::
                      JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME) {
      it.Next();  // Skip bailout id.
      jsframe_count--;

      // The second operand of the frame points to the function.
      Object shared = literal_array.get(it.Next());
      functions->push_back(SharedFunctionInfo::cast(shared));

      // Skip over remaining operands to advance to the next opcode.
      it.Skip(TranslationOpcodeOperandCount(opcode) - 2);
    } else {
      // Skip over operands to advance to the next opcode.
      it.Skip(TranslationOpcodeOperandCount(opcode));
    }
  }
}

int OptimizedFrame::StackSlotOffsetRelativeToFp(int slot_index) {
  return StandardFrameConstants::kCallerSPOffset -
         ((slot_index + 1) * kSystemPointerSize);
}

int UnoptimizedFrame::position() const {
  AbstractCode code = AbstractCode::cast(GetBytecodeArray());
  int code_offset = GetBytecodeOffset();
  return code.SourcePosition(isolate(), code_offset);
}

int UnoptimizedFrame::LookupExceptionHandlerInTable(
    int* context_register, HandlerTable::CatchPrediction* prediction) {
  HandlerTable table(GetBytecodeArray());
  return table.LookupRange(GetBytecodeOffset(), context_register, prediction);
}

BytecodeArray UnoptimizedFrame::GetBytecodeArray() const {
  const int index = UnoptimizedFrameConstants::kBytecodeArrayExpressionIndex;
  DCHECK_EQ(UnoptimizedFrameConstants::kBytecodeArrayFromFp,
            UnoptimizedFrameConstants::kExpressionsOffset -
                index * kSystemPointerSize);
  return BytecodeArray::cast(GetExpression(index));
}

Object UnoptimizedFrame::ReadInterpreterRegister(int register_index) const {
  const int index = UnoptimizedFrameConstants::kRegisterFileExpressionIndex;
  DCHECK_EQ(UnoptimizedFrameConstants::kRegisterFileFromFp,
            UnoptimizedFrameConstants::kExpressionsOffset -
                index * kSystemPointerSize);
  return GetExpression(index + register_index);
}

void UnoptimizedFrame::Summarize(std::vector<FrameSummary>* functions) const {
  DCHECK(functions->empty());
  Handle<AbstractCode> abstract_code(AbstractCode::cast(GetBytecodeArray()),
                                     isolate());
  Handle<FixedArray> params = GetParameters();
  FrameSummary::JavaScriptFrameSummary summary(
      isolate(), receiver(), function(), *abstract_code, GetBytecodeOffset(),
      IsConstructor(), *params);
  functions->push_back(summary);
}

int InterpretedFrame::GetBytecodeOffset() const {
  const int index = InterpreterFrameConstants::kBytecodeOffsetExpressionIndex;
  DCHECK_EQ(InterpreterFrameConstants::kBytecodeOffsetFromFp,
            InterpreterFrameConstants::kExpressionsOffset -
                index * kSystemPointerSize);
  int raw_offset = Smi::ToInt(GetExpression(index));
  return raw_offset - BytecodeArray::kHeaderSize + kHeapObjectTag;
}

// static
int InterpretedFrame::GetBytecodeOffset(Address fp) {
  const int offset = InterpreterFrameConstants::kExpressionsOffset;
  const int index = InterpreterFrameConstants::kBytecodeOffsetExpressionIndex;
  DCHECK_EQ(InterpreterFrameConstants::kBytecodeOffsetFromFp,
            InterpreterFrameConstants::kExpressionsOffset -
                index * kSystemPointerSize);
  Address expression_offset = fp + offset - index * kSystemPointerSize;
  int raw_offset = Smi::ToInt(Object(Memory<Address>(expression_offset)));
  return raw_offset - BytecodeArray::kHeaderSize + kHeapObjectTag;
}

void InterpretedFrame::PatchBytecodeOffset(int new_offset) {
  const int index = InterpreterFrameConstants::kBytecodeOffsetExpressionIndex;
  DCHECK_EQ(InterpreterFrameConstants::kBytecodeOffsetFromFp,
            InterpreterFrameConstants::kExpressionsOffset -
                index * kSystemPointerSize);
  int raw_offset = BytecodeArray::kHeaderSize - kHeapObjectTag + new_offset;
  SetExpression(index, Smi::FromInt(raw_offset));
}

void InterpretedFrame::PatchBytecodeArray(BytecodeArray bytecode_array) {
  const int index = InterpreterFrameConstants::kBytecodeArrayExpressionIndex;
  DCHECK_EQ(InterpreterFrameConstants::kBytecodeArrayFromFp,
            InterpreterFrameConstants::kExpressionsOffset -
                index * kSystemPointerSize);
  SetExpression(index, bytecode_array);
}

int BaselineFrame::GetBytecodeOffset() const {
  Code code = LookupCodeT().code();
  return code.GetBytecodeOffsetForBaselinePC(this->pc(), GetBytecodeArray());
}

intptr_t BaselineFrame::GetPCForBytecodeOffset(int bytecode_offset) const {
  Code code = LookupCodeT().code();
  return code.GetBaselineStartPCForBytecodeOffset(bytecode_offset,
                                                  GetBytecodeArray());
}

void BaselineFrame::PatchContext(Context value) {
  base::Memory<Address>(fp() + BaselineFrameConstants::kContextOffset) =
      value.ptr();
}

JSFunction BuiltinFrame::function() const {
  const int offset = BuiltinFrameConstants::kFunctionOffset;
  return JSFunction::cast(Object(base::Memory<Address>(fp() + offset)));
}

int BuiltinFrame::ComputeParametersCount() const {
  const int offset = BuiltinFrameConstants::kLengthOffset;
  return Smi::ToInt(Object(base::Memory<Address>(fp() + offset))) -
         kJSArgcReceiverSlots;
}

#if V8_ENABLE_WEBASSEMBLY
void WasmFrame::Print(StringStream* accumulator, PrintMode mode,
                      int index) const {
  PrintIndex(accumulator, mode, index);
  if (function_index() == wasm::kAnonymousFuncIndex) {
    accumulator->Add("Anonymous wasm wrapper [pc: %p]\n",
                     reinterpret_cast<void*>(pc()));
    return;
  }
  wasm::WasmCodeRefScope code_ref_scope;
  accumulator->Add("Wasm [");
  accumulator->PrintName(script().name());
  Address instruction_start = wasm_code()->instruction_start();
  base::Vector<const uint8_t> raw_func_name =
      module_object().GetRawFunctionName(function_index());
  const int kMaxPrintedFunctionName = 64;
  char func_name[kMaxPrintedFunctionName + 1];
  int func_name_len = std::min(kMaxPrintedFunctionName, raw_func_name.length());
  memcpy(func_name, raw_func_name.begin(), func_name_len);
  func_name[func_name_len] = '\0';
  int pos = position();
  const wasm::WasmModule* module = wasm_instance().module_object().module();
  int func_index = function_index();
  int func_code_offset = module->functions[func_index].code.offset();
  accumulator->Add("], function #%u ('%s'), pc=%p (+0x%x), pos=%d (+%d)\n",
                   func_index, func_name, reinterpret_cast<void*>(pc()),
                   static_cast<int>(pc() - instruction_start), pos,
                   pos - func_code_offset);
  if (mode != OVERVIEW) accumulator->Add("\n");
}

wasm::WasmCode* WasmFrame::wasm_code() const {
  return wasm::GetWasmCodeManager()->LookupCode(pc());
}

WasmInstanceObject WasmFrame::wasm_instance() const {
  const int offset = WasmFrameConstants::kWasmInstanceOffset;
  Object instance(Memory<Address>(fp() + offset));
  return WasmInstanceObject::cast(instance);
}

wasm::NativeModule* WasmFrame::native_module() const {
  return module_object().native_module();
}

WasmModuleObject WasmFrame::module_object() const {
  return wasm_instance().module_object();
}

int WasmFrame::function_index() const {
  wasm::WasmCodeRefScope code_ref_scope;
  return wasm_code()->index();
}

Script WasmFrame::script() const { return module_object().script(); }

int WasmFrame::position() const {
  wasm::WasmCodeRefScope code_ref_scope;
  const wasm::WasmModule* module = wasm_instance().module_object().module();
  return GetSourcePosition(module, function_index(), byte_offset(),
                           at_to_number_conversion());
}

int WasmFrame::byte_offset() const {
  wasm::WasmCode* code = wasm_code();
  int offset = static_cast<int>(pc() - code->instruction_start());
  return code->GetSourcePositionBefore(offset);
}

bool WasmFrame::is_inspectable() const {
  wasm::WasmCodeRefScope code_ref_scope;
  return wasm_code()->is_inspectable();
}

Object WasmFrame::context() const { return wasm_instance().native_context(); }

void WasmFrame::Summarize(std::vector<FrameSummary>* functions) const {
  DCHECK(functions->empty());
  // The {WasmCode*} escapes this scope via the {FrameSummary}, which is fine,
  // since this code object is part of our stack.
  wasm::WasmCodeRefScope code_ref_scope;
  wasm::WasmCode* code = wasm_code();
  int offset = static_cast<int>(pc() - code->instruction_start());
  Handle<WasmInstanceObject> instance(wasm_instance(), isolate());
  FrameSummary::WasmFrameSummary summary(isolate(), instance, code, offset,
                                         at_to_number_conversion());
  functions->push_back(summary);
}

bool WasmFrame::at_to_number_conversion() const {
  // Check whether our callee is a WASM_TO_JS frame, and this frame is at the
  // ToNumber conversion call.
  wasm::WasmCode* code =
      callee_pc() != kNullAddress
          ? wasm::GetWasmCodeManager()->LookupCode(callee_pc())
          : nullptr;
  if (!code || code->kind() != wasm::WasmCode::kWasmToJsWrapper) return false;
  int offset = static_cast<int>(callee_pc() - code->instruction_start());
  int pos = code->GetSourcePositionBefore(offset);
  // The imported call has position 0, ToNumber has position 1.
  // If there is no source position available, this is also not a ToNumber call.
  DCHECK(pos == wasm::kNoCodePosition || pos == 0 || pos == 1);
  return pos == 1;
}

int WasmFrame::LookupExceptionHandlerInTable() {
  wasm::WasmCode* code = wasm::GetWasmCodeManager()->LookupCode(pc());
  if (!code->IsAnonymous() && code->handler_table_size() > 0) {
    HandlerTable table(code);
    int pc_offset = static_cast<int>(pc() - code->instruction_start());
    return table.LookupReturn(pc_offset);
  }
  return -1;
}

void WasmDebugBreakFrame::Iterate(RootVisitor* v) const {
  DCHECK(caller_pc());
  wasm::WasmCode* code = wasm::GetWasmCodeManager()->LookupCode(caller_pc());
  DCHECK(code);
  SafepointTable table(code);
  SafepointEntry safepoint_entry = table.FindEntry(caller_pc());
  uint32_t tagged_register_indexes = safepoint_entry.tagged_register_indexes();

  while (tagged_register_indexes != 0) {
    int reg_code = base::bits::CountTrailingZeros(tagged_register_indexes);
    tagged_register_indexes &= ~(1 << reg_code);
    FullObjectSlot spill_slot(&Memory<Address>(
        fp() +
        WasmDebugBreakFrameConstants::GetPushedGpRegisterOffset(reg_code)));

    v->VisitRootPointer(Root::kStackRoots, nullptr, spill_slot);
  }
}

void WasmDebugBreakFrame::Print(StringStream* accumulator, PrintMode mode,
                                int index) const {
  PrintIndex(accumulator, mode, index);
  accumulator->Add("WasmDebugBreak");
  if (mode != OVERVIEW) accumulator->Add("\n");
}

void JsToWasmFrame::Iterate(RootVisitor* v) const {
  CodeLookupResult lookup_result = GetContainingCode(isolate(), pc());
  CHECK(lookup_result.IsFound());
#ifdef DEBUG
  Builtin builtin = lookup_result.builtin_id();
  DCHECK_EQ(builtin, Builtin::kGenericJSToWasmWrapper);
#endif  // DEBUG
  //  GenericJSToWasmWrapper stack layout
  //  ------+-----------------+----------------------
  //        |  return addr    |
  //    fp  |- - - - - - - - -|  -------------------|
  //        |       fp        |                     |
  //   fp-p |- - - - - - - - -|                     |
  //        |  frame marker   |                     | no GC scan
  //  fp-2p |- - - - - - - - -|                     |
  //        |   scan_count    |                     |
  //  fp-3p |- - - - - - - - -|  -------------------|
  //        |      ....       | <- spill_slot_limit |
  //        |   spill slots   |                     | GC scan scan_count slots
  //        |      ....       | <- spill_slot_base--|
  //        |- - - - - - - - -|                     |
  // The [fp + BuiltinFrameConstants::kGCScanSlotCount] on the stack is a value
  // indicating how many values should be scanned from the top.
  intptr_t scan_count = *reinterpret_cast<intptr_t*>(
      fp() + BuiltinWasmWrapperConstants::kGCScanSlotCountOffset);

  FullObjectSlot spill_slot_base(&Memory<Address>(sp()));
  FullObjectSlot spill_slot_limit(
      &Memory<Address>(sp() + scan_count * kSystemPointerSize));
  v->VisitRootPointers(Root::kStackRoots, nullptr, spill_slot_base,
                       spill_slot_limit);
}

void StackSwitchFrame::Iterate(RootVisitor* v) const {
  //  See JsToWasmFrame layout.
  //  We cannot DCHECK that the pc matches the expected builtin code here,
  //  because the return address is on a different stack.
  // The [fp + BuiltinFrameConstants::kGCScanSlotCountOffset] on the stack is a
  // value indicating how many values should be scanned from the top.
  intptr_t scan_count = *reinterpret_cast<intptr_t*>(
      fp() + BuiltinWasmWrapperConstants::kGCScanSlotCountOffset);

  FullObjectSlot spill_slot_base(&Memory<Address>(sp()));
  FullObjectSlot spill_slot_limit(
      &Memory<Address>(sp() + scan_count * kSystemPointerSize));
  v->VisitRootPointers(Root::kStackRoots, nullptr, spill_slot_base,
                       spill_slot_limit);
  // Also visit fixed spill slots that contain references.
  FullObjectSlot suspender_slot(
      &Memory<Address>(fp() + BuiltinWasmWrapperConstants::kSuspenderOffset));
  v->VisitRootPointer(Root::kStackRoots, nullptr, suspender_slot);
}

// static
void StackSwitchFrame::GetStateForJumpBuffer(wasm::JumpBuffer* jmpbuf,
                                             State* state) {
  DCHECK_NE(jmpbuf->fp, kNullAddress);
  DCHECK_EQ(ComputeFrameType(jmpbuf->fp), STACK_SWITCH);
  FillState(jmpbuf->fp, jmpbuf->sp, state);
  DCHECK_NE(*state->pc_address, kNullAddress);
}

int WasmCompileLazyFrame::GetFunctionIndex() const {
  Object func_index(Memory<Address>(
      sp() + WasmCompileLazyFrameConstants::kFunctionIndexOffset));
  return Smi::ToInt(func_index);
}

wasm::NativeModule* WasmCompileLazyFrame::GetNativeModule() const {
  return *reinterpret_cast<wasm::NativeModule**>(
      sp() + WasmCompileLazyFrameConstants::kNativeModuleOffset);
}

FullObjectSlot WasmCompileLazyFrame::wasm_instance_slot() const {
  return FullObjectSlot(&Memory<Address>(
      sp() + WasmCompileLazyFrameConstants::kWasmInstanceOffset));
}

void WasmCompileLazyFrame::Iterate(RootVisitor* v) const {
  FullObjectSlot spilled_instance_slot(&Memory<Address>(
      fp() + WasmCompileLazyFrameConstants::kInstanceSpillOffset));
  v->VisitRootPointer(Root::kStackRoots, "spilled wasm instance",
                      spilled_instance_slot);
  v->VisitRootPointer(Root::kStackRoots, "wasm instance parameter",
                      wasm_instance_slot());

  int func_index = GetFunctionIndex();
  wasm::NativeModule* native_module = GetNativeModule();
  if (!native_module) {
    // This GC was triggered by lazy compilation, because otherwise this frame
    // would not be on the stack. The native module gets set on the stack after
    // a successful compilation. The native module being nullptr means that
    // compilation failed, and we don't have to preserve any references because
    // the stack will get unwound immediately after the GC.
    return;
  }

  // Scan the spill slots of the parameter registers. Parameters in WebAssembly
  // get reordered such that first all value parameters get put into registers.
  // If there are more registers than value parameters, the remaining registers
  // are used for reference parameters. Therefore we can determine which
  // registers get used for which parameters by counting the number of value
  // parameters and the number of reference parameters.
  int num_int_params = 0;
  int num_ref_params = 0;
  const wasm::FunctionSig* sig =
      native_module->module()->functions[func_index].sig;
  for (auto param : sig->parameters()) {
    if (param == wasm::kWasmI32) {
      num_int_params++;
    } else if (param == wasm::kWasmI64) {
      num_int_params += kSystemPointerSize == 8 ? 1 : 2;
    } else if (param.is_reference()) {
      num_ref_params++;
    }
  }

  // There are no reference parameters, there is nothing to scan.
  if (num_ref_params == 0) return;

  int num_int_params_in_registers = std::min(
      num_int_params, WasmCompileLazyFrameConstants::kNumberOfSavedGpParamRegs);
  int num_ref_params_in_registers = std::min(
      num_ref_params, WasmCompileLazyFrameConstants::kNumberOfSavedGpParamRegs -
                          num_int_params_in_registers);

  for (int i = 0; i < num_ref_params_in_registers; ++i) {
    FullObjectSlot spill_slot(
        fp() + WasmCompileLazyFrameConstants::kParameterSpillsOffset
                   [num_int_params_in_registers + i]);

    v->VisitRootPointer(Root::kStackRoots, "register parameter", spill_slot);
  }

  // Next we scan the slots of stack parameters.
  wasm::WasmCode* wasm_code = native_module->GetCode(func_index);
  uint32_t first_tagged_stack_slot = wasm_code->first_tagged_parameter_slot();
  uint32_t num_tagged_stack_slots = wasm_code->num_tagged_parameter_slots();

  // Visit tagged parameters that have been passed to the function of this
  // frame. Conceptionally these parameters belong to the parent frame. However,
  // the exact count is only known by this frame (in the presence of tail calls,
  // this information cannot be derived from the call site).
  if (num_tagged_stack_slots > 0) {
    FullObjectSlot tagged_parameter_base(&Memory<Address>(caller_sp()));
    tagged_parameter_base += first_tagged_stack_slot;
    FullObjectSlot tagged_parameter_limit =
        tagged_parameter_base + num_tagged_stack_slots;

    v->VisitRootPointers(Root::kStackRoots, "stack parameter",
                         tagged_parameter_base, tagged_parameter_limit);
  }
}
#endif  // V8_ENABLE_WEBASSEMBLY

namespace {

void PrintFunctionSource(StringStream* accumulator, SharedFunctionInfo shared) {
  if (v8_flags.max_stack_trace_source_length != 0) {
    std::ostringstream os;
    os << "--------- s o u r c e   c o d e ---------\n"
       << SourceCodeOf(shared, v8_flags.max_stack_trace_source_length)
       << "\n-----------------------------------------\n";
    accumulator->Add(os.str().c_str());
  }
}

}  // namespace

void JavaScriptFrame::Print(StringStream* accumulator, PrintMode mode,
                            int index) const {
  Handle<SharedFunctionInfo> shared = handle(function().shared(), isolate());
  SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate(), shared);

  DisallowGarbageCollection no_gc;
  Object receiver = this->receiver();
  JSFunction function = this->function();

  accumulator->PrintSecurityTokenIfChanged(function);
  PrintIndex(accumulator, mode, index);
  PrintFrameKind(accumulator);
  if (IsConstructor()) accumulator->Add("new ");
  accumulator->PrintFunction(function, receiver);
  accumulator->Add(" [%p]", function);

  // Get scope information for nicer output, if possible. If code is nullptr, or
  // doesn't contain scope info, scope_info will return 0 for the number of
  // parameters, stack local variables, context local variables, stack slots,
  // or context slots.
  ScopeInfo scope_info = shared->scope_info();
  Object script_obj = shared->script();
  if (script_obj.IsScript()) {
    Script script = Script::cast(script_obj);
    accumulator->Add(" [");
    accumulator->PrintName(script.name());

    if (is_interpreted()) {
      const InterpretedFrame* iframe = InterpretedFrame::cast(this);
      BytecodeArray bytecodes = iframe->GetBytecodeArray();
      int offset = iframe->GetBytecodeOffset();
      int source_pos =
          AbstractCode::cast(bytecodes).SourcePosition(isolate(), offset);
      int line = script.GetLineNumber(source_pos) + 1;
      accumulator->Add(":%d] [bytecode=%p offset=%d]", line,
                       reinterpret_cast<void*>(bytecodes.ptr()), offset);
    } else {
      int function_start_pos = shared->StartPosition();
      int line = script.GetLineNumber(function_start_pos) + 1;
      accumulator->Add(":~%d] [pc=%p]", line, reinterpret_cast<void*>(pc()));
    }
  }

  accumulator->Add("(this=%o", receiver);

  // Print the parameters.
  int parameters_count = ComputeParametersCount();
  for (int i = 0; i < parameters_count; i++) {
    accumulator->Add(",");
    accumulator->Add("%o", GetParameter(i));
  }

  accumulator->Add(")");
  if (mode == OVERVIEW) {
    accumulator->Add("\n");
    return;
  }
  if (is_optimized()) {
    accumulator->Add(" {\n// optimized frame\n");
    PrintFunctionSource(accumulator, *shared);
    accumulator->Add("}\n");
    return;
  }
  accumulator->Add(" {\n");

  // Compute the number of locals and expression stack elements.
  int heap_locals_count = scope_info.ContextLocalCount();
  int expressions_count = ComputeExpressionsCount();

  // Try to get hold of the context of this frame.
  Context context;
  if (this->context().IsContext()) {
    context = Context::cast(this->context());
    while (context.IsWithContext()) {
      context = context.previous();
      DCHECK(!context.is_null());
    }
  }

  // Print heap-allocated local variables.
  if (heap_locals_count > 0) {
    accumulator->Add("  // heap-allocated locals\n");
  }
  for (auto it : ScopeInfo::IterateLocalNames(&scope_info, no_gc)) {
    accumulator->Add("  var ");
    accumulator->PrintName(it->name());
    accumulator->Add(" = ");
    if (!context.is_null()) {
      int slot_index = Context::MIN_CONTEXT_SLOTS + it->index();
      if (slot_index < context.length()) {
        accumulator->Add("%o", context.get(slot_index));
      } else {
        accumulator->Add(
            "// warning: missing context slot - inconsistent frame?");
      }
    } else {
      accumulator->Add("// warning: no context found - inconsistent frame?");
    }
    accumulator->Add("\n");
  }

  // Print the expression stack.
  if (0 < expressions_count) {
    accumulator->Add("  // expression stack (top to bottom)\n");
  }
  for (int i = expressions_count - 1; i >= 0; i--) {
    accumulator->Add("  [%02d] : %o\n", i, GetExpression(i));
  }

  PrintFunctionSource(accumulator, *shared);

  accumulator->Add("}\n\n");
}

void EntryFrame::Iterate(RootVisitor* v) const {
  IteratePc(v, pc_address(), constant_pool_address(), LookupCodeT());
}

void CommonFrame::IterateExpressions(RootVisitor* v) const {
  const int last_object_offset = StandardFrameConstants::kLastObjectOffset;
  intptr_t marker =
      Memory<intptr_t>(fp() + CommonFrameConstants::kContextOrFrameTypeOffset);
  FullObjectSlot base(&Memory<Address>(sp()));
  FullObjectSlot limit(&Memory<Address>(fp() + last_object_offset) + 1);
  if (StackFrame::IsTypeMarker(marker)) {
    v->VisitRootPointers(Root::kStackRoots, nullptr, base, limit);
  } else {
    // The frame contains the actual argument count (intptr) that should not be
    // visited.
    FullObjectSlot argc(
        &Memory<Address>(fp() + StandardFrameConstants::kArgCOffset));
    v->VisitRootPointers(Root::kStackRoots, nullptr, base, argc);
    v->VisitRootPointers(Root::kStackRoots, nullptr, argc + 1, limit);
  }
}

void JavaScriptFrame::Iterate(RootVisitor* v) const {
  IterateExpressions(v);
  IteratePc(v, pc_address(), constant_pool_address(), LookupCodeT());
}

void InternalFrame::Iterate(RootVisitor* v) const {
  CodeLookupResult code = LookupCodeT();
  IteratePc(v, pc_address(), constant_pool_address(), code);
  // Internal frames typically do not receive any arguments, hence their stack
  // only contains tagged pointers.
  // We are misusing the has_tagged_outgoing_params flag here to tell us whether
  // the full stack frame contains only tagged pointers or only raw values.
  // This is used for the WasmCompileLazy builtin, where we actually pass
  // untagged arguments and also store untagged values on the stack.
  if (code.has_tagged_outgoing_params()) IterateExpressions(v);
}

// -------------------------------------------------------------------------

namespace {

// Predictably converts PC to uint32 by calculating offset of the PC in
// from the embedded builtins start or from respective MemoryChunk.
uint32_t PcAddressForHashing(Isolate* isolate, Address address) {
  uint32_t hashable_address;
  if (OffHeapInstructionStream::TryGetAddressForHashing(isolate, address,
                                                        &hashable_address)) {
    return hashable_address;
  }
  return ObjectAddressForHashing(address);
}

}  // namespace

InnerPointerToCodeCache::InnerPointerToCodeCacheEntry*
InnerPointerToCodeCache::GetCacheEntry(Address inner_pointer) {
  DCHECK(base::bits::IsPowerOfTwo(kInnerPointerToCodeCacheSize));
  uint32_t hash =
      ComputeUnseededHash(PcAddressForHashing(isolate_, inner_pointer));
  uint32_t index = hash & (kInnerPointerToCodeCacheSize - 1);
  InnerPointerToCodeCacheEntry* entry = cache(index);
  if (entry->inner_pointer == inner_pointer) {
    if (DEBUG_BOOL) {
      CodeLookupResult lookup_result =
          isolate_->heap()->GcSafeFindCodeForInnerPointer(inner_pointer);
      CHECK(lookup_result.IsFound());
      CHECK_EQ(entry->code, lookup_result);
    }
  } else {
    // Because this code may be interrupted by a profiling signal that
    // also queries the cache, we cannot update inner_pointer before the code
    // has been set. Otherwise, we risk trying to use a cache entry before
    // the code has been computed.
    entry->code =
        isolate_->heap()->GcSafeFindCodeForInnerPointer(inner_pointer);
    if (entry->code.IsCode() && entry->code.code().is_maglevved()) {
      entry->maglev_safepoint_entry.Reset();
    } else {
      entry->safepoint_entry.Reset();
    }
    entry->inner_pointer = inner_pointer;
  }
  return entry;
}

// Frame layout helper class implementation.
// -------------------------------------------------------------------------

namespace {

// Some architectures need to push padding together with the TOS register
// in order to maintain stack alignment.
constexpr int TopOfStackRegisterPaddingSlots() {
  return ArgumentPaddingSlots(1);
}

bool BuiltinContinuationModeIsWithCatch(BuiltinContinuationMode mode) {
  switch (mode) {
    case BuiltinContinuationMode::STUB:
    case BuiltinContinuationMode::JAVASCRIPT:
      return false;
    case BuiltinContinuationMode::JAVASCRIPT_WITH_CATCH:
    case BuiltinContinuationMode::JAVASCRIPT_HANDLE_EXCEPTION:
      return true;
  }
  UNREACHABLE();
}

}  // namespace

UnoptimizedFrameInfo::UnoptimizedFrameInfo(int parameters_count_with_receiver,
                                           int translation_height,
                                           bool is_topmost, bool pad_arguments,
                                           FrameInfoKind frame_info_kind) {
  const int locals_count = translation_height;

  register_stack_slot_count_ =
      UnoptimizedFrameConstants::RegisterStackSlotCount(locals_count);

  static constexpr int kTheAccumulator = 1;
  static constexpr int kTopOfStackPadding = TopOfStackRegisterPaddingSlots();
  int maybe_additional_slots =
      (is_topmost || frame_info_kind == FrameInfoKind::kConservative)
          ? (kTheAccumulator + kTopOfStackPadding)
          : 0;
  frame_size_in_bytes_without_fixed_ =
      (register_stack_slot_count_ + maybe_additional_slots) *
      kSystemPointerSize;

  // The 'fixed' part of the frame consists of the incoming parameters and
  // the part described by InterpreterFrameConstants. This will include
  // argument padding, when needed.
  const int parameter_padding_slots =
      pad_arguments ? ArgumentPaddingSlots(parameters_count_with_receiver) : 0;
  const int fixed_frame_size =
      InterpreterFrameConstants::kFixedFrameSize +
      (parameters_count_with_receiver + parameter_padding_slots) *
          kSystemPointerSize;
  frame_size_in_bytes_ = frame_size_in_bytes_without_fixed_ + fixed_frame_size;
}

// static
uint32_t UnoptimizedFrameInfo::GetStackSizeForAdditionalArguments(
    int parameters_count) {
  return (parameters_count + ArgumentPaddingSlots(parameters_count)) *
         kSystemPointerSize;
}

ConstructStubFrameInfo::ConstructStubFrameInfo(int translation_height,
                                               bool is_topmost,
                                               FrameInfoKind frame_info_kind) {
  // Note: This is according to the Translation's notion of 'parameters' which
  // differs to that of the SharedFunctionInfo, e.g. by including the receiver.
  const int parameters_count = translation_height;

  // If the construct frame appears to be topmost we should ensure that the
  // value of result register is preserved during continuation execution.
  // We do this here by "pushing" the result of the constructor function to
  // the top of the reconstructed stack and popping it in
  // {Builtin::kNotifyDeoptimized}.

  static constexpr int kTopOfStackPadding = TopOfStackRegisterPaddingSlots();
  static constexpr int kTheResult = 1;
  const int argument_padding = ArgumentPaddingSlots(parameters_count);

  const int adjusted_height =
      (is_topmost || frame_info_kind == FrameInfoKind::kConservative)
          ? parameters_count + argument_padding + kTheResult +
                kTopOfStackPadding
          : parameters_count + argument_padding;
  frame_size_in_bytes_without_fixed_ = adjusted_height * kSystemPointerSize;
  frame_size_in_bytes_ = frame_size_in_bytes_without_fixed_ +
                         ConstructFrameConstants::kFixedFrameSize;
}

BuiltinContinuationFrameInfo::BuiltinContinuationFrameInfo(
    int translation_height,
    const CallInterfaceDescriptor& continuation_descriptor,
    const RegisterConfiguration* register_config, bool is_topmost,
    DeoptimizeKind deopt_kind, BuiltinContinuationMode continuation_mode,
    FrameInfoKind frame_info_kind) {
  const bool is_conservative = frame_info_kind == FrameInfoKind::kConservative;

  // Note: This is according to the Translation's notion of 'parameters' which
  // differs to that of the SharedFunctionInfo, e.g. by including the receiver.
  const int parameters_count = translation_height;
  frame_has_result_stack_slot_ =
      !is_topmost || deopt_kind == DeoptimizeKind::kLazy;
  const int result_slot_count =
      (frame_has_result_stack_slot_ || is_conservative) ? 1 : 0;

  const int exception_slot_count =
      (BuiltinContinuationModeIsWithCatch(continuation_mode) || is_conservative)
          ? 1
          : 0;

  const int allocatable_register_count =
      register_config->num_allocatable_general_registers();
  const int padding_slot_count =
      BuiltinContinuationFrameConstants::PaddingSlotCount(
          allocatable_register_count);

  const int register_parameter_count =
      continuation_descriptor.GetRegisterParameterCount();
  translated_stack_parameter_count_ =
      parameters_count - register_parameter_count;
  stack_parameter_count_ = translated_stack_parameter_count_ +
                           result_slot_count + exception_slot_count;
  const int stack_param_pad_count =
      ArgumentPaddingSlots(stack_parameter_count_);

  // If the builtins frame appears to be topmost we should ensure that the
  // value of result register is preserved during continuation execution.
  // We do this here by "pushing" the result of callback function to the
  // top of the reconstructed stack and popping it in
  // {Builtin::kNotifyDeoptimized}.
  static constexpr int kTopOfStackPadding = TopOfStackRegisterPaddingSlots();
  static constexpr int kTheResult = 1;
  const int push_result_count =
      (is_topmost || is_conservative) ? kTheResult + kTopOfStackPadding : 0;

  frame_size_in_bytes_ =
      kSystemPointerSize * (stack_parameter_count_ + stack_param_pad_count +
                            allocatable_register_count + padding_slot_count +
                            push_result_count) +
      BuiltinContinuationFrameConstants::kFixedFrameSize;

  frame_size_in_bytes_above_fp_ =
      kSystemPointerSize * (allocatable_register_count + padding_slot_count +
                            push_result_count) +
      (BuiltinContinuationFrameConstants::kFixedFrameSize -
       BuiltinContinuationFrameConstants::kFixedFrameSizeAboveFp);
}

}  // namespace internal
}  // namespace v8
