// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/graph-builder-interface.h"

#include "src/compiler/wasm-compiler-definitions.h"
#include "src/compiler/wasm-compiler.h"
#include "src/flags/flags.h"
#include "src/wasm/branch-hint-map.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

// Expose {compiler::Node} opaquely as {wasm::TFNode}.
using TFNode = compiler::Node;

// An SsaEnv environment carries the current local variable renaming
// as well as the current effect and control dependency in the TF graph.
// It maintains a control state that tracks whether the environment
// is reachable, has reached a control end, or has been merged.
struct SsaEnv : public ZoneObject {
  enum State { kUnreachable, kReached, kMerged };

  State state;
  TFNode* control;
  TFNode* effect;
  compiler::WasmInstanceCacheNodes instance_cache;
  ZoneVector<TFNode*> locals;

  SsaEnv(Zone* zone, State state, TFNode* control, TFNode* effect,
         uint32_t locals_size)
      : state(state),
        control(control),
        effect(effect),
        locals(locals_size, zone) {}

  SsaEnv(const SsaEnv& other) V8_NOEXCEPT = default;
  SsaEnv(SsaEnv&& other) V8_NOEXCEPT : state(other.state),
                                       control(other.control),
                                       effect(other.effect),
                                       instance_cache(other.instance_cache),
                                       locals(std::move(other.locals)) {
    other.Kill();
  }

  void Kill() {
    state = kUnreachable;
    for (TFNode*& local : locals) {
      local = nullptr;
    }
    control = nullptr;
    effect = nullptr;
    instance_cache = {};
  }
  void SetNotMerged() {
    if (state == kMerged) state = kReached;
  }
};

class WasmGraphBuildingInterface {
 public:
  static constexpr Decoder::ValidateFlag validate = Decoder::kFullValidation;
  using FullDecoder = WasmFullDecoder<validate, WasmGraphBuildingInterface>;
  using CheckForNull = compiler::WasmGraphBuilder::CheckForNull;

  struct Value : public ValueBase<validate> {
    TFNode* node = nullptr;

    template <typename... Args>
    explicit Value(Args&&... args) V8_NOEXCEPT
        : ValueBase(std::forward<Args>(args)...) {}
  };
  using ValueVector = base::SmallVector<Value, 8>;
  using NodeVector = base::SmallVector<TFNode*, 8>;

  struct TryInfo : public ZoneObject {
    SsaEnv* catch_env;
    TFNode* exception = nullptr;

    bool might_throw() const { return exception != nullptr; }

    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(TryInfo);

    explicit TryInfo(SsaEnv* c) : catch_env(c) {}
  };

  struct Control : public ControlBase<Value, validate> {
    SsaEnv* merge_env = nullptr;  // merge environment for the construct.
    SsaEnv* false_env = nullptr;  // false environment (only for if).
    TryInfo* try_info = nullptr;  // information about try statements.
    int32_t previous_catch = -1;  // previous Control with a catch.
    BitVector* loop_assignments = nullptr;  // locals assigned in this loop.
    TFNode* loop_node = nullptr;            // loop header of this loop.
    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(Control);

    template <typename... Args>
    explicit Control(Args&&... args) V8_NOEXCEPT
        : ControlBase(std::forward<Args>(args)...) {}
  };

  WasmGraphBuildingInterface(compiler::WasmGraphBuilder* builder,
                             int func_index, InlinedStatus inlined_status)
      : builder_(builder),
        func_index_(func_index),
        inlined_status_(inlined_status) {}

  void StartFunction(FullDecoder* decoder) {
    // Get the branch hints map and type feedback for this function (if
    // available).
    if (decoder->module_) {
      auto branch_hints_it = decoder->module_->branch_hints.find(func_index_);
      if (branch_hints_it != decoder->module_->branch_hints.end()) {
        branch_hints_ = &branch_hints_it->second;
      }
      TypeFeedbackStorage& feedbacks = decoder->module_->type_feedback;
      base::MutexGuard mutex_guard(&feedbacks.mutex);
      auto feedback = feedbacks.feedback_for_function.find(func_index_);
      if (feedback != feedbacks.feedback_for_function.end()) {
        // This creates a copy of the vector, which is cheaper than holding on
        // to the mutex throughout graph building.
        type_feedback_ = feedback->second.feedback_vector;
        // Preallocate space for storing call counts to save Zone memory.
        int total_calls = 0;
        for (size_t i = 0; i < type_feedback_.size(); i++) {
          total_calls += type_feedback_[i].num_cases();
        }
        builder_->ReserveCallCounts(static_cast<size_t>(total_calls));
        // We need to keep the feedback in the module to inline later. However,
        // this means we are stuck with it forever.
        // TODO(jkummerow): Reconsider our options here.
      }
    }
    // The first '+ 1' is needed by TF Start node, the second '+ 1' is for the
    // instance parameter.
    builder_->Start(static_cast<int>(decoder->sig_->parameter_count() + 1 + 1));
    uint32_t num_locals = decoder->num_locals();
    SsaEnv* ssa_env = decoder->zone()->New<SsaEnv>(
        decoder->zone(), SsaEnv::kReached, effect(), control(), num_locals);
    SetEnv(ssa_env);

    // Initialize local variables. Parameters are shifted by 1 because of the
    // the instance parameter.
    uint32_t index = 0;
    for (; index < decoder->sig_->parameter_count(); ++index) {
      ssa_env->locals[index] = builder_->SetType(
          builder_->Param(index + 1), decoder->sig_->GetParam(index));
    }
    while (index < num_locals) {
      ValueType type = decoder->local_type(index);
      TFNode* node;
      if (!type.is_defaultable()) {
        DCHECK(type.is_reference());
        // TODO(jkummerow): Consider using "the hole" instead, to make any
        // illegal uses more obvious.
        node = builder_->SetType(builder_->RefNull(), type);
      } else {
        node = builder_->SetType(builder_->DefaultValue(type), type);
      }
      while (index < num_locals && decoder->local_type(index) == type) {
        // Do a whole run of like-typed locals at a time.
        ssa_env->locals[index++] = node;
      }
    }
    LoadContextIntoSsa(ssa_env, decoder);

    if (v8_flags.trace_wasm && inlined_status_ == kRegularFunction) {
      builder_->TraceFunctionEntry(decoder->position());
    }
  }

  // Reload the instance cache entries into the Ssa Environment.
  void LoadContextIntoSsa(SsaEnv* ssa_env, FullDecoder* decoder) {
    if (ssa_env != nullptr) {
      builder_->InitInstanceCache(&ssa_env->instance_cache);
      TFNode* mem_size = ssa_env->instance_cache.mem_size;
      if (mem_size != nullptr) {
        bool is_memory64 =
            decoder->module_ != nullptr && decoder->module_->is_memory64;
        builder_->SetType(mem_size, is_memory64 ? kWasmI64 : kWasmI32);
      }
    }
  }

  void StartFunctionBody(FullDecoder* decoder, Control* block) {}

  void FinishFunction(FullDecoder*) {
    if (v8_flags.wasm_speculative_inlining) {
      DCHECK_EQ(feedback_instruction_index_, type_feedback_.size());
    }
    if (inlined_status_ == kRegularFunction) {
      builder_->PatchInStackCheckIfNeeded();
    }
  }

  void OnFirstError(FullDecoder*) {}

  void NextInstruction(FullDecoder*, WasmOpcode) {}

  void Block(FullDecoder* decoder, Control* block) {
    // The branch environment is the outer environment.
    block->merge_env = ssa_env_;
    SetEnv(Steal(decoder->zone(), ssa_env_));
  }

  void Loop(FullDecoder* decoder, Control* block) {
    // This is the merge environment at the beginning of the loop.
    SsaEnv* merge_env = Steal(decoder->zone(), ssa_env_);
    block->merge_env = merge_env;
    SetEnv(merge_env);

    ssa_env_->state = SsaEnv::kMerged;

    TFNode* loop_node = builder_->Loop(control());

    if (emit_loop_exits()) {
      uint32_t nesting_depth = 0;
      for (uint32_t depth = 1; depth < decoder->control_depth(); depth++) {
        if (decoder->control_at(depth)->is_loop()) {
          nesting_depth++;
        }
      }
      // If this loop is nested, the parent loop's can_be_innermost field needs
      // to be false. If the last loop in loop_infos_ has less depth, it has to
      // be the parent loop. If it does not, it means another loop has been
      // found within the parent loop, and that loop will have set the parent's
      // can_be_innermost to false, so we do not need to do anything.
      if (nesting_depth > 0 &&
          loop_infos_.back().nesting_depth < nesting_depth) {
        loop_infos_.back().can_be_innermost = false;
      }
      loop_infos_.emplace_back(loop_node, nesting_depth, true);
    }

    builder_->SetControl(loop_node);
    decoder->control_at(0)->loop_node = loop_node;

    TFNode* effect_inputs[] = {effect(), control()};
    builder_->SetEffect(builder_->EffectPhi(1, effect_inputs));
    builder_->TerminateLoop(effect(), control());
    // Doing a preprocessing pass to analyze loop assignments seems to pay off
    // compared to reallocating Nodes when rearranging Phis in Goto.
    BitVector* assigned = WasmDecoder<validate>::AnalyzeLoopAssignment(
        decoder, decoder->pc(), decoder->num_locals(), decoder->zone());
    if (decoder->failed()) return;
    int instance_cache_index = decoder->num_locals();
    // If the module has shared memory, the stack guard might reallocate the
    // shared memory. We have to assume the instance cache will be updated.
    if (decoder->module_->has_shared_memory) {
      assigned->Add(instance_cache_index);
    }
    DCHECK_NOT_NULL(assigned);
    decoder->control_at(0)->loop_assignments = assigned;

    // Only introduce phis for variables assigned in this loop.
    for (int i = decoder->num_locals() - 1; i >= 0; i--) {
      if (!assigned->Contains(i)) continue;
      TFNode* inputs[] = {ssa_env_->locals[i], control()};
      ssa_env_->locals[i] =
          builder_->SetType(builder_->Phi(decoder->local_type(i), 1, inputs),
                            decoder->local_type(i));
    }
    // Introduce phis for instance cache pointers if necessary.
    if (assigned->Contains(instance_cache_index)) {
      builder_->PrepareInstanceCacheForLoop(&ssa_env_->instance_cache,
                                            control());
    }

    // Now we setup a new environment for the inside of the loop.
    SetEnv(Split(decoder->zone(), ssa_env_));
    builder_->StackCheck(decoder->module_->has_shared_memory
                             ? &ssa_env_->instance_cache
                             : nullptr,
                         decoder->position());
    ssa_env_->SetNotMerged();

    // Wrap input merge into phis.
    for (uint32_t i = 0; i < block->start_merge.arity; ++i) {
      Value& val = block->start_merge[i];
      TFNode* inputs[] = {val.node, block->merge_env->control};
      SetAndTypeNode(&val, builder_->Phi(val.type, 1, inputs));
    }
  }

  void Try(FullDecoder* decoder, Control* block) {
    SsaEnv* outer_env = ssa_env_;
    SsaEnv* catch_env = Split(decoder->zone(), outer_env);
    // Mark catch environment as unreachable, since only accessable
    // through catch unwinding (i.e. landing pads).
    catch_env->state = SsaEnv::kUnreachable;
    SsaEnv* try_env = Steal(decoder->zone(), outer_env);
    SetEnv(try_env);
    TryInfo* try_info = decoder->zone()->New<TryInfo>(catch_env);
    block->merge_env = outer_env;
    block->try_info = try_info;
  }

  void If(FullDecoder* decoder, const Value& cond, Control* if_block) {
    TFNode* if_true = nullptr;
    TFNode* if_false = nullptr;
    WasmBranchHint hint = WasmBranchHint::kNoHint;
    if (branch_hints_) {
      hint = branch_hints_->GetHintFor(decoder->pc_relative_offset());
    }
    switch (hint) {
      case WasmBranchHint::kNoHint:
        builder_->BranchNoHint(cond.node, &if_true, &if_false);
        break;
      case WasmBranchHint::kUnlikely:
        builder_->BranchExpectFalse(cond.node, &if_true, &if_false);
        break;
      case WasmBranchHint::kLikely:
        builder_->BranchExpectTrue(cond.node, &if_true, &if_false);
        break;
    }
    SsaEnv* merge_env = ssa_env_;
    SsaEnv* false_env = Split(decoder->zone(), ssa_env_);
    false_env->control = if_false;
    SsaEnv* true_env = Steal(decoder->zone(), ssa_env_);
    true_env->control = if_true;
    if_block->merge_env = merge_env;
    if_block->false_env = false_env;
    SetEnv(true_env);
  }

  void FallThruTo(FullDecoder* decoder, Control* c) {
    DCHECK(!c->is_loop());
    MergeValuesInto(decoder, c, &c->end_merge);
  }

  void PopControl(FullDecoder* decoder, Control* block) {
    // A loop just continues with the end environment. There is no merge.
    // However, if loop unrolling is enabled, we must create a loop exit and
    // wrap the fallthru values on the stack.
    if (block->is_loop()) {
      if (emit_loop_exits() && block->reachable()) {
        BuildLoopExits(decoder, block);
        WrapLocalsAtLoopExit(decoder, block);
        uint32_t arity = block->end_merge.arity;
        if (arity > 0) {
          Value* stack_base = decoder->stack_value(arity);
          for (uint32_t i = 0; i < arity; i++) {
            Value* val = stack_base + i;
            SetAndTypeNode(val,
                           builder_->LoopExitValue(
                               val->node, val->type.machine_representation()));
          }
        }
      }
      return;
    }
    // Any other block falls through to the parent block.
    if (block->reachable()) FallThruTo(decoder, block);
    if (block->is_onearmed_if()) {
      // Merge the else branch into the end merge.
      SetEnv(block->false_env);
      DCHECK_EQ(block->start_merge.arity, block->end_merge.arity);
      Value* values =
          block->start_merge.arity > 0 ? &block->start_merge[0] : nullptr;
      MergeValuesInto(decoder, block, &block->end_merge, values);
    }
    // Now continue with the merged environment.
    SetEnv(block->merge_env);
  }

  void UnOp(FullDecoder* decoder, WasmOpcode opcode, const Value& value,
            Value* result) {
    SetAndTypeNode(result,
                   builder_->Unop(opcode, value.node, decoder->position()));
  }

  void BinOp(FullDecoder* decoder, WasmOpcode opcode, const Value& lhs,
             const Value& rhs, Value* result) {
    TFNode* node =
        builder_->Binop(opcode, lhs.node, rhs.node, decoder->position());
    if (result) SetAndTypeNode(result, node);
  }

  void TraceInstruction(FullDecoder* decoder, uint32_t markid) {
    builder_->TraceInstruction(markid);
  }

  void I32Const(FullDecoder* decoder, Value* result, int32_t value) {
    SetAndTypeNode(result, builder_->Int32Constant(value));
  }

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {
    SetAndTypeNode(result, builder_->Int64Constant(value));
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    SetAndTypeNode(result, builder_->Float32Constant(value));
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    SetAndTypeNode(result, builder_->Float64Constant(value));
  }

  void S128Const(FullDecoder* decoder, const Simd128Immediate<validate>& imm,
                 Value* result) {
    SetAndTypeNode(result, builder_->Simd128Constant(imm.value));
  }

  void RefNull(FullDecoder* decoder, ValueType type, Value* result) {
    SetAndTypeNode(result, builder_->RefNull());
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    SetAndTypeNode(result, builder_->RefFunc(function_index));
  }

  void RefAsNonNull(FullDecoder* decoder, const Value& arg, Value* result) {
    TFNode* cast_node =
        v8_flags.experimental_wasm_skip_null_checks
            ? builder_->TypeGuard(arg.node, result->type)
            : builder_->RefAsNonNull(arg.node, decoder->position());
    SetAndTypeNode(result, cast_node);
  }

  void Drop(FullDecoder* decoder) {}

  void LocalGet(FullDecoder* decoder, Value* result,
                const IndexImmediate<validate>& imm) {
    result->node = ssa_env_->locals[imm.index];
  }

  void LocalSet(FullDecoder* decoder, const Value& value,
                const IndexImmediate<validate>& imm) {
    ssa_env_->locals[imm.index] = value.node;
  }

  void LocalTee(FullDecoder* decoder, const Value& value, Value* result,
                const IndexImmediate<validate>& imm) {
    result->node = value.node;
    ssa_env_->locals[imm.index] = value.node;
  }

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate<validate>& imm) {
    SetAndTypeNode(result, builder_->GlobalGet(imm.index));
  }

  void GlobalSet(FullDecoder* decoder, const Value& value,
                 const GlobalIndexImmediate<validate>& imm) {
    builder_->GlobalSet(imm.index, value.node);
  }

  void TableGet(FullDecoder* decoder, const Value& index, Value* result,
                const IndexImmediate<validate>& imm) {
    SetAndTypeNode(
        result, builder_->TableGet(imm.index, index.node, decoder->position()));
  }

  void TableSet(FullDecoder* decoder, const Value& index, const Value& value,
                const IndexImmediate<validate>& imm) {
    builder_->TableSet(imm.index, index.node, value.node, decoder->position());
  }

  void Trap(FullDecoder* decoder, TrapReason reason) {
    builder_->Trap(reason, decoder->position());
  }

  void AssertNull(FullDecoder* decoder, const Value& obj, Value* result) {
    builder_->TrapIfFalse(wasm::TrapReason::kTrapIllegalCast,
                          builder_->IsNull(obj.node), decoder->position());
    Forward(decoder, obj, result);
  }

  void NopForTestingUnsupportedInLiftoff(FullDecoder* decoder) {}

  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    SetAndTypeNode(result, builder_->Select(cond.node, tval.node, fval.node,
                                            result->type));
  }

  ValueVector CopyStackValues(FullDecoder* decoder, uint32_t count,
                              uint32_t drop_values) {
    Value* stack_base =
        count > 0 ? decoder->stack_value(count + drop_values) : nullptr;
    ValueVector stack_values(count);
    for (uint32_t i = 0; i < count; i++) {
      stack_values[i] = stack_base[i];
    }
    return stack_values;
  }

  void DoReturn(FullDecoder* decoder, uint32_t drop_values) {
    uint32_t ret_count = static_cast<uint32_t>(decoder->sig_->return_count());
    NodeVector values(ret_count);
    SsaEnv* internal_env = ssa_env_;
    if (emit_loop_exits()) {
      SsaEnv* exit_env = Split(decoder->zone(), ssa_env_);
      SetEnv(exit_env);
      auto stack_values = CopyStackValues(decoder, ret_count, drop_values);
      BuildNestedLoopExits(decoder, decoder->control_depth() - 1, false,
                           stack_values);
      GetNodes(values.begin(), base::VectorOf(stack_values));
    } else {
      Value* stack_base = ret_count == 0
                              ? nullptr
                              : decoder->stack_value(ret_count + drop_values);
      GetNodes(values.begin(), stack_base, ret_count);
    }
    if (v8_flags.trace_wasm && inlined_status_ == kRegularFunction) {
      builder_->TraceFunctionExit(base::VectorOf(values), decoder->position());
    }
    builder_->Return(base::VectorOf(values));
    SetEnv(internal_env);
  }

  void BrOrRet(FullDecoder* decoder, uint32_t depth, uint32_t drop_values) {
    if (depth == decoder->control_depth() - 1) {
      DoReturn(decoder, drop_values);
    } else {
      Control* target = decoder->control_at(depth);
      if (emit_loop_exits()) {
        SsaEnv* internal_env = ssa_env_;
        SsaEnv* exit_env = Split(decoder->zone(), ssa_env_);
        SetEnv(exit_env);
        uint32_t value_count = target->br_merge()->arity;
        auto stack_values = CopyStackValues(decoder, value_count, drop_values);
        BuildNestedLoopExits(decoder, depth, true, stack_values);
        MergeValuesInto(decoder, target, target->br_merge(),
                        stack_values.data());
        SetEnv(internal_env);
      } else {
        MergeValuesInto(decoder, target, target->br_merge(), drop_values);
      }
    }
  }

  void BrIf(FullDecoder* decoder, const Value& cond, uint32_t depth) {
    SsaEnv* fenv = ssa_env_;
    SsaEnv* tenv = Split(decoder->zone(), fenv);
    fenv->SetNotMerged();
    WasmBranchHint hint = WasmBranchHint::kNoHint;
    if (branch_hints_) {
      hint = branch_hints_->GetHintFor(decoder->pc_relative_offset());
    }
    switch (hint) {
      case WasmBranchHint::kNoHint:
        builder_->BranchNoHint(cond.node, &tenv->control, &fenv->control);
        break;
      case WasmBranchHint::kUnlikely:
        builder_->BranchExpectFalse(cond.node, &tenv->control, &fenv->control);
        break;
      case WasmBranchHint::kLikely:
        builder_->BranchExpectTrue(cond.node, &tenv->control, &fenv->control);
        break;
    }
    builder_->SetControl(fenv->control);
    SetEnv(tenv);
    BrOrRet(decoder, depth, 1);
    SetEnv(fenv);
  }

  void BrTable(FullDecoder* decoder, const BranchTableImmediate<validate>& imm,
               const Value& key) {
    if (imm.table_count == 0) {
      // Only a default target. Do the equivalent of br.
      uint32_t target = BranchTableIterator<validate>(decoder, imm).next();
      BrOrRet(decoder, target, 1);
      return;
    }

    SsaEnv* branch_env = ssa_env_;
    // Build branches to the various blocks based on the table.
    TFNode* sw = builder_->Switch(imm.table_count + 1, key.node);

    SsaEnv* copy = Steal(decoder->zone(), branch_env);
    SetEnv(copy);
    BranchTableIterator<validate> iterator(decoder, imm);
    while (iterator.has_next()) {
      uint32_t i = iterator.cur_index();
      uint32_t target = iterator.next();
      SetEnv(Split(decoder->zone(), copy));
      builder_->SetControl(i == imm.table_count ? builder_->IfDefault(sw)
                                                : builder_->IfValue(i, sw));
      BrOrRet(decoder, target, 1);
    }
    DCHECK(decoder->ok());
    SetEnv(branch_env);
  }

  void Else(FullDecoder* decoder, Control* if_block) {
    if (if_block->reachable()) {
      // Merge the if branch into the end merge.
      MergeValuesInto(decoder, if_block, &if_block->end_merge);
    }
    SetEnv(if_block->false_env);
  }

  void LoadMem(FullDecoder* decoder, LoadType type,
               const MemoryAccessImmediate<validate>& imm, const Value& index,
               Value* result) {
    SetAndTypeNode(result, builder_->LoadMem(
                               type.value_type(), type.mem_type(), index.node,
                               imm.offset, imm.alignment, decoder->position()));
  }

  void LoadTransform(FullDecoder* decoder, LoadType type,
                     LoadTransformationKind transform,
                     const MemoryAccessImmediate<validate>& imm,
                     const Value& index, Value* result) {
    SetAndTypeNode(result,
                   builder_->LoadTransform(type.value_type(), type.mem_type(),
                                           transform, index.node, imm.offset,
                                           imm.alignment, decoder->position()));
  }

  void LoadLane(FullDecoder* decoder, LoadType type, const Value& value,
                const Value& index, const MemoryAccessImmediate<validate>& imm,
                const uint8_t laneidx, Value* result) {
    SetAndTypeNode(
        result, builder_->LoadLane(
                    type.value_type(), type.mem_type(), value.node, index.node,
                    imm.offset, imm.alignment, laneidx, decoder->position()));
  }

  void StoreMem(FullDecoder* decoder, StoreType type,
                const MemoryAccessImmediate<validate>& imm, const Value& index,
                const Value& value) {
    builder_->StoreMem(type.mem_rep(), index.node, imm.offset, imm.alignment,
                       value.node, decoder->position(), type.value_type());
  }

  void StoreLane(FullDecoder* decoder, StoreType type,
                 const MemoryAccessImmediate<validate>& imm, const Value& index,
                 const Value& value, const uint8_t laneidx) {
    builder_->StoreLane(type.mem_rep(), index.node, imm.offset, imm.alignment,
                        value.node, laneidx, decoder->position(),
                        type.value_type());
  }

  void CurrentMemoryPages(FullDecoder* decoder, Value* result) {
    SetAndTypeNode(result, builder_->CurrentMemoryPages());
  }

  void MemoryGrow(FullDecoder* decoder, const Value& value, Value* result) {
    SetAndTypeNode(result, builder_->MemoryGrow(value.node));
    // Always reload the instance cache after growing memory.
    LoadContextIntoSsa(ssa_env_, decoder);
  }

  void CallDirect(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[], Value returns[]) {
    int maybe_call_count = -1;
    if (v8_flags.wasm_speculative_inlining && type_feedback_.size() > 0) {
      const CallSiteFeedback& feedback = next_call_feedback();
      DCHECK_EQ(feedback.num_cases(), 1);
      maybe_call_count = feedback.call_count(0);
    }
    DoCall(decoder, CallInfo::CallDirect(imm.index, maybe_call_count), imm.sig,
           args, returns);
  }

  void ReturnCall(FullDecoder* decoder,
                  const CallFunctionImmediate<validate>& imm,
                  const Value args[]) {
    int maybe_call_count = -1;
    if (v8_flags.wasm_speculative_inlining && type_feedback_.size() > 0) {
      const CallSiteFeedback& feedback = next_call_feedback();
      DCHECK_EQ(feedback.num_cases(), 1);
      maybe_call_count = feedback.call_count(0);
    }
    DoReturnCall(decoder, CallInfo::CallDirect(imm.index, maybe_call_count),
                 imm.sig, args);
  }

  void CallIndirect(FullDecoder* decoder, const Value& index,
                    const CallIndirectImmediate<validate>& imm,
                    const Value args[], Value returns[]) {
    DoCall(
        decoder,
        CallInfo::CallIndirect(index, imm.table_imm.index, imm.sig_imm.index),
        imm.sig, args, returns);
  }

  void ReturnCallIndirect(FullDecoder* decoder, const Value& index,
                          const CallIndirectImmediate<validate>& imm,
                          const Value args[]) {
    DoReturnCall(
        decoder,
        CallInfo::CallIndirect(index, imm.table_imm.index, imm.sig_imm.index),
        imm.sig, args);
  }

  void CallRef(FullDecoder* decoder, const Value& func_ref,
               const FunctionSig* sig, uint32_t sig_index, const Value args[],
               Value returns[]) {
    const CallSiteFeedback* feedback = nullptr;
    if (v8_flags.wasm_speculative_inlining && type_feedback_.size() > 0) {
      feedback = &next_call_feedback();
    }
    if (feedback == nullptr || feedback->num_cases() == 0) {
      DoCall(decoder, CallInfo::CallRef(func_ref, NullCheckFor(func_ref.type)),
             sig, args, returns);
      return;
    }

    // Check for equality against a function at a specific index, and if
    // successful, just emit a direct call.
    int num_cases = feedback->num_cases();
    std::vector<TFNode*> control_args;
    std::vector<TFNode*> effect_args;
    std::vector<Value*> returns_values;
    control_args.reserve(num_cases + 1);
    effect_args.reserve(num_cases + 2);
    returns_values.reserve(num_cases);
    for (int i = 0; i < num_cases; i++) {
      const uint32_t expected_function_index = feedback->function_index(i);

      if (v8_flags.trace_wasm_speculative_inlining) {
        PrintF("[Function #%d call #%d: graph support for inlining #%d]\n",
               func_index_, feedback_instruction_index_ - 1,
               expected_function_index);
      }

      TFNode* success_control;
      TFNode* failure_control;
      builder_->CompareToInternalFunctionAtIndex(
          func_ref.node, expected_function_index, &success_control,
          &failure_control, i == num_cases - 1);
      TFNode* initial_effect = effect();

      builder_->SetControl(success_control);
      ssa_env_->control = success_control;
      Value* returns_direct =
          decoder->zone()->NewArray<Value>(sig->return_count());
      for (size_t i = 0; i < sig->return_count(); i++) {
        returns_direct[i].type = returns[i].type;
      }
      DoCall(decoder,
             CallInfo::CallDirect(expected_function_index,
                                  feedback->call_count(i)),
             decoder->module_->signature(sig_index), args, returns_direct);
      control_args.push_back(control());
      effect_args.push_back(effect());
      returns_values.push_back(returns_direct);

      builder_->SetEffectControl(initial_effect, failure_control);
      ssa_env_->effect = initial_effect;
      ssa_env_->control = failure_control;
    }
    Value* returns_ref = decoder->zone()->NewArray<Value>(sig->return_count());
    for (size_t i = 0; i < sig->return_count(); i++) {
      returns_ref[i].type = returns[i].type;
    }
    DoCall(decoder, CallInfo::CallRef(func_ref, NullCheckFor(func_ref.type)),
           sig, args, returns_ref);

    control_args.push_back(control());
    TFNode* control = builder_->Merge(num_cases + 1, control_args.data());

    effect_args.push_back(effect());
    effect_args.push_back(control);
    TFNode* effect = builder_->EffectPhi(num_cases + 1, effect_args.data());

    ssa_env_->control = control;
    ssa_env_->effect = effect;
    builder_->SetEffectControl(effect, control);

    // Each of the {DoCall} helpers above has created a reload of the instance
    // cache nodes. Rather than merging all of them into a Phi here, just
    // let them get DCE'ed and perform a single reload after the merge.
    if (decoder->module_->initial_pages != decoder->module_->maximum_pages) {
      // The invoked function could have used grow_memory, so we need to
      // reload mem_size and mem_start.
      LoadContextIntoSsa(ssa_env_, decoder);
    }

    for (uint32_t i = 0; i < sig->return_count(); i++) {
      std::vector<TFNode*> phi_args;
      for (int j = 0; j < num_cases; j++) {
        phi_args.push_back(returns_values[j][i].node);
      }
      phi_args.push_back(returns_ref[i].node);
      phi_args.push_back(control);
      SetAndTypeNode(
          &returns[i],
          builder_->Phi(sig->GetReturn(i), num_cases + 1, phi_args.data()));
    }
  }

  void ReturnCallRef(FullDecoder* decoder, const Value& func_ref,
                     const FunctionSig* sig, uint32_t sig_index,
                     const Value args[]) {
    const CallSiteFeedback* feedback = nullptr;
    if (v8_flags.wasm_speculative_inlining && type_feedback_.size() > 0) {
      feedback = &next_call_feedback();
    }
    if (feedback == nullptr || feedback->num_cases() == 0) {
      DoReturnCall(decoder,
                   CallInfo::CallRef(func_ref, NullCheckFor(func_ref.type)),
                   sig, args);
      return;
    }

    // Check for equality against a function at a specific index, and if
    // successful, just emit a direct call.
    int num_cases = feedback->num_cases();
    for (int i = 0; i < num_cases; i++) {
      const uint32_t expected_function_index = feedback->function_index(i);

      if (v8_flags.trace_wasm_speculative_inlining) {
        PrintF("[Function #%d call #%d: graph support for inlining #%d]\n",
               func_index_, feedback_instruction_index_ - 1,
               expected_function_index);
      }

      TFNode* success_control;
      TFNode* failure_control;
      builder_->CompareToInternalFunctionAtIndex(
          func_ref.node, expected_function_index, &success_control,
          &failure_control, i == num_cases - 1);
      TFNode* initial_effect = effect();

      builder_->SetControl(success_control);
      ssa_env_->control = success_control;
      DoReturnCall(decoder,
                   CallInfo::CallDirect(expected_function_index,
                                        feedback->call_count(i)),
                   sig, args);

      builder_->SetEffectControl(initial_effect, failure_control);
      ssa_env_->effect = initial_effect;
      ssa_env_->control = failure_control;
    }

    DoReturnCall(decoder,
                 CallInfo::CallRef(func_ref, NullCheckFor(func_ref.type)), sig,
                 args);
  }

  void BrOnNull(FullDecoder* decoder, const Value& ref_object, uint32_t depth,
                bool pass_null_along_branch, Value* result_on_fallthrough) {
    SsaEnv* false_env = ssa_env_;
    SsaEnv* true_env = Split(decoder->zone(), false_env);
    false_env->SetNotMerged();
    builder_->BrOnNull(ref_object.node, &true_env->control,
                       &false_env->control);
    builder_->SetControl(false_env->control);
    SetEnv(true_env);
    BrOrRet(decoder, depth, pass_null_along_branch ? 0 : 1);
    SetEnv(false_env);
    SetAndTypeNode(
        result_on_fallthrough,
        builder_->TypeGuard(ref_object.node, result_on_fallthrough->type));
  }

  void BrOnNonNull(FullDecoder* decoder, const Value& ref_object, Value* result,
                   uint32_t depth, bool /* drop_null_on_fallthrough */) {
    result->node =
        builder_->TypeGuard(ref_object.node, ref_object.type.AsNonNull());
    SsaEnv* false_env = ssa_env_;
    SsaEnv* true_env = Split(decoder->zone(), false_env);
    false_env->SetNotMerged();
    builder_->BrOnNull(ref_object.node, &false_env->control,
                       &true_env->control);
    builder_->SetControl(false_env->control);
    SetEnv(true_env);
    BrOrRet(decoder, depth, 0);
    SetEnv(false_env);
  }

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, base::Vector<Value> args,
              Value* result) {
    NodeVector inputs(args.size());
    GetNodes(inputs.begin(), args);
    TFNode* node = builder_->SimdOp(opcode, inputs.begin());
    if (result) SetAndTypeNode(result, node);
  }

  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate<validate>& imm,
                  base::Vector<Value> inputs, Value* result) {
    NodeVector nodes(inputs.size());
    GetNodes(nodes.begin(), inputs);
    SetAndTypeNode(result,
                   builder_->SimdLaneOp(opcode, imm.lane, nodes.begin()));
  }

  void Simd8x16ShuffleOp(FullDecoder* decoder,
                         const Simd128Immediate<validate>& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {
    TFNode* input_nodes[] = {input0.node, input1.node};
    SetAndTypeNode(result, builder_->Simd8x16ShuffleOp(imm.value, input_nodes));
  }

  void Throw(FullDecoder* decoder, const TagIndexImmediate<validate>& imm,
             const base::Vector<Value>& value_args) {
    int count = value_args.length();
    ZoneVector<TFNode*> args(count, decoder->zone());
    for (int i = 0; i < count; ++i) {
      args[i] = value_args[i].node;
    }
    CheckForException(decoder,
                      builder_->Throw(imm.index, imm.tag, base::VectorOf(args),
                                      decoder->position()));
    builder_->TerminateThrow(effect(), control());
  }

  void Rethrow(FullDecoder* decoder, Control* block) {
    DCHECK(block->is_try_catchall() || block->is_try_catch());
    TFNode* exception = block->try_info->exception;
    DCHECK_NOT_NULL(exception);
    CheckForException(decoder, builder_->Rethrow(exception));
    builder_->TerminateThrow(effect(), control());
  }

  void CatchException(FullDecoder* decoder,
                      const TagIndexImmediate<validate>& imm, Control* block,
                      base::Vector<Value> values) {
    DCHECK(block->is_try_catch());
    // The catch block is unreachable if no possible throws in the try block
    // exist. We only build a landing pad if some node in the try block can
    // (possibly) throw. Otherwise the catch environments remain empty.
    if (!block->try_info->might_throw()) {
      block->reachability = kSpecOnlyReachable;
      return;
    }

    TFNode* exception = block->try_info->exception;
    SetEnv(block->try_info->catch_env);

    TFNode* if_catch = nullptr;
    TFNode* if_no_catch = nullptr;

    // Get the exception tag and see if it matches the expected one.
    TFNode* caught_tag = builder_->GetExceptionTag(exception);
    TFNode* exception_tag = builder_->LoadTagFromTable(imm.index);
    TFNode* compare = builder_->ExceptionTagEqual(caught_tag, exception_tag);
    builder_->BranchNoHint(compare, &if_catch, &if_no_catch);

    // If the tags don't match we continue with the next tag by setting the
    // false environment as the new {TryInfo::catch_env} here.
    SsaEnv* if_no_catch_env = Split(decoder->zone(), ssa_env_);
    if_no_catch_env->control = if_no_catch;
    SsaEnv* if_catch_env = Steal(decoder->zone(), ssa_env_);
    if_catch_env->control = if_catch;
    block->try_info->catch_env = if_no_catch_env;

    // If the tags match we extract the values from the exception object and
    // push them onto the operand stack using the passed {values} vector.
    SetEnv(if_catch_env);
    NodeVector caught_values(values.size());
    base::Vector<TFNode*> caught_vector = base::VectorOf(caught_values);
    builder_->GetExceptionValues(exception, imm.tag, caught_vector);
    for (size_t i = 0, e = values.size(); i < e; ++i) {
      SetAndTypeNode(&values[i], caught_values[i]);
    }
  }

  void Delegate(FullDecoder* decoder, uint32_t depth, Control* block) {
    DCHECK_EQ(decoder->control_at(0), block);
    DCHECK(block->is_incomplete_try());

    if (block->try_info->might_throw()) {
      // Merge the current env into the target handler's env.
      SetEnv(block->try_info->catch_env);
      if (depth == decoder->control_depth() - 1) {
        // We just throw to the caller here, so no need to generate IfSuccess
        // and IfFailure nodes.
        builder_->Rethrow(block->try_info->exception);
        builder_->TerminateThrow(effect(), control());
        return;
      }
      DCHECK(decoder->control_at(depth)->is_try());
      TryInfo* target_try = decoder->control_at(depth)->try_info;
      if (emit_loop_exits()) {
        ValueVector stack_values;
        BuildNestedLoopExits(decoder, depth, true, stack_values,
                             &block->try_info->exception);
      }
      Goto(decoder, target_try->catch_env);

      // Create or merge the exception.
      if (target_try->catch_env->state == SsaEnv::kReached) {
        target_try->exception = block->try_info->exception;
      } else {
        DCHECK_EQ(target_try->catch_env->state, SsaEnv::kMerged);
        target_try->exception = builder_->CreateOrMergeIntoPhi(
            MachineRepresentation::kTagged, target_try->catch_env->control,
            target_try->exception, block->try_info->exception);
      }
    }
  }

  void CatchAll(FullDecoder* decoder, Control* block) {
    DCHECK(block->is_try_catchall() || block->is_try_catch());
    DCHECK_EQ(decoder->control_at(0), block);

    // The catch block is unreachable if no possible throws in the try block
    // exist. We only build a landing pad if some node in the try block can
    // (possibly) throw. Otherwise the catch environments remain empty.
    if (!block->try_info->might_throw()) {
      decoder->SetSucceedingCodeDynamicallyUnreachable();
      return;
    }

    SetEnv(block->try_info->catch_env);
  }

  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode,
                base::Vector<Value> args,
                const MemoryAccessImmediate<validate>& imm, Value* result) {
    NodeVector inputs(args.size());
    GetNodes(inputs.begin(), args);
    TFNode* node = builder_->AtomicOp(opcode, inputs.begin(), imm.alignment,
                                      imm.offset, decoder->position());
    if (result) SetAndTypeNode(result, node);
  }

  void AtomicFence(FullDecoder* decoder) { builder_->AtomicFence(); }

  void MemoryInit(FullDecoder* decoder,
                  const MemoryInitImmediate<validate>& imm, const Value& dst,
                  const Value& src, const Value& size) {
    builder_->MemoryInit(imm.data_segment.index, dst.node, src.node, size.node,
                         decoder->position());
  }

  void DataDrop(FullDecoder* decoder, const IndexImmediate<validate>& imm) {
    builder_->DataDrop(imm.index, decoder->position());
  }

  void MemoryCopy(FullDecoder* decoder,
                  const MemoryCopyImmediate<validate>& imm, const Value& dst,
                  const Value& src, const Value& size) {
    builder_->MemoryCopy(dst.node, src.node, size.node, decoder->position());
  }

  void MemoryFill(FullDecoder* decoder,
                  const MemoryIndexImmediate<validate>& imm, const Value& dst,
                  const Value& value, const Value& size) {
    builder_->MemoryFill(dst.node, value.node, size.node, decoder->position());
  }

  void TableInit(FullDecoder* decoder, const TableInitImmediate<validate>& imm,
                 base::Vector<Value> args) {
    builder_->TableInit(imm.table.index, imm.element_segment.index,
                        args[0].node, args[1].node, args[2].node,
                        decoder->position());
  }

  void ElemDrop(FullDecoder* decoder, const IndexImmediate<validate>& imm) {
    builder_->ElemDrop(imm.index, decoder->position());
  }

  void TableCopy(FullDecoder* decoder, const TableCopyImmediate<validate>& imm,
                 base::Vector<Value> args) {
    builder_->TableCopy(imm.table_dst.index, imm.table_src.index, args[0].node,
                        args[1].node, args[2].node, decoder->position());
  }

  void TableGrow(FullDecoder* decoder, const IndexImmediate<validate>& imm,
                 const Value& value, const Value& delta, Value* result) {
    SetAndTypeNode(result,
                   builder_->TableGrow(imm.index, value.node, delta.node));
  }

  void TableSize(FullDecoder* decoder, const IndexImmediate<validate>& imm,
                 Value* result) {
    SetAndTypeNode(result, builder_->TableSize(imm.index));
  }

  void TableFill(FullDecoder* decoder, const IndexImmediate<validate>& imm,
                 const Value& start, const Value& value, const Value& count) {
    builder_->TableFill(imm.index, start.node, value.node, count.node);
  }

  void StructNew(FullDecoder* decoder,
                 const StructIndexImmediate<validate>& imm, const Value& rtt,
                 const Value args[], Value* result) {
    uint32_t field_count = imm.struct_type->field_count();
    NodeVector arg_nodes(field_count);
    for (uint32_t i = 0; i < field_count; i++) {
      arg_nodes[i] = args[i].node;
    }
    SetAndTypeNode(result,
                   builder_->StructNew(imm.index, imm.struct_type, rtt.node,
                                       base::VectorOf(arg_nodes)));
  }
  void StructNewDefault(FullDecoder* decoder,
                        const StructIndexImmediate<validate>& imm,
                        const Value& rtt, Value* result) {
    uint32_t field_count = imm.struct_type->field_count();
    NodeVector arg_nodes(field_count);
    for (uint32_t i = 0; i < field_count; i++) {
      ValueType field_type = imm.struct_type->field(i);
      arg_nodes[i] = builder_->SetType(builder_->DefaultValue(field_type),
                                       field_type.Unpacked());
    }
    SetAndTypeNode(result,
                   builder_->StructNew(imm.index, imm.struct_type, rtt.node,
                                       base::VectorOf(arg_nodes)));
  }

  void StructGet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate<validate>& field, bool is_signed,
                 Value* result) {
    SetAndTypeNode(result, builder_->StructGet(struct_object.node,
                                               field.struct_imm.struct_type,
                                               field.field_imm.index,
                                               NullCheckFor(struct_object.type),
                                               is_signed, decoder->position()));
  }

  void StructSet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate<validate>& field,
                 const Value& field_value) {
    builder_->StructSet(struct_object.node, field.struct_imm.struct_type,
                        field.field_imm.index, field_value.node,
                        NullCheckFor(struct_object.type), decoder->position());
  }

  void ArrayNew(FullDecoder* decoder, const ArrayIndexImmediate<validate>& imm,
                const Value& length, const Value& initial_value,
                const Value& rtt, Value* result) {
    SetAndTypeNode(result, builder_->ArrayNew(imm.index, imm.array_type,
                                              length.node, initial_value.node,
                                              rtt.node, decoder->position()));
    // array.new_with_rtt introduces a loop. Therefore, we have to mark the
    // immediately nesting loop (if any) as non-innermost.
    if (!loop_infos_.empty()) loop_infos_.back().can_be_innermost = false;
  }

  void ArrayNewDefault(FullDecoder* decoder,
                       const ArrayIndexImmediate<validate>& imm,
                       const Value& length, const Value& rtt, Value* result) {
    // This will be set in {builder_}.
    TFNode* initial_value = nullptr;
    SetAndTypeNode(result, builder_->ArrayNew(imm.index, imm.array_type,
                                              length.node, initial_value,
                                              rtt.node, decoder->position()));
  }

  void ArrayGet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate<validate>& imm, const Value& index,
                bool is_signed, Value* result) {
    SetAndTypeNode(
        result, builder_->ArrayGet(array_obj.node, imm.array_type, index.node,
                                   NullCheckFor(array_obj.type), is_signed,
                                   decoder->position()));
  }

  void ArraySet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate<validate>& imm, const Value& index,
                const Value& value) {
    builder_->ArraySet(array_obj.node, imm.array_type, index.node, value.node,
                       NullCheckFor(array_obj.type), decoder->position());
  }

  void ArrayLen(FullDecoder* decoder, const Value& array_obj, Value* result) {
    SetAndTypeNode(
        result, builder_->ArrayLen(array_obj.node, NullCheckFor(array_obj.type),
                                   decoder->position()));
  }

  void ArrayCopy(FullDecoder* decoder, const Value& dst, const Value& dst_index,
                 const Value& src, const Value& src_index,
                 const Value& length) {
    builder_->ArrayCopy(dst.node, dst_index.node, NullCheckFor(dst.type),
                        src.node, src_index.node, NullCheckFor(src.type),
                        length.node, decoder->position());
  }

  void ArrayNewFixed(FullDecoder* decoder,
                     const ArrayIndexImmediate<validate>& imm,
                     const base::Vector<Value>& elements, const Value& rtt,
                     Value* result) {
    NodeVector element_nodes(elements.size());
    for (uint32_t i = 0; i < elements.size(); i++) {
      element_nodes[i] = elements[i].node;
    }
    SetAndTypeNode(result, builder_->ArrayNewFixed(imm.array_type, rtt.node,
                                                   VectorOf(element_nodes)));
  }

  void ArrayNewSegment(FullDecoder* decoder,
                       const ArrayIndexImmediate<validate>& array_imm,
                       const IndexImmediate<validate>& data_segment,
                       const Value& offset, const Value& length,
                       const Value& rtt, Value* result) {
    SetAndTypeNode(result,
                   builder_->ArrayNewSegment(
                       array_imm.array_type, data_segment.index, offset.node,
                       length.node, rtt.node, decoder->position()));
  }

  void I31New(FullDecoder* decoder, const Value& input, Value* result) {
    SetAndTypeNode(result, builder_->I31New(input.node));
  }

  void I31GetS(FullDecoder* decoder, const Value& input, Value* result) {
    SetAndTypeNode(result,
                   builder_->I31GetS(input.node, NullCheckFor(input.type),
                                     decoder->position()));
  }

  void I31GetU(FullDecoder* decoder, const Value& input, Value* result) {
    SetAndTypeNode(result,
                   builder_->I31GetU(input.node, NullCheckFor(input.type),
                                     decoder->position()));
  }

  void RttCanon(FullDecoder* decoder, uint32_t type_index, Value* result) {
    SetAndTypeNode(result, builder_->RttCanon(type_index));
  }

  using WasmTypeCheckConfig = v8::internal::compiler::WasmTypeCheckConfig;

  WasmTypeCheckConfig ComputeWasmTypeCheckConfig(ValueType object_type,
                                                 ValueType rtt_type,
                                                 const WasmModule* module) {
    WasmTypeCheckConfig result;
    result.object_can_be_null = object_type.is_nullable();
    DCHECK(object_type.is_object_reference());  // Checked by validation.
    // In the bottom case, the result is irrelevant.
    result.rtt_depth = rtt_type.is_bottom()
                           ? 0 /* unused */
                           : static_cast<uint8_t>(GetSubtypingDepth(
                                 module, rtt_type.ref_index()));
    return result;
  }

  void RefTest(FullDecoder* decoder, const Value& object, const Value& rtt,
               Value* result) {
    WasmTypeCheckConfig config =
        ComputeWasmTypeCheckConfig(object.type, rtt.type, decoder->module_);
    SetAndTypeNode(result, builder_->RefTest(object.node, rtt.node, config));
  }

  void RefCast(FullDecoder* decoder, const Value& object, const Value& rtt,
               Value* result) {
    WasmTypeCheckConfig config =
        ComputeWasmTypeCheckConfig(object.type, rtt.type, decoder->module_);
    TFNode* cast_node = v8_flags.experimental_wasm_assume_ref_cast_succeeds
                            ? builder_->TypeGuard(object.node, result->type)
                            : builder_->RefCast(object.node, rtt.node, config,
                                                decoder->position());
    SetAndTypeNode(result, cast_node);
  }

  template <void (compiler::WasmGraphBuilder::*branch_function)(
      TFNode*, TFNode*, WasmTypeCheckConfig, TFNode**, TFNode**, TFNode**,
      TFNode**)>
  void BrOnCastAbs(FullDecoder* decoder, const Value& object, const Value& rtt,
                   Value* forwarding_value, uint32_t br_depth,
                   bool branch_on_match) {
    WasmTypeCheckConfig config =
        ComputeWasmTypeCheckConfig(object.type, rtt.type, decoder->module_);
    SsaEnv* branch_env = Split(decoder->zone(), ssa_env_);
    SsaEnv* no_branch_env = Steal(decoder->zone(), ssa_env_);
    no_branch_env->SetNotMerged();
    SsaEnv* match_env = branch_on_match ? branch_env : no_branch_env;
    SsaEnv* no_match_env = branch_on_match ? no_branch_env : branch_env;
    (builder_->*branch_function)(object.node, rtt.node, config,
                                 &match_env->control, &match_env->effect,
                                 &no_match_env->control, &no_match_env->effect);
    builder_->SetControl(no_branch_env->control);
    SetEnv(branch_env);
    SetAndTypeNode(forwarding_value,
                   builder_->TypeGuard(object.node, forwarding_value->type));
    // Currently, br_on_* instructions modify the value stack before calling
    // the interface function, so we don't need to drop any values here.
    BrOrRet(decoder, br_depth, 0);
    SetEnv(no_branch_env);
  }

  void BrOnCast(FullDecoder* decoder, const Value& object, const Value& rtt,
                Value* value_on_branch, uint32_t br_depth) {
    BrOnCastAbs<&compiler::WasmGraphBuilder::BrOnCast>(
        decoder, object, rtt, value_on_branch, br_depth, true);
  }

  void BrOnCastFail(FullDecoder* decoder, const Value& object, const Value& rtt,
                    Value* value_on_fallthrough, uint32_t br_depth) {
    BrOnCastAbs<&compiler::WasmGraphBuilder::BrOnCast>(
        decoder, object, rtt, value_on_fallthrough, br_depth, false);
  }

  void RefIsData(FullDecoder* decoder, const Value& object, Value* result) {
    SetAndTypeNode(result,
                   builder_->RefIsData(object.node, object.type.is_nullable()));
  }

  void RefAsData(FullDecoder* decoder, const Value& object, Value* result) {
    TFNode* cast_object = builder_->RefAsData(
        object.node, object.type.is_nullable(), decoder->position());
    TFNode* rename = builder_->TypeGuard(cast_object, result->type);
    SetAndTypeNode(result, rename);
  }

  void BrOnData(FullDecoder* decoder, const Value& object,
                Value* value_on_branch, uint32_t br_depth) {
    BrOnCastAbs<&compiler::WasmGraphBuilder::BrOnData>(
        decoder, object, Value{nullptr, kWasmBottom}, value_on_branch, br_depth,
        true);
  }

  void BrOnNonData(FullDecoder* decoder, const Value& object,
                   Value* value_on_fallthrough, uint32_t br_depth) {
    BrOnCastAbs<&compiler::WasmGraphBuilder::BrOnData>(
        decoder, object, Value{nullptr, kWasmBottom}, value_on_fallthrough,
        br_depth, false);
  }

  void RefIsArray(FullDecoder* decoder, const Value& object, Value* result) {
    SetAndTypeNode(
        result, builder_->RefIsArray(object.node, object.type.is_nullable()));
  }

  void RefAsArray(FullDecoder* decoder, const Value& object, Value* result) {
    TFNode* cast_object = builder_->RefAsArray(
        object.node, object.type.is_nullable(), decoder->position());
    TFNode* rename = builder_->TypeGuard(cast_object, result->type);
    SetAndTypeNode(result, rename);
  }

  void BrOnArray(FullDecoder* decoder, const Value& object,
                 Value* value_on_branch, uint32_t br_depth) {
    BrOnCastAbs<&compiler::WasmGraphBuilder::BrOnArray>(
        decoder, object, Value{nullptr, kWasmBottom}, value_on_branch, br_depth,
        true);
  }

  void BrOnNonArray(FullDecoder* decoder, const Value& object,
                    Value* value_on_fallthrough, uint32_t br_depth) {
    BrOnCastAbs<&compiler::WasmGraphBuilder::BrOnArray>(
        decoder, object, Value{nullptr, kWasmBottom}, value_on_fallthrough,
        br_depth, false);
  }

  void RefIsI31(FullDecoder* decoder, const Value& object, Value* result) {
    SetAndTypeNode(result, builder_->RefIsI31(object.node));
  }

  void RefAsI31(FullDecoder* decoder, const Value& object, Value* result) {
    TFNode* cast_object = builder_->RefAsI31(object.node, decoder->position());
    TFNode* rename = builder_->TypeGuard(cast_object, result->type);
    SetAndTypeNode(result, rename);
  }

  void BrOnI31(FullDecoder* decoder, const Value& object,
               Value* value_on_branch, uint32_t br_depth) {
    BrOnCastAbs<&compiler::WasmGraphBuilder::BrOnI31>(
        decoder, object, Value{nullptr, kWasmBottom}, value_on_branch, br_depth,
        true);
  }

  void BrOnNonI31(FullDecoder* decoder, const Value& object,
                  Value* value_on_fallthrough, uint32_t br_depth) {
    BrOnCastAbs<&compiler::WasmGraphBuilder::BrOnI31>(
        decoder, object, Value{nullptr, kWasmBottom}, value_on_fallthrough,
        br_depth, false);
  }

  void StringNewWtf8(FullDecoder* decoder,
                     const MemoryIndexImmediate<validate>& memory,
                     const unibrow::Utf8Variant variant, const Value& offset,
                     const Value& size, Value* result) {
    SetAndTypeNode(result, builder_->StringNewWtf8(memory.index, variant,
                                                   offset.node, size.node));
  }

  void StringNewWtf8Array(FullDecoder* decoder,
                          const unibrow::Utf8Variant variant,
                          const Value& array, const Value& start,
                          const Value& end, Value* result) {
    SetAndTypeNode(result, builder_->StringNewWtf8Array(variant, array.node,
                                                        start.node, end.node));
  }

  void StringNewWtf16(FullDecoder* decoder,
                      const MemoryIndexImmediate<validate>& imm,
                      const Value& offset, const Value& size, Value* result) {
    SetAndTypeNode(result,
                   builder_->StringNewWtf16(imm.index, offset.node, size.node));
  }

  void StringNewWtf16Array(FullDecoder* decoder, const Value& array,
                           const Value& start, const Value& end,
                           Value* result) {
    SetAndTypeNode(result, builder_->StringNewWtf16Array(array.node, start.node,
                                                         end.node));
  }

  void StringConst(FullDecoder* decoder,
                   const StringConstImmediate<validate>& imm, Value* result) {
    SetAndTypeNode(result, builder_->StringConst(imm.index));
  }

  void StringMeasureWtf8(FullDecoder* decoder,
                         const unibrow::Utf8Variant variant, const Value& str,
                         Value* result) {
    switch (variant) {
      case unibrow::Utf8Variant::kUtf8:
        result->node = builder_->StringMeasureUtf8(
            str.node, NullCheckFor(str.type), decoder->position());
        break;
      case unibrow::Utf8Variant::kLossyUtf8:
      case unibrow::Utf8Variant::kWtf8:
        result->node = builder_->StringMeasureWtf8(
            str.node, NullCheckFor(str.type), decoder->position());
        break;
    }
  }

  void StringMeasureWtf16(FullDecoder* decoder, const Value& str,
                          Value* result) {
    result->node = builder_->StringMeasureWtf16(
        str.node, NullCheckFor(str.type), decoder->position());
  }

  void StringEncodeWtf8(FullDecoder* decoder,
                        const MemoryIndexImmediate<validate>& memory,
                        const unibrow::Utf8Variant variant, const Value& str,
                        const Value& offset, Value* result) {
    result->node = builder_->StringEncodeWtf8(memory.index, variant, str.node,
                                              NullCheckFor(str.type),
                                              offset.node, decoder->position());
  }

  void StringEncodeWtf8Array(FullDecoder* decoder,
                             const unibrow::Utf8Variant variant,
                             const Value& str, const Value& array,
                             const Value& start, Value* result) {
    result->node = builder_->StringEncodeWtf8Array(
        variant, str.node, NullCheckFor(str.type), array.node,
        NullCheckFor(array.type), start.node, decoder->position());
  }

  void StringEncodeWtf16(FullDecoder* decoder,
                         const MemoryIndexImmediate<validate>& imm,
                         const Value& str, const Value& offset, Value* result) {
    result->node =
        builder_->StringEncodeWtf16(imm.index, str.node, NullCheckFor(str.type),
                                    offset.node, decoder->position());
  }

  void StringEncodeWtf16Array(FullDecoder* decoder, const Value& str,
                              const Value& array, const Value& start,
                              Value* result) {
    result->node = builder_->StringEncodeWtf16Array(
        str.node, NullCheckFor(str.type), array.node, NullCheckFor(array.type),
        start.node, decoder->position());
  }

  void StringConcat(FullDecoder* decoder, const Value& head, const Value& tail,
                    Value* result) {
    SetAndTypeNode(result, builder_->StringConcat(
                               head.node, NullCheckFor(head.type), tail.node,
                               NullCheckFor(tail.type), decoder->position()));
  }

  void StringEq(FullDecoder* decoder, const Value& a, const Value& b,
                Value* result) {
    result->node =
        builder_->StringEqual(a.node, NullCheckFor(a.type), b.node,
                              NullCheckFor(b.type), decoder->position());
  }

  void StringIsUSVSequence(FullDecoder* decoder, const Value& str,
                           Value* result) {
    result->node = builder_->StringIsUSVSequence(
        str.node, NullCheckFor(str.type), decoder->position());
  }

  void StringAsWtf8(FullDecoder* decoder, const Value& str, Value* result) {
    SetAndTypeNode(result,
                   builder_->StringAsWtf8(str.node, NullCheckFor(str.type),
                                          decoder->position()));
  }

  void StringViewWtf8Advance(FullDecoder* decoder, const Value& view,
                             const Value& pos, const Value& bytes,
                             Value* result) {
    result->node = builder_->StringViewWtf8Advance(
        view.node, NullCheckFor(view.type), pos.node, bytes.node,
        decoder->position());
  }

  void StringViewWtf8Encode(FullDecoder* decoder,
                            const MemoryIndexImmediate<validate>& memory,
                            const unibrow::Utf8Variant variant,
                            const Value& view, const Value& addr,
                            const Value& pos, const Value& bytes,
                            Value* next_pos, Value* bytes_written) {
    builder_->StringViewWtf8Encode(memory.index, variant, view.node,
                                   NullCheckFor(view.type), addr.node, pos.node,
                                   bytes.node, &next_pos->node,
                                   &bytes_written->node, decoder->position());
  }

  void StringViewWtf8Slice(FullDecoder* decoder, const Value& view,
                           const Value& start, const Value& end,
                           Value* result) {
    SetAndTypeNode(result, builder_->StringViewWtf8Slice(
                               view.node, NullCheckFor(view.type), start.node,
                               end.node, decoder->position()));
  }

  void StringAsWtf16(FullDecoder* decoder, const Value& str, Value* result) {
    // Since we implement stringview_wtf16 as string, that's the type we'll
    // use for the Node. (The decoder's Value type must be stringview_wtf16
    // because static type validation relies on it.)
    result->node =
        builder_->SetType(builder_->RefAsNonNull(str.node, decoder->position()),
                          ValueType::Ref(HeapType::kString));
  }

  void StringViewWtf16GetCodeUnit(FullDecoder* decoder, const Value& view,
                                  const Value& pos, Value* result) {
    result->node = builder_->StringViewWtf16GetCodeUnit(
        view.node, NullCheckFor(view.type), pos.node, decoder->position());
  }

  void StringViewWtf16Encode(FullDecoder* decoder,
                             const MemoryIndexImmediate<validate>& imm,
                             const Value& view, const Value& offset,
                             const Value& pos, const Value& codeunits,
                             Value* result) {
    result->node = builder_->StringViewWtf16Encode(
        imm.index, view.node, NullCheckFor(view.type), offset.node, pos.node,
        codeunits.node, decoder->position());
  }

  void StringViewWtf16Slice(FullDecoder* decoder, const Value& view,
                            const Value& start, const Value& end,
                            Value* result) {
    SetAndTypeNode(result, builder_->StringViewWtf16Slice(
                               view.node, NullCheckFor(view.type), start.node,
                               end.node, decoder->position()));
  }

  void StringAsIter(FullDecoder* decoder, const Value& str, Value* result) {
    SetAndTypeNode(result,
                   builder_->StringAsIter(str.node, NullCheckFor(str.type),
                                          decoder->position()));
  }

  void StringViewIterNext(FullDecoder* decoder, const Value& view,
                          Value* result) {
    result->node = builder_->StringViewIterNext(
        view.node, NullCheckFor(view.type), decoder->position());
  }

  void StringViewIterAdvance(FullDecoder* decoder, const Value& view,
                             const Value& codepoints, Value* result) {
    result->node =
        builder_->StringViewIterAdvance(view.node, NullCheckFor(view.type),
                                        codepoints.node, decoder->position());
  }

  void StringViewIterRewind(FullDecoder* decoder, const Value& view,
                            const Value& codepoints, Value* result) {
    result->node =
        builder_->StringViewIterRewind(view.node, NullCheckFor(view.type),
                                       codepoints.node, decoder->position());
  }

  void StringViewIterSlice(FullDecoder* decoder, const Value& view,
                           const Value& codepoints, Value* result) {
    SetAndTypeNode(result, builder_->StringViewIterSlice(
                               view.node, NullCheckFor(view.type),
                               codepoints.node, decoder->position()));
  }

  void Forward(FullDecoder* decoder, const Value& from, Value* to) {
    if (from.type == to->type) {
      to->node = from.node;
    } else {
      SetAndTypeNode(to, builder_->TypeGuard(from.node, to->type));
    }
  }

  std::vector<compiler::WasmLoopInfo> loop_infos() { return loop_infos_; }

 private:
  SsaEnv* ssa_env_ = nullptr;
  compiler::WasmGraphBuilder* builder_;
  int func_index_;
  const BranchHintMap* branch_hints_ = nullptr;
  // Tracks loop data for loop unrolling.
  std::vector<compiler::WasmLoopInfo> loop_infos_;
  InlinedStatus inlined_status_;
  // The entries in {type_feedback_} are indexed by the position of feedback-
  // consuming instructions (currently only calls).
  int feedback_instruction_index_ = 0;
  std::vector<CallSiteFeedback> type_feedback_;

  TFNode* effect() { return builder_->effect(); }

  TFNode* control() { return builder_->control(); }

  TryInfo* current_try_info(FullDecoder* decoder) {
    DCHECK_LT(decoder->current_catch(), decoder->control_depth());
    return decoder->control_at(decoder->control_depth_of_current_catch())
        ->try_info;
  }

  // If {emit_loop_exits()} returns true, we need to emit LoopExit,
  // LoopExitEffect, and LoopExit nodes whenever a control resp. effect resp.
  // value escapes a loop. We emit loop exits in the following cases:
  // - When popping the control of a loop.
  // - At some nodes which connect to the graph's end. We do not always need to
  //   emit loop exits for such nodes, since the wasm loop analysis algorithm
  //   can handle a loop body which connects directly to the graph's end.
  //   However, we need to emit them anyway for nodes that may be rewired to
  //   different nodes during inlining. These are Return and TailCall nodes.
  // - After IfFailure nodes.
  // - When exiting a loop through Delegate.
  bool emit_loop_exits() {
    return v8_flags.wasm_loop_unrolling || v8_flags.wasm_loop_peeling;
  }

  void GetNodes(TFNode** nodes, Value* values, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      nodes[i] = values[i].node;
    }
  }

  void GetNodes(TFNode** nodes, base::Vector<Value> values) {
    GetNodes(nodes, values.begin(), values.size());
  }

  void SetEnv(SsaEnv* env) {
    if (v8_flags.trace_wasm_decoder) {
      char state = 'X';
      if (env) {
        switch (env->state) {
          case SsaEnv::kReached:
            state = 'R';
            break;
          case SsaEnv::kUnreachable:
            state = 'U';
            break;
          case SsaEnv::kMerged:
            state = 'M';
            break;
        }
      }
      PrintF("{set_env = %p, state = %c", env, state);
      if (env && env->control) {
        PrintF(", control = ");
        compiler::WasmGraphBuilder::PrintDebugName(env->control);
      }
      PrintF("}\n");
    }
    if (ssa_env_) {
      ssa_env_->control = control();
      ssa_env_->effect = effect();
    }
    ssa_env_ = env;
    builder_->SetEffectControl(env->effect, env->control);
    builder_->set_instance_cache(&env->instance_cache);
  }

  TFNode* CheckForException(FullDecoder* decoder, TFNode* node) {
    DCHECK_NOT_NULL(node);

    // We need to emit IfSuccess/IfException nodes if this node throws and has
    // an exception handler. An exception handler can either be a try-scope
    // around this node, or if this function is being inlined, the IfException
    // output of the inlined Call node.
    const bool inside_try_scope = decoder->current_catch() != -1;
    if (inlined_status_ != kInlinedHandledCall && !inside_try_scope) {
      return node;
    }

    TFNode* if_success = nullptr;
    TFNode* if_exception = nullptr;
    // TODO(manoskouk): Can we assign a wasm type to the exception value?
    if (!builder_->ThrowsException(node, &if_success, &if_exception)) {
      return node;
    }

    SsaEnv* success_env = Steal(decoder->zone(), ssa_env_);
    success_env->control = if_success;

    SsaEnv* exception_env = Split(decoder->zone(), success_env);
    exception_env->control = if_exception;
    exception_env->effect = if_exception;
    SetEnv(exception_env);

    if (emit_loop_exits()) {
      ValueVector values;
      BuildNestedLoopExits(decoder,
                           inside_try_scope
                               ? decoder->control_depth_of_current_catch()
                               : decoder->control_depth() - 1,
                           true, values, &if_exception);
    }
    if (inside_try_scope) {
      TryInfo* try_info = current_try_info(decoder);
      Goto(decoder, try_info->catch_env);
      if (try_info->exception == nullptr) {
        DCHECK_EQ(SsaEnv::kReached, try_info->catch_env->state);
        try_info->exception = if_exception;
      } else {
        DCHECK_EQ(SsaEnv::kMerged, try_info->catch_env->state);
        try_info->exception = builder_->CreateOrMergeIntoPhi(
            MachineRepresentation::kTaggedPointer, try_info->catch_env->control,
            try_info->exception, if_exception);
      }
    } else {
      DCHECK_EQ(inlined_status_, kInlinedHandledCall);
      // Leave the IfException/LoopExit node dangling. We will connect it during
      // inlining to the handler of the inlined call.
      // Note: We have to generate the handler now since we have no way of
      // generating a LoopExit if needed in the inlining code.
    }

    SetEnv(success_env);
    return node;
  }

  void MergeValuesInto(FullDecoder* decoder, Control* c, Merge<Value>* merge,
                       Value* values) {
    DCHECK(merge == &c->start_merge || merge == &c->end_merge);

    SsaEnv* target = c->merge_env;
    // This has to be computed before calling Goto().
    const bool first = target->state == SsaEnv::kUnreachable;

    Goto(decoder, target);

    if (merge->arity == 0) return;

    for (uint32_t i = 0; i < merge->arity; ++i) {
      Value& val = values[i];
      Value& old = (*merge)[i];
      DCHECK_NOT_NULL(val.node);
      DCHECK(val.type == kWasmBottom || val.type.machine_representation() ==
                                            old.type.machine_representation());
      old.node = first ? val.node
                       : builder_->CreateOrMergeIntoPhi(
                             old.type.machine_representation(), target->control,
                             old.node, val.node);
    }
  }

  void MergeValuesInto(FullDecoder* decoder, Control* c, Merge<Value>* merge,
                       uint32_t drop_values = 0) {
#ifdef DEBUG
    uint32_t avail = decoder->stack_size() -
                     decoder->control_at(0)->stack_depth - drop_values;
    DCHECK_GE(avail, merge->arity);
#endif
    Value* stack_values = merge->arity > 0
                              ? decoder->stack_value(merge->arity + drop_values)
                              : nullptr;
    MergeValuesInto(decoder, c, merge, stack_values);
  }

  void Goto(FullDecoder* decoder, SsaEnv* to) {
    DCHECK_NOT_NULL(to);
    switch (to->state) {
      case SsaEnv::kUnreachable: {  // Overwrite destination.
        to->state = SsaEnv::kReached;
        DCHECK_EQ(ssa_env_->locals.size(), decoder->num_locals());
        to->locals = ssa_env_->locals;
        to->control = control();
        to->effect = effect();
        to->instance_cache = ssa_env_->instance_cache;
        break;
      }
      case SsaEnv::kReached: {  // Create a new merge.
        to->state = SsaEnv::kMerged;
        // Merge control.
        TFNode* controls[] = {to->control, control()};
        TFNode* merge = builder_->Merge(2, controls);
        to->control = merge;
        // Merge effects.
        TFNode* old_effect = effect();
        if (old_effect != to->effect) {
          TFNode* inputs[] = {to->effect, old_effect, merge};
          to->effect = builder_->EffectPhi(2, inputs);
        }
        // Merge locals.
        DCHECK_EQ(ssa_env_->locals.size(), decoder->num_locals());
        for (uint32_t i = 0; i < to->locals.size(); i++) {
          TFNode* a = to->locals[i];
          TFNode* b = ssa_env_->locals[i];
          if (a != b) {
            TFNode* inputs[] = {a, b, merge};
            to->locals[i] = builder_->Phi(decoder->local_type(i), 2, inputs);
          }
        }
        // Start a new merge from the instance cache.
        builder_->NewInstanceCacheMerge(&to->instance_cache,
                                        &ssa_env_->instance_cache, merge);
        break;
      }
      case SsaEnv::kMerged: {
        TFNode* merge = to->control;
        // Extend the existing merge control node.
        builder_->AppendToMerge(merge, control());
        // Merge effects.
        to->effect =
            builder_->CreateOrMergeIntoEffectPhi(merge, to->effect, effect());
        // Merge locals.
        for (uint32_t i = 0; i < to->locals.size(); i++) {
          to->locals[i] = builder_->CreateOrMergeIntoPhi(
              decoder->local_type(i).machine_representation(), merge,
              to->locals[i], ssa_env_->locals[i]);
        }
        // Merge the instance caches.
        builder_->MergeInstanceCacheInto(&to->instance_cache,
                                         &ssa_env_->instance_cache, merge);
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  // Create a complete copy of {from}.
  SsaEnv* Split(Zone* zone, SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    if (from == ssa_env_) {
      ssa_env_->control = control();
      ssa_env_->effect = effect();
    }
    SsaEnv* result = zone->New<SsaEnv>(*from);
    result->state = SsaEnv::kReached;
    return result;
  }

  // Create a copy of {from} that steals its state and leaves {from}
  // unreachable.
  SsaEnv* Steal(Zone* zone, SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    if (from == ssa_env_) {
      ssa_env_->control = control();
      ssa_env_->effect = effect();
    }
    SsaEnv* result = zone->New<SsaEnv>(std::move(*from));
    // Restore the length of {from->locals} after applying move-constructor.
    from->locals.resize(result->locals.size());
    result->state = SsaEnv::kReached;
    return result;
  }

  class CallInfo {
   public:
    enum CallMode { kCallDirect, kCallIndirect, kCallRef };

    static CallInfo CallDirect(uint32_t callee_index, int call_count) {
      return {kCallDirect, callee_index, nullptr,
              static_cast<uint32_t>(call_count),
              CheckForNull::kWithoutNullCheck};
    }

    static CallInfo CallIndirect(const Value& index_value, uint32_t table_index,
                                 uint32_t sig_index) {
      return {kCallIndirect, sig_index, &index_value, table_index,
              CheckForNull::kWithoutNullCheck};
    }

    static CallInfo CallRef(const Value& funcref_value,
                            CheckForNull null_check) {
      return {kCallRef, 0, &funcref_value, 0, null_check};
    }

    CallMode call_mode() { return call_mode_; }

    uint32_t sig_index() {
      DCHECK_EQ(call_mode_, kCallIndirect);
      return callee_or_sig_index_;
    }

    uint32_t callee_index() {
      DCHECK_EQ(call_mode_, kCallDirect);
      return callee_or_sig_index_;
    }

    int call_count() {
      DCHECK_EQ(call_mode_, kCallDirect);
      return static_cast<int>(table_index_or_call_count_);
    }

    CheckForNull null_check() {
      DCHECK_EQ(call_mode_, kCallRef);
      return null_check_;
    }

    const Value* index_or_callee_value() {
      DCHECK_NE(call_mode_, kCallDirect);
      return index_or_callee_value_;
    }

    uint32_t table_index() {
      DCHECK_EQ(call_mode_, kCallIndirect);
      return table_index_or_call_count_;
    }

   private:
    CallInfo(CallMode call_mode, uint32_t callee_or_sig_index,
             const Value* index_or_callee_value,
             uint32_t table_index_or_call_count, CheckForNull null_check)
        : call_mode_(call_mode),
          callee_or_sig_index_(callee_or_sig_index),
          index_or_callee_value_(index_or_callee_value),
          table_index_or_call_count_(table_index_or_call_count),
          null_check_(null_check) {}
    CallMode call_mode_;
    uint32_t callee_or_sig_index_;
    const Value* index_or_callee_value_;
    uint32_t table_index_or_call_count_;
    CheckForNull null_check_;
  };

  void DoCall(FullDecoder* decoder, CallInfo call_info, const FunctionSig* sig,
              const Value args[], Value returns[]) {
    size_t param_count = sig->parameter_count();
    size_t return_count = sig->return_count();
    NodeVector arg_nodes(param_count + 1);
    base::SmallVector<TFNode*, 1> return_nodes(return_count);
    arg_nodes[0] = (call_info.call_mode() == CallInfo::kCallDirect)
                       ? nullptr
                       : call_info.index_or_callee_value()->node;

    for (size_t i = 0; i < param_count; ++i) {
      arg_nodes[i + 1] = args[i].node;
    }
    switch (call_info.call_mode()) {
      case CallInfo::kCallIndirect:
        CheckForException(
            decoder, builder_->CallIndirect(
                         call_info.table_index(), call_info.sig_index(),
                         base::VectorOf(arg_nodes),
                         base::VectorOf(return_nodes), decoder->position()));
        break;
      case CallInfo::kCallDirect: {
        TFNode* call = builder_->CallDirect(
            call_info.callee_index(), base::VectorOf(arg_nodes),
            base::VectorOf(return_nodes), decoder->position());
        builder_->StoreCallCount(call, call_info.call_count());
        CheckForException(decoder, call);
        break;
      }
      case CallInfo::kCallRef:
        CheckForException(
            decoder,
            builder_->CallRef(sig, base::VectorOf(arg_nodes),
                              base::VectorOf(return_nodes),
                              call_info.null_check(), decoder->position()));
        break;
    }
    for (size_t i = 0; i < return_count; ++i) {
      SetAndTypeNode(&returns[i], return_nodes[i]);
    }
    if (decoder->module_->initial_pages != decoder->module_->maximum_pages) {
      // The invoked function could have used grow_memory, so we need to
      // reload mem_size and mem_start.
      LoadContextIntoSsa(ssa_env_, decoder);
    }
  }

  void DoReturnCall(FullDecoder* decoder, CallInfo call_info,
                    const FunctionSig* sig, const Value args[]) {
    size_t arg_count = sig->parameter_count();

    ValueVector arg_values(arg_count + 1);
    if (call_info.call_mode() == CallInfo::kCallDirect) {
      arg_values[0].node = nullptr;
    } else {
      arg_values[0] = *call_info.index_or_callee_value();
      // This is not done by copy assignment.
      arg_values[0].node = call_info.index_or_callee_value()->node;
    }
    if (arg_count > 0) {
      std::memcpy(arg_values.data() + 1, args, arg_count * sizeof(Value));
    }

    if (emit_loop_exits()) {
      BuildNestedLoopExits(decoder, decoder->control_depth(), false,
                           arg_values);
    }

    NodeVector arg_nodes(arg_count + 1);
    GetNodes(arg_nodes.data(), base::VectorOf(arg_values));

    switch (call_info.call_mode()) {
      case CallInfo::kCallIndirect:
        builder_->ReturnCallIndirect(
            call_info.table_index(), call_info.sig_index(),
            base::VectorOf(arg_nodes), decoder->position());
        break;
      case CallInfo::kCallDirect: {
        TFNode* call = builder_->ReturnCall(call_info.callee_index(),
                                            base::VectorOf(arg_nodes),
                                            decoder->position());
        builder_->StoreCallCount(call, call_info.call_count());
        break;
      }
      case CallInfo::kCallRef:
        builder_->ReturnCallRef(sig, base::VectorOf(arg_nodes),
                                call_info.null_check(), decoder->position());
        break;
    }
  }

  const CallSiteFeedback& next_call_feedback() {
    DCHECK_LT(feedback_instruction_index_, type_feedback_.size());
    return type_feedback_[feedback_instruction_index_++];
  }

  void BuildLoopExits(FullDecoder* decoder, Control* loop) {
    builder_->LoopExit(loop->loop_node);
    ssa_env_->control = control();
    ssa_env_->effect = effect();
  }

  void WrapLocalsAtLoopExit(FullDecoder* decoder, Control* loop) {
    for (uint32_t index = 0; index < decoder->num_locals(); index++) {
      if (loop->loop_assignments->Contains(static_cast<int>(index))) {
        ssa_env_->locals[index] = builder_->LoopExitValue(
            ssa_env_->locals[index],
            decoder->local_type(index).machine_representation());
      }
    }
    if (loop->loop_assignments->Contains(decoder->num_locals())) {
#define WRAP_CACHE_FIELD(field)                                                \
  if (ssa_env_->instance_cache.field != nullptr) {                             \
    ssa_env_->instance_cache.field = builder_->LoopExitValue(                  \
        ssa_env_->instance_cache.field, MachineType::PointerRepresentation()); \
  }

      WRAP_CACHE_FIELD(mem_start);
      WRAP_CACHE_FIELD(mem_size);
#undef WRAP_CACHE_FIELD
    }
  }

  void BuildNestedLoopExits(FullDecoder* decoder, uint32_t depth_limit,
                            bool wrap_exit_values, ValueVector& stack_values,
                            TFNode** exception_value = nullptr) {
    DCHECK(emit_loop_exits());
    Control* control = nullptr;
    // We are only interested in exits from the innermost loop.
    for (uint32_t i = 0; i < depth_limit; i++) {
      Control* c = decoder->control_at(i);
      if (c->is_loop()) {
        control = c;
        break;
      }
    }
    if (control != nullptr) {
      BuildLoopExits(decoder, control);
      for (Value& value : stack_values) {
        if (value.node != nullptr) {
          value.node = builder_->SetType(
              builder_->LoopExitValue(value.node,
                                      value.type.machine_representation()),
              value.type);
        }
      }
      if (exception_value != nullptr) {
        // TODO(manoskouk): Can we assign a wasm type to the exception value?
        *exception_value = builder_->LoopExitValue(
            *exception_value, MachineRepresentation::kWord32);
      }
      if (wrap_exit_values) {
        WrapLocalsAtLoopExit(decoder, control);
      }
    }
  }

  CheckForNull NullCheckFor(ValueType type) {
    DCHECK(type.is_object_reference());
    return (!v8_flags.experimental_wasm_skip_null_checks && type.is_nullable())
               ? CheckForNull::kWithNullCheck
               : CheckForNull::kWithoutNullCheck;
  }

  void SetAndTypeNode(Value* value, TFNode* node) {
    // This DCHECK will help us catch uninitialized values.
    DCHECK_LT(value->type.kind(), kBottom);
    value->node = builder_->SetType(node, value->type);
  }
};

}  // namespace

DecodeResult BuildTFGraph(AccountingAllocator* allocator,
                          const WasmFeatures& enabled, const WasmModule* module,
                          compiler::WasmGraphBuilder* builder,
                          WasmFeatures* detected, const FunctionBody& body,
                          std::vector<compiler::WasmLoopInfo>* loop_infos,
                          compiler::NodeOriginTable* node_origins,
                          int func_index, InlinedStatus inlined_status) {
  Zone zone(allocator, ZONE_NAME);
  WasmFullDecoder<Decoder::kFullValidation, WasmGraphBuildingInterface> decoder(
      &zone, module, enabled, detected, body, builder, func_index,
      inlined_status);
  if (node_origins) {
    builder->AddBytecodePositionDecorator(node_origins, &decoder);
  }
  decoder.Decode();
  if (node_origins) {
    builder->RemoveBytecodePositionDecorator();
  }
  *loop_infos = decoder.interface().loop_infos();

  return decoder.toResult(nullptr);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
