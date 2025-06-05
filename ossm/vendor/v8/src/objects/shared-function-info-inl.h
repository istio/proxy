// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SHARED_FUNCTION_INFO_INL_H_
#define V8_OBJECTS_SHARED_FUNCTION_INFO_INL_H_

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/common/globals.h"
#include "src/handles/handles-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/scope-info-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/templates-inl.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/shared-function-info-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(PreparseData)

int PreparseData::inner_start_offset() const {
  return InnerOffset(data_length());
}

ObjectSlot PreparseData::inner_data_start() const {
  return RawField(inner_start_offset());
}

void PreparseData::clear_padding() {
  int data_end_offset = kDataStartOffset + data_length();
  int padding_size = inner_start_offset() - data_end_offset;
  DCHECK_LE(0, padding_size);
  if (padding_size == 0) return;
  memset(reinterpret_cast<void*>(address() + data_end_offset), 0, padding_size);
}

byte PreparseData::get(int index) const {
  DCHECK_LE(0, index);
  DCHECK_LT(index, data_length());
  int offset = kDataStartOffset + index * kByteSize;
  return ReadField<byte>(offset);
}

void PreparseData::set(int index, byte value) {
  DCHECK_LE(0, index);
  DCHECK_LT(index, data_length());
  int offset = kDataStartOffset + index * kByteSize;
  WriteField<byte>(offset, value);
}

void PreparseData::copy_in(int index, const byte* buffer, int length) {
  DCHECK(index >= 0 && length >= 0 && length <= kMaxInt - index &&
         index + length <= this->data_length());
  Address dst_addr = field_address(kDataStartOffset + index * kByteSize);
  memcpy(reinterpret_cast<void*>(dst_addr), buffer, length);
}

PreparseData PreparseData::get_child(int index) const {
  return PreparseData::cast(get_child_raw(index));
}

Object PreparseData::get_child_raw(int index) const {
  DCHECK_LE(0, index);
  DCHECK_LT(index, this->children_length());
  int offset = inner_start_offset() + index * kTaggedSize;
  return RELAXED_READ_FIELD(*this, offset);
}

void PreparseData::set_child(int index, PreparseData value,
                             WriteBarrierMode mode) {
  DCHECK_LE(0, index);
  DCHECK_LT(index, this->children_length());
  int offset = inner_start_offset() + index * kTaggedSize;
  RELAXED_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

TQ_OBJECT_CONSTRUCTORS_IMPL(UncompiledData)
TQ_OBJECT_CONSTRUCTORS_IMPL(UncompiledDataWithoutPreparseData)
TQ_OBJECT_CONSTRUCTORS_IMPL(UncompiledDataWithPreparseData)
TQ_OBJECT_CONSTRUCTORS_IMPL(UncompiledDataWithoutPreparseDataWithJob)
TQ_OBJECT_CONSTRUCTORS_IMPL(UncompiledDataWithPreparseDataAndJob)

TQ_OBJECT_CONSTRUCTORS_IMPL(InterpreterData)
TQ_OBJECT_CONSTRUCTORS_IMPL(SharedFunctionInfo)
NEVER_READ_ONLY_SPACE_IMPL(SharedFunctionInfo)
DEFINE_DEOPT_ELEMENT_ACCESSORS(SharedFunctionInfo, Object)

RELEASE_ACQUIRE_ACCESSORS(SharedFunctionInfo, function_data, Object,
                          kFunctionDataOffset)
RELEASE_ACQUIRE_ACCESSORS(SharedFunctionInfo, name_or_scope_info, Object,
                          kNameOrScopeInfoOffset)
RELEASE_ACQUIRE_ACCESSORS(SharedFunctionInfo, script_or_debug_info, HeapObject,
                          kScriptOrDebugInfoOffset)

RENAME_TORQUE_ACCESSORS(SharedFunctionInfo,
                        raw_outer_scope_info_or_feedback_metadata,
                        outer_scope_info_or_feedback_metadata, HeapObject)
DEF_ACQUIRE_GETTER(SharedFunctionInfo,
                   raw_outer_scope_info_or_feedback_metadata, HeapObject) {
  HeapObject value =
      TaggedField<HeapObject, kOuterScopeInfoOrFeedbackMetadataOffset>::
          Acquire_Load(cage_base, *this);
  return value;
}

uint16_t SharedFunctionInfo::internal_formal_parameter_count_with_receiver()
    const {
  const uint16_t param_count = TorqueGeneratedClass::formal_parameter_count();
  return param_count;
}

uint16_t SharedFunctionInfo::internal_formal_parameter_count_without_receiver()
    const {
  const uint16_t param_count = TorqueGeneratedClass::formal_parameter_count();
  if (param_count == kDontAdaptArgumentsSentinel) return param_count;
  return param_count - kJSArgcReceiverSlots;
}

void SharedFunctionInfo::set_internal_formal_parameter_count(int value) {
  DCHECK_EQ(value, static_cast<uint16_t>(value));
  DCHECK_GE(value, kJSArgcReceiverSlots);
  TorqueGeneratedClass::set_formal_parameter_count(value);
}

RENAME_PRIMITIVE_TORQUE_ACCESSORS(SharedFunctionInfo, raw_function_token_offset,
                                  function_token_offset, uint16_t)

RELAXED_INT32_ACCESSORS(SharedFunctionInfo, flags, kFlagsOffset)
int32_t SharedFunctionInfo::relaxed_flags() const {
  return flags(kRelaxedLoad);
}
void SharedFunctionInfo::set_relaxed_flags(int32_t flags) {
  return set_flags(flags, kRelaxedStore);
}

UINT8_ACCESSORS(SharedFunctionInfo, flags2, kFlags2Offset)

bool SharedFunctionInfo::HasSharedName() const {
  Object value = name_or_scope_info(kAcquireLoad);
  if (value.IsScopeInfo()) {
    return ScopeInfo::cast(value).HasSharedFunctionName();
  }
  return value != kNoSharedNameSentinel;
}

String SharedFunctionInfo::Name() const {
  if (!HasSharedName()) return GetReadOnlyRoots().empty_string();
  Object value = name_or_scope_info(kAcquireLoad);
  if (value.IsScopeInfo()) {
    if (ScopeInfo::cast(value).HasFunctionName()) {
      return String::cast(ScopeInfo::cast(value).FunctionName());
    }
    return GetReadOnlyRoots().empty_string();
  }
  return String::cast(value);
}

void SharedFunctionInfo::SetName(String name) {
  Object maybe_scope_info = name_or_scope_info(kAcquireLoad);
  if (maybe_scope_info.IsScopeInfo()) {
    ScopeInfo::cast(maybe_scope_info).SetFunctionName(name);
  } else {
    DCHECK(maybe_scope_info.IsString() ||
           maybe_scope_info == kNoSharedNameSentinel);
    set_name_or_scope_info(name, kReleaseStore);
  }
  UpdateFunctionMapIndex();
}

bool SharedFunctionInfo::is_script() const {
  return scope_info(kAcquireLoad).is_script_scope() &&
         Script::cast(script()).compilation_type() ==
             Script::COMPILATION_TYPE_HOST;
}

bool SharedFunctionInfo::needs_script_context() const {
  return is_script() && scope_info(kAcquireLoad).ContextLocalCount() > 0;
}

template <typename IsolateT>
AbstractCode SharedFunctionInfo::abstract_code(IsolateT* isolate) {
  // TODO(v8:11429): Decide if this return bytecode or baseline code, when the
  // latter is present.
  if (HasBytecodeArray(isolate)) {
    return AbstractCode::cast(GetBytecodeArray(isolate));
  } else {
    return ToAbstractCode(GetCode());
  }
}

int SharedFunctionInfo::function_token_position() const {
  int offset = raw_function_token_offset();
  if (offset == kFunctionTokenOutOfRange) {
    return kNoSourcePosition;
  } else {
    return StartPosition() - offset;
  }
}

template <typename IsolateT>
bool SharedFunctionInfo::AreSourcePositionsAvailable(IsolateT* isolate) const {
  if (v8_flags.enable_lazy_source_positions) {
    return !HasBytecodeArray() ||
           GetBytecodeArray(isolate).HasSourcePositionTable();
  }
  return true;
}

template <typename IsolateT>
SharedFunctionInfo::Inlineability SharedFunctionInfo::GetInlineability(
    IsolateT* isolate) const {
  if (!script().IsScript()) return kHasNoScript;

  if (GetIsolate()->is_precise_binary_code_coverage() &&
      !has_reported_binary_coverage()) {
    // We may miss invocations if this function is inlined.
    return kNeedsBinaryCoverage;
  }

  // Built-in functions are handled by the JSCallReducer.
  if (HasBuiltinId()) return kIsBuiltin;

  if (!IsUserJavaScript()) return kIsNotUserCode;

  // If there is no bytecode array, it is either not compiled or it is compiled
  // with WebAssembly for the asm.js pipeline. In either case we don't want to
  // inline.
  if (!HasBytecodeArray()) return kHasNoBytecode;

  if (GetBytecodeArray(isolate).length() > v8_flags.max_inlined_bytecode_size) {
    return kExceedsBytecodeLimit;
  }

  if (HasBreakInfo()) return kMayContainBreakPoints;

  if (optimization_disabled()) return kHasOptimizationDisabled;

  return kIsInlineable;
}

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2, class_scope_has_private_brand,
                    SharedFunctionInfo::ClassScopeHasPrivateBrandBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2,
                    has_static_private_methods_or_accessors,
                    SharedFunctionInfo::HasStaticPrivateMethodsOrAccessorsBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2, is_sparkplug_compiling,
                    SharedFunctionInfo::IsSparkplugCompilingBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2, maglev_compilation_failed,
                    SharedFunctionInfo::MaglevCompilationFailedBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, flags2, sparkplug_compiled,
                    SharedFunctionInfo::SparkplugCompiledBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, syntax_kind,
                    SharedFunctionInfo::FunctionSyntaxKindBits)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, allows_lazy_compilation,
                    SharedFunctionInfo::AllowLazyCompilationBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, has_duplicate_parameters,
                    SharedFunctionInfo::HasDuplicateParametersBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, native,
                    SharedFunctionInfo::IsNativeBit)
#if V8_ENABLE_WEBASSEMBLY
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, is_asm_wasm_broken,
                    SharedFunctionInfo::IsAsmWasmBrokenBit)
#endif  // V8_ENABLE_WEBASSEMBLY
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags,
                    requires_instance_members_initializer,
                    SharedFunctionInfo::RequiresInstanceMembersInitializerBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags,
                    name_should_print_as_anonymous,
                    SharedFunctionInfo::NameShouldPrintAsAnonymousBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags,
                    has_reported_binary_coverage,
                    SharedFunctionInfo::HasReportedBinaryCoverageBit)

BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, is_toplevel,
                    SharedFunctionInfo::IsTopLevelBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags, properties_are_final,
                    SharedFunctionInfo::PropertiesAreFinalBit)
BIT_FIELD_ACCESSORS(SharedFunctionInfo, relaxed_flags,
                    private_name_lookup_skips_outer_class,
                    SharedFunctionInfo::PrivateNameLookupSkipsOuterClassBit)

bool SharedFunctionInfo::optimization_disabled() const {
  return disabled_optimization_reason() != BailoutReason::kNoReason;
}

BailoutReason SharedFunctionInfo::disabled_optimization_reason() const {
  return DisabledOptimizationReasonBits::decode(flags(kRelaxedLoad));
}

LanguageMode SharedFunctionInfo::language_mode() const {
  static_assert(LanguageModeSize == 2);
  return construct_language_mode(IsStrictBit::decode(flags(kRelaxedLoad)));
}

void SharedFunctionInfo::set_language_mode(LanguageMode language_mode) {
  static_assert(LanguageModeSize == 2);
  // We only allow language mode transitions that set the same language mode
  // again or go up in the chain:
  DCHECK(is_sloppy(this->language_mode()) || is_strict(language_mode));
  int hints = flags(kRelaxedLoad);
  hints = IsStrictBit::update(hints, is_strict(language_mode));
  set_flags(hints, kRelaxedStore);
  UpdateFunctionMapIndex();
}

FunctionKind SharedFunctionInfo::kind() const {
  static_assert(FunctionKindBits::kSize == kFunctionKindBitSize);
  return FunctionKindBits::decode(flags(kRelaxedLoad));
}

void SharedFunctionInfo::set_kind(FunctionKind kind) {
  int hints = flags(kRelaxedLoad);
  hints = FunctionKindBits::update(hints, kind);
  hints = IsClassConstructorBit::update(hints, IsClassConstructor(kind));
  set_flags(hints, kRelaxedStore);
  UpdateFunctionMapIndex();
}

bool SharedFunctionInfo::is_wrapped() const {
  return syntax_kind() == FunctionSyntaxKind::kWrapped;
}

bool SharedFunctionInfo::construct_as_builtin() const {
  return ConstructAsBuiltinBit::decode(flags(kRelaxedLoad));
}

void SharedFunctionInfo::CalculateConstructAsBuiltin() {
  bool uses_builtins_construct_stub = false;
  if (HasBuiltinId()) {
    Builtin id = builtin_id();
    if (id != Builtin::kCompileLazy && id != Builtin::kEmptyFunction) {
      uses_builtins_construct_stub = true;
    }
  } else if (IsApiFunction()) {
    uses_builtins_construct_stub = true;
  }

  int f = flags(kRelaxedLoad);
  f = ConstructAsBuiltinBit::update(f, uses_builtins_construct_stub);
  set_flags(f, kRelaxedStore);
}

int SharedFunctionInfo::function_map_index() const {
  // Note: Must be kept in sync with the FastNewClosure builtin.
  int index = Context::FIRST_FUNCTION_MAP_INDEX +
              FunctionMapIndexBits::decode(flags(kRelaxedLoad));
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  return index;
}

void SharedFunctionInfo::set_function_map_index(int index) {
  static_assert(Context::LAST_FUNCTION_MAP_INDEX <=
                Context::FIRST_FUNCTION_MAP_INDEX + FunctionMapIndexBits::kMax);
  DCHECK_LE(Context::FIRST_FUNCTION_MAP_INDEX, index);
  DCHECK_LE(index, Context::LAST_FUNCTION_MAP_INDEX);
  index -= Context::FIRST_FUNCTION_MAP_INDEX;
  set_flags(FunctionMapIndexBits::update(flags(kRelaxedLoad), index),
            kRelaxedStore);
}

void SharedFunctionInfo::clear_padding() {
  memset(reinterpret_cast<void*>(this->address() + kSize), 0,
         kAlignedSize - kSize);
}

void SharedFunctionInfo::UpdateFunctionMapIndex() {
  int map_index =
      Context::FunctionMapIndex(language_mode(), kind(), HasSharedName());
  set_function_map_index(map_index);
}

void SharedFunctionInfo::DontAdaptArguments() {
#if V8_ENABLE_WEBASSEMBLY
  // TODO(leszeks): Revise this DCHECK now that the code field is gone.
  DCHECK(!HasWasmExportedFunctionData());
#endif  // V8_ENABLE_WEBASSEMBLY
  TorqueGeneratedClass::set_formal_parameter_count(kDontAdaptArgumentsSentinel);
}

bool SharedFunctionInfo::IsDontAdaptArguments() const {
  return TorqueGeneratedClass::formal_parameter_count() ==
         kDontAdaptArgumentsSentinel;
}

DEF_ACQUIRE_GETTER(SharedFunctionInfo, scope_info, ScopeInfo) {
  Object maybe_scope_info = name_or_scope_info(cage_base, kAcquireLoad);
  if (maybe_scope_info.IsScopeInfo(cage_base)) {
    return ScopeInfo::cast(maybe_scope_info);
  }
  return GetReadOnlyRoots().empty_scope_info();
}

DEF_GETTER(SharedFunctionInfo, scope_info, ScopeInfo) {
  return scope_info(cage_base, kAcquireLoad);
}

void SharedFunctionInfo::SetScopeInfo(ScopeInfo scope_info,
                                      WriteBarrierMode mode) {
  // Move the existing name onto the ScopeInfo.
  Object name = name_or_scope_info(kAcquireLoad);
  if (name.IsScopeInfo()) {
    name = ScopeInfo::cast(name).FunctionName();
  }
  DCHECK(name.IsString() || name == kNoSharedNameSentinel);
  // Only set the function name for function scopes.
  scope_info.SetFunctionName(name);
  if (HasInferredName() && inferred_name().length() != 0) {
    scope_info.SetInferredFunctionName(inferred_name());
  }
  set_name_or_scope_info(scope_info, kReleaseStore, mode);
}

void SharedFunctionInfo::set_raw_scope_info(ScopeInfo scope_info,
                                            WriteBarrierMode mode) {
  WRITE_FIELD(*this, kNameOrScopeInfoOffset, scope_info);
  CONDITIONAL_WRITE_BARRIER(*this, kNameOrScopeInfoOffset, scope_info, mode);
}

HeapObject SharedFunctionInfo::outer_scope_info() const {
  DCHECK(!is_compiled());
  DCHECK(!HasFeedbackMetadata());
  return raw_outer_scope_info_or_feedback_metadata();
}

bool SharedFunctionInfo::HasOuterScopeInfo() const {
  ScopeInfo outer_info;
  if (!is_compiled()) {
    if (!outer_scope_info().IsScopeInfo()) return false;
    outer_info = ScopeInfo::cast(outer_scope_info());
  } else {
    ScopeInfo info = scope_info(kAcquireLoad);
    if (!info.HasOuterScopeInfo()) return false;
    outer_info = info.OuterScopeInfo();
  }
  return !outer_info.IsEmpty();
}

ScopeInfo SharedFunctionInfo::GetOuterScopeInfo() const {
  DCHECK(HasOuterScopeInfo());
  if (!is_compiled()) return ScopeInfo::cast(outer_scope_info());
  return scope_info(kAcquireLoad).OuterScopeInfo();
}

void SharedFunctionInfo::set_outer_scope_info(HeapObject value,
                                              WriteBarrierMode mode) {
  DCHECK(!is_compiled());
  DCHECK(raw_outer_scope_info_or_feedback_metadata().IsTheHole());
  DCHECK(value.IsScopeInfo() || value.IsTheHole());
  set_raw_outer_scope_info_or_feedback_metadata(value, mode);
}

bool SharedFunctionInfo::HasFeedbackMetadata() const {
  return raw_outer_scope_info_or_feedback_metadata().IsFeedbackMetadata();
}

bool SharedFunctionInfo::HasFeedbackMetadata(AcquireLoadTag tag) const {
  return raw_outer_scope_info_or_feedback_metadata(tag).IsFeedbackMetadata();
}

FeedbackMetadata SharedFunctionInfo::feedback_metadata() const {
  DCHECK(HasFeedbackMetadata());
  return FeedbackMetadata::cast(raw_outer_scope_info_or_feedback_metadata());
}

RELEASE_ACQUIRE_ACCESSORS_CHECKED2(SharedFunctionInfo, feedback_metadata,
                                   FeedbackMetadata,
                                   kOuterScopeInfoOrFeedbackMetadataOffset,
                                   HasFeedbackMetadata(kAcquireLoad),
                                   !HasFeedbackMetadata(kAcquireLoad) &&
                                       value.IsFeedbackMetadata())

bool SharedFunctionInfo::is_compiled() const {
  Object data = function_data(kAcquireLoad);
  return data != Smi::FromEnum(Builtin::kCompileLazy) &&
         !data.IsUncompiledData();
}

template <typename IsolateT>
IsCompiledScope SharedFunctionInfo::is_compiled_scope(IsolateT* isolate) const {
  return IsCompiledScope(*this, isolate);
}

IsCompiledScope::IsCompiledScope(const SharedFunctionInfo shared,
                                 Isolate* isolate)
    : is_compiled_(shared.is_compiled()) {
  if (shared.HasBaselineCode()) {
    retain_code_ = handle(shared.baseline_code(kAcquireLoad), isolate);
  } else if (shared.HasBytecodeArray()) {
    retain_code_ = handle(shared.GetBytecodeArray(isolate), isolate);
  } else {
    retain_code_ = MaybeHandle<HeapObject>();
  }

  DCHECK_IMPLIES(!retain_code_.is_null(), is_compiled());
}

IsCompiledScope::IsCompiledScope(const SharedFunctionInfo shared,
                                 LocalIsolate* isolate)
    : is_compiled_(shared.is_compiled()) {
  if (shared.HasBaselineCode()) {
    retain_code_ = isolate->heap()->NewPersistentHandle(
        shared.baseline_code(kAcquireLoad));
  } else if (shared.HasBytecodeArray()) {
    retain_code_ =
        isolate->heap()->NewPersistentHandle(shared.GetBytecodeArray(isolate));
  } else {
    retain_code_ = MaybeHandle<HeapObject>();
  }

  DCHECK_IMPLIES(!retain_code_.is_null(), is_compiled());
}

bool SharedFunctionInfo::has_simple_parameters() {
  return scope_info(kAcquireLoad).HasSimpleParameters();
}

bool SharedFunctionInfo::CanCollectSourcePosition(Isolate* isolate) {
  return v8_flags.enable_lazy_source_positions && HasBytecodeArray() &&
         !GetBytecodeArray(isolate).HasSourcePositionTable();
}

bool SharedFunctionInfo::IsApiFunction() const {
  return function_data(kAcquireLoad).IsFunctionTemplateInfo();
}

FunctionTemplateInfo SharedFunctionInfo::get_api_func_data() const {
  DCHECK(IsApiFunction());
  return FunctionTemplateInfo::cast(function_data(kAcquireLoad));
}

DEF_GETTER(SharedFunctionInfo, HasBytecodeArray, bool) {
  Object data = function_data(cage_base, kAcquireLoad);
  if (!data.IsHeapObject()) return false;
  InstanceType instance_type =
      HeapObject::cast(data).map(cage_base).instance_type();
  return InstanceTypeChecker::IsBytecodeArray(instance_type) ||
         InstanceTypeChecker::IsInterpreterData(instance_type) ||
         InstanceTypeChecker::IsCodeT(instance_type);
}

template <typename IsolateT>
BytecodeArray SharedFunctionInfo::GetBytecodeArray(IsolateT* isolate) const {
  // TODO(ishell): access shared_function_info_access() via IsolateT.
  SharedMutexGuardIfOffThread<IsolateT, base::kShared> mutex_guard(
      GetIsolate()->shared_function_info_access(), isolate);

  DCHECK(HasBytecodeArray());
  if (HasDebugInfo() && GetDebugInfo().HasInstrumentedBytecodeArray()) {
    return GetDebugInfo().OriginalBytecodeArray();
  }

  return GetActiveBytecodeArray();
}

BytecodeArray SharedFunctionInfo::GetActiveBytecodeArray() const {
  Object data = function_data(kAcquireLoad);
  if (data.IsCodeT()) {
    CodeT baseline_code = CodeT::cast(data);
    data = baseline_code.bytecode_or_interpreter_data();
  }
  if (data.IsBytecodeArray()) {
    return BytecodeArray::cast(data);
  } else {
    DCHECK(data.IsInterpreterData());
    return InterpreterData::cast(data).bytecode_array();
  }
}

void SharedFunctionInfo::SetActiveBytecodeArray(BytecodeArray bytecode) {
  // We don't allow setting the active bytecode array on baseline-optimized
  // functions. They should have been flushed earlier.
  DCHECK(!HasBaselineCode());

  Object data = function_data(kAcquireLoad);
  if (data.IsBytecodeArray()) {
    set_function_data(bytecode, kReleaseStore);
  } else {
    DCHECK(data.IsInterpreterData());
    interpreter_data().set_bytecode_array(bytecode);
  }
}

void SharedFunctionInfo::set_bytecode_array(BytecodeArray bytecode) {
  DCHECK(function_data(kAcquireLoad) == Smi::FromEnum(Builtin::kCompileLazy) ||
         HasUncompiledData());
  set_function_data(bytecode, kReleaseStore);
}

bool SharedFunctionInfo::ShouldFlushCode(
    base::EnumSet<CodeFlushMode> code_flush_mode) {
  if (IsFlushingDisabled(code_flush_mode)) return false;

  // TODO(rmcilroy): Enable bytecode flushing for resumable functions.
  if (IsResumableFunction(kind()) || !allows_lazy_compilation()) {
    return false;
  }

  // Get a snapshot of the function data field, and if it is a bytecode array,
  // check if it is old. Note, this is done this way since this function can be
  // called by the concurrent marker.
  Object data = function_data(kAcquireLoad);
  if (data.IsCodeT()) {
    CodeT baseline_code = CodeT::cast(data);
    DCHECK_EQ(baseline_code.kind(), CodeKind::BASELINE);
    // If baseline code flushing isn't enabled and we have baseline data on SFI
    // we cannot flush baseline / bytecode.
    if (!IsBaselineCodeFlushingEnabled(code_flush_mode)) return false;
    data = baseline_code.bytecode_or_interpreter_data();
  } else if (!IsByteCodeFlushingEnabled(code_flush_mode)) {
    // If bytecode flushing isn't enabled and there is no baseline code there is
    // nothing to flush.
    return false;
  }
  if (!data.IsBytecodeArray()) return false;

  if (IsStressFlushingEnabled(code_flush_mode)) return true;

  BytecodeArray bytecode = BytecodeArray::cast(data);

  return bytecode.IsOld();
}

DEF_GETTER(SharedFunctionInfo, InterpreterTrampoline, CodeT) {
  DCHECK(HasInterpreterData(cage_base));
  return interpreter_data(cage_base).interpreter_trampoline(cage_base);
}

DEF_GETTER(SharedFunctionInfo, HasInterpreterData, bool) {
  Object data = function_data(cage_base, kAcquireLoad);
  if (data.IsCodeT(cage_base)) {
    CodeT baseline_code = CodeT::cast(data);
    DCHECK_EQ(baseline_code.kind(), CodeKind::BASELINE);
    data = baseline_code.bytecode_or_interpreter_data(cage_base);
  }
  return data.IsInterpreterData(cage_base);
}

DEF_GETTER(SharedFunctionInfo, interpreter_data, InterpreterData) {
  DCHECK(HasInterpreterData(cage_base));
  Object data = function_data(cage_base, kAcquireLoad);
  if (data.IsCodeT(cage_base)) {
    CodeT baseline_code = CodeT::cast(data);
    DCHECK_EQ(baseline_code.kind(), CodeKind::BASELINE);
    data = baseline_code.bytecode_or_interpreter_data(cage_base);
  }
  return InterpreterData::cast(data);
}

void SharedFunctionInfo::set_interpreter_data(
    InterpreterData interpreter_data) {
  DCHECK(v8_flags.interpreted_frames_native_stack);
  DCHECK(!HasBaselineCode());
  set_function_data(interpreter_data, kReleaseStore);
}

DEF_GETTER(SharedFunctionInfo, HasBaselineCode, bool) {
  Object data = function_data(cage_base, kAcquireLoad);
  if (data.IsCodeT(cage_base)) {
    DCHECK_EQ(CodeT::cast(data).kind(), CodeKind::BASELINE);
    return true;
  }
  return false;
}

DEF_ACQUIRE_GETTER(SharedFunctionInfo, baseline_code, CodeT) {
  DCHECK(HasBaselineCode(cage_base));
  return CodeT::cast(function_data(cage_base, kAcquireLoad));
}

void SharedFunctionInfo::set_baseline_code(CodeT baseline_code,
                                           ReleaseStoreTag tag,
                                           WriteBarrierMode mode) {
  DCHECK_EQ(baseline_code.kind(), CodeKind::BASELINE);
  set_function_data(baseline_code, tag, mode);
}

void SharedFunctionInfo::FlushBaselineCode() {
  DCHECK(HasBaselineCode());
  set_function_data(baseline_code(kAcquireLoad).bytecode_or_interpreter_data(),
                    kReleaseStore);
}

#if V8_ENABLE_WEBASSEMBLY
bool SharedFunctionInfo::HasAsmWasmData() const {
  return function_data(kAcquireLoad).IsAsmWasmData();
}

bool SharedFunctionInfo::HasWasmFunctionData() const {
  return function_data(kAcquireLoad).IsWasmFunctionData();
}

bool SharedFunctionInfo::HasWasmExportedFunctionData() const {
  return function_data(kAcquireLoad).IsWasmExportedFunctionData();
}

bool SharedFunctionInfo::HasWasmJSFunctionData() const {
  return function_data(kAcquireLoad).IsWasmJSFunctionData();
}

bool SharedFunctionInfo::HasWasmCapiFunctionData() const {
  return function_data(kAcquireLoad).IsWasmCapiFunctionData();
}

bool SharedFunctionInfo::HasWasmResumeData() const {
  return function_data(kAcquireLoad).IsWasmResumeData();
}

AsmWasmData SharedFunctionInfo::asm_wasm_data() const {
  DCHECK(HasAsmWasmData());
  return AsmWasmData::cast(function_data(kAcquireLoad));
}

void SharedFunctionInfo::set_asm_wasm_data(AsmWasmData data) {
  DCHECK(function_data(kAcquireLoad) == Smi::FromEnum(Builtin::kCompileLazy) ||
         HasUncompiledData() || HasAsmWasmData());
  set_function_data(data, kReleaseStore);
}

const wasm::WasmModule* SharedFunctionInfo::wasm_module() const {
  if (!HasWasmExportedFunctionData()) return nullptr;
  const WasmExportedFunctionData& function_data = wasm_exported_function_data();
  const WasmInstanceObject& wasm_instance = function_data.instance();
  const WasmModuleObject& wasm_module_object = wasm_instance.module_object();
  return wasm_module_object.module();
}

const wasm::FunctionSig* SharedFunctionInfo::wasm_function_signature() const {
  const wasm::WasmModule* module = wasm_module();
  if (!module) return nullptr;
  const WasmExportedFunctionData& function_data = wasm_exported_function_data();
  DCHECK_LT(function_data.function_index(), module->functions.size());
  return module->functions[function_data.function_index()].sig;
}
#endif  // V8_ENABLE_WEBASSEMBLY

bool SharedFunctionInfo::HasBuiltinId() const {
  return function_data(kAcquireLoad).IsSmi();
}

Builtin SharedFunctionInfo::builtin_id() const {
  DCHECK(HasBuiltinId());
  int id = Smi::ToInt(function_data(kAcquireLoad));
  DCHECK(Builtins::IsBuiltinId(id));
  return Builtins::FromInt(id);
}

void SharedFunctionInfo::set_builtin_id(Builtin builtin) {
  DCHECK(Builtins::IsBuiltinId(builtin));
  set_function_data(Smi::FromInt(static_cast<int>(builtin)), kReleaseStore,
                    SKIP_WRITE_BARRIER);
}

bool SharedFunctionInfo::HasUncompiledData() const {
  return function_data(kAcquireLoad).IsUncompiledData();
}

UncompiledData SharedFunctionInfo::uncompiled_data() const {
  DCHECK(HasUncompiledData());
  return UncompiledData::cast(function_data(kAcquireLoad));
}

void SharedFunctionInfo::set_uncompiled_data(UncompiledData uncompiled_data) {
  DCHECK(function_data(kAcquireLoad) == Smi::FromEnum(Builtin::kCompileLazy) ||
         HasUncompiledData());
  DCHECK(uncompiled_data.IsUncompiledData());
  set_function_data(uncompiled_data, kReleaseStore);
}

bool SharedFunctionInfo::HasUncompiledDataWithPreparseData() const {
  return function_data(kAcquireLoad).IsUncompiledDataWithPreparseData();
}

UncompiledDataWithPreparseData
SharedFunctionInfo::uncompiled_data_with_preparse_data() const {
  DCHECK(HasUncompiledDataWithPreparseData());
  return UncompiledDataWithPreparseData::cast(function_data(kAcquireLoad));
}

void SharedFunctionInfo::set_uncompiled_data_with_preparse_data(
    UncompiledDataWithPreparseData uncompiled_data_with_preparse_data) {
  DCHECK(function_data(kAcquireLoad) == Smi::FromEnum(Builtin::kCompileLazy));
  DCHECK(uncompiled_data_with_preparse_data.IsUncompiledDataWithPreparseData());
  set_function_data(uncompiled_data_with_preparse_data, kReleaseStore);
}

bool SharedFunctionInfo::HasUncompiledDataWithoutPreparseData() const {
  return function_data(kAcquireLoad).IsUncompiledDataWithoutPreparseData();
}

void SharedFunctionInfo::ClearUncompiledDataJobPointer() {
  UncompiledData uncompiled_data = this->uncompiled_data();
  if (uncompiled_data.IsUncompiledDataWithPreparseDataAndJob()) {
    UncompiledDataWithPreparseDataAndJob::cast(uncompiled_data)
        .set_job(kNullAddress);
  } else if (uncompiled_data.IsUncompiledDataWithoutPreparseDataWithJob()) {
    UncompiledDataWithoutPreparseDataWithJob::cast(uncompiled_data)
        .set_job(kNullAddress);
  }
}

void SharedFunctionInfo::ClearPreparseData() {
  DCHECK(HasUncompiledDataWithPreparseData());
  UncompiledDataWithPreparseData data = uncompiled_data_with_preparse_data();

  // Trim off the pre-parsed scope data from the uncompiled data by swapping the
  // map, leaving only an uncompiled data without pre-parsed scope.
  DisallowGarbageCollection no_gc;
  Heap* heap = GetHeapFromWritableObject(data);

  // We are basically trimming that object to its supertype, so recorded slots
  // within the object don't need to be invalidated.
  heap->NotifyObjectLayoutChange(data, no_gc, InvalidateRecordedSlots::kNo);
  static_assert(UncompiledDataWithoutPreparseData::kSize <
                UncompiledDataWithPreparseData::kSize);
  static_assert(UncompiledDataWithoutPreparseData::kSize ==
                UncompiledData::kHeaderSize);

  // Fill the remaining space with filler and clear slots in the trimmed area.
  heap->NotifyObjectSizeChange(data, UncompiledDataWithPreparseData::kSize,
                               UncompiledDataWithoutPreparseData::kSize,
                               ClearRecordedSlots::kYes);

  // Swap the map.
  data.set_map(GetReadOnlyRoots().uncompiled_data_without_preparse_data_map(),
               kReleaseStore);

  // Ensure that the clear was successful.
  DCHECK(HasUncompiledDataWithoutPreparseData());
}

void UncompiledData::InitAfterBytecodeFlush(
    String inferred_name, int start_position, int end_position,
    std::function<void(HeapObject object, ObjectSlot slot, HeapObject target)>
        gc_notify_updated_slot) {
  set_inferred_name(inferred_name);
  gc_notify_updated_slot(*this, RawField(UncompiledData::kInferredNameOffset),
                         inferred_name);
  set_start_position(start_position);
  set_end_position(end_position);
}

DEF_GETTER(SharedFunctionInfo, script, HeapObject) {
  HeapObject maybe_script = script_or_debug_info(cage_base, kAcquireLoad);
  if (maybe_script.IsDebugInfo(cage_base)) {
    return DebugInfo::cast(maybe_script).script();
  }
  return maybe_script;
}

void SharedFunctionInfo::set_script(HeapObject script) {
  HeapObject maybe_debug_info = script_or_debug_info(kAcquireLoad);
  if (maybe_debug_info.IsDebugInfo()) {
    DebugInfo::cast(maybe_debug_info).set_script(script);
  } else {
    set_script_or_debug_info(script, kReleaseStore);
  }
}

bool SharedFunctionInfo::is_repl_mode() const {
  return script().IsScript() && Script::cast(script()).is_repl_mode();
}

DEF_GETTER(SharedFunctionInfo, HasDebugInfo, bool) {
  return script_or_debug_info(cage_base, kAcquireLoad).IsDebugInfo(cage_base);
}

DEF_GETTER(SharedFunctionInfo, GetDebugInfo, DebugInfo) {
  auto debug_info = script_or_debug_info(cage_base, kAcquireLoad);
  DCHECK(debug_info.IsDebugInfo(cage_base));
  return DebugInfo::cast(debug_info);
}

void SharedFunctionInfo::SetDebugInfo(DebugInfo debug_info) {
  DCHECK(!HasDebugInfo());
  DCHECK_EQ(debug_info.script(), script_or_debug_info(kAcquireLoad));
  set_script_or_debug_info(debug_info, kReleaseStore);
}

bool SharedFunctionInfo::HasInferredName() {
  Object scope_info = name_or_scope_info(kAcquireLoad);
  if (scope_info.IsScopeInfo()) {
    return ScopeInfo::cast(scope_info).HasInferredFunctionName();
  }
  return HasUncompiledData();
}

String SharedFunctionInfo::inferred_name() const {
  Object maybe_scope_info = name_or_scope_info(kAcquireLoad);
  if (maybe_scope_info.IsScopeInfo()) {
    ScopeInfo scope_info = ScopeInfo::cast(maybe_scope_info);
    if (scope_info.HasInferredFunctionName()) {
      Object name = scope_info.InferredFunctionName();
      if (name.IsString()) return String::cast(name);
    }
  } else if (HasUncompiledData()) {
    return uncompiled_data().inferred_name();
  }
  return GetReadOnlyRoots().empty_string();
}

bool SharedFunctionInfo::IsUserJavaScript() const {
  Object script_obj = script();
  if (script_obj.IsUndefined()) return false;
  Script script = Script::cast(script_obj);
  return script.IsUserJavaScript();
}

bool SharedFunctionInfo::IsSubjectToDebugging() const {
#if V8_ENABLE_WEBASSEMBLY
  if (HasAsmWasmData()) return false;
#endif  // V8_ENABLE_WEBASSEMBLY
  return IsUserJavaScript();
}

bool SharedFunctionInfo::CanDiscardCompiled() const {
#if V8_ENABLE_WEBASSEMBLY
  if (HasAsmWasmData()) return true;
#endif  // V8_ENABLE_WEBASSEMBLY
  return HasBytecodeArray() || HasUncompiledDataWithPreparseData() ||
         HasBaselineCode();
}

bool SharedFunctionInfo::is_class_constructor() const {
  return IsClassConstructorBit::decode(flags(kRelaxedLoad));
}

void SharedFunctionInfo::set_are_properties_final(bool value) {
  if (is_class_constructor()) {
    set_properties_are_final(value);
  }
}

bool SharedFunctionInfo::are_properties_final() const {
  bool bit = properties_are_final();
  return bit && is_class_constructor();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SHARED_FUNCTION_INFO_INL_H_
