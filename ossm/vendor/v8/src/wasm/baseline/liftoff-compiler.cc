// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/baseline/liftoff-compiler.h"

#include "src/base/enum-set.h"
#include "src/base/optional.h"
#include "src/codegen/assembler-inl.h"
// TODO(clemensb): Remove dependences on compiler stuff.
#include "src/codegen/external-reference.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/compiler/linkage.h"
#include "src/compiler/wasm-compiler.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/smi.h"
#include "src/tracing/trace-event.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"
#include "src/wasm/assembler-buffer-cache.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/baseline/liftoff-register.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/object-access.h"
#include "src/wasm/simd-shuffle.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

constexpr auto kRegister = LiftoffAssembler::VarState::kRegister;
constexpr auto kIntConst = LiftoffAssembler::VarState::kIntConst;
constexpr auto kStack = LiftoffAssembler::VarState::kStack;

namespace {

#define __ asm_.

// It's important that we don't modify the LiftoffAssembler's cache state
// in conditionally-executed code paths. Creating these witnesses helps
// enforce that (using DCHECKs in the cache state).
// Conditional jump instructions require a witness to have been created (to
// make sure we don't forget); the witness should stay alive until the label
// is bound where regular control flow resumes. This implies that when we're
// jumping to a trap, the live range of the witness isn't important.
#define FREEZE_STATE(witness_name) FreezeCacheState witness_name(asm_)

#define TRACE(...)                                                \
  do {                                                            \
    if (v8_flags.trace_liftoff) PrintF("[liftoff] " __VA_ARGS__); \
  } while (false)

#define WASM_INSTANCE_OBJECT_FIELD_OFFSET(name) \
  ObjectAccess::ToTagged(WasmInstanceObject::k##name##Offset)

template <int expected_size, int actual_size>
struct assert_field_size {
  static_assert(expected_size == actual_size,
                "field in WasmInstance does not have the expected size");
  static constexpr int size = actual_size;
};

#define WASM_INSTANCE_OBJECT_FIELD_SIZE(name) \
  FIELD_SIZE(WasmInstanceObject::k##name##Offset)

#define LOAD_INSTANCE_FIELD(dst, name, load_size, pinned)                      \
  __ LoadFromInstance(dst, LoadInstanceIntoRegister(pinned, dst),              \
                      WASM_INSTANCE_OBJECT_FIELD_OFFSET(name),                 \
                      assert_field_size<WASM_INSTANCE_OBJECT_FIELD_SIZE(name), \
                                        load_size>::size);

#define LOAD_TAGGED_PTR_INSTANCE_FIELD(dst, name, pinned)                      \
  static_assert(WASM_INSTANCE_OBJECT_FIELD_SIZE(name) == kTaggedSize,          \
                "field in WasmInstance does not have the expected size");      \
  __ LoadTaggedPointerFromInstance(dst, LoadInstanceIntoRegister(pinned, dst), \
                                   WASM_INSTANCE_OBJECT_FIELD_OFFSET(name));

#ifdef V8_CODE_COMMENTS
#define CODE_COMMENT(str)  \
  do {                     \
    __ RecordComment(str); \
  } while (false)
#else
#define CODE_COMMENT(str) ((void)0)
#endif

constexpr LoadType::LoadTypeValue kPointerLoadType =
    kSystemPointerSize == 8 ? LoadType::kI64Load : LoadType::kI32Load;

constexpr ValueKind kPointerKind = LiftoffAssembler::kPointerKind;
constexpr ValueKind kSmiKind = LiftoffAssembler::kSmiKind;
constexpr ValueKind kTaggedKind = LiftoffAssembler::kTaggedKind;

// Used to construct fixed-size signatures: MakeSig::Returns(...).Params(...);
using MakeSig = FixedSizeSignature<ValueKind>;

#if V8_TARGET_ARCH_ARM64
// On ARM64, the Assembler keeps track of pointers to Labels to resolve
// branches to distant targets. Moving labels would confuse the Assembler,
// thus store the label on the heap and keep a unique_ptr.
class MovableLabel {
 public:
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(MovableLabel);
  MovableLabel() : label_(new Label()) {}

  Label* get() { return label_.get(); }

 private:
  std::unique_ptr<Label> label_;
};
#else
// On all other platforms, just store the Label directly.
class MovableLabel {
 public:
  MOVE_ONLY_WITH_DEFAULT_CONSTRUCTORS(MovableLabel);

  Label* get() { return &label_; }

 private:
  Label label_;
};
#endif

compiler::CallDescriptor* GetLoweredCallDescriptor(
    Zone* zone, compiler::CallDescriptor* call_desc) {
  return kSystemPointerSize == 4
             ? compiler::GetI32WasmCallDescriptor(zone, call_desc)
             : call_desc;
}

constexpr LiftoffRegList GetGpParamRegisters() {
  LiftoffRegList registers;
  for (auto reg : kGpParamRegisters) registers.set(reg);
  return registers;
}

constexpr LiftoffCondition GetCompareCondition(WasmOpcode opcode) {
  switch (opcode) {
    case kExprI32Eq:
      return kEqual;
    case kExprI32Ne:
      return kUnequal;
    case kExprI32LtS:
      return kSignedLessThan;
    case kExprI32LtU:
      return kUnsignedLessThan;
    case kExprI32GtS:
      return kSignedGreaterThan;
    case kExprI32GtU:
      return kUnsignedGreaterThan;
    case kExprI32LeS:
      return kSignedLessEqual;
    case kExprI32LeU:
      return kUnsignedLessEqual;
    case kExprI32GeS:
      return kSignedGreaterEqual;
    case kExprI32GeU:
      return kUnsignedGreaterEqual;
    default:
      UNREACHABLE();
  }
}

// Builds a {DebugSideTable}.
class DebugSideTableBuilder {
  using Entry = DebugSideTable::Entry;
  using Value = Entry::Value;

 public:
  enum AssumeSpilling {
    // All register values will be spilled before the pc covered by the debug
    // side table entry. Register slots will be marked as stack slots in the
    // generated debug side table entry.
    kAssumeSpilling,
    // Register slots will be written out as they are.
    kAllowRegisters,
    // Register slots cannot appear since we already spilled.
    kDidSpill
  };

  class EntryBuilder {
   public:
    explicit EntryBuilder(int pc_offset, int stack_height,
                          std::vector<Value> changed_values)
        : pc_offset_(pc_offset),
          stack_height_(stack_height),
          changed_values_(std::move(changed_values)) {}

    Entry ToTableEntry() {
      return Entry{pc_offset_, stack_height_, std::move(changed_values_)};
    }

    void MinimizeBasedOnPreviousStack(const std::vector<Value>& last_values) {
      auto dst = changed_values_.begin();
      auto end = changed_values_.end();
      for (auto src = dst; src != end; ++src) {
        if (src->index < static_cast<int>(last_values.size()) &&
            *src == last_values[src->index]) {
          continue;
        }
        if (dst != src) *dst = *src;
        ++dst;
      }
      changed_values_.erase(dst, end);
    }

    int pc_offset() const { return pc_offset_; }
    void set_pc_offset(int new_pc_offset) { pc_offset_ = new_pc_offset; }

   private:
    int pc_offset_;
    int stack_height_;
    std::vector<Value> changed_values_;
  };

  // Adds a new entry in regular code.
  void NewEntry(int pc_offset,
                base::Vector<DebugSideTable::Entry::Value> values) {
    entries_.emplace_back(pc_offset, static_cast<int>(values.size()),
                          GetChangedStackValues(last_values_, values));
  }

  // Adds a new entry for OOL code, and returns a pointer to a builder for
  // modifying that entry.
  EntryBuilder* NewOOLEntry(base::Vector<DebugSideTable::Entry::Value> values) {
    constexpr int kNoPcOffsetYet = -1;
    ool_entries_.emplace_back(kNoPcOffsetYet, static_cast<int>(values.size()),
                              GetChangedStackValues(last_ool_values_, values));
    return &ool_entries_.back();
  }

  void SetNumLocals(int num_locals) {
    DCHECK_EQ(-1, num_locals_);
    DCHECK_LE(0, num_locals);
    num_locals_ = num_locals;
  }

  std::unique_ptr<DebugSideTable> GenerateDebugSideTable() {
    DCHECK_LE(0, num_locals_);

    // Connect {entries_} and {ool_entries_} by removing redundant stack
    // information from the first {ool_entries_} entry (based on
    // {last_values_}).
    if (!entries_.empty() && !ool_entries_.empty()) {
      ool_entries_.front().MinimizeBasedOnPreviousStack(last_values_);
    }

    std::vector<Entry> entries;
    entries.reserve(entries_.size() + ool_entries_.size());
    for (auto& entry : entries_) entries.push_back(entry.ToTableEntry());
    for (auto& entry : ool_entries_) entries.push_back(entry.ToTableEntry());
    DCHECK(std::is_sorted(
        entries.begin(), entries.end(),
        [](Entry& a, Entry& b) { return a.pc_offset() < b.pc_offset(); }));
    return std::make_unique<DebugSideTable>(num_locals_, std::move(entries));
  }

 private:
  static std::vector<Value> GetChangedStackValues(
      std::vector<Value>& last_values,
      base::Vector<DebugSideTable::Entry::Value> values) {
    std::vector<Value> changed_values;
    int old_stack_size = static_cast<int>(last_values.size());
    last_values.resize(values.size());

    int index = 0;
    for (const auto& value : values) {
      if (index >= old_stack_size || last_values[index] != value) {
        changed_values.push_back(value);
        last_values[index] = value;
      }
      ++index;
    }
    return changed_values;
  }

  int num_locals_ = -1;
  // Keep a snapshot of the stack of the last entry, to generate a delta to the
  // next entry.
  std::vector<Value> last_values_;
  std::vector<EntryBuilder> entries_;
  // Keep OOL code entries separate so we can do proper delta-encoding (more
  // entries might be added between the existing {entries_} and the
  // {ool_entries_}). Store the entries in a list so the pointer is not
  // invalidated by adding more entries.
  std::vector<Value> last_ool_values_;
  std::list<EntryBuilder> ool_entries_;
};

void CheckBailoutAllowed(LiftoffBailoutReason reason, const char* detail,
                         const CompilationEnv* env) {
  // Decode errors are ok.
  if (reason == kDecodeError) return;

  // --liftoff-only ensures that tests actually exercise the Liftoff path
  // without bailing out. We also fail for missing CPU support, to avoid
  // running any TurboFan code under --liftoff-only.
  if (v8_flags.liftoff_only) {
    FATAL("--liftoff-only: treating bailout as fatal error. Cause: %s", detail);
  }

  // Missing CPU features are generally OK, except with --liftoff-only.
  if (reason == kMissingCPUFeature) return;

  // If --enable-testing-opcode-in-wasm is set, we are expected to bailout with
  // "testing opcode".
  if (v8_flags.enable_testing_opcode_in_wasm &&
      strcmp(detail, "testing opcode") == 0) {
    return;
  }

  // Some externally maintained architectures don't fully implement Liftoff yet.
#if V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_S390X || V8_TARGET_ARCH_PPC || \
    V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_LOONG64
  return;
#endif

#if V8_TARGET_ARCH_ARM
  // Allow bailout for missing ARMv7 support.
  if (!CpuFeatures::IsSupported(ARMv7) && reason == kUnsupportedArchitecture) {
    return;
  }
#endif

#define LIST_FEATURE(name, ...) kFeature_##name,
  constexpr WasmFeatures kExperimentalFeatures{
      FOREACH_WASM_EXPERIMENTAL_FEATURE_FLAG(LIST_FEATURE)};
#undef LIST_FEATURE

  // Bailout is allowed if any experimental feature is enabled.
  if (env->enabled_features.contains_any(kExperimentalFeatures)) return;

  // Otherwise, bailout is not allowed.
  FATAL("Liftoff bailout should not happen. Cause: %s\n", detail);
}

class LiftoffCompiler {
 public:
  // TODO(clemensb): Make this a template parameter.
  static constexpr Decoder::ValidateFlag validate = Decoder::kBooleanValidation;

  using Value = ValueBase<validate>;

  struct ElseState {
    MovableLabel label;
    LiftoffAssembler::CacheState state;
  };

  struct TryInfo {
    TryInfo() = default;
    LiftoffAssembler::CacheState catch_state;
    Label catch_label;
    bool catch_reached = false;
    bool in_handler = false;
  };

  struct Control : public ControlBase<Value, validate> {
    std::unique_ptr<ElseState> else_state;
    LiftoffAssembler::CacheState label_state;
    MovableLabel label;
    std::unique_ptr<TryInfo> try_info;
    // Number of exceptions on the stack below this control.
    int num_exceptions = 0;

    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(Control);

    template <typename... Args>
    explicit Control(Args&&... args) V8_NOEXCEPT
        : ControlBase(std::forward<Args>(args)...) {}
  };

  using FullDecoder = WasmFullDecoder<validate, LiftoffCompiler>;
  using ValueKindSig = LiftoffAssembler::ValueKindSig;

  class MostlySmallValueKindSig : public Signature<ValueKind> {
   public:
    MostlySmallValueKindSig(Zone* zone, const FunctionSig* sig)
        : Signature<ValueKind>(sig->return_count(), sig->parameter_count(),
                               MakeKinds(inline_storage_, zone, sig)) {}

   private:
    static constexpr size_t kInlineStorage = 8;

    static ValueKind* MakeKinds(ValueKind* storage, Zone* zone,
                                const FunctionSig* sig) {
      const size_t size = sig->parameter_count() + sig->return_count();
      if (V8_UNLIKELY(size > kInlineStorage)) {
        storage = zone->NewArray<ValueKind>(size);
      }
      std::transform(sig->all().begin(), sig->all().end(), storage,
                     [](ValueType type) { return type.kind(); });
      return storage;
    }

    ValueKind inline_storage_[kInlineStorage];
  };

  // For debugging, we need to spill registers before a trap or a stack check to
  // be able to inspect them.
  struct SpilledRegistersForInspection : public ZoneObject {
    struct Entry {
      int offset;
      LiftoffRegister reg;
      ValueKind kind;
    };
    ZoneVector<Entry> entries;

    explicit SpilledRegistersForInspection(Zone* zone) : entries(zone) {}
  };

  struct OutOfLineSafepointInfo {
    ZoneVector<int> slots;
    LiftoffRegList spills;

    explicit OutOfLineSafepointInfo(Zone* zone) : slots(zone) {}
  };

  struct OutOfLineCode {
    MovableLabel label;
    MovableLabel continuation;
    WasmCode::RuntimeStubId stub;
    WasmCodePosition position;
    LiftoffRegList regs_to_save;
    Register cached_instance;
    OutOfLineSafepointInfo* safepoint_info;
    uint32_t pc;  // for trap handler.
    // These two pointers will only be used for debug code:
    SpilledRegistersForInspection* spilled_registers;
    DebugSideTableBuilder::EntryBuilder* debug_sidetable_entry_builder;

    // Named constructors:
    static OutOfLineCode Trap(
        WasmCode::RuntimeStubId s, WasmCodePosition pos,
        SpilledRegistersForInspection* spilled_registers,
        OutOfLineSafepointInfo* safepoint_info, uint32_t pc,
        DebugSideTableBuilder::EntryBuilder* debug_sidetable_entry_builder) {
      DCHECK_LT(0, pos);
      return {
          {},                            // label
          {},                            // continuation
          s,                             // stub
          pos,                           // position
          {},                            // regs_to_save
          no_reg,                        // cached_instance
          safepoint_info,                // safepoint_info
          pc,                            // pc
          spilled_registers,             // spilled_registers
          debug_sidetable_entry_builder  // debug_side_table_entry_builder
      };
    }
    static OutOfLineCode StackCheck(
        WasmCodePosition pos, LiftoffRegList regs_to_save,
        Register cached_instance, SpilledRegistersForInspection* spilled_regs,
        OutOfLineSafepointInfo* safepoint_info,
        DebugSideTableBuilder::EntryBuilder* debug_sidetable_entry_builder) {
      return {
          {},                            // label
          {},                            // continuation
          WasmCode::kWasmStackGuard,     // stub
          pos,                           // position
          regs_to_save,                  // regs_to_save
          cached_instance,               // cached_instance
          safepoint_info,                // safepoint_info
          0,                             // pc
          spilled_regs,                  // spilled_registers
          debug_sidetable_entry_builder  // debug_side_table_entry_builder
      };
    }
    static OutOfLineCode TierupCheck(
        WasmCodePosition pos, LiftoffRegList regs_to_save,
        Register cached_instance, SpilledRegistersForInspection* spilled_regs,
        OutOfLineSafepointInfo* safepoint_info,
        DebugSideTableBuilder::EntryBuilder* debug_sidetable_entry_builder) {
      return {
          {},                            // label
          {},                            // continuation,
          WasmCode::kWasmTriggerTierUp,  // stub
          pos,                           // position
          regs_to_save,                  // regs_to_save
          cached_instance,               // cached_instance
          safepoint_info,                // safepoint_info
          0,                             // pc
          spilled_regs,                  // spilled_registers
          debug_sidetable_entry_builder  // debug_side_table_entry_builder
      };
    }
  };

  LiftoffCompiler(compiler::CallDescriptor* call_descriptor,
                  CompilationEnv* env, Zone* compilation_zone,
                  std::unique_ptr<AssemblerBuffer> buffer,
                  DebugSideTableBuilder* debug_sidetable_builder,
                  const LiftoffOptions& options)
      : asm_(std::move(buffer)),
        descriptor_(
            GetLoweredCallDescriptor(compilation_zone, call_descriptor)),
        env_(env),
        debug_sidetable_builder_(debug_sidetable_builder),
        for_debugging_(options.for_debugging),
        func_index_(options.func_index),
        out_of_line_code_(compilation_zone),
        source_position_table_builder_(compilation_zone),
        protected_instructions_(compilation_zone),
        compilation_zone_(compilation_zone),
        safepoint_table_builder_(compilation_zone_),
        next_breakpoint_ptr_(options.breakpoints.begin()),
        next_breakpoint_end_(options.breakpoints.end()),
        dead_breakpoint_(options.dead_breakpoint),
        handlers_(compilation_zone),
        max_steps_(options.max_steps),
        nondeterminism_(options.nondeterminism) {
    // We often see huge numbers of traps per function, so pre-reserve some
    // space in that vector. 128 entries is enough for ~94% of functions on
    // modern modules, as of 2022-06-03.
    out_of_line_code_.reserve(128);

    DCHECK(options.is_initialized());
    // If there are no breakpoints, both pointers should be nullptr.
    DCHECK_IMPLIES(
        next_breakpoint_ptr_ == next_breakpoint_end_,
        next_breakpoint_ptr_ == nullptr && next_breakpoint_end_ == nullptr);
  }

  bool did_bailout() const { return bailout_reason_ != kSuccess; }
  LiftoffBailoutReason bailout_reason() const { return bailout_reason_; }

  void GetCode(CodeDesc* desc) {
    asm_.GetCode(nullptr, desc, &safepoint_table_builder_,
                 handler_table_offset_);
  }

  std::unique_ptr<AssemblerBuffer> ReleaseBuffer() {
    return asm_.ReleaseBuffer();
  }

  base::OwnedVector<uint8_t> GetSourcePositionTable() {
    return source_position_table_builder_.ToSourcePositionTableVector();
  }

  base::OwnedVector<uint8_t> GetProtectedInstructionsData() const {
    return base::OwnedVector<uint8_t>::Of(base::Vector<const uint8_t>::cast(
        base::VectorOf(protected_instructions_)));
  }

  uint32_t GetTotalFrameSlotCountForGC() const {
    return __ GetTotalFrameSlotCountForGC();
  }

  int GetFeedbackVectorSlots() const {
    // The number of call instructions is capped by max function size.
    static_assert(kV8MaxWasmFunctionSize < std::numeric_limits<int>::max() / 2);
    return static_cast<int>(encountered_call_instructions_.size()) * 2;
  }

  void unsupported(FullDecoder* decoder, LiftoffBailoutReason reason,
                   const char* detail) {
    DCHECK_NE(kSuccess, reason);
    if (did_bailout()) return;
    bailout_reason_ = reason;
    TRACE("unsupported: %s\n", detail);
    decoder->errorf(decoder->pc_offset(), "unsupported liftoff operation: %s",
                    detail);
    UnuseLabels(decoder);
    CheckBailoutAllowed(reason, detail, env_);
  }

  bool DidAssemblerBailout(FullDecoder* decoder) {
    if (decoder->failed() || !__ did_bailout()) return false;
    unsupported(decoder, __ bailout_reason(), __ bailout_detail());
    return true;
  }

  V8_INLINE bool CheckSupportedType(FullDecoder* decoder, ValueKind kind,
                                    const char* context) {
    if (V8_LIKELY(supported_types_.contains(kind))) return true;
    return MaybeBailoutForUnsupportedType(decoder, kind, context);
  }

  V8_NOINLINE bool MaybeBailoutForUnsupportedType(FullDecoder* decoder,
                                                  ValueKind kind,
                                                  const char* context) {
    DCHECK(!supported_types_.contains(kind));

    // Lazily update {supported_types_}; then check again.
    if (CpuFeatures::SupportsWasmSimd128()) supported_types_.Add(kS128);
    if (supported_types_.contains(kind)) return true;

    LiftoffBailoutReason bailout_reason;
    switch (kind) {
      case kS128:
        bailout_reason = kSimd;
        break;
      case kRef:
      case kRefNull:
      case kRtt:
      case kI8:
      case kI16:
        bailout_reason = kGC;
        break;
      default:
        UNREACHABLE();
    }
    base::EmbeddedVector<char, 128> buffer;
    SNPrintF(buffer, "%s %s", name(kind), context);
    unsupported(decoder, bailout_reason, buffer.begin());
    return false;
  }

  void UnuseLabels(FullDecoder* decoder) {
#ifdef DEBUG
    auto Unuse = [](Label* label) {
      label->Unuse();
      label->UnuseNear();
    };
    // Unuse all labels now, otherwise their destructor will fire a DCHECK error
    // if they where referenced before.
    uint32_t control_depth = decoder ? decoder->control_depth() : 0;
    for (uint32_t i = 0; i < control_depth; ++i) {
      Control* c = decoder->control_at(i);
      Unuse(c->label.get());
      if (c->else_state) Unuse(c->else_state->label.get());
      if (c->try_info != nullptr) Unuse(&c->try_info->catch_label);
    }
    for (auto& ool : out_of_line_code_) Unuse(ool.label.get());
#endif
  }

  void StartFunction(FullDecoder* decoder) {
    if (v8_flags.trace_liftoff && !v8_flags.trace_wasm_decoder) {
      StdoutStream{} << "hint: add --trace-wasm-decoder to also see the wasm "
                        "instructions being decoded\n";
    }
    int num_locals = decoder->num_locals();
    __ set_num_locals(num_locals);
    for (int i = 0; i < num_locals; ++i) {
      ValueKind kind = decoder->local_type(i).kind();
      __ set_local_kind(i, kind);
    }
  }

  class ParameterProcessor {
   public:
    ParameterProcessor(LiftoffCompiler* compiler, uint32_t num_params)
        : compiler_(compiler), num_params_(num_params) {}

    void Process() {
      // First pass: collect parameter registers.
      while (NextParam()) {
        MaybeCollectRegister();
        if (needs_gp_pair_) {
          NextLocation();
          MaybeCollectRegister();
        }
      }
      // Second pass: allocate parameters.
      param_idx_ = 0;
      input_idx_ = kFirstInputIdx;
      while (NextParam()) {
        LiftoffRegister reg = LoadToReg(param_regs_);
        if (needs_gp_pair_) {
          NextLocation();
          LiftoffRegister reg2 = LoadToReg(param_regs_ | LiftoffRegList{reg});
          reg = LiftoffRegister::ForPair(reg.gp(), reg2.gp());
        }
        compiler_->asm_.PushRegister(kind_, reg);
      }
    }

   private:
    bool NextParam() {
      if (param_idx_ >= num_params_) {
        DCHECK_EQ(input_idx_, compiler_->descriptor_->InputCount());
        return false;
      }
      kind_ = compiler_->asm_.local_kind(param_idx_++);
      needs_gp_pair_ = needs_gp_reg_pair(kind_);
      reg_kind_ = needs_gp_pair_ ? kI32 : kind_;
      rc_ = reg_class_for(reg_kind_);
      NextLocation();
      return true;
    }

    void NextLocation() {
      location_ = compiler_->descriptor_->GetInputLocation(input_idx_++);
    }

    LiftoffRegister CurrentRegister() {
      DCHECK(!location_.IsAnyRegister());
      return LiftoffRegister::from_external_code(rc_, reg_kind_,
                                                 location_.AsRegister());
    }

    void MaybeCollectRegister() {
      if (!location_.IsRegister()) return;
      DCHECK(!param_regs_.has(CurrentRegister()));
      param_regs_.set(CurrentRegister());
    }

    LiftoffRegister LoadToReg(LiftoffRegList pinned) {
      if (location_.IsRegister()) {
        LiftoffRegister reg = CurrentRegister();
        DCHECK(compiler_->asm_.cache_state()->is_free(reg));
        // Unpin the register, to avoid depending on the set of allocatable
        // registers being larger than the set of parameter registers.
        param_regs_.clear(reg);
        return reg;
      }
      DCHECK(location_.IsCallerFrameSlot());
      LiftoffRegister reg = compiler_->asm_.GetUnusedRegister(rc_, pinned);
      compiler_->asm_.LoadCallerFrameSlot(reg, -location_.AsCallerFrameSlot(),
                                          reg_kind_);
      return reg;
    }

    // Input 0 is the code target, 1 is the instance.
    static constexpr uint32_t kFirstInputIdx = 2;

    LiftoffCompiler* compiler_;
    const uint32_t num_params_;
    uint32_t param_idx_{0};
    uint32_t input_idx_{kFirstInputIdx};
    ValueKind kind_;
    bool needs_gp_pair_;
    ValueKind reg_kind_;
    RegClass rc_;
    compiler::LinkageLocation location_{
        compiler::LinkageLocation::ForAnyRegister()};
    LiftoffRegList param_regs_;
  };

  void StackCheck(FullDecoder* decoder, WasmCodePosition position) {
    CODE_COMMENT("stack check");
    if (!v8_flags.wasm_stack_checks || !env_->runtime_exception_support) return;

    // Loading the limit address can change the stack state, hence do this
    // before storing information about registers.
    Register limit_address = __ GetUnusedRegister(kGpReg, {}).gp();
    LOAD_INSTANCE_FIELD(limit_address, StackLimitAddress, kSystemPointerSize,
                        {});

    LiftoffRegList regs_to_save = __ cache_state()->used_registers;
    // The cached instance will be reloaded separately.
    if (__ cache_state()->cached_instance != no_reg) {
      DCHECK(regs_to_save.has(__ cache_state()->cached_instance));
      regs_to_save.clear(__ cache_state()->cached_instance);
    }
    SpilledRegistersForInspection* spilled_regs = nullptr;

    OutOfLineSafepointInfo* safepoint_info =
        compilation_zone_->New<OutOfLineSafepointInfo>(compilation_zone_);
    __ cache_state()->GetTaggedSlotsForOOLCode(
        &safepoint_info->slots, &safepoint_info->spills,
        for_debugging_
            ? LiftoffAssembler::CacheState::SpillLocation::kStackSlots
            : LiftoffAssembler::CacheState::SpillLocation::kTopOfStack);
    if (V8_UNLIKELY(for_debugging_)) {
      // When debugging, we do not just push all registers to the stack, but we
      // spill them to their proper stack locations such that we can inspect
      // them.
      // The only exception is the cached memory start, which we just push
      // before the stack check and pop afterwards.
      regs_to_save = {};
      if (__ cache_state()->cached_mem_start != no_reg) {
        regs_to_save.set(__ cache_state()->cached_mem_start);
      }
      spilled_regs = GetSpilledRegistersForInspection();
    }
    out_of_line_code_.push_back(OutOfLineCode::StackCheck(
        position, regs_to_save, __ cache_state()->cached_instance, spilled_regs,
        safepoint_info, RegisterOOLDebugSideTableEntry(decoder)));
    OutOfLineCode& ool = out_of_line_code_.back();
    __ StackCheck(ool.label.get(), limit_address);
    __ bind(ool.continuation.get());
  }

  void TierupCheck(FullDecoder* decoder, WasmCodePosition position,
                   int budget_used, Register tmp1, Register tmp2) {
    // We should always decrement the budget, and we don't expect integer
    // overflows in the budget calculation.
    DCHECK_LE(1, budget_used);

    if (for_debugging_ != kNoDebugging) return;
    CODE_COMMENT("tierup check");
    // We never want to blow the entire budget at once.
    const int kMax = v8_flags.wasm_tiering_budget / 4;
    if (budget_used > kMax) budget_used = kMax;

    LiftoffRegister budget_reg(tmp2);
    // Be careful not to cause caching of the instance.
    Register instance = __ cache_state()->cached_instance;
    if (instance == no_reg) {
      instance = tmp1;
      __ LoadInstanceFromFrame(instance);
    }
    constexpr int kArraySize = kSystemPointerSize;
    constexpr int kArrayOffset =
        WASM_INSTANCE_OBJECT_FIELD_OFFSET(TieringBudgetArray);
    static_assert(WASM_INSTANCE_OBJECT_FIELD_SIZE(TieringBudgetArray) ==
                  kArraySize);
    Register array_reg = tmp1;  // Overwriting {instance}.
    __ LoadFromInstance(array_reg, instance, kArrayOffset, kArraySize);
    uint32_t offset =
        kInt32Size * declared_function_index(env_->module, func_index_);
#if V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_ARM64
    // Platforms where both this load and the later store would have to
    // explicitly add the offset can save code size by performing the addition
    // only once.
    __ emit_ptrsize_addi(array_reg, array_reg, offset);
    offset = 0;
#endif
    __ Load(budget_reg, array_reg, no_reg, offset, LoadType::kI32Load, {});
    LiftoffRegList regs_to_save = __ cache_state()->used_registers;
    // The cached instance will be reloaded separately.
    if (__ cache_state()->cached_instance != no_reg) {
      DCHECK(regs_to_save.has(__ cache_state()->cached_instance));
      regs_to_save.clear(__ cache_state()->cached_instance);
    }
    SpilledRegistersForInspection* spilled_regs = nullptr;

    OutOfLineSafepointInfo* safepoint_info =
        compilation_zone_->New<OutOfLineSafepointInfo>(compilation_zone_);
    __ cache_state()->GetTaggedSlotsForOOLCode(
        &safepoint_info->slots, &safepoint_info->spills,
        LiftoffAssembler::CacheState::SpillLocation::kTopOfStack);
    out_of_line_code_.push_back(OutOfLineCode::TierupCheck(
        position, regs_to_save, __ cache_state()->cached_instance, spilled_regs,
        safepoint_info, RegisterOOLDebugSideTableEntry(decoder)));
    OutOfLineCode& ool = out_of_line_code_.back();
    FREEZE_STATE(trapping);
    __ emit_i32_subi_jump_negative(budget_reg.gp(), budget_used,
                                   ool.label.get(), trapping);
    __ Store(array_reg, no_reg, offset, budget_reg, StoreType::kI32Store, {});
    __ bind(ool.continuation.get());
  }

  bool SpillLocalsInitially(FullDecoder* decoder, uint32_t num_params) {
    int actual_locals = __ num_locals() - num_params;
    DCHECK_LE(0, actual_locals);
    constexpr int kNumCacheRegisters = kLiftoffAssemblerGpCacheRegs.Count();
    // If we have many locals, we put them on the stack initially. This avoids
    // having to spill them on merge points. Use of these initial values should
    // be rare anyway.
    if (actual_locals > kNumCacheRegisters / 2) return true;
    // If there are locals which are not i32 or i64, we also spill all locals,
    // because other types cannot be initialized to constants.
    for (uint32_t param_idx = num_params; param_idx < __ num_locals();
         ++param_idx) {
      ValueKind kind = __ local_kind(param_idx);
      if (kind != kI32 && kind != kI64) return true;
    }
    return false;
  }

  void TraceFunctionEntry(FullDecoder* decoder) {
    CODE_COMMENT("trace function entry");
    __ SpillAllRegisters();
    source_position_table_builder_.AddPosition(
        __ pc_offset(), SourcePosition(decoder->position()), false);
    __ CallRuntimeStub(WasmCode::kWasmTraceEnter);
    DefineSafepoint();
  }

  bool dynamic_tiering() {
    return env_->dynamic_tiering && for_debugging_ == kNoDebugging &&
           (v8_flags.wasm_tier_up_filter == -1 ||
            v8_flags.wasm_tier_up_filter == func_index_);
  }

  void StartFunctionBody(FullDecoder* decoder, Control* block) {
    for (uint32_t i = 0; i < __ num_locals(); ++i) {
      if (!CheckSupportedType(decoder, __ local_kind(i), "param")) return;
    }

    // Parameter 0 is the instance parameter.
    uint32_t num_params =
        static_cast<uint32_t>(decoder->sig_->parameter_count());

    __ CodeEntry();

    __ EnterFrame(StackFrame::WASM);
    __ set_has_frame(true);
    pc_offset_stack_frame_construction_ = __ PrepareStackFrame();
    // {PrepareStackFrame} is the first platform-specific assembler method.
    // If this failed, we can bail out immediately, avoiding runtime overhead
    // and potential failures because of other unimplemented methods.
    // A platform implementing {PrepareStackFrame} must ensure that we can
    // finish compilation without errors even if we hit unimplemented
    // LiftoffAssembler methods.
    if (DidAssemblerBailout(decoder)) return;

    // Input 0 is the call target, the instance is at 1.
    constexpr int kInstanceParameterIndex = 1;
    // Check that {kWasmInstanceRegister} matches our call descriptor.
    DCHECK_EQ(kWasmInstanceRegister,
              Register::from_code(
                  descriptor_->GetInputLocation(kInstanceParameterIndex)
                      .AsRegister()));
    USE(kInstanceParameterIndex);
    __ cache_state()->SetInstanceCacheRegister(kWasmInstanceRegister);
    // Load the feedback vector and cache it in a stack slot.
    constexpr LiftoffRegList kGpParamRegisters = GetGpParamRegisters();
    if (v8_flags.wasm_speculative_inlining) {
      CODE_COMMENT("load feedback vector");
      int declared_func_index =
          func_index_ - env_->module->num_imported_functions;
      DCHECK_GE(declared_func_index, 0);
      LiftoffRegList pinned = kGpParamRegisters;
      LiftoffRegister tmp = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      __ LoadTaggedPointerFromInstance(
          tmp.gp(), kWasmInstanceRegister,
          WASM_INSTANCE_OBJECT_FIELD_OFFSET(FeedbackVectors));
      __ LoadTaggedPointer(tmp.gp(), tmp.gp(), no_reg,
                           wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(
                               declared_func_index));
      __ Spill(liftoff::kFeedbackVectorOffset, tmp, kPointerKind);
    }
    if (for_debugging_) __ ResetOSRTarget();

    if (num_params) {
      CODE_COMMENT("process parameters");
      ParameterProcessor processor(this, num_params);
      processor.Process();
    }
    int params_size = __ TopSpillOffset();

    // Initialize locals beyond parameters.
    if (num_params < __ num_locals()) CODE_COMMENT("init locals");
    if (SpillLocalsInitially(decoder, num_params)) {
      bool has_refs = false;
      for (uint32_t param_idx = num_params; param_idx < __ num_locals();
           ++param_idx) {
        ValueKind kind = __ local_kind(param_idx);
        has_refs |= is_reference(kind);
        __ PushStack(kind);
      }
      int spill_size = __ TopSpillOffset() - params_size;
      __ FillStackSlotsWithZero(params_size, spill_size);

      // Initialize all reference type locals with ref.null.
      if (has_refs) {
        Register null_ref_reg = __ GetUnusedRegister(kGpReg, {}).gp();
        LoadNullValue(null_ref_reg, {});
        for (uint32_t local_index = num_params; local_index < __ num_locals();
             ++local_index) {
          ValueKind kind = __ local_kind(local_index);
          if (is_reference(kind)) {
            __ Spill(__ cache_state()->stack_state[local_index].offset(),
                     LiftoffRegister(null_ref_reg), kind);
          }
        }
      }
    } else {
      for (uint32_t param_idx = num_params; param_idx < __ num_locals();
           ++param_idx) {
        ValueKind kind = __ local_kind(param_idx);
        // Anything which is not i32 or i64 requires spilling.
        DCHECK(kind == kI32 || kind == kI64);
        __ PushConstant(kind, int32_t{0});
      }
    }

    DCHECK_EQ(__ num_locals(), __ cache_state()->stack_height());

    if (V8_UNLIKELY(debug_sidetable_builder_)) {
      debug_sidetable_builder_->SetNumLocals(__ num_locals());
    }

    // The function-prologue stack check is associated with position 0, which
    // is never a position of any instruction in the function.
    StackCheck(decoder, 0);

    if (v8_flags.trace_wasm) TraceFunctionEntry(decoder);
  }

  void GenerateOutOfLineCode(OutOfLineCode* ool) {
    CODE_COMMENT(
        (std::string("OOL: ") + GetRuntimeStubName(ool->stub)).c_str());
    __ bind(ool->label.get());
    const bool is_stack_check = ool->stub == WasmCode::kWasmStackGuard;
    const bool is_tierup = ool->stub == WasmCode::kWasmTriggerTierUp;

    // Only memory OOB traps need a {pc}, but not unconditionally. Static OOB
    // accesses do not need protected instruction information, hence they also
    // do not set {pc}.
    DCHECK_IMPLIES(ool->stub != WasmCode::kThrowWasmTrapMemOutOfBounds,
                   ool->pc == 0);

    if (env_->bounds_checks == kTrapHandler && ool->pc != 0) {
      uint32_t pc = static_cast<uint32_t>(__ pc_offset());
      DCHECK_EQ(pc, __ pc_offset());
      protected_instructions_.emplace_back(
          trap_handler::ProtectedInstructionData{ool->pc, pc});
    }

    if (!env_->runtime_exception_support) {
      // We cannot test calls to the runtime in cctest/test-run-wasm.
      // Therefore we emit a call to C here instead of a call to the runtime.
      // In this mode, we never generate stack checks.
      DCHECK(!is_stack_check);
      __ CallTrapCallbackForTesting();
      __ LeaveFrame(StackFrame::WASM);
      __ DropStackSlotsAndRet(
          static_cast<uint32_t>(descriptor_->ParameterSlotCount()));
      return;
    }

    if (!ool->regs_to_save.is_empty()) {
      __ PushRegisters(ool->regs_to_save);
    }
    if (V8_UNLIKELY(ool->spilled_registers != nullptr)) {
      for (auto& entry : ool->spilled_registers->entries) {
        // We should not push and spill the same register.
        DCHECK(!ool->regs_to_save.has(entry.reg));
        __ Spill(entry.offset, entry.reg, entry.kind);
      }
    }

    source_position_table_builder_.AddPosition(
        __ pc_offset(), SourcePosition(ool->position), true);
    __ CallRuntimeStub(ool->stub);
    auto safepoint = safepoint_table_builder_.DefineSafepoint(&asm_);

    if (ool->safepoint_info) {
      for (auto index : ool->safepoint_info->slots) {
        safepoint.DefineTaggedStackSlot(index);
      }

      int total_frame_size = __ GetTotalFrameSize();
      LiftoffRegList gp_regs = ool->regs_to_save & kGpCacheRegList;
      // {total_frame_size} is the highest offset from the FP that is used to
      // store a value. The offset of the first spill slot should therefore be
      // {(total_frame_size / kSystemPointerSize) + 1}. However, spill slots
      // don't start at offset '0' but at offset '-1' (or
      // {-kSystemPointerSize}). Therefore we have to add another '+ 1' to the
      // index of the first spill slot.
      int index = (total_frame_size / kSystemPointerSize) + 2;

      __ RecordSpillsInSafepoint(safepoint, gp_regs,
                                 ool->safepoint_info->spills, index);
    }

    DCHECK_EQ(!debug_sidetable_builder_, !ool->debug_sidetable_entry_builder);
    if (V8_UNLIKELY(ool->debug_sidetable_entry_builder)) {
      ool->debug_sidetable_entry_builder->set_pc_offset(__ pc_offset());
    }
    DCHECK_EQ(ool->continuation.get()->is_bound(), is_stack_check || is_tierup);
    if (is_stack_check) {
      MaybeOSR();
    }
    if (!ool->regs_to_save.is_empty()) __ PopRegisters(ool->regs_to_save);
    if (is_stack_check || is_tierup) {
      if (V8_UNLIKELY(ool->spilled_registers != nullptr)) {
        DCHECK(for_debugging_);
        for (auto& entry : ool->spilled_registers->entries) {
          __ Fill(entry.reg, entry.offset, entry.kind);
        }
      }
      if (ool->cached_instance != no_reg) {
        __ LoadInstanceFromFrame(ool->cached_instance);
      }
      __ emit_jump(ool->continuation.get());
    } else {
      __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
    }
  }

  void FinishFunction(FullDecoder* decoder) {
    if (DidAssemblerBailout(decoder)) return;
    __ AlignFrameSize();
#if DEBUG
    int frame_size = __ GetTotalFrameSize();
#endif
    for (OutOfLineCode& ool : out_of_line_code_) {
      GenerateOutOfLineCode(&ool);
    }
    DCHECK_EQ(frame_size, __ GetTotalFrameSize());
    __ PatchPrepareStackFrame(pc_offset_stack_frame_construction_,
                              &safepoint_table_builder_);
    __ FinishCode();
    safepoint_table_builder_.Emit(&asm_, __ GetTotalFrameSlotCountForGC());
    // Emit the handler table.
    if (!handlers_.empty()) {
      handler_table_offset_ = HandlerTable::EmitReturnTableStart(&asm_);
      for (auto& handler : handlers_) {
        HandlerTable::EmitReturnEntry(&asm_, handler.pc_offset,
                                      handler.handler.get()->pos());
      }
    }
    __ MaybeEmitOutOfLineConstantPool();
    // The previous calls may have also generated a bailout.
    DidAssemblerBailout(decoder);
    DCHECK_EQ(num_exceptions_, 0);

    if (v8_flags.wasm_speculative_inlining &&
        !encountered_call_instructions_.empty()) {
      // Update the call targets stored in the WasmModule.
      TypeFeedbackStorage& type_feedback = env_->module->type_feedback;
      base::MutexGuard mutex_guard(&type_feedback.mutex);
      base::OwnedVector<uint32_t>& call_targets =
          type_feedback.feedback_for_function[func_index_].call_targets;
      if (call_targets.empty()) {
        call_targets =
            base::OwnedVector<uint32_t>::Of(encountered_call_instructions_);
      } else {
        DCHECK_EQ(call_targets.as_vector(),
                  base::VectorOf(encountered_call_instructions_));
      }
    }
  }

  void OnFirstError(FullDecoder* decoder) {
    if (!did_bailout()) bailout_reason_ = kDecodeError;
    UnuseLabels(decoder);
    asm_.AbortCompilation();
  }

  V8_NOINLINE void EmitDebuggingInfo(FullDecoder* decoder, WasmOpcode opcode) {
    DCHECK(for_debugging_);
    if (!WasmOpcodes::IsBreakable(opcode)) return;
    bool has_breakpoint = false;
    if (next_breakpoint_ptr_) {
      if (*next_breakpoint_ptr_ == 0) {
        // A single breakpoint at offset 0 indicates stepping.
        DCHECK_EQ(next_breakpoint_ptr_ + 1, next_breakpoint_end_);
        has_breakpoint = true;
      } else {
        while (next_breakpoint_ptr_ != next_breakpoint_end_ &&
               *next_breakpoint_ptr_ < decoder->position()) {
          // Skip unreachable breakpoints.
          ++next_breakpoint_ptr_;
        }
        if (next_breakpoint_ptr_ == next_breakpoint_end_) {
          next_breakpoint_ptr_ = next_breakpoint_end_ = nullptr;
        } else if (*next_breakpoint_ptr_ == decoder->position()) {
          has_breakpoint = true;
        }
      }
    }
    if (has_breakpoint) {
      CODE_COMMENT("breakpoint");
      EmitBreakpoint(decoder);
      // Once we emitted an unconditional breakpoint, we don't need to check
      // function entry breaks any more.
      did_function_entry_break_checks_ = true;
    } else if (!did_function_entry_break_checks_) {
      did_function_entry_break_checks_ = true;
      CODE_COMMENT("check function entry break");
      Label do_break;
      Label no_break;
      Register flag = __ GetUnusedRegister(kGpReg, {}).gp();

      // Check the "hook on function call" flag. If set, trigger a break.
      LOAD_INSTANCE_FIELD(flag, HookOnFunctionCallAddress, kSystemPointerSize,
                          {});
      FREEZE_STATE(frozen);
      __ Load(LiftoffRegister{flag}, flag, no_reg, 0, LoadType::kI32Load8U, {});
      __ emit_cond_jump(kNotEqualZero, &do_break, kI32, flag, no_reg, frozen);

      // Check if we should stop on "script entry".
      LOAD_INSTANCE_FIELD(flag, BreakOnEntry, kUInt8Size, {});
      __ emit_cond_jump(kEqualZero, &no_break, kI32, flag, no_reg, frozen);

      __ bind(&do_break);
      EmitBreakpoint(decoder);
      __ bind(&no_break);
    } else if (dead_breakpoint_ == decoder->position()) {
      DCHECK(!next_breakpoint_ptr_ ||
             *next_breakpoint_ptr_ != dead_breakpoint_);
      // The top frame is paused at this position, but the breakpoint was
      // removed. Adding a dead breakpoint here ensures that the source
      // position exists, and that the offset to the return address is the
      // same as in the old code.
      CODE_COMMENT("dead breakpoint");
      Label cont;
      __ emit_jump(&cont);
      EmitBreakpoint(decoder);
      __ bind(&cont);
    }
    if (V8_UNLIKELY(max_steps_ != nullptr)) {
      CODE_COMMENT("check max steps");
      LiftoffRegList pinned;
      LiftoffRegister max_steps = __ GetUnusedRegister(kGpReg, {});
      pinned.set(max_steps);
      LiftoffRegister max_steps_addr = __ GetUnusedRegister(kGpReg, pinned);
      pinned.set(max_steps_addr);
      {
        FREEZE_STATE(frozen);
        __ LoadConstant(
            max_steps_addr,
            WasmValue::ForUintPtr(reinterpret_cast<uintptr_t>(max_steps_)));
        __ Load(max_steps, max_steps_addr.gp(), no_reg, 0, LoadType::kI32Load);
        Label cont;
        __ emit_i32_cond_jumpi(kUnequal, &cont, max_steps.gp(), 0, frozen);
        // Abort.
        Trap(decoder, kTrapUnreachable);
        __ bind(&cont);
      }
      __ emit_i32_subi(max_steps.gp(), max_steps.gp(), 1);
      __ Store(max_steps_addr.gp(), no_reg, 0, max_steps, StoreType::kI32Store,
               pinned);
    }
  }

  void NextInstruction(FullDecoder* decoder, WasmOpcode opcode) {
    // Add a single check, so that the fast path can be inlined while
    // {EmitDebuggingInfo} stays outlined.
    if (V8_UNLIKELY(for_debugging_)) EmitDebuggingInfo(decoder, opcode);
    TraceCacheState(decoder);
    SLOW_DCHECK(__ ValidateCacheState());
    CODE_COMMENT(WasmOpcodes::OpcodeName(
        WasmOpcodes::IsPrefixOpcode(opcode)
            ? decoder->read_prefixed_opcode<Decoder::kFullValidation>(
                  decoder->pc())
            : opcode));
  }

  void EmitBreakpoint(FullDecoder* decoder) {
    DCHECK(for_debugging_);
    source_position_table_builder_.AddPosition(
        __ pc_offset(), SourcePosition(decoder->position()), true);
    __ CallRuntimeStub(WasmCode::kWasmDebugBreak);
    DefineSafepointWithCalleeSavedRegisters();
    RegisterDebugSideTableEntry(decoder,
                                DebugSideTableBuilder::kAllowRegisters);
    MaybeOSR();
  }

  void PushControl(Control* block) {
    // The Liftoff stack includes implicit exception refs stored for catch
    // blocks, so that they can be rethrown.
    block->num_exceptions = num_exceptions_;
  }

  void Block(FullDecoder* decoder, Control* block) { PushControl(block); }

  void Loop(FullDecoder* decoder, Control* loop) {
    // Before entering a loop, spill all locals to the stack, in order to free
    // the cache registers, and to avoid unnecessarily reloading stack values
    // into registers at branches.
    // TODO(clemensb): Come up with a better strategy here, involving
    // pre-analysis of the function.
    __ SpillLocals();

    __ PrepareLoopArgs(loop->start_merge.arity);

    // Loop labels bind at the beginning of the block.
    __ bind(loop->label.get());

    // Save the current cache state for the merge when jumping to this loop.
    loop->label_state.Split(*__ cache_state());

    PushControl(loop);

    if (!dynamic_tiering()) {
      // When the budget-based tiering mechanism is enabled, use that to
      // check for interrupt requests; otherwise execute a stack check in the
      // loop header.
      StackCheck(decoder, decoder->position());
    }
  }

  void Try(FullDecoder* decoder, Control* block) {
    block->try_info = std::make_unique<TryInfo>();
    PushControl(block);
  }

  // Load the property in {kReturnRegister0}.
  LiftoffRegister GetExceptionProperty(LiftoffAssembler::VarState& exception,
                                       RootIndex root_index) {
    DCHECK(root_index == RootIndex::kwasm_exception_tag_symbol ||
           root_index == RootIndex::kwasm_exception_values_symbol);

    LiftoffRegList pinned;
    LiftoffRegister tag_symbol_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadExceptionSymbol(tag_symbol_reg.gp(), pinned, root_index);
    LiftoffRegister context_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LOAD_TAGGED_PTR_INSTANCE_FIELD(context_reg.gp(), NativeContext, pinned);

    LiftoffAssembler::VarState tag_symbol(kPointerKind, tag_symbol_reg, 0);
    LiftoffAssembler::VarState context(kPointerKind, context_reg, 0);

    CallRuntimeStub(WasmCode::kWasmGetOwnProperty,
                    MakeSig::Returns(kPointerKind)
                        .Params(kPointerKind, kPointerKind, kPointerKind),
                    {exception, tag_symbol, context}, kNoSourcePosition);

    return LiftoffRegister(kReturnRegister0);
  }

  void CatchException(FullDecoder* decoder,
                      const TagIndexImmediate<validate>& imm, Control* block,
                      base::Vector<Value> values) {
    DCHECK(block->is_try_catch());
    __ emit_jump(block->label.get());

    // The catch block is unreachable if no possible throws in the try block
    // exist. We only build a landing pad if some node in the try block can
    // (possibly) throw. Otherwise the catch environments remain empty.
    if (!block->try_info->catch_reached) {
      block->reachability = kSpecOnlyReachable;
      return;
    }

    // This is the last use of this label. Re-use the field for the label of the
    // next catch block, and jump there if the tag does not match.
    __ bind(&block->try_info->catch_label);
    new (&block->try_info->catch_label) Label();

    __ cache_state()->Split(block->try_info->catch_state);

    CODE_COMMENT("load caught exception tag");
    DCHECK_EQ(__ cache_state()->stack_state.back().kind(), kRef);
    LiftoffRegister caught_tag =
        GetExceptionProperty(__ cache_state()->stack_state.back(),
                             RootIndex::kwasm_exception_tag_symbol);
    LiftoffRegList pinned;
    pinned.set(caught_tag);

    CODE_COMMENT("load expected exception tag");
    Register imm_tag = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_TAGGED_PTR_INSTANCE_FIELD(imm_tag, TagsTable, pinned);
    __ LoadTaggedPointer(
        imm_tag, imm_tag, no_reg,
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(imm.index));

    CODE_COMMENT("compare tags");
    {
      FREEZE_STATE(frozen);
      Label caught;
      __ emit_cond_jump(kEqual, &caught, kI32, imm_tag, caught_tag.gp(),
                        frozen);
      // The tags don't match, merge the current state into the catch state and
      // jump to the next handler.
      __ MergeFullStackWith(block->try_info->catch_state, *__ cache_state());
      __ emit_jump(&block->try_info->catch_label);
      __ bind(&caught);
    }
    if (!block->try_info->in_handler) {
      block->try_info->in_handler = true;
      num_exceptions_++;
    }
    GetExceptionValues(decoder, __ cache_state()->stack_state.back(), imm.tag);
  }

  void Rethrow(FullDecoder* decoder,
               const LiftoffAssembler::VarState& exception) {
    DCHECK_EQ(exception.kind(), kRef);
    CallRuntimeStub(WasmCode::kWasmRethrow, MakeSig::Params(kPointerKind),
                    {exception}, decoder->position());
  }

  void Delegate(FullDecoder* decoder, uint32_t depth, Control* block) {
    DCHECK_EQ(block, decoder->control_at(0));
    Control* target = decoder->control_at(depth);
    DCHECK(block->is_incomplete_try());
    __ bind(&block->try_info->catch_label);
    if (block->try_info->catch_reached) {
      __ cache_state()->Steal(block->try_info->catch_state);
      if (depth == decoder->control_depth() - 1) {
        // Delegate to the caller, do not emit a landing pad.
        Rethrow(decoder, __ cache_state()->stack_state.back());
        MaybeOSR();
      } else {
        DCHECK(target->is_incomplete_try());
        if (!target->try_info->catch_reached) {
          target->try_info->catch_state.InitMerge(
              *__ cache_state(), __ num_locals(), 1,
              target->stack_depth + target->num_exceptions);
          target->try_info->catch_reached = true;
        }
        __ MergeStackWith(target->try_info->catch_state, 1,
                          LiftoffAssembler::kForwardJump);
        __ emit_jump(&target->try_info->catch_label);
      }
    }
  }

  void Rethrow(FullDecoder* decoder, Control* try_block) {
    int index = try_block->try_info->catch_state.stack_height() - 1;
    auto& exception = __ cache_state()->stack_state[index];
    Rethrow(decoder, exception);
    int pc_offset = __ pc_offset();
    MaybeOSR();
    EmitLandingPad(decoder, pc_offset);
  }

  void CatchAll(FullDecoder* decoder, Control* block) {
    DCHECK(block->is_try_catchall() || block->is_try_catch());
    DCHECK_EQ(decoder->control_at(0), block);

    // The catch block is unreachable if no possible throws in the try block
    // exist. We only build a landing pad if some node in the try block can
    // (possibly) throw. Otherwise the catch environments remain empty.
    if (!block->try_info->catch_reached) {
      decoder->SetSucceedingCodeDynamicallyUnreachable();
      return;
    }

    __ bind(&block->try_info->catch_label);
    __ cache_state()->Steal(block->try_info->catch_state);
    if (!block->try_info->in_handler) {
      block->try_info->in_handler = true;
      num_exceptions_++;
    }
  }

  void JumpIfFalse(FullDecoder* decoder, Label* false_dst,
                   std::unique_ptr<FreezeCacheState>& will_freeze) {
    LiftoffCondition cond =
        test_and_reset_outstanding_op(kExprI32Eqz) ? kNotEqualZero : kEqualZero;

    if (!has_outstanding_op()) {
      // Unary comparison.
      Register value = __ PopToRegister().gp();
      will_freeze.reset(new FreezeCacheState(asm_));
      __ emit_cond_jump(cond, false_dst, kI32, value, no_reg,
                        *will_freeze.get());
      return;
    }

    // Binary comparison of i32 values.
    cond = Negate(GetCompareCondition(outstanding_op_));
    outstanding_op_ = kNoOutstandingOp;
    LiftoffAssembler::VarState rhs_slot = __ cache_state()->stack_state.back();
    if (rhs_slot.is_const()) {
      // Compare to a constant.
      int32_t rhs_imm = rhs_slot.i32_const();
      __ cache_state()->stack_state.pop_back();
      Register lhs = __ PopToRegister().gp();
      will_freeze.reset(new FreezeCacheState(asm_));
      __ emit_i32_cond_jumpi(cond, false_dst, lhs, rhs_imm, *will_freeze.get());
      return;
    }

    Register rhs = __ PopToRegister().gp();
    LiftoffAssembler::VarState lhs_slot = __ cache_state()->stack_state.back();
    if (lhs_slot.is_const()) {
      // Compare a constant to an arbitrary value.
      int32_t lhs_imm = lhs_slot.i32_const();
      __ cache_state()->stack_state.pop_back();
      // Flip the condition, because {lhs} and {rhs} are swapped.
      will_freeze.reset(new FreezeCacheState(asm_));
      __ emit_i32_cond_jumpi(Flip(cond), false_dst, rhs, lhs_imm,
                             *will_freeze.get());
      return;
    }

    // Compare two arbitrary values.
    Register lhs = __ PopToRegister(LiftoffRegList{rhs}).gp();
    will_freeze.reset(new FreezeCacheState(asm_));
    __ emit_cond_jump(cond, false_dst, kI32, lhs, rhs, *will_freeze.get());
  }

  void If(FullDecoder* decoder, const Value& cond, Control* if_block) {
    DCHECK_EQ(if_block, decoder->control_at(0));
    DCHECK(if_block->is_if());

    // Allocate the else state.
    if_block->else_state = std::make_unique<ElseState>();

    // Test the condition on the value stack, jump to else if zero.
    std::unique_ptr<FreezeCacheState> frozen;
    JumpIfFalse(decoder, if_block->else_state->label.get(), frozen);
    frozen.reset();

    // Store the state (after popping the value) for executing the else branch.
    if_block->else_state->state.Split(*__ cache_state());

    PushControl(if_block);
  }

  void FallThruTo(FullDecoder* decoder, Control* c) {
    if (!c->end_merge.reached) {
      c->label_state.InitMerge(*__ cache_state(), __ num_locals(),
                               c->end_merge.arity,
                               c->stack_depth + c->num_exceptions);
    }
    DCHECK(!c->is_try_catchall());
    if (c->is_try_catch()) {
      // Drop the implicit exception ref if any. There may be none if this is a
      // catch-less try block.
      __ MergeStackWith(c->label_state, c->br_merge()->arity,
                        LiftoffAssembler::kForwardJump);
    } else {
      __ MergeFullStackWith(c->label_state, *__ cache_state());
    }
    __ emit_jump(c->label.get());
    TraceCacheState(decoder);
  }

  void FinishOneArmedIf(FullDecoder* decoder, Control* c) {
    DCHECK(c->is_onearmed_if());
    if (c->end_merge.reached) {
      // Someone already merged to the end of the if. Merge both arms into that.
      if (c->reachable()) {
        // Merge the if state into the end state.
        __ MergeFullStackWith(c->label_state, *__ cache_state());
        __ emit_jump(c->label.get());
      }
      // Merge the else state into the end state. Set this state as the current
      // state first so helper functions know which registers are in use.
      __ bind(c->else_state->label.get());
      __ cache_state()->Steal(c->else_state->state);
      __ MergeFullStackWith(c->label_state, *__ cache_state());
      __ cache_state()->Steal(c->label_state);
    } else if (c->reachable()) {
      // No merge yet at the end of the if, but we need to create a merge for
      // the both arms of this if. Thus init the merge point from the else
      // state, then merge the if state into that.
      DCHECK_EQ(c->start_merge.arity, c->end_merge.arity);
      c->label_state.InitMerge(c->else_state->state, __ num_locals(),
                               c->start_merge.arity,
                               c->stack_depth + c->num_exceptions);
      __ MergeFullStackWith(c->label_state, *__ cache_state());
      __ emit_jump(c->label.get());
      // Merge the else state into the end state. Set this state as the current
      // state first so helper functions know which registers are in use.
      __ bind(c->else_state->label.get());
      __ cache_state()->Steal(c->else_state->state);
      __ MergeFullStackWith(c->label_state, *__ cache_state());
      __ cache_state()->Steal(c->label_state);
    } else {
      // No merge needed, just continue with the else state.
      __ bind(c->else_state->label.get());
      __ cache_state()->Steal(c->else_state->state);
    }
  }

  void FinishTry(FullDecoder* decoder, Control* c) {
    DCHECK(c->is_try_catch() || c->is_try_catchall());
    if (!c->end_merge.reached) {
      if (c->try_info->catch_reached) {
        // Drop the implicit exception ref.
        __ DropValue(__ num_locals() + c->stack_depth + c->num_exceptions);
      }
      // Else we did not enter the catch state, continue with the current state.
    } else {
      if (c->reachable()) {
        __ MergeStackWith(c->label_state, c->br_merge()->arity,
                          LiftoffAssembler::kForwardJump);
      }
      __ cache_state()->Steal(c->label_state);
    }
    if (c->try_info->catch_reached) {
      num_exceptions_--;
    }
  }

  void PopControl(FullDecoder* decoder, Control* c) {
    if (c->is_loop()) return;  // A loop just falls through.
    if (c->is_onearmed_if()) {
      // Special handling for one-armed ifs.
      FinishOneArmedIf(decoder, c);
    } else if (c->is_try_catch() || c->is_try_catchall()) {
      FinishTry(decoder, c);
    } else if (c->end_merge.reached) {
      // There is a merge already. Merge our state into that, then continue with
      // that state.
      if (c->reachable()) {
        __ MergeFullStackWith(c->label_state, *__ cache_state());
      }
      __ cache_state()->Steal(c->label_state);
    } else {
      // No merge, just continue with our current state.
    }

    if (!c->label.get()->is_bound()) __ bind(c->label.get());
  }

  void GenerateCCall(const LiftoffRegister* result_regs,
                     const ValueKindSig* sig, ValueKind out_argument_kind,
                     const LiftoffRegister* arg_regs,
                     ExternalReference ext_ref) {
    // Before making a call, spill all cache registers.
    __ SpillAllRegisters();

    // Store arguments on our stack, then align the stack for calling to C.
    int param_bytes = 0;
    for (ValueKind param_kind : sig->parameters()) {
      param_bytes += value_kind_size(param_kind);
    }
    int out_arg_bytes =
        out_argument_kind == kVoid ? 0 : value_kind_size(out_argument_kind);
    int stack_bytes = std::max(param_bytes, out_arg_bytes);
    __ CallC(sig, arg_regs, result_regs, out_argument_kind, stack_bytes,
             ext_ref);
  }

  template <typename EmitFn, typename... Args>
  typename std::enable_if<!std::is_member_function_pointer<EmitFn>::value>::type
  CallEmitFn(EmitFn fn, Args... args) {
    fn(args...);
  }

  template <typename EmitFn, typename... Args>
  typename std::enable_if<std::is_member_function_pointer<EmitFn>::value>::type
  CallEmitFn(EmitFn fn, Args... args) {
    (asm_.*fn)(ConvertAssemblerArg(args)...);
  }

  // Wrap a {LiftoffRegister} with implicit conversions to {Register} and
  // {DoubleRegister}.
  struct AssemblerRegisterConverter {
    LiftoffRegister reg;
    operator LiftoffRegister() { return reg; }
    operator Register() { return reg.gp(); }
    operator DoubleRegister() { return reg.fp(); }
  };

  // Convert {LiftoffRegister} to {AssemblerRegisterConverter}, other types stay
  // unchanged.
  template <typename T>
  typename std::conditional<std::is_same<LiftoffRegister, T>::value,
                            AssemblerRegisterConverter, T>::type
  ConvertAssemblerArg(T t) {
    return {t};
  }

  template <typename EmitFn, typename ArgType>
  struct EmitFnWithFirstArg {
    EmitFn fn;
    ArgType first_arg;
  };

  template <typename EmitFn, typename ArgType>
  EmitFnWithFirstArg<EmitFn, ArgType> BindFirst(EmitFn fn, ArgType arg) {
    return {fn, arg};
  }

  template <typename EmitFn, typename T, typename... Args>
  void CallEmitFn(EmitFnWithFirstArg<EmitFn, T> bound_fn, Args... args) {
    CallEmitFn(bound_fn.fn, bound_fn.first_arg, ConvertAssemblerArg(args)...);
  }

  template <ValueKind src_kind, ValueKind result_kind,
            ValueKind result_lane_kind = kVoid, class EmitFn>
  void EmitUnOp(EmitFn fn) {
    constexpr RegClass src_rc = reg_class_for(src_kind);
    constexpr RegClass result_rc = reg_class_for(result_kind);
    LiftoffRegister src = __ PopToRegister();
    LiftoffRegister dst = src_rc == result_rc
                              ? __ GetUnusedRegister(result_rc, {src}, {})
                              : __ GetUnusedRegister(result_rc, {});
    CallEmitFn(fn, dst, src);
    if (V8_UNLIKELY(nondeterminism_)) {
      LiftoffRegList pinned{dst};
      if (result_kind == ValueKind::kF32 || result_kind == ValueKind::kF64) {
        CheckNan(dst, pinned, result_kind);
      } else if (result_kind == ValueKind::kS128 &&
                 (result_lane_kind == kF32 || result_lane_kind == kF64)) {
        CheckS128Nan(dst, pinned, result_lane_kind);
      }
    }
    __ PushRegister(result_kind, dst);
  }

  template <ValueKind kind>
  void EmitFloatUnOpWithCFallback(
      bool (LiftoffAssembler::*emit_fn)(DoubleRegister, DoubleRegister),
      ExternalReference (*fallback_fn)()) {
    auto emit_with_c_fallback = [=](LiftoffRegister dst, LiftoffRegister src) {
      if ((asm_.*emit_fn)(dst.fp(), src.fp())) return;
      ExternalReference ext_ref = fallback_fn();
      auto sig = MakeSig::Params(kind);
      GenerateCCall(&dst, &sig, kind, &src, ext_ref);
    };
    EmitUnOp<kind, kind>(emit_with_c_fallback);
  }

  enum TypeConversionTrapping : bool { kCanTrap = true, kNoTrap = false };
  template <ValueKind dst_kind, ValueKind src_kind,
            TypeConversionTrapping can_trap>
  void EmitTypeConversion(FullDecoder* decoder, WasmOpcode opcode,
                          ExternalReference (*fallback_fn)()) {
    static constexpr RegClass src_rc = reg_class_for(src_kind);
    static constexpr RegClass dst_rc = reg_class_for(dst_kind);
    LiftoffRegister src = __ PopToRegister();
    LiftoffRegister dst = src_rc == dst_rc
                              ? __ GetUnusedRegister(dst_rc, {src}, {})
                              : __ GetUnusedRegister(dst_rc, {});
    Label* trap =
        can_trap ? AddOutOfLineTrap(
                       decoder, WasmCode::kThrowWasmTrapFloatUnrepresentable)
                 : nullptr;
    if (!__ emit_type_conversion(opcode, dst, src, trap)) {
      DCHECK_NOT_NULL(fallback_fn);
      ExternalReference ext_ref = fallback_fn();
      if (can_trap) {
        // External references for potentially trapping conversions return int.
        auto sig = MakeSig::Returns(kI32).Params(src_kind);
        LiftoffRegister ret_reg =
            __ GetUnusedRegister(kGpReg, LiftoffRegList{dst});
        LiftoffRegister dst_regs[] = {ret_reg, dst};
        GenerateCCall(dst_regs, &sig, dst_kind, &src, ext_ref);
        // It's okay that this is short-lived: we're trapping anyway.
        FREEZE_STATE(trapping);
        __ emit_cond_jump(kEqual, trap, kI32, ret_reg.gp(), no_reg, trapping);
      } else {
        ValueKind sig_kinds[] = {src_kind};
        ValueKindSig sig(0, 1, sig_kinds);
        GenerateCCall(&dst, &sig, dst_kind, &src, ext_ref);
      }
    }
    __ PushRegister(dst_kind, dst);
  }

  void UnOp(FullDecoder* decoder, WasmOpcode opcode, const Value& value,
            Value* result) {
#define CASE_I32_UNOP(opcode, fn) \
  case kExpr##opcode:             \
    return EmitUnOp<kI32, kI32>(&LiftoffAssembler::emit_##fn);
#define CASE_I64_UNOP(opcode, fn) \
  case kExpr##opcode:             \
    return EmitUnOp<kI64, kI64>(&LiftoffAssembler::emit_##fn);
#define CASE_FLOAT_UNOP(opcode, kind, fn) \
  case kExpr##opcode:                     \
    return EmitUnOp<k##kind, k##kind>(&LiftoffAssembler::emit_##fn);
#define CASE_FLOAT_UNOP_WITH_CFALLBACK(opcode, kind, fn)                     \
  case kExpr##opcode:                                                        \
    return EmitFloatUnOpWithCFallback<k##kind>(&LiftoffAssembler::emit_##fn, \
                                               &ExternalReference::wasm_##fn);
#define CASE_TYPE_CONVERSION(opcode, dst_kind, src_kind, ext_ref, can_trap) \
  case kExpr##opcode:                                                       \
    return EmitTypeConversion<k##dst_kind, k##src_kind, can_trap>(          \
        decoder, kExpr##opcode, ext_ref);
    switch (opcode) {
      CASE_I32_UNOP(I32Clz, i32_clz)
      CASE_I32_UNOP(I32Ctz, i32_ctz)
      CASE_FLOAT_UNOP(F32Abs, F32, f32_abs)
      CASE_FLOAT_UNOP(F32Neg, F32, f32_neg)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F32Ceil, F32, f32_ceil)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F32Floor, F32, f32_floor)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F32Trunc, F32, f32_trunc)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F32NearestInt, F32, f32_nearest_int)
      CASE_FLOAT_UNOP(F32Sqrt, F32, f32_sqrt)
      CASE_FLOAT_UNOP(F64Abs, F64, f64_abs)
      CASE_FLOAT_UNOP(F64Neg, F64, f64_neg)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64Ceil, F64, f64_ceil)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64Floor, F64, f64_floor)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64Trunc, F64, f64_trunc)
      CASE_FLOAT_UNOP_WITH_CFALLBACK(F64NearestInt, F64, f64_nearest_int)
      CASE_FLOAT_UNOP(F64Sqrt, F64, f64_sqrt)
      CASE_TYPE_CONVERSION(I32ConvertI64, I32, I64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I32SConvertF32, I32, F32, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32UConvertF32, I32, F32, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32SConvertF64, I32, F64, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32UConvertF64, I32, F64, nullptr, kCanTrap)
      CASE_TYPE_CONVERSION(I32ReinterpretF32, I32, F32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64SConvertI32, I64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64UConvertI32, I64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64SConvertF32, I64, F32,
                           &ExternalReference::wasm_float32_to_int64, kCanTrap)
      CASE_TYPE_CONVERSION(I64UConvertF32, I64, F32,
                           &ExternalReference::wasm_float32_to_uint64, kCanTrap)
      CASE_TYPE_CONVERSION(I64SConvertF64, I64, F64,
                           &ExternalReference::wasm_float64_to_int64, kCanTrap)
      CASE_TYPE_CONVERSION(I64UConvertF64, I64, F64,
                           &ExternalReference::wasm_float64_to_uint64, kCanTrap)
      CASE_TYPE_CONVERSION(I64ReinterpretF64, I64, F64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32SConvertI32, F32, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32UConvertI32, F32, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32SConvertI64, F32, I64,
                           &ExternalReference::wasm_int64_to_float32, kNoTrap)
      CASE_TYPE_CONVERSION(F32UConvertI64, F32, I64,
                           &ExternalReference::wasm_uint64_to_float32, kNoTrap)
      CASE_TYPE_CONVERSION(F32ConvertF64, F32, F64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F32ReinterpretI32, F32, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64SConvertI32, F64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64UConvertI32, F64, I32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64SConvertI64, F64, I64,
                           &ExternalReference::wasm_int64_to_float64, kNoTrap)
      CASE_TYPE_CONVERSION(F64UConvertI64, F64, I64,
                           &ExternalReference::wasm_uint64_to_float64, kNoTrap)
      CASE_TYPE_CONVERSION(F64ConvertF32, F64, F32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(F64ReinterpretI64, F64, I64, nullptr, kNoTrap)
      CASE_I32_UNOP(I32SExtendI8, i32_signextend_i8)
      CASE_I32_UNOP(I32SExtendI16, i32_signextend_i16)
      CASE_I64_UNOP(I64SExtendI8, i64_signextend_i8)
      CASE_I64_UNOP(I64SExtendI16, i64_signextend_i16)
      CASE_I64_UNOP(I64SExtendI32, i64_signextend_i32)
      CASE_I64_UNOP(I64Clz, i64_clz)
      CASE_I64_UNOP(I64Ctz, i64_ctz)
      CASE_TYPE_CONVERSION(I32SConvertSatF32, I32, F32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I32UConvertSatF32, I32, F32, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I32SConvertSatF64, I32, F64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I32UConvertSatF64, I32, F64, nullptr, kNoTrap)
      CASE_TYPE_CONVERSION(I64SConvertSatF32, I64, F32,
                           &ExternalReference::wasm_float32_to_int64_sat,
                           kNoTrap)
      CASE_TYPE_CONVERSION(I64UConvertSatF32, I64, F32,
                           &ExternalReference::wasm_float32_to_uint64_sat,
                           kNoTrap)
      CASE_TYPE_CONVERSION(I64SConvertSatF64, I64, F64,
                           &ExternalReference::wasm_float64_to_int64_sat,
                           kNoTrap)
      CASE_TYPE_CONVERSION(I64UConvertSatF64, I64, F64,
                           &ExternalReference::wasm_float64_to_uint64_sat,
                           kNoTrap)
      case kExprI32Eqz:
        DCHECK(decoder->lookahead(0, kExprI32Eqz));
        if ((decoder->lookahead(1, kExprBrIf) ||
             decoder->lookahead(1, kExprIf)) &&
            !for_debugging_) {
          DCHECK(!has_outstanding_op());
          outstanding_op_ = kExprI32Eqz;
          break;
        }
        return EmitUnOp<kI32, kI32>(&LiftoffAssembler::emit_i32_eqz);
      case kExprI64Eqz:
        return EmitUnOp<kI64, kI32>(&LiftoffAssembler::emit_i64_eqz);
      case kExprI32Popcnt:
        return EmitUnOp<kI32, kI32>(
            [=](LiftoffRegister dst, LiftoffRegister src) {
              if (__ emit_i32_popcnt(dst.gp(), src.gp())) return;
              auto sig = MakeSig::Returns(kI32).Params(kI32);
              GenerateCCall(&dst, &sig, kVoid, &src,
                            ExternalReference::wasm_word32_popcnt());
            });
      case kExprI64Popcnt:
        return EmitUnOp<kI64, kI64>(
            [=](LiftoffRegister dst, LiftoffRegister src) {
              if (__ emit_i64_popcnt(dst, src)) return;
              // The c function returns i32. We will zero-extend later.
              auto sig = MakeSig::Returns(kI32).Params(kI64);
              LiftoffRegister c_call_dst = kNeedI64RegPair ? dst.low() : dst;
              GenerateCCall(&c_call_dst, &sig, kVoid, &src,
                            ExternalReference::wasm_word64_popcnt());
              // Now zero-extend the result to i64.
              __ emit_type_conversion(kExprI64UConvertI32, dst, c_call_dst,
                                      nullptr);
            });
      case kExprRefIsNull:
      // We abuse ref.as_non_null, which isn't otherwise used in this switch, as
      // a sentinel for the negation of ref.is_null.
      case kExprRefAsNonNull: {
        LiftoffRegList pinned;
        LiftoffRegister ref = pinned.set(__ PopToRegister());
        LiftoffRegister null = __ GetUnusedRegister(kGpReg, pinned);
        LoadNullValue(null.gp(), pinned);
        // Prefer to overwrite one of the input registers with the result
        // of the comparison.
        LiftoffRegister dst = __ GetUnusedRegister(kGpReg, {ref, null}, {});
        __ emit_ptrsize_set_cond(opcode == kExprRefIsNull ? kEqual : kUnequal,
                                 dst.gp(), ref, null);
        __ PushRegister(kI32, dst);
        return;
      }
      case kExprExternInternalize:
        if (!v8_flags.wasm_gc_js_interop) {
          LiftoffRegList pinned;
          LiftoffRegister context_reg =
              pinned.set(__ GetUnusedRegister(kGpReg, pinned));
          LOAD_TAGGED_PTR_INSTANCE_FIELD(context_reg.gp(), NativeContext,
                                         pinned);
          LiftoffAssembler::VarState& extern_value =
              __ cache_state()->stack_state.back();

          LiftoffAssembler::VarState context(kPointerKind, context_reg, 0);

          CallRuntimeStub(
              WasmCode::kWasmExternInternalize,
              MakeSig::Returns(kPointerKind).Params(kPointerKind, kPointerKind),
              {extern_value, context}, decoder->position());
          __ DropValues(1);
          __ PushRegister(kRefNull, LiftoffRegister(kReturnRegister0));
        }
        return;
      case kExprExternExternalize:
        if (!v8_flags.wasm_gc_js_interop) {
          LiftoffRegList pinned;
          LiftoffRegister context_reg =
              pinned.set(__ GetUnusedRegister(kGpReg, pinned));
          LOAD_TAGGED_PTR_INSTANCE_FIELD(context_reg.gp(), NativeContext,
                                         pinned);
          LiftoffAssembler::VarState& value =
              __ cache_state()->stack_state.back();

          LiftoffAssembler::VarState context(kPointerKind, context_reg, 0);

          CallRuntimeStub(
              WasmCode::kWasmExternExternalize,
              MakeSig::Returns(kPointerKind).Params(kPointerKind, kPointerKind),
              {value, context}, decoder->position());
          __ DropValues(1);
          __ PushRegister(kRefNull, LiftoffRegister(kReturnRegister0));
        }
        return;
      default:
        UNREACHABLE();
    }
#undef CASE_I32_UNOP
#undef CASE_I64_UNOP
#undef CASE_FLOAT_UNOP
#undef CASE_FLOAT_UNOP_WITH_CFALLBACK
#undef CASE_TYPE_CONVERSION
  }

  template <ValueKind src_kind, ValueKind result_kind, typename EmitFn,
            typename EmitFnImm>
  void EmitBinOpImm(EmitFn fn, EmitFnImm fnImm) {
    static constexpr RegClass src_rc = reg_class_for(src_kind);
    static constexpr RegClass result_rc = reg_class_for(result_kind);

    LiftoffAssembler::VarState rhs_slot = __ cache_state()->stack_state.back();
    // Check if the RHS is an immediate.
    if (rhs_slot.is_const()) {
      __ cache_state()->stack_state.pop_back();
      int32_t imm = rhs_slot.i32_const();

      LiftoffRegister lhs = __ PopToRegister();
      // Either reuse {lhs} for {dst}, or choose a register (pair) which does
      // not overlap, for easier code generation.
      LiftoffRegList pinned{lhs};
      LiftoffRegister dst = src_rc == result_rc
                                ? __ GetUnusedRegister(result_rc, {lhs}, pinned)
                                : __ GetUnusedRegister(result_rc, pinned);

      CallEmitFn(fnImm, dst, lhs, imm);
      static_assert(result_kind != kF32 && result_kind != kF64,
                    "Unhandled nondeterminism for fuzzing.");
      __ PushRegister(result_kind, dst);
    } else {
      // The RHS was not an immediate.
      EmitBinOp<src_kind, result_kind>(fn);
    }
  }

  template <ValueKind src_kind, ValueKind result_kind,
            bool swap_lhs_rhs = false, ValueKind result_lane_kind = kVoid,
            typename EmitFn>
  void EmitBinOp(EmitFn fn) {
    static constexpr RegClass src_rc = reg_class_for(src_kind);
    static constexpr RegClass result_rc = reg_class_for(result_kind);
    LiftoffRegister rhs = __ PopToRegister();
    LiftoffRegister lhs = __ PopToRegister(LiftoffRegList{rhs});
    LiftoffRegister dst = src_rc == result_rc
                              ? __ GetUnusedRegister(result_rc, {lhs, rhs}, {})
                              : __ GetUnusedRegister(result_rc, {});

    if (swap_lhs_rhs) std::swap(lhs, rhs);

    CallEmitFn(fn, dst, lhs, rhs);
    if (V8_UNLIKELY(nondeterminism_)) {
      LiftoffRegList pinned{dst};
      if (result_kind == ValueKind::kF32 || result_kind == ValueKind::kF64) {
        CheckNan(dst, pinned, result_kind);
      } else if (result_kind == ValueKind::kS128 &&
                 (result_lane_kind == kF32 || result_lane_kind == kF64)) {
        CheckS128Nan(dst, pinned, result_lane_kind);
      }
    }
    __ PushRegister(result_kind, dst);
  }

  void EmitDivOrRem64CCall(LiftoffRegister dst, LiftoffRegister lhs,
                           LiftoffRegister rhs, ExternalReference ext_ref,
                           Label* trap_by_zero,
                           Label* trap_unrepresentable = nullptr) {
    // Cannot emit native instructions, build C call.
    LiftoffRegister ret = __ GetUnusedRegister(kGpReg, LiftoffRegList{dst});
    LiftoffRegister tmp =
        __ GetUnusedRegister(kGpReg, LiftoffRegList{dst, ret});
    LiftoffRegister arg_regs[] = {lhs, rhs};
    LiftoffRegister result_regs[] = {ret, dst};
    auto sig = MakeSig::Returns(kI32).Params(kI64, kI64);
    GenerateCCall(result_regs, &sig, kI64, arg_regs, ext_ref);
    FREEZE_STATE(trapping);
    __ LoadConstant(tmp, WasmValue(int32_t{0}));
    __ emit_cond_jump(kEqual, trap_by_zero, kI32, ret.gp(), tmp.gp(), trapping);
    if (trap_unrepresentable) {
      __ LoadConstant(tmp, WasmValue(int32_t{-1}));
      __ emit_cond_jump(kEqual, trap_unrepresentable, kI32, ret.gp(), tmp.gp(),
                        trapping);
    }
  }

  template <WasmOpcode opcode>
  void EmitI32CmpOp(FullDecoder* decoder) {
    DCHECK(decoder->lookahead(0, opcode));
    if ((decoder->lookahead(1, kExprBrIf) || decoder->lookahead(1, kExprIf)) &&
        !for_debugging_) {
      DCHECK(!has_outstanding_op());
      outstanding_op_ = opcode;
      return;
    }
    return EmitBinOp<kI32, kI32>(BindFirst(&LiftoffAssembler::emit_i32_set_cond,
                                           GetCompareCondition(opcode)));
  }

  void BinOp(FullDecoder* decoder, WasmOpcode opcode, const Value& lhs,
             const Value& rhs, Value* result) {
#define CASE_I64_SHIFTOP(opcode, fn)                                         \
  case kExpr##opcode:                                                        \
    return EmitBinOpImm<kI64, kI64>(                                         \
        [=](LiftoffRegister dst, LiftoffRegister src,                        \
            LiftoffRegister amount) {                                        \
          __ emit_##fn(dst, src,                                             \
                       amount.is_gp_pair() ? amount.low_gp() : amount.gp()); \
        },                                                                   \
        &LiftoffAssembler::emit_##fn##i);
#define CASE_CCALL_BINOP(opcode, kind, ext_ref_fn)                           \
  case kExpr##opcode:                                                        \
    return EmitBinOp<k##kind, k##kind>(                                      \
        [=](LiftoffRegister dst, LiftoffRegister lhs, LiftoffRegister rhs) { \
          LiftoffRegister args[] = {lhs, rhs};                               \
          auto ext_ref = ExternalReference::ext_ref_fn();                    \
          ValueKind sig_kinds[] = {k##kind, k##kind, k##kind};               \
          const bool out_via_stack = k##kind == kI64;                        \
          ValueKindSig sig(out_via_stack ? 0 : 1, 2, sig_kinds);             \
          ValueKind out_arg_kind = out_via_stack ? kI64 : kVoid;             \
          GenerateCCall(&dst, &sig, out_arg_kind, args, ext_ref);            \
        });
    switch (opcode) {
      case kExprI32Add:
        return EmitBinOpImm<kI32, kI32>(&LiftoffAssembler::emit_i32_add,
                                        &LiftoffAssembler::emit_i32_addi);
      case kExprI32Sub:
        return EmitBinOp<kI32, kI32>(&LiftoffAssembler::emit_i32_sub);
      case kExprI32Mul:
        return EmitBinOp<kI32, kI32>(&LiftoffAssembler::emit_i32_mul);
      case kExprI32And:
        return EmitBinOpImm<kI32, kI32>(&LiftoffAssembler::emit_i32_and,
                                        &LiftoffAssembler::emit_i32_andi);
      case kExprI32Ior:
        return EmitBinOpImm<kI32, kI32>(&LiftoffAssembler::emit_i32_or,
                                        &LiftoffAssembler::emit_i32_ori);
      case kExprI32Xor:
        return EmitBinOpImm<kI32, kI32>(&LiftoffAssembler::emit_i32_xor,
                                        &LiftoffAssembler::emit_i32_xori);
      case kExprI32Eq:
        return EmitI32CmpOp<kExprI32Eq>(decoder);
      case kExprI32Ne:
        return EmitI32CmpOp<kExprI32Ne>(decoder);
      case kExprI32LtS:
        return EmitI32CmpOp<kExprI32LtS>(decoder);
      case kExprI32LtU:
        return EmitI32CmpOp<kExprI32LtU>(decoder);
      case kExprI32GtS:
        return EmitI32CmpOp<kExprI32GtS>(decoder);
      case kExprI32GtU:
        return EmitI32CmpOp<kExprI32GtU>(decoder);
      case kExprI32LeS:
        return EmitI32CmpOp<kExprI32LeS>(decoder);
      case kExprI32LeU:
        return EmitI32CmpOp<kExprI32LeU>(decoder);
      case kExprI32GeS:
        return EmitI32CmpOp<kExprI32GeS>(decoder);
      case kExprI32GeU:
        return EmitI32CmpOp<kExprI32GeU>(decoder);
      case kExprI64Add:
        return EmitBinOpImm<kI64, kI64>(&LiftoffAssembler::emit_i64_add,
                                        &LiftoffAssembler::emit_i64_addi);
      case kExprI64Sub:
        return EmitBinOp<kI64, kI64>(&LiftoffAssembler::emit_i64_sub);
      case kExprI64Mul:
        return EmitBinOp<kI64, kI64>(&LiftoffAssembler::emit_i64_mul);
      case kExprI64And:
        return EmitBinOpImm<kI64, kI64>(&LiftoffAssembler::emit_i64_and,
                                        &LiftoffAssembler::emit_i64_andi);
      case kExprI64Ior:
        return EmitBinOpImm<kI64, kI64>(&LiftoffAssembler::emit_i64_or,
                                        &LiftoffAssembler::emit_i64_ori);
      case kExprI64Xor:
        return EmitBinOpImm<kI64, kI64>(&LiftoffAssembler::emit_i64_xor,
                                        &LiftoffAssembler::emit_i64_xori);
      case kExprI64Eq:
        return EmitBinOp<kI64, kI32>(
            BindFirst(&LiftoffAssembler::emit_i64_set_cond, kEqual));
      case kExprI64Ne:
        return EmitBinOp<kI64, kI32>(
            BindFirst(&LiftoffAssembler::emit_i64_set_cond, kUnequal));
      case kExprI64LtS:
        return EmitBinOp<kI64, kI32>(
            BindFirst(&LiftoffAssembler::emit_i64_set_cond, kSignedLessThan));
      case kExprI64LtU:
        return EmitBinOp<kI64, kI32>(
            BindFirst(&LiftoffAssembler::emit_i64_set_cond, kUnsignedLessThan));
      case kExprI64GtS:
        return EmitBinOp<kI64, kI32>(BindFirst(
            &LiftoffAssembler::emit_i64_set_cond, kSignedGreaterThan));
      case kExprI64GtU:
        return EmitBinOp<kI64, kI32>(BindFirst(
            &LiftoffAssembler::emit_i64_set_cond, kUnsignedGreaterThan));
      case kExprI64LeS:
        return EmitBinOp<kI64, kI32>(
            BindFirst(&LiftoffAssembler::emit_i64_set_cond, kSignedLessEqual));
      case kExprI64LeU:
        return EmitBinOp<kI64, kI32>(BindFirst(
            &LiftoffAssembler::emit_i64_set_cond, kUnsignedLessEqual));
      case kExprI64GeS:
        return EmitBinOp<kI64, kI32>(BindFirst(
            &LiftoffAssembler::emit_i64_set_cond, kSignedGreaterEqual));
      case kExprI64GeU:
        return EmitBinOp<kI64, kI32>(BindFirst(
            &LiftoffAssembler::emit_i64_set_cond, kUnsignedGreaterEqual));
      case kExprF32Eq:
        return EmitBinOp<kF32, kI32>(
            BindFirst(&LiftoffAssembler::emit_f32_set_cond, kEqual));
      case kExprF32Ne:
        return EmitBinOp<kF32, kI32>(
            BindFirst(&LiftoffAssembler::emit_f32_set_cond, kUnequal));
      case kExprF32Lt:
        return EmitBinOp<kF32, kI32>(
            BindFirst(&LiftoffAssembler::emit_f32_set_cond, kUnsignedLessThan));
      case kExprF32Gt:
        return EmitBinOp<kF32, kI32>(BindFirst(
            &LiftoffAssembler::emit_f32_set_cond, kUnsignedGreaterThan));
      case kExprF32Le:
        return EmitBinOp<kF32, kI32>(BindFirst(
            &LiftoffAssembler::emit_f32_set_cond, kUnsignedLessEqual));
      case kExprF32Ge:
        return EmitBinOp<kF32, kI32>(BindFirst(
            &LiftoffAssembler::emit_f32_set_cond, kUnsignedGreaterEqual));
      case kExprF64Eq:
        return EmitBinOp<kF64, kI32>(
            BindFirst(&LiftoffAssembler::emit_f64_set_cond, kEqual));
      case kExprF64Ne:
        return EmitBinOp<kF64, kI32>(
            BindFirst(&LiftoffAssembler::emit_f64_set_cond, kUnequal));
      case kExprF64Lt:
        return EmitBinOp<kF64, kI32>(
            BindFirst(&LiftoffAssembler::emit_f64_set_cond, kUnsignedLessThan));
      case kExprF64Gt:
        return EmitBinOp<kF64, kI32>(BindFirst(
            &LiftoffAssembler::emit_f64_set_cond, kUnsignedGreaterThan));
      case kExprF64Le:
        return EmitBinOp<kF64, kI32>(BindFirst(
            &LiftoffAssembler::emit_f64_set_cond, kUnsignedLessEqual));
      case kExprF64Ge:
        return EmitBinOp<kF64, kI32>(BindFirst(
            &LiftoffAssembler::emit_f64_set_cond, kUnsignedGreaterEqual));
      case kExprI32Shl:
        return EmitBinOpImm<kI32, kI32>(&LiftoffAssembler::emit_i32_shl,
                                        &LiftoffAssembler::emit_i32_shli);
      case kExprI32ShrS:
        return EmitBinOpImm<kI32, kI32>(&LiftoffAssembler::emit_i32_sar,
                                        &LiftoffAssembler::emit_i32_sari);
      case kExprI32ShrU:
        return EmitBinOpImm<kI32, kI32>(&LiftoffAssembler::emit_i32_shr,
                                        &LiftoffAssembler::emit_i32_shri);
        CASE_CCALL_BINOP(I32Rol, I32, wasm_word32_rol)
        CASE_CCALL_BINOP(I32Ror, I32, wasm_word32_ror)
        CASE_I64_SHIFTOP(I64Shl, i64_shl)
        CASE_I64_SHIFTOP(I64ShrS, i64_sar)
        CASE_I64_SHIFTOP(I64ShrU, i64_shr)
        CASE_CCALL_BINOP(I64Rol, I64, wasm_word64_rol)
        CASE_CCALL_BINOP(I64Ror, I64, wasm_word64_ror)
      case kExprF32Add:
        return EmitBinOp<kF32, kF32>(&LiftoffAssembler::emit_f32_add);
      case kExprF32Sub:
        return EmitBinOp<kF32, kF32>(&LiftoffAssembler::emit_f32_sub);
      case kExprF32Mul:
        return EmitBinOp<kF32, kF32>(&LiftoffAssembler::emit_f32_mul);
      case kExprF32Div:
        return EmitBinOp<kF32, kF32>(&LiftoffAssembler::emit_f32_div);
      case kExprF32Min:
        return EmitBinOp<kF32, kF32>(&LiftoffAssembler::emit_f32_min);
      case kExprF32Max:
        return EmitBinOp<kF32, kF32>(&LiftoffAssembler::emit_f32_max);
      case kExprF32CopySign:
        return EmitBinOp<kF32, kF32>(&LiftoffAssembler::emit_f32_copysign);
      case kExprF64Add:
        return EmitBinOp<kF64, kF64>(&LiftoffAssembler::emit_f64_add);
      case kExprF64Sub:
        return EmitBinOp<kF64, kF64>(&LiftoffAssembler::emit_f64_sub);
      case kExprF64Mul:
        return EmitBinOp<kF64, kF64>(&LiftoffAssembler::emit_f64_mul);
      case kExprF64Div:
        return EmitBinOp<kF64, kF64>(&LiftoffAssembler::emit_f64_div);
      case kExprF64Min:
        return EmitBinOp<kF64, kF64>(&LiftoffAssembler::emit_f64_min);
      case kExprF64Max:
        return EmitBinOp<kF64, kF64>(&LiftoffAssembler::emit_f64_max);
      case kExprF64CopySign:
        return EmitBinOp<kF64, kF64>(&LiftoffAssembler::emit_f64_copysign);
      case kExprI32DivS:
        return EmitBinOp<kI32, kI32>([this, decoder](LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
          AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapDivByZero);
          // Adding the second trap might invalidate the pointer returned for
          // the first one, thus get both pointers afterwards.
          AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapDivUnrepresentable);
          Label* div_by_zero = out_of_line_code_.end()[-2].label.get();
          Label* div_unrepresentable = out_of_line_code_.end()[-1].label.get();
          __ emit_i32_divs(dst.gp(), lhs.gp(), rhs.gp(), div_by_zero,
                           div_unrepresentable);
        });
      case kExprI32DivU:
        return EmitBinOp<kI32, kI32>([this, decoder](LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
          Label* div_by_zero =
              AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapDivByZero);
          __ emit_i32_divu(dst.gp(), lhs.gp(), rhs.gp(), div_by_zero);
        });
      case kExprI32RemS:
        return EmitBinOp<kI32, kI32>([this, decoder](LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
          Label* rem_by_zero =
              AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapRemByZero);
          __ emit_i32_rems(dst.gp(), lhs.gp(), rhs.gp(), rem_by_zero);
        });
      case kExprI32RemU:
        return EmitBinOp<kI32, kI32>([this, decoder](LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
          Label* rem_by_zero =
              AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapRemByZero);
          __ emit_i32_remu(dst.gp(), lhs.gp(), rhs.gp(), rem_by_zero);
        });
      case kExprI64DivS:
        return EmitBinOp<kI64, kI64>([this, decoder](LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
          AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapDivByZero);
          // Adding the second trap might invalidate the pointer returned for
          // the first one, thus get both pointers afterwards.
          AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapDivUnrepresentable);
          Label* div_by_zero = out_of_line_code_.end()[-2].label.get();
          Label* div_unrepresentable = out_of_line_code_.end()[-1].label.get();
          if (!__ emit_i64_divs(dst, lhs, rhs, div_by_zero,
                                div_unrepresentable)) {
            ExternalReference ext_ref = ExternalReference::wasm_int64_div();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, div_by_zero,
                                div_unrepresentable);
          }
        });
      case kExprI64DivU:
        return EmitBinOp<kI64, kI64>([this, decoder](LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
          Label* div_by_zero =
              AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapDivByZero);
          if (!__ emit_i64_divu(dst, lhs, rhs, div_by_zero)) {
            ExternalReference ext_ref = ExternalReference::wasm_uint64_div();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, div_by_zero);
          }
        });
      case kExprI64RemS:
        return EmitBinOp<kI64, kI64>([this, decoder](LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
          Label* rem_by_zero =
              AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapRemByZero);
          if (!__ emit_i64_rems(dst, lhs, rhs, rem_by_zero)) {
            ExternalReference ext_ref = ExternalReference::wasm_int64_mod();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, rem_by_zero);
          }
        });
      case kExprI64RemU:
        return EmitBinOp<kI64, kI64>([this, decoder](LiftoffRegister dst,
                                                     LiftoffRegister lhs,
                                                     LiftoffRegister rhs) {
          Label* rem_by_zero =
              AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapRemByZero);
          if (!__ emit_i64_remu(dst, lhs, rhs, rem_by_zero)) {
            ExternalReference ext_ref = ExternalReference::wasm_uint64_mod();
            EmitDivOrRem64CCall(dst, lhs, rhs, ext_ref, rem_by_zero);
          }
        });
      case kExprRefEq: {
        return EmitBinOp<kRefNull, kI32>(
            BindFirst(&LiftoffAssembler::emit_ptrsize_set_cond, kEqual));
      }

      default:
        UNREACHABLE();
    }
#undef CASE_I64_SHIFTOP
#undef CASE_CCALL_BINOP
  }

  void TraceInstruction(FullDecoder* decoder, uint32_t markid) {
#if V8_TARGET_ARCH_X64
    __ emit_trace_instruction(markid);
#endif
  }

  void I32Const(FullDecoder* decoder, Value* result, int32_t value) {
    __ PushConstant(kI32, value);
  }

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {
    // The {VarState} stores constant values as int32_t, thus we only store
    // 64-bit constants in this field if it fits in an int32_t. Larger values
    // cannot be used as immediate value anyway, so we can also just put them in
    // a register immediately.
    int32_t value_i32 = static_cast<int32_t>(value);
    if (value_i32 == value) {
      __ PushConstant(kI64, value_i32);
    } else {
      LiftoffRegister reg = __ GetUnusedRegister(reg_class_for(kI64), {});
      __ LoadConstant(reg, WasmValue(value));
      __ PushRegister(kI64, reg);
    }
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    LiftoffRegister reg = __ GetUnusedRegister(kFpReg, {});
    __ LoadConstant(reg, WasmValue(value));
    __ PushRegister(kF32, reg);
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    LiftoffRegister reg = __ GetUnusedRegister(kFpReg, {});
    __ LoadConstant(reg, WasmValue(value));
    __ PushRegister(kF64, reg);
  }

  void RefNull(FullDecoder* decoder, ValueType type, Value*) {
    LiftoffRegister null = __ GetUnusedRegister(kGpReg, {});
    LoadNullValue(null.gp(), {});
    __ PushRegister(type.kind(), null);
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    LiftoffRegister func_index_reg = __ GetUnusedRegister(kGpReg, {});
    __ LoadConstant(func_index_reg, WasmValue(function_index));
    LiftoffAssembler::VarState func_index_var(kI32, func_index_reg, 0);
    CallRuntimeStub(WasmCode::kWasmRefFunc, MakeSig::Returns(kRef).Params(kI32),
                    {func_index_var}, decoder->position());
    __ PushRegister(kRef, LiftoffRegister(kReturnRegister0));
  }

  void RefAsNonNull(FullDecoder* decoder, const Value& arg, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister obj = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, obj.gp(), pinned, arg.type);
    __ PushRegister(kRef, obj);
  }

  void Drop(FullDecoder* decoder) { __ DropValues(1); }

  void TraceFunctionExit(FullDecoder* decoder) {
    CODE_COMMENT("trace function exit");
    // Before making the runtime call, spill all cache registers.
    __ SpillAllRegisters();

    // Store the return value if there is exactly one. Multiple return values
    // are not handled yet.
    size_t num_returns = decoder->sig_->return_count();
    // Put the parameter in its place.
    WasmTraceExitDescriptor descriptor;
    DCHECK_EQ(0, descriptor.GetStackParameterCount());
    DCHECK_EQ(1, descriptor.GetRegisterParameterCount());
    Register param_reg = descriptor.GetRegisterParameter(0);
    if (num_returns == 1) {
      auto& return_slot = __ cache_state()->stack_state.back();
      if (return_slot.is_const()) {
        __ Spill(&return_slot);
      }
      DCHECK(return_slot.is_stack());
      __ LoadSpillAddress(param_reg, return_slot.offset(), return_slot.kind());
    }

    source_position_table_builder_.AddPosition(
        __ pc_offset(), SourcePosition(decoder->position()), false);
    __ CallRuntimeStub(WasmCode::kWasmTraceExit);
    DefineSafepoint();
  }

  void TierupCheckOnTailCall(FullDecoder* decoder) {
    if (!dynamic_tiering()) return;
    LiftoffRegList pinned;
    Register tmp1 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register tmp2 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    TierupCheck(decoder, decoder->position(), __ pc_offset(), tmp1, tmp2);
  }

  void DoReturn(FullDecoder* decoder, uint32_t /* drop values */) {
    Register tmp1 = no_reg;
    Register tmp2 = no_reg;
    if (dynamic_tiering()) {
      LiftoffRegList pinned;
      tmp1 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
      tmp2 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    }
    ReturnImpl(decoder, tmp1, tmp2);
  }

  void ReturnImpl(FullDecoder* decoder, Register tmp1, Register tmp2) {
    if (v8_flags.trace_wasm) TraceFunctionExit(decoder);
    if (dynamic_tiering()) {
      TierupCheck(decoder, decoder->position(), __ pc_offset(), tmp1, tmp2);
    }
    size_t num_returns = decoder->sig_->return_count();
    if (num_returns > 0) __ MoveToReturnLocations(decoder->sig_, descriptor_);
    __ LeaveFrame(StackFrame::WASM);
    __ DropStackSlotsAndRet(
        static_cast<uint32_t>(descriptor_->ParameterSlotCount()));
  }

  void LocalGet(FullDecoder* decoder, Value* result,
                const IndexImmediate<validate>& imm) {
    auto local_slot = __ cache_state()->stack_state[imm.index];
    __ cache_state()->stack_state.emplace_back(
        local_slot.kind(), __ NextSpillOffset(local_slot.kind()));
    auto* slot = &__ cache_state()->stack_state.back();
    if (local_slot.is_reg()) {
      __ cache_state()->inc_used(local_slot.reg());
      slot->MakeRegister(local_slot.reg());
    } else if (local_slot.is_const()) {
      slot->MakeConstant(local_slot.i32_const());
    } else {
      DCHECK(local_slot.is_stack());
      auto rc = reg_class_for(local_slot.kind());
      LiftoffRegister reg = __ GetUnusedRegister(rc, {});
      __ cache_state()->inc_used(reg);
      slot->MakeRegister(reg);
      __ Fill(reg, local_slot.offset(), local_slot.kind());
    }
  }

  void LocalSetFromStackSlot(LiftoffAssembler::VarState* dst_slot,
                             uint32_t local_index) {
    auto& state = *__ cache_state();
    auto& src_slot = state.stack_state.back();
    ValueKind kind = dst_slot->kind();
    if (dst_slot->is_reg()) {
      LiftoffRegister slot_reg = dst_slot->reg();
      if (state.get_use_count(slot_reg) == 1) {
        __ Fill(dst_slot->reg(), src_slot.offset(), kind);
        return;
      }
      state.dec_used(slot_reg);
      dst_slot->MakeStack();
    }
    DCHECK(CheckCompatibleStackSlotTypes(kind, __ local_kind(local_index)));
    RegClass rc = reg_class_for(kind);
    LiftoffRegister dst_reg = __ GetUnusedRegister(rc, {});
    __ Fill(dst_reg, src_slot.offset(), kind);
    *dst_slot = LiftoffAssembler::VarState(kind, dst_reg, dst_slot->offset());
    __ cache_state()->inc_used(dst_reg);
  }

  void LocalSet(uint32_t local_index, bool is_tee) {
    auto& state = *__ cache_state();
    auto& source_slot = state.stack_state.back();
    auto& target_slot = state.stack_state[local_index];
    switch (source_slot.loc()) {
      case kRegister:
        if (target_slot.is_reg()) state.dec_used(target_slot.reg());
        target_slot.Copy(source_slot);
        if (is_tee) state.inc_used(target_slot.reg());
        break;
      case kIntConst:
        if (target_slot.is_reg()) state.dec_used(target_slot.reg());
        target_slot.Copy(source_slot);
        break;
      case kStack:
        LocalSetFromStackSlot(&target_slot, local_index);
        break;
    }
    if (!is_tee) __ cache_state()->stack_state.pop_back();
  }

  void LocalSet(FullDecoder* decoder, const Value& value,
                const IndexImmediate<validate>& imm) {
    LocalSet(imm.index, false);
  }

  void LocalTee(FullDecoder* decoder, const Value& value, Value* result,
                const IndexImmediate<validate>& imm) {
    LocalSet(imm.index, true);
  }

  Register GetGlobalBaseAndOffset(const WasmGlobal* global,
                                  LiftoffRegList* pinned, uint32_t* offset) {
    Register addr = pinned->set(__ GetUnusedRegister(kGpReg, {})).gp();
    if (global->mutability && global->imported) {
      LOAD_TAGGED_PTR_INSTANCE_FIELD(addr, ImportedMutableGlobals, *pinned);
      int field_offset =
          wasm::ObjectAccess::ElementOffsetInTaggedFixedAddressArray(
              global->index);
      __ Load(LiftoffRegister(addr), addr, no_reg, field_offset,
              kPointerLoadType);
      *offset = 0;
    } else {
      LOAD_INSTANCE_FIELD(addr, GlobalsStart, kSystemPointerSize, *pinned);
      *offset = global->offset;
    }
#ifdef V8_ENABLE_SANDBOX
      __ DecodeSandboxedPointer(addr);
#endif
      return addr;
  }

  void GetBaseAndOffsetForImportedMutableExternRefGlobal(
      const WasmGlobal* global, LiftoffRegList* pinned, Register* base,
      Register* offset) {
    Register globals_buffer =
        pinned->set(__ GetUnusedRegister(kGpReg, *pinned)).gp();
    LOAD_TAGGED_PTR_INSTANCE_FIELD(globals_buffer,
                                   ImportedMutableGlobalsBuffers, *pinned);
    *base = globals_buffer;
    __ LoadTaggedPointer(
        *base, globals_buffer, no_reg,
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(global->offset));

    // For the offset we need the index of the global in the buffer, and
    // then calculate the actual offset from the index. Load the index from
    // the ImportedMutableGlobals array of the instance.
    Register imported_mutable_globals =
        pinned->set(__ GetUnusedRegister(kGpReg, *pinned)).gp();

    LOAD_TAGGED_PTR_INSTANCE_FIELD(imported_mutable_globals,
                                   ImportedMutableGlobals, *pinned);
    *offset = imported_mutable_globals;
    int field_offset =
        wasm::ObjectAccess::ElementOffsetInTaggedFixedAddressArray(
            global->index);
    __ Load(LiftoffRegister(*offset), imported_mutable_globals, no_reg,
            field_offset, LoadType::kI32Load);
    __ emit_i32_shli(*offset, *offset, kTaggedSizeLog2);
    __ emit_i32_addi(*offset, *offset,
                     wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(0));
  }

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate<validate>& imm) {
    const auto* global = &env_->module->globals[imm.index];
    ValueKind kind = global->type.kind();
    if (!CheckSupportedType(decoder, kind, "global")) {
      return;
    }

    if (is_reference(kind)) {
      if (global->mutability && global->imported) {
        LiftoffRegList pinned;
        Register base = no_reg;
        Register offset = no_reg;
        GetBaseAndOffsetForImportedMutableExternRefGlobal(global, &pinned,
                                                          &base, &offset);
        __ LoadTaggedPointer(base, base, offset, 0);
        __ PushRegister(kind, LiftoffRegister(base));
        return;
      }

      LiftoffRegList pinned;
      Register globals_buffer =
          pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
      LOAD_TAGGED_PTR_INSTANCE_FIELD(globals_buffer, TaggedGlobalsBuffer,
                                     pinned);
      Register value = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
      __ LoadTaggedPointer(value, globals_buffer, no_reg,
                           wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(
                               imm.global->offset));
      __ PushRegister(kind, LiftoffRegister(value));
      return;
    }
    LiftoffRegList pinned;
    uint32_t offset = 0;
    Register addr = GetGlobalBaseAndOffset(global, &pinned, &offset);
    LiftoffRegister value =
        pinned.set(__ GetUnusedRegister(reg_class_for(kind), pinned));
    LoadType type = LoadType::ForValueKind(kind);
    __ Load(value, addr, no_reg, offset, type, nullptr, false);
    __ PushRegister(kind, value);
  }

  void GlobalSet(FullDecoder* decoder, const Value&,
                 const GlobalIndexImmediate<validate>& imm) {
    auto* global = &env_->module->globals[imm.index];
    ValueKind kind = global->type.kind();
    if (!CheckSupportedType(decoder, kind, "global")) {
      return;
    }

    if (is_reference(kind)) {
      if (global->mutability && global->imported) {
        LiftoffRegList pinned;
        LiftoffRegister value = pinned.set(__ PopToRegister(pinned));
        Register base = no_reg;
        Register offset = no_reg;
        GetBaseAndOffsetForImportedMutableExternRefGlobal(global, &pinned,
                                                          &base, &offset);
        __ StoreTaggedPointer(base, offset, 0, value, pinned);
        return;
      }

      LiftoffRegList pinned;
      Register globals_buffer =
          pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
      LOAD_TAGGED_PTR_INSTANCE_FIELD(globals_buffer, TaggedGlobalsBuffer,
                                     pinned);
      LiftoffRegister value = pinned.set(__ PopToRegister(pinned));
      __ StoreTaggedPointer(globals_buffer, no_reg,
                            wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(
                                imm.global->offset),
                            value, pinned);
      return;
    }
    LiftoffRegList pinned;
    uint32_t offset = 0;
    Register addr = GetGlobalBaseAndOffset(global, &pinned, &offset);
    LiftoffRegister reg = pinned.set(__ PopToRegister(pinned));
    StoreType type = StoreType::ForValueKind(kind);
    __ Store(addr, no_reg, offset, reg, type, {}, nullptr, false);
  }

  void TableGet(FullDecoder* decoder, const Value&, Value*,
                const IndexImmediate<validate>& imm) {
    LiftoffRegList pinned;

    LiftoffRegister table_index_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(table_index_reg, WasmValue(imm.index));
    LiftoffAssembler::VarState table_index(kPointerKind, table_index_reg, 0);

    LiftoffAssembler::VarState index = __ cache_state()->stack_state.back();

    ValueType type = env_->module->tables[imm.index].type;
    bool is_funcref = IsSubtypeOf(type, kWasmFuncRef, env_->module);
    auto stub =
        is_funcref ? WasmCode::kWasmTableGetFuncRef : WasmCode::kWasmTableGet;

    CallRuntimeStub(stub, MakeSig::Returns(type.kind()).Params(kI32, kI32),
                    {table_index, index}, decoder->position());

    // Pop parameters from the value stack.
    __ cache_state()->stack_state.pop_back(1);

    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    __ PushRegister(type.kind(), LiftoffRegister(kReturnRegister0));
  }

  void TableSet(FullDecoder* decoder, const Value&, const Value&,
                const IndexImmediate<validate>& imm) {
    LiftoffRegList pinned;

    LiftoffRegister table_index_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(table_index_reg, WasmValue(imm.index));
    LiftoffAssembler::VarState table_index(kPointerKind, table_index_reg, 0);

    LiftoffAssembler::VarState value = __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState index = __ cache_state()->stack_state.end()[-2];

    ValueType type = env_->module->tables[imm.index].type;
    bool is_funcref = IsSubtypeOf(type, kWasmFuncRef, env_->module);
    auto stub =
        is_funcref ? WasmCode::kWasmTableSetFuncRef : WasmCode::kWasmTableSet;

    CallRuntimeStub(stub, MakeSig::Params(kI32, kI32, type.kind()),
                    {table_index, index, value}, decoder->position());

    // Pop parameters from the value stack.
    __ cache_state()->stack_state.pop_back(2);

    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);
  }

  WasmCode::RuntimeStubId GetRuntimeStubIdForTrapReason(TrapReason reason) {
    switch (reason) {
#define RUNTIME_STUB_FOR_TRAP(trap_reason) \
  case k##trap_reason:                     \
    return WasmCode::kThrowWasm##trap_reason;

      FOREACH_WASM_TRAPREASON(RUNTIME_STUB_FOR_TRAP)
#undef RUNTIME_STUB_FOR_TRAP
      default:
        UNREACHABLE();
    }
  }

  void Trap(FullDecoder* decoder, TrapReason reason) {
    Label* trap_label =
        AddOutOfLineTrap(decoder, GetRuntimeStubIdForTrapReason(reason));
    __ emit_jump(trap_label);
    __ AssertUnreachable(AbortReason::kUnexpectedReturnFromWasmTrap);
  }

  void AssertNull(FullDecoder* decoder, const Value& arg, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister obj = pinned.set(__ PopToRegister(pinned));
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapIllegalCast);
    LiftoffRegister null = __ GetUnusedRegister(kGpReg, pinned);
    LoadNullValue(null.gp(), pinned);
    {
      FREEZE_STATE(trapping);
      __ emit_cond_jump(kUnequal, trap_label, kRefNull, obj.gp(), null.gp(),
                        trapping);
    }
    __ PushRegister(kRefNull, obj);
  }

  void NopForTestingUnsupportedInLiftoff(FullDecoder* decoder) {
    unsupported(decoder, kOtherReason, "testing opcode");
  }

  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    LiftoffRegList pinned;
    Register condition = pinned.set(__ PopToRegister()).gp();
    ValueKind kind = __ cache_state()->stack_state.end()[-1].kind();
    DCHECK(CheckCompatibleStackSlotTypes(
        kind, __ cache_state()->stack_state.end()[-2].kind()));
    LiftoffRegister false_value = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister true_value = __ PopToRegister(pinned);
    LiftoffRegister dst = __ GetUnusedRegister(true_value.reg_class(),
                                               {true_value, false_value}, {});
    if (!__ emit_select(dst, condition, true_value, false_value, kind)) {
      FREEZE_STATE(frozen);
      // Emit generic code (using branches) instead.
      Label cont;
      Label case_false;
      __ emit_cond_jump(kEqual, &case_false, kI32, condition, no_reg, frozen);
      if (dst != true_value) __ Move(dst, true_value, kind);
      __ emit_jump(&cont);

      __ bind(&case_false);
      if (dst != false_value) __ Move(dst, false_value, kind);
      __ bind(&cont);
    }
    __ PushRegister(kind, dst);
  }

  // {tmp1} and {tmp2} may be {no_reg} if it is guaranteed that {target}
  // isn't a loop.
  void BrImpl(FullDecoder* decoder, Control* target, Register tmp1,
              Register tmp2) {
    if (dynamic_tiering()) {
      if (target->is_loop()) {
        DCHECK(target->label.get()->is_bound());
        DCHECK_NE(tmp1, no_reg);
        DCHECK_NE(tmp2, no_reg);
        int jump_distance = __ pc_offset() - target->label.get()->pos();
        // For now we just add one as the cost for the tier up check. We might
        // want to revisit this when tuning tiering budgets later.
        const int kTierUpCheckCost = 1;
        TierupCheck(decoder, decoder->position(),
                    jump_distance + kTierUpCheckCost, tmp1, tmp2);
      } else {
        // To estimate time spent in this function more accurately, we could
        // increment the tiering budget on forward jumps. However, we don't
        // know the jump distance yet; using a blanket value has been tried
        // and found to not make a difference.
      }
    }
    if (!target->br_merge()->reached) {
      target->label_state.InitMerge(
          *__ cache_state(), __ num_locals(), target->br_merge()->arity,
          target->stack_depth + target->num_exceptions);
    }
    __ MergeStackWith(target->label_state, target->br_merge()->arity,
                      target->is_loop() ? LiftoffAssembler::kBackwardJump
                                        : LiftoffAssembler::kForwardJump);
    __ jmp(target->label.get());
  }

  bool NeedsTierupCheck(FullDecoder* decoder, uint32_t br_depth) {
    if (!dynamic_tiering()) return false;
    return br_depth == decoder->control_depth() - 1 ||
           decoder->control_at(br_depth)->is_loop();
  }

  struct TierupTempRegisters {
    Register tmp1 = no_reg;
    Register tmp2 = no_reg;
  };
  void AllocateTempRegisters(TierupTempRegisters& temps) {
    LiftoffRegList pinned;
    temps.tmp1 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    temps.tmp2 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
  }

  void BrOrRet(FullDecoder* decoder, uint32_t depth,
               uint32_t /* drop_values */) {
    TierupTempRegisters temps;
    if (NeedsTierupCheck(decoder, depth)) AllocateTempRegisters(temps);
    BrOrRetImpl(decoder, depth, temps.tmp1, temps.tmp2);
  }

  void BrOrRetImpl(FullDecoder* decoder, uint32_t depth, Register tmp1,
                   Register tmp2) {
    if (depth == decoder->control_depth() - 1) {
      ReturnImpl(decoder, tmp1, tmp2);
    } else {
      BrImpl(decoder, decoder->control_at(depth), tmp1, tmp2);
    }
  }

  void BrIf(FullDecoder* decoder, const Value& /* cond */, uint32_t depth) {
    // Avoid having sequences of branches do duplicate work.
    if (depth != decoder->control_depth() - 1) {
      __ PrepareForBranch(decoder->control_at(depth)->br_merge()->arity, {});
    }

    Label cont_false;
    TierupTempRegisters temps;
    if (NeedsTierupCheck(decoder, depth)) AllocateTempRegisters(temps);

    // Test the condition on the value stack, jump to {cont_false} if zero.
    std::unique_ptr<FreezeCacheState> frozen;
    JumpIfFalse(decoder, &cont_false, frozen);

    BrOrRetImpl(decoder, depth, temps.tmp1, temps.tmp2);

    __ bind(&cont_false);
  }

  // Generate a branch table case, potentially reusing previously generated
  // stack transfer code.
  void GenerateBrCase(FullDecoder* decoder, uint32_t br_depth,
                      std::map<uint32_t, MovableLabel>* br_targets,
                      Register tmp1, Register tmp2) {
    MovableLabel& label = (*br_targets)[br_depth];
    if (label.get()->is_bound()) {
      __ jmp(label.get());
    } else {
      __ bind(label.get());
      BrOrRetImpl(decoder, br_depth, tmp1, tmp2);
    }
  }

  // Generate a branch table for input in [min, max).
  // TODO(wasm): Generate a real branch table (like TF TableSwitch).
  void GenerateBrTable(FullDecoder* decoder, LiftoffRegister tmp,
                       LiftoffRegister value, uint32_t min, uint32_t max,
                       BranchTableIterator<validate>* table_iterator,
                       std::map<uint32_t, MovableLabel>* br_targets,
                       Register tmp1, Register tmp2,
                       const FreezeCacheState& frozen) {
    DCHECK_LT(min, max);
    // Check base case.
    if (max == min + 1) {
      DCHECK_EQ(min, table_iterator->cur_index());
      GenerateBrCase(decoder, table_iterator->next(), br_targets, tmp1, tmp2);
      return;
    }

    uint32_t split = min + (max - min) / 2;
    Label upper_half;
    __ LoadConstant(tmp, WasmValue(split));
    __ emit_cond_jump(kUnsignedGreaterEqual, &upper_half, kI32, value.gp(),
                      tmp.gp(), frozen);
    // Emit br table for lower half:
    GenerateBrTable(decoder, tmp, value, min, split, table_iterator, br_targets,
                    tmp1, tmp2, frozen);
    __ bind(&upper_half);
    // table_iterator will trigger a DCHECK if we don't stop decoding now.
    if (did_bailout()) return;
    // Emit br table for upper half:
    GenerateBrTable(decoder, tmp, value, split, max, table_iterator, br_targets,
                    tmp1, tmp2, frozen);
  }

  void BrTable(FullDecoder* decoder, const BranchTableImmediate<validate>& imm,
               const Value& key) {
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());

    // Reserve temp registers if any of the table entries will do a tierup
    // check (function exit, or loop back edge).
    Register tmp1 = no_reg;
    Register tmp2 = no_reg;
    if (dynamic_tiering()) {
      bool need_temps = false;
      BranchTableIterator<validate> table_iterator(decoder, imm);
      while (table_iterator.has_next()) {
        uint32_t depth = table_iterator.next();
        if (depth == decoder->control_depth() - 1 ||
            decoder->control_at(depth)->is_loop()) {
          need_temps = true;
          break;
        }
      }
      if (need_temps) {
        tmp1 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
        tmp2 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
      }
    }

    {
      // All targets must have the same arity (checked by validation), so
      // we can just sample any of them to find that arity.
      uint32_t ignored_length;
      uint32_t sample_depth = decoder->read_u32v<Decoder::kNoValidation>(
          imm.table, &ignored_length, "first depth");
      __ PrepareForBranch(decoder->control_at(sample_depth)->br_merge()->arity,
                          pinned);
    }

    BranchTableIterator<validate> table_iterator(decoder, imm);
    std::map<uint32_t, MovableLabel> br_targets;

    if (imm.table_count > 0) {
      LiftoffRegister tmp = __ GetUnusedRegister(kGpReg, pinned);
      __ LoadConstant(tmp, WasmValue(uint32_t{imm.table_count}));
      FREEZE_STATE(frozen);
      Label case_default;
      __ emit_cond_jump(kUnsignedGreaterEqual, &case_default, kI32, value.gp(),
                        tmp.gp(), frozen);

      GenerateBrTable(decoder, tmp, value, 0, imm.table_count, &table_iterator,
                      &br_targets, tmp1, tmp2, frozen);

      __ bind(&case_default);
      // table_iterator will trigger a DCHECK if we don't stop decoding now.
      if (did_bailout()) return;
    }

    // Generate the default case.
    GenerateBrCase(decoder, table_iterator.next(), &br_targets, tmp1, tmp2);
    DCHECK(!table_iterator.has_next());
  }

  void Else(FullDecoder* decoder, Control* c) {
    if (c->reachable()) {
      if (!c->end_merge.reached) {
        c->label_state.InitMerge(*__ cache_state(), __ num_locals(),
                                 c->end_merge.arity,
                                 c->stack_depth + c->num_exceptions);
      }
      __ MergeFullStackWith(c->label_state, *__ cache_state());
      __ emit_jump(c->label.get());
    }
    __ bind(c->else_state->label.get());
    __ cache_state()->Steal(c->else_state->state);
  }

  SpilledRegistersForInspection* GetSpilledRegistersForInspection() {
    DCHECK(for_debugging_);
    // If we are generating debugging code, we really need to spill all
    // registers to make them inspectable when stopping at the trap.
    auto* spilled = compilation_zone_->New<SpilledRegistersForInspection>(
        compilation_zone_);
    for (uint32_t i = 0, e = __ cache_state()->stack_height(); i < e; ++i) {
      auto& slot = __ cache_state()->stack_state[i];
      if (!slot.is_reg()) continue;
      spilled->entries.push_back(SpilledRegistersForInspection::Entry{
          slot.offset(), slot.reg(), slot.kind()});
      __ RecordUsedSpillOffset(slot.offset());
    }
    return spilled;
  }

  Label* AddOutOfLineTrap(FullDecoder* decoder, WasmCode::RuntimeStubId stub,
                          uint32_t pc = 0) {
    // Only memory OOB traps need a {pc}.
    DCHECK_IMPLIES(stub != WasmCode::kThrowWasmTrapMemOutOfBounds, pc == 0);
    DCHECK(v8_flags.wasm_bounds_checks);
    OutOfLineSafepointInfo* safepoint_info = nullptr;
    if (V8_UNLIKELY(for_debugging_)) {
      // Execution does not return after a trap. Therefore we don't have to
      // define a safepoint for traps that would preserve references on the
      // stack. However, if this is debug code, then we have to preserve the
      // references so that they can be inspected.
      safepoint_info =
          compilation_zone_->New<OutOfLineSafepointInfo>(compilation_zone_);
      __ cache_state()->GetTaggedSlotsForOOLCode(
          &safepoint_info->slots, &safepoint_info->spills,
          LiftoffAssembler::CacheState::SpillLocation::kStackSlots);
    }
    out_of_line_code_.push_back(OutOfLineCode::Trap(
        stub, decoder->position(),
        V8_UNLIKELY(for_debugging_) ? GetSpilledRegistersForInspection()
                                    : nullptr,
        safepoint_info, pc, RegisterOOLDebugSideTableEntry(decoder)));
    return out_of_line_code_.back().label.get();
  }

  enum ForceCheck : bool { kDoForceCheck = true, kDontForceCheck = false };

  // Returns {no_reg} if the memory access is statically known to be out of
  // bounds (a jump to the trap was generated then); return the GP {index}
  // register otherwise (holding the ptrsized index).
  Register BoundsCheckMem(FullDecoder* decoder, uint32_t access_size,
                          uint64_t offset, LiftoffRegister index,
                          LiftoffRegList pinned, ForceCheck force_check) {
    const bool statically_oob =
        !base::IsInBounds<uintptr_t>(offset, access_size,
                                     env_->max_memory_size);

    // After bounds checking, we know that the index must be ptrsize, hence only
    // look at the lower word on 32-bit systems (the high word is bounds-checked
    // further down).
    Register index_ptrsize =
        kNeedI64RegPair && index.is_gp_pair() ? index.low_gp() : index.gp();

    // Without bounds checks (testing only), just return the ptrsize index.
    if (V8_UNLIKELY(env_->bounds_checks == kNoBoundsChecks)) {
      return index_ptrsize;
    }

    // Early return for trap handler.
    DCHECK_IMPLIES(env_->module->is_memory64,
                   env_->bounds_checks == kExplicitBoundsChecks);
    if (!force_check && !statically_oob &&
        env_->bounds_checks == kTrapHandler) {
      // With trap handlers we should not have a register pair as input (we
      // would only return the lower half).
      DCHECK(index.is_gp());
      return index_ptrsize;
    }

    CODE_COMMENT("bounds check memory");

    // Set {pc} of the OOL code to {0} to avoid generation of protected
    // instruction information (see {GenerateOutOfLineCode}.
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapMemOutOfBounds, 0);

    if (V8_UNLIKELY(statically_oob)) {
      __ emit_jump(trap_label);
      decoder->SetSucceedingCodeDynamicallyUnreachable();
      return no_reg;
    }

    // Convert the index to ptrsize, bounds-checking the high word on 32-bit
    // systems for memory64.
    if (!env_->module->is_memory64) {
      __ emit_u32_to_uintptr(index_ptrsize, index_ptrsize);
    } else if (kSystemPointerSize == kInt32Size) {
      DCHECK_GE(kMaxUInt32, env_->max_memory_size);
      FREEZE_STATE(trapping);
      __ emit_cond_jump(kNotEqualZero, trap_label, kI32, index.high_gp(),
                        no_reg, trapping);
    }

    uintptr_t end_offset = offset + access_size - 1u;

    pinned.set(index_ptrsize);
    LiftoffRegister end_offset_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister mem_size = __ GetUnusedRegister(kGpReg, pinned);
    LOAD_INSTANCE_FIELD(mem_size.gp(), MemorySize, kSystemPointerSize, pinned);

    __ LoadConstant(end_offset_reg, WasmValue::ForUintPtr(end_offset));

    FREEZE_STATE(trapping);
    // If the end offset is larger than the smallest memory, dynamically check
    // the end offset against the actual memory size, which is not known at
    // compile time. Otherwise, only one check is required (see below).
    if (end_offset > env_->min_memory_size) {
      __ emit_cond_jump(kUnsignedGreaterEqual, trap_label, kPointerKind,
                        end_offset_reg.gp(), mem_size.gp(), trapping);
    }

    // Just reuse the end_offset register for computing the effective size
    // (which is >= 0 because of the check above).
    LiftoffRegister effective_size_reg = end_offset_reg;
    __ emit_ptrsize_sub(effective_size_reg.gp(), mem_size.gp(),
                        end_offset_reg.gp());

    __ emit_cond_jump(kUnsignedGreaterEqual, trap_label, kPointerKind,
                      index_ptrsize, effective_size_reg.gp(), trapping);
    return index_ptrsize;
  }

  void AlignmentCheckMem(FullDecoder* decoder, uint32_t access_size,
                         uintptr_t offset, Register index,
                         LiftoffRegList pinned) {
    CODE_COMMENT("alignment check");
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapUnalignedAccess, 0);
    Register address = __ GetUnusedRegister(kGpReg, pinned).gp();

    FREEZE_STATE(trapping);
    const uint32_t align_mask = access_size - 1;
    if ((offset & align_mask) == 0) {
      // If {offset} is aligned, we can produce faster code.

      // TODO(ahaas): On Intel, the "test" instruction implicitly computes the
      // AND of two operands. We could introduce a new variant of
      // {emit_cond_jump} to use the "test" instruction without the "and" here.
      // Then we can also avoid using the temp register here.
      __ emit_i32_andi(address, index, align_mask);
      __ emit_cond_jump(kUnequal, trap_label, kI32, address, no_reg, trapping);
    } else {
      // For alignment checks we only look at the lower 32-bits in {offset}.
      __ emit_i32_addi(address, index, static_cast<uint32_t>(offset));
      __ emit_i32_andi(address, address, align_mask);
      __ emit_cond_jump(kUnequal, trap_label, kI32, address, no_reg, trapping);
    }
  }

  void TraceMemoryOperation(bool is_store, MachineRepresentation rep,
                            Register index, uintptr_t offset,
                            WasmCodePosition position) {
    // Before making the runtime call, spill all cache registers.
    __ SpillAllRegisters();

    LiftoffRegList pinned;
    if (index != no_reg) pinned.set(index);
    // Get one register for computing the effective offset (offset + index).
    LiftoffRegister effective_offset =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    DCHECK_GE(kMaxUInt32, offset);
    __ LoadConstant(effective_offset, WasmValue(static_cast<uint32_t>(offset)));
    if (index != no_reg) {
      // TODO(clemensb): Do a 64-bit addition here if memory64 is used.
      __ emit_i32_add(effective_offset.gp(), effective_offset.gp(), index);
    }

    // Get a register to hold the stack slot for MemoryTracingInfo.
    LiftoffRegister info = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    // Allocate stack slot for MemoryTracingInfo.
    __ AllocateStackSlot(info.gp(), sizeof(MemoryTracingInfo));

    // Reuse the {effective_offset} register for all information to be stored in
    // the MemoryTracingInfo struct.
    LiftoffRegister data = effective_offset;

    // Now store all information into the MemoryTracingInfo struct.
    if (kSystemPointerSize == 8) {
      // Zero-extend the effective offset to u64.
      CHECK(__ emit_type_conversion(kExprI64UConvertI32, data, effective_offset,
                                    nullptr));
    }
    __ Store(
        info.gp(), no_reg, offsetof(MemoryTracingInfo, offset), data,
        kSystemPointerSize == 8 ? StoreType::kI64Store : StoreType::kI32Store,
        pinned);
    __ LoadConstant(data, WasmValue(is_store ? 1 : 0));
    __ Store(info.gp(), no_reg, offsetof(MemoryTracingInfo, is_store), data,
             StoreType::kI32Store8, pinned);
    __ LoadConstant(data, WasmValue(static_cast<int>(rep)));
    __ Store(info.gp(), no_reg, offsetof(MemoryTracingInfo, mem_rep), data,
             StoreType::kI32Store8, pinned);

    WasmTraceMemoryDescriptor descriptor;
    DCHECK_EQ(0, descriptor.GetStackParameterCount());
    DCHECK_EQ(1, descriptor.GetRegisterParameterCount());
    Register param_reg = descriptor.GetRegisterParameter(0);
    if (info.gp() != param_reg) {
      __ Move(param_reg, info.gp(), kPointerKind);
    }

    source_position_table_builder_.AddPosition(__ pc_offset(),
                                               SourcePosition(position), false);
    __ CallRuntimeStub(WasmCode::kWasmTraceMemory);
    DefineSafepoint();

    __ DeallocateStackSlot(sizeof(MemoryTracingInfo));
  }

  bool IndexStaticallyInBounds(const LiftoffAssembler::VarState& index_slot,
                               int access_size, uintptr_t* offset) {
    if (!index_slot.is_const()) return false;

    // Potentially zero extend index (which is a 32-bit constant).
    const uintptr_t index = static_cast<uint32_t>(index_slot.i32_const());
    const uintptr_t effective_offset = index + *offset;

    if (effective_offset < index  // overflow
        || !base::IsInBounds<uintptr_t>(effective_offset, access_size,
                                        env_->min_memory_size)) {
      return false;
    }

    *offset = effective_offset;
    return true;
  }

  Register GetMemoryStart(LiftoffRegList pinned) {
    Register memory_start = __ cache_state()->cached_mem_start;
    if (memory_start == no_reg) {
      memory_start = __ GetUnusedRegister(kGpReg, pinned).gp();
      LOAD_INSTANCE_FIELD(memory_start, MemoryStart, kSystemPointerSize,
                          pinned);
#ifdef V8_ENABLE_SANDBOX
      __ DecodeSandboxedPointer(memory_start);
#endif
      __ cache_state()->SetMemStartCacheRegister(memory_start);
    }
    return memory_start;
  }

  void LoadMem(FullDecoder* decoder, LoadType type,
               const MemoryAccessImmediate<validate>& imm,
               const Value& index_val, Value* result) {
    ValueKind kind = type.value_type().kind();
    DCHECK_EQ(kind, result->type.kind());
    if (!CheckSupportedType(decoder, kind, "load")) return;

    uintptr_t offset = imm.offset;
    Register index = no_reg;
    RegClass rc = reg_class_for(kind);

    // Only look at the slot, do not pop it yet (will happen in PopToRegister
    // below, if this is not a statically-in-bounds index).
    auto& index_slot = __ cache_state()->stack_state.back();
    DCHECK_EQ(index_val.type.kind(), index_slot.kind());
    DCHECK(index_slot.kind() == kI32 || index_slot.kind() == kI64);
    bool i64_offset = index_slot.kind() == kI64;
    if (IndexStaticallyInBounds(index_slot, type.size(), &offset)) {
      __ cache_state()->stack_state.pop_back();
      CODE_COMMENT("load from memory (constant offset)");
      LiftoffRegList pinned;
      Register mem = pinned.set(GetMemoryStart(pinned));
      LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));
      __ Load(value, mem, no_reg, offset, type, nullptr, true, i64_offset);
      __ PushRegister(kind, value);
    } else {
      LiftoffRegister full_index = __ PopToRegister();
      index = BoundsCheckMem(decoder, type.size(), offset, full_index, {},
                             kDontForceCheck);
      if (index == no_reg) return;

      CODE_COMMENT("load from memory");
      LiftoffRegList pinned{index};

      // Load the memory start address only now to reduce register pressure
      // (important on ia32).
      Register mem = pinned.set(GetMemoryStart(pinned));
      LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));

      uint32_t protected_load_pc = 0;
      __ Load(value, mem, index, offset, type, &protected_load_pc, true,
              i64_offset);
      if (env_->bounds_checks == kTrapHandler) {
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapMemOutOfBounds,
                         protected_load_pc);
      }
      __ PushRegister(kind, value);
    }

    if (V8_UNLIKELY(v8_flags.trace_wasm_memory)) {
      TraceMemoryOperation(false, type.mem_type().representation(), index,
                           offset, decoder->position());
    }
  }

  void LoadTransform(FullDecoder* decoder, LoadType type,
                     LoadTransformationKind transform,
                     const MemoryAccessImmediate<validate>& imm,
                     const Value& index_val, Value* result) {
    // LoadTransform requires SIMD support, so check for it here. If
    // unsupported, bailout and let TurboFan lower the code.
    if (!CheckSupportedType(decoder, kS128, "LoadTransform")) {
      return;
    }

    LiftoffRegister full_index = __ PopToRegister();
    // For load splats and load zero, LoadType is the size of the load, and for
    // load extends, LoadType is the size of the lane, and it always loads 8
    // bytes.
    uint32_t access_size =
        transform == LoadTransformationKind::kExtend ? 8 : type.size();
    Register index = BoundsCheckMem(decoder, access_size, imm.offset,
                                    full_index, {}, kDontForceCheck);
    if (index == no_reg) return;

    uintptr_t offset = imm.offset;
    LiftoffRegList pinned{index};
    CODE_COMMENT("load with transformation");
    Register addr = GetMemoryStart(pinned);
    LiftoffRegister value = __ GetUnusedRegister(reg_class_for(kS128), {});
    uint32_t protected_load_pc = 0;
    __ LoadTransform(value, addr, index, offset, type, transform,
                     &protected_load_pc);

    if (env_->bounds_checks == kTrapHandler) {
      AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapMemOutOfBounds,
                       protected_load_pc);
    }
    __ PushRegister(kS128, value);

    if (V8_UNLIKELY(v8_flags.trace_wasm_memory)) {
      // Again load extend is different.
      MachineRepresentation mem_rep =
          transform == LoadTransformationKind::kExtend
              ? MachineRepresentation::kWord64
              : type.mem_type().representation();
      TraceMemoryOperation(false, mem_rep, index, offset, decoder->position());
    }
  }

  void LoadLane(FullDecoder* decoder, LoadType type, const Value& _value,
                const Value& _index, const MemoryAccessImmediate<validate>& imm,
                const uint8_t laneidx, Value* _result) {
    if (!CheckSupportedType(decoder, kS128, "LoadLane")) {
      return;
    }

    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    LiftoffRegister full_index = __ PopToRegister();
    Register index = BoundsCheckMem(decoder, type.size(), imm.offset,
                                    full_index, pinned, kDontForceCheck);
    if (index == no_reg) return;

    uintptr_t offset = imm.offset;
    pinned.set(index);
    CODE_COMMENT("load lane");
    Register addr = GetMemoryStart(pinned);
    LiftoffRegister result = __ GetUnusedRegister(reg_class_for(kS128), {});
    uint32_t protected_load_pc = 0;

    __ LoadLane(result, value, addr, index, offset, type, laneidx,
                &protected_load_pc);
    if (env_->bounds_checks == kTrapHandler) {
      AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapMemOutOfBounds,
                       protected_load_pc);
    }

    __ PushRegister(kS128, result);

    if (V8_UNLIKELY(v8_flags.trace_wasm_memory)) {
      TraceMemoryOperation(false, type.mem_type().representation(), index,
                           offset, decoder->position());
    }
  }

  void StoreMem(FullDecoder* decoder, StoreType type,
                const MemoryAccessImmediate<validate>& imm,
                const Value& index_val, const Value& value_val) {
    ValueKind kind = type.value_type().kind();
    DCHECK_EQ(kind, value_val.type.kind());
    if (!CheckSupportedType(decoder, kind, "store")) return;

    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());

    uintptr_t offset = imm.offset;
    Register index = no_reg;

    auto& index_slot = __ cache_state()->stack_state.back();
    DCHECK_EQ(index_val.type.kind(), index_slot.kind());
    DCHECK(index_slot.kind() == kI32 || index_slot.kind() == kI64);
    bool i64_offset = index_slot.kind() == kI64;
    if (IndexStaticallyInBounds(index_slot, type.size(), &offset)) {
      __ cache_state()->stack_state.pop_back();
      CODE_COMMENT("store to memory (constant offset)");
      Register mem = pinned.set(GetMemoryStart(pinned));
      __ Store(mem, no_reg, offset, value, type, pinned, nullptr, true,
               i64_offset);
    } else {
      LiftoffRegister full_index = __ PopToRegister(pinned);
      index = BoundsCheckMem(decoder, type.size(), imm.offset, full_index,
                             pinned, kDontForceCheck);
      if (index == no_reg) return;

      pinned.set(index);
      CODE_COMMENT("store to memory");
      uint32_t protected_store_pc = 0;
      // Load the memory start address only now to reduce register pressure
      // (important on ia32).
      Register mem = pinned.set(GetMemoryStart(pinned));
      LiftoffRegList outer_pinned;
      if (V8_UNLIKELY(v8_flags.trace_wasm_memory)) outer_pinned.set(index);
      __ Store(mem, index, offset, value, type, outer_pinned,
               &protected_store_pc, true, i64_offset);
      if (env_->bounds_checks == kTrapHandler) {
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapMemOutOfBounds,
                         protected_store_pc);
      }
    }

    if (V8_UNLIKELY(v8_flags.trace_wasm_memory)) {
      TraceMemoryOperation(true, type.mem_rep(), index, offset,
                           decoder->position());
    }
  }

  void StoreLane(FullDecoder* decoder, StoreType type,
                 const MemoryAccessImmediate<validate>& imm,
                 const Value& _index, const Value& _value, const uint8_t lane) {
    if (!CheckSupportedType(decoder, kS128, "StoreLane")) return;
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    LiftoffRegister full_index = __ PopToRegister(pinned);
    Register index = BoundsCheckMem(decoder, type.size(), imm.offset,
                                    full_index, pinned, kDontForceCheck);
    if (index == no_reg) return;

    uintptr_t offset = imm.offset;
    pinned.set(index);
    CODE_COMMENT("store lane to memory");
    Register addr = pinned.set(GetMemoryStart(pinned));
    uint32_t protected_store_pc = 0;
    __ StoreLane(addr, index, offset, value, type, lane, &protected_store_pc);
    if (env_->bounds_checks == kTrapHandler) {
      AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapMemOutOfBounds,
                       protected_store_pc);
    }
    if (V8_UNLIKELY(v8_flags.trace_wasm_memory)) {
      TraceMemoryOperation(true, type.mem_rep(), index, offset,
                           decoder->position());
    }
  }

  void CurrentMemoryPages(FullDecoder* /* decoder */, Value* /* result */) {
    Register mem_size = __ GetUnusedRegister(kGpReg, {}).gp();
    LOAD_INSTANCE_FIELD(mem_size, MemorySize, kSystemPointerSize, {});
    __ emit_ptrsize_shri(mem_size, mem_size, kWasmPageSizeLog2);
    LiftoffRegister result{mem_size};
    if (env_->module->is_memory64 && kNeedI64RegPair) {
      LiftoffRegister high_word =
          __ GetUnusedRegister(kGpReg, LiftoffRegList{mem_size});
      // The high word is always 0 on 32-bit systems.
      __ LoadConstant(high_word, WasmValue{uint32_t{0}});
      result = LiftoffRegister::ForPair(mem_size, high_word.gp());
    }
    __ PushRegister(env_->module->is_memory64 ? kI64 : kI32, result);
  }

  void MemoryGrow(FullDecoder* decoder, const Value& value, Value* result_val) {
    // Pop the input, then spill all cache registers to make the runtime call.
    LiftoffRegList pinned;
    LiftoffRegister input = pinned.set(__ PopToRegister());
    __ SpillAllRegisters();

    LiftoffRegister result = pinned.set(__ GetUnusedRegister(kGpReg, pinned));

    Label done;

    if (env_->module->is_memory64) {
      // If the high word is not 0, this will always fail (would grow by
      // >=256TB). The int32_t value will be sign-extended below.
      __ LoadConstant(result, WasmValue(int32_t{-1}));
      if (kNeedI64RegPair) {
        FREEZE_STATE(all_spilled_anyway);
        __ emit_cond_jump(kUnequal, &done, kI32, input.high_gp(), no_reg,
                          all_spilled_anyway);
        input = input.low();
      } else {
        LiftoffRegister high_word = __ GetUnusedRegister(kGpReg, pinned);
        __ emit_i64_shri(high_word, input, 32);
        FREEZE_STATE(all_spilled_anyway);
        __ emit_cond_jump(kUnequal, &done, kI32, high_word.gp(), no_reg,
                          all_spilled_anyway);
      }
    }

    WasmMemoryGrowDescriptor descriptor;
    DCHECK_EQ(0, descriptor.GetStackParameterCount());
    DCHECK_EQ(1, descriptor.GetRegisterParameterCount());
    DCHECK_EQ(machine_type(kI32), descriptor.GetParameterType(0));

    Register param_reg = descriptor.GetRegisterParameter(0);
    if (input.gp() != param_reg) __ Move(param_reg, input.gp(), kI32);

    __ CallRuntimeStub(WasmCode::kWasmMemoryGrow);
    DefineSafepoint();
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    if (kReturnRegister0 != result.gp()) {
      __ Move(result.gp(), kReturnRegister0, kI32);
    }

    __ bind(&done);

    if (env_->module->is_memory64) {
      LiftoffRegister result64 = result;
      if (kNeedI64RegPair) result64 = __ GetUnusedRegister(kGpRegPair, pinned);
      __ emit_type_conversion(kExprI64SConvertI32, result64, result, nullptr);
      __ PushRegister(kI64, result64);
    } else {
      __ PushRegister(kI32, result);
    }
  }

  base::OwnedVector<DebugSideTable::Entry::Value>
  GetCurrentDebugSideTableEntries(
      FullDecoder* decoder,
      DebugSideTableBuilder::AssumeSpilling assume_spilling) {
    auto& stack_state = __ cache_state()->stack_state;
    auto values =
        base::OwnedVector<DebugSideTable::Entry::Value>::NewForOverwrite(
            stack_state.size());

    // For function calls, the decoder still has the arguments on the stack, but
    // Liftoff already popped them. Hence {decoder->stack_size()} can be bigger
    // than expected. Just ignore that and use the lower part only.
    DCHECK_LE(stack_state.size() - num_exceptions_,
              decoder->num_locals() + decoder->stack_size());
    int index = 0;
    int decoder_stack_index = decoder->stack_size();
    // Iterate the operand stack control block by control block, so that we can
    // handle the implicit exception value for try blocks.
    for (int j = decoder->control_depth() - 1; j >= 0; j--) {
      Control* control = decoder->control_at(j);
      Control* next_control = j > 0 ? decoder->control_at(j - 1) : nullptr;
      int end_index = next_control
                          ? next_control->stack_depth + __ num_locals() +
                                next_control->num_exceptions
                          : __ cache_state()->stack_height();
      bool exception = control->is_try_catch() || control->is_try_catchall();
      for (; index < end_index; ++index) {
        auto& slot = stack_state[index];
        auto& value = values[index];
        value.index = index;
        ValueType type =
            index < static_cast<int>(__ num_locals())
                ? decoder->local_type(index)
            : exception ? ValueType::Ref(HeapType::kAny)
                        : decoder->stack_value(decoder_stack_index--)->type;
        DCHECK(CheckCompatibleStackSlotTypes(slot.kind(), type.kind()));
        value.type = type;
        switch (slot.loc()) {
          case kIntConst:
            value.storage = DebugSideTable::Entry::kConstant;
            value.i32_const = slot.i32_const();
            break;
          case kRegister:
            DCHECK_NE(DebugSideTableBuilder::kDidSpill, assume_spilling);
            if (assume_spilling == DebugSideTableBuilder::kAllowRegisters) {
              value.storage = DebugSideTable::Entry::kRegister;
              value.reg_code = slot.reg().liftoff_code();
              break;
            }
            DCHECK_EQ(DebugSideTableBuilder::kAssumeSpilling, assume_spilling);
            V8_FALLTHROUGH;
          case kStack:
            value.storage = DebugSideTable::Entry::kStack;
            value.stack_offset = slot.offset();
            break;
        }
        exception = false;
      }
    }
    DCHECK_EQ(values.size(), index);
    return values;
  }

  // Call this after emitting a runtime call that can show up in a stack trace
  // (e.g. because it can trap).
  void RegisterDebugSideTableEntry(
      FullDecoder* decoder,
      DebugSideTableBuilder::AssumeSpilling assume_spilling) {
    if (V8_LIKELY(!debug_sidetable_builder_)) return;
    debug_sidetable_builder_->NewEntry(
        __ pc_offset(),
        GetCurrentDebugSideTableEntries(decoder, assume_spilling).as_vector());
  }

  DebugSideTableBuilder::EntryBuilder* RegisterOOLDebugSideTableEntry(
      FullDecoder* decoder) {
    if (V8_LIKELY(!debug_sidetable_builder_)) return nullptr;
    return debug_sidetable_builder_->NewOOLEntry(
        GetCurrentDebugSideTableEntries(decoder,
                                        DebugSideTableBuilder::kAssumeSpilling)
            .as_vector());
  }

  enum TailCall : bool { kTailCall = true, kNoTailCall = false };

  void CallDirect(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[], Value[]) {
    CallDirect(decoder, imm, args, nullptr, kNoTailCall);
  }

  void CallIndirect(FullDecoder* decoder, const Value& index_val,
                    const CallIndirectImmediate<validate>& imm,
                    const Value args[], Value returns[]) {
    CallIndirect(decoder, index_val, imm, kNoTailCall);
  }

  void CallRef(FullDecoder* decoder, const Value& func_ref,
               const FunctionSig* sig, uint32_t sig_index, const Value args[],
               Value returns[]) {
    CallRef(decoder, func_ref.type, sig, kNoTailCall);
  }

  void ReturnCall(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[]) {
    TierupCheckOnTailCall(decoder);
    CallDirect(decoder, imm, args, nullptr, kTailCall);
  }

  void ReturnCallIndirect(FullDecoder* decoder, const Value& index_val,
                          const CallIndirectImmediate<validate>& imm,
                          const Value args[]) {
    TierupCheckOnTailCall(decoder);
    CallIndirect(decoder, index_val, imm, kTailCall);
  }

  void ReturnCallRef(FullDecoder* decoder, const Value& func_ref,
                     const FunctionSig* sig, uint32_t sig_index,
                     const Value args[]) {
    TierupCheckOnTailCall(decoder);
    CallRef(decoder, func_ref.type, sig, kTailCall);
  }

  void BrOnNull(FullDecoder* decoder, const Value& ref_object, uint32_t depth,
                bool pass_null_along_branch,
                Value* /* result_on_fallthrough */) {
    // Avoid having sequences of branches do duplicate work.
    if (depth != decoder->control_depth() - 1) {
      __ PrepareForBranch(decoder->control_at(depth)->br_merge()->arity, {});
    }

    Label cont_false;
    LiftoffRegList pinned;
    LiftoffRegister ref =
        pinned.set(pass_null_along_branch ? __ PeekToRegister(0, pinned)
                                          : __ PopToRegister(pinned));
    Register null = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register tmp = NeedsTierupCheck(decoder, depth)
                       ? pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp()
                       : no_reg;
    LoadNullValue(null, pinned);
    {
      FREEZE_STATE(frozen);
      __ emit_cond_jump(kUnequal, &cont_false, ref_object.type.kind(), ref.gp(),
                        null, frozen);
      BrOrRetImpl(decoder, depth, null, tmp);
    }
    __ bind(&cont_false);
    if (!pass_null_along_branch) {
      // We popped the value earlier, must push it back now.
      __ PushRegister(kRef, ref);
    }
  }

  void BrOnNonNull(FullDecoder* decoder, const Value& ref_object,
                   Value* /* result */, uint32_t depth,
                   bool drop_null_on_fallthrough) {
    // Avoid having sequences of branches do duplicate work.
    if (depth != decoder->control_depth() - 1) {
      __ PrepareForBranch(decoder->control_at(depth)->br_merge()->arity, {});
    }

    Label cont_false;
    LiftoffRegList pinned;
    LiftoffRegister ref = pinned.set(__ PeekToRegister(0, pinned));

    Register null = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register tmp = NeedsTierupCheck(decoder, depth)
                       ? pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp()
                       : no_reg;
    LoadNullValue(null, pinned);
    {
      FREEZE_STATE(frozen);
      __ emit_cond_jump(kEqual, &cont_false, ref_object.type.kind(), ref.gp(),
                        null, frozen);

      BrOrRetImpl(decoder, depth, null, tmp);
    }
    // Drop the reference if we are not branching.
    if (drop_null_on_fallthrough) __ DropValues(1);
    __ bind(&cont_false);
  }

  template <ValueKind src_kind, ValueKind result_kind,
            ValueKind result_lane_kind = kVoid, typename EmitFn>
  void EmitTerOp(EmitFn fn, LiftoffRegister dst, LiftoffRegister src1,
                 LiftoffRegister src2, LiftoffRegister src3) {
    CallEmitFn(fn, dst, src1, src2, src3);
    if (V8_UNLIKELY(nondeterminism_)) {
      LiftoffRegList pinned{dst};
      if (result_kind == ValueKind::kF32 || result_kind == ValueKind::kF64) {
        CheckNan(dst, pinned, result_kind);
      } else if (result_kind == ValueKind::kS128 &&
                 (result_lane_kind == kF32 || result_lane_kind == kF64)) {
        CheckS128Nan(dst, LiftoffRegList{src1, src2, src3, dst},
                     result_lane_kind);
      }
    }
    __ PushRegister(result_kind, dst);
  }

  template <ValueKind src_kind, ValueKind result_kind,
            ValueKind result_lane_kind = kVoid, typename EmitFn>
  void EmitTerOp(EmitFn fn) {
    LiftoffRegister src3 = __ PopToRegister();
    LiftoffRegister src2 = __ PopToRegister(LiftoffRegList{src3});
    LiftoffRegister src1 = __ PopToRegister(LiftoffRegList{src3, src2});
    static constexpr RegClass src_rc = reg_class_for(src_kind);
    static constexpr RegClass result_rc = reg_class_for(result_kind);
    // Reusing src1 and src2 will complicate codegen for select for some
    // backend, so we allow only reusing src3 (the mask), and pin src1 and src2.
    LiftoffRegister dst = src_rc == result_rc
                              ? __ GetUnusedRegister(result_rc, {src3},
                                                     LiftoffRegList{src1, src2})
                              : __ GetUnusedRegister(result_rc, {});
    EmitTerOp<src_kind, result_kind, result_lane_kind, EmitFn>(fn, dst, src1,
                                                               src2, src3);
  }

  void EmitRelaxedLaneSelect() {
#if defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_X64)
    if (!CpuFeatures::IsSupported(AVX)) {
      LiftoffRegister mask(xmm0);
      __ PopToFixedRegister(mask);
      LiftoffRegister src2 = __ PopToModifiableRegister(LiftoffRegList{mask});
      LiftoffRegister src1 = __ PopToRegister(LiftoffRegList{src2, mask});
      EmitTerOp<kS128, kS128>(&LiftoffAssembler::emit_s128_relaxed_laneselect,
                              src2, src1, src2, mask);
      return;
    }
#endif
    LiftoffRegList pinned;
    LiftoffRegister mask = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister src2 = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister src1 = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister dst =
        __ GetUnusedRegister(reg_class_for(kS128), {}, pinned);
    EmitTerOp<kS128, kS128>(&LiftoffAssembler::emit_s128_relaxed_laneselect,
                            dst, src1, src2, mask);
  }

  template <typename EmitFn, typename EmitFnImm>
  void EmitSimdShiftOp(EmitFn fn, EmitFnImm fnImm) {
    static constexpr RegClass result_rc = reg_class_for(kS128);

    LiftoffAssembler::VarState rhs_slot = __ cache_state()->stack_state.back();
    // Check if the RHS is an immediate.
    if (rhs_slot.is_const()) {
      __ cache_state()->stack_state.pop_back();
      int32_t imm = rhs_slot.i32_const();

      LiftoffRegister operand = __ PopToRegister();
      LiftoffRegister dst = __ GetUnusedRegister(result_rc, {operand}, {});

      CallEmitFn(fnImm, dst, operand, imm);
      __ PushRegister(kS128, dst);
    } else {
      LiftoffRegister count = __ PopToRegister();
      LiftoffRegister operand = __ PopToRegister();
      LiftoffRegister dst = __ GetUnusedRegister(result_rc, {operand}, {});

      CallEmitFn(fn, dst, operand, count);
      __ PushRegister(kS128, dst);
    }
  }

  template <ValueKind result_lane_kind>
  void EmitSimdFloatRoundingOpWithCFallback(
      bool (LiftoffAssembler::*emit_fn)(LiftoffRegister, LiftoffRegister),
      ExternalReference (*ext_ref)()) {
    static constexpr RegClass rc = reg_class_for(kS128);
    LiftoffRegister src = __ PopToRegister();
    LiftoffRegister dst = __ GetUnusedRegister(rc, {src}, {});
    if (!(asm_.*emit_fn)(dst, src)) {
      // Return v128 via stack for ARM.
      auto sig_v_s = MakeSig::Params(kS128);
      GenerateCCall(&dst, &sig_v_s, kS128, &src, ext_ref());
    }
    if (V8_UNLIKELY(nondeterminism_)) {
      LiftoffRegList pinned{dst};
      CheckS128Nan(dst, pinned, result_lane_kind);
    }
    __ PushRegister(kS128, dst);
  }

  template <typename EmitFn>
  void EmitSimdFmaOp(EmitFn emit_fn) {
    LiftoffRegList pinned;
    LiftoffRegister src3 = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister src2 = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister src1 = pinned.set(__ PopToRegister(pinned));
    RegClass dst_rc = reg_class_for(kS128);
    LiftoffRegister dst = __ GetUnusedRegister(dst_rc, {});
    (asm_.*emit_fn)(dst, src1, src2, src3);
    __ PushRegister(kS128, src1);
    return;
  }

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, base::Vector<Value> args,
              Value* result) {
    if (!CpuFeatures::SupportsWasmSimd128()) {
      return unsupported(decoder, kSimd, "simd");
    }
    switch (opcode) {
      case wasm::kExprI8x16Swizzle:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_swizzle);
      case wasm::kExprI8x16RelaxedSwizzle:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i8x16_relaxed_swizzle);
      case wasm::kExprI8x16Popcnt:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_popcnt);
      case wasm::kExprI8x16Splat:
        return EmitUnOp<kI32, kS128>(&LiftoffAssembler::emit_i8x16_splat);
      case wasm::kExprI16x8Splat:
        return EmitUnOp<kI32, kS128>(&LiftoffAssembler::emit_i16x8_splat);
      case wasm::kExprI32x4Splat:
        return EmitUnOp<kI32, kS128>(&LiftoffAssembler::emit_i32x4_splat);
      case wasm::kExprI64x2Splat:
        return EmitUnOp<kI64, kS128>(&LiftoffAssembler::emit_i64x2_splat);
      case wasm::kExprF32x4Splat:
        return EmitUnOp<kF32, kS128, kF32>(&LiftoffAssembler::emit_f32x4_splat);
      case wasm::kExprF64x2Splat:
        return EmitUnOp<kF64, kS128, kF64>(&LiftoffAssembler::emit_f64x2_splat);
      case wasm::kExprI8x16Eq:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_eq);
      case wasm::kExprI8x16Ne:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_ne);
      case wasm::kExprI8x16LtS:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i8x16_gt_s);
      case wasm::kExprI8x16LtU:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i8x16_gt_u);
      case wasm::kExprI8x16GtS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_gt_s);
      case wasm::kExprI8x16GtU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_gt_u);
      case wasm::kExprI8x16LeS:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i8x16_ge_s);
      case wasm::kExprI8x16LeU:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i8x16_ge_u);
      case wasm::kExprI8x16GeS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_ge_s);
      case wasm::kExprI8x16GeU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_ge_u);
      case wasm::kExprI16x8Eq:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_eq);
      case wasm::kExprI16x8Ne:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_ne);
      case wasm::kExprI16x8LtS:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i16x8_gt_s);
      case wasm::kExprI16x8LtU:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i16x8_gt_u);
      case wasm::kExprI16x8GtS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_gt_s);
      case wasm::kExprI16x8GtU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_gt_u);
      case wasm::kExprI16x8LeS:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i16x8_ge_s);
      case wasm::kExprI16x8LeU:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i16x8_ge_u);
      case wasm::kExprI16x8GeS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_ge_s);
      case wasm::kExprI16x8GeU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_ge_u);
      case wasm::kExprI32x4Eq:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_eq);
      case wasm::kExprI32x4Ne:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_ne);
      case wasm::kExprI32x4LtS:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i32x4_gt_s);
      case wasm::kExprI32x4LtU:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i32x4_gt_u);
      case wasm::kExprI32x4GtS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_gt_s);
      case wasm::kExprI32x4GtU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_gt_u);
      case wasm::kExprI32x4LeS:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i32x4_ge_s);
      case wasm::kExprI32x4LeU:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i32x4_ge_u);
      case wasm::kExprI32x4GeS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_ge_s);
      case wasm::kExprI32x4GeU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_ge_u);
      case wasm::kExprI64x2Eq:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i64x2_eq);
      case wasm::kExprI64x2Ne:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i64x2_ne);
      case wasm::kExprI64x2LtS:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i64x2_gt_s);
      case wasm::kExprI64x2GtS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i64x2_gt_s);
      case wasm::kExprI64x2LeS:
        return EmitBinOp<kS128, kS128, true>(
            &LiftoffAssembler::emit_i64x2_ge_s);
      case wasm::kExprI64x2GeS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i64x2_ge_s);
      case wasm::kExprF32x4Eq:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_f32x4_eq);
      case wasm::kExprF32x4Ne:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_f32x4_ne);
      case wasm::kExprF32x4Lt:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_f32x4_lt);
      case wasm::kExprF32x4Gt:
        return EmitBinOp<kS128, kS128, true>(&LiftoffAssembler::emit_f32x4_lt);
      case wasm::kExprF32x4Le:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_f32x4_le);
      case wasm::kExprF32x4Ge:
        return EmitBinOp<kS128, kS128, true>(&LiftoffAssembler::emit_f32x4_le);
      case wasm::kExprF64x2Eq:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_f64x2_eq);
      case wasm::kExprF64x2Ne:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_f64x2_ne);
      case wasm::kExprF64x2Lt:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_f64x2_lt);
      case wasm::kExprF64x2Gt:
        return EmitBinOp<kS128, kS128, true>(&LiftoffAssembler::emit_f64x2_lt);
      case wasm::kExprF64x2Le:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_f64x2_le);
      case wasm::kExprF64x2Ge:
        return EmitBinOp<kS128, kS128, true>(&LiftoffAssembler::emit_f64x2_le);
      case wasm::kExprS128Not:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_s128_not);
      case wasm::kExprS128And:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_s128_and);
      case wasm::kExprS128Or:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_s128_or);
      case wasm::kExprS128Xor:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_s128_xor);
      case wasm::kExprS128Select:
        return EmitTerOp<kS128, kS128>(&LiftoffAssembler::emit_s128_select);
      case wasm::kExprI8x16Neg:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_neg);
      case wasm::kExprV128AnyTrue:
        return EmitUnOp<kS128, kI32>(&LiftoffAssembler::emit_v128_anytrue);
      case wasm::kExprI8x16AllTrue:
        return EmitUnOp<kS128, kI32>(&LiftoffAssembler::emit_i8x16_alltrue);
      case wasm::kExprI8x16BitMask:
        return EmitUnOp<kS128, kI32>(&LiftoffAssembler::emit_i8x16_bitmask);
      case wasm::kExprI8x16Shl:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i8x16_shl,
                               &LiftoffAssembler::emit_i8x16_shli);
      case wasm::kExprI8x16ShrS:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i8x16_shr_s,
                               &LiftoffAssembler::emit_i8x16_shri_s);
      case wasm::kExprI8x16ShrU:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i8x16_shr_u,
                               &LiftoffAssembler::emit_i8x16_shri_u);
      case wasm::kExprI8x16Add:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_add);
      case wasm::kExprI8x16AddSatS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_add_sat_s);
      case wasm::kExprI8x16AddSatU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_add_sat_u);
      case wasm::kExprI8x16Sub:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_sub);
      case wasm::kExprI8x16SubSatS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_sub_sat_s);
      case wasm::kExprI8x16SubSatU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_sub_sat_u);
      case wasm::kExprI8x16MinS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_min_s);
      case wasm::kExprI8x16MinU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_min_u);
      case wasm::kExprI8x16MaxS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_max_s);
      case wasm::kExprI8x16MaxU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_max_u);
      case wasm::kExprI16x8Neg:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_neg);
      case wasm::kExprI16x8AllTrue:
        return EmitUnOp<kS128, kI32>(&LiftoffAssembler::emit_i16x8_alltrue);
      case wasm::kExprI16x8BitMask:
        return EmitUnOp<kS128, kI32>(&LiftoffAssembler::emit_i16x8_bitmask);
      case wasm::kExprI16x8Shl:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i16x8_shl,
                               &LiftoffAssembler::emit_i16x8_shli);
      case wasm::kExprI16x8ShrS:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i16x8_shr_s,
                               &LiftoffAssembler::emit_i16x8_shri_s);
      case wasm::kExprI16x8ShrU:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i16x8_shr_u,
                               &LiftoffAssembler::emit_i16x8_shri_u);
      case wasm::kExprI16x8Add:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_add);
      case wasm::kExprI16x8AddSatS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_add_sat_s);
      case wasm::kExprI16x8AddSatU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_add_sat_u);
      case wasm::kExprI16x8Sub:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_sub);
      case wasm::kExprI16x8SubSatS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_sub_sat_s);
      case wasm::kExprI16x8SubSatU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_sub_sat_u);
      case wasm::kExprI16x8Mul:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_mul);
      case wasm::kExprI16x8MinS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_min_s);
      case wasm::kExprI16x8MinU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_min_u);
      case wasm::kExprI16x8MaxS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_max_s);
      case wasm::kExprI16x8MaxU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_max_u);
      case wasm::kExprI16x8ExtAddPairwiseI8x16S:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_s);
      case wasm::kExprI16x8ExtAddPairwiseI8x16U:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_u);
      case wasm::kExprI16x8ExtMulLowI8x16S:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_extmul_low_i8x16_s);
      case wasm::kExprI16x8ExtMulLowI8x16U:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_extmul_low_i8x16_u);
      case wasm::kExprI16x8ExtMulHighI8x16S:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_extmul_high_i8x16_s);
      case wasm::kExprI16x8ExtMulHighI8x16U:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_extmul_high_i8x16_u);
      case wasm::kExprI16x8Q15MulRSatS:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_q15mulr_sat_s);
      case wasm::kExprI32x4Neg:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_neg);
      case wasm::kExprI32x4AllTrue:
        return EmitUnOp<kS128, kI32>(&LiftoffAssembler::emit_i32x4_alltrue);
      case wasm::kExprI32x4BitMask:
        return EmitUnOp<kS128, kI32>(&LiftoffAssembler::emit_i32x4_bitmask);
      case wasm::kExprI32x4Shl:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i32x4_shl,
                               &LiftoffAssembler::emit_i32x4_shli);
      case wasm::kExprI32x4ShrS:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i32x4_shr_s,
                               &LiftoffAssembler::emit_i32x4_shri_s);
      case wasm::kExprI32x4ShrU:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i32x4_shr_u,
                               &LiftoffAssembler::emit_i32x4_shri_u);
      case wasm::kExprI32x4Add:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_add);
      case wasm::kExprI32x4Sub:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_sub);
      case wasm::kExprI32x4Mul:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_mul);
      case wasm::kExprI32x4MinS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_min_s);
      case wasm::kExprI32x4MinU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_min_u);
      case wasm::kExprI32x4MaxS:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_max_s);
      case wasm::kExprI32x4MaxU:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_max_u);
      case wasm::kExprI32x4DotI16x8S:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_dot_i16x8_s);
      case wasm::kExprI32x4ExtAddPairwiseI16x8S:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_s);
      case wasm::kExprI32x4ExtAddPairwiseI16x8U:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_u);
      case wasm::kExprI32x4ExtMulLowI16x8S:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_extmul_low_i16x8_s);
      case wasm::kExprI32x4ExtMulLowI16x8U:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_extmul_low_i16x8_u);
      case wasm::kExprI32x4ExtMulHighI16x8S:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_extmul_high_i16x8_s);
      case wasm::kExprI32x4ExtMulHighI16x8U:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_extmul_high_i16x8_u);
      case wasm::kExprI64x2Neg:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_i64x2_neg);
      case wasm::kExprI64x2AllTrue:
        return EmitUnOp<kS128, kI32>(&LiftoffAssembler::emit_i64x2_alltrue);
      case wasm::kExprI64x2Shl:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i64x2_shl,
                               &LiftoffAssembler::emit_i64x2_shli);
      case wasm::kExprI64x2ShrS:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i64x2_shr_s,
                               &LiftoffAssembler::emit_i64x2_shri_s);
      case wasm::kExprI64x2ShrU:
        return EmitSimdShiftOp(&LiftoffAssembler::emit_i64x2_shr_u,
                               &LiftoffAssembler::emit_i64x2_shri_u);
      case wasm::kExprI64x2Add:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i64x2_add);
      case wasm::kExprI64x2Sub:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i64x2_sub);
      case wasm::kExprI64x2Mul:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_i64x2_mul);
      case wasm::kExprI64x2ExtMulLowI32x4S:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i64x2_extmul_low_i32x4_s);
      case wasm::kExprI64x2ExtMulLowI32x4U:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i64x2_extmul_low_i32x4_u);
      case wasm::kExprI64x2ExtMulHighI32x4S:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i64x2_extmul_high_i32x4_s);
      case wasm::kExprI64x2ExtMulHighI32x4U:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i64x2_extmul_high_i32x4_u);
      case wasm::kExprI64x2BitMask:
        return EmitUnOp<kS128, kI32>(&LiftoffAssembler::emit_i64x2_bitmask);
      case wasm::kExprI64x2SConvertI32x4Low:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i64x2_sconvert_i32x4_low);
      case wasm::kExprI64x2SConvertI32x4High:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i64x2_sconvert_i32x4_high);
      case wasm::kExprI64x2UConvertI32x4Low:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i64x2_uconvert_i32x4_low);
      case wasm::kExprI64x2UConvertI32x4High:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i64x2_uconvert_i32x4_high);
      case wasm::kExprF32x4Abs:
        return EmitUnOp<kS128, kS128, kF32>(&LiftoffAssembler::emit_f32x4_abs);
      case wasm::kExprF32x4Neg:
        return EmitUnOp<kS128, kS128, kF32>(&LiftoffAssembler::emit_f32x4_neg);
      case wasm::kExprF32x4Sqrt:
        return EmitUnOp<kS128, kS128, kF32>(&LiftoffAssembler::emit_f32x4_sqrt);
      case wasm::kExprF32x4Ceil:
        return EmitSimdFloatRoundingOpWithCFallback<kF32>(
            &LiftoffAssembler::emit_f32x4_ceil,
            &ExternalReference::wasm_f32x4_ceil);
      case wasm::kExprF32x4Floor:
        return EmitSimdFloatRoundingOpWithCFallback<kF32>(
            &LiftoffAssembler::emit_f32x4_floor,
            ExternalReference::wasm_f32x4_floor);
      case wasm::kExprF32x4Trunc:
        return EmitSimdFloatRoundingOpWithCFallback<kF32>(
            &LiftoffAssembler::emit_f32x4_trunc,
            ExternalReference::wasm_f32x4_trunc);
      case wasm::kExprF32x4NearestInt:
        return EmitSimdFloatRoundingOpWithCFallback<kF32>(
            &LiftoffAssembler::emit_f32x4_nearest_int,
            ExternalReference::wasm_f32x4_nearest_int);
      case wasm::kExprF32x4Add:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_add);
      case wasm::kExprF32x4Sub:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_sub);
      case wasm::kExprF32x4Mul:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_mul);
      case wasm::kExprF32x4Div:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_div);
      case wasm::kExprF32x4Min:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_min);
      case wasm::kExprF32x4Max:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_max);
      case wasm::kExprF32x4Pmin:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_pmin);
      case wasm::kExprF32x4Pmax:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_pmax);
      case wasm::kExprF64x2Abs:
        return EmitUnOp<kS128, kS128, kF64>(&LiftoffAssembler::emit_f64x2_abs);
      case wasm::kExprF64x2Neg:
        return EmitUnOp<kS128, kS128, kF64>(&LiftoffAssembler::emit_f64x2_neg);
      case wasm::kExprF64x2Sqrt:
        return EmitUnOp<kS128, kS128, kF64>(&LiftoffAssembler::emit_f64x2_sqrt);
      case wasm::kExprF64x2Ceil:
        return EmitSimdFloatRoundingOpWithCFallback<kF64>(
            &LiftoffAssembler::emit_f64x2_ceil,
            &ExternalReference::wasm_f64x2_ceil);
      case wasm::kExprF64x2Floor:
        return EmitSimdFloatRoundingOpWithCFallback<kF64>(
            &LiftoffAssembler::emit_f64x2_floor,
            ExternalReference::wasm_f64x2_floor);
      case wasm::kExprF64x2Trunc:
        return EmitSimdFloatRoundingOpWithCFallback<kF64>(
            &LiftoffAssembler::emit_f64x2_trunc,
            ExternalReference::wasm_f64x2_trunc);
      case wasm::kExprF64x2NearestInt:
        return EmitSimdFloatRoundingOpWithCFallback<kF64>(
            &LiftoffAssembler::emit_f64x2_nearest_int,
            ExternalReference::wasm_f64x2_nearest_int);
      case wasm::kExprF64x2Add:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_add);
      case wasm::kExprF64x2Sub:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_sub);
      case wasm::kExprF64x2Mul:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_mul);
      case wasm::kExprF64x2Div:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_div);
      case wasm::kExprF64x2Min:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_min);
      case wasm::kExprF64x2Max:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_max);
      case wasm::kExprF64x2Pmin:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_pmin);
      case wasm::kExprF64x2Pmax:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_pmax);
      case wasm::kExprI32x4SConvertF32x4:
        return EmitUnOp<kS128, kS128, kF32>(
            &LiftoffAssembler::emit_i32x4_sconvert_f32x4);
      case wasm::kExprI32x4UConvertF32x4:
        return EmitUnOp<kS128, kS128, kF32>(
            &LiftoffAssembler::emit_i32x4_uconvert_f32x4);
      case wasm::kExprF32x4SConvertI32x4:
        return EmitUnOp<kS128, kS128, kF32>(
            &LiftoffAssembler::emit_f32x4_sconvert_i32x4);
      case wasm::kExprF32x4UConvertI32x4:
        return EmitUnOp<kS128, kS128, kF32>(
            &LiftoffAssembler::emit_f32x4_uconvert_i32x4);
      case wasm::kExprI8x16SConvertI16x8:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i8x16_sconvert_i16x8);
      case wasm::kExprI8x16UConvertI16x8:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i8x16_uconvert_i16x8);
      case wasm::kExprI16x8SConvertI32x4:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_sconvert_i32x4);
      case wasm::kExprI16x8UConvertI32x4:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_uconvert_i32x4);
      case wasm::kExprI16x8SConvertI8x16Low:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_sconvert_i8x16_low);
      case wasm::kExprI16x8SConvertI8x16High:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_sconvert_i8x16_high);
      case wasm::kExprI16x8UConvertI8x16Low:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_uconvert_i8x16_low);
      case wasm::kExprI16x8UConvertI8x16High:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_uconvert_i8x16_high);
      case wasm::kExprI32x4SConvertI16x8Low:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_sconvert_i16x8_low);
      case wasm::kExprI32x4SConvertI16x8High:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_sconvert_i16x8_high);
      case wasm::kExprI32x4UConvertI16x8Low:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_uconvert_i16x8_low);
      case wasm::kExprI32x4UConvertI16x8High:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_uconvert_i16x8_high);
      case wasm::kExprS128AndNot:
        return EmitBinOp<kS128, kS128>(&LiftoffAssembler::emit_s128_and_not);
      case wasm::kExprI8x16RoundingAverageU:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i8x16_rounding_average_u);
      case wasm::kExprI16x8RoundingAverageU:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_rounding_average_u);
      case wasm::kExprI8x16Abs:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_i8x16_abs);
      case wasm::kExprI16x8Abs:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_i16x8_abs);
      case wasm::kExprI32x4Abs:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_i32x4_abs);
      case wasm::kExprI64x2Abs:
        return EmitUnOp<kS128, kS128>(&LiftoffAssembler::emit_i64x2_abs);
      case wasm::kExprF64x2ConvertLowI32x4S:
        return EmitUnOp<kS128, kS128, kF64>(
            &LiftoffAssembler::emit_f64x2_convert_low_i32x4_s);
      case wasm::kExprF64x2ConvertLowI32x4U:
        return EmitUnOp<kS128, kS128, kF64>(
            &LiftoffAssembler::emit_f64x2_convert_low_i32x4_u);
      case wasm::kExprF64x2PromoteLowF32x4:
        return EmitUnOp<kS128, kS128, kF64>(
            &LiftoffAssembler::emit_f64x2_promote_low_f32x4);
      case wasm::kExprF32x4DemoteF64x2Zero:
        return EmitUnOp<kS128, kS128, kF32>(
            &LiftoffAssembler::emit_f32x4_demote_f64x2_zero);
      case wasm::kExprI32x4TruncSatF64x2SZero:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_s_zero);
      case wasm::kExprI32x4TruncSatF64x2UZero:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_trunc_sat_f64x2_u_zero);
      case wasm::kExprF32x4Qfma:
        return EmitSimdFmaOp(&LiftoffAssembler::emit_f32x4_qfma);
      case wasm::kExprF32x4Qfms:
        return EmitSimdFmaOp(&LiftoffAssembler::emit_f32x4_qfms);
      case wasm::kExprF64x2Qfma:
        return EmitSimdFmaOp(&LiftoffAssembler::emit_f64x2_qfma);
      case wasm::kExprF64x2Qfms:
        return EmitSimdFmaOp(&LiftoffAssembler::emit_f64x2_qfms);
      case wasm::kExprI16x8RelaxedLaneSelect:
      case wasm::kExprI8x16RelaxedLaneSelect:
      case wasm::kExprI32x4RelaxedLaneSelect:
      case wasm::kExprI64x2RelaxedLaneSelect:
        return EmitRelaxedLaneSelect();
      case wasm::kExprF32x4RelaxedMin:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_relaxed_min);
      case wasm::kExprF32x4RelaxedMax:
        return EmitBinOp<kS128, kS128, false, kF32>(
            &LiftoffAssembler::emit_f32x4_relaxed_max);
      case wasm::kExprF64x2RelaxedMin:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_relaxed_min);
      case wasm::kExprF64x2RelaxedMax:
        return EmitBinOp<kS128, kS128, false, kF64>(
            &LiftoffAssembler::emit_f64x2_relaxed_max);
      case wasm::kExprI16x8RelaxedQ15MulRS:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_relaxed_q15mulr_s);
      case wasm::kExprI32x4RelaxedTruncF32x4S:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_relaxed_trunc_f32x4_s);
      case wasm::kExprI32x4RelaxedTruncF32x4U:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_relaxed_trunc_f32x4_u);
      case wasm::kExprI32x4RelaxedTruncF64x2SZero:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_relaxed_trunc_f64x2_s_zero);
      case wasm::kExprI32x4RelaxedTruncF64x2UZero:
        return EmitUnOp<kS128, kS128>(
            &LiftoffAssembler::emit_i32x4_relaxed_trunc_f64x2_u_zero);
      case wasm::kExprI16x8DotI8x16I7x16S:
        return EmitBinOp<kS128, kS128>(
            &LiftoffAssembler::emit_i16x8_dot_i8x16_i7x16_s);
      case wasm::kExprI32x4DotI8x16I7x16AddS: {
        // There is no helper for an instruction with 3 SIMD operands
        // and we do not expect to add any more, so inlining it here.
        static constexpr RegClass res_rc = reg_class_for(kS128);
        LiftoffRegList pinned;
        LiftoffRegister acc = pinned.set(__ PopToRegister(pinned));
        LiftoffRegister rhs = pinned.set(__ PopToRegister(pinned));
        LiftoffRegister lhs = pinned.set(__ PopToRegister(pinned));
        LiftoffRegister dst = __ GetUnusedRegister(res_rc, {lhs, rhs, acc}, {});

        __ emit_i32x4_dot_i8x16_i7x16_add_s(dst, lhs, rhs, acc);
        __ PushRegister(kS128, dst);
        return;
      }
      default:
        UNREACHABLE();
    }
  }

  template <ValueKind src_kind, ValueKind result_kind, typename EmitFn>
  void EmitSimdExtractLaneOp(EmitFn fn,
                             const SimdLaneImmediate<validate>& imm) {
    static constexpr RegClass src_rc = reg_class_for(src_kind);
    static constexpr RegClass result_rc = reg_class_for(result_kind);
    LiftoffRegister lhs = __ PopToRegister();
    LiftoffRegister dst = src_rc == result_rc
                              ? __ GetUnusedRegister(result_rc, {lhs}, {})
                              : __ GetUnusedRegister(result_rc, {});
    fn(dst, lhs, imm.lane);
    __ PushRegister(result_kind, dst);
  }

  template <ValueKind src2_kind, typename EmitFn>
  void EmitSimdReplaceLaneOp(EmitFn fn,
                             const SimdLaneImmediate<validate>& imm) {
    static constexpr RegClass src1_rc = reg_class_for(kS128);
    static constexpr RegClass src2_rc = reg_class_for(src2_kind);
    static constexpr RegClass result_rc = reg_class_for(kS128);
    // On backends which need fp pair, src1_rc and result_rc end up being
    // kFpRegPair, which is != kFpReg, but we still want to pin src2 when it is
    // kFpReg, since it can overlap with those pairs.
    static constexpr bool pin_src2 = kNeedS128RegPair && src2_rc == kFpReg;

    // Does not work for arm
    LiftoffRegister src2 = __ PopToRegister();
    LiftoffRegister src1 = (src1_rc == src2_rc || pin_src2)
                               ? __ PopToRegister(LiftoffRegList{src2})
                               : __
                                 PopToRegister();
    LiftoffRegister dst =
        (src2_rc == result_rc || pin_src2)
            ? __ GetUnusedRegister(result_rc, {src1}, LiftoffRegList{src2})
            : __ GetUnusedRegister(result_rc, {src1}, {});
    fn(dst, src1, src2, imm.lane);
    __ PushRegister(kS128, dst);
  }

  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate<validate>& imm,
                  const base::Vector<Value> inputs, Value* result) {
    if (!CpuFeatures::SupportsWasmSimd128()) {
      return unsupported(decoder, kSimd, "simd");
    }
    switch (opcode) {
#define CASE_SIMD_EXTRACT_LANE_OP(opcode, kind, fn)                           \
  case wasm::kExpr##opcode:                                                   \
    EmitSimdExtractLaneOp<kS128, k##kind>(                                    \
        [=](LiftoffRegister dst, LiftoffRegister lhs, uint8_t imm_lane_idx) { \
          __ emit_##fn(dst, lhs, imm_lane_idx);                               \
        },                                                                    \
        imm);                                                                 \
    break;
      CASE_SIMD_EXTRACT_LANE_OP(I8x16ExtractLaneS, I32, i8x16_extract_lane_s)
      CASE_SIMD_EXTRACT_LANE_OP(I8x16ExtractLaneU, I32, i8x16_extract_lane_u)
      CASE_SIMD_EXTRACT_LANE_OP(I16x8ExtractLaneS, I32, i16x8_extract_lane_s)
      CASE_SIMD_EXTRACT_LANE_OP(I16x8ExtractLaneU, I32, i16x8_extract_lane_u)
      CASE_SIMD_EXTRACT_LANE_OP(I32x4ExtractLane, I32, i32x4_extract_lane)
      CASE_SIMD_EXTRACT_LANE_OP(I64x2ExtractLane, I64, i64x2_extract_lane)
      CASE_SIMD_EXTRACT_LANE_OP(F32x4ExtractLane, F32, f32x4_extract_lane)
      CASE_SIMD_EXTRACT_LANE_OP(F64x2ExtractLane, F64, f64x2_extract_lane)
#undef CASE_SIMD_EXTRACT_LANE_OP
#define CASE_SIMD_REPLACE_LANE_OP(opcode, kind, fn)                          \
  case wasm::kExpr##opcode:                                                  \
    EmitSimdReplaceLaneOp<k##kind>(                                          \
        [=](LiftoffRegister dst, LiftoffRegister src1, LiftoffRegister src2, \
            uint8_t imm_lane_idx) {                                          \
          __ emit_##fn(dst, src1, src2, imm_lane_idx);                       \
        },                                                                   \
        imm);                                                                \
    break;
      CASE_SIMD_REPLACE_LANE_OP(I8x16ReplaceLane, I32, i8x16_replace_lane)
      CASE_SIMD_REPLACE_LANE_OP(I16x8ReplaceLane, I32, i16x8_replace_lane)
      CASE_SIMD_REPLACE_LANE_OP(I32x4ReplaceLane, I32, i32x4_replace_lane)
      CASE_SIMD_REPLACE_LANE_OP(I64x2ReplaceLane, I64, i64x2_replace_lane)
      CASE_SIMD_REPLACE_LANE_OP(F32x4ReplaceLane, F32, f32x4_replace_lane)
      CASE_SIMD_REPLACE_LANE_OP(F64x2ReplaceLane, F64, f64x2_replace_lane)
#undef CASE_SIMD_REPLACE_LANE_OP
      default:
        unsupported(decoder, kSimd, "simd");
    }
  }

  void S128Const(FullDecoder* decoder, const Simd128Immediate<validate>& imm,
                 Value* result) {
    if (!CpuFeatures::SupportsWasmSimd128()) {
      return unsupported(decoder, kSimd, "simd");
    }
    constexpr RegClass result_rc = reg_class_for(kS128);
    LiftoffRegister dst = __ GetUnusedRegister(result_rc, {});
    bool all_zeroes = std::all_of(std::begin(imm.value), std::end(imm.value),
                                  [](uint8_t v) { return v == 0; });
    bool all_ones = std::all_of(std::begin(imm.value), std::end(imm.value),
                                [](uint8_t v) { return v == 0xff; });
    if (all_zeroes) {
      __ LiftoffAssembler::emit_s128_xor(dst, dst, dst);
    } else if (all_ones) {
      // Any SIMD eq will work, i32x4 is efficient on all archs.
      __ LiftoffAssembler::emit_i32x4_eq(dst, dst, dst);
    } else {
      __ LiftoffAssembler::emit_s128_const(dst, imm.value);
    }
    __ PushRegister(kS128, dst);
  }

  void Simd8x16ShuffleOp(FullDecoder* decoder,
                         const Simd128Immediate<validate>& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {
    if (!CpuFeatures::SupportsWasmSimd128()) {
      return unsupported(decoder, kSimd, "simd");
    }
    static constexpr RegClass result_rc = reg_class_for(kS128);
    LiftoffRegList pinned;
    LiftoffRegister rhs = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister lhs = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister dst = __ GetUnusedRegister(result_rc, {lhs, rhs}, {});

    uint8_t shuffle[kSimd128Size];
    memcpy(shuffle, imm.value, sizeof(shuffle));
    bool is_swizzle;
    bool needs_swap;
    wasm::SimdShuffle::CanonicalizeShuffle(lhs == rhs, shuffle, &needs_swap,
                                           &is_swizzle);
    if (needs_swap) {
      std::swap(lhs, rhs);
    }
    __ LiftoffAssembler::emit_i8x16_shuffle(dst, lhs, rhs, shuffle, is_swizzle);
    __ PushRegister(kS128, dst);
  }

  void ToSmi(Register reg) {
    if (COMPRESS_POINTERS_BOOL || kSystemPointerSize == 4) {
      __ emit_i32_shli(reg, reg, kSmiShiftSize + kSmiTagSize);
    } else {
      __ emit_i64_shli(LiftoffRegister{reg}, LiftoffRegister{reg},
                       kSmiShiftSize + kSmiTagSize);
    }
  }

  void Store32BitExceptionValue(Register values_array, int* index_in_array,
                                Register value, LiftoffRegList pinned) {
    LiftoffRegister tmp_reg = __ GetUnusedRegister(kGpReg, pinned);
    // Get the lower half word into tmp_reg and extend to a Smi.
    --*index_in_array;
    __ emit_i32_andi(tmp_reg.gp(), value, 0xffff);
    ToSmi(tmp_reg.gp());
    __ StoreTaggedPointer(
        values_array, no_reg,
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(*index_in_array),
        tmp_reg, pinned, LiftoffAssembler::kSkipWriteBarrier);

    // Get the upper half word into tmp_reg and extend to a Smi.
    --*index_in_array;
    __ emit_i32_shri(tmp_reg.gp(), value, 16);
    ToSmi(tmp_reg.gp());
    __ StoreTaggedPointer(
        values_array, no_reg,
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(*index_in_array),
        tmp_reg, pinned, LiftoffAssembler::kSkipWriteBarrier);
  }

  void Store64BitExceptionValue(Register values_array, int* index_in_array,
                                LiftoffRegister value, LiftoffRegList pinned) {
    if (kNeedI64RegPair) {
      Store32BitExceptionValue(values_array, index_in_array, value.low_gp(),
                               pinned);
      Store32BitExceptionValue(values_array, index_in_array, value.high_gp(),
                               pinned);
    } else {
      Store32BitExceptionValue(values_array, index_in_array, value.gp(),
                               pinned);
      __ emit_i64_shri(value, value, 32);
      Store32BitExceptionValue(values_array, index_in_array, value.gp(),
                               pinned);
    }
  }

  void Load16BitExceptionValue(LiftoffRegister dst,
                               LiftoffRegister values_array, uint32_t* index,
                               LiftoffRegList pinned) {
    __ LoadSmiAsInt32(
        dst, values_array.gp(),
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(*index));
    (*index)++;
  }

  void Load32BitExceptionValue(Register dst, LiftoffRegister values_array,
                               uint32_t* index, LiftoffRegList pinned) {
    LiftoffRegister upper = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    Load16BitExceptionValue(upper, values_array, index, pinned);
    __ emit_i32_shli(upper.gp(), upper.gp(), 16);
    Load16BitExceptionValue(LiftoffRegister(dst), values_array, index, pinned);
    __ emit_i32_or(dst, upper.gp(), dst);
  }

  void Load64BitExceptionValue(LiftoffRegister dst,
                               LiftoffRegister values_array, uint32_t* index,
                               LiftoffRegList pinned) {
    if (kNeedI64RegPair) {
      Load32BitExceptionValue(dst.high_gp(), values_array, index, pinned);
      Load32BitExceptionValue(dst.low_gp(), values_array, index, pinned);
    } else {
      Load16BitExceptionValue(dst, values_array, index, pinned);
      __ emit_i64_shli(dst, dst, 48);
      LiftoffRegister tmp_reg =
          pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      Load16BitExceptionValue(tmp_reg, values_array, index, pinned);
      __ emit_i64_shli(tmp_reg, tmp_reg, 32);
      __ emit_i64_or(dst, tmp_reg, dst);
      Load16BitExceptionValue(tmp_reg, values_array, index, pinned);
      __ emit_i64_shli(tmp_reg, tmp_reg, 16);
      __ emit_i64_or(dst, tmp_reg, dst);
      Load16BitExceptionValue(tmp_reg, values_array, index, pinned);
      __ emit_i64_or(dst, tmp_reg, dst);
    }
  }

  void StoreExceptionValue(ValueType type, Register values_array,
                           int* index_in_array, LiftoffRegList pinned) {
    LiftoffRegister value = pinned.set(__ PopToRegister(pinned));
    switch (type.kind()) {
      case kI32:
        Store32BitExceptionValue(values_array, index_in_array, value.gp(),
                                 pinned);
        break;
      case kF32: {
        LiftoffRegister gp_reg =
            pinned.set(__ GetUnusedRegister(kGpReg, pinned));
        __ emit_type_conversion(kExprI32ReinterpretF32, gp_reg, value, nullptr);
        Store32BitExceptionValue(values_array, index_in_array, gp_reg.gp(),
                                 pinned);
        break;
      }
      case kI64:
        Store64BitExceptionValue(values_array, index_in_array, value, pinned);
        break;
      case kF64: {
        LiftoffRegister tmp_reg =
            pinned.set(__ GetUnusedRegister(reg_class_for(kI64), pinned));
        __ emit_type_conversion(kExprI64ReinterpretF64, tmp_reg, value,
                                nullptr);
        Store64BitExceptionValue(values_array, index_in_array, tmp_reg, pinned);
        break;
      }
      case kS128: {
        LiftoffRegister tmp_reg =
            pinned.set(__ GetUnusedRegister(kGpReg, pinned));
        for (int i : {3, 2, 1, 0}) {
          __ emit_i32x4_extract_lane(tmp_reg, value, i);
          Store32BitExceptionValue(values_array, index_in_array, tmp_reg.gp(),
                                   pinned);
        }
        break;
      }
      case wasm::kRef:
      case wasm::kRefNull:
      case wasm::kRtt: {
        --(*index_in_array);
        __ StoreTaggedPointer(
            values_array, no_reg,
            wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(
                *index_in_array),
            value, pinned);
        break;
      }
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }
  }

  void LoadExceptionValue(ValueKind kind, LiftoffRegister values_array,
                          uint32_t* index, LiftoffRegList pinned) {
    RegClass rc = reg_class_for(kind);
    LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));
    switch (kind) {
      case kI32:
        Load32BitExceptionValue(value.gp(), values_array, index, pinned);
        break;
      case kF32: {
        LiftoffRegister tmp_reg =
            pinned.set(__ GetUnusedRegister(kGpReg, pinned));
        Load32BitExceptionValue(tmp_reg.gp(), values_array, index, pinned);
        __ emit_type_conversion(kExprF32ReinterpretI32, value, tmp_reg,
                                nullptr);
        break;
      }
      case kI64:
        Load64BitExceptionValue(value, values_array, index, pinned);
        break;
      case kF64: {
        RegClass rc_i64 = reg_class_for(kI64);
        LiftoffRegister tmp_reg =
            pinned.set(__ GetUnusedRegister(rc_i64, pinned));
        Load64BitExceptionValue(tmp_reg, values_array, index, pinned);
        __ emit_type_conversion(kExprF64ReinterpretI64, value, tmp_reg,
                                nullptr);
        break;
      }
      case kS128: {
        LiftoffRegister tmp_reg =
            pinned.set(__ GetUnusedRegister(kGpReg, pinned));
        Load32BitExceptionValue(tmp_reg.gp(), values_array, index, pinned);
        __ emit_i32x4_splat(value, tmp_reg);
        for (int lane : {1, 2, 3}) {
          Load32BitExceptionValue(tmp_reg.gp(), values_array, index, pinned);
          __ emit_i32x4_replace_lane(value, value, tmp_reg, lane);
        }
        break;
      }
      case wasm::kRef:
      case wasm::kRefNull:
      case wasm::kRtt: {
        __ LoadTaggedPointer(
            value.gp(), values_array.gp(), no_reg,
            wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(*index));
        (*index)++;
        break;
      }
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }
    __ PushRegister(kind, value);
  }

  void GetExceptionValues(FullDecoder* decoder,
                          LiftoffAssembler::VarState& exception_var,
                          const WasmTag* tag) {
    LiftoffRegList pinned;
    CODE_COMMENT("get exception values");
    LiftoffRegister values_array = GetExceptionProperty(
        exception_var, RootIndex::kwasm_exception_values_symbol);
    pinned.set(values_array);
    uint32_t index = 0;
    const WasmTagSig* sig = tag->sig;
    for (ValueType param : sig->parameters()) {
      LoadExceptionValue(param.kind(), values_array, &index, pinned);
    }
    DCHECK_EQ(index, WasmExceptionPackage::GetEncodedSize(tag));
  }

  void EmitLandingPad(FullDecoder* decoder, int handler_offset) {
    if (decoder->current_catch() == -1) return;
    MovableLabel handler;

    // If we return from the throwing code normally, just skip over the handler.
    Label skip_handler;
    __ emit_jump(&skip_handler);

    // Handler: merge into the catch state, and jump to the catch body.
    CODE_COMMENT("-- landing pad --");
    __ bind(handler.get());
    __ ExceptionHandler();
    __ PushException();
    handlers_.push_back({std::move(handler), handler_offset});
    Control* current_try =
        decoder->control_at(decoder->control_depth_of_current_catch());
    DCHECK_NOT_NULL(current_try->try_info);
    if (!current_try->try_info->catch_reached) {
      current_try->try_info->catch_state.InitMerge(
          *__ cache_state(), __ num_locals(), 1,
          current_try->stack_depth + current_try->num_exceptions);
      current_try->try_info->catch_reached = true;
    }
    __ MergeStackWith(current_try->try_info->catch_state, 1,
                      LiftoffAssembler::kForwardJump);
    __ emit_jump(&current_try->try_info->catch_label);

    __ bind(&skip_handler);
    // Drop the exception.
    __ DropValues(1);
  }

  void Throw(FullDecoder* decoder, const TagIndexImmediate<validate>& imm,
             const base::Vector<Value>& /* args */) {
    LiftoffRegList pinned;

    // Load the encoded size in a register for the builtin call.
    int encoded_size = WasmExceptionPackage::GetEncodedSize(imm.tag);
    LiftoffRegister encoded_size_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(encoded_size_reg, WasmValue(encoded_size));

    // Call the WasmAllocateFixedArray builtin to create the values array.
    CallRuntimeStub(WasmCode::kWasmAllocateFixedArray,
                    MakeSig::Returns(kPointerKind).Params(kPointerKind),
                    {LiftoffAssembler::VarState{
                        kSmiKind, LiftoffRegister{encoded_size_reg}, 0}},
                    decoder->position());
    MaybeOSR();

    // The FixedArray for the exception values is now in the first gp return
    // register.
    LiftoffRegister values_array{kReturnRegister0};
    pinned.set(values_array);

    // Now store the exception values in the FixedArray. Do this from last to
    // first value, such that we can just pop them from the value stack.
    CODE_COMMENT("fill values array");
    int index = encoded_size;
    auto* sig = imm.tag->sig;
    for (size_t param_idx = sig->parameter_count(); param_idx > 0;
         --param_idx) {
      ValueType type = sig->GetParam(param_idx - 1);
      StoreExceptionValue(type, values_array.gp(), &index, pinned);
    }
    DCHECK_EQ(0, index);

    // Load the exception tag.
    CODE_COMMENT("load exception tag");
    LiftoffRegister exception_tag =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LOAD_TAGGED_PTR_INSTANCE_FIELD(exception_tag.gp(), TagsTable, pinned);
    __ LoadTaggedPointer(
        exception_tag.gp(), exception_tag.gp(), no_reg,
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(imm.index));

    // Finally, call WasmThrow.
    CallRuntimeStub(WasmCode::kWasmThrow,
                    MakeSig::Params(kPointerKind, kPointerKind),
                    {LiftoffAssembler::VarState{kPointerKind, exception_tag, 0},
                     LiftoffAssembler::VarState{kPointerKind, values_array, 0}},
                    decoder->position());

    int pc_offset = __ pc_offset();
    MaybeOSR();
    EmitLandingPad(decoder, pc_offset);
  }

  void AtomicStoreMem(FullDecoder* decoder, StoreType type,
                      const MemoryAccessImmediate<validate>& imm) {
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
    LiftoffRegister full_index = __ PopToRegister(pinned);
    Register index = BoundsCheckMem(decoder, type.size(), imm.offset,
                                    full_index, pinned, kDoForceCheck);
    if (index == no_reg) return;

    pinned.set(index);
    AlignmentCheckMem(decoder, type.size(), imm.offset, index, pinned);
    uintptr_t offset = imm.offset;
    CODE_COMMENT("atomic store to memory");
    Register addr = pinned.set(GetMemoryStart(pinned));
    LiftoffRegList outer_pinned;
    if (V8_UNLIKELY(v8_flags.trace_wasm_memory)) outer_pinned.set(index);
    __ AtomicStore(addr, index, offset, value, type, outer_pinned);
    if (V8_UNLIKELY(v8_flags.trace_wasm_memory)) {
      TraceMemoryOperation(true, type.mem_rep(), index, offset,
                           decoder->position());
    }
  }

  void AtomicLoadMem(FullDecoder* decoder, LoadType type,
                     const MemoryAccessImmediate<validate>& imm) {
    ValueKind kind = type.value_type().kind();
    LiftoffRegister full_index = __ PopToRegister();
    Register index = BoundsCheckMem(decoder, type.size(), imm.offset,
                                    full_index, {}, kDoForceCheck);
    if (index == no_reg) return;

    LiftoffRegList pinned{index};
    AlignmentCheckMem(decoder, type.size(), imm.offset, index, pinned);
    uintptr_t offset = imm.offset;
    CODE_COMMENT("atomic load from memory");
    Register addr = pinned.set(GetMemoryStart(pinned));
    RegClass rc = reg_class_for(kind);
    LiftoffRegister value = pinned.set(__ GetUnusedRegister(rc, pinned));
    __ AtomicLoad(value, addr, index, offset, type, pinned);
    __ PushRegister(kind, value);

    if (V8_UNLIKELY(v8_flags.trace_wasm_memory)) {
      TraceMemoryOperation(false, type.mem_type().representation(), index,
                           offset, decoder->position());
    }
  }

  void AtomicBinop(FullDecoder* decoder, StoreType type,
                   const MemoryAccessImmediate<validate>& imm,
                   void (LiftoffAssembler::*emit_fn)(Register, Register,
                                                     uintptr_t, LiftoffRegister,
                                                     LiftoffRegister,
                                                     StoreType)) {
    ValueKind result_kind = type.value_type().kind();
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister());
#ifdef V8_TARGET_ARCH_IA32
    // We have to reuse the value register as the result register so that we
    // don't run out of registers on ia32. For this we use the value register as
    // the result register if it has no other uses. Otherwise we allocate a new
    // register and let go of the value register to get spilled.
    LiftoffRegister result = value;
    if (__ cache_state()->is_used(value)) {
      result = pinned.set(__ GetUnusedRegister(value.reg_class(), pinned));
      __ Move(result, value, result_kind);
      pinned.clear(value);
      value = result;
    }
#else
    LiftoffRegister result =
        pinned.set(__ GetUnusedRegister(value.reg_class(), pinned));
#endif
    LiftoffRegister full_index = __ PopToRegister(pinned);
    Register index = BoundsCheckMem(decoder, type.size(), imm.offset,
                                    full_index, pinned, kDoForceCheck);
    if (index == no_reg) return;

    pinned.set(index);
    AlignmentCheckMem(decoder, type.size(), imm.offset, index, pinned);

    CODE_COMMENT("atomic binop");
    uintptr_t offset = imm.offset;
    Register addr = pinned.set(GetMemoryStart(pinned));

    (asm_.*emit_fn)(addr, index, offset, value, result, type);
    __ PushRegister(result_kind, result);
  }

  void AtomicCompareExchange(FullDecoder* decoder, StoreType type,
                             const MemoryAccessImmediate<validate>& imm) {
#ifdef V8_TARGET_ARCH_IA32
    // On ia32 we don't have enough registers to first pop all the values off
    // the stack and then start with the code generation. Instead we do the
    // complete address calculation first, so that the address only needs a
    // single register. Afterwards we load all remaining values into the
    // other registers.
    LiftoffRegister full_index = __ PeekToRegister(2, {});
    Register index = BoundsCheckMem(decoder, type.size(), imm.offset,
                                    full_index, {}, kDoForceCheck);
    if (index == no_reg) return;
    LiftoffRegList pinned{index};
    AlignmentCheckMem(decoder, type.size(), imm.offset, index, pinned);

    uintptr_t offset = imm.offset;
    Register addr = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_INSTANCE_FIELD(addr, MemoryStart, kSystemPointerSize, pinned);
#ifdef V8_ENABLE_SANDBOX
    __ DecodeSandboxedPointer(addr);
#endif
    __ emit_i32_add(addr, addr, index);
    pinned.clear(LiftoffRegister(index));
    LiftoffRegister new_value = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister expected = pinned.set(__ PopToRegister(pinned));

    // Pop the index from the stack.
    __ DropValues(1);

    LiftoffRegister result = expected;
    if (__ cache_state()->is_used(result)) __ SpillRegister(result);

    // We already added the index to addr, so we can just pass no_reg to the
    // assembler now.
    __ AtomicCompareExchange(addr, no_reg, offset, expected, new_value, result,
                             type);
    __ PushRegister(type.value_type().kind(), result);
    return;
#else
    ValueKind result_kind = type.value_type().kind();
    LiftoffRegList pinned;
    LiftoffRegister new_value = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister expected = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister full_index = __ PopToRegister(pinned);
    Register index = BoundsCheckMem(decoder, type.size(), imm.offset,
                                    full_index, pinned, kDoForceCheck);
    if (index == no_reg) return;
    pinned.set(index);
    AlignmentCheckMem(decoder, type.size(), imm.offset, index, pinned);

    uintptr_t offset = imm.offset;
    Register addr = pinned.set(GetMemoryStart(pinned));
    LiftoffRegister result =
        pinned.set(__ GetUnusedRegister(reg_class_for(result_kind), pinned));

    __ AtomicCompareExchange(addr, index, offset, expected, new_value, result,
                             type);
    __ PushRegister(result_kind, result);
#endif
  }

  void CallRuntimeStub(WasmCode::RuntimeStubId stub_id, const ValueKindSig& sig,
                       std::initializer_list<LiftoffAssembler::VarState> params,
                       int position) {
    CODE_COMMENT(
        (std::string{"call builtin: "} + GetRuntimeStubName(stub_id)).c_str());
    auto interface_descriptor = Builtins::CallInterfaceDescriptorFor(
        RuntimeStubIdToBuiltinName(stub_id));
    auto* call_descriptor = compiler::Linkage::GetStubCallDescriptor(
        compilation_zone_,                              // zone
        interface_descriptor,                           // descriptor
        interface_descriptor.GetStackParameterCount(),  // stack parameter count
        compiler::CallDescriptor::kNoFlags,             // flags
        compiler::Operator::kNoProperties,              // properties
        StubCallMode::kCallWasmRuntimeStub);            // stub call mode

    __ PrepareBuiltinCall(&sig, call_descriptor, params);
    if (position != kNoSourcePosition) {
      source_position_table_builder_.AddPosition(
          __ pc_offset(), SourcePosition(position), true);
    }
    __ CallRuntimeStub(stub_id);
    DefineSafepoint();
  }

  void AtomicWait(FullDecoder* decoder, ValueKind kind,
                  const MemoryAccessImmediate<validate>& imm) {
    LiftoffRegister full_index = __ PeekToRegister(2, {});
    Register index_reg =
        BoundsCheckMem(decoder, value_kind_size(kind), imm.offset, full_index,
                       {}, kDoForceCheck);
    if (index_reg == no_reg) return;
    LiftoffRegList pinned{index_reg};
    AlignmentCheckMem(decoder, value_kind_size(kind), imm.offset, index_reg,
                      pinned);

    uintptr_t offset = imm.offset;
    Register index_plus_offset =
        __ cache_state()->is_used(LiftoffRegister(index_reg))
            ? pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp()
            : index_reg;
    // TODO(clemensb): Skip this if memory is 64 bit.
    __ emit_ptrsize_zeroextend_i32(index_plus_offset, index_reg);
    if (offset) {
      __ emit_ptrsize_addi(index_plus_offset, index_plus_offset, offset);
    }

    LiftoffAssembler::VarState timeout =
        __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState expected_value =
        __ cache_state()->stack_state.end()[-2];
    LiftoffAssembler::VarState index = __ cache_state()->stack_state.end()[-3];

    // We have to set the correct register for the index.
    index.MakeRegister(LiftoffRegister(index_plus_offset));

    static constexpr WasmCode::RuntimeStubId kTargets[2][2]{
        // 64 bit systems (kNeedI64RegPair == false):
        {WasmCode::kWasmI64AtomicWait64, WasmCode::kWasmI32AtomicWait64},
        // 32 bit systems (kNeedI64RegPair == true):
        {WasmCode::kWasmI64AtomicWait32, WasmCode::kWasmI32AtomicWait32}};
    auto target = kTargets[kNeedI64RegPair][kind == kI32];

    CallRuntimeStub(target, MakeSig::Params(kPointerKind, kind, kI64),
                    {index, expected_value, timeout}, decoder->position());
    // Pop parameters from the value stack.
    __ DropValues(3);

    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    __ PushRegister(kI32, LiftoffRegister(kReturnRegister0));
  }

  void AtomicNotify(FullDecoder* decoder,
                    const MemoryAccessImmediate<validate>& imm) {
    LiftoffRegister full_index = __ PeekToRegister(1, {});
    Register index_reg = BoundsCheckMem(decoder, kInt32Size, imm.offset,
                                        full_index, {}, kDoForceCheck);
    if (index_reg == no_reg) return;
    LiftoffRegList pinned{index_reg};
    AlignmentCheckMem(decoder, kInt32Size, imm.offset, index_reg, pinned);

    uintptr_t offset = imm.offset;
    Register index_plus_offset =
        __ cache_state()->is_used(LiftoffRegister(index_reg))
            ? pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp()
            : index_reg;
    // TODO(clemensb): Skip this if memory is 64 bit.
    __ emit_ptrsize_zeroextend_i32(index_plus_offset, index_reg);
    if (offset) {
      __ emit_ptrsize_addi(index_plus_offset, index_plus_offset, offset);
    }

    LiftoffAssembler::VarState count = __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState index = __ cache_state()->stack_state.end()[-2];
    index.MakeRegister(LiftoffRegister(index_plus_offset));

    CallRuntimeStub(WasmCode::kWasmAtomicNotify,
                    MakeSig::Returns(kI32).Params(kPointerKind, kI32),
                    {index, count}, decoder->position());
    // Pop parameters from the value stack.
    __ DropValues(2);

    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    __ PushRegister(kI32, LiftoffRegister(kReturnRegister0));
  }

#define ATOMIC_STORE_LIST(V)        \
  V(I32AtomicStore, kI32Store)      \
  V(I64AtomicStore, kI64Store)      \
  V(I32AtomicStore8U, kI32Store8)   \
  V(I32AtomicStore16U, kI32Store16) \
  V(I64AtomicStore8U, kI64Store8)   \
  V(I64AtomicStore16U, kI64Store16) \
  V(I64AtomicStore32U, kI64Store32)

#define ATOMIC_LOAD_LIST(V)        \
  V(I32AtomicLoad, kI32Load)       \
  V(I64AtomicLoad, kI64Load)       \
  V(I32AtomicLoad8U, kI32Load8U)   \
  V(I32AtomicLoad16U, kI32Load16U) \
  V(I64AtomicLoad8U, kI64Load8U)   \
  V(I64AtomicLoad16U, kI64Load16U) \
  V(I64AtomicLoad32U, kI64Load32U)

#define ATOMIC_BINOP_INSTRUCTION_LIST(V)         \
  V(Add, I32AtomicAdd, kI32Store)                \
  V(Add, I64AtomicAdd, kI64Store)                \
  V(Add, I32AtomicAdd8U, kI32Store8)             \
  V(Add, I32AtomicAdd16U, kI32Store16)           \
  V(Add, I64AtomicAdd8U, kI64Store8)             \
  V(Add, I64AtomicAdd16U, kI64Store16)           \
  V(Add, I64AtomicAdd32U, kI64Store32)           \
  V(Sub, I32AtomicSub, kI32Store)                \
  V(Sub, I64AtomicSub, kI64Store)                \
  V(Sub, I32AtomicSub8U, kI32Store8)             \
  V(Sub, I32AtomicSub16U, kI32Store16)           \
  V(Sub, I64AtomicSub8U, kI64Store8)             \
  V(Sub, I64AtomicSub16U, kI64Store16)           \
  V(Sub, I64AtomicSub32U, kI64Store32)           \
  V(And, I32AtomicAnd, kI32Store)                \
  V(And, I64AtomicAnd, kI64Store)                \
  V(And, I32AtomicAnd8U, kI32Store8)             \
  V(And, I32AtomicAnd16U, kI32Store16)           \
  V(And, I64AtomicAnd8U, kI64Store8)             \
  V(And, I64AtomicAnd16U, kI64Store16)           \
  V(And, I64AtomicAnd32U, kI64Store32)           \
  V(Or, I32AtomicOr, kI32Store)                  \
  V(Or, I64AtomicOr, kI64Store)                  \
  V(Or, I32AtomicOr8U, kI32Store8)               \
  V(Or, I32AtomicOr16U, kI32Store16)             \
  V(Or, I64AtomicOr8U, kI64Store8)               \
  V(Or, I64AtomicOr16U, kI64Store16)             \
  V(Or, I64AtomicOr32U, kI64Store32)             \
  V(Xor, I32AtomicXor, kI32Store)                \
  V(Xor, I64AtomicXor, kI64Store)                \
  V(Xor, I32AtomicXor8U, kI32Store8)             \
  V(Xor, I32AtomicXor16U, kI32Store16)           \
  V(Xor, I64AtomicXor8U, kI64Store8)             \
  V(Xor, I64AtomicXor16U, kI64Store16)           \
  V(Xor, I64AtomicXor32U, kI64Store32)           \
  V(Exchange, I32AtomicExchange, kI32Store)      \
  V(Exchange, I64AtomicExchange, kI64Store)      \
  V(Exchange, I32AtomicExchange8U, kI32Store8)   \
  V(Exchange, I32AtomicExchange16U, kI32Store16) \
  V(Exchange, I64AtomicExchange8U, kI64Store8)   \
  V(Exchange, I64AtomicExchange16U, kI64Store16) \
  V(Exchange, I64AtomicExchange32U, kI64Store32)

#define ATOMIC_COMPARE_EXCHANGE_LIST(V)       \
  V(I32AtomicCompareExchange, kI32Store)      \
  V(I64AtomicCompareExchange, kI64Store)      \
  V(I32AtomicCompareExchange8U, kI32Store8)   \
  V(I32AtomicCompareExchange16U, kI32Store16) \
  V(I64AtomicCompareExchange8U, kI64Store8)   \
  V(I64AtomicCompareExchange16U, kI64Store16) \
  V(I64AtomicCompareExchange32U, kI64Store32)

  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode,
                base::Vector<Value> args,
                const MemoryAccessImmediate<validate>& imm, Value* result) {
    switch (opcode) {
#define ATOMIC_STORE_OP(name, type)                \
  case wasm::kExpr##name:                          \
    AtomicStoreMem(decoder, StoreType::type, imm); \
    break;

      ATOMIC_STORE_LIST(ATOMIC_STORE_OP)
#undef ATOMIC_STORE_OP

#define ATOMIC_LOAD_OP(name, type)               \
  case wasm::kExpr##name:                        \
    AtomicLoadMem(decoder, LoadType::type, imm); \
    break;

      ATOMIC_LOAD_LIST(ATOMIC_LOAD_OP)
#undef ATOMIC_LOAD_OP

#define ATOMIC_BINOP_OP(op, name, type)                                        \
  case wasm::kExpr##name:                                                      \
    AtomicBinop(decoder, StoreType::type, imm, &LiftoffAssembler::Atomic##op); \
    break;

      ATOMIC_BINOP_INSTRUCTION_LIST(ATOMIC_BINOP_OP)
#undef ATOMIC_BINOP_OP

#define ATOMIC_COMPARE_EXCHANGE_OP(name, type)            \
  case wasm::kExpr##name:                                 \
    AtomicCompareExchange(decoder, StoreType::type, imm); \
    break;

      ATOMIC_COMPARE_EXCHANGE_LIST(ATOMIC_COMPARE_EXCHANGE_OP)
#undef ATOMIC_COMPARE_EXCHANGE_OP

      case kExprI32AtomicWait:
        AtomicWait(decoder, kI32, imm);
        break;
      case kExprI64AtomicWait:
        AtomicWait(decoder, kI64, imm);
        break;
      case kExprAtomicNotify:
        AtomicNotify(decoder, imm);
        break;
      default:
        unsupported(decoder, kAtomics, "atomicop");
    }
  }

#undef ATOMIC_STORE_LIST
#undef ATOMIC_LOAD_LIST
#undef ATOMIC_BINOP_INSTRUCTION_LIST
#undef ATOMIC_COMPARE_EXCHANGE_LIST

  void AtomicFence(FullDecoder* decoder) { __ AtomicFence(); }

  // Pop a memtype (i32 or i64 depending on {WasmModule::is_memory64}) to a
  // register, updating {*high_word} to contain the ORed combination of all
  // popped high words. Returns the ptrsized register holding the popped value.
  LiftoffRegister PopMemTypeToRegister(FullDecoder* decoder,
                                       Register* high_word,
                                       LiftoffRegList* pinned) {
    LiftoffRegister reg = __ PopToRegister(*pinned);
    LiftoffRegister intptr_reg = reg;
    // For memory32 on 64-bit hosts, zero-extend.
    if (kSystemPointerSize == kInt64Size && !env_->module->is_memory64) {
      // Only overwrite {reg} if it's not used otherwise.
      if (pinned->has(reg) || __ cache_state()->is_used(reg)) {
        intptr_reg = __ GetUnusedRegister(kGpReg, *pinned);
      }
      __ emit_u32_to_uintptr(intptr_reg.gp(), reg.gp());
    }
    // For memory32 or memory64 on 64-bit, we are done here.
    if (kSystemPointerSize == kInt64Size || !env_->module->is_memory64) {
      pinned->set(intptr_reg);
      return intptr_reg;
    }

    // For memory64 on 32-bit systems, combine all high words for a zero-check
    // and only use the low words afterwards. This keeps the register pressure
    // managable.
    DCHECK_GE(kMaxUInt32, env_->max_memory_size);
    pinned->set(reg.low());
    if (*high_word == no_reg) {
      // Choose a register to hold the (combination of) high word(s). It cannot
      // be one of the pinned registers, and it cannot be used in the value
      // stack.
      *high_word =
          pinned->has(reg.high())
              ? __ GetUnusedRegister(kGpReg, *pinned).gp()
              : __ GetUnusedRegister(kGpReg, {reg.high()}, *pinned).gp();
      pinned->set(*high_word);
      if (*high_word != reg.high_gp()) {
        __ Move(*high_word, reg.high_gp(), kI32);
      }
    } else if (*high_word != reg.high_gp()) {
      // Combine the new high word into existing high words.
      __ emit_i32_or(*high_word, *high_word, reg.high_gp());
    }
    return reg.low();
  }

  void MemoryInit(FullDecoder* decoder,
                  const MemoryInitImmediate<validate>& imm, const Value&,
                  const Value&, const Value&) {
    Register mem_offsets_high_word = no_reg;
    LiftoffRegList pinned;
    LiftoffRegister size = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister src = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister dst =
        PopMemTypeToRegister(decoder, &mem_offsets_high_word, &pinned);

    Register instance = __ cache_state()->cached_instance;
    if (instance == no_reg) {
      instance = __ GetUnusedRegister(kGpReg, pinned).gp();
      __ LoadInstanceFromFrame(instance);
    }
    pinned.set(instance);

    // Only allocate the OOB code now, so the state of the stack is reflected
    // correctly.
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapMemOutOfBounds);
    if (mem_offsets_high_word != no_reg) {
      // If any high word has bits set, jump to the OOB trap.
      FREEZE_STATE(trapping);
      __ emit_cond_jump(kNotEqualZero, trap_label, kI32, mem_offsets_high_word,
                        no_reg, trapping);
      pinned.clear(mem_offsets_high_word);
    }

    LiftoffRegister segment_index =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(segment_index, WasmValue(imm.data_segment.index));

    auto sig = MakeSig::Returns(kI32).Params(kPointerKind, kPointerKind, kI32,
                                             kI32, kI32);
    LiftoffRegister args[] = {LiftoffRegister(instance), dst, src,
                              segment_index, size};
    // We don't need the instance anymore after the call. We can use the
    // register for the result.
    LiftoffRegister result(instance);
    GenerateCCall(&result, &sig, kVoid, args,
                  ExternalReference::wasm_memory_init());
    FREEZE_STATE(trapping);
    __ emit_cond_jump(kEqual, trap_label, kI32, result.gp(), no_reg, trapping);
  }

  void DataDrop(FullDecoder* decoder, const IndexImmediate<validate>& imm) {
    LiftoffRegList pinned;

    Register seg_size_array =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_TAGGED_PTR_INSTANCE_FIELD(seg_size_array, DataSegmentSizes, pinned);

    LiftoffRegister seg_index =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    // Scale the seg_index for the array access.
    __ LoadConstant(
        seg_index,
        WasmValue(wasm::ObjectAccess::ElementOffsetInTaggedFixedUInt32Array(
            imm.index)));

    // Set the length of the segment to '0' to drop it.
    LiftoffRegister null_reg = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(null_reg, WasmValue(0));
    __ Store(seg_size_array, seg_index.gp(), 0, null_reg, StoreType::kI32Store,
             pinned);
  }

  void MemoryCopy(FullDecoder* decoder,
                  const MemoryCopyImmediate<validate>& imm, const Value&,
                  const Value&, const Value&) {
    Register mem_offsets_high_word = no_reg;
    LiftoffRegList pinned;
    LiftoffRegister size = pinned.set(
        PopMemTypeToRegister(decoder, &mem_offsets_high_word, &pinned));
    LiftoffRegister src = pinned.set(
        PopMemTypeToRegister(decoder, &mem_offsets_high_word, &pinned));
    LiftoffRegister dst = pinned.set(
        PopMemTypeToRegister(decoder, &mem_offsets_high_word, &pinned));

    Register instance = __ cache_state()->cached_instance;
    if (instance == no_reg) {
      instance = __ GetUnusedRegister(kGpReg, pinned).gp();
      __ LoadInstanceFromFrame(instance);
    }

    // Only allocate the OOB code now, so the state of the stack is reflected
    // correctly.
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapMemOutOfBounds);
    if (mem_offsets_high_word != no_reg) {
      // If any high word has bits set, jump to the OOB trap.
      FREEZE_STATE(trapping);
      __ emit_cond_jump(kNotEqualZero, trap_label, kI32, mem_offsets_high_word,
                        no_reg, trapping);
    }

    auto sig = MakeSig::Returns(kI32).Params(kPointerKind, kPointerKind,
                                             kPointerKind, kPointerKind);
    LiftoffRegister args[] = {LiftoffRegister(instance), dst, src, size};
    // We don't need the instance anymore after the call. We can use the
    // register for the result.
    LiftoffRegister result(instance);
    GenerateCCall(&result, &sig, kVoid, args,
                  ExternalReference::wasm_memory_copy());
    FREEZE_STATE(trapping);
    __ emit_cond_jump(kEqual, trap_label, kI32, result.gp(), no_reg, trapping);
  }

  void MemoryFill(FullDecoder* decoder,
                  const MemoryIndexImmediate<validate>& imm, const Value&,
                  const Value&, const Value&) {
    Register mem_offsets_high_word = no_reg;
    LiftoffRegList pinned;
    LiftoffRegister size = pinned.set(
        PopMemTypeToRegister(decoder, &mem_offsets_high_word, &pinned));
    LiftoffRegister value = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister dst = pinned.set(
        PopMemTypeToRegister(decoder, &mem_offsets_high_word, &pinned));

    Register instance = __ cache_state()->cached_instance;
    if (instance == no_reg) {
      instance = __ GetUnusedRegister(kGpReg, pinned).gp();
      __ LoadInstanceFromFrame(instance);
    }

    // Only allocate the OOB code now, so the state of the stack is reflected
    // correctly.
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapMemOutOfBounds);
    if (mem_offsets_high_word != no_reg) {
      // If any high word has bits set, jump to the OOB trap.
      FREEZE_STATE(trapping);
      __ emit_cond_jump(kNotEqualZero, trap_label, kI32, mem_offsets_high_word,
                        no_reg, trapping);
    }

    auto sig = MakeSig::Returns(kI32).Params(kPointerKind, kPointerKind, kI32,
                                             kPointerKind);
    LiftoffRegister args[] = {LiftoffRegister(instance), dst, value, size};
    // We don't need the instance anymore after the call. We can use the
    // register for the result.
    LiftoffRegister result(instance);
    GenerateCCall(&result, &sig, kVoid, args,
                  ExternalReference::wasm_memory_fill());
    FREEZE_STATE(trapping);
    __ emit_cond_jump(kEqual, trap_label, kI32, result.gp(), no_reg, trapping);
  }

  void LoadSmi(LiftoffRegister reg, int value) {
    Address smi_value = Smi::FromInt(value).ptr();
    using smi_type = std::conditional_t<kSmiKind == kI32, int32_t, int64_t>;
    __ LoadConstant(reg, WasmValue{static_cast<smi_type>(smi_value)});
  }

  void TableInit(FullDecoder* decoder, const TableInitImmediate<validate>& imm,
                 base::Vector<Value> args) {
    LiftoffRegList pinned;
    LiftoffRegister table_index_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));

    LoadSmi(table_index_reg, imm.table.index);
    LiftoffAssembler::VarState table_index(kPointerKind, table_index_reg, 0);

    LiftoffRegister segment_index_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(segment_index_reg, imm.element_segment.index);
    LiftoffAssembler::VarState segment_index(kPointerKind, segment_index_reg,
                                             0);

    LiftoffAssembler::VarState size = __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState src = __ cache_state()->stack_state.end()[-2];
    LiftoffAssembler::VarState dst = __ cache_state()->stack_state.end()[-3];

    CallRuntimeStub(WasmCode::kWasmTableInit,
                    MakeSig::Params(kI32, kI32, kI32, kSmiKind, kSmiKind),
                    {dst, src, size, table_index, segment_index},
                    decoder->position());

    // Pop parameters from the value stack.
    __ cache_state()->stack_state.pop_back(3);

    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);
  }

  void ElemDrop(FullDecoder* decoder, const IndexImmediate<validate>& imm) {
    LiftoffRegList pinned;
    Register dropped_elem_segments =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_TAGGED_PTR_INSTANCE_FIELD(dropped_elem_segments, DroppedElemSegments,
                                   pinned);

    LiftoffRegister seg_index =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(
        seg_index,
        WasmValue(wasm::ObjectAccess::ElementOffsetInTaggedFixedUInt8Array(
            imm.index)));

    // Mark the segment as dropped by setting its value in the dropped
    // segments list to 1.
    LiftoffRegister one_reg = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(one_reg, WasmValue(1));
    __ Store(dropped_elem_segments, seg_index.gp(), 0, one_reg,
             StoreType::kI32Store8, pinned);
  }

  void TableCopy(FullDecoder* decoder, const TableCopyImmediate<validate>& imm,
                 base::Vector<Value> args) {
    LiftoffRegList pinned;

    LiftoffRegister table_dst_index_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(table_dst_index_reg, imm.table_dst.index);
    LiftoffAssembler::VarState table_dst_index(kPointerKind,
                                               table_dst_index_reg, 0);

    LiftoffRegister table_src_index_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(table_src_index_reg, imm.table_src.index);
    LiftoffAssembler::VarState table_src_index(kPointerKind,
                                               table_src_index_reg, 0);

    LiftoffAssembler::VarState size = __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState src = __ cache_state()->stack_state.end()[-2];
    LiftoffAssembler::VarState dst = __ cache_state()->stack_state.end()[-3];

    CallRuntimeStub(WasmCode::kWasmTableCopy,
                    MakeSig::Params(kI32, kI32, kI32, kSmiKind, kSmiKind),
                    {dst, src, size, table_dst_index, table_src_index},
                    decoder->position());

    // Pop parameters from the value stack.
    __ cache_state()->stack_state.pop_back(3);

    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);
  }

  void TableGrow(FullDecoder* decoder, const IndexImmediate<validate>& imm,
                 const Value&, const Value&, Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister table_index_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(table_index_reg, imm.index);
    LiftoffAssembler::VarState table_index(kPointerKind, table_index_reg, 0);

    LiftoffAssembler::VarState delta = __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState value = __ cache_state()->stack_state.end()[-2];

    CallRuntimeStub(
        WasmCode::kWasmTableGrow,
        MakeSig::Returns(kSmiKind).Params(kSmiKind, kI32, kTaggedKind),
        {table_index, delta, value}, decoder->position());

    // Pop parameters from the value stack.
    __ cache_state()->stack_state.pop_back(2);

    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);
    __ SmiToInt32(kReturnRegister0);
    __ PushRegister(kI32, LiftoffRegister(kReturnRegister0));
  }

  void TableSize(FullDecoder* decoder, const IndexImmediate<validate>& imm,
                 Value*) {
    // We have to look up instance->tables[table_index].length.

    LiftoffRegList pinned;
    // Get the number of calls array address.
    Register tables = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LOAD_TAGGED_PTR_INSTANCE_FIELD(tables, Tables, pinned);

    Register table = tables;
    __ LoadTaggedPointer(
        table, tables, no_reg,
        ObjectAccess::ElementOffsetInTaggedFixedArray(imm.index));

    int length_field_size = WasmTableObject::kCurrentLengthOffsetEnd -
                            WasmTableObject::kCurrentLengthOffset + 1;

    Register result = table;
    __ Load(LiftoffRegister(result), table, no_reg,
            wasm::ObjectAccess::ToTagged(WasmTableObject::kCurrentLengthOffset),
            length_field_size == 4 ? LoadType::kI32Load : LoadType::kI64Load);

    __ SmiUntag(result);
    __ PushRegister(kI32, LiftoffRegister(result));
  }

  void TableFill(FullDecoder* decoder, const IndexImmediate<validate>& imm,
                 const Value&, const Value&, const Value&) {
    LiftoffRegList pinned;

    LiftoffRegister table_index_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(table_index_reg, imm.index);
    LiftoffAssembler::VarState table_index(kPointerKind, table_index_reg, 0);

    LiftoffAssembler::VarState count = __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState value = __ cache_state()->stack_state.end()[-2];
    LiftoffAssembler::VarState start = __ cache_state()->stack_state.end()[-3];

    CallRuntimeStub(WasmCode::kWasmTableFill,
                    MakeSig::Params(kSmiKind, kI32, kI32, kTaggedKind),
                    {table_index, start, count, value}, decoder->position());

    // Pop parameters from the value stack.
    __ cache_state()->stack_state.pop_back(3);

    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);
  }

  void StructNew(FullDecoder* decoder,
                 const StructIndexImmediate<validate>& imm, const Value& rtt,
                 bool initial_values_on_stack) {
    LiftoffRegList pinned;
    LiftoffRegister instance_size =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffAssembler::VarState instance_size_state(kI32, instance_size, 0);
    LiftoffAssembler::VarState rtt_value =
        __ cache_state()->stack_state.end()[-1];

    __ LoadConstant(
        instance_size,
        WasmValue(static_cast<int32_t>(WasmStruct::Size(imm.struct_type))));
    CallRuntimeStub(WasmCode::kWasmAllocateStructWithRtt,
                    MakeSig::Returns(kRef).Params(rtt.type.kind(), kI32),
                    {rtt_value, instance_size_state}, decoder->position());
    // Drop the RTT.
    __ cache_state()->stack_state.pop_back(1);

    LiftoffRegister obj(kReturnRegister0);
    pinned.set(obj);

    for (uint32_t i = imm.struct_type->field_count(); i > 0;) {
      i--;
      int offset = StructFieldOffset(imm.struct_type, i);
      ValueKind field_kind = imm.struct_type->field(i).kind();
      LiftoffRegister value = pinned.set(
          initial_values_on_stack
              ? __ PopToRegister(pinned)
              : __ GetUnusedRegister(reg_class_for(field_kind), pinned));
      if (!initial_values_on_stack) {
        if (!CheckSupportedType(decoder, field_kind, "default value")) return;
        SetDefaultValue(value, field_kind, pinned);
      }
      StoreObjectField(obj.gp(), no_reg, offset, value, pinned, field_kind);
      pinned.clear(value);
    }
    // If this assert fails then initialization of padding field might be
    // necessary.
    static_assert(Heap::kMinObjectSizeInTaggedWords == 2 &&
                      WasmStruct::kHeaderSize == 2 * kTaggedSize,
                  "empty struct might require initialization of padding field");
    __ PushRegister(kRef, obj);
  }

  void StructNew(FullDecoder* decoder,
                 const StructIndexImmediate<validate>& imm, const Value& rtt,
                 const Value args[], Value* result) {
    StructNew(decoder, imm, rtt, true);
  }

  void StructNewDefault(FullDecoder* decoder,
                        const StructIndexImmediate<validate>& imm,
                        const Value& rtt, Value* result) {
    StructNew(decoder, imm, rtt, false);
  }

  void StructGet(FullDecoder* decoder, const Value& struct_obj,
                 const FieldImmediate<validate>& field, bool is_signed,
                 Value* result) {
    const StructType* struct_type = field.struct_imm.struct_type;
    ValueKind field_kind = struct_type->field(field.field_imm.index).kind();
    if (!CheckSupportedType(decoder, field_kind, "field load")) return;
    int offset = StructFieldOffset(struct_type, field.field_imm.index);
    LiftoffRegList pinned;
    LiftoffRegister obj = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, obj.gp(), pinned, struct_obj.type);
    LiftoffRegister value =
        __ GetUnusedRegister(reg_class_for(field_kind), pinned);
    LoadObjectField(value, obj.gp(), no_reg, offset, field_kind, is_signed,
                    pinned);
    __ PushRegister(unpacked(field_kind), value);
  }

  void StructSet(FullDecoder* decoder, const Value& struct_obj,
                 const FieldImmediate<validate>& field,
                 const Value& field_value) {
    const StructType* struct_type = field.struct_imm.struct_type;
    ValueKind field_kind = struct_type->field(field.field_imm.index).kind();
    int offset = StructFieldOffset(struct_type, field.field_imm.index);
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister obj = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, obj.gp(), pinned, struct_obj.type);
    StoreObjectField(obj.gp(), no_reg, offset, value, pinned, field_kind);
  }

  void ArrayNew(FullDecoder* decoder, const ArrayIndexImmediate<validate>& imm,
                ValueKind rtt_kind, bool initial_value_on_stack) {
    // Max length check.
    {
      LiftoffRegister length =
          __ LoadToRegister(__ cache_state()->stack_state.end()[-2], {});
      Label* trap_label =
          AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapArrayTooLarge);
      FREEZE_STATE(trapping);
      __ emit_i32_cond_jumpi(kUnsignedGreaterThan, trap_label, length.gp(),
                             WasmArray::MaxLength(imm.array_type), trapping);
    }
    ValueKind elem_kind = imm.array_type->element_type().kind();
    int elem_size = value_kind_size(elem_kind);
    // Allocate the array.
    {
      LiftoffRegister elem_size_reg = __ GetUnusedRegister(kGpReg, {});
      LiftoffAssembler::VarState rtt_var =
          __ cache_state()->stack_state.end()[-1];
      LiftoffAssembler::VarState length_var =
          __ cache_state()->stack_state.end()[-2];
      __ LoadConstant(elem_size_reg, WasmValue(elem_size));
      LiftoffAssembler::VarState elem_size_var(kI32, elem_size_reg, 0);
      CallRuntimeStub(WasmCode::kWasmAllocateArray_Uninitialized,
                      MakeSig::Returns(kRef).Params(rtt_kind, kI32, kI32),
                      {rtt_var, length_var, elem_size_var},
                      decoder->position());
      // Drop the RTT.
      __ cache_state()->stack_state.pop_back(1);
    }

    LiftoffRegister obj(kReturnRegister0);
    LiftoffRegList pinned{obj};
    LiftoffRegister length = pinned.set(__ PopToModifiableRegister(pinned));
    LiftoffRegister value =
        pinned.set(__ GetUnusedRegister(reg_class_for(elem_kind), pinned));
    if (initial_value_on_stack) {
      __ PopToFixedRegister(value);
    } else {
      if (!CheckSupportedType(decoder, elem_kind, "default value")) return;
      SetDefaultValue(value, elem_kind, pinned);
    }
    // Initialize the array's elements.
    LiftoffRegister offset = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(
        offset,
        WasmValue(wasm::ObjectAccess::ToTagged(WasmArray::kHeaderSize)));
    LiftoffRegister end_offset = length;
    if (value_kind_size_log2(elem_kind) != 0) {
      __ emit_i32_shli(end_offset.gp(), length.gp(),
                       value_kind_size_log2(elem_kind));
    }
    __ emit_i32_add(end_offset.gp(), end_offset.gp(), offset.gp());
    Label loop, done;
    __ bind(&loop);
    {
      // This is subtle: {StoreObjectField} can request a temp register, which
      // is precisely what {FREEZE_STATE} (with non-trivial live range) is
      // supposed to guard against. In this case it's fine though, because we've
      // just done a call, so there are plenty of recently-spilled unused
      // registers, so requesting a temp register won't actually cause any state
      // changes.
      // TODO(jkummerow): See if we can make this more elegant, e.g. by passing
      // a temp register to {StoreObjectField}.
      FREEZE_STATE(in_this_case_its_fine);
      __ emit_cond_jump(kUnsignedGreaterEqual, &done, kI32, offset.gp(),
                        end_offset.gp(), in_this_case_its_fine);
    }
    StoreObjectField(obj.gp(), offset.gp(), 0, value, pinned, elem_kind);
    __ emit_i32_addi(offset.gp(), offset.gp(), elem_size);
    __ emit_jump(&loop);

    __ bind(&done);

    __ PushRegister(kRef, obj);
  }

  void ArrayNew(FullDecoder* decoder, const ArrayIndexImmediate<validate>& imm,
                const Value& length_value, const Value& initial_value,
                const Value& rtt, Value* result) {
    ArrayNew(decoder, imm, rtt.type.kind(), true);
  }

  void ArrayNewDefault(FullDecoder* decoder,
                       const ArrayIndexImmediate<validate>& imm,
                       const Value& length, const Value& rtt, Value* result) {
    ArrayNew(decoder, imm, rtt.type.kind(), false);
  }

  void ArrayGet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate<validate>& imm,
                const Value& index_val, bool is_signed, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister index = pinned.set(__ PopToModifiableRegister(pinned));
    LiftoffRegister array = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, array.gp(), pinned, array_obj.type);
    BoundsCheckArray(decoder, array, index, pinned);
    ValueKind elem_kind = imm.array_type->element_type().kind();
    if (!CheckSupportedType(decoder, elem_kind, "array load")) return;
    int elem_size_shift = value_kind_size_log2(elem_kind);
    if (elem_size_shift != 0) {
      __ emit_i32_shli(index.gp(), index.gp(), elem_size_shift);
    }
    LiftoffRegister value =
        __ GetUnusedRegister(reg_class_for(elem_kind), pinned);
    LoadObjectField(value, array.gp(), index.gp(),
                    wasm::ObjectAccess::ToTagged(WasmArray::kHeaderSize),
                    elem_kind, is_signed, pinned);
    __ PushRegister(unpacked(elem_kind), value);
  }

  void ArraySet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate<validate>& imm,
                const Value& index_val, const Value& value_val) {
    LiftoffRegList pinned;
    LiftoffRegister value = pinned.set(__ PopToRegister(pinned));
    DCHECK_EQ(reg_class_for(imm.array_type->element_type().kind()),
              value.reg_class());
    LiftoffRegister index = pinned.set(__ PopToModifiableRegister(pinned));
    LiftoffRegister array = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, array.gp(), pinned, array_obj.type);
    BoundsCheckArray(decoder, array, index, pinned);
    ValueKind elem_kind = imm.array_type->element_type().kind();
    int elem_size_shift = value_kind_size_log2(elem_kind);
    if (elem_size_shift != 0) {
      __ emit_i32_shli(index.gp(), index.gp(), elem_size_shift);
    }
    StoreObjectField(array.gp(), index.gp(),
                     wasm::ObjectAccess::ToTagged(WasmArray::kHeaderSize),
                     value, pinned, elem_kind);
  }

  void ArrayLen(FullDecoder* decoder, const Value& array_obj, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister obj = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, obj.gp(), pinned, array_obj.type);
    LiftoffRegister len = __ GetUnusedRegister(kGpReg, pinned);
    int kLengthOffset = wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset);
    LoadObjectField(len, obj.gp(), no_reg, kLengthOffset, kI32, false, pinned);
    __ PushRegister(kI32, len);
  }

  void ArrayCopy(FullDecoder* decoder, const Value& dst, const Value& dst_index,
                 const Value& src, const Value& src_index,
                 const Value& length) {
    // TODO(7748): Unify implementation with TF: Implement this with
    // GenerateCCall. Remove runtime function and builtin in wasm.tq.
    CallRuntimeStub(v8_flags.experimental_wasm_skip_bounds_checks
                        ? WasmCode::kWasmArrayCopy
                        : WasmCode::kWasmArrayCopyWithChecks,
                    MakeSig::Params(kI32, kI32, kI32, kRefNull, kRefNull),
                    // Builtin parameter order:
                    // [dst_index, src_index, length, dst, src].
                    {__ cache_state()->stack_state.end()[-4],
                     __ cache_state()->stack_state.end()[-2],
                     __ cache_state()->stack_state.end()[-1],
                     __ cache_state()->stack_state.end()[-5],
                     __ cache_state()->stack_state.end()[-3]},
                    decoder->position());
    __ cache_state()->stack_state.pop_back(5);
  }

  void ArrayNewFixed(FullDecoder* decoder,
                     const ArrayIndexImmediate<validate>& imm,
                     const base::Vector<Value>& elements, const Value& rtt,
                     Value* result) {
    ValueKind rtt_kind = rtt.type.kind();
    ValueKind elem_kind = imm.array_type->element_type().kind();
    // Allocate the array.
    {
      LiftoffRegList pinned;
      LiftoffRegister elem_size_reg =
          pinned.set(__ GetUnusedRegister(kGpReg, pinned));

      __ LoadConstant(elem_size_reg, WasmValue(value_kind_size(elem_kind)));
      LiftoffAssembler::VarState elem_size_var(kI32, elem_size_reg, 0);

      LiftoffRegister length_reg =
          pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      __ LoadConstant(length_reg,
                      WasmValue(static_cast<int32_t>(elements.size())));
      LiftoffAssembler::VarState length_var(kI32, length_reg, 0);

      LiftoffAssembler::VarState rtt_var =
          __ cache_state()->stack_state.end()[-1];

      CallRuntimeStub(WasmCode::kWasmAllocateArray_Uninitialized,
                      MakeSig::Returns(kRef).Params(rtt_kind, kI32, kI32),
                      {rtt_var, length_var, elem_size_var},
                      decoder->position());
      // Drop the RTT.
      __ DropValues(1);
    }

    // Initialize the array with stack arguments.
    LiftoffRegister array(kReturnRegister0);
    if (!CheckSupportedType(decoder, elem_kind, "array.new_fixed")) return;
    for (int i = static_cast<int>(elements.size()) - 1; i >= 0; i--) {
      LiftoffRegList pinned{array};
      LiftoffRegister element = pinned.set(__ PopToRegister(pinned));
      LiftoffRegister offset_reg =
          pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      __ LoadConstant(offset_reg,
                      WasmValue(i << value_kind_size_log2(elem_kind)));
      StoreObjectField(array.gp(), offset_reg.gp(),
                       wasm::ObjectAccess::ToTagged(WasmArray::kHeaderSize),
                       element, pinned, elem_kind);
    }

    // Push the array onto the stack.
    __ PushRegister(kRef, array);
  }

  void ArrayNewSegment(FullDecoder* decoder,
                       const ArrayIndexImmediate<validate>& array_imm,
                       const IndexImmediate<validate>& data_segment,
                       const Value& /* offset */, const Value& /* length */,
                       const Value& /* rtt */, Value* /* result */) {
    LiftoffRegList pinned;
    LiftoffRegister data_segment_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(data_segment_reg,
                    WasmValue(static_cast<int32_t>(data_segment.index)));
    LiftoffAssembler::VarState data_segment_var(kI32, data_segment_reg, 0);

    CallRuntimeStub(WasmCode::kWasmArrayNewSegment,
                    MakeSig::Returns(kRef).Params(kI32, kI32, kI32, kRtt),
                    {
                        data_segment_var,
                        __ cache_state()->stack_state.end()[-3],  // offset
                        __ cache_state()->stack_state.end()[-2],  // length
                        __ cache_state()->stack_state.end()[-1]   // rtt
                    },
                    decoder->position());

    // Pop parameters from the value stack.
    __ cache_state()->stack_state.pop_back(3);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result(kReturnRegister0);
    __ PushRegister(kRef, result);
  }

  // 1 bit Smi tag, 31 bits Smi shift, 1 bit i31ref high-bit truncation.
  constexpr static int kI31To32BitSmiShift = 33;

  void I31New(FullDecoder* decoder, const Value& input, Value* result) {
    LiftoffRegister src = __ PopToRegister();
    LiftoffRegister dst = __ GetUnusedRegister(kGpReg, {src}, {});
    if (SmiValuesAre31Bits()) {
      static_assert(kSmiTag == 0);
      __ emit_i32_shli(dst.gp(), src.gp(), kSmiTagSize);
    } else {
      DCHECK(SmiValuesAre32Bits());
      __ emit_i64_shli(dst, src, kI31To32BitSmiShift);
    }
    __ PushRegister(kRef, dst);
  }

  void I31GetS(FullDecoder* decoder, const Value& input, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister src = pinned.set(__ PopToRegister());
    MaybeEmitNullCheck(decoder, src.gp(), pinned, input.type);
    LiftoffRegister dst = __ GetUnusedRegister(kGpReg, {src}, {});
    if (SmiValuesAre31Bits()) {
      __ emit_i32_sari(dst.gp(), src.gp(), kSmiTagSize);
    } else {
      DCHECK(SmiValuesAre32Bits());
      __ emit_i64_sari(dst, src, kI31To32BitSmiShift);
    }
    __ PushRegister(kI32, dst);
  }

  void I31GetU(FullDecoder* decoder, const Value& input, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister src = pinned.set(__ PopToRegister());
    MaybeEmitNullCheck(decoder, src.gp(), pinned, input.type);
    LiftoffRegister dst = __ GetUnusedRegister(kGpReg, {src}, {});
    if (SmiValuesAre31Bits()) {
      __ emit_i32_shri(dst.gp(), src.gp(), kSmiTagSize);
    } else {
      DCHECK(SmiValuesAre32Bits());
      __ emit_i64_shri(dst, src, kI31To32BitSmiShift);
    }
    __ PushRegister(kI32, dst);
  }

  void RttCanon(FullDecoder* decoder, uint32_t type_index, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister rtt = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LOAD_TAGGED_PTR_INSTANCE_FIELD(rtt.gp(), ManagedObjectMaps, pinned);
    __ LoadTaggedPointer(
        rtt.gp(), rtt.gp(), no_reg,
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(type_index));
    __ PushRegister(kRtt, rtt);
  }

  enum NullSucceeds : bool {  // --
    kNullSucceeds = true,
    kNullFails = false
  };

  // Falls through on match (=successful type check).
  // Returns the register containing the object.
  void SubtypeCheck(const WasmModule* module, Register obj_reg,
                    ValueType obj_type, Register rtt_reg, ValueType rtt_type,
                    Register scratch_null, Register scratch2, Label* no_match,
                    NullSucceeds null_succeeds,
                    const FreezeCacheState& frozen) {
    Label match;

    if (obj_type.is_nullable()) {
      __ emit_cond_jump(kEqual, null_succeeds ? &match : no_match,
                        obj_type.kind(), obj_reg, scratch_null, frozen);
    }
    Register tmp1 = scratch_null;  // Done with null checks.

    __ LoadMap(tmp1, obj_reg);
    // {tmp1} now holds the object's map.

    // Check for rtt equality, and if not, check if the rtt is a struct/array
    // rtt.
    __ emit_cond_jump(kEqual, &match, rtt_type.kind(), tmp1, rtt_reg, frozen);

    // Constant-time subtyping check: load exactly one candidate RTT from the
    // supertypes list.
    // Step 1: load the WasmTypeInfo into {tmp1}.
    constexpr int kTypeInfoOffset = wasm::ObjectAccess::ToTagged(
        Map::kConstructorOrBackPointerOrNativeContextOffset);
    __ LoadTaggedPointer(tmp1, tmp1, no_reg, kTypeInfoOffset);
    // Step 2: check the list's length if needed.
    uint32_t rtt_depth = GetSubtypingDepth(module, rtt_type.ref_index());
    if (rtt_depth >= kMinimumSupertypeArraySize) {
      LiftoffRegister list_length(scratch2);
      int offset =
          ObjectAccess::ToTagged(WasmTypeInfo::kSupertypesLengthOffset);
      __ LoadSmiAsInt32(list_length, tmp1, offset);
      __ emit_i32_cond_jumpi(kUnsignedLessEqual, no_match, list_length.gp(),
                             rtt_depth, frozen);
    }
    // Step 3: load the candidate list slot into {tmp1}, and compare it.
    __ LoadTaggedPointer(
        tmp1, tmp1, no_reg,
        ObjectAccess::ToTagged(WasmTypeInfo::kSupertypesOffset +
                               rtt_depth * kTaggedSize));
    __ emit_cond_jump(kUnequal, no_match, rtt_type.kind(), tmp1, rtt_reg,
                      frozen);

    // Fall through to {match}.
    __ bind(&match);
  }

  void RefTest(FullDecoder* decoder, const Value& obj, const Value& rtt,
               Value* /* result_val */) {
    Label return_false, done;
    LiftoffRegList pinned;
    LiftoffRegister rtt_reg = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister obj_reg = pinned.set(__ PopToRegister(pinned));
    Register scratch_null =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    LiftoffRegister result = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    if (obj.type.is_nullable()) LoadNullValue(scratch_null, pinned);

    {
      FREEZE_STATE(frozen);
      SubtypeCheck(decoder->module_, obj_reg.gp(), obj.type, rtt_reg.gp(),
                   rtt.type, scratch_null, result.gp(), &return_false,
                   kNullFails, frozen);

      __ LoadConstant(result, WasmValue(1));
      // TODO(jkummerow): Emit near jumps on platforms that have them.
      __ emit_jump(&done);

      __ bind(&return_false);
      __ LoadConstant(result, WasmValue(0));
      __ bind(&done);
    }
    __ PushRegister(kI32, result);
  }

  void RefCast(FullDecoder* decoder, const Value& obj, const Value& rtt,
               Value* result) {
    if (v8_flags.experimental_wasm_assume_ref_cast_succeeds) {
      // Just drop the rtt.
      __ DropValues(1);
      return;
    }
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapIllegalCast);
    LiftoffRegList pinned;
    LiftoffRegister rtt_reg = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister obj_reg = pinned.set(__ PopToRegister(pinned));
    Register scratch_null =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register scratch2 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    if (obj.type.is_nullable()) LoadNullValue(scratch_null, pinned);

    {
      FREEZE_STATE(frozen);
      SubtypeCheck(decoder->module_, obj_reg.gp(), obj.type, rtt_reg.gp(),
                   rtt.type, scratch_null, scratch2, trap_label, kNullSucceeds,
                   frozen);
    }
    __ PushRegister(obj.type.kind(), obj_reg);
  }

  void BrOnCast(FullDecoder* decoder, const Value& obj, const Value& rtt,
                Value* /* result_on_branch */, uint32_t depth) {
    // Avoid having sequences of branches do duplicate work.
    if (depth != decoder->control_depth() - 1) {
      __ PrepareForBranch(decoder->control_at(depth)->br_merge()->arity, {});
    }

    Label cont_false;
    LiftoffRegList pinned;
    LiftoffRegister rtt_reg = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister obj_reg = pinned.set(__ PeekToRegister(0, pinned));
    Register scratch_null =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register scratch2 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    if (obj.type.is_nullable()) LoadNullValue(scratch_null, pinned);
    FREEZE_STATE(frozen);

    SubtypeCheck(decoder->module_, obj_reg.gp(), obj.type, rtt_reg.gp(),
                 rtt.type, scratch_null, scratch2, &cont_false, kNullFails,
                 frozen);

    BrOrRetImpl(decoder, depth, scratch_null, scratch2);

    __ bind(&cont_false);
  }

  void BrOnCastFail(FullDecoder* decoder, const Value& obj, const Value& rtt,
                    Value* /* result_on_fallthrough */, uint32_t depth) {
    // Avoid having sequences of branches do duplicate work.
    if (depth != decoder->control_depth() - 1) {
      __ PrepareForBranch(decoder->control_at(depth)->br_merge()->arity, {});
    }

    Label cont_branch, fallthrough;
    LiftoffRegList pinned;
    LiftoffRegister rtt_reg = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister obj_reg = pinned.set(__ PeekToRegister(0, pinned));
    Register scratch_null =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register scratch2 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    if (obj.type.is_nullable()) LoadNullValue(scratch_null, pinned);
    FREEZE_STATE(frozen);

    SubtypeCheck(decoder->module_, obj_reg.gp(), obj.type, rtt_reg.gp(),
                 rtt.type, scratch_null, scratch2, &cont_branch, kNullFails,
                 frozen);
    __ emit_jump(&fallthrough);

    __ bind(&cont_branch);
    BrOrRetImpl(decoder, depth, scratch_null, scratch2);

    __ bind(&fallthrough);
  }

  struct TypeCheck {
    Register obj_reg = no_reg;
    ValueType obj_type;
    Register tmp1 = no_reg;
    Register tmp2 = no_reg;
    Label* no_match;

    TypeCheck(ValueType obj_type, Label* no_match)
        : obj_type(obj_type), no_match(no_match) {}

    Register null_reg() { return tmp1; }       // After {Initialize}.
    Register instance_type() { return tmp1; }  // After {LoadInstanceType}.
  };

  enum PopOrPeek { kPop, kPeek };

  void Initialize(TypeCheck& check, PopOrPeek pop_or_peek) {
    LiftoffRegList pinned;
    if (pop_or_peek == kPop) {
      check.obj_reg = pinned.set(__ PopToRegister(pinned)).gp();
    } else {
      check.obj_reg = pinned.set(__ PeekToRegister(0, pinned)).gp();
    }
    check.tmp1 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    check.tmp2 = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    if (check.obj_type.is_nullable()) {
      LoadNullValue(check.null_reg(), pinned);
    }
  }
  void LoadInstanceType(TypeCheck& check, const FreezeCacheState& frozen) {
    if (check.obj_type.is_nullable()) {
      __ emit_cond_jump(kEqual, check.no_match, kRefNull, check.obj_reg,
                        check.null_reg(), frozen);
    }
    __ emit_smi_check(check.obj_reg, check.no_match,
                      LiftoffAssembler::kJumpOnSmi, frozen);
    __ LoadMap(check.instance_type(), check.obj_reg);
    __ Load(LiftoffRegister(check.instance_type()), check.instance_type(),
            no_reg, wasm::ObjectAccess::ToTagged(Map::kInstanceTypeOffset),
            LoadType::kI32Load16U);
  }

  // Abstract type checkers. They all fall through on match.
  void DataCheck(TypeCheck& check, const FreezeCacheState& frozen) {
    LoadInstanceType(check, frozen);
    // We're going to test a range of WasmObject instance types with a single
    // unsigned comparison.
    Register tmp = check.instance_type();
    __ emit_i32_subi(tmp, tmp, FIRST_WASM_OBJECT_TYPE);
    __ emit_i32_cond_jumpi(kUnsignedGreaterThan, check.no_match, tmp,
                           LAST_WASM_OBJECT_TYPE - FIRST_WASM_OBJECT_TYPE,
                           frozen);
  }

  void ArrayCheck(TypeCheck& check, const FreezeCacheState& frozen) {
    LoadInstanceType(check, frozen);
    LiftoffRegister instance_type(check.instance_type());
    __ emit_i32_cond_jumpi(kUnequal, check.no_match, check.instance_type(),
                           WASM_ARRAY_TYPE, frozen);
  }

  void I31Check(TypeCheck& check, const FreezeCacheState& frozen) {
    __ emit_smi_check(check.obj_reg, check.no_match,
                      LiftoffAssembler::kJumpOnNotSmi, frozen);
  }

  using TypeChecker = void (LiftoffCompiler::*)(TypeCheck& check,
                                                const FreezeCacheState& frozen);

  template <TypeChecker type_checker>
  void AbstractTypeCheck(const Value& object) {
    Label match, no_match, done;
    TypeCheck check(object.type, &no_match);
    Initialize(check, kPop);
    LiftoffRegister result(check.tmp1);
    {
      FREEZE_STATE(frozen);

      (this->*type_checker)(check, frozen);

      __ bind(&match);
      __ LoadConstant(result, WasmValue(1));
      // TODO(jkummerow): Emit near jumps on platforms that have them.
      __ emit_jump(&done);

      __ bind(&no_match);
      __ LoadConstant(result, WasmValue(0));
      __ bind(&done);
    }
    __ PushRegister(kI32, result);
  }

  void RefIsData(FullDecoder* /* decoder */, const Value& object,
                 Value* /* result_val */) {
    AbstractTypeCheck<&LiftoffCompiler::DataCheck>(object);
  }

  void RefIsArray(FullDecoder* /* decoder */, const Value& object,
                  Value* /* result_val */) {
    AbstractTypeCheck<&LiftoffCompiler::ArrayCheck>(object);
  }

  void RefIsI31(FullDecoder* decoder, const Value& object,
                Value* /* result */) {
    AbstractTypeCheck<&LiftoffCompiler::I31Check>(object);
  }

  template <TypeChecker type_checker>
  void AbstractTypeCast(const Value& object, FullDecoder* decoder,
                        ValueKind result_kind) {
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapIllegalCast);
    TypeCheck check(object.type, trap_label);
    Initialize(check, kPeek);
    FREEZE_STATE(frozen);
    (this->*type_checker)(check, frozen);
  }

  void RefAsData(FullDecoder* decoder, const Value& object,
                 Value* /* result */) {
    AbstractTypeCast<&LiftoffCompiler::DataCheck>(object, decoder, kRef);
  }

  void RefAsI31(FullDecoder* decoder, const Value& object, Value* result) {
    AbstractTypeCast<&LiftoffCompiler::I31Check>(object, decoder, kRef);
  }

  void RefAsArray(FullDecoder* decoder, const Value& object, Value* result) {
    AbstractTypeCast<&LiftoffCompiler::ArrayCheck>(object, decoder, kRef);
  }

  template <TypeChecker type_checker>
  void BrOnAbstractType(const Value& object, FullDecoder* decoder,
                        uint32_t br_depth) {
    // Avoid having sequences of branches do duplicate work.
    if (br_depth != decoder->control_depth() - 1) {
      __ PrepareForBranch(decoder->control_at(br_depth)->br_merge()->arity, {});
    }

    Label no_match;
    TypeCheck check(object.type, &no_match);
    Initialize(check, kPeek);
    FREEZE_STATE(frozen);

    (this->*type_checker)(check, frozen);
    BrOrRetImpl(decoder, br_depth, check.tmp1, check.tmp2);

    __ bind(&no_match);
  }

  template <TypeChecker type_checker>
  void BrOnNonAbstractType(const Value& object, FullDecoder* decoder,
                           uint32_t br_depth) {
    // Avoid having sequences of branches do duplicate work.
    if (br_depth != decoder->control_depth() - 1) {
      __ PrepareForBranch(decoder->control_at(br_depth)->br_merge()->arity, {});
    }

    Label no_match, end;
    TypeCheck check(object.type, &no_match);
    Initialize(check, kPeek);
    FREEZE_STATE(frozen);

    (this->*type_checker)(check, frozen);
    __ emit_jump(&end);

    __ bind(&no_match);
    BrOrRetImpl(decoder, br_depth, check.tmp1, check.tmp2);

    __ bind(&end);
  }

  void BrOnData(FullDecoder* decoder, const Value& object,
                Value* /* value_on_branch */, uint32_t br_depth) {
    BrOnAbstractType<&LiftoffCompiler::DataCheck>(object, decoder, br_depth);
  }

  void BrOnI31(FullDecoder* decoder, const Value& object,
               Value* /* value_on_branch */, uint32_t br_depth) {
    BrOnAbstractType<&LiftoffCompiler::I31Check>(object, decoder, br_depth);
  }

  void BrOnArray(FullDecoder* decoder, const Value& object,
                 Value* /* value_on_branch */, uint32_t br_depth) {
    BrOnAbstractType<&LiftoffCompiler::ArrayCheck>(object, decoder, br_depth);
  }

  void BrOnNonData(FullDecoder* decoder, const Value& object,
                   Value* /* value_on_branch */, uint32_t br_depth) {
    BrOnNonAbstractType<&LiftoffCompiler::DataCheck>(object, decoder, br_depth);
  }

  void BrOnNonI31(FullDecoder* decoder, const Value& object,
                  Value* /* value_on_branch */, uint32_t br_depth) {
    BrOnNonAbstractType<&LiftoffCompiler::I31Check>(object, decoder, br_depth);
  }

  void BrOnNonArray(FullDecoder* decoder, const Value& object,
                    Value* /* value_on_branch */, uint32_t br_depth) {
    BrOnNonAbstractType<&LiftoffCompiler::ArrayCheck>(object, decoder,
                                                      br_depth);
  }

  void StringNewWtf8(FullDecoder* decoder,
                     const MemoryIndexImmediate<validate>& imm,
                     const unibrow::Utf8Variant variant, const Value& offset,
                     const Value& size, Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister memory_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(memory_reg, imm.index);
    LiftoffAssembler::VarState memory_var(kSmiKind, memory_reg, 0);

    LiftoffRegister variant_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(variant_reg, static_cast<int32_t>(variant));
    LiftoffAssembler::VarState variant_var(kSmiKind, variant_reg, 0);

    CallRuntimeStub(
        WasmCode::kWasmStringNewWtf8,
        MakeSig::Returns(kRef).Params(kI32, kI32, kSmiKind, kSmiKind),
        {
            __ cache_state()->stack_state.end()[-2],  // offset
            __ cache_state()->stack_state.end()[-1],  // size
            memory_var,
            variant_var,
        },
        decoder->position());
    __ cache_state()->stack_state.pop_back(2);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringNewWtf8Array(FullDecoder* decoder,
                          const unibrow::Utf8Variant variant,
                          const Value& array, const Value& start,
                          const Value& end, Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister array_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-3], pinned));
    MaybeEmitNullCheck(decoder, array_reg.gp(), pinned, array.type);
    LiftoffAssembler::VarState array_var(kRef, array_reg, 0);

    LiftoffRegister variant_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(variant_reg, static_cast<int32_t>(variant));
    LiftoffAssembler::VarState variant_var(kSmiKind, variant_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringNewWtf8Array,
                    MakeSig::Returns(kRef).Params(kI32, kI32, kRef, kSmiKind),
                    {
                        __ cache_state()->stack_state.end()[-2],  // start
                        __ cache_state()->stack_state.end()[-1],  // end
                        array_var,
                        variant_var,
                    },
                    decoder->position());
    __ cache_state()->stack_state.pop_back(3);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringNewWtf16(FullDecoder* decoder,
                      const MemoryIndexImmediate<validate>& imm,
                      const Value& offset, const Value& size, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister memory_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(memory_reg, WasmValue(static_cast<int32_t>(imm.index)));
    LiftoffAssembler::VarState memory_var(kI32, memory_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringNewWtf16,
                    MakeSig::Returns(kRef).Params(kI32, kI32, kI32),
                    {
                        memory_var,
                        __ cache_state()->stack_state.end()[-2],  // offset
                        __ cache_state()->stack_state.end()[-1]   // size
                    },
                    decoder->position());
    __ cache_state()->stack_state.pop_back(2);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringNewWtf16Array(FullDecoder* decoder, const Value& array,
                           const Value& start, const Value& end,
                           Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister array_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-3], pinned));
    MaybeEmitNullCheck(decoder, array_reg.gp(), pinned, array.type);
    LiftoffAssembler::VarState array_var(kRef, array_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringNewWtf16Array,
                    MakeSig::Returns(kRef).Params(kRef, kI32, kI32),
                    {
                        array_var,
                        __ cache_state()->stack_state.end()[-2],  // start
                        __ cache_state()->stack_state.end()[-1],  // end
                    },
                    decoder->position());
    __ cache_state()->stack_state.pop_back(3);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringConst(FullDecoder* decoder,
                   const StringConstImmediate<validate>& imm, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister index_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(index_reg, WasmValue(static_cast<int32_t>(imm.index)));
    LiftoffAssembler::VarState index_var(kI32, index_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringConst,
                    MakeSig::Returns(kRef).Params(kI32),
                    {
                        index_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringMeasureWtf8(FullDecoder* decoder,
                         const unibrow::Utf8Variant variant, const Value& str,
                         Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister string_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, string_reg.gp(), pinned, str.type);
    LiftoffAssembler::VarState string_var(kRef, string_reg, 0);

    WasmCode::RuntimeStubId stub_id;
    switch (variant) {
      case unibrow::Utf8Variant::kUtf8:
        stub_id = WasmCode::kWasmStringMeasureUtf8;
        break;
      case unibrow::Utf8Variant::kLossyUtf8:
      case unibrow::Utf8Variant::kWtf8:
        stub_id = WasmCode::kWasmStringMeasureWtf8;
        break;
    }
    CallRuntimeStub(stub_id, MakeSig::Returns(kI32).Params(kRef),
                    {
                        string_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringMeasureWtf16(FullDecoder* decoder, const Value& str,
                          Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister string_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, string_reg.gp(), pinned, str.type);
    LiftoffRegister value = __ GetUnusedRegister(kGpReg, pinned);
    LoadObjectField(value, string_reg.gp(), no_reg,
                    wasm::ObjectAccess::ToTagged(String::kLengthOffset),
                    ValueKind::kI32, false /* is_signed */, pinned);
    __ PushRegister(kI32, value);
  }

  void StringEncodeWtf8(FullDecoder* decoder,
                        const MemoryIndexImmediate<validate>& imm,
                        const unibrow::Utf8Variant variant, const Value& str,
                        const Value& offset, Value* result) {
    LiftoffRegList pinned;

    LiftoffAssembler::VarState& offset_var =
        __ cache_state()->stack_state.end()[-1];

    LiftoffRegister string_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-2], pinned));
    MaybeEmitNullCheck(decoder, string_reg.gp(), pinned, str.type);
    LiftoffAssembler::VarState string_var(kRef, string_reg, 0);

    LiftoffRegister memory_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(memory_reg, imm.index);
    LiftoffAssembler::VarState memory_var(kSmiKind, memory_reg, 0);

    LiftoffRegister variant_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(variant_reg, static_cast<int32_t>(variant));
    LiftoffAssembler::VarState variant_var(kSmiKind, variant_reg, 0);

    CallRuntimeStub(
        WasmCode::kWasmStringEncodeWtf8,
        MakeSig::Returns(kI32).Params(kRef, kI32, kSmiKind, kSmiKind),
        {
            string_var,
            offset_var,
            memory_var,
            variant_var,
        },
        decoder->position());
    __ DropValues(2);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringEncodeWtf8Array(FullDecoder* decoder,
                             const unibrow::Utf8Variant variant,
                             const Value& str, const Value& array,
                             const Value& start, Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister array_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-2], pinned));
    MaybeEmitNullCheck(decoder, array_reg.gp(), pinned, array.type);
    LiftoffAssembler::VarState array_var(kRef, array_reg, 0);

    LiftoffRegister string_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-3], pinned));
    MaybeEmitNullCheck(decoder, string_reg.gp(), pinned, str.type);
    LiftoffAssembler::VarState string_var(kRef, string_reg, 0);

    LiftoffAssembler::VarState& start_var =
        __ cache_state()->stack_state.end()[-1];

    LiftoffRegister variant_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(variant_reg, static_cast<int32_t>(variant));
    LiftoffAssembler::VarState variant_var(kSmiKind, variant_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringEncodeWtf8Array,
                    MakeSig::Returns(kI32).Params(kRef, kRef, kI32, kSmiKind),
                    {
                        string_var,
                        array_var,
                        start_var,
                        variant_var,
                    },
                    decoder->position());
    __ DropValues(3);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringEncodeWtf16(FullDecoder* decoder,
                         const MemoryIndexImmediate<validate>& imm,
                         const Value& str, const Value& offset, Value* result) {
    LiftoffRegList pinned;

    LiftoffAssembler::VarState& offset_var =
        __ cache_state()->stack_state.end()[-1];

    LiftoffRegister string_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-2], pinned));
    MaybeEmitNullCheck(decoder, string_reg.gp(), pinned, str.type);
    LiftoffAssembler::VarState string_var(kRef, string_reg, 0);

    LiftoffRegister memory_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(memory_reg, imm.index);
    LiftoffAssembler::VarState memory_var(kSmiKind, memory_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringEncodeWtf16,
                    MakeSig::Returns(kI32).Params(kRef, kI32, kSmiKind),
                    {
                        string_var,
                        offset_var,
                        memory_var,
                    },
                    decoder->position());
    __ DropValues(2);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringEncodeWtf16Array(FullDecoder* decoder, const Value& str,
                              const Value& array, const Value& start,
                              Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister array_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-2], pinned));
    MaybeEmitNullCheck(decoder, array_reg.gp(), pinned, array.type);
    LiftoffAssembler::VarState array_var(kRef, array_reg, 0);

    LiftoffRegister string_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-3], pinned));
    MaybeEmitNullCheck(decoder, string_reg.gp(), pinned, str.type);
    LiftoffAssembler::VarState string_var(kRef, string_reg, 0);

    LiftoffAssembler::VarState& start_var =
        __ cache_state()->stack_state.end()[-1];

    CallRuntimeStub(WasmCode::kWasmStringEncodeWtf16Array,
                    MakeSig::Returns(kI32).Params(kRef, kRef, kI32),
                    {
                        string_var,
                        array_var,
                        start_var,
                    },
                    decoder->position());
    __ DropValues(3);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringConcat(FullDecoder* decoder, const Value& head, const Value& tail,
                    Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister tail_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, tail_reg.gp(), pinned, tail.type);
    LiftoffAssembler::VarState tail_var(kRef, tail_reg, 0);

    LiftoffRegister head_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, head_reg.gp(), pinned, head.type);
    LiftoffAssembler::VarState head_var(kRef, head_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringConcat,
                    MakeSig::Returns(kRef).Params(kRef, kRef),
                    {
                        head_var,
                        tail_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringEq(FullDecoder* decoder, const Value& a, const Value& b,
                Value* result) {
    LiftoffRegister result_reg(kReturnRegister0);
    LiftoffRegList pinned{result_reg};
    LiftoffRegister b_reg = pinned.set(__ PopToModifiableRegister(pinned));
    LiftoffRegister a_reg = pinned.set(__ PopToModifiableRegister(pinned));

    __ SpillAllRegisters();

    Label done;

    {
      LiftoffRegister null = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      bool check_for_null = a.type.is_nullable() || b.type.is_nullable();
      if (check_for_null) {
        LoadNullValue(null.gp(), pinned);
      }

      FREEZE_STATE(frozen);

      // If values pointer-equal, result is 1.
      __ LoadConstant(result_reg, WasmValue(int32_t{1}));
      __ emit_cond_jump(LiftoffCondition::kEqual, &done, kRefNull, a_reg.gp(),
                        b_reg.gp(), frozen);

      // Otherwise if either operand is null, result is 0.
      if (check_for_null) {
        __ LoadConstant(result_reg, WasmValue(int32_t{0}));
        if (a.type.is_nullable()) {
          __ emit_cond_jump(LiftoffCondition::kEqual, &done, kRefNull,
                            a_reg.gp(), null.gp(), frozen);
        }
        if (b.type.is_nullable()) {
          __ emit_cond_jump(LiftoffCondition::kEqual, &done, kRefNull,
                            b_reg.gp(), null.gp(), frozen);
        }
      }

      // Ending the frozen state here is fine, because we already spilled the
      // rest of the cache, and the subsequent runtime call will reset the cache
      // state anyway.
    }

    // Operands are pointer-distinct and neither is null; call out to the
    // runtime.
    LiftoffAssembler::VarState a_var(kRef, a_reg, 0);
    LiftoffAssembler::VarState b_var(kRef, b_reg, 0);
    CallRuntimeStub(WasmCode::kWasmStringEqual,
                    MakeSig::Returns(kI32).Params(kRef, kRef),
                    {
                        a_var,
                        b_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    __ bind(&done);

    __ PushRegister(kI32, result_reg);
  }

  void StringIsUSVSequence(FullDecoder* decoder, const Value& str,
                           Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister str_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, str_reg.gp(), pinned, str.type);
    LiftoffAssembler::VarState str_var(kRef, str_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringIsUSVSequence,
                    MakeSig::Returns(kI32).Params(kRef),
                    {
                        str_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringAsWtf8(FullDecoder* decoder, const Value& str, Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister str_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, str_reg.gp(), pinned, str.type);
    LiftoffAssembler::VarState str_var(kRef, str_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringAsWtf8,
                    MakeSig::Returns(kRef).Params(kRef),
                    {
                        str_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringViewWtf8Advance(FullDecoder* decoder, const Value& view,
                             const Value& pos, const Value& bytes,
                             Value* result) {
    LiftoffRegList pinned;

    LiftoffAssembler::VarState& bytes_var =
        __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState& pos_var =
        __ cache_state()->stack_state.end()[-2];

    LiftoffRegister view_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-3], pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringViewWtf8Advance,
                    MakeSig::Returns(kI32).Params(kRef, kI32, kI32),
                    {
                        view_var,
                        pos_var,
                        bytes_var,
                    },
                    decoder->position());
    __ DropValues(3);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringViewWtf8Encode(FullDecoder* decoder,
                            const MemoryIndexImmediate<validate>& imm,
                            const unibrow::Utf8Variant variant,
                            const Value& view, const Value& addr,
                            const Value& pos, const Value& bytes,
                            Value* next_pos, Value* bytes_written) {
    LiftoffRegList pinned;

    LiftoffAssembler::VarState& bytes_var =
        __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState& pos_var =
        __ cache_state()->stack_state.end()[-2];
    LiftoffAssembler::VarState& addr_var =
        __ cache_state()->stack_state.end()[-3];

    LiftoffRegister view_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-4], pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);

    LiftoffRegister memory_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(memory_reg, imm.index);
    LiftoffAssembler::VarState memory_var(kSmiKind, memory_reg, 0);

    LiftoffRegister variant_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(variant_reg, static_cast<int32_t>(variant));
    LiftoffAssembler::VarState variant_var(kSmiKind, variant_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringViewWtf8Encode,
                    MakeSig::Returns(kI32, kI32)
                        .Params(kI32, kI32, kI32, kRef, kSmiKind, kSmiKind),
                    {
                        addr_var,
                        pos_var,
                        bytes_var,
                        view_var,
                        memory_var,
                        variant_var,
                    },
                    decoder->position());
    __ DropValues(4);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister next_pos_reg(kReturnRegister0);
    __ PushRegister(kI32, next_pos_reg);
    LiftoffRegister bytes_written_reg(kReturnRegister1);
    __ PushRegister(kI32, bytes_written_reg);
  }

  void StringViewWtf8Slice(FullDecoder* decoder, const Value& view,
                           const Value& start, const Value& end,
                           Value* result) {
    LiftoffRegList pinned;

    LiftoffAssembler::VarState& end_var =
        __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState& start_var =
        __ cache_state()->stack_state.end()[-2];

    LiftoffRegister view_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-3], pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringViewWtf8Slice,
                    MakeSig::Returns(kRef).Params(kRef, kI32, kI32),
                    {
                        view_var,
                        start_var,
                        end_var,
                    },
                    decoder->position());
    __ DropValues(3);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringAsWtf16(FullDecoder* decoder, const Value& str, Value* result) {
    RefAsNonNull(decoder, str, result);
  }

  void StringViewWtf16GetCodeUnit(FullDecoder* decoder, const Value& view,
                                  const Value& pos, Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister pos_reg = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister view_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);
    LiftoffAssembler::VarState pos_var(kI32, pos_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringViewWtf16GetCodeUnit,
                    MakeSig::Returns(kI32).Params(kRef, kI32),
                    {
                        view_var,
                        pos_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringViewWtf16Encode(FullDecoder* decoder,
                             const MemoryIndexImmediate<validate>& imm,
                             const Value& view, const Value& offset,
                             const Value& pos, const Value& codeunits,
                             Value* result) {
    LiftoffRegList pinned;

    LiftoffAssembler::VarState& codeunits_var =
        __ cache_state()->stack_state.end()[-1];
    LiftoffAssembler::VarState& pos_var =
        __ cache_state()->stack_state.end()[-2];
    LiftoffAssembler::VarState& offset_var =
        __ cache_state()->stack_state.end()[-3];

    LiftoffRegister view_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-4], pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);

    LiftoffRegister memory_reg =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LoadSmi(memory_reg, imm.index);
    LiftoffAssembler::VarState memory_var(kSmiKind, memory_reg, 0);

    CallRuntimeStub(
        WasmCode::kWasmStringViewWtf16Encode,
        MakeSig::Returns(kI32).Params(kI32, kI32, kI32, kRef, kSmiKind),
        {
            offset_var,
            pos_var,
            codeunits_var,
            view_var,
            memory_var,
        },
        decoder->position());
    __ DropValues(4);
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringViewWtf16Slice(FullDecoder* decoder, const Value& view,
                            const Value& start, const Value& end,
                            Value* result) {
    LiftoffRegList pinned;
    LiftoffRegister end_reg = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister start_reg = pinned.set(__ PopToRegister(pinned));
    LiftoffRegister view_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);
    LiftoffAssembler::VarState start_var(kI32, start_reg, 0);
    LiftoffAssembler::VarState end_var(kI32, end_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringViewWtf16Slice,
                    MakeSig::Returns(kRef).Params(kRef, kI32, kI32),
                    {
                        view_var,
                        start_var,
                        end_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringAsIter(FullDecoder* decoder, const Value& str, Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister str_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, str_reg.gp(), pinned, str.type);
    LiftoffAssembler::VarState str_var(kRef, str_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringAsIter,
                    MakeSig::Returns(kRef).Params(kRef),
                    {
                        str_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void StringViewIterNext(FullDecoder* decoder, const Value& view,
                          Value* result) {
    LiftoffRegList pinned;

    LiftoffRegister view_reg = pinned.set(__ PopToRegister(pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringViewIterNext,
                    MakeSig::Returns(kI32).Params(kRef),
                    {
                        view_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringViewIterAdvance(FullDecoder* decoder, const Value& view,
                             const Value& codepoints, Value* result) {
    LiftoffRegList pinned;

    LiftoffAssembler::VarState& codepoints_var =
        __ cache_state()->stack_state.end()[-1];

    LiftoffRegister view_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-2], pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringViewIterAdvance,
                    MakeSig::Returns(kI32).Params(kRef, kI32),
                    {
                        view_var,
                        codepoints_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringViewIterRewind(FullDecoder* decoder, const Value& view,
                            const Value& codepoints, Value* result) {
    LiftoffRegList pinned;

    LiftoffAssembler::VarState& codepoints_var =
        __ cache_state()->stack_state.end()[-1];

    LiftoffRegister view_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-2], pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringViewIterRewind,
                    MakeSig::Returns(kI32).Params(kRef, kI32),
                    {
                        view_var,
                        codepoints_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kI32, result_reg);
  }

  void StringViewIterSlice(FullDecoder* decoder, const Value& view,
                           const Value& codepoints, Value* result) {
    LiftoffRegList pinned;

    LiftoffAssembler::VarState& codepoints_var =
        __ cache_state()->stack_state.end()[-1];

    LiftoffRegister view_reg = pinned.set(
        __ LoadToRegister(__ cache_state()->stack_state.end()[-2], pinned));
    MaybeEmitNullCheck(decoder, view_reg.gp(), pinned, view.type);
    LiftoffAssembler::VarState view_var(kRef, view_reg, 0);

    CallRuntimeStub(WasmCode::kWasmStringViewIterSlice,
                    MakeSig::Returns(kRef).Params(kRef, kI32),
                    {
                        view_var,
                        codepoints_var,
                    },
                    decoder->position());
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);

    LiftoffRegister result_reg(kReturnRegister0);
    __ PushRegister(kRef, result_reg);
  }

  void Forward(FullDecoder* decoder, const Value& from, Value* to) {
    // Nothing to do here.
  }

 private:
  void CallDirect(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[], Value returns[], TailCall tail_call) {
    MostlySmallValueKindSig sig(compilation_zone_, imm.sig);
    for (ValueKind ret : sig.returns()) {
      if (!CheckSupportedType(decoder, ret, "return")) return;
    }

    auto call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, imm.sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    // One slot would be enough for call_direct, but would make index
    // computations much more complicated.
    size_t vector_slot = encountered_call_instructions_.size() * 2;
    if (v8_flags.wasm_speculative_inlining) {
      encountered_call_instructions_.push_back(imm.index);
    }

    if (imm.index < env_->module->num_imported_functions) {
      // A direct call to an imported function.
      LiftoffRegList pinned;
      Register tmp = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
      Register target = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();

      Register imported_targets = tmp;
      LOAD_TAGGED_PTR_INSTANCE_FIELD(imported_targets, ImportedFunctionTargets,
                                     pinned);
      __ Load(
          LiftoffRegister(target), imported_targets, no_reg,
          wasm::ObjectAccess::ElementOffsetInTaggedFixedAddressArray(imm.index),
          kPointerLoadType);

      Register imported_function_refs = tmp;
      LOAD_TAGGED_PTR_INSTANCE_FIELD(imported_function_refs,
                                     ImportedFunctionRefs, pinned);
      Register imported_function_ref = tmp;
      __ LoadTaggedPointer(
          imported_function_ref, imported_function_refs, no_reg,
          ObjectAccess::ElementOffsetInTaggedFixedArray(imm.index));

      Register* explicit_instance = &imported_function_ref;
      __ PrepareCall(&sig, call_descriptor, &target, explicit_instance);
      if (tail_call) {
        __ PrepareTailCall(
            static_cast<int>(call_descriptor->ParameterSlotCount()),
            static_cast<int>(
                call_descriptor->GetStackParameterDelta(descriptor_)));
        __ TailCallIndirect(target);
      } else {
        source_position_table_builder_.AddPosition(
            __ pc_offset(), SourcePosition(decoder->position()), true);
        __ CallIndirect(&sig, call_descriptor, target);
        FinishCall(decoder, &sig, call_descriptor);
      }
    } else {
      // Inlining direct calls isn't speculative, but existence of the
      // feedback vector currently depends on this flag.
      if (v8_flags.wasm_speculative_inlining) {
        LiftoffRegister vector = __ GetUnusedRegister(kGpReg, {});
        __ Fill(vector, liftoff::kFeedbackVectorOffset, kPointerKind);
        __ IncrementSmi(vector,
                        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(
                            static_cast<int>(vector_slot)));
        // Warning: {vector} may be clobbered by {IncrementSmi}!
      }
      // A direct call within this module just gets the current instance.
      __ PrepareCall(&sig, call_descriptor);
      // Just encode the function index. This will be patched at instantiation.
      Address addr = static_cast<Address>(imm.index);
      if (tail_call) {
        DCHECK(descriptor_->CanTailCall(call_descriptor));
        __ PrepareTailCall(
            static_cast<int>(call_descriptor->ParameterSlotCount()),
            static_cast<int>(
                call_descriptor->GetStackParameterDelta(descriptor_)));
        __ TailCallNativeWasmCode(addr);
      } else {
        source_position_table_builder_.AddPosition(
            __ pc_offset(), SourcePosition(decoder->position()), true);
        __ CallNativeWasmCode(addr);
        FinishCall(decoder, &sig, call_descriptor);
      }
    }
  }

  void CallIndirect(FullDecoder* decoder, const Value& index_val,
                    const CallIndirectImmediate<validate>& imm,
                    TailCall tail_call) {
    MostlySmallValueKindSig sig(compilation_zone_, imm.sig);
    for (ValueKind ret : sig.returns()) {
      if (!CheckSupportedType(decoder, ret, "return")) return;
    }

    Register index = __ PeekToRegister(0, {}).gp();

    LiftoffRegList pinned{index};
    // Get all temporary registers unconditionally up front.
    Register table = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register tmp_const = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register scratch = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
    Register indirect_function_table = no_reg;
    if (imm.table_imm.index != 0) {
      Register indirect_function_tables =
          pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();
      LOAD_TAGGED_PTR_INSTANCE_FIELD(indirect_function_tables,
                                     IndirectFunctionTables, pinned);

      indirect_function_table = indirect_function_tables;
      __ LoadTaggedPointer(
          indirect_function_table, indirect_function_tables, no_reg,
          ObjectAccess::ElementOffsetInTaggedFixedArray(imm.table_imm.index));
    }

    // Bounds check against the table size.
    Label* invalid_func_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapTableOutOfBounds);

    // Compare against table size stored in
    // {instance->indirect_function_table_size}.
    if (imm.table_imm.index == 0) {
      LOAD_INSTANCE_FIELD(tmp_const, IndirectFunctionTableSize, kUInt32Size,
                          pinned);
    } else {
      __ Load(
          LiftoffRegister(tmp_const), indirect_function_table, no_reg,
          wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kSizeOffset),
          LoadType::kI32Load);
    }
    {
      FREEZE_STATE(trapping);
      __ emit_cond_jump(kUnsignedGreaterEqual, invalid_func_label, kI32, index,
                        tmp_const, trapping);
    }

    CODE_COMMENT("Check indirect call signature");
    // Load the signature from {instance->ift_sig_ids[key]}
    if (imm.table_imm.index == 0) {
      LOAD_INSTANCE_FIELD(table, IndirectFunctionTableSigIds,
                          kSystemPointerSize, pinned);
    } else {
      __ Load(LiftoffRegister(table), indirect_function_table, no_reg,
              wasm::ObjectAccess::ToTagged(
                  WasmIndirectFunctionTable::kSigIdsOffset),
              kPointerLoadType);
    }
    static_assert((1 << 2) == kInt32Size);
    __ Load(LiftoffRegister(scratch), table, index, 0, LoadType::kI32Load,
            nullptr, false, false, true);

    // Compare against expected signature.
    if (v8_flags.wasm_type_canonicalization) {
      LOAD_INSTANCE_FIELD(tmp_const, IsorecursiveCanonicalTypes,
                          kSystemPointerSize, pinned);
      __ Load(LiftoffRegister(tmp_const), tmp_const, no_reg,
              imm.sig_imm.index * kInt32Size, LoadType::kI32Load);
    } else {
      uint32_t canonical_sig_num =
          env_->module->per_module_canonical_type_ids[imm.sig_imm.index];
      DCHECK_GE(canonical_sig_num, 0);
      DCHECK_GE(kMaxInt, canonical_sig_num);
      __ LoadConstant(LiftoffRegister(tmp_const), WasmValue(canonical_sig_num));
    }

    Label* sig_mismatch_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapFuncSigMismatch);
    __ DropValues(1);
    {
      FREEZE_STATE(trapping);
      __ emit_cond_jump(kUnequal, sig_mismatch_label, kPointerKind, scratch,
                        tmp_const, trapping);
    }

    CODE_COMMENT("Execute indirect call");
    // At this point {index} has already been multiplied by kTaggedSize.

    // Load the instance from {instance->ift_instances[key]}
    if (imm.table_imm.index == 0) {
      LOAD_TAGGED_PTR_INSTANCE_FIELD(table, IndirectFunctionTableRefs, pinned);
    } else {
      __ LoadTaggedPointer(
          table, indirect_function_table, no_reg,
          wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kRefsOffset));
    }
    __ LoadTaggedPointer(tmp_const, table, index,
                         ObjectAccess::ElementOffsetInTaggedFixedArray(0),
                         true);

    Register* explicit_instance = &tmp_const;

    // Load the target from {instance->ift_targets[key]}
    if (imm.table_imm.index == 0) {
      LOAD_INSTANCE_FIELD(table, IndirectFunctionTableTargets,
                          kSystemPointerSize, pinned);
    } else {
      __ Load(LiftoffRegister(table), indirect_function_table, no_reg,
              wasm::ObjectAccess::ToTagged(
                  WasmIndirectFunctionTable::kTargetsOffset),
              kPointerLoadType);
    }
    __ Load(LiftoffRegister(scratch), table, index, 0, kPointerLoadType,
            nullptr, false, false, true);

    auto call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, imm.sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    Register target = scratch;
    __ PrepareCall(&sig, call_descriptor, &target, explicit_instance);
    if (tail_call) {
      __ PrepareTailCall(
          static_cast<int>(call_descriptor->ParameterSlotCount()),
          static_cast<int>(
              call_descriptor->GetStackParameterDelta(descriptor_)));
      __ TailCallIndirect(target);
    } else {
      source_position_table_builder_.AddPosition(
          __ pc_offset(), SourcePosition(decoder->position()), true);
      __ CallIndirect(&sig, call_descriptor, target);

      FinishCall(decoder, &sig, call_descriptor);
    }
  }

  void CallRef(FullDecoder* decoder, ValueType func_ref_type,
               const FunctionSig* type_sig, TailCall tail_call) {
    MostlySmallValueKindSig sig(compilation_zone_, type_sig);
    for (ValueKind ret : sig.returns()) {
      if (!CheckSupportedType(decoder, ret, "return")) return;
    }
    compiler::CallDescriptor* call_descriptor =
        compiler::GetWasmCallDescriptor(compilation_zone_, type_sig);
    call_descriptor =
        GetLoweredCallDescriptor(compilation_zone_, call_descriptor);

    Register target_reg = no_reg, instance_reg = no_reg;

    if (v8_flags.wasm_speculative_inlining) {
      ValueKind kIntPtrKind = kPointerKind;

      LiftoffRegList pinned;
      LiftoffRegister vector = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      LiftoffAssembler::VarState funcref =
          __ cache_state()->stack_state.end()[-1];
      if (funcref.is_reg()) pinned.set(funcref.reg());
      __ Fill(vector, liftoff::kFeedbackVectorOffset, kPointerKind);
      LiftoffAssembler::VarState vector_var(kPointerKind, vector, 0);
      LiftoffRegister index = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      size_t vector_slot = encountered_call_instructions_.size() * 2;
      encountered_call_instructions_.push_back(
          FunctionTypeFeedback::kNonDirectCall);
      __ LoadConstant(index, WasmValue::ForUintPtr(vector_slot));
      LiftoffAssembler::VarState index_var(kIntPtrKind, index, 0);

      // CallRefIC(vector: FixedArray, index: intptr,
      //           funcref: WasmInternalFunction)
      CallRuntimeStub(WasmCode::kCallRefIC,
                      MakeSig::Returns(kPointerKind, kPointerKind)
                          .Params(kPointerKind, kIntPtrKind, kPointerKind),
                      {vector_var, index_var, funcref}, decoder->position());

      __ cache_state()->stack_state.pop_back(1);  // Drop funcref.
      target_reg = LiftoffRegister(kReturnRegister0).gp();
      instance_reg = LiftoffRegister(kReturnRegister1).gp();

    } else {  // v8_flags.wasm_speculative_inlining
      // Non-feedback-collecting version.
      // Executing a write barrier needs temp registers; doing this on a
      // conditional branch confuses the LiftoffAssembler's register management.
      // Spill everything up front to work around that.
      __ SpillAllRegisters();

      // We limit ourselves to four registers:
      // (1) func_data, initially reused for func_ref.
      // (2) instance, initially used as temp.
      // (3) target, initially used as temp.
      // (4) temp.
      LiftoffRegList pinned;
      LiftoffRegister func_ref = pinned.set(__ PopToModifiableRegister(pinned));
      MaybeEmitNullCheck(decoder, func_ref.gp(), pinned, func_ref_type);
      LiftoffRegister instance =
          pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      LiftoffRegister target = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
      LiftoffRegister temp = pinned.set(__ GetUnusedRegister(kGpReg, pinned));

      // Load "ref" (instance or WasmApiFunctionRef) and target.
      __ LoadTaggedPointer(
          instance.gp(), func_ref.gp(), no_reg,
          wasm::ObjectAccess::ToTagged(WasmInternalFunction::kRefOffset));

#ifdef V8_ENABLE_SANDBOX
      LOAD_INSTANCE_FIELD(temp.gp(), IsolateRoot, kSystemPointerSize, pinned);
      __ LoadExternalPointer(target.gp(), func_ref.gp(),
                             WasmInternalFunction::kCallTargetOffset,
                             kWasmInternalFunctionCallTargetTag, temp.gp());
#else
      __ Load(
          target, func_ref.gp(), no_reg,
          wasm::ObjectAccess::ToTagged(WasmInternalFunction::kCallTargetOffset),
          kPointerLoadType);
#endif

      FREEZE_STATE(frozen);
      Label perform_call;

      LiftoffRegister null_address = temp;
      __ LoadConstant(null_address, WasmValue::ForUintPtr(0));
      __ emit_cond_jump(kUnequal, &perform_call, kRef, target.gp(),
                        null_address.gp(), frozen);
      // The cached target can only be null for WasmJSFunctions.
      __ LoadTaggedPointer(
          target.gp(), func_ref.gp(), no_reg,
          wasm::ObjectAccess::ToTagged(WasmInternalFunction::kCodeOffset));
#ifdef V8_EXTERNAL_CODE_SPACE
      __ LoadCodeDataContainerEntry(target.gp(), target.gp());
#else
      __ emit_ptrsize_addi(target.gp(), target.gp(),
                           wasm::ObjectAccess::ToTagged(Code::kHeaderSize));
#endif
      // Fall through to {perform_call}.

      __ bind(&perform_call);
      // Now the call target is in {target}, and the right instance object
      // is in {instance}.
      target_reg = target.gp();
      instance_reg = instance.gp();
    }  // v8_flags.wasm_speculative_inlining

    __ PrepareCall(&sig, call_descriptor, &target_reg, &instance_reg);
    if (tail_call) {
      __ PrepareTailCall(
          static_cast<int>(call_descriptor->ParameterSlotCount()),
          static_cast<int>(
              call_descriptor->GetStackParameterDelta(descriptor_)));
      __ TailCallIndirect(target_reg);
    } else {
      source_position_table_builder_.AddPosition(
          __ pc_offset(), SourcePosition(decoder->position()), true);
      __ CallIndirect(&sig, call_descriptor, target_reg);

      FinishCall(decoder, &sig, call_descriptor);
    }
  }

  void LoadNullValue(Register null, LiftoffRegList pinned) {
    LOAD_INSTANCE_FIELD(null, IsolateRoot, kSystemPointerSize, pinned);
    __ LoadFullPointer(null, null,
                       IsolateData::root_slot_offset(RootIndex::kNullValue));
  }

  void LoadExceptionSymbol(Register dst, LiftoffRegList pinned,
                           RootIndex root_index) {
    LOAD_INSTANCE_FIELD(dst, IsolateRoot, kSystemPointerSize, pinned);
    uint32_t offset_imm = IsolateData::root_slot_offset(root_index);
    __ LoadFullPointer(dst, dst, offset_imm);
  }

  void MaybeEmitNullCheck(FullDecoder* decoder, Register object,
                          LiftoffRegList pinned, ValueType type) {
    if (v8_flags.experimental_wasm_skip_null_checks || !type.is_nullable())
      return;
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapNullDereference);
    LiftoffRegister null = __ GetUnusedRegister(kGpReg, pinned);
    LoadNullValue(null.gp(), pinned);
    FREEZE_STATE(trapping);
    __ emit_cond_jump(LiftoffCondition::kEqual, trap_label, kRefNull, object,
                      null.gp(), trapping);
  }

  void BoundsCheckArray(FullDecoder* decoder, LiftoffRegister array,
                        LiftoffRegister index, LiftoffRegList pinned) {
    if (V8_UNLIKELY(v8_flags.experimental_wasm_skip_bounds_checks)) return;
    Label* trap_label =
        AddOutOfLineTrap(decoder, WasmCode::kThrowWasmTrapArrayOutOfBounds);
    LiftoffRegister length = __ GetUnusedRegister(kGpReg, pinned);
    constexpr int kLengthOffset =
        wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset);
    __ Load(length, array.gp(), no_reg, kLengthOffset, LoadType::kI32Load);
    FREEZE_STATE(trapping);
    __ emit_cond_jump(LiftoffCondition::kUnsignedGreaterEqual, trap_label, kI32,
                      index.gp(), length.gp(), trapping);
  }

  int StructFieldOffset(const StructType* struct_type, int field_index) {
    return wasm::ObjectAccess::ToTagged(WasmStruct::kHeaderSize +
                                        struct_type->field_offset(field_index));
  }

  void LoadObjectField(LiftoffRegister dst, Register src, Register offset_reg,
                       int offset, ValueKind kind, bool is_signed,
                       LiftoffRegList pinned) {
    if (is_reference(kind)) {
      __ LoadTaggedPointer(dst.gp(), src, offset_reg, offset);
    } else {
      // Primitive kind.
      LoadType load_type = LoadType::ForValueKind(kind, is_signed);
      __ Load(dst, src, offset_reg, offset, load_type);
    }
  }

  void StoreObjectField(Register obj, Register offset_reg, int offset,
                        LiftoffRegister value, LiftoffRegList pinned,
                        ValueKind kind) {
    if (is_reference(kind)) {
      __ StoreTaggedPointer(obj, offset_reg, offset, value, pinned);
    } else {
      // Primitive kind.
      StoreType store_type = StoreType::ForValueKind(kind);
      __ Store(obj, offset_reg, offset, value, store_type, pinned);
    }
  }

  void SetDefaultValue(LiftoffRegister reg, ValueKind kind,
                       LiftoffRegList pinned) {
    DCHECK(is_defaultable(kind));
    switch (kind) {
      case kI8:
      case kI16:
      case kI32:
        return __ LoadConstant(reg, WasmValue(int32_t{0}));
      case kI64:
        return __ LoadConstant(reg, WasmValue(int64_t{0}));
      case kF32:
        return __ LoadConstant(reg, WasmValue(float{0.0}));
      case kF64:
        return __ LoadConstant(reg, WasmValue(double{0.0}));
      case kS128:
        DCHECK(CpuFeatures::SupportsWasmSimd128());
        return __ emit_s128_xor(reg, reg, reg);
      case kRefNull:
        return LoadNullValue(reg.gp(), pinned);
      case kRtt:
      case kVoid:
      case kBottom:
      case kRef:
        UNREACHABLE();
    }
  }

  void MaybeOSR() {
    if (V8_UNLIKELY(for_debugging_)) {
      __ MaybeOSR();
    }
  }

  void FinishCall(FullDecoder* decoder, ValueKindSig* sig,
                  compiler::CallDescriptor* call_descriptor) {
    DefineSafepoint();
    RegisterDebugSideTableEntry(decoder, DebugSideTableBuilder::kDidSpill);
    int pc_offset = __ pc_offset();
    MaybeOSR();
    EmitLandingPad(decoder, pc_offset);
    __ FinishCall(sig, call_descriptor);
  }

  void CheckNan(LiftoffRegister src, LiftoffRegList pinned, ValueKind kind) {
    DCHECK(kind == ValueKind::kF32 || kind == ValueKind::kF64);
    auto nondeterminism_addr = __ GetUnusedRegister(kGpReg, pinned);
    __ LoadConstant(
        nondeterminism_addr,
        WasmValue::ForUintPtr(reinterpret_cast<uintptr_t>(nondeterminism_)));
    __ emit_set_if_nan(nondeterminism_addr.gp(), src.fp(), kind);
  }

  void CheckS128Nan(LiftoffRegister dst, LiftoffRegList pinned,
                    ValueKind lane_kind) {
    RegClass rc = reg_class_for(kS128);
    LiftoffRegister tmp_gp = pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    LiftoffRegister tmp_s128 = pinned.set(__ GetUnusedRegister(rc, pinned));
    LiftoffRegister nondeterminism_addr =
        pinned.set(__ GetUnusedRegister(kGpReg, pinned));
    __ LoadConstant(
        nondeterminism_addr,
        WasmValue::ForUintPtr(reinterpret_cast<uintptr_t>(nondeterminism_)));
    __ emit_s128_set_if_nan(nondeterminism_addr.gp(), dst, tmp_gp.gp(),
                            tmp_s128, lane_kind);
  }

  bool has_outstanding_op() const {
    return outstanding_op_ != kNoOutstandingOp;
  }

  bool test_and_reset_outstanding_op(WasmOpcode opcode) {
    DCHECK_NE(kNoOutstandingOp, opcode);
    if (outstanding_op_ != opcode) return false;
    outstanding_op_ = kNoOutstandingOp;
    return true;
  }

  void TraceCacheState(FullDecoder* decoder) const {
    if (!v8_flags.trace_liftoff) return;
    StdoutStream os;
    for (int control_depth = decoder->control_depth() - 1; control_depth >= -1;
         --control_depth) {
      auto* cache_state =
          control_depth == -1 ? __ cache_state()
                              : &decoder->control_at(control_depth)
                                     ->label_state;
      os << PrintCollection(cache_state->stack_state);
      if (control_depth != -1) PrintF("; ");
    }
    os << "\n";
  }

  void DefineSafepoint() {
    auto safepoint = safepoint_table_builder_.DefineSafepoint(&asm_);
    __ cache_state()->DefineSafepoint(safepoint);
  }

  void DefineSafepointWithCalleeSavedRegisters() {
    auto safepoint = safepoint_table_builder_.DefineSafepoint(&asm_);
    __ cache_state()->DefineSafepointWithCalleeSavedRegisters(safepoint);
  }

  Register LoadInstanceIntoRegister(LiftoffRegList pinned, Register fallback) {
    Register instance = __ cache_state()->cached_instance;
    if (instance == no_reg) {
      instance = __ cache_state()->TrySetCachedInstanceRegister(
          pinned | LiftoffRegList{fallback});
      if (instance == no_reg) instance = fallback;
      __ LoadInstanceFromFrame(instance);
    }
    return instance;
  }

  static constexpr WasmOpcode kNoOutstandingOp = kExprUnreachable;
  static constexpr base::EnumSet<ValueKind> kUnconditionallySupported{
      // MVP:
      kI32, kI64, kF32, kF64,
      // Extern ref:
      kRef, kRefNull, kRtt, kI8, kI16};

  LiftoffAssembler asm_;

  // Used for merging code generation of subsequent operations (via look-ahead).
  // Set by the first opcode, reset by the second.
  WasmOpcode outstanding_op_ = kNoOutstandingOp;

  // {supported_types_} is updated in {MaybeBailoutForUnsupportedType}.
  base::EnumSet<ValueKind> supported_types_ = kUnconditionallySupported;
  compiler::CallDescriptor* const descriptor_;
  CompilationEnv* const env_;
  DebugSideTableBuilder* const debug_sidetable_builder_;
  const ForDebugging for_debugging_;
  LiftoffBailoutReason bailout_reason_ = kSuccess;
  const int func_index_;
  ZoneVector<OutOfLineCode> out_of_line_code_;
  SourcePositionTableBuilder source_position_table_builder_;
  ZoneVector<trap_handler::ProtectedInstructionData> protected_instructions_;
  // Zone used to store information during compilation. The result will be
  // stored independently, such that this zone can die together with the
  // LiftoffCompiler after compilation.
  Zone* compilation_zone_;
  SafepointTableBuilder safepoint_table_builder_;
  // The pc offset of the instructions to reserve the stack frame. Needed to
  // patch the actually needed stack size in the end.
  uint32_t pc_offset_stack_frame_construction_ = 0;
  // For emitting breakpoint, we store a pointer to the position of the next
  // breakpoint, and a pointer after the list of breakpoints as end marker.
  // A single breakpoint at offset 0 indicates that we should prepare the
  // function for stepping by flooding it with breakpoints.
  const int* next_breakpoint_ptr_ = nullptr;
  const int* next_breakpoint_end_ = nullptr;

  // Introduce a dead breakpoint to ensure that the calculation of the return
  // address in OSR is correct.
  int dead_breakpoint_ = 0;

  // Remember whether the did function-entry break checks (for "hook on function
  // call" and "break on entry" a.k.a. instrumentation breakpoint). This happens
  // at the first breakable opcode in the function (if compiling for debugging).
  bool did_function_entry_break_checks_ = false;

  struct HandlerInfo {
    MovableLabel handler;
    int pc_offset;
  };

  ZoneVector<HandlerInfo> handlers_;
  int handler_table_offset_ = Assembler::kNoHandlerTable;

  // Current number of exception refs on the stack.
  int num_exceptions_ = 0;

  // Updated during compilation on every "call" or "call_ref" instruction.
  // Holds the call target, or {FunctionTypeFeedback::kNonDirectCall} for
  // "call_ref".
  // After compilation, this is transferred into {WasmModule::type_feedback}.
  std::vector<uint32_t> encountered_call_instructions_;

  int32_t* max_steps_;
  int32_t* nondeterminism_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(LiftoffCompiler);
};

// static
constexpr WasmOpcode LiftoffCompiler::kNoOutstandingOp;
// static
constexpr base::EnumSet<ValueKind> LiftoffCompiler::kUnconditionallySupported;

std::unique_ptr<AssemblerBuffer> NewLiftoffAssemblerBuffer(
    AssemblerBufferCache* assembler_buffer_cache, int func_body_size) {
  size_t code_size_estimate =
      WasmCodeManager::EstimateLiftoffCodeSize(func_body_size);
  // Allocate the initial buffer a bit bigger to avoid reallocation during code
  // generation. Overflows when casting to int are fine, as we will allocate at
  // least {AssemblerBase::kMinimalBufferSize} anyway, so in the worst case we
  // have to grow more often.
  int initial_buffer_size = static_cast<int>(128 + code_size_estimate * 4 / 3);

  return assembler_buffer_cache
             ? assembler_buffer_cache->GetAssemblerBuffer(initial_buffer_size)
             : NewAssemblerBuffer(initial_buffer_size);
}

}  // namespace

WasmCompilationResult ExecuteLiftoffCompilation(
    CompilationEnv* env, const FunctionBody& func_body,
    const LiftoffOptions& compiler_options) {
  DCHECK(compiler_options.is_initialized());
  base::TimeTicks start_time;
  if (V8_UNLIKELY(v8_flags.trace_wasm_compilation_times)) {
    start_time = base::TimeTicks::Now();
  }
  int func_body_size = static_cast<int>(func_body.end - func_body.start);
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileBaseline", "funcIndex", compiler_options.func_index,
               "bodySize", func_body_size);

  Zone zone(GetWasmEngine()->allocator(), "LiftoffCompilationZone");
  auto call_descriptor = compiler::GetWasmCallDescriptor(&zone, func_body.sig);

  std::unique_ptr<DebugSideTableBuilder> debug_sidetable_builder;
  if (compiler_options.debug_sidetable) {
    debug_sidetable_builder = std::make_unique<DebugSideTableBuilder>();
  }
  DCHECK_IMPLIES(compiler_options.max_steps,
                 compiler_options.for_debugging == kForDebugging);
  WasmFeatures unused_detected_features;

  WasmFullDecoder<Decoder::kBooleanValidation, LiftoffCompiler> decoder(
      &zone, env->module, env->enabled_features,
      compiler_options.detected_features ? compiler_options.detected_features
                                         : &unused_detected_features,
      func_body, call_descriptor, env, &zone,
      NewLiftoffAssemblerBuffer(compiler_options.assembler_buffer_cache,
                                func_body_size),
      debug_sidetable_builder.get(), compiler_options);
  decoder.Decode();
  LiftoffCompiler* compiler = &decoder.interface();
  if (decoder.failed()) compiler->OnFirstError(&decoder);

  if (auto* counters = compiler_options.counters) {
    // Check that the histogram for the bailout reasons has the correct size.
    DCHECK_EQ(0, counters->liftoff_bailout_reasons()->min());
    DCHECK_EQ(kNumBailoutReasons - 1,
              counters->liftoff_bailout_reasons()->max());
    DCHECK_EQ(kNumBailoutReasons,
              counters->liftoff_bailout_reasons()->num_buckets());
    // Register the bailout reason (can also be {kSuccess}).
    counters->liftoff_bailout_reasons()->AddSample(
        static_cast<int>(compiler->bailout_reason()));
  }

  if (compiler->did_bailout()) return WasmCompilationResult{};

  WasmCompilationResult result;
  compiler->GetCode(&result.code_desc);
  result.instr_buffer = compiler->ReleaseBuffer();
  result.source_positions = compiler->GetSourcePositionTable();
  result.protected_instructions_data = compiler->GetProtectedInstructionsData();
  result.frame_slot_count = compiler->GetTotalFrameSlotCountForGC();
  auto* lowered_call_desc = GetLoweredCallDescriptor(&zone, call_descriptor);
  result.tagged_parameter_slots = lowered_call_desc->GetTaggedParameterSlots();
  result.func_index = compiler_options.func_index;
  result.result_tier = ExecutionTier::kLiftoff;
  result.for_debugging = compiler_options.for_debugging;
  if (auto* debug_sidetable = compiler_options.debug_sidetable) {
    *debug_sidetable = debug_sidetable_builder->GenerateDebugSideTable();
  }
  result.feedback_vector_slots = compiler->GetFeedbackVectorSlots();

  if (V8_UNLIKELY(v8_flags.trace_wasm_compilation_times)) {
    base::TimeDelta time = base::TimeTicks::Now() - start_time;
    int codesize = result.code_desc.body_size();
    StdoutStream{} << "Compiled function "
                   << reinterpret_cast<const void*>(env->module) << "#"
                   << compiler_options.func_index << " using Liftoff, took "
                   << time.InMilliseconds() << " ms and "
                   << zone.allocation_size() << " bytes; bodysize "
                   << func_body_size << " codesize " << codesize << std::endl;
  }

  DCHECK(result.succeeded());
  return result;
}

std::unique_ptr<DebugSideTable> GenerateLiftoffDebugSideTable(
    const WasmCode* code) {
  auto* native_module = code->native_module();
  auto* function = &native_module->module()->functions[code->index()];
  ModuleWireBytes wire_bytes{native_module->wire_bytes()};
  base::Vector<const byte> function_bytes =
      wire_bytes.GetFunctionBytes(function);
  CompilationEnv env = native_module->CreateCompilationEnv();
  FunctionBody func_body{function->sig, 0, function_bytes.begin(),
                         function_bytes.end()};

  Zone zone(GetWasmEngine()->allocator(), "LiftoffDebugSideTableZone");
  auto call_descriptor = compiler::GetWasmCallDescriptor(&zone, function->sig);
  DebugSideTableBuilder debug_sidetable_builder;
  WasmFeatures detected;
  constexpr int kSteppingBreakpoints[] = {0};
  DCHECK(code->for_debugging() == kForDebugging ||
         code->for_debugging() == kForStepping);
  base::Vector<const int> breakpoints =
      code->for_debugging() == kForStepping
          ? base::ArrayVector(kSteppingBreakpoints)
          : base::Vector<const int>{};
  WasmFullDecoder<Decoder::kBooleanValidation, LiftoffCompiler> decoder(
      &zone, native_module->module(), env.enabled_features, &detected,
      func_body, call_descriptor, &env, &zone,
      NewAssemblerBuffer(AssemblerBase::kDefaultBufferSize),
      &debug_sidetable_builder,
      LiftoffOptions{}
          .set_func_index(code->index())
          .set_for_debugging(code->for_debugging())
          .set_breakpoints(breakpoints));
  decoder.Decode();
  DCHECK(decoder.ok());
  DCHECK(!decoder.interface().did_bailout());
  return debug_sidetable_builder.GenerateDebugSideTable();
}

#undef __
#undef TRACE
#undef WASM_INSTANCE_OBJECT_FIELD_OFFSET
#undef WASM_INSTANCE_OBJECT_FIELD_SIZE
#undef LOAD_INSTANCE_FIELD
#undef LOAD_TAGGED_PTR_INSTANCE_FIELD
#undef CODE_COMMENT

}  // namespace wasm
}  // namespace internal
}  // namespace v8
