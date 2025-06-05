// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/assert-scope.h"
#include "src/common/message-template.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/frames.h"
#include "src/heap/factory.h"
#include "src/numbers/conversions.h"
#include "src/objects/objects-inl.h"
#include "src/strings/unicode-inl.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {

// TODO(13036): See if we can find a way to have the stack walker visit
// tagged values being passed from Wasm to runtime functions. In the meantime,
// disallow access to safe-looking-but-actually-unsafe stack-backed handles
// and thereby force manual creation of safe handles (backed by HandleScope).
class RuntimeArgumentsWithoutHandles : public RuntimeArguments {
 public:
  RuntimeArgumentsWithoutHandles(int length, Address* arguments)
      : RuntimeArguments(length, arguments) {}

 private:
  // Disallowing the superclass method.
  template <class S = Object>
  V8_INLINE Handle<S> at(int index) const;
};

#define RuntimeArguments RuntimeArgumentsWithoutHandles

// (End of TODO(13036)-related hackery.)

namespace {

template <typename FrameType>
class FrameFinder {
 public:
  explicit FrameFinder(Isolate* isolate,
                       std::initializer_list<StackFrame::Type>
                           skipped_frame_types = {StackFrame::EXIT})
      : frame_iterator_(isolate, isolate->thread_local_top()) {
    // We skip at least one frame.
    DCHECK_LT(0, skipped_frame_types.size());

    for (auto type : skipped_frame_types) {
      DCHECK_EQ(type, frame_iterator_.frame()->type());
      USE(type);
      frame_iterator_.Advance();
    }
    // Type check the frame where the iterator stopped now.
    DCHECK_NOT_NULL(frame());
  }

  FrameType* frame() { return FrameType::cast(frame_iterator_.frame()); }

 private:
  StackFrameIterator frame_iterator_;
};

WasmInstanceObject GetWasmInstanceOnStackTop(
    Isolate* isolate,
    std::initializer_list<StackFrame::Type> skipped_frame_types = {
        StackFrame::EXIT}) {
  return FrameFinder<WasmFrame>(isolate, skipped_frame_types)
      .frame()
      ->wasm_instance();
}

Context GetNativeContextFromWasmInstanceOnStackTop(Isolate* isolate) {
  return GetWasmInstanceOnStackTop(isolate).native_context();
}

class V8_NODISCARD ClearThreadInWasmScope {
 public:
  explicit ClearThreadInWasmScope(Isolate* isolate) : isolate_(isolate) {
    DCHECK_IMPLIES(trap_handler::IsTrapHandlerEnabled(),
                   trap_handler::IsThreadInWasm());
    trap_handler::ClearThreadInWasm();
  }
  ~ClearThreadInWasmScope() {
    DCHECK_IMPLIES(trap_handler::IsTrapHandlerEnabled(),
                   !trap_handler::IsThreadInWasm());
    if (!isolate_->has_pending_exception()) {
      trap_handler::SetThreadInWasm();
    }
    // Otherwise we only want to set the flag if the exception is caught in
    // wasm. This is handled by the unwinder.
  }

 private:
  Isolate* isolate_;
};

Object ThrowWasmError(Isolate* isolate, MessageTemplate message,
                      Handle<Object> arg0 = Handle<Object>()) {
  Handle<JSObject> error_obj =
      isolate->factory()->NewWasmRuntimeError(message, arg0);
  JSObject::AddProperty(isolate, error_obj,
                        isolate->factory()->wasm_uncatchable_symbol(),
                        isolate->factory()->true_value(), NONE);
  return isolate->Throw(*error_obj);
}
}  // namespace

// Takes a JS object and a wasm type as Smi. Type checks the object against the
// type; if the check succeeds, returns the object in its wasm representation;
// otherwise throws a type error.
RUNTIME_FUNCTION(Runtime_WasmJSToWasmObject) {
  // This code is called from wrappers, so the "thread is wasm" flag is not set.
  DCHECK_IMPLIES(trap_handler::IsTrapHandlerEnabled(),
                 !trap_handler::IsThreadInWasm());
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // 'raw_instance' can be either a WasmInstanceObject or undefined.
  Object raw_instance = args[0];
  Handle<Object> value(args[1], isolate);
  // Make sure ValueType fits properly in a Smi.
  static_assert(wasm::ValueType::kLastUsedBit + 1 <= kSmiValueSize);
  int raw_type = args.smi_value_at(2);

  const wasm::WasmModule* module =
      raw_instance.IsWasmInstanceObject()
          ? WasmInstanceObject::cast(raw_instance).module()
          : nullptr;

  wasm::ValueType type = wasm::ValueType::FromRawBitField(raw_type);
  const char* error_message;

  Handle<Object> result;
  bool success = internal::wasm::JSToWasmObject(isolate, module, value, type,
                                                &error_message)
                     .ToHandle(&result);
  if (success) return *result;
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kWasmTrapJSTypeError));
}

RUNTIME_FUNCTION(Runtime_WasmMemoryGrow) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  // {delta_pages} is checked to be a positive smi in the WasmMemoryGrow builtin
  // which calls this runtime function.
  uint32_t delta_pages = args.positive_smi_value_at(1);

  int ret = WasmMemoryObject::Grow(
      isolate, handle(instance.memory_object(), isolate), delta_pages);
  // The WasmMemoryGrow builtin which calls this runtime function expects us to
  // always return a Smi.
  DCHECK(!isolate->has_pending_exception());
  return Smi::FromInt(ret);
}

RUNTIME_FUNCTION(Runtime_ThrowWasmError) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  int message_id = args.smi_value_at(0);
  return ThrowWasmError(isolate, MessageTemplateFromInt(message_id));
}

RUNTIME_FUNCTION(Runtime_ThrowWasmStackOverflow) {
  ClearThreadInWasmScope clear_wasm_flag(isolate);
  SealHandleScope shs(isolate);
  DCHECK_LE(0, args.length());
  return isolate->StackOverflow();
}

RUNTIME_FUNCTION(Runtime_WasmThrowJSTypeError) {
  // The caller may be wasm or JS. Only clear the thread_in_wasm flag if the
  // caller is wasm, and let the unwinder set it back depending on the handler.
  if (trap_handler::IsTrapHandlerEnabled() && trap_handler::IsThreadInWasm()) {
    trap_handler::ClearThreadInWasm();
  }
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kWasmTrapJSTypeError));
}

// This error is thrown from a wasm-to-JS wrapper, so unlike
// Runtime_ThrowWasmError, this function does not check or unset the
// thread-in-wasm flag.
RUNTIME_FUNCTION(Runtime_ThrowBadSuspenderError) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  return ThrowWasmError(isolate, MessageTemplate::kWasmTrapBadSuspender);
}

RUNTIME_FUNCTION(Runtime_WasmThrow) {
  ClearThreadInWasmScope clear_wasm_flag(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  isolate->set_context(GetNativeContextFromWasmInstanceOnStackTop(isolate));
  Handle<WasmExceptionTag> tag(WasmExceptionTag::cast(args[0]), isolate);
  Handle<FixedArray> values(FixedArray::cast(args[1]), isolate);
  Handle<WasmExceptionPackage> exception =
      WasmExceptionPackage::New(isolate, tag, values);
  wasm::GetWasmEngine()->SampleThrowEvent(isolate);
  return isolate->Throw(*exception);
}

RUNTIME_FUNCTION(Runtime_WasmReThrow) {
  ClearThreadInWasmScope clear_wasm_flag(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  wasm::GetWasmEngine()->SampleRethrowEvent(isolate);
  return isolate->ReThrow(args[0]);
}

RUNTIME_FUNCTION(Runtime_WasmStackGuard) {
  ClearThreadInWasmScope wasm_flag(isolate);
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());

  // Check if this is a real stack overflow.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) return isolate->StackOverflow();

  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_WasmCompileLazy) {
  ClearThreadInWasmScope wasm_flag(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<WasmInstanceObject> instance(WasmInstanceObject::cast(args[0]),
                                      isolate);
  int func_index = args.smi_value_at(1);
  wasm::NativeModule** native_module_stack_slot =
      reinterpret_cast<wasm::NativeModule**>(args.address_of_arg_at(2));
  *native_module_stack_slot = nullptr;

  DCHECK(isolate->context().is_null());
  isolate->set_context(instance->native_context());
  bool success = wasm::CompileLazy(isolate, instance, func_index,
                                   native_module_stack_slot);
  if (!success) {
    {
      wasm::ThrowLazyCompilationError(
          isolate, instance->module_object().native_module(), func_index);
    }
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots{isolate}.exception();
  }

  return Smi::FromInt(wasm::JumpTableOffset(instance->module(), func_index));
}

namespace {
void ReplaceWrapper(Isolate* isolate, Handle<WasmInstanceObject> instance,
                    int function_index, Handle<CodeT> wrapper_code) {
  Handle<WasmInternalFunction> internal =
      WasmInstanceObject::GetWasmInternalFunction(isolate, instance,
                                                  function_index)
          .ToHandleChecked();
  Handle<WasmExternalFunction> exported_function =
      handle(WasmExternalFunction::cast(internal->external()), isolate);
  exported_function->set_code(*wrapper_code, kReleaseStore);
  WasmExportedFunctionData function_data =
      exported_function->shared().wasm_exported_function_data();
  function_data.set_wrapper_code(*wrapper_code);
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmCompileWrapper) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<WasmInstanceObject> instance(WasmInstanceObject::cast(args[0]),
                                      isolate);
  Handle<WasmExportedFunctionData> function_data(
      WasmExportedFunctionData::cast(args[1]), isolate);
  DCHECK(isolate->context().is_null());
  isolate->set_context(instance->native_context());

  const wasm::WasmModule* module = instance->module();
  const int function_index = function_data->function_index();
  const wasm::WasmFunction& function = module->functions[function_index];
  const wasm::FunctionSig* sig = function.sig;

  // The start function is not guaranteed to be registered as
  // an exported function (although it is called as one).
  // If there is no entry for the start function,
  // the tier-up is abandoned.
  if (WasmInstanceObject::GetWasmInternalFunction(isolate, instance,
                                                  function_index)
          .is_null()) {
    DCHECK_EQ(function_index, module->start_function_index);
    return ReadOnlyRoots(isolate).undefined_value();
  }

  Handle<CodeT> wrapper_code =
      wasm::JSToWasmWrapperCompilationUnit::CompileSpecificJSToWasmWrapper(
          isolate, sig, module);

  // Replace the wrapper for the function that triggered the tier-up.
  // This is to verify that the wrapper is replaced, even if the function
  // is implicitly exported and is not part of the export_table.
  ReplaceWrapper(isolate, instance, function_index, wrapper_code);

  // Iterate over all exports to replace eagerly the wrapper for all functions
  // that share the signature of the function that tiered up.
  for (wasm::WasmExport exp : module->export_table) {
    if (exp.kind != wasm::kExternalFunction) {
      continue;
    }
    int index = static_cast<int>(exp.index);
    const wasm::WasmFunction& exp_function = module->functions[index];
    if (exp_function.sig == sig && index != function_index) {
      ReplaceWrapper(isolate, instance, index, wrapper_code);
    }
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTriggerTierUp) {
  ClearThreadInWasmScope clear_wasm_flag(isolate);
  SealHandleScope shs(isolate);

  // We're reusing this interrupt mechanism to interrupt long-running loops.
  StackLimitCheck check(isolate);
  DCHECK(!check.JsHasOverflowed());
  if (check.InterruptRequested()) {
    Object result = isolate->stack_guard()->HandleInterrupts();
    if (result.IsException()) return result;
  }

  DisallowGarbageCollection no_gc;
  DCHECK_EQ(1, args.length());
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);

  FrameFinder<WasmFrame> frame_finder(isolate);
  int func_index = frame_finder.frame()->function_index();
  DCHECK_EQ(instance, frame_finder.frame()->wasm_instance());

  wasm::TriggerTierUp(instance, func_index);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmAtomicNotify) {
  ClearThreadInWasmScope clear_wasm_flag(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  double offset_double = args.number_value_at(1);
  uintptr_t offset = static_cast<uintptr_t>(offset_double);
  uint32_t count = NumberToUint32(args[2]);
  Handle<JSArrayBuffer> array_buffer{instance.memory_object().array_buffer(),
                                     isolate};
  // Should have trapped if address was OOB.
  DCHECK_LT(offset, array_buffer->byte_length());
  if (!array_buffer->is_shared()) return Smi::FromInt(0);
  return FutexEmulation::Wake(array_buffer, offset, count);
}

RUNTIME_FUNCTION(Runtime_WasmI32AtomicWait) {
  ClearThreadInWasmScope clear_wasm_flag(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  double offset_double = args.number_value_at(1);
  uintptr_t offset = static_cast<uintptr_t>(offset_double);
  int32_t expected_value = NumberToInt32(args[2]);
  BigInt timeout_ns = BigInt::cast(args[3]);

  Handle<JSArrayBuffer> array_buffer{instance.memory_object().array_buffer(),
                                     isolate};
  // Should have trapped if address was OOB.
  DCHECK_LT(offset, array_buffer->byte_length());

  // Trap if memory is not shared, or wait is not allowed on the isolate
  if (!array_buffer->is_shared() || !isolate->allow_atomics_wait()) {
    return ThrowWasmError(
        isolate, MessageTemplate::kAtomicsOperationNotAllowed,
        isolate->factory()->NewStringFromAsciiChecked("Atomics.wait"));
  }
  return FutexEmulation::WaitWasm32(isolate, array_buffer, offset,
                                    expected_value, timeout_ns.AsInt64());
}

RUNTIME_FUNCTION(Runtime_WasmI64AtomicWait) {
  ClearThreadInWasmScope clear_wasm_flag(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  double offset_double = args.number_value_at(1);
  uintptr_t offset = static_cast<uintptr_t>(offset_double);
  BigInt expected_value = BigInt::cast(args[2]);
  BigInt timeout_ns = BigInt::cast(args[3]);

  Handle<JSArrayBuffer> array_buffer{instance.memory_object().array_buffer(),
                                     isolate};
  // Should have trapped if address was OOB.
  DCHECK_LT(offset, array_buffer->byte_length());

  // Trap if memory is not shared, or if wait is not allowed on the isolate
  if (!array_buffer->is_shared() || !isolate->allow_atomics_wait()) {
    return ThrowWasmError(
        isolate, MessageTemplate::kAtomicsOperationNotAllowed,
        isolate->factory()->NewStringFromAsciiChecked("Atomics.wait"));
  }
  return FutexEmulation::WaitWasm64(isolate, array_buffer, offset,
                                    expected_value.AsInt64(),
                                    timeout_ns.AsInt64());
}

namespace {
Object ThrowTableOutOfBounds(Isolate* isolate,
                             Handle<WasmInstanceObject> instance) {
  // Handle out-of-bounds access here in the runtime call, rather
  // than having the lower-level layers deal with JS exceptions.
  if (isolate->context().is_null()) {
    isolate->set_context(instance->native_context());
  }
  return ThrowWasmError(isolate, MessageTemplate::kWasmTrapTableOutOfBounds);
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmRefFunc) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<WasmInstanceObject> instance(WasmInstanceObject::cast(args[0]),
                                      isolate);
  uint32_t function_index = args.positive_smi_value_at(1);

  return *WasmInstanceObject::GetOrCreateWasmInternalFunction(isolate, instance,
                                                              function_index);
}

RUNTIME_FUNCTION(Runtime_WasmFunctionTableGet) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  uint32_t table_index = args.positive_smi_value_at(1);
  uint32_t entry_index = args.positive_smi_value_at(2);
  DCHECK_LT(table_index, instance.tables().length());
  auto table = handle(WasmTableObject::cast(instance.tables().get(table_index)),
                      isolate);
  // We only use the runtime call for lazily initialized function references.
  DCHECK(
      table->instance().IsUndefined()
          ? table->type() == wasm::kWasmFuncRef
          : IsSubtypeOf(table->type(), wasm::kWasmFuncRef,
                        WasmInstanceObject::cast(table->instance()).module()));

  if (!WasmTableObject::IsInBounds(isolate, table, entry_index)) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapTableOutOfBounds);
  }

  return *WasmTableObject::Get(isolate, table, entry_index);
}

RUNTIME_FUNCTION(Runtime_WasmFunctionTableSet) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  uint32_t table_index = args.positive_smi_value_at(1);
  uint32_t entry_index = args.positive_smi_value_at(2);
  Handle<Object> element(args[3], isolate);
  DCHECK_LT(table_index, instance.tables().length());
  auto table = handle(WasmTableObject::cast(instance.tables().get(table_index)),
                      isolate);
  // We only use the runtime call for lazily initialized function references.
  DCHECK(
      table->instance().IsUndefined()
          ? table->type() == wasm::kWasmFuncRef
          : IsSubtypeOf(table->type(), wasm::kWasmFuncRef,
                        WasmInstanceObject::cast(table->instance()).module()));

  if (!WasmTableObject::IsInBounds(isolate, table, entry_index)) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapTableOutOfBounds);
  }
  WasmTableObject::Set(isolate, table, entry_index, element);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTableInit) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());
  Handle<WasmInstanceObject> instance(WasmInstanceObject::cast(args[0]),
                                      isolate);
  uint32_t table_index = args.positive_smi_value_at(1);
  uint32_t elem_segment_index = args.positive_smi_value_at(2);
  static_assert(
      wasm::kV8MaxWasmTableSize < kSmiMaxValue,
      "Make sure clamping to Smi range doesn't make an invalid call valid");
  uint32_t dst = args.positive_smi_value_at(3);
  uint32_t src = args.positive_smi_value_at(4);
  uint32_t count = args.positive_smi_value_at(5);

  DCHECK(!isolate->context().is_null());

  base::Optional<MessageTemplate> opt_error =
      WasmInstanceObject::InitTableEntries(isolate, instance, table_index,
                                           elem_segment_index, dst, src, count);
  if (opt_error.has_value()) {
    return ThrowWasmError(isolate, opt_error.value());
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTableCopy) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());
  Handle<WasmInstanceObject> instance(WasmInstanceObject::cast(args[0]),
                                      isolate);
  uint32_t table_dst_index = args.positive_smi_value_at(1);
  uint32_t table_src_index = args.positive_smi_value_at(2);
  static_assert(
      wasm::kV8MaxWasmTableSize < kSmiMaxValue,
      "Make sure clamping to Smi range doesn't make an invalid call valid");
  uint32_t dst = args.positive_smi_value_at(3);
  uint32_t src = args.positive_smi_value_at(4);
  uint32_t count = args.positive_smi_value_at(5);

  DCHECK(!isolate->context().is_null());

  bool oob = !WasmInstanceObject::CopyTableEntries(
      isolate, instance, table_dst_index, table_src_index, dst, src, count);
  if (oob) return ThrowTableOutOfBounds(isolate, instance);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmTableGrow) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  uint32_t table_index = args.positive_smi_value_at(1);
  Handle<Object> value(args[2], isolate);
  uint32_t delta = args.positive_smi_value_at(3);

  Handle<WasmTableObject> table(
      WasmTableObject::cast(instance.tables().get(table_index)), isolate);
  int result = WasmTableObject::Grow(isolate, table, delta, value);

  return Smi::FromInt(result);
}

RUNTIME_FUNCTION(Runtime_WasmTableFill) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  Handle<WasmInstanceObject> instance(WasmInstanceObject::cast(args[0]),
                                      isolate);
  uint32_t table_index = args.positive_smi_value_at(1);
  uint32_t start = args.positive_smi_value_at(2);
  Handle<Object> value(args[3], isolate);
  uint32_t count = args.positive_smi_value_at(4);

  Handle<WasmTableObject> table(
      WasmTableObject::cast(instance->tables().get(table_index)), isolate);

  uint32_t table_size = table->current_length();

  if (start > table_size) {
    return ThrowTableOutOfBounds(isolate, instance);
  }

  // Even when table.fill goes out-of-bounds, as many entries as possible are
  // put into the table. Only afterwards we trap.
  uint32_t fill_count = std::min(count, table_size - start);
  if (fill_count < count) {
    return ThrowTableOutOfBounds(isolate, instance);
  }
  WasmTableObject::Fill(isolate, table, start, value, fill_count);

  return ReadOnlyRoots(isolate).undefined_value();
}

namespace {
// Returns true if any breakpoint was hit, false otherwise.
bool ExecuteWasmDebugBreaks(Isolate* isolate,
                            Handle<WasmInstanceObject> instance,
                            WasmFrame* frame) {
  Handle<Script> script{instance->module_object().script(), isolate};
  auto* debug_info = instance->module_object().native_module()->GetDebugInfo();

  // Enter the debugger.
  DebugScope debug_scope(isolate->debug());

  // Check for instrumentation breakpoints first, but still execute regular
  // breakpoints afterwards.
  bool paused_on_instrumentation = false;
  DCHECK_EQ(script->break_on_entry(), !!instance->break_on_entry());
  if (script->break_on_entry()) {
    MaybeHandle<FixedArray> maybe_on_entry_breakpoints =
        WasmScript::CheckBreakPoints(isolate, script,
                                     WasmScript::kOnEntryBreakpointPosition,
                                     frame->id());
    script->set_break_on_entry(false);
    // Update the "break_on_entry" flag on all live instances.
    i::WeakArrayList weak_instance_list = script->wasm_weak_instance_list();
    for (int i = 0; i < weak_instance_list.length(); ++i) {
      if (weak_instance_list.Get(i)->IsCleared()) continue;
      i::WasmInstanceObject::cast(weak_instance_list.Get(i)->GetHeapObject())
          .set_break_on_entry(false);
    }
    DCHECK(!instance->break_on_entry());
    if (!maybe_on_entry_breakpoints.is_null()) {
      isolate->debug()->OnInstrumentationBreak();
      paused_on_instrumentation = true;
    }
  }

  if (debug_info->IsStepping(frame)) {
    debug_info->ClearStepping(isolate);
    StepAction step_action = isolate->debug()->last_step_action();
    isolate->debug()->ClearStepping();
    isolate->debug()->OnDebugBreak(isolate->factory()->empty_fixed_array(),
                                   step_action);
    return true;
  }

  // Check whether we hit a breakpoint.
  Handle<FixedArray> breakpoints;
  if (WasmScript::CheckBreakPoints(isolate, script, frame->position(),
                                   frame->id())
          .ToHandle(&breakpoints)) {
    debug_info->ClearStepping(isolate);
    StepAction step_action = isolate->debug()->last_step_action();
    isolate->debug()->ClearStepping();
    if (isolate->debug()->break_points_active()) {
      // We hit one or several breakpoints. Notify the debug listeners.
      isolate->debug()->OnDebugBreak(breakpoints, step_action);
    }
    return true;
  }

  return paused_on_instrumentation;
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmDebugBreak) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  FrameFinder<WasmFrame> frame_finder(
      isolate, {StackFrame::EXIT, StackFrame::WASM_DEBUG_BREAK});
  WasmFrame* frame = frame_finder.frame();
  auto instance = handle(frame->wasm_instance(), isolate);
  isolate->set_context(instance->native_context());

  if (!ExecuteWasmDebugBreaks(isolate, instance, frame)) {
    // We did not hit a breakpoint. If we are in stepping code, but the user did
    // not request stepping, clear this (to save further calls into this runtime
    // function).
    auto* debug_info =
        instance->module_object().native_module()->GetDebugInfo();
    debug_info->ClearStepping(frame);
  }

  // Execute a stack check before leaving this function. This is to handle any
  // interrupts set by the debugger (e.g. termination), but also to execute Wasm
  // code GC to get rid of temporarily created Wasm code.
  StackLimitCheck check(isolate);
  if (check.InterruptRequested()) {
    Object interrupt_object = isolate->stack_guard()->HandleInterrupts();
    // Interrupt handling can create an exception, including the
    // termination exception.
    if (interrupt_object.IsException(isolate)) return interrupt_object;
    DCHECK(interrupt_object.IsUndefined(isolate));
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

// Assumes copy ranges are in-bounds and copy length > 0.
RUNTIME_FUNCTION(Runtime_WasmArrayCopy) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DisallowGarbageCollection no_gc;
  DCHECK_EQ(5, args.length());
  WasmArray dst_array = WasmArray::cast(args[0]);
  uint32_t dst_index = args.positive_smi_value_at(1);
  WasmArray src_array = WasmArray::cast(args[2]);
  uint32_t src_index = args.positive_smi_value_at(3);
  uint32_t length = args.positive_smi_value_at(4);
  DCHECK_GT(length, 0);
  bool overlapping_ranges =
      dst_array.ptr() == src_array.ptr() &&
      (dst_index < src_index ? dst_index + length > src_index
                             : src_index + length > dst_index);
  wasm::ValueType element_type = src_array.type()->element_type();
  if (element_type.is_reference()) {
    ObjectSlot dst_slot = dst_array.ElementSlot(dst_index);
    ObjectSlot src_slot = src_array.ElementSlot(src_index);
    if (overlapping_ranges) {
      isolate->heap()->MoveRange(dst_array, dst_slot, src_slot, length,
                                 UPDATE_WRITE_BARRIER);
    } else {
      isolate->heap()->CopyRange(dst_array, dst_slot, src_slot, length,
                                 UPDATE_WRITE_BARRIER);
    }
  } else {
    void* dst = reinterpret_cast<void*>(dst_array.ElementAddress(dst_index));
    void* src = reinterpret_cast<void*>(src_array.ElementAddress(src_index));
    size_t copy_size = length * element_type.value_kind_size();
    if (overlapping_ranges) {
      MemMove(dst, src, copy_size);
    } else {
      MemCopy(dst, src, copy_size);
    }
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_WasmArrayNewSegment) {
  ClearThreadInWasmScope flag_scope(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  Handle<WasmInstanceObject> instance(WasmInstanceObject::cast(args[0]),
                                      isolate);
  uint32_t segment_index = args.positive_smi_value_at(1);
  uint32_t offset = args.positive_smi_value_at(2);
  uint32_t length = args.positive_smi_value_at(3);
  Handle<Map> rtt(Map::cast(args[4]), isolate);

  wasm::ArrayType* type =
      reinterpret_cast<wasm::ArrayType*>(rtt->wasm_type_info().native_type());

  uint32_t element_size = type->element_type().value_kind_size();
  // This check also implies no overflow.
  if (length > static_cast<uint32_t>(WasmArray::MaxLength(element_size))) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapArrayTooLarge);
  }

  if (type->element_type().is_numeric()) {
    uint32_t length_in_bytes = length * element_size;

    DCHECK_EQ(length_in_bytes / element_size, length);
    if (!base::IsInBounds<uint32_t>(
            offset, length_in_bytes,
            instance->data_segment_sizes().get(segment_index))) {
      return ThrowWasmError(isolate,
                            MessageTemplate::kWasmTrapDataSegmentOutOfBounds);
    }

    Address source =
        instance->data_segment_starts().get(segment_index) + offset;
    return *isolate->factory()->NewWasmArrayFromMemory(length, rtt, source);
  } else {
    const wasm::WasmElemSegment* elem_segment =
        &instance->module()->elem_segments[segment_index];
    if (!base::IsInBounds<size_t>(
            offset, length,
            instance->dropped_elem_segments().get(segment_index)
                ? 0
                : elem_segment->entries.size())) {
      return ThrowWasmError(
          isolate, MessageTemplate::kWasmTrapElementSegmentOutOfBounds);
    }

    Handle<Object> result = isolate->factory()->NewWasmArrayFromElementSegment(
        instance, elem_segment, offset, length, rtt);
    if (result->IsSmi()) {
      return ThrowWasmError(
          isolate, static_cast<MessageTemplate>(result->ToSmi().value()));
    } else {
      return *result;
    }
  }
}

namespace {
// Synchronize the stack limit with the active continuation for stack-switching.
// This can be done before or after changing the stack pointer itself, as long
// as we update both before the next stack check.
// {StackGuard::SetStackLimit} doesn't update the value of the jslimit if it
// contains a sentinel value, and it is also thread-safe. So if an interrupt is
// requested before, during or after this call, it will be preserved and handled
// at the next stack check.
void SyncStackLimit(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  auto continuation = WasmContinuationObject::cast(
      isolate->root(RootIndex::kActiveContinuation));
  auto stack = Managed<wasm::StackMemory>::cast(continuation.stack()).get();
  if (v8_flags.trace_wasm_stack_switching) {
    PrintF("Switch to stack #%d\n", stack->id());
  }
  uintptr_t limit = reinterpret_cast<uintptr_t>(stack->jmpbuf()->stack_limit);
  isolate->stack_guard()->SetStackLimit(limit);
}
}  // namespace

// Allocate a new suspender, and prepare for stack switching by updating the
// active continuation, active suspender and stack limit.
RUNTIME_FUNCTION(Runtime_WasmAllocateSuspender) {
  CHECK(v8_flags.experimental_wasm_stack_switching);
  HandleScope scope(isolate);
  Handle<WasmSuspenderObject> suspender = WasmSuspenderObject::New(isolate);

  // Update the continuation state.
  auto parent = handle(WasmContinuationObject::cast(
                           isolate->root(RootIndex::kActiveContinuation)),
                       isolate);
  Handle<WasmContinuationObject> target =
      WasmContinuationObject::New(isolate, wasm::JumpBuffer::Inactive, parent);
  auto target_stack =
      Managed<wasm::StackMemory>::cast(target->stack()).get().get();
  isolate->wasm_stacks()->Add(target_stack);
  isolate->roots_table().slot(RootIndex::kActiveContinuation).store(*target);

  // Update the suspender state.
  FullObjectSlot active_suspender_slot =
      isolate->roots_table().slot(RootIndex::kActiveSuspender);
  suspender->set_parent(HeapObject::cast(*active_suspender_slot));
  suspender->set_state(WasmSuspenderObject::kActive);
  suspender->set_continuation(*target);
  active_suspender_slot.store(*suspender);

  SyncStackLimit(isolate);
  wasm::JumpBuffer* jmpbuf = reinterpret_cast<wasm::JumpBuffer*>(
      parent->ReadExternalPointerField<kWasmContinuationJmpbufTag>(
          WasmContinuationObject::kJmpbufOffset, isolate));
  DCHECK_EQ(jmpbuf->state, wasm::JumpBuffer::Active);
  jmpbuf->state = wasm::JumpBuffer::Inactive;
  return *suspender;
}

// Update the stack limit after a stack switch, and preserve pending interrupts.
RUNTIME_FUNCTION(Runtime_WasmSyncStackLimit) {
  CHECK(v8_flags.experimental_wasm_stack_switching);
  SyncStackLimit(isolate);
  return ReadOnlyRoots(isolate).undefined_value();
}

// Takes a promise and a suspender, and returns
// promise.then(suspender.resume(), suspender.reject());
RUNTIME_FUNCTION(Runtime_WasmCreateResumePromise) {
  CHECK(v8_flags.experimental_wasm_stack_switching);
  HandleScope scope(isolate);
  Handle<Object> promise(args[0], isolate);
  WasmSuspenderObject suspender = WasmSuspenderObject::cast(args[1]);

  i::Handle<i::Object> argv[] = {handle(suspender.resume(), isolate),
                                 handle(suspender.reject(), isolate)};
  i::Handle<i::Object> result;
  bool has_pending_exception =
      !i::Execution::CallBuiltin(isolate, isolate->promise_then(), promise,
                                 arraysize(argv), argv)
           .ToHandle(&result);
  // TODO(thibaudm): Propagate exception.
  CHECK(!has_pending_exception);
  return *result;
}

// Returns the new string if the operation succeeds.  Otherwise throws an
// exception and returns an empty result.
RUNTIME_FUNCTION(Runtime_WasmStringNewWtf8) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(5, args.length());
  HandleScope scope(isolate);
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  uint32_t memory = args.positive_smi_value_at(1);
  uint32_t utf8_variant_value = args.positive_smi_value_at(2);
  uint32_t offset = NumberToUint32(args[3]);
  uint32_t size = NumberToUint32(args[4]);

  DCHECK_EQ(memory, 0);
  USE(memory);
  DCHECK(utf8_variant_value <=
         static_cast<uint32_t>(unibrow::Utf8Variant::kLastUtf8Variant));

  auto utf8_variant = static_cast<unibrow::Utf8Variant>(utf8_variant_value);

  uint64_t mem_size = instance.memory_size();
  if (!base::IsInBounds<uint64_t>(offset, size, mem_size)) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapMemOutOfBounds);
  }

  const base::Vector<const uint8_t> bytes{instance.memory_start() + offset,
                                          size};
  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromUtf8(bytes, utf8_variant));
}

RUNTIME_FUNCTION(Runtime_WasmStringNewWtf8Array) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(4, args.length());
  HandleScope scope(isolate);
  uint32_t utf8_variant_value = args.positive_smi_value_at(0);
  Handle<WasmArray> array(WasmArray::cast(args[1]), isolate);
  uint32_t start = NumberToUint32(args[2]);
  uint32_t end = NumberToUint32(args[3]);

  DCHECK(utf8_variant_value <=
         static_cast<uint32_t>(unibrow::Utf8Variant::kLastUtf8Variant));
  auto utf8_variant = static_cast<unibrow::Utf8Variant>(utf8_variant_value);

  RETURN_RESULT_OR_FAILURE(isolate, isolate->factory()->NewStringFromUtf8(
                                        array, start, end, utf8_variant));
}

RUNTIME_FUNCTION(Runtime_WasmStringNewWtf16) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(4, args.length());
  HandleScope scope(isolate);
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  uint32_t memory = args.positive_smi_value_at(1);
  uint32_t offset = NumberToUint32(args[2]);
  uint32_t size_in_codeunits = NumberToUint32(args[3]);

  DCHECK_EQ(memory, 0);
  USE(memory);

  uint64_t mem_size = instance.memory_size();
  if (size_in_codeunits > kMaxUInt32 / 2 ||
      !base::IsInBounds<uint64_t>(offset, size_in_codeunits * 2, mem_size)) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapMemOutOfBounds);
  }
  if (offset & 1) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapUnalignedAccess);
  }

  const byte* bytes = instance.memory_start() + offset;
  const base::uc16* codeunits = reinterpret_cast<const base::uc16*>(bytes);
  // TODO(12868): Override any exception with an uncatchable-by-wasm trap.
  RETURN_RESULT_OR_FAILURE(isolate,
                           isolate->factory()->NewStringFromTwoByteLittleEndian(
                               {codeunits, size_in_codeunits}));
}

RUNTIME_FUNCTION(Runtime_WasmStringNewWtf16Array) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(3, args.length());
  HandleScope scope(isolate);
  Handle<WasmArray> array(WasmArray::cast(args[0]), isolate);
  uint32_t start = NumberToUint32(args[1]);
  uint32_t end = NumberToUint32(args[2]);

  // TODO(12868): Override any exception with an uncatchable-by-wasm trap.
  RETURN_RESULT_OR_FAILURE(
      isolate, isolate->factory()->NewStringFromUtf16(array, start, end));
}

// Returns the new string if the operation succeeds.  Otherwise traps.
RUNTIME_FUNCTION(Runtime_WasmStringConst) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  static_assert(
      base::IsInRange(wasm::kV8MaxWasmStringLiterals, 0, Smi::kMaxValue));
  uint32_t index = args.positive_smi_value_at(1);

  DCHECK_LT(index, instance.module()->stringref_literals.size());

  const wasm::WasmStringRefLiteral& literal =
      instance.module()->stringref_literals[index];
  const base::Vector<const uint8_t> module_bytes =
      instance.module_object().native_module()->wire_bytes();
  const base::Vector<const uint8_t> string_bytes =
      module_bytes.SubVector(literal.source.offset(),
                             literal.source.offset() + literal.source.length());
  // TODO(12868): No need to re-validate WTF-8.  Also, result should be cached.
  return *isolate->factory()
              ->NewStringFromUtf8(string_bytes, unibrow::Utf8Variant::kWtf8)
              .ToHandleChecked();
}

namespace {
// TODO(12868): Consider unifying with api.cc:String::Utf8Length.
template <typename T>
int MeasureWtf8(base::Vector<const T> wtf16) {
  int previous = unibrow::Utf16::kNoPreviousCharacter;
  int length = 0;
  DCHECK(wtf16.size() <= String::kMaxLength);
  static_assert(String::kMaxLength <=
                (kMaxInt / unibrow::Utf8::kMaxEncodedSize));
  for (size_t i = 0; i < wtf16.size(); i++) {
    int current = wtf16[i];
    length += unibrow::Utf8::Length(current, previous);
    previous = current;
  }
  return length;
}
int MeasureWtf8(Isolate* isolate, Handle<String> string) {
  string = String::Flatten(isolate, string);
  DisallowGarbageCollection no_gc;
  String::FlatContent content = string->GetFlatContent(no_gc);
  DCHECK(content.IsFlat());
  return content.IsOneByte() ? MeasureWtf8(content.ToOneByteVector())
                             : MeasureWtf8(content.ToUC16Vector());
}
size_t MaxEncodedSize(base::Vector<const uint8_t> wtf16) {
  DCHECK(wtf16.size() < std::numeric_limits<size_t>::max() /
                            unibrow::Utf8::kMax8BitCodeUnitSize);
  return wtf16.size() * unibrow::Utf8::kMax8BitCodeUnitSize;
}
size_t MaxEncodedSize(base::Vector<const base::uc16> wtf16) {
  DCHECK(wtf16.size() < std::numeric_limits<size_t>::max() /
                            unibrow::Utf8::kMax16BitCodeUnitSize);
  return wtf16.size() * unibrow::Utf8::kMax16BitCodeUnitSize;
}
bool HasUnpairedSurrogate(base::Vector<const uint8_t> wtf16) { return false; }
bool HasUnpairedSurrogate(base::Vector<const base::uc16> wtf16) {
  return unibrow::Utf16::HasUnpairedSurrogate(wtf16.begin(), wtf16.size());
}
// TODO(12868): Consider unifying with api.cc:String::WriteUtf8.
template <typename T>
int EncodeWtf8(base::Vector<char> bytes, size_t offset,
               base::Vector<const T> wtf16, unibrow::Utf8Variant variant,
               MessageTemplate* message, MessageTemplate out_of_bounds) {
  // The first check is a quick estimate to decide whether the second check
  // is worth the computation.
  if (!base::IsInBounds<size_t>(offset, MaxEncodedSize(wtf16), bytes.size()) &&
      !base::IsInBounds<size_t>(offset, MeasureWtf8(wtf16), bytes.size())) {
    *message = out_of_bounds;
    return -1;
  }

  bool replace_invalid = false;
  switch (variant) {
    case unibrow::Utf8Variant::kWtf8:
      break;
    case unibrow::Utf8Variant::kUtf8:
      if (HasUnpairedSurrogate(wtf16)) {
        *message = MessageTemplate::kWasmTrapStringIsolatedSurrogate;
        return -1;
      }
      break;
    case unibrow::Utf8Variant::kLossyUtf8:
      replace_invalid = true;
      break;
    default:
      UNREACHABLE();
  }

  char* dst_start = bytes.begin() + offset;
  char* dst = dst_start;
  int previous = unibrow::Utf16::kNoPreviousCharacter;
  for (auto code_unit : wtf16) {
    dst += unibrow::Utf8::Encode(dst, code_unit, previous, replace_invalid);
    previous = code_unit;
  }
  DCHECK_LE(dst - dst_start, static_cast<ptrdiff_t>(kMaxInt));
  return static_cast<int>(dst - dst_start);
}
template <typename GetWritableBytes>
Object EncodeWtf8(Isolate* isolate, unibrow::Utf8Variant variant,
                  Handle<String> string, GetWritableBytes get_writable_bytes,
                  size_t offset, MessageTemplate out_of_bounds_message) {
  string = String::Flatten(isolate, string);
  MessageTemplate message;
  int written;
  {
    DisallowGarbageCollection no_gc;
    String::FlatContent content = string->GetFlatContent(no_gc);
    base::Vector<char> dst = get_writable_bytes(no_gc);
    written = content.IsOneByte()
                  ? EncodeWtf8(dst, offset, content.ToOneByteVector(), variant,
                               &message, out_of_bounds_message)
                  : EncodeWtf8(dst, offset, content.ToUC16Vector(), variant,
                               &message, out_of_bounds_message);
  }
  if (written < 0) {
    DCHECK_NE(message, MessageTemplate::kNone);
    return ThrowWasmError(isolate, message);
  }
  return *isolate->factory()->NewNumberFromInt(written);
}
}  // namespace

RUNTIME_FUNCTION(Runtime_WasmStringMeasureUtf8) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  Handle<String> string(String::cast(args[0]), isolate);

  string = String::Flatten(isolate, string);
  int length;
  {
    DisallowGarbageCollection no_gc;
    String::FlatContent content = string->GetFlatContent(no_gc);
    DCHECK(content.IsFlat());
    if (content.IsOneByte()) {
      length = MeasureWtf8(content.ToOneByteVector());
    } else {
      base::Vector<const base::uc16> code_units = content.ToUC16Vector();
      if (unibrow::Utf16::HasUnpairedSurrogate(code_units.begin(),
                                               code_units.size())) {
        length = -1;
      } else {
        length = MeasureWtf8(code_units);
      }
    }
  }
  return *isolate->factory()->NewNumberFromInt(length);
}

RUNTIME_FUNCTION(Runtime_WasmStringMeasureWtf8) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  Handle<String> string(String::cast(args[0]), isolate);

  int length = MeasureWtf8(isolate, string);
  return *isolate->factory()->NewNumberFromInt(length);
}

RUNTIME_FUNCTION(Runtime_WasmStringEncodeWtf8) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(5, args.length());
  HandleScope scope(isolate);
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  uint32_t memory = args.positive_smi_value_at(1);
  uint32_t utf8_variant_value = args.positive_smi_value_at(2);
  Handle<String> string(String::cast(args[3]), isolate);
  uint32_t offset = NumberToUint32(args[4]);

  DCHECK_EQ(memory, 0);
  USE(memory);
  DCHECK(utf8_variant_value <=
         static_cast<uint32_t>(unibrow::Utf8Variant::kLastUtf8Variant));

  char* memory_start = reinterpret_cast<char*>(instance.memory_start());
  auto utf8_variant = static_cast<unibrow::Utf8Variant>(utf8_variant_value);
  auto get_writable_bytes =
      [&](const DisallowGarbageCollection&) -> base::Vector<char> {
    return {memory_start, instance.memory_size()};
  };
  return EncodeWtf8(isolate, utf8_variant, string, get_writable_bytes, offset,
                    MessageTemplate::kWasmTrapMemOutOfBounds);
}

RUNTIME_FUNCTION(Runtime_WasmStringEncodeWtf8Array) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(4, args.length());
  HandleScope scope(isolate);
  uint32_t utf8_variant_value = args.positive_smi_value_at(0);
  Handle<String> string(String::cast(args[1]), isolate);
  Handle<WasmArray> array(WasmArray::cast(args[2]), isolate);
  uint32_t start = NumberToUint32(args[3]);

  DCHECK(utf8_variant_value <=
         static_cast<uint32_t>(unibrow::Utf8Variant::kLastUtf8Variant));
  auto utf8_variant = static_cast<unibrow::Utf8Variant>(utf8_variant_value);
  auto get_writable_bytes =
      [&](const DisallowGarbageCollection&) -> base::Vector<char> {
    return {reinterpret_cast<char*>(array->ElementAddress(0)), array->length()};
  };
  return EncodeWtf8(isolate, utf8_variant, string, get_writable_bytes, start,
                    MessageTemplate::kWasmTrapArrayOutOfBounds);
}

RUNTIME_FUNCTION(Runtime_WasmStringEncodeWtf16) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(6, args.length());
  HandleScope scope(isolate);
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  uint32_t memory = args.positive_smi_value_at(1);
  String string = String::cast(args[2]);
  uint32_t offset = NumberToUint32(args[3]);
  uint32_t start = args.positive_smi_value_at(4);
  uint32_t length = args.positive_smi_value_at(5);

  DCHECK_EQ(memory, 0);
  USE(memory);
  DCHECK(base::IsInBounds<uint32_t>(start, length, string.length()));

  size_t mem_size = instance.memory_size();
  static_assert(String::kMaxLength <=
                (std::numeric_limits<size_t>::max() / sizeof(base::uc16)));
  if (!base::IsInBounds<size_t>(offset, length * sizeof(base::uc16),
                                mem_size)) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapMemOutOfBounds);
  }
  if (offset & 1) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapUnalignedAccess);
  }

#if defined(V8_TARGET_LITTLE_ENDIAN)
  uint16_t* dst = reinterpret_cast<uint16_t*>(instance.memory_start() + offset);
  String::WriteToFlat(string, dst, start, length);
#elif defined(V8_TARGET_BIG_ENDIAN)
  // TODO(12868): The host is big-endian but we need to write the string
  // contents as little-endian.
  USE(string);
  USE(start);
  UNIMPLEMENTED();
#else
#error Unknown endianness
#endif

  return Smi::zero();  // Unused.
}

RUNTIME_FUNCTION(Runtime_WasmStringAsWtf8) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  Handle<String> string(String::cast(args[0]), isolate);
  int wtf8_length = MeasureWtf8(isolate, string);
  Handle<ByteArray> array = isolate->factory()->NewByteArray(wtf8_length);

  auto utf8_variant = unibrow::Utf8Variant::kWtf8;
  auto get_writable_bytes =
      [&](const DisallowGarbageCollection&) -> base::Vector<char> {
    return {reinterpret_cast<char*>(array->GetDataStartAddress()),
            static_cast<size_t>(wtf8_length)};
  };
  EncodeWtf8(isolate, utf8_variant, string, get_writable_bytes, 0,
             MessageTemplate::kWasmTrapArrayOutOfBounds);
  return *array;
}

RUNTIME_FUNCTION(Runtime_WasmStringViewWtf8Encode) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(6, args.length());
  HandleScope scope(isolate);
  WasmInstanceObject instance = WasmInstanceObject::cast(args[0]);
  uint32_t utf8_variant_value = args.positive_smi_value_at(1);
  Handle<ByteArray> array(ByteArray::cast(args[2]), isolate);
  uint32_t addr = NumberToUint32(args[3]);
  uint32_t start = NumberToUint32(args[4]);
  uint32_t end = NumberToUint32(args[5]);

  DCHECK(utf8_variant_value <=
         static_cast<uint32_t>(unibrow::Utf8Variant::kLastUtf8Variant));
  DCHECK_LE(start, end);
  DCHECK(base::IsInBounds<size_t>(start, end - start, array->length()));

  auto utf8_variant = static_cast<unibrow::Utf8Variant>(utf8_variant_value);
  size_t length = end - start;

  if (!base::IsInBounds<size_t>(addr, length, instance.memory_size())) {
    return ThrowWasmError(isolate, MessageTemplate::kWasmTrapMemOutOfBounds);
  }

  byte* memory_start = reinterpret_cast<byte*>(instance.memory_start());
  const byte* src =
      reinterpret_cast<const byte*>(array->GetDataStartAddress() + start);
  byte* dst = memory_start + addr;

  std::vector<size_t> surrogates;
  if (utf8_variant != unibrow::Utf8Variant::kWtf8) {
    unibrow::Wtf8::ScanForSurrogates({src, length}, &surrogates);
    if (utf8_variant == unibrow::Utf8Variant::kUtf8 && !surrogates.empty()) {
      return ThrowWasmError(isolate,
                            MessageTemplate::kWasmTrapStringIsolatedSurrogate);
    }
  }

  MemCopy(dst, src, length);

  for (size_t surrogate : surrogates) {
    DCHECK_LT(surrogate, length);
    DCHECK_EQ(utf8_variant, unibrow::Utf8Variant::kLossyUtf8);
    unibrow::Utf8::Encode(reinterpret_cast<char*>(dst + surrogate),
                          unibrow::Utf8::kBadChar, 0, false);
  }

  // Unused.
  return Smi(0);
}

RUNTIME_FUNCTION(Runtime_WasmStringViewWtf8Slice) {
  ClearThreadInWasmScope flag_scope(isolate);
  DCHECK_EQ(3, args.length());
  HandleScope scope(isolate);
  Handle<ByteArray> array(ByteArray::cast(args[0]), isolate);
  uint32_t start = NumberToUint32(args[1]);
  uint32_t end = NumberToUint32(args[2]);

  DCHECK_LT(start, end);
  DCHECK(base::IsInBounds<size_t>(start, end - start, array->length()));

  RETURN_RESULT_OR_FAILURE(isolate,
                           isolate->factory()->NewStringFromUtf8(
                               array, start, end, unibrow::Utf8Variant::kWtf8));
}

}  // namespace internal
}  // namespace v8
