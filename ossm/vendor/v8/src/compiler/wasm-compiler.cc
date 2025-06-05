// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"

#include <memory>

#include "src/base/optional.h"
#include "src/base/small-vector.h"
#include "src/base/v8-fallthrough.h"
#include "src/base/vector.h"
#include "src/codegen/assembler.h"
#include "src/codegen/compiler.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/backend/code-generator.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/diamond.h"
#include "src/compiler/fast-api-calls.h"
#include "src/compiler/graph-assembler.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/graph.h"
#include "src/compiler/int64-lowering.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/wasm-compiler-definitions.h"
#include "src/compiler/wasm-graph-assembler.h"
#include "src/execution/simulator-base.h"
#include "src/heap/factory.h"
#include "src/logging/counters.h"
#include "src/objects/heap-number.h"
#include "src/objects/instance-type.h"
#include "src/roots/roots.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/graph-builder-interface.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

#define FATAL_UNSUPPORTED_OPCODE(opcode)        \
  FATAL("Unsupported opcode 0x%x:%s", (opcode), \
        wasm::WasmOpcodes::OpcodeName(opcode));

MachineType assert_size(int expected_size, MachineType type) {
  DCHECK_EQ(expected_size, ElementSizeInBytes(type.representation()));
  return type;
}

#define WASM_INSTANCE_OBJECT_SIZE(name)     \
  (WasmInstanceObject::k##name##OffsetEnd - \
   WasmInstanceObject::k##name##Offset + 1)  // NOLINT(whitespace/indent)

#define LOAD_MUTABLE_INSTANCE_FIELD(name, type)                          \
  gasm_->LoadFromObject(                                                 \
      assert_size(WASM_INSTANCE_OBJECT_SIZE(name), type), GetInstance(), \
      wasm::ObjectAccess::ToTagged(WasmInstanceObject::k##name##Offset))

#define LOAD_INSTANCE_FIELD(name, type)                                  \
  gasm_->LoadImmutable(                                                  \
      assert_size(WASM_INSTANCE_OBJECT_SIZE(name), type), GetInstance(), \
      wasm::ObjectAccess::ToTagged(WasmInstanceObject::k##name##Offset))

#define LOAD_INSTANCE_FIELD_NO_ELIMINATION(name, type)                   \
  gasm_->Load(                                                           \
      assert_size(WASM_INSTANCE_OBJECT_SIZE(name), type), GetInstance(), \
      wasm::ObjectAccess::ToTagged(WasmInstanceObject::k##name##Offset))

// Use MachineType::Pointer() over Tagged() to load root pointers because they
// do not get compressed.
#define LOAD_ROOT(root_name, factory_name)                   \
  (parameter_mode_ == kNoSpecialParameterMode                \
       ? graph()->NewNode(mcgraph()->common()->HeapConstant( \
             isolate_->factory()->factory_name()))           \
       : gasm_->LoadImmutable(                               \
             MachineType::Pointer(), BuildLoadIsolateRoot(), \
             IsolateData::root_slot_offset(RootIndex::k##root_name)))

bool ContainsSimd(const wasm::FunctionSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmS128) return true;
  }
  return false;
}

bool ContainsInt64(const wasm::FunctionSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmI64) return true;
  }
  return false;
}

}  // namespace

WasmGraphBuilder::WasmGraphBuilder(
    wasm::CompilationEnv* env, Zone* zone, MachineGraph* mcgraph,
    const wasm::FunctionSig* sig,
    compiler::SourcePositionTable* source_position_table,
    Parameter0Mode parameter_mode, Isolate* isolate)
    : gasm_(std::make_unique<WasmGraphAssembler>(mcgraph, zone)),
      zone_(zone),
      mcgraph_(mcgraph),
      env_(env),
      has_simd_(ContainsSimd(sig)),
      sig_(sig),
      source_position_table_(source_position_table),
      parameter_mode_(parameter_mode),
      isolate_(isolate) {
  DCHECK_EQ(isolate == nullptr, parameter_mode_ != kNoSpecialParameterMode);
  DCHECK_IMPLIES(env && env->bounds_checks == wasm::kTrapHandler,
                 trap_handler::IsTrapHandlerEnabled());
  DCHECK_NOT_NULL(mcgraph_);
}

// Destructor define here where the definition of {WasmGraphAssembler} is
// available.
WasmGraphBuilder::~WasmGraphBuilder() = default;

void WasmGraphBuilder::Start(unsigned params) {
  Node* start = graph()->NewNode(mcgraph()->common()->Start(params));
  graph()->SetStart(start);
  SetEffectControl(start);
  // Initialize parameter nodes.
  parameters_ = zone_->NewArray<Node*>(params);
  for (unsigned i = 0; i < params; i++) {
    parameters_[i] = nullptr;
  }
  // Initialize instance node.
  switch (parameter_mode_) {
    case kInstanceMode:
      instance_node_ = Param(wasm::kWasmInstanceParameterIndex);
      break;
    case kNoSpecialParameterMode:
      instance_node_ = gasm_->LoadExportedFunctionInstance(
          gasm_->LoadFunctionDataFromJSFunction(
              Param(Linkage::kJSCallClosureParamIndex, "%closure")));
      break;
    case kWasmApiFunctionRefMode:
      instance_node_ = gasm_->Load(
          MachineType::TaggedPointer(), Param(0),
          wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kInstanceOffset));
      break;
  }
  graph()->SetEnd(graph()->NewNode(mcgraph()->common()->End(0)));
}

Node* WasmGraphBuilder::Param(int index, const char* debug_name) {
  DCHECK_NOT_NULL(graph()->start());
  // Turbofan allows negative parameter indices.
  static constexpr int kMinParameterIndex = -1;
  DCHECK_GE(index, kMinParameterIndex);
  int array_index = index - kMinParameterIndex;
  if (parameters_[array_index] == nullptr) {
    parameters_[array_index] = graph()->NewNode(
        mcgraph()->common()->Parameter(index, debug_name), graph()->start());
  }
  return parameters_[array_index];
}

Node* WasmGraphBuilder::Loop(Node* entry) {
  return graph()->NewNode(mcgraph()->common()->Loop(1), entry);
}

void WasmGraphBuilder::TerminateLoop(Node* effect, Node* control) {
  Node* terminate =
      graph()->NewNode(mcgraph()->common()->Terminate(), effect, control);
  gasm_->MergeControlToEnd(terminate);
}

Node* WasmGraphBuilder::LoopExit(Node* loop_node) {
  DCHECK(loop_node->opcode() == IrOpcode::kLoop);
  Node* loop_exit =
      graph()->NewNode(mcgraph()->common()->LoopExit(), control(), loop_node);
  Node* loop_exit_effect = graph()->NewNode(
      mcgraph()->common()->LoopExitEffect(), effect(), loop_exit);
  SetEffectControl(loop_exit_effect, loop_exit);
  return loop_exit;
}

Node* WasmGraphBuilder::LoopExitValue(Node* value,
                                      MachineRepresentation representation) {
  DCHECK_EQ(control()->opcode(), IrOpcode::kLoopExit);
  return graph()->NewNode(mcgraph()->common()->LoopExitValue(representation),
                          value, control());
}

void WasmGraphBuilder::TerminateThrow(Node* effect, Node* control) {
  Node* terminate =
      graph()->NewNode(mcgraph()->common()->Throw(), effect, control);
  gasm_->MergeControlToEnd(terminate);
  gasm_->InitializeEffectControl(nullptr, nullptr);
}

bool WasmGraphBuilder::IsPhiWithMerge(Node* phi, Node* merge) {
  return phi && IrOpcode::IsPhiOpcode(phi->opcode()) &&
         NodeProperties::GetControlInput(phi) == merge;
}

bool WasmGraphBuilder::ThrowsException(Node* node, Node** if_success,
                                       Node** if_exception) {
  if (node->op()->HasProperty(compiler::Operator::kNoThrow)) {
    return false;
  }

  *if_success = graph()->NewNode(mcgraph()->common()->IfSuccess(), node);
  *if_exception =
      graph()->NewNode(mcgraph()->common()->IfException(), node, node);

  return true;
}

void WasmGraphBuilder::AppendToMerge(Node* merge, Node* from) {
  DCHECK(IrOpcode::IsMergeOpcode(merge->opcode()));
  merge->AppendInput(mcgraph()->zone(), from);
  int new_size = merge->InputCount();
  NodeProperties::ChangeOp(
      merge, mcgraph()->common()->ResizeMergeOrPhi(merge->op(), new_size));
}

void WasmGraphBuilder::AppendToPhi(Node* phi, Node* from) {
  DCHECK(IrOpcode::IsPhiOpcode(phi->opcode()));
  int new_size = phi->InputCount();
  phi->InsertInput(mcgraph()->zone(), phi->InputCount() - 1, from);
  NodeProperties::ChangeOp(
      phi, mcgraph()->common()->ResizeMergeOrPhi(phi->op(), new_size));
}

template <typename... Nodes>
Node* WasmGraphBuilder::Merge(Node* fst, Nodes*... args) {
  return graph()->NewNode(this->mcgraph()->common()->Merge(1 + sizeof...(args)),
                          fst, args...);
}

Node* WasmGraphBuilder::Merge(unsigned count, Node** controls) {
  return graph()->NewNode(mcgraph()->common()->Merge(count), count, controls);
}

Node* WasmGraphBuilder::Phi(wasm::ValueType type, unsigned count,
                            Node** vals_and_control) {
  DCHECK(IrOpcode::IsMergeOpcode(vals_and_control[count]->opcode()));
  DCHECK_EQ(vals_and_control[count]->op()->ControlInputCount(), count);
  return graph()->NewNode(
      mcgraph()->common()->Phi(type.machine_representation(), count), count + 1,
      vals_and_control);
}

Node* WasmGraphBuilder::EffectPhi(unsigned count, Node** effects_and_control) {
  DCHECK(IrOpcode::IsMergeOpcode(effects_and_control[count]->opcode()));
  return graph()->NewNode(mcgraph()->common()->EffectPhi(count), count + 1,
                          effects_and_control);
}

Node* WasmGraphBuilder::RefNull() {
  return (v8_flags.experimental_wasm_gc && parameter_mode_ == kInstanceMode)
             ? gasm_->Null()
             : LOAD_ROOT(NullValue, null_value);
}

Node* WasmGraphBuilder::RefFunc(uint32_t function_index) {
  return gasm_->CallRuntimeStub(wasm::WasmCode::kWasmRefFunc,
                                Operator::kNoThrow,
                                gasm_->Uint32Constant(function_index));
}

Node* WasmGraphBuilder::RefAsNonNull(Node* arg,
                                     wasm::WasmCodePosition position) {
  return AssertNotNull(arg, position);
}

Node* WasmGraphBuilder::NoContextConstant() {
  return mcgraph()->IntPtrConstant(0);
}

Node* WasmGraphBuilder::GetInstance() { return instance_node_.get(); }

Node* WasmGraphBuilder::BuildLoadIsolateRoot() {
  switch (parameter_mode_) {
    case kInstanceMode:
      // For wasm functions, the IsolateRoot is loaded from the instance node so
      // that the generated code is Isolate independent.
      return LOAD_INSTANCE_FIELD(IsolateRoot, MachineType::Pointer());
    case kWasmApiFunctionRefMode:
      // Note: Even if the sandbox is enabled, the pointer to the isolate root
      // is not encoded, much like the case above.
      // TODO(manoskouk): Decode the pointer here if that changes.
      return gasm_->Load(
          MachineType::Pointer(), Param(0),
          wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kIsolateRootOffset));
    case kNoSpecialParameterMode:
      return mcgraph()->IntPtrConstant(isolate_->isolate_root());
  }
}

Node* WasmGraphBuilder::TraceInstruction(uint32_t mark_id) {
  const Operator* op = mcgraph()->machine()->TraceInstruction(mark_id);
  Node* node = SetEffect(graph()->NewNode(op, effect(), control()));
  return node;
}

Node* WasmGraphBuilder::Int32Constant(int32_t value) {
  return mcgraph()->Int32Constant(value);
}

Node* WasmGraphBuilder::Int64Constant(int64_t value) {
  return mcgraph()->Int64Constant(value);
}

Node* WasmGraphBuilder::UndefinedValue() {
  return LOAD_ROOT(UndefinedValue, undefined_value);
}

void WasmGraphBuilder::StackCheck(
    WasmInstanceCacheNodes* shared_memory_instance_cache,
    wasm::WasmCodePosition position) {
  DCHECK_NOT_NULL(env_);  // Wrappers don't get stack checks.
  if (!v8_flags.wasm_stack_checks || !env_->runtime_exception_support) {
    return;
  }

  Node* limit_address =
      LOAD_INSTANCE_FIELD(StackLimitAddress, MachineType::Pointer());
  Node* limit = gasm_->LoadFromObject(MachineType::Pointer(), limit_address, 0);

  Node* check = SetEffect(graph()->NewNode(
      mcgraph()->machine()->StackPointerGreaterThan(StackCheckKind::kWasm),
      limit, effect()));

  Node* if_true;
  Node* if_false;
  BranchExpectTrue(check, &if_true, &if_false);

  if (stack_check_call_operator_ == nullptr) {
    // Build and cache the stack check call operator and the constant
    // representing the stack check code.

    // A direct call to a wasm runtime stub defined in this module.
    // Just encode the stub index. This will be patched at relocation.
    stack_check_code_node_.set(mcgraph()->RelocatableIntPtrConstant(
        wasm::WasmCode::kWasmStackGuard, RelocInfo::WASM_STUB_CALL));

    constexpr Operator::Properties properties =
        Operator::kNoThrow | Operator::kNoWrite;
    // If we ever want to mark this call as kNoDeopt, we'll have to make it
    // non-eliminatable some other way.
    static_assert((properties & Operator::kEliminatable) !=
                  Operator::kEliminatable);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        mcgraph()->zone(),                    // zone
        NoContextDescriptor{},                // descriptor
        0,                                    // stack parameter count
        CallDescriptor::kNoFlags,             // flags
        properties,                           // properties
        StubCallMode::kCallWasmRuntimeStub);  // stub call mode
    stack_check_call_operator_ = mcgraph()->common()->Call(call_descriptor);
  }

  Node* call =
      graph()->NewNode(stack_check_call_operator_.get(),
                       stack_check_code_node_.get(), effect(), if_false);
  SetSourcePosition(call, position);

  DCHECK_GT(call->op()->EffectOutputCount(), 0);
  DCHECK_EQ(call->op()->ControlOutputCount(), 0);

  SetEffectControl(call, if_false);

  // We only need to refresh the size of a shared memory, as its start can never
  // change.
  // We handle caching of the instance cache nodes manually, and we may reload
  // them in contexts where load elimination would eliminate the reload.
  // Therefore, we use plain Load nodes which are not subject to load
  // elimination.
  Node* new_memory_size = shared_memory_instance_cache == nullptr
                              ? nullptr
                              : LOAD_INSTANCE_FIELD_NO_ELIMINATION(
                                    MemorySize, MachineType::UintPtr());

  Node* merge = Merge(if_true, control());
  Node* ephi_inputs[] = {check, effect(), merge};
  Node* ephi = EffectPhi(2, ephi_inputs);

  if (shared_memory_instance_cache != nullptr) {
    shared_memory_instance_cache->mem_size = CreateOrMergeIntoPhi(
        MachineType::PointerRepresentation(), merge,
        shared_memory_instance_cache->mem_size, new_memory_size);
  }

  SetEffectControl(ephi, merge);
}

void WasmGraphBuilder::PatchInStackCheckIfNeeded() {
  if (!needs_stack_check_) return;

  Node* start = graph()->start();
  // Place a stack check which uses a dummy node as control and effect.
  Node* dummy = graph()->NewNode(mcgraph()->common()->Dead());
  SetEffectControl(dummy);
  // The function-prologue stack check is associated with position 0, which
  // is never a position of any instruction in the function.
  // We pass the null instance cache, as we are at the beginning of the function
  // and do not need to update it.
  StackCheck(nullptr, 0);

  // In testing, no stack checks were emitted. Nothing to rewire then.
  if (effect() == dummy) return;

  // Now patch all control uses of {start} to use {control} and all effect uses
  // to use {effect} instead. We exclude Projection nodes: Projections pointing
  // to start are floating control, and we want it to point directly to start
  // because of restrictions later in the pipeline (specifically, loop
  // unrolling).
  // Then rewire the dummy node to use start instead.
  NodeProperties::ReplaceUses(start, start, effect(), control());
  {
    // We need an intermediate vector because we are not allowed to modify a use
    // while traversing uses().
    std::vector<Node*> projections;
    for (Node* use : control()->uses()) {
      if (use->opcode() == IrOpcode::kProjection) projections.emplace_back(use);
    }
    for (Node* use : projections) {
      use->ReplaceInput(NodeProperties::FirstControlIndex(use), start);
    }
  }
  NodeProperties::ReplaceUses(dummy, nullptr, start, start);
}

Node* WasmGraphBuilder::Binop(wasm::WasmOpcode opcode, Node* left, Node* right,
                              wasm::WasmCodePosition position) {
  const Operator* op;
  MachineOperatorBuilder* m = mcgraph()->machine();
  switch (opcode) {
    case wasm::kExprI32Add:
      op = m->Int32Add();
      break;
    case wasm::kExprI32Sub:
      op = m->Int32Sub();
      break;
    case wasm::kExprI32Mul:
      op = m->Int32Mul();
      break;
    case wasm::kExprI32DivS:
      return BuildI32DivS(left, right, position);
    case wasm::kExprI32DivU:
      return BuildI32DivU(left, right, position);
    case wasm::kExprI32RemS:
      return BuildI32RemS(left, right, position);
    case wasm::kExprI32RemU:
      return BuildI32RemU(left, right, position);
    case wasm::kExprI32And:
      op = m->Word32And();
      break;
    case wasm::kExprI32Ior:
      op = m->Word32Or();
      break;
    case wasm::kExprI32Xor:
      op = m->Word32Xor();
      break;
    case wasm::kExprI32Shl:
      op = m->Word32Shl();
      right = MaskShiftCount32(right);
      break;
    case wasm::kExprI32ShrU:
      op = m->Word32Shr();
      right = MaskShiftCount32(right);
      break;
    case wasm::kExprI32ShrS:
      op = m->Word32Sar();
      right = MaskShiftCount32(right);
      break;
    case wasm::kExprI32Ror:
      op = m->Word32Ror();
      right = MaskShiftCount32(right);
      break;
    case wasm::kExprI32Rol:
      if (m->Word32Rol().IsSupported()) {
        op = m->Word32Rol().op();
        right = MaskShiftCount32(right);
        break;
      }
      return BuildI32Rol(left, right);
    case wasm::kExprI32Eq:
      op = m->Word32Equal();
      break;
    case wasm::kExprI32Ne:
      return Invert(Binop(wasm::kExprI32Eq, left, right));
    case wasm::kExprI32LtS:
      op = m->Int32LessThan();
      break;
    case wasm::kExprI32LeS:
      op = m->Int32LessThanOrEqual();
      break;
    case wasm::kExprI32LtU:
      op = m->Uint32LessThan();
      break;
    case wasm::kExprI32LeU:
      op = m->Uint32LessThanOrEqual();
      break;
    case wasm::kExprI32GtS:
      op = m->Int32LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprI32GeS:
      op = m->Int32LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprI32GtU:
      op = m->Uint32LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprI32GeU:
      op = m->Uint32LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprI64And:
      op = m->Word64And();
      break;
    case wasm::kExprI64Add:
      op = m->Int64Add();
      break;
    case wasm::kExprI64Sub:
      op = m->Int64Sub();
      break;
    case wasm::kExprI64Mul:
      op = m->Int64Mul();
      break;
    case wasm::kExprI64DivS:
      return BuildI64DivS(left, right, position);
    case wasm::kExprI64DivU:
      return BuildI64DivU(left, right, position);
    case wasm::kExprI64RemS:
      return BuildI64RemS(left, right, position);
    case wasm::kExprI64RemU:
      return BuildI64RemU(left, right, position);
    case wasm::kExprI64Ior:
      op = m->Word64Or();
      break;
    case wasm::kExprI64Xor:
      op = m->Word64Xor();
      break;
    case wasm::kExprI64Shl:
      op = m->Word64Shl();
      right = MaskShiftCount64(right);
      break;
    case wasm::kExprI64ShrU:
      op = m->Word64Shr();
      right = MaskShiftCount64(right);
      break;
    case wasm::kExprI64ShrS:
      op = m->Word64Sar();
      right = MaskShiftCount64(right);
      break;
    case wasm::kExprI64Eq:
      op = m->Word64Equal();
      break;
    case wasm::kExprI64Ne:
      return Invert(Binop(wasm::kExprI64Eq, left, right));
    case wasm::kExprI64LtS:
      op = m->Int64LessThan();
      break;
    case wasm::kExprI64LeS:
      op = m->Int64LessThanOrEqual();
      break;
    case wasm::kExprI64LtU:
      op = m->Uint64LessThan();
      break;
    case wasm::kExprI64LeU:
      op = m->Uint64LessThanOrEqual();
      break;
    case wasm::kExprI64GtS:
      op = m->Int64LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprI64GeS:
      op = m->Int64LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprI64GtU:
      op = m->Uint64LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprI64GeU:
      op = m->Uint64LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprI64Ror:
      right = MaskShiftCount64(right);
      return m->Is64() ? graph()->NewNode(m->Word64Ror(), left, right)
                       : graph()->NewNode(m->Word64RorLowerable(), left, right,
                                          control());
    case wasm::kExprI64Rol:
      if (m->Word64Rol().IsSupported()) {
        return m->Is64() ? graph()->NewNode(m->Word64Rol().op(), left,
                                            MaskShiftCount64(right))
                         : graph()->NewNode(m->Word64RolLowerable().op(), left,
                                            MaskShiftCount64(right), control());
      } else if (m->Word32Rol().IsSupported()) {
        return graph()->NewNode(m->Word64RolLowerable().placeholder(), left,
                                right, control());
      }
      return BuildI64Rol(left, right);
    case wasm::kExprF32CopySign:
      return BuildF32CopySign(left, right);
    case wasm::kExprF64CopySign:
      return BuildF64CopySign(left, right);
    case wasm::kExprF32Add:
      op = m->Float32Add();
      break;
    case wasm::kExprF32Sub:
      op = m->Float32Sub();
      break;
    case wasm::kExprF32Mul:
      op = m->Float32Mul();
      break;
    case wasm::kExprF32Div:
      op = m->Float32Div();
      break;
    case wasm::kExprF32Eq:
      op = m->Float32Equal();
      break;
    case wasm::kExprF32Ne:
      return Invert(Binop(wasm::kExprF32Eq, left, right));
    case wasm::kExprF32Lt:
      op = m->Float32LessThan();
      break;
    case wasm::kExprF32Ge:
      op = m->Float32LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprF32Gt:
      op = m->Float32LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprF32Le:
      op = m->Float32LessThanOrEqual();
      break;
    case wasm::kExprF64Add:
      op = m->Float64Add();
      break;
    case wasm::kExprF64Sub:
      op = m->Float64Sub();
      break;
    case wasm::kExprF64Mul:
      op = m->Float64Mul();
      break;
    case wasm::kExprF64Div:
      op = m->Float64Div();
      break;
    case wasm::kExprF64Eq:
      op = m->Float64Equal();
      break;
    case wasm::kExprF64Ne:
      return Invert(Binop(wasm::kExprF64Eq, left, right));
    case wasm::kExprF64Lt:
      op = m->Float64LessThan();
      break;
    case wasm::kExprF64Le:
      op = m->Float64LessThanOrEqual();
      break;
    case wasm::kExprF64Gt:
      op = m->Float64LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprF64Ge:
      op = m->Float64LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprF32Min:
      op = m->Float32Min();
      break;
    case wasm::kExprF64Min:
      op = m->Float64Min();
      break;
    case wasm::kExprF32Max:
      op = m->Float32Max();
      break;
    case wasm::kExprF64Max:
      op = m->Float64Max();
      break;
    case wasm::kExprF64Pow:
      return BuildF64Pow(left, right);
    case wasm::kExprF64Atan2:
      op = m->Float64Atan2();
      break;
    case wasm::kExprF64Mod:
      return BuildF64Mod(left, right);
    case wasm::kExprRefEq:
      return gasm_->TaggedEqual(left, right);
    case wasm::kExprI32AsmjsDivS:
      return BuildI32AsmjsDivS(left, right);
    case wasm::kExprI32AsmjsDivU:
      return BuildI32AsmjsDivU(left, right);
    case wasm::kExprI32AsmjsRemS:
      return BuildI32AsmjsRemS(left, right);
    case wasm::kExprI32AsmjsRemU:
      return BuildI32AsmjsRemU(left, right);
    case wasm::kExprI32AsmjsStoreMem8:
      return BuildAsmjsStoreMem(MachineType::Int8(), left, right);
    case wasm::kExprI32AsmjsStoreMem16:
      return BuildAsmjsStoreMem(MachineType::Int16(), left, right);
    case wasm::kExprI32AsmjsStoreMem:
      return BuildAsmjsStoreMem(MachineType::Int32(), left, right);
    case wasm::kExprF32AsmjsStoreMem:
      return BuildAsmjsStoreMem(MachineType::Float32(), left, right);
    case wasm::kExprF64AsmjsStoreMem:
      return BuildAsmjsStoreMem(MachineType::Float64(), left, right);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
  return graph()->NewNode(op, left, right);
}

Node* WasmGraphBuilder::Unop(wasm::WasmOpcode opcode, Node* input,
                             wasm::WasmCodePosition position) {
  const Operator* op;
  MachineOperatorBuilder* m = mcgraph()->machine();
  switch (opcode) {
    case wasm::kExprI32Eqz:
      return gasm_->Word32Equal(input, Int32Constant(0));
    case wasm::kExprF32Abs:
      op = m->Float32Abs();
      break;
    case wasm::kExprF32Neg: {
      op = m->Float32Neg();
      break;
    }
    case wasm::kExprF32Sqrt:
      op = m->Float32Sqrt();
      break;
    case wasm::kExprF64Abs:
      op = m->Float64Abs();
      break;
    case wasm::kExprF64Neg: {
      op = m->Float64Neg();
      break;
    }
    case wasm::kExprF64Sqrt:
      op = m->Float64Sqrt();
      break;
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI32SConvertSatF64:
    case wasm::kExprI32UConvertSatF64:
    case wasm::kExprI32SConvertSatF32:
    case wasm::kExprI32UConvertSatF32:
      return BuildIntConvertFloat(input, position, opcode);
    case wasm::kExprI32AsmjsSConvertF64:
      return BuildI32AsmjsSConvertF64(input);
    case wasm::kExprI32AsmjsUConvertF64:
      return BuildI32AsmjsUConvertF64(input);
    case wasm::kExprF32ConvertF64:
      op = m->TruncateFloat64ToFloat32();
      break;
    case wasm::kExprF64SConvertI32:
      op = m->ChangeInt32ToFloat64();
      break;
    case wasm::kExprF64UConvertI32:
      op = m->ChangeUint32ToFloat64();
      break;
    case wasm::kExprF32SConvertI32:
      op = m->RoundInt32ToFloat32();
      break;
    case wasm::kExprF32UConvertI32:
      op = m->RoundUint32ToFloat32();
      break;
    case wasm::kExprI32AsmjsSConvertF32:
      return BuildI32AsmjsSConvertF32(input);
    case wasm::kExprI32AsmjsUConvertF32:
      return BuildI32AsmjsUConvertF32(input);
    case wasm::kExprF64ConvertF32:
      op = m->ChangeFloat32ToFloat64();
      break;
    case wasm::kExprF32ReinterpretI32:
      op = m->BitcastInt32ToFloat32();
      break;
    case wasm::kExprI32ReinterpretF32:
      op = m->BitcastFloat32ToInt32();
      break;
    case wasm::kExprI32Clz:
      op = m->Word32Clz();
      break;
    case wasm::kExprI32Ctz: {
      if (m->Word32Ctz().IsSupported()) {
        op = m->Word32Ctz().op();
        break;
      } else if (m->Word32ReverseBits().IsSupported()) {
        Node* reversed = graph()->NewNode(m->Word32ReverseBits().op(), input);
        Node* result = graph()->NewNode(m->Word32Clz(), reversed);
        return result;
      } else {
        return BuildI32Ctz(input);
      }
    }
    case wasm::kExprI32Popcnt: {
      if (m->Word32Popcnt().IsSupported()) {
        op = m->Word32Popcnt().op();
        break;
      } else {
        return BuildI32Popcnt(input);
      }
    }
    case wasm::kExprF32Floor: {
      if (!m->Float32RoundDown().IsSupported()) return BuildF32Floor(input);
      op = m->Float32RoundDown().op();
      break;
    }
    case wasm::kExprF32Ceil: {
      if (!m->Float32RoundUp().IsSupported()) return BuildF32Ceil(input);
      op = m->Float32RoundUp().op();
      break;
    }
    case wasm::kExprF32Trunc: {
      if (!m->Float32RoundTruncate().IsSupported()) return BuildF32Trunc(input);
      op = m->Float32RoundTruncate().op();
      break;
    }
    case wasm::kExprF32NearestInt: {
      if (!m->Float32RoundTiesEven().IsSupported())
        return BuildF32NearestInt(input);
      op = m->Float32RoundTiesEven().op();
      break;
    }
    case wasm::kExprF64Floor: {
      if (!m->Float64RoundDown().IsSupported()) return BuildF64Floor(input);
      op = m->Float64RoundDown().op();
      break;
    }
    case wasm::kExprF64Ceil: {
      if (!m->Float64RoundUp().IsSupported()) return BuildF64Ceil(input);
      op = m->Float64RoundUp().op();
      break;
    }
    case wasm::kExprF64Trunc: {
      if (!m->Float64RoundTruncate().IsSupported()) return BuildF64Trunc(input);
      op = m->Float64RoundTruncate().op();
      break;
    }
    case wasm::kExprF64NearestInt: {
      if (!m->Float64RoundTiesEven().IsSupported())
        return BuildF64NearestInt(input);
      op = m->Float64RoundTiesEven().op();
      break;
    }
    case wasm::kExprF64Acos: {
      return BuildF64Acos(input);
    }
    case wasm::kExprF64Asin: {
      return BuildF64Asin(input);
    }
    case wasm::kExprF64Atan:
      op = m->Float64Atan();
      break;
    case wasm::kExprF64Cos: {
      op = m->Float64Cos();
      break;
    }
    case wasm::kExprF64Sin: {
      op = m->Float64Sin();
      break;
    }
    case wasm::kExprF64Tan: {
      op = m->Float64Tan();
      break;
    }
    case wasm::kExprF64Exp: {
      op = m->Float64Exp();
      break;
    }
    case wasm::kExprF64Log:
      op = m->Float64Log();
      break;
    case wasm::kExprI32ConvertI64:
      op = m->TruncateInt64ToInt32();
      break;
    case wasm::kExprI64SConvertI32:
      op = m->ChangeInt32ToInt64();
      break;
    case wasm::kExprI64UConvertI32:
      op = m->ChangeUint32ToUint64();
      break;
    case wasm::kExprF64ReinterpretI64:
      op = m->BitcastInt64ToFloat64();
      break;
    case wasm::kExprI64ReinterpretF64:
      op = m->BitcastFloat64ToInt64();
      break;
    case wasm::kExprI64Clz:
      return m->Is64()
                 ? graph()->NewNode(m->Word64Clz(), input)
                 : graph()->NewNode(m->Word64ClzLowerable(), input, control());
    case wasm::kExprI64Ctz: {
      if (m->Word64Ctz().IsSupported()) {
        return m->Is64() ? graph()->NewNode(m->Word64Ctz().op(), input)
                         : graph()->NewNode(m->Word64CtzLowerable().op(), input,
                                            control());
      } else if (m->Is32() && m->Word32Ctz().IsSupported()) {
        return graph()->NewNode(m->Word64CtzLowerable().placeholder(), input,
                                control());
      } else if (m->Word64ReverseBits().IsSupported()) {
        Node* reversed = graph()->NewNode(m->Word64ReverseBits().op(), input);
        Node* result = m->Is64() ? graph()->NewNode(m->Word64Clz(), reversed)
                                 : graph()->NewNode(m->Word64ClzLowerable(),
                                                    reversed, control());
        return result;
      } else {
        return BuildI64Ctz(input);
      }
    }
    case wasm::kExprI64Popcnt: {
      OptionalOperator popcnt64 = m->Word64Popcnt();
      if (popcnt64.IsSupported()) {
        op = popcnt64.op();
      } else if (m->Is32() && m->Word32Popcnt().IsSupported()) {
        op = popcnt64.placeholder();
      } else {
        return BuildI64Popcnt(input);
      }
      break;
    }
    case wasm::kExprI64Eqz:
      return gasm_->Word64Equal(input, Int64Constant(0));
    case wasm::kExprF32SConvertI64:
      if (m->Is32()) {
        return BuildF32SConvertI64(input);
      }
      op = m->RoundInt64ToFloat32();
      break;
    case wasm::kExprF32UConvertI64:
      if (m->Is32()) {
        return BuildF32UConvertI64(input);
      }
      op = m->RoundUint64ToFloat32();
      break;
    case wasm::kExprF64SConvertI64:
      if (m->Is32()) {
        return BuildF64SConvertI64(input);
      }
      op = m->RoundInt64ToFloat64();
      break;
    case wasm::kExprF64UConvertI64:
      if (m->Is32()) {
        return BuildF64UConvertI64(input);
      }
      op = m->RoundUint64ToFloat64();
      break;
    case wasm::kExprI32SExtendI8:
      op = m->SignExtendWord8ToInt32();
      break;
    case wasm::kExprI32SExtendI16:
      op = m->SignExtendWord16ToInt32();
      break;
    case wasm::kExprI64SExtendI8:
      op = m->SignExtendWord8ToInt64();
      break;
    case wasm::kExprI64SExtendI16:
      op = m->SignExtendWord16ToInt64();
      break;
    case wasm::kExprI64SExtendI32:
      op = m->SignExtendWord32ToInt64();
      break;
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI64SConvertSatF32:
    case wasm::kExprI64UConvertSatF32:
    case wasm::kExprI64SConvertSatF64:
    case wasm::kExprI64UConvertSatF64:
      return mcgraph()->machine()->Is32()
                 ? BuildCcallConvertFloat(input, position, opcode)
                 : BuildIntConvertFloat(input, position, opcode);
    case wasm::kExprRefIsNull:
      return IsNull(input);
    // We abuse ref.as_non_null, which isn't otherwise used in this switch, as
    // a sentinel for the negation of ref.is_null.
    case wasm::kExprRefAsNonNull:
      return gasm_->Int32Sub(gasm_->Int32Constant(1), IsNull(input));
    case wasm::kExprI32AsmjsLoadMem8S:
      return BuildAsmjsLoadMem(MachineType::Int8(), input);
    case wasm::kExprI32AsmjsLoadMem8U:
      return BuildAsmjsLoadMem(MachineType::Uint8(), input);
    case wasm::kExprI32AsmjsLoadMem16S:
      return BuildAsmjsLoadMem(MachineType::Int16(), input);
    case wasm::kExprI32AsmjsLoadMem16U:
      return BuildAsmjsLoadMem(MachineType::Uint16(), input);
    case wasm::kExprI32AsmjsLoadMem:
      return BuildAsmjsLoadMem(MachineType::Int32(), input);
    case wasm::kExprF32AsmjsLoadMem:
      return BuildAsmjsLoadMem(MachineType::Float32(), input);
    case wasm::kExprF64AsmjsLoadMem:
      return BuildAsmjsLoadMem(MachineType::Float64(), input);
    case wasm::kExprExternInternalize:
      return gasm_->WasmExternInternalize(input);
    case wasm::kExprExternExternalize:
      return gasm_->WasmExternExternalize(input);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
  return graph()->NewNode(op, input);
}

Node* WasmGraphBuilder::Float32Constant(float value) {
  return mcgraph()->Float32Constant(value);
}

Node* WasmGraphBuilder::Float64Constant(double value) {
  return mcgraph()->Float64Constant(value);
}

Node* WasmGraphBuilder::Simd128Constant(const uint8_t value[16]) {
  has_simd_ = true;
  return graph()->NewNode(mcgraph()->machine()->S128Const(value));
}

Node* WasmGraphBuilder::BranchNoHint(Node* cond, Node** true_node,
                                     Node** false_node) {
  return gasm_->Branch(cond, true_node, false_node, BranchHint::kNone);
}

Node* WasmGraphBuilder::BranchExpectFalse(Node* cond, Node** true_node,
                                          Node** false_node) {
  return gasm_->Branch(cond, true_node, false_node, BranchHint::kFalse);
}

Node* WasmGraphBuilder::BranchExpectTrue(Node* cond, Node** true_node,
                                         Node** false_node) {
  return gasm_->Branch(cond, true_node, false_node, BranchHint::kTrue);
}

Node* WasmGraphBuilder::Select(Node *cond, Node* true_node,
                               Node* false_node, wasm::ValueType type) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  wasm::ValueKind kind = type.kind();
  // Lower to select if supported.
  if (kind == wasm::kF32 && m->Float32Select().IsSupported()) {
    return mcgraph()->graph()->NewNode(m->Float32Select().op(), cond,
                                       true_node, false_node);
  }
  if (kind == wasm::kF64 && m->Float64Select().IsSupported()) {
    return mcgraph()->graph()->NewNode(m->Float64Select().op(), cond,
                                       true_node, false_node);
  }
  if (kind == wasm::kI32 && m->Word32Select().IsSupported()) {
    return mcgraph()->graph()->NewNode(m->Word32Select().op(), cond, true_node,
                                       false_node);
  }
  if (kind == wasm::kI64 && m->Word64Select().IsSupported()) {
    return mcgraph()->graph()->NewNode(m->Word64Select().op(), cond, true_node,
                                       false_node);
  }
  // Default to control-flow.
  Node* controls[2];
  BranchNoHint(cond, &controls[0], &controls[1]);
  Node* merge = Merge(2, controls);
  SetControl(merge);
  Node* inputs[] = {true_node, false_node, merge};
  return Phi(type, 2, inputs);
}

TrapId WasmGraphBuilder::GetTrapIdForTrap(wasm::TrapReason reason) {
  // TODO(wasm): "!env_" should not happen when compiling an actual wasm
  // function.
  if (!env_ || !env_->runtime_exception_support) {
    // We use TrapId::kInvalid as a marker to tell the code generator
    // to generate a call to a testing c-function instead of a runtime
    // stub. This code should only be called from a cctest.
    return TrapId::kInvalid;
  }

  switch (reason) {
#define TRAPREASON_TO_TRAPID(name)                                             \
  case wasm::k##name:                                                          \
    static_assert(                                                             \
        static_cast<int>(TrapId::k##name) == wasm::WasmCode::kThrowWasm##name, \
        "trap id mismatch");                                                   \
    return TrapId::k##name;
    FOREACH_WASM_TRAPREASON(TRAPREASON_TO_TRAPID)
#undef TRAPREASON_TO_TRAPID
    default:
      UNREACHABLE();
  }
}

void WasmGraphBuilder::TrapIfTrue(wasm::TrapReason reason, Node* cond,
                                  wasm::WasmCodePosition position) {
  TrapId trap_id = GetTrapIdForTrap(reason);
  gasm_->TrapIf(cond, trap_id);
  SetSourcePosition(control(), position);
}

void WasmGraphBuilder::TrapIfFalse(wasm::TrapReason reason, Node* cond,
                                   wasm::WasmCodePosition position) {
  TrapId trap_id = GetTrapIdForTrap(reason);
  gasm_->TrapUnless(cond, trap_id);
  SetSourcePosition(control(), position);
}

Node* WasmGraphBuilder::AssertNotNull(Node* object,
                                      wasm::WasmCodePosition position) {
  if (v8_flags.experimental_wasm_skip_null_checks) return object;
  Node* result = gasm_->AssertNotNull(object);
  SetSourcePosition(result, position);
  return result;
}

// Add a check that traps if {node} is equal to {val}.
void WasmGraphBuilder::TrapIfEq32(wasm::TrapReason reason, Node* node,
                                  int32_t val,
                                  wasm::WasmCodePosition position) {
  if (val == 0) {
    TrapIfFalse(reason, node, position);
  } else {
    TrapIfTrue(reason, gasm_->Word32Equal(node, Int32Constant(val)), position);
  }
}

// Add a check that traps if {node} is zero.
void WasmGraphBuilder::ZeroCheck32(wasm::TrapReason reason, Node* node,
                                   wasm::WasmCodePosition position) {
  TrapIfEq32(reason, node, 0, position);
}

// Add a check that traps if {node} is equal to {val}.
void WasmGraphBuilder::TrapIfEq64(wasm::TrapReason reason, Node* node,
                                  int64_t val,
                                  wasm::WasmCodePosition position) {
  TrapIfTrue(reason, gasm_->Word64Equal(node, Int64Constant(val)), position);
}

// Add a check that traps if {node} is zero.
void WasmGraphBuilder::ZeroCheck64(wasm::TrapReason reason, Node* node,
                                   wasm::WasmCodePosition position) {
  TrapIfEq64(reason, node, 0, position);
}

Node* WasmGraphBuilder::Switch(unsigned count, Node* key) {
  // The instruction selector will use {kArchTableSwitch} for large switches,
  // which has limited input count, see {InstructionSelector::EmitTableSwitch}.
  DCHECK_LE(count, Instruction::kMaxInputCount - 2);          // value_range + 2
  DCHECK_LE(count, wasm::kV8MaxWasmFunctionBrTableSize + 1);  // plus IfDefault
  return graph()->NewNode(mcgraph()->common()->Switch(count), key, control());
}

Node* WasmGraphBuilder::IfValue(int32_t value, Node* sw) {
  DCHECK_EQ(IrOpcode::kSwitch, sw->opcode());
  return graph()->NewNode(mcgraph()->common()->IfValue(value), sw);
}

Node* WasmGraphBuilder::IfDefault(Node* sw) {
  DCHECK_EQ(IrOpcode::kSwitch, sw->opcode());
  return graph()->NewNode(mcgraph()->common()->IfDefault(), sw);
}

Node* WasmGraphBuilder::Return(base::Vector<Node*> vals) {
  unsigned count = static_cast<unsigned>(vals.size());
  base::SmallVector<Node*, 8> buf(count + 3);

  buf[0] = Int32Constant(0);
  if (count > 0) {
    memcpy(buf.data() + 1, vals.begin(), sizeof(void*) * count);
  }
  buf[count + 1] = effect();
  buf[count + 2] = control();
  Node* ret = graph()->NewNode(mcgraph()->common()->Return(count), count + 3,
                               buf.data());

  gasm_->MergeControlToEnd(ret);
  return ret;
}

void WasmGraphBuilder::Trap(wasm::TrapReason reason,
                            wasm::WasmCodePosition position) {
  TrapIfFalse(reason, Int32Constant(0), position);
  // Connect control to end via a Throw() node.
  TerminateThrow(effect(), control());
}

Node* WasmGraphBuilder::MaskShiftCount32(Node* node) {
  static const int32_t kMask32 = 0x1F;
  if (!mcgraph()->machine()->Word32ShiftIsSafe()) {
    // Shifts by constants are so common we pattern-match them here.
    Int32Matcher match(node);
    if (match.HasResolvedValue()) {
      int32_t masked = (match.ResolvedValue() & kMask32);
      if (match.ResolvedValue() != masked) node = Int32Constant(masked);
    } else {
      node = gasm_->Word32And(node, Int32Constant(kMask32));
    }
  }
  return node;
}

Node* WasmGraphBuilder::MaskShiftCount64(Node* node) {
  static const int64_t kMask64 = 0x3F;
  if (!mcgraph()->machine()->Word32ShiftIsSafe()) {
    // Shifts by constants are so common we pattern-match them here.
    Int64Matcher match(node);
    if (match.HasResolvedValue()) {
      int64_t masked = (match.ResolvedValue() & kMask64);
      if (match.ResolvedValue() != masked) node = Int64Constant(masked);
    } else {
      node = gasm_->Word64And(node, Int64Constant(kMask64));
    }
  }
  return node;
}

namespace {

bool ReverseBytesSupported(MachineOperatorBuilder* m, size_t size_in_bytes) {
  switch (size_in_bytes) {
    case 4:
    case 16:
      return true;
    case 8:
      return m->Is64();
    default:
      break;
  }
  return false;
}

}  // namespace

Node* WasmGraphBuilder::BuildChangeEndiannessStore(
    Node* node, MachineRepresentation mem_rep, wasm::ValueType wasmtype) {
  Node* result;
  Node* value = node;
  MachineOperatorBuilder* m = mcgraph()->machine();
  int valueSizeInBytes = wasmtype.value_kind_size();
  int valueSizeInBits = 8 * valueSizeInBytes;
  bool isFloat = false;

  switch (wasmtype.kind()) {
    case wasm::kF64:
      value = gasm_->BitcastFloat64ToInt64(node);
      isFloat = true;
      V8_FALLTHROUGH;
    case wasm::kI64:
      result = Int64Constant(0);
      break;
    case wasm::kF32:
      value = gasm_->BitcastFloat32ToInt32(node);
      isFloat = true;
      V8_FALLTHROUGH;
    case wasm::kI32:
      result = Int32Constant(0);
      break;
    case wasm::kS128:
      DCHECK(ReverseBytesSupported(m, valueSizeInBytes));
      break;
    default:
      UNREACHABLE();
  }

  if (mem_rep == MachineRepresentation::kWord8) {
    // No need to change endianness for byte size, return original node
    return node;
  }
  if (wasmtype == wasm::kWasmI64 && mem_rep < MachineRepresentation::kWord64) {
    // In case we store lower part of WasmI64 expression, we can truncate
    // upper 32bits
    value = gasm_->TruncateInt64ToInt32(value);
    valueSizeInBytes = wasm::kWasmI32.value_kind_size();
    valueSizeInBits = 8 * valueSizeInBytes;
    if (mem_rep == MachineRepresentation::kWord16) {
      value = gasm_->Word32Shl(value, Int32Constant(16));
    }
  } else if (wasmtype == wasm::kWasmI32 &&
             mem_rep == MachineRepresentation::kWord16) {
    value = gasm_->Word32Shl(value, Int32Constant(16));
  }

  int i;
  uint32_t shiftCount;

  if (ReverseBytesSupported(m, valueSizeInBytes)) {
    switch (valueSizeInBytes) {
      case 4:
        result = gasm_->Word32ReverseBytes(value);
        break;
      case 8:
        result = gasm_->Word64ReverseBytes(value);
        break;
      case 16:
        result = graph()->NewNode(m->Simd128ReverseBytes(), value);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    for (i = 0, shiftCount = valueSizeInBits - 8; i < valueSizeInBits / 2;
         i += 8, shiftCount -= 16) {
      Node* shiftLower;
      Node* shiftHigher;
      Node* lowerByte;
      Node* higherByte;

      DCHECK_LT(0, shiftCount);
      DCHECK_EQ(0, (shiftCount + 8) % 16);

      if (valueSizeInBits > 32) {
        shiftLower = gasm_->Word64Shl(value, Int64Constant(shiftCount));
        shiftHigher = gasm_->Word64Shr(value, Int64Constant(shiftCount));
        lowerByte = gasm_->Word64And(
            shiftLower, Int64Constant(static_cast<uint64_t>(0xFF)
                                      << (valueSizeInBits - 8 - i)));
        higherByte = gasm_->Word64And(
            shiftHigher, Int64Constant(static_cast<uint64_t>(0xFF) << i));
        result = gasm_->Word64Or(result, lowerByte);
        result = gasm_->Word64Or(result, higherByte);
      } else {
        shiftLower = gasm_->Word32Shl(value, Int32Constant(shiftCount));
        shiftHigher = gasm_->Word32Shr(value, Int32Constant(shiftCount));
        lowerByte = gasm_->Word32And(
            shiftLower, Int32Constant(static_cast<uint32_t>(0xFF)
                                      << (valueSizeInBits - 8 - i)));
        higherByte = gasm_->Word32And(
            shiftHigher, Int32Constant(static_cast<uint32_t>(0xFF) << i));
        result = gasm_->Word32Or(result, lowerByte);
        result = gasm_->Word32Or(result, higherByte);
      }
    }
  }

  if (isFloat) {
    switch (wasmtype.kind()) {
      case wasm::kF64:
        result = gasm_->BitcastInt64ToFloat64(result);
        break;
      case wasm::kF32:
        result = gasm_->BitcastInt32ToFloat32(result);
        break;
      default:
        UNREACHABLE();
    }
  }

  return result;
}

Node* WasmGraphBuilder::BuildChangeEndiannessLoad(Node* node,
                                                  MachineType memtype,
                                                  wasm::ValueType wasmtype) {
  Node* result;
  Node* value = node;
  MachineOperatorBuilder* m = mcgraph()->machine();
  int valueSizeInBytes = ElementSizeInBytes(memtype.representation());
  int valueSizeInBits = 8 * valueSizeInBytes;
  bool isFloat = false;

  switch (memtype.representation()) {
    case MachineRepresentation::kFloat64:
      value = gasm_->BitcastFloat64ToInt64(node);
      isFloat = true;
      V8_FALLTHROUGH;
    case MachineRepresentation::kWord64:
      result = Int64Constant(0);
      break;
    case MachineRepresentation::kFloat32:
      value = gasm_->BitcastFloat32ToInt32(node);
      isFloat = true;
      V8_FALLTHROUGH;
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kWord16:
      result = Int32Constant(0);
      break;
    case MachineRepresentation::kWord8:
      // No need to change endianness for byte size, return original node
      return node;
    case MachineRepresentation::kSimd128:
      DCHECK(ReverseBytesSupported(m, valueSizeInBytes));
      break;
    default:
      UNREACHABLE();
  }

  int i;
  uint32_t shiftCount;

  if (ReverseBytesSupported(m, valueSizeInBytes < 4 ? 4 : valueSizeInBytes)) {
    switch (valueSizeInBytes) {
      case 2:
        result = gasm_->Word32ReverseBytes(
            gasm_->Word32Shl(value, Int32Constant(16)));
        break;
      case 4:
        result = gasm_->Word32ReverseBytes(value);
        break;
      case 8:
        result = gasm_->Word64ReverseBytes(value);
        break;
      case 16:
        result = graph()->NewNode(m->Simd128ReverseBytes(), value);
        break;
      default:
        UNREACHABLE();
    }
  } else {
    for (i = 0, shiftCount = valueSizeInBits - 8; i < valueSizeInBits / 2;
         i += 8, shiftCount -= 16) {
      Node* shiftLower;
      Node* shiftHigher;
      Node* lowerByte;
      Node* higherByte;

      DCHECK_LT(0, shiftCount);
      DCHECK_EQ(0, (shiftCount + 8) % 16);

      if (valueSizeInBits > 32) {
        shiftLower = gasm_->Word64Shl(value, Int64Constant(shiftCount));
        shiftHigher = gasm_->Word64Shr(value, Int64Constant(shiftCount));
        lowerByte = gasm_->Word64And(
            shiftLower, Int64Constant(static_cast<uint64_t>(0xFF)
                                      << (valueSizeInBits - 8 - i)));
        higherByte = gasm_->Word64And(
            shiftHigher, Int64Constant(static_cast<uint64_t>(0xFF) << i));
        result = gasm_->Word64Or(result, lowerByte);
        result = gasm_->Word64Or(result, higherByte);
      } else {
        shiftLower = gasm_->Word32Shl(value, Int32Constant(shiftCount));
        shiftHigher = gasm_->Word32Shr(value, Int32Constant(shiftCount));
        lowerByte = gasm_->Word32And(
            shiftLower, Int32Constant(static_cast<uint32_t>(0xFF)
                                      << (valueSizeInBits - 8 - i)));
        higherByte = gasm_->Word32And(
            shiftHigher, Int32Constant(static_cast<uint32_t>(0xFF) << i));
        result = gasm_->Word32Or(result, lowerByte);
        result = gasm_->Word32Or(result, higherByte);
      }
    }
  }

  if (isFloat) {
    switch (memtype.representation()) {
      case MachineRepresentation::kFloat64:
        result = gasm_->BitcastInt64ToFloat64(result);
        break;
      case MachineRepresentation::kFloat32:
        result = gasm_->BitcastInt32ToFloat32(result);
        break;
      default:
        UNREACHABLE();
    }
  }

  // We need to sign or zero extend the value
  if (memtype.IsSigned()) {
    DCHECK(!isFloat);
    if (valueSizeInBits < 32) {
      Node* shiftBitCount;
      // Perform sign extension using following trick
      // result = (x << machine_width - type_width) >> (machine_width -
      // type_width)
      if (wasmtype == wasm::kWasmI64) {
        shiftBitCount = Int32Constant(64 - valueSizeInBits);
        result = gasm_->Word64Sar(
            gasm_->Word64Shl(gasm_->ChangeInt32ToInt64(result), shiftBitCount),
            shiftBitCount);
      } else if (wasmtype == wasm::kWasmI32) {
        shiftBitCount = Int32Constant(32 - valueSizeInBits);
        result = gasm_->Word32Sar(gasm_->Word32Shl(result, shiftBitCount),
                                  shiftBitCount);
      }
    }
  } else if (wasmtype == wasm::kWasmI64 && valueSizeInBits < 64) {
    result = gasm_->ChangeUint32ToUint64(result);
  }

  return result;
}

Node* WasmGraphBuilder::BuildF32CopySign(Node* left, Node* right) {
  Node* result = Unop(
      wasm::kExprF32ReinterpretI32,
      Binop(wasm::kExprI32Ior,
            Binop(wasm::kExprI32And, Unop(wasm::kExprI32ReinterpretF32, left),
                  Int32Constant(0x7FFFFFFF)),
            Binop(wasm::kExprI32And, Unop(wasm::kExprI32ReinterpretF32, right),
                  Int32Constant(0x80000000))));

  return result;
}

Node* WasmGraphBuilder::BuildF64CopySign(Node* left, Node* right) {
  if (mcgraph()->machine()->Is64()) {
    return gasm_->BitcastInt64ToFloat64(
        gasm_->Word64Or(gasm_->Word64And(gasm_->BitcastFloat64ToInt64(left),
                                         Int64Constant(0x7FFFFFFFFFFFFFFF)),
                        gasm_->Word64And(gasm_->BitcastFloat64ToInt64(right),
                                         Int64Constant(0x8000000000000000))));
  }

  DCHECK(mcgraph()->machine()->Is32());

  Node* high_word_left = gasm_->Float64ExtractHighWord32(left);
  Node* high_word_right = gasm_->Float64ExtractHighWord32(right);

  Node* new_high_word = gasm_->Word32Or(
      gasm_->Word32And(high_word_left, Int32Constant(0x7FFFFFFF)),
      gasm_->Word32And(high_word_right, Int32Constant(0x80000000)));

  return gasm_->Float64InsertHighWord32(left, new_high_word);
}

namespace {

MachineType IntConvertType(wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32SConvertSatF32:
    case wasm::kExprI32SConvertSatF64:
      return MachineType::Int32();
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI32UConvertSatF32:
    case wasm::kExprI32UConvertSatF64:
      return MachineType::Uint32();
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64SConvertSatF32:
    case wasm::kExprI64SConvertSatF64:
      return MachineType::Int64();
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI64UConvertSatF32:
    case wasm::kExprI64UConvertSatF64:
      return MachineType::Uint64();
    default:
      UNREACHABLE();
  }
}

MachineType FloatConvertType(wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32SConvertSatF32:
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI32UConvertSatF32:
    case wasm::kExprI64SConvertSatF32:
    case wasm::kExprI64UConvertSatF32:
      return MachineType::Float32();
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI32SConvertSatF64:
    case wasm::kExprI32UConvertSatF64:
    case wasm::kExprI64SConvertSatF64:
    case wasm::kExprI64UConvertSatF64:
      return MachineType::Float64();
    default:
      UNREACHABLE();
  }
}

const Operator* ConvertOp(WasmGraphBuilder* builder, wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
      return builder->mcgraph()->machine()->TruncateFloat32ToInt32(
          TruncateKind::kSetOverflowToMin);
    case wasm::kExprI32SConvertSatF32:
      return builder->mcgraph()->machine()->TruncateFloat32ToInt32(
          TruncateKind::kArchitectureDefault);
    case wasm::kExprI32UConvertF32:
      return builder->mcgraph()->machine()->TruncateFloat32ToUint32(
          TruncateKind::kSetOverflowToMin);
    case wasm::kExprI32UConvertSatF32:
      return builder->mcgraph()->machine()->TruncateFloat32ToUint32(
          TruncateKind::kArchitectureDefault);
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32SConvertSatF64:
      return builder->mcgraph()->machine()->ChangeFloat64ToInt32();
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI32UConvertSatF64:
      return builder->mcgraph()->machine()->TruncateFloat64ToUint32();
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64SConvertSatF32:
      return builder->mcgraph()->machine()->TryTruncateFloat32ToInt64();
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64UConvertSatF32:
      return builder->mcgraph()->machine()->TryTruncateFloat32ToUint64();
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64SConvertSatF64:
      return builder->mcgraph()->machine()->TryTruncateFloat64ToInt64();
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI64UConvertSatF64:
      return builder->mcgraph()->machine()->TryTruncateFloat64ToUint64();
    default:
      UNREACHABLE();
  }
}

wasm::WasmOpcode ConvertBackOp(wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32SConvertSatF32:
      return wasm::kExprF32SConvertI32;
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32UConvertSatF32:
      return wasm::kExprF32UConvertI32;
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32SConvertSatF64:
      return wasm::kExprF64SConvertI32;
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI32UConvertSatF64:
      return wasm::kExprF64UConvertI32;
    default:
      UNREACHABLE();
  }
}

bool IsTrappingConvertOp(wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64UConvertF64:
      return true;
    case wasm::kExprI32SConvertSatF64:
    case wasm::kExprI32UConvertSatF64:
    case wasm::kExprI32SConvertSatF32:
    case wasm::kExprI32UConvertSatF32:
    case wasm::kExprI64SConvertSatF32:
    case wasm::kExprI64UConvertSatF32:
    case wasm::kExprI64SConvertSatF64:
    case wasm::kExprI64UConvertSatF64:
      return false;
    default:
      UNREACHABLE();
  }
}

Node* Zero(WasmGraphBuilder* builder, const MachineType& ty) {
  switch (ty.representation()) {
    case MachineRepresentation::kWord32:
      return builder->Int32Constant(0);
    case MachineRepresentation::kWord64:
      return builder->Int64Constant(0);
    case MachineRepresentation::kFloat32:
      return builder->Float32Constant(0.0);
    case MachineRepresentation::kFloat64:
      return builder->Float64Constant(0.0);
    default:
      UNREACHABLE();
  }
}

Node* Min(WasmGraphBuilder* builder, const MachineType& ty) {
  switch (ty.semantic()) {
    case MachineSemantic::kInt32:
      return builder->Int32Constant(std::numeric_limits<int32_t>::min());
    case MachineSemantic::kUint32:
      return builder->Int32Constant(std::numeric_limits<uint32_t>::min());
    case MachineSemantic::kInt64:
      return builder->Int64Constant(std::numeric_limits<int64_t>::min());
    case MachineSemantic::kUint64:
      return builder->Int64Constant(std::numeric_limits<uint64_t>::min());
    default:
      UNREACHABLE();
  }
}

Node* Max(WasmGraphBuilder* builder, const MachineType& ty) {
  switch (ty.semantic()) {
    case MachineSemantic::kInt32:
      return builder->Int32Constant(std::numeric_limits<int32_t>::max());
    case MachineSemantic::kUint32:
      return builder->Int32Constant(std::numeric_limits<uint32_t>::max());
    case MachineSemantic::kInt64:
      return builder->Int64Constant(std::numeric_limits<int64_t>::max());
    case MachineSemantic::kUint64:
      return builder->Int64Constant(std::numeric_limits<uint64_t>::max());
    default:
      UNREACHABLE();
  }
}

wasm::WasmOpcode TruncOp(const MachineType& ty) {
  switch (ty.representation()) {
    case MachineRepresentation::kFloat32:
      return wasm::kExprF32Trunc;
    case MachineRepresentation::kFloat64:
      return wasm::kExprF64Trunc;
    default:
      UNREACHABLE();
  }
}

wasm::WasmOpcode NeOp(const MachineType& ty) {
  switch (ty.representation()) {
    case MachineRepresentation::kFloat32:
      return wasm::kExprF32Ne;
    case MachineRepresentation::kFloat64:
      return wasm::kExprF64Ne;
    default:
      UNREACHABLE();
  }
}

wasm::WasmOpcode LtOp(const MachineType& ty) {
  switch (ty.representation()) {
    case MachineRepresentation::kFloat32:
      return wasm::kExprF32Lt;
    case MachineRepresentation::kFloat64:
      return wasm::kExprF64Lt;
    default:
      UNREACHABLE();
  }
}

Node* ConvertTrapTest(WasmGraphBuilder* builder, wasm::WasmOpcode opcode,
                      const MachineType& int_ty, const MachineType& float_ty,
                      Node* trunc, Node* converted_value) {
  if (int_ty.representation() == MachineRepresentation::kWord32) {
    Node* check = builder->Unop(ConvertBackOp(opcode), converted_value);
    return builder->Binop(NeOp(float_ty), trunc, check);
  }
  return builder->graph()->NewNode(builder->mcgraph()->common()->Projection(1),
                                   trunc, builder->graph()->start());
}

Node* ConvertSaturateTest(WasmGraphBuilder* builder, wasm::WasmOpcode opcode,
                          const MachineType& int_ty,
                          const MachineType& float_ty, Node* trunc,
                          Node* converted_value) {
  Node* test = ConvertTrapTest(builder, opcode, int_ty, float_ty, trunc,
                               converted_value);
  if (int_ty.representation() == MachineRepresentation::kWord64) {
    test = builder->Binop(wasm::kExprI64Eq, test, builder->Int64Constant(0));
  }
  return test;
}

}  // namespace

Node* WasmGraphBuilder::BuildIntConvertFloat(Node* input,
                                             wasm::WasmCodePosition position,
                                             wasm::WasmOpcode opcode) {
  const MachineType int_ty = IntConvertType(opcode);
  const MachineType float_ty = FloatConvertType(opcode);
  const Operator* conv_op = ConvertOp(this, opcode);
  Node* trunc = nullptr;
  Node* converted_value = nullptr;
  const bool is_int32 =
      int_ty.representation() == MachineRepresentation::kWord32;
  if (is_int32) {
    trunc = Unop(TruncOp(float_ty), input);
    converted_value = graph()->NewNode(conv_op, trunc);
  } else {
    trunc = graph()->NewNode(conv_op, input);
    converted_value = graph()->NewNode(mcgraph()->common()->Projection(0),
                                       trunc, graph()->start());
  }
  if (IsTrappingConvertOp(opcode)) {
    Node* test =
        ConvertTrapTest(this, opcode, int_ty, float_ty, trunc, converted_value);
    if (is_int32) {
      TrapIfTrue(wasm::kTrapFloatUnrepresentable, test, position);
    } else {
      ZeroCheck64(wasm::kTrapFloatUnrepresentable, test, position);
    }
    return converted_value;
  }
  if (mcgraph()->machine()->SatConversionIsSafe()) {
    return converted_value;
  }
  Node* test = ConvertSaturateTest(this, opcode, int_ty, float_ty, trunc,
                                   converted_value);
  Diamond tl_d(graph(), mcgraph()->common(), test, BranchHint::kFalse);
  tl_d.Chain(control());
  Node* nan_test = Binop(NeOp(float_ty), input, input);
  Diamond nan_d(graph(), mcgraph()->common(), nan_test, BranchHint::kFalse);
  nan_d.Nest(tl_d, true);
  Node* neg_test = Binop(LtOp(float_ty), input, Zero(this, float_ty));
  Diamond sat_d(graph(), mcgraph()->common(), neg_test, BranchHint::kNone);
  sat_d.Nest(nan_d, false);
  Node* sat_val =
      sat_d.Phi(int_ty.representation(), Min(this, int_ty), Max(this, int_ty));
  Node* nan_val =
      nan_d.Phi(int_ty.representation(), Zero(this, int_ty), sat_val);
  return tl_d.Phi(int_ty.representation(), nan_val, converted_value);
}

Node* WasmGraphBuilder::BuildI32AsmjsSConvertF32(Node* input) {
  // asm.js must use the wacky JS semantics.
  return gasm_->TruncateFloat64ToWord32(gasm_->ChangeFloat32ToFloat64(input));
}

Node* WasmGraphBuilder::BuildI32AsmjsSConvertF64(Node* input) {
  // asm.js must use the wacky JS semantics.
  return gasm_->TruncateFloat64ToWord32(input);
}

Node* WasmGraphBuilder::BuildI32AsmjsUConvertF32(Node* input) {
  // asm.js must use the wacky JS semantics.
  return gasm_->TruncateFloat64ToWord32(gasm_->ChangeFloat32ToFloat64(input));
}

Node* WasmGraphBuilder::BuildI32AsmjsUConvertF64(Node* input) {
  // asm.js must use the wacky JS semantics.
  return gasm_->TruncateFloat64ToWord32(input);
}

Node* WasmGraphBuilder::BuildBitCountingCall(Node* input, ExternalReference ref,
                                             MachineRepresentation input_type) {
  Node* stack_slot_param = StoreArgsInStackSlot({{input_type, input}});

  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);

  Node* function = gasm_->ExternalConstant(ref);

  return BuildCCall(&sig, function, stack_slot_param);
}

Node* WasmGraphBuilder::BuildI32Ctz(Node* input) {
  return BuildBitCountingCall(input, ExternalReference::wasm_word32_ctz(),
                              MachineRepresentation::kWord32);
}

Node* WasmGraphBuilder::BuildI64Ctz(Node* input) {
  return Unop(wasm::kExprI64UConvertI32,
              BuildBitCountingCall(input, ExternalReference::wasm_word64_ctz(),
                                   MachineRepresentation::kWord64));
}

Node* WasmGraphBuilder::BuildI32Popcnt(Node* input) {
  return BuildBitCountingCall(input, ExternalReference::wasm_word32_popcnt(),
                              MachineRepresentation::kWord32);
}

Node* WasmGraphBuilder::BuildI64Popcnt(Node* input) {
  return Unop(
      wasm::kExprI64UConvertI32,
      BuildBitCountingCall(input, ExternalReference::wasm_word64_popcnt(),
                           MachineRepresentation::kWord64));
}

Node* WasmGraphBuilder::BuildF32Trunc(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref = ExternalReference::wasm_f32_trunc();

  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32Floor(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref = ExternalReference::wasm_f32_floor();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32Ceil(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref = ExternalReference::wasm_f32_ceil();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32NearestInt(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref = ExternalReference::wasm_f32_nearest_int();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Trunc(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_f64_trunc();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Floor(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_f64_floor();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Ceil(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_f64_ceil();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64NearestInt(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_f64_nearest_int();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Acos(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::f64_acos_wrapper_function();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Asin(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::f64_asin_wrapper_function();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Pow(Node* left, Node* right) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_float64_pow();
  return BuildCFuncInstruction(ref, type, left, right);
}

Node* WasmGraphBuilder::BuildF64Mod(Node* left, Node* right) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::f64_mod_wrapper_function();
  return BuildCFuncInstruction(ref, type, left, right);
}

Node* WasmGraphBuilder::BuildCFuncInstruction(ExternalReference ref,
                                              MachineType type, Node* input0,
                                              Node* input1) {
  // We do truncation by calling a C function which calculates the result.
  // The input is passed to the C function as a byte buffer holding the two
  // input doubles. We reserve this byte buffer as a stack slot, store the
  // parameters in this buffer slots, pass a pointer to the buffer to the C
  // function, and after calling the C function we collect the return value from
  // the buffer.
  Node* stack_slot;
  if (input1) {
    stack_slot = StoreArgsInStackSlot(
        {{type.representation(), input0}, {type.representation(), input1}});
  } else {
    stack_slot = StoreArgsInStackSlot({{type.representation(), input0}});
  }

  MachineType sig_types[] = {MachineType::Pointer()};
  MachineSignature sig(0, 1, sig_types);
  Node* function = gasm_->ExternalConstant(ref);
  BuildCCall(&sig, function, stack_slot);

  return gasm_->LoadFromObject(type, stack_slot, 0);
}

Node* WasmGraphBuilder::BuildF32SConvertI64(Node* input) {
  // TODO(titzer/bradnelson): Check handlng of asm.js case.
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_int64_to_float32(),
      MachineRepresentation::kWord64, MachineType::Float32());
}
Node* WasmGraphBuilder::BuildF32UConvertI64(Node* input) {
  // TODO(titzer/bradnelson): Check handlng of asm.js case.
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_uint64_to_float32(),
      MachineRepresentation::kWord64, MachineType::Float32());
}
Node* WasmGraphBuilder::BuildF64SConvertI64(Node* input) {
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_int64_to_float64(),
      MachineRepresentation::kWord64, MachineType::Float64());
}
Node* WasmGraphBuilder::BuildF64UConvertI64(Node* input) {
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_uint64_to_float64(),
      MachineRepresentation::kWord64, MachineType::Float64());
}

Node* WasmGraphBuilder::BuildIntToFloatConversionInstruction(
    Node* input, ExternalReference ref,
    MachineRepresentation parameter_representation,
    const MachineType result_type) {
  int stack_slot_size =
      std::max(ElementSizeInBytes(parameter_representation),
               ElementSizeInBytes(result_type.representation()));
  Node* stack_slot =
      graph()->NewNode(mcgraph()->machine()->StackSlot(stack_slot_size));
  auto store_rep =
      StoreRepresentation(parameter_representation, kNoWriteBarrier);
  gasm_->Store(store_rep, stack_slot, 0, input);
  MachineType sig_types[] = {MachineType::Pointer()};
  MachineSignature sig(0, 1, sig_types);
  Node* function = gasm_->ExternalConstant(ref);
  BuildCCall(&sig, function, stack_slot);
  return gasm_->LoadFromObject(result_type, stack_slot, 0);
}

namespace {

ExternalReference convert_ccall_ref(wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64SConvertSatF32:
      return ExternalReference::wasm_float32_to_int64();
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64UConvertSatF32:
      return ExternalReference::wasm_float32_to_uint64();
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64SConvertSatF64:
      return ExternalReference::wasm_float64_to_int64();
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI64UConvertSatF64:
      return ExternalReference::wasm_float64_to_uint64();
    default:
      UNREACHABLE();
  }
}

}  // namespace

Node* WasmGraphBuilder::BuildCcallConvertFloat(Node* input,
                                               wasm::WasmCodePosition position,
                                               wasm::WasmOpcode opcode) {
  const MachineType int_ty = IntConvertType(opcode);
  const MachineType float_ty = FloatConvertType(opcode);
  ExternalReference call_ref = convert_ccall_ref(opcode);
  int stack_slot_size = std::max(ElementSizeInBytes(int_ty.representation()),
                                 ElementSizeInBytes(float_ty.representation()));
  Node* stack_slot =
      graph()->NewNode(mcgraph()->machine()->StackSlot(stack_slot_size));
  auto store_rep =
      StoreRepresentation(float_ty.representation(), kNoWriteBarrier);
  gasm_->Store(store_rep, stack_slot, 0, input);
  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);
  Node* function = gasm_->ExternalConstant(call_ref);
  Node* overflow = BuildCCall(&sig, function, stack_slot);
  if (IsTrappingConvertOp(opcode)) {
    ZeroCheck32(wasm::kTrapFloatUnrepresentable, overflow, position);
    return gasm_->LoadFromObject(int_ty, stack_slot, 0);
  }
  Node* test = Binop(wasm::kExprI32Eq, overflow, Int32Constant(0), position);
  Diamond tl_d(graph(), mcgraph()->common(), test, BranchHint::kFalse);
  tl_d.Chain(control());
  Node* nan_test = Binop(NeOp(float_ty), input, input);
  Diamond nan_d(graph(), mcgraph()->common(), nan_test, BranchHint::kFalse);
  nan_d.Nest(tl_d, true);
  Node* neg_test = Binop(LtOp(float_ty), input, Zero(this, float_ty));
  Diamond sat_d(graph(), mcgraph()->common(), neg_test, BranchHint::kNone);
  sat_d.Nest(nan_d, false);
  Node* sat_val =
      sat_d.Phi(int_ty.representation(), Min(this, int_ty), Max(this, int_ty));
  Node* load = gasm_->LoadFromObject(int_ty, stack_slot, 0);
  Node* nan_val =
      nan_d.Phi(int_ty.representation(), Zero(this, int_ty), sat_val);
  return tl_d.Phi(int_ty.representation(), nan_val, load);
}

Node* WasmGraphBuilder::MemoryGrow(Node* input) {
  needs_stack_check_ = true;
  if (!env_->module->is_memory64) {
    // For 32-bit memories, just call the builtin.
    return gasm_->CallRuntimeStub(wasm::WasmCode::kWasmMemoryGrow,
                                  Operator::kNoThrow, input);
  }

  // If the input is not a positive int32, growing will always fail
  // (growing negative or requesting >= 256 TB).
  Node* old_effect = effect();
  Diamond is_32_bit(graph(), mcgraph()->common(),
                    gasm_->Uint64LessThanOrEqual(input, Int64Constant(kMaxInt)),
                    BranchHint::kTrue);
  is_32_bit.Chain(control());

  SetControl(is_32_bit.if_true);

  Node* grow_result = gasm_->ChangeInt32ToInt64(gasm_->CallRuntimeStub(
      wasm::WasmCode::kWasmMemoryGrow, Operator::kNoThrow,
      gasm_->TruncateInt64ToInt32(input)));

  Node* diamond_result = is_32_bit.Phi(MachineRepresentation::kWord64,
                                       grow_result, gasm_->Int64Constant(-1));
  SetEffectControl(is_32_bit.EffectPhi(effect(), old_effect), is_32_bit.merge);
  return diamond_result;
}

Node* WasmGraphBuilder::Throw(uint32_t tag_index, const wasm::WasmTag* tag,
                              const base::Vector<Node*> values,
                              wasm::WasmCodePosition position) {
  needs_stack_check_ = true;
  uint32_t encoded_size = WasmExceptionPackage::GetEncodedSize(tag);

  Node* values_array = gasm_->CallRuntimeStub(
      wasm::WasmCode::kWasmAllocateFixedArray, Operator::kNoThrow,
      gasm_->IntPtrConstant(encoded_size));
  SetSourcePosition(values_array, position);

  uint32_t index = 0;
  const wasm::WasmTagSig* sig = tag->sig;
  MachineOperatorBuilder* m = mcgraph()->machine();
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    Node* value = values[i];
    switch (sig->GetParam(i).kind()) {
      case wasm::kF32:
        value = gasm_->BitcastFloat32ToInt32(value);
        V8_FALLTHROUGH;
      case wasm::kI32:
        BuildEncodeException32BitValue(values_array, &index, value);
        break;
      case wasm::kF64:
        value = gasm_->BitcastFloat64ToInt64(value);
        V8_FALLTHROUGH;
      case wasm::kI64: {
        Node* upper32 = gasm_->TruncateInt64ToInt32(
            Binop(wasm::kExprI64ShrU, value, Int64Constant(32)));
        BuildEncodeException32BitValue(values_array, &index, upper32);
        Node* lower32 = gasm_->TruncateInt64ToInt32(value);
        BuildEncodeException32BitValue(values_array, &index, lower32);
        break;
      }
      case wasm::kS128:
        BuildEncodeException32BitValue(
            values_array, &index,
            graph()->NewNode(m->I32x4ExtractLane(0), value));
        BuildEncodeException32BitValue(
            values_array, &index,
            graph()->NewNode(m->I32x4ExtractLane(1), value));
        BuildEncodeException32BitValue(
            values_array, &index,
            graph()->NewNode(m->I32x4ExtractLane(2), value));
        BuildEncodeException32BitValue(
            values_array, &index,
            graph()->NewNode(m->I32x4ExtractLane(3), value));
        break;
      case wasm::kRef:
      case wasm::kRefNull:
      case wasm::kRtt:
        gasm_->StoreFixedArrayElementAny(values_array, index, value);
        ++index;
        break;
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }
  }
  DCHECK_EQ(encoded_size, index);

  Node* exception_tag = LoadTagFromTable(tag_index);

  Node* throw_call = gasm_->CallRuntimeStub(wasm::WasmCode::kWasmThrow,
                                            Operator::kNoProperties,
                                            exception_tag, values_array);
  SetSourcePosition(throw_call, position);
  return throw_call;
}

void WasmGraphBuilder::BuildEncodeException32BitValue(Node* values_array,
                                                      uint32_t* index,
                                                      Node* value) {
  Node* upper_halfword_as_smi =
      gasm_->BuildChangeUint31ToSmi(gasm_->Word32Shr(value, Int32Constant(16)));
  gasm_->StoreFixedArrayElementSmi(values_array, *index, upper_halfword_as_smi);
  ++(*index);
  Node* lower_halfword_as_smi = gasm_->BuildChangeUint31ToSmi(
      gasm_->Word32And(value, Int32Constant(0xFFFFu)));
  gasm_->StoreFixedArrayElementSmi(values_array, *index, lower_halfword_as_smi);
  ++(*index);
}

Node* WasmGraphBuilder::BuildDecodeException32BitValue(Node* values_array,
                                                       uint32_t* index) {
  Node* upper = gasm_->BuildChangeSmiToInt32(
      gasm_->LoadFixedArrayElementSmi(values_array, *index));
  (*index)++;
  upper = gasm_->Word32Shl(upper, Int32Constant(16));
  Node* lower = gasm_->BuildChangeSmiToInt32(
      gasm_->LoadFixedArrayElementSmi(values_array, *index));
  (*index)++;
  Node* value = gasm_->Word32Or(upper, lower);
  return value;
}

Node* WasmGraphBuilder::BuildDecodeException64BitValue(Node* values_array,
                                                       uint32_t* index) {
  Node* upper = Binop(wasm::kExprI64Shl,
                      Unop(wasm::kExprI64UConvertI32,
                           BuildDecodeException32BitValue(values_array, index)),
                      Int64Constant(32));
  Node* lower = Unop(wasm::kExprI64UConvertI32,
                     BuildDecodeException32BitValue(values_array, index));
  return Binop(wasm::kExprI64Ior, upper, lower);
}

Node* WasmGraphBuilder::Rethrow(Node* except_obj) {
  // TODO(v8:8091): Currently the message of the original exception is not being
  // preserved when rethrown to the console. The pending message will need to be
  // saved when caught and restored here while being rethrown.
  return gasm_->CallRuntimeStub(wasm::WasmCode::kWasmRethrow,
                                Operator::kNoProperties, except_obj);
}

Node* WasmGraphBuilder::ExceptionTagEqual(Node* caught_tag,
                                          Node* expected_tag) {
  return gasm_->WordEqual(caught_tag, expected_tag);
}

Node* WasmGraphBuilder::LoadTagFromTable(uint32_t tag_index) {
  Node* tags_table =
      LOAD_INSTANCE_FIELD(TagsTable, MachineType::TaggedPointer());
  Node* tag = gasm_->LoadFixedArrayElementPtr(tags_table, tag_index);
  return tag;
}

Node* WasmGraphBuilder::GetExceptionTag(Node* except_obj) {
  return gasm_->CallBuiltin(
      Builtin::kWasmGetOwnProperty, Operator::kEliminatable, except_obj,
      LOAD_ROOT(wasm_exception_tag_symbol, wasm_exception_tag_symbol),
      LOAD_INSTANCE_FIELD(NativeContext, MachineType::TaggedPointer()));
}

Node* WasmGraphBuilder::GetExceptionValues(Node* except_obj,
                                           const wasm::WasmTag* tag,
                                           base::Vector<Node*> values) {
  Node* values_array = gasm_->CallBuiltin(
      Builtin::kWasmGetOwnProperty, Operator::kEliminatable, except_obj,
      LOAD_ROOT(wasm_exception_values_symbol, wasm_exception_values_symbol),
      LOAD_INSTANCE_FIELD(NativeContext, MachineType::TaggedPointer()));
  uint32_t index = 0;
  const wasm::WasmTagSig* sig = tag->sig;
  DCHECK_EQ(sig->parameter_count(), values.size());
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    Node* value;
    switch (sig->GetParam(i).kind()) {
      case wasm::kI32:
        value = BuildDecodeException32BitValue(values_array, &index);
        break;
      case wasm::kI64:
        value = BuildDecodeException64BitValue(values_array, &index);
        break;
      case wasm::kF32: {
        value = Unop(wasm::kExprF32ReinterpretI32,
                     BuildDecodeException32BitValue(values_array, &index));
        break;
      }
      case wasm::kF64: {
        value = Unop(wasm::kExprF64ReinterpretI64,
                     BuildDecodeException64BitValue(values_array, &index));
        break;
      }
      case wasm::kS128:
        value = graph()->NewNode(
            mcgraph()->machine()->I32x4Splat(),
            BuildDecodeException32BitValue(values_array, &index));
        value = graph()->NewNode(
            mcgraph()->machine()->I32x4ReplaceLane(1), value,
            BuildDecodeException32BitValue(values_array, &index));
        value = graph()->NewNode(
            mcgraph()->machine()->I32x4ReplaceLane(2), value,
            BuildDecodeException32BitValue(values_array, &index));
        value = graph()->NewNode(
            mcgraph()->machine()->I32x4ReplaceLane(3), value,
            BuildDecodeException32BitValue(values_array, &index));
        break;
      case wasm::kRef:
      case wasm::kRefNull:
      case wasm::kRtt:
        value = gasm_->LoadFixedArrayElementAny(values_array, index);
        ++index;
        break;
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }
    values[i] = value;
  }
  DCHECK_EQ(index, WasmExceptionPackage::GetEncodedSize(tag));
  return values_array;
}

Node* WasmGraphBuilder::BuildI32DivS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  ZeroCheck32(wasm::kTrapDivByZero, right, position);
  Node* previous_effect = effect();
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(gasm_->Word32Equal(right, Int32Constant(-1)), &denom_is_m1,
                    &denom_is_not_m1);
  SetControl(denom_is_m1);
  TrapIfEq32(wasm::kTrapDivUnrepresentable, left, kMinInt, position);
  Node* merge = Merge(control(), denom_is_not_m1);
  SetEffectControl(graph()->NewNode(mcgraph()->common()->EffectPhi(2), effect(),
                                    previous_effect, merge),
                   merge);
  return gasm_->Int32Div(left, right);
}

Node* WasmGraphBuilder::BuildI32RemS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = mcgraph()->machine();

  ZeroCheck32(wasm::kTrapRemByZero, right, position);

  Diamond d(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(-1)), BranchHint::kFalse);
  d.Chain(control());

  return d.Phi(MachineRepresentation::kWord32, Int32Constant(0),
               graph()->NewNode(m->Int32Mod(), left, right, d.if_false));
}

Node* WasmGraphBuilder::BuildI32DivU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  ZeroCheck32(wasm::kTrapDivByZero, right, position);
  return gasm_->Uint32Div(left, right);
}

Node* WasmGraphBuilder::BuildI32RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  ZeroCheck32(wasm::kTrapRemByZero, right, position);
  return gasm_->Uint32Mod(left, right);
}

Node* WasmGraphBuilder::BuildI32AsmjsDivS(Node* left, Node* right) {
  MachineOperatorBuilder* m = mcgraph()->machine();

  Int32Matcher mr(right);
  if (mr.HasResolvedValue()) {
    if (mr.ResolvedValue() == 0) {
      return Int32Constant(0);
    } else if (mr.ResolvedValue() == -1) {
      // The result is the negation of the left input.
      return gasm_->Int32Sub(Int32Constant(0), left);
    }
    return gasm_->Int32Div(left, right);
  }

  // asm.js semantics return 0 on divide or mod by zero.
  if (m->Int32DivIsSafe()) {
    // The hardware instruction does the right thing (e.g. arm).
    return gasm_->Int32Div(left, right);
  }

  // Check denominator for zero.
  Diamond z(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(0)), BranchHint::kFalse);
  z.Chain(control());

  // Check denominator for -1. (avoid minint / -1 case).
  Diamond n(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(-1)), BranchHint::kFalse);
  n.Chain(z.if_false);

  Node* div = graph()->NewNode(m->Int32Div(), left, right, n.if_false);

  Node* neg = gasm_->Int32Sub(Int32Constant(0), left);

  return z.Phi(MachineRepresentation::kWord32, Int32Constant(0),
               n.Phi(MachineRepresentation::kWord32, neg, div));
}

Node* WasmGraphBuilder::BuildI32AsmjsRemS(Node* left, Node* right) {
  CommonOperatorBuilder* c = mcgraph()->common();
  MachineOperatorBuilder* m = mcgraph()->machine();
  Node* const zero = Int32Constant(0);

  Int32Matcher mr(right);
  if (mr.HasResolvedValue()) {
    if (mr.ResolvedValue() == 0 || mr.ResolvedValue() == -1) {
      return zero;
    }
    return gasm_->Int32Mod(left, right);
  }

  // General case for signed integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if 0 < right then
  //     msk = right - 1
  //     if right & msk != 0 then
  //       left % right
  //     else
  //       if left < 0 then
  //         -(-left & msk)
  //       else
  //         left & msk
  //   else
  //     if right < -1 then
  //       left % right
  //     else
  //       zero
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  Node* const minus_one = Int32Constant(-1);

  const Operator* const merge_op = c->Merge(2);
  const Operator* const phi_op = c->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = gasm_->Int32LessThan(zero, right);
  Node* branch0 =
      graph()->NewNode(c->Branch(BranchHint::kTrue), check0, control());

  Node* if_true0 = graph()->NewNode(c->IfTrue(), branch0);
  Node* true0;
  {
    Node* msk = graph()->NewNode(m->Int32Add(), right, minus_one);

    Node* check1 = graph()->NewNode(m->Word32And(), right, msk);
    Node* branch1 = graph()->NewNode(c->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(c->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(m->Int32Mod(), left, right, if_true1);

    Node* if_false1 = graph()->NewNode(c->IfFalse(), branch1);
    Node* false1;
    {
      Node* check2 = graph()->NewNode(m->Int32LessThan(), left, zero);
      Node* branch2 =
          graph()->NewNode(c->Branch(BranchHint::kFalse), check2, if_false1);

      Node* if_true2 = graph()->NewNode(c->IfTrue(), branch2);
      Node* true2 = graph()->NewNode(
          m->Int32Sub(), zero,
          graph()->NewNode(m->Word32And(),
                           graph()->NewNode(m->Int32Sub(), zero, left), msk));

      Node* if_false2 = graph()->NewNode(c->IfFalse(), branch2);
      Node* false2 = graph()->NewNode(m->Word32And(), left, msk);

      if_false1 = graph()->NewNode(merge_op, if_true2, if_false2);
      false1 = graph()->NewNode(phi_op, true2, false2, if_false1);
    }

    if_true0 = graph()->NewNode(merge_op, if_true1, if_false1);
    true0 = graph()->NewNode(phi_op, true1, false1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(c->IfFalse(), branch0);
  Node* false0;
  {
    Node* check1 = graph()->NewNode(m->Int32LessThan(), right, minus_one);
    Node* branch1 =
        graph()->NewNode(c->Branch(BranchHint::kTrue), check1, if_false0);

    Node* if_true1 = graph()->NewNode(c->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(m->Int32Mod(), left, right, if_true1);

    Node* if_false1 = graph()->NewNode(c->IfFalse(), branch1);
    Node* false1 = zero;

    if_false0 = graph()->NewNode(merge_op, if_true1, if_false1);
    false0 = graph()->NewNode(phi_op, true1, false1, if_false0);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}

Node* WasmGraphBuilder::BuildI32AsmjsDivU(Node* left, Node* right) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  // asm.js semantics return 0 on divide or mod by zero.
  if (m->Uint32DivIsSafe()) {
    // The hardware instruction does the right thing (e.g. arm).
    return gasm_->Uint32Div(left, right);
  }

  // Explicit check for x % 0.
  Diamond z(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(0)), BranchHint::kFalse);
  z.Chain(control());

  return z.Phi(MachineRepresentation::kWord32, Int32Constant(0),
               graph()->NewNode(mcgraph()->machine()->Uint32Div(), left, right,
                                z.if_false));
}

Node* WasmGraphBuilder::BuildI32AsmjsRemU(Node* left, Node* right) {
  // asm.js semantics return 0 on divide or mod by zero.
  // Explicit check for x % 0.
  Diamond z(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(0)), BranchHint::kFalse);
  z.Chain(control());

  Node* rem = graph()->NewNode(mcgraph()->machine()->Uint32Mod(), left, right,
                               z.if_false);
  return z.Phi(MachineRepresentation::kWord32, Int32Constant(0), rem);
}

Node* WasmGraphBuilder::BuildI64DivS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_int64_div(),
                          MachineType::Int64(), wasm::kTrapDivByZero, position);
  }
  ZeroCheck64(wasm::kTrapDivByZero, right, position);
  Node* previous_effect = effect();
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(gasm_->Word64Equal(right, Int64Constant(-1)), &denom_is_m1,
                    &denom_is_not_m1);
  SetControl(denom_is_m1);
  TrapIfEq64(wasm::kTrapDivUnrepresentable, left,
             std::numeric_limits<int64_t>::min(), position);
  Node* merge = Merge(control(), denom_is_not_m1);
  SetEffectControl(graph()->NewNode(mcgraph()->common()->EffectPhi(2), effect(),
                                    previous_effect, merge),
                   merge);
  return gasm_->Int64Div(left, right);
}

Node* WasmGraphBuilder::BuildI64RemS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_int64_mod(),
                          MachineType::Int64(), wasm::kTrapRemByZero, position);
  }
  ZeroCheck64(wasm::kTrapRemByZero, right, position);
  Diamond d(mcgraph()->graph(), mcgraph()->common(),
            gasm_->Word64Equal(right, Int64Constant(-1)));

  d.Chain(control());

  Node* rem = graph()->NewNode(mcgraph()->machine()->Int64Mod(), left, right,
                               d.if_false);

  return d.Phi(MachineRepresentation::kWord64, Int64Constant(0), rem);
}

Node* WasmGraphBuilder::BuildI64DivU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_uint64_div(),
                          MachineType::Int64(), wasm::kTrapDivByZero, position);
  }
  ZeroCheck64(wasm::kTrapDivByZero, right, position);
  return gasm_->Uint64Div(left, right);
}
Node* WasmGraphBuilder::BuildI64RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_uint64_mod(),
                          MachineType::Int64(), wasm::kTrapRemByZero, position);
  }
  ZeroCheck64(wasm::kTrapRemByZero, right, position);
  return gasm_->Uint64Mod(left, right);
}

Node* WasmGraphBuilder::BuildDiv64Call(Node* left, Node* right,
                                       ExternalReference ref,
                                       MachineType result_type,
                                       wasm::TrapReason trap_zero,
                                       wasm::WasmCodePosition position) {
  Node* stack_slot =
      StoreArgsInStackSlot({{MachineRepresentation::kWord64, left},
                            {MachineRepresentation::kWord64, right}});

  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);

  Node* function = gasm_->ExternalConstant(ref);
  Node* call = BuildCCall(&sig, function, stack_slot);

  ZeroCheck32(trap_zero, call, position);
  TrapIfEq32(wasm::kTrapDivUnrepresentable, call, -1, position);
  return gasm_->Load(result_type, stack_slot, 0);
}

Node* WasmGraphBuilder::IsNull(Node* object) {
  return (v8_flags.experimental_wasm_gc && parameter_mode_ == kInstanceMode)
             ? gasm_->IsNull(object)
             : gasm_->TaggedEqual(object, RefNull());
}

template <typename... Args>
Node* WasmGraphBuilder::BuildCCall(MachineSignature* sig, Node* function,
                                   Args... args) {
  DCHECK_LE(sig->return_count(), 1);
  DCHECK_EQ(sizeof...(args), sig->parameter_count());
  Node* call_args[] = {function, args..., effect(), control()};

  auto call_descriptor =
      Linkage::GetSimplifiedCDescriptor(mcgraph()->zone(), sig);

  return gasm_->Call(call_descriptor, arraysize(call_args), call_args);
}

Node* WasmGraphBuilder::BuildCallNode(const wasm::FunctionSig* sig,
                                      base::Vector<Node*> args,
                                      wasm::WasmCodePosition position,
                                      Node* instance_node, const Operator* op,
                                      Node* frame_state) {
  if (instance_node == nullptr) {
    instance_node = GetInstance();
  }
  needs_stack_check_ = true;
  const size_t params = sig->parameter_count();
  const size_t has_frame_state = frame_state != nullptr ? 1 : 0;
  const size_t extra = 3;  // instance_node, effect, and control.
  const size_t count = 1 + params + extra + has_frame_state;

  // Reallocate the buffer to make space for extra inputs.
  base::SmallVector<Node*, 16 + extra> inputs(count);
  DCHECK_EQ(1 + params, args.size());

  // Make room for the instance_node parameter at index 1, just after code.
  inputs[0] = args[0];  // code
  inputs[1] = instance_node;
  if (params > 0) memcpy(&inputs[2], &args[1], params * sizeof(Node*));

  // Add effect and control inputs.
  if (has_frame_state != 0) inputs[params + 2] = frame_state;
  inputs[params + has_frame_state + 2] = effect();
  inputs[params + has_frame_state + 3] = control();

  Node* call = graph()->NewNode(op, static_cast<int>(count), inputs.begin());
  // Return calls have no effect output. Other calls are the new effect node.
  if (op->EffectOutputCount() > 0) SetEffect(call);
  DCHECK(position == wasm::kNoCodePosition || position > 0);
  if (position > 0) SetSourcePosition(call, position);

  return call;
}

Node* WasmGraphBuilder::BuildWasmCall(const wasm::FunctionSig* sig,
                                      base::Vector<Node*> args,
                                      base::Vector<Node*> rets,
                                      wasm::WasmCodePosition position,
                                      Node* instance_node, Node* frame_state) {
  CallDescriptor* call_descriptor = GetWasmCallDescriptor(
      mcgraph()->zone(), sig, kWasmFunction, frame_state != nullptr);
  const Operator* op = mcgraph()->common()->Call(call_descriptor);
  Node* call =
      BuildCallNode(sig, args, position, instance_node, op, frame_state);
  // TODO(manoskouk): These assume the call has control and effect outputs.
  DCHECK_GT(op->ControlOutputCount(), 0);
  DCHECK_GT(op->EffectOutputCount(), 0);
  SetEffectControl(call);

  size_t ret_count = sig->return_count();
  if (ret_count == 0) return call;  // No return value.

  DCHECK_EQ(ret_count, rets.size());
  if (ret_count == 1) {
    // Only a single return value.
    rets[0] = call;
  } else {
    // Create projections for all return values.
    for (size_t i = 0; i < ret_count; i++) {
      rets[i] = graph()->NewNode(mcgraph()->common()->Projection(i), call,
                                 graph()->start());
    }
  }
  return call;
}

Node* WasmGraphBuilder::BuildWasmReturnCall(const wasm::FunctionSig* sig,
                                            base::Vector<Node*> args,
                                            wasm::WasmCodePosition position,
                                            Node* instance_node) {
  CallDescriptor* call_descriptor =
      GetWasmCallDescriptor(mcgraph()->zone(), sig);
  const Operator* op = mcgraph()->common()->TailCall(call_descriptor);
  Node* call = BuildCallNode(sig, args, position, instance_node, op);

  // TODO(manoskouk): If we have kNoThrow calls, do not merge them to end.
  DCHECK_GT(call->op()->ControlOutputCount(), 0);
  gasm_->MergeControlToEnd(call);

  return call;
}

Node* WasmGraphBuilder::BuildImportCall(const wasm::FunctionSig* sig,
                                        base::Vector<Node*> args,
                                        base::Vector<Node*> rets,
                                        wasm::WasmCodePosition position,
                                        int func_index,
                                        IsReturnCall continuation) {
  return BuildImportCall(sig, args, rets, position,
                         gasm_->Uint32Constant(func_index), continuation);
}

Node* WasmGraphBuilder::BuildImportCall(const wasm::FunctionSig* sig,
                                        base::Vector<Node*> args,
                                        base::Vector<Node*> rets,
                                        wasm::WasmCodePosition position,
                                        Node* func_index,
                                        IsReturnCall continuation) {
  // Load the imported function refs array from the instance.
  Node* imported_function_refs =
      LOAD_INSTANCE_FIELD(ImportedFunctionRefs, MachineType::TaggedPointer());
  // Access fixed array at {header_size - tag + func_index * kTaggedSize}.
  Node* func_index_intptr = gasm_->BuildChangeUint32ToUintPtr(func_index);
  Node* ref_node = gasm_->LoadFixedArrayElement(
      imported_function_refs, func_index_intptr, MachineType::TaggedPointer());

  // Load the target from the imported_targets array at the offset of
  // {func_index}.
  Node* offset = gasm_->IntAdd(
      gasm_->IntMul(func_index_intptr,
                    gasm_->IntPtrConstant(kSystemPointerSize)),
      gasm_->IntPtrConstant(
          wasm::ObjectAccess::ToTagged(FixedArray::kObjectsOffset)));
  Node* imported_targets = LOAD_INSTANCE_FIELD(ImportedFunctionTargets,
                                               MachineType::TaggedPointer());
  Node* target_node = gasm_->LoadImmutableFromObject(MachineType::Pointer(),
                                                     imported_targets, offset);
  args[0] = target_node;

  switch (continuation) {
    case kCallContinues:
      return BuildWasmCall(sig, args, rets, position, ref_node);
    case kReturnCall:
      DCHECK(rets.empty());
      return BuildWasmReturnCall(sig, args, position, ref_node);
  }
}

Node* WasmGraphBuilder::CallDirect(uint32_t index, base::Vector<Node*> args,
                                   base::Vector<Node*> rets,
                                   wasm::WasmCodePosition position) {
  DCHECK_NULL(args[0]);
  const wasm::FunctionSig* sig = env_->module->functions[index].sig;

  if (env_ && index < env_->module->num_imported_functions) {
    // Call to an imported function.
    return BuildImportCall(sig, args, rets, position, index, kCallContinues);
  }

  // A direct call to a wasm function defined in this module.
  // Just encode the function index. This will be patched at instantiation.
  Address code = static_cast<Address>(index);
  args[0] = mcgraph()->RelocatableIntPtrConstant(code, RelocInfo::WASM_CALL);

  return BuildWasmCall(sig, args, rets, position, nullptr);
}

Node* WasmGraphBuilder::CallIndirect(uint32_t table_index, uint32_t sig_index,
                                     base::Vector<Node*> args,
                                     base::Vector<Node*> rets,
                                     wasm::WasmCodePosition position) {
  return BuildIndirectCall(table_index, sig_index, args, rets, position,
                           kCallContinues);
}

void WasmGraphBuilder::LoadIndirectFunctionTable(uint32_t table_index,
                                                 Node** ift_size,
                                                 Node** ift_sig_ids,
                                                 Node** ift_targets,
                                                 Node** ift_instances) {
  bool needs_dynamic_size = true;
  const wasm::WasmTable& table = env_->module->tables[table_index];
  if (table.has_maximum_size && table.maximum_size == table.initial_size) {
    *ift_size = Int32Constant(table.initial_size);
    needs_dynamic_size = false;
  }

  if (table_index == 0) {
    if (needs_dynamic_size) {
      *ift_size = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTableSize,
                                              MachineType::Uint32());
    }
    *ift_sig_ids = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTableSigIds,
                                               MachineType::Pointer());
    *ift_targets = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTableTargets,
                                               MachineType::Pointer());
    *ift_instances = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTableRefs,
                                                 MachineType::TaggedPointer());
    return;
  }

  Node* ift_tables = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTables,
                                                 MachineType::TaggedPointer());
  Node* ift_table = gasm_->LoadFixedArrayElementAny(ift_tables, table_index);

  if (needs_dynamic_size) {
    *ift_size = gasm_->LoadFromObject(
        MachineType::Int32(), ift_table,
        wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kSizeOffset));
  }

  *ift_sig_ids = gasm_->LoadFromObject(
      MachineType::Pointer(), ift_table,
      wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kSigIdsOffset));

  *ift_targets = gasm_->LoadFromObject(
      MachineType::Pointer(), ift_table,
      wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kTargetsOffset));

  *ift_instances = gasm_->LoadFromObject(
      MachineType::TaggedPointer(), ift_table,
      wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kRefsOffset));
}

Node* WasmGraphBuilder::BuildIndirectCall(uint32_t table_index,
                                          uint32_t sig_index,
                                          base::Vector<Node*> args,
                                          base::Vector<Node*> rets,
                                          wasm::WasmCodePosition position,
                                          IsReturnCall continuation) {
  DCHECK_NOT_NULL(args[0]);
  DCHECK_NOT_NULL(env_);

  // First we have to load the table.
  Node* ift_size;
  Node* ift_sig_ids;
  Node* ift_targets;
  Node* ift_instances;
  LoadIndirectFunctionTable(table_index, &ift_size, &ift_sig_ids, &ift_targets,
                            &ift_instances);

  const wasm::FunctionSig* sig = env_->module->signature(sig_index);

  Node* key = args[0];

  // Bounds check against the table size.
  Node* in_bounds = gasm_->Uint32LessThan(key, ift_size);
  TrapIfFalse(wasm::kTrapTableOutOfBounds, in_bounds, position);

  // Check that the table entry is not null and that the type of the function is
  // **identical with** the function type declared at the call site (no
  // subtyping of functions is allowed).
  // Note: Since null entries are identified by having ift_sig_id (-1), we only
  // need one comparison.
  // TODO(9495): Change this if we should do full function subtyping instead.
  Node* expected_sig_id;
  if (v8_flags.wasm_type_canonicalization) {
    Node* isorecursive_canonical_types =
        LOAD_INSTANCE_FIELD(IsorecursiveCanonicalTypes, MachineType::Pointer());
    expected_sig_id = gasm_->LoadImmutable(
        MachineType::Uint32(), isorecursive_canonical_types,
        gasm_->IntPtrConstant(sig_index * kInt32Size));
  } else {
    expected_sig_id =
        Int32Constant(env_->module->per_module_canonical_type_ids[sig_index]);
  }

  Node* int32_scaled_key = gasm_->BuildChangeUint32ToUintPtr(
      gasm_->Word32Shl(key, Int32Constant(2)));
  Node* loaded_sig = gasm_->LoadFromObject(MachineType::Int32(), ift_sig_ids,
                                           int32_scaled_key);
  Node* sig_match = gasm_->Word32Equal(loaded_sig, expected_sig_id);

  TrapIfFalse(wasm::kTrapFuncSigMismatch, sig_match, position);

  Node* key_intptr = gasm_->BuildChangeUint32ToUintPtr(key);

  Node* target_instance = gasm_->LoadFixedArrayElement(
      ift_instances, key_intptr, MachineType::TaggedPointer());

  Node* intptr_scaled_key =
      gasm_->IntMul(key_intptr, gasm_->IntPtrConstant(kSystemPointerSize));

  Node* target = gasm_->LoadFromObject(MachineType::Pointer(), ift_targets,
                                       intptr_scaled_key);

  args[0] = target;

  switch (continuation) {
    case kCallContinues:
      return BuildWasmCall(sig, args, rets, position, target_instance);
    case kReturnCall:
      return BuildWasmReturnCall(sig, args, position, target_instance);
  }
}

Node* WasmGraphBuilder::BuildLoadExternalPointerFromObject(
    Node* object, int offset, ExternalPointerTag tag) {
#ifdef V8_ENABLE_SANDBOX
  if (IsSandboxedExternalPointerType(tag)) {
    DCHECK(!IsSharedExternalPointerType(tag));
    Node* external_pointer = gasm_->LoadFromObject(
        MachineType::Uint32(), object, wasm::ObjectAccess::ToTagged(offset));
    static_assert(kExternalPointerIndexShift > kSystemPointerSizeLog2);
    Node* shift_amount = gasm_->Int32Constant(kExternalPointerIndexShift -
                                              kSystemPointerSizeLog2);
    Node* scaled_index = gasm_->Word32Shr(external_pointer, shift_amount);
    Node* isolate_root = BuildLoadIsolateRoot();
    Node* table =
        gasm_->LoadFromObject(MachineType::Pointer(), isolate_root,
                              IsolateData::external_pointer_table_offset() +
                                  Internals::kExternalPointerTableBufferOffset);
    Node* decoded_ptr =
        gasm_->Load(MachineType::Pointer(), table, scaled_index);
    return gasm_->WordAnd(decoded_ptr, gasm_->IntPtrConstant(~tag));
  }
#endif
  return gasm_->LoadFromObject(MachineType::Pointer(), object,
                               wasm::ObjectAccess::ToTagged(offset));
}

Node* WasmGraphBuilder::BuildLoadCallTargetFromExportedFunctionData(
    Node* function) {
  Node* internal = gasm_->LoadFromObject(
      MachineType::TaggedPointer(), function,
      wasm::ObjectAccess::ToTagged(WasmExportedFunctionData::kInternalOffset));
  return BuildLoadExternalPointerFromObject(
      internal, WasmInternalFunction::kCallTargetOffset,
      kWasmInternalFunctionCallTargetTag);
}

// TODO(9495): Support CAPI function refs.
Node* WasmGraphBuilder::BuildCallRef(const wasm::FunctionSig* sig,
                                     base::Vector<Node*> args,
                                     base::Vector<Node*> rets,
                                     CheckForNull null_check,
                                     IsReturnCall continuation,
                                     wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    args[0] = AssertNotNull(args[0], position);
  }

  Node* function = args[0];

  auto load_target = gasm_->MakeLabel();
  auto end_label = gasm_->MakeLabel(MachineType::PointerRepresentation());

  Node* ref_node = gasm_->LoadImmutableFromObject(
      MachineType::TaggedPointer(), function,
      wasm::ObjectAccess::ToTagged(WasmInternalFunction::kRefOffset));

  Node* target = BuildLoadExternalPointerFromObject(
      function, WasmInternalFunction::kCallTargetOffset,
      kWasmInternalFunctionCallTargetTag);
  Node* is_null_target = gasm_->WordEqual(target, gasm_->IntPtrConstant(0));
  gasm_->GotoIfNot(is_null_target, &end_label, target);
  {
    // Compute the call target from the (on-heap) wrapper code. The cached
    // target can only be null for WasmJSFunctions.
    Node* wrapper_code = gasm_->LoadImmutableFromObject(
        MachineType::TaggedPointer(), function,
        wasm::ObjectAccess::ToTagged(WasmInternalFunction::kCodeOffset));
    Node* call_target;
    if (V8_EXTERNAL_CODE_SPACE_BOOL) {
      call_target =
          gasm_->LoadFromObject(MachineType::Pointer(), wrapper_code,
                                wasm::ObjectAccess::ToTagged(
                                    CodeDataContainer::kCodeEntryPointOffset));
    } else {
      call_target = gasm_->IntAdd(
          wrapper_code, gasm_->IntPtrConstant(
                            wasm::ObjectAccess::ToTagged(Code::kHeaderSize)));
    }
    gasm_->Goto(&end_label, call_target);
  }

  gasm_->Bind(&end_label);

  args[0] = end_label.PhiAt(0);

  Node* call = continuation == kCallContinues
                   ? BuildWasmCall(sig, args, rets, position, ref_node)
                   : BuildWasmReturnCall(sig, args, position, ref_node);
  return call;
}

void WasmGraphBuilder::CompareToInternalFunctionAtIndex(Node* func_ref,
                                                        uint32_t function_index,
                                                        Node** success_control,
                                                        Node** failure_control,
                                                        bool is_last_case) {
  // Since we are comparing to a function reference, it is guaranteed that
  // instance->wasm_internal_functions() has been initialized.
  Node* internal_functions = gasm_->LoadImmutable(
      MachineType::TaggedPointer(), GetInstance(),
      wasm::ObjectAccess::ToTagged(
          WasmInstanceObject::kWasmInternalFunctionsOffset));
  // We cannot use an immutable load here, since function references are
  // initialized lazily: Calling {RefFunc()} between two invocations of this
  // function may initialize the function, i.e. mutate the object we are
  // loading.
  Node* function_ref_at_index = gasm_->LoadFixedArrayElement(
      internal_functions, gasm_->IntPtrConstant(function_index),
      MachineType::AnyTagged());
  BranchHint hint = is_last_case ? BranchHint::kTrue : BranchHint::kNone;
  gasm_->Branch(gasm_->TaggedEqual(function_ref_at_index, func_ref),
                success_control, failure_control, hint);
}

Node* WasmGraphBuilder::CallRef(const wasm::FunctionSig* sig,
                                base::Vector<Node*> args,
                                base::Vector<Node*> rets,
                                WasmGraphBuilder::CheckForNull null_check,
                                wasm::WasmCodePosition position) {
  return BuildCallRef(sig, args, rets, null_check, IsReturnCall::kCallContinues,
                      position);
}

Node* WasmGraphBuilder::ReturnCallRef(const wasm::FunctionSig* sig,
                                      base::Vector<Node*> args,
                                      WasmGraphBuilder::CheckForNull null_check,
                                      wasm::WasmCodePosition position) {
  return BuildCallRef(sig, args, {}, null_check, IsReturnCall::kReturnCall,
                      position);
}

Node* WasmGraphBuilder::ReturnCall(uint32_t index, base::Vector<Node*> args,
                                   wasm::WasmCodePosition position) {
  DCHECK_NULL(args[0]);
  const wasm::FunctionSig* sig = env_->module->functions[index].sig;

  if (env_ && index < env_->module->num_imported_functions) {
    // Return Call to an imported function.
    return BuildImportCall(sig, args, {}, position, index, kReturnCall);
  }

  // A direct tail call to a wasm function defined in this module.
  // Just encode the function index. This will be patched during code
  // generation.
  Address code = static_cast<Address>(index);
  args[0] = mcgraph()->RelocatableIntPtrConstant(code, RelocInfo::WASM_CALL);

  return BuildWasmReturnCall(sig, args, position, nullptr);
}

Node* WasmGraphBuilder::ReturnCallIndirect(uint32_t table_index,
                                           uint32_t sig_index,
                                           base::Vector<Node*> args,
                                           wasm::WasmCodePosition position) {
  return BuildIndirectCall(table_index, sig_index, args, {}, position,
                           kReturnCall);
}

void WasmGraphBuilder::BrOnNull(Node* ref_object, Node** null_node,
                                Node** non_null_node) {
  BranchExpectFalse(IsNull(ref_object), null_node, non_null_node);
}

Node* WasmGraphBuilder::BuildI32Rol(Node* left, Node* right) {
  // Implement Rol by Ror since TurboFan does not have Rol opcode.
  // TODO(weiliang): support Word32Rol opcode in TurboFan.
  Int32Matcher m(right);
  if (m.HasResolvedValue()) {
    return Binop(wasm::kExprI32Ror, left,
                 Int32Constant(32 - (m.ResolvedValue() & 0x1F)));
  } else {
    return Binop(wasm::kExprI32Ror, left,
                 Binop(wasm::kExprI32Sub, Int32Constant(32), right));
  }
}

Node* WasmGraphBuilder::BuildI64Rol(Node* left, Node* right) {
  // Implement Rol by Ror since TurboFan does not have Rol opcode.
  // TODO(weiliang): support Word64Rol opcode in TurboFan.
  Int64Matcher m(right);
  Node* inv_right = m.HasResolvedValue()
                        ? Int64Constant(64 - (m.ResolvedValue() & 0x3F))
                        : Binop(wasm::kExprI64Sub, Int64Constant(64), right);
  return Binop(wasm::kExprI64Ror, left, inv_right);
}

Node* WasmGraphBuilder::Invert(Node* node) {
  return Unop(wasm::kExprI32Eqz, node);
}

void WasmGraphBuilder::InitInstanceCache(
    WasmInstanceCacheNodes* instance_cache) {
  // We handle caching of the instance cache nodes manually, and we may reload
  // them in contexts where load elimination would eliminate the reload.
  // Therefore, we use plain Load nodes which are not subject to load
  // elimination.

  // Load the memory start.
#ifdef V8_ENABLE_SANDBOX
  instance_cache->mem_start = LOAD_INSTANCE_FIELD_NO_ELIMINATION(
      MemoryStart, MachineType::SandboxedPointer());
#else
  instance_cache->mem_start =
      LOAD_INSTANCE_FIELD_NO_ELIMINATION(MemoryStart, MachineType::UintPtr());
#endif

  // Load the memory size.
  instance_cache->mem_size =
      LOAD_INSTANCE_FIELD_NO_ELIMINATION(MemorySize, MachineType::UintPtr());
}

void WasmGraphBuilder::PrepareInstanceCacheForLoop(
    WasmInstanceCacheNodes* instance_cache, Node* control) {
#define INTRODUCE_PHI(field, rep)                                            \
  instance_cache->field = graph()->NewNode(mcgraph()->common()->Phi(rep, 1), \
                                           instance_cache->field, control);

  INTRODUCE_PHI(mem_start, MachineType::PointerRepresentation());
  INTRODUCE_PHI(mem_size, MachineType::PointerRepresentation());
#undef INTRODUCE_PHI
}

void WasmGraphBuilder::NewInstanceCacheMerge(WasmInstanceCacheNodes* to,
                                             WasmInstanceCacheNodes* from,
                                             Node* merge) {
#define INTRODUCE_PHI(field, rep)                                            \
  if (to->field != from->field) {                                            \
    Node* vals[] = {to->field, from->field, merge};                          \
    to->field = graph()->NewNode(mcgraph()->common()->Phi(rep, 2), 3, vals); \
  }

  INTRODUCE_PHI(mem_start, MachineType::PointerRepresentation());
  INTRODUCE_PHI(mem_size, MachineRepresentation::kWord32);
#undef INTRODUCE_PHI
}

void WasmGraphBuilder::MergeInstanceCacheInto(WasmInstanceCacheNodes* to,
                                              WasmInstanceCacheNodes* from,
                                              Node* merge) {
  to->mem_size = CreateOrMergeIntoPhi(MachineType::PointerRepresentation(),
                                      merge, to->mem_size, from->mem_size);
  to->mem_start = CreateOrMergeIntoPhi(MachineType::PointerRepresentation(),
                                       merge, to->mem_start, from->mem_start);
}

Node* WasmGraphBuilder::CreateOrMergeIntoPhi(MachineRepresentation rep,
                                             Node* merge, Node* tnode,
                                             Node* fnode) {
  if (IsPhiWithMerge(tnode, merge)) {
    AppendToPhi(tnode, fnode);
  } else if (tnode != fnode) {
    // Note that it is not safe to use {Buffer} here since this method is used
    // via {CheckForException} while the {Buffer} is in use by another method.
    uint32_t count = merge->InputCount();
    // + 1 for the merge node.
    base::SmallVector<Node*, 9> inputs(count + 1);
    for (uint32_t j = 0; j < count - 1; j++) inputs[j] = tnode;
    inputs[count - 1] = fnode;
    inputs[count] = merge;
    tnode = graph()->NewNode(mcgraph()->common()->Phi(rep, count), count + 1,
                             inputs.begin());
  }
  return tnode;
}

Node* WasmGraphBuilder::CreateOrMergeIntoEffectPhi(Node* merge, Node* tnode,
                                                   Node* fnode) {
  if (IsPhiWithMerge(tnode, merge)) {
    AppendToPhi(tnode, fnode);
  } else if (tnode != fnode) {
    // Note that it is not safe to use {Buffer} here since this method is used
    // via {CheckForException} while the {Buffer} is in use by another method.
    uint32_t count = merge->InputCount();
    // + 1 for the merge node.
    base::SmallVector<Node*, 9> inputs(count + 1);
    for (uint32_t j = 0; j < count - 1; j++) {
      inputs[j] = tnode;
    }
    inputs[count - 1] = fnode;
    inputs[count] = merge;
    tnode = graph()->NewNode(mcgraph()->common()->EffectPhi(count), count + 1,
                             inputs.begin());
  }
  return tnode;
}

Node* WasmGraphBuilder::effect() { return gasm_->effect(); }

Node* WasmGraphBuilder::control() { return gasm_->control(); }

Node* WasmGraphBuilder::SetEffect(Node* node) {
  SetEffectControl(node, control());
  return node;
}

Node* WasmGraphBuilder::SetControl(Node* node) {
  SetEffectControl(effect(), node);
  return node;
}

void WasmGraphBuilder::SetEffectControl(Node* effect, Node* control) {
  gasm_->InitializeEffectControl(effect, control);
}

Node* WasmGraphBuilder::MemBuffer(uintptr_t offset) {
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_start = instance_cache_->mem_start;
  DCHECK_NOT_NULL(mem_start);
  if (offset == 0) return mem_start;
  return gasm_->IntAdd(mem_start, gasm_->UintPtrConstant(offset));
}

Node* WasmGraphBuilder::CurrentMemoryPages() {
  // CurrentMemoryPages can not be called from asm.js.
  DCHECK_EQ(wasm::kWasmOrigin, env_->module->origin);
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_size = instance_cache_->mem_size;
  DCHECK_NOT_NULL(mem_size);
  Node* result =
      gasm_->WordShr(mem_size, Int32Constant(wasm::kWasmPageSizeLog2));
  result = env_->module->is_memory64
               ? gasm_->BuildChangeIntPtrToInt64(result)
               : gasm_->BuildTruncateIntPtrToInt32(result);
  return result;
}

// Only call this function for code which is not reused across instantiations,
// as we do not patch the embedded js_context.
Node* WasmGraphBuilder::BuildCallToRuntimeWithContext(Runtime::FunctionId f,
                                                      Node* js_context,
                                                      Node** parameters,
                                                      int parameter_count) {
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
      mcgraph()->zone(), f, fun->nargs, Operator::kNoProperties,
      CallDescriptor::kNoFlags);
  // The CEntryStub is loaded from the IsolateRoot so that generated code is
  // Isolate independent. At the moment this is only done for CEntryStub(1).
  Node* isolate_root = BuildLoadIsolateRoot();
  DCHECK_EQ(1, fun->result_size);
  auto centry_id =
      Builtin::kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit;
  int builtin_slot_offset = IsolateData::BuiltinSlotOffset(centry_id);
  Node* centry_stub = gasm_->LoadFromObject(MachineType::Pointer(),
                                            isolate_root, builtin_slot_offset);
  // TODO(titzer): allow arbitrary number of runtime arguments
  // At the moment we only allow 5 parameters. If more parameters are needed,
  // increase this constant accordingly.
  static const int kMaxParams = 5;
  DCHECK_GE(kMaxParams, parameter_count);
  Node* inputs[kMaxParams + 6];
  int count = 0;
  inputs[count++] = centry_stub;
  for (int i = 0; i < parameter_count; i++) {
    inputs[count++] = parameters[i];
  }
  inputs[count++] =
      mcgraph()->ExternalConstant(ExternalReference::Create(f));  // ref
  inputs[count++] = Int32Constant(fun->nargs);                    // arity
  inputs[count++] = js_context;                                   // js_context
  inputs[count++] = effect();
  inputs[count++] = control();

  return gasm_->Call(call_descriptor, count, inputs);
}

Node* WasmGraphBuilder::BuildCallToRuntime(Runtime::FunctionId f,
                                           Node** parameters,
                                           int parameter_count) {
  return BuildCallToRuntimeWithContext(f, NoContextConstant(), parameters,
                                       parameter_count);
}

void WasmGraphBuilder::GetGlobalBaseAndOffset(const wasm::WasmGlobal& global,
                                              Node** base, Node** offset) {
  if (global.mutability && global.imported) {
    Node* imported_mutable_globals = LOAD_INSTANCE_FIELD(
        ImportedMutableGlobals, MachineType::TaggedPointer());
    Node* field_offset = Int32Constant(
        wasm::ObjectAccess::ElementOffsetInTaggedFixedAddressArray(
            global.index));
    if (global.type.is_reference()) {
      // Load the base from the ImportedMutableGlobalsBuffer of the instance.
      Node* buffers = LOAD_INSTANCE_FIELD(ImportedMutableGlobalsBuffers,
                                          MachineType::TaggedPointer());
      *base = gasm_->LoadFixedArrayElementAny(buffers, global.index);

      Node* index = gasm_->LoadFromObject(
          MachineType::Int32(), imported_mutable_globals, field_offset);
      // For this case, {index} gives the index of the global in the buffer.
      // From the index, calculate the actual offset in the FixedArray. This is
      // kHeaderSize + (index * kTaggedSize).
      *offset = gasm_->IntAdd(
          gasm_->IntMul(index, gasm_->IntPtrConstant(kTaggedSize)),
          gasm_->IntPtrConstant(
              wasm::ObjectAccess::ToTagged(FixedArray::kObjectsOffset)));
    } else {
      MachineType machine_type = V8_ENABLE_SANDBOX_BOOL
                                     ? MachineType::SandboxedPointer()
                                     : MachineType::UintPtr();
      *base = gasm_->LoadFromObject(machine_type, imported_mutable_globals,
                                    field_offset);
      *offset = gasm_->IntPtrConstant(0);
    }
  } else if (global.type.is_reference()) {
    *base =
        LOAD_INSTANCE_FIELD(TaggedGlobalsBuffer, MachineType::TaggedPointer());
    *offset = gasm_->IntPtrConstant(
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(global.offset));
  } else {
    MachineType machine_type = V8_ENABLE_SANDBOX_BOOL
                                   ? MachineType::SandboxedPointer()
                                   : MachineType::UintPtr();
    *base = LOAD_INSTANCE_FIELD(GlobalsStart, machine_type);
    *offset = gasm_->IntPtrConstant(global.offset);
  }
}

Node* WasmGraphBuilder::GlobalGet(uint32_t index) {
  const wasm::WasmGlobal& global = env_->module->globals[index];
  if (global.type == wasm::kWasmS128) has_simd_ = true;
  Node* base = nullptr;
  Node* offset = nullptr;
  GetGlobalBaseAndOffset(global, &base, &offset);
  MachineType mem_type = global.type.machine_type();
  return global.mutability ? gasm_->LoadFromObject(mem_type, base, offset)
                           : gasm_->LoadImmutable(mem_type, base, offset);
}

void WasmGraphBuilder::GlobalSet(uint32_t index, Node* val) {
  const wasm::WasmGlobal& global = env_->module->globals[index];
  if (global.type == wasm::kWasmS128) has_simd_ = true;
  Node* base = nullptr;
  Node* offset = nullptr;
  GetGlobalBaseAndOffset(global, &base, &offset);
  ObjectAccess access(global.type.machine_type(), global.type.is_reference()
                                                      ? kFullWriteBarrier
                                                      : kNoWriteBarrier);
  gasm_->StoreToObject(access, base, offset, val);
}

Node* WasmGraphBuilder::TableGet(uint32_t table_index, Node* index,
                                 wasm::WasmCodePosition position) {
  const wasm::WasmTable& table = env_->module->tables[table_index];
  bool is_funcref = IsSubtypeOf(table.type, wasm::kWasmFuncRef, env_->module);
  auto stub = is_funcref ? wasm::WasmCode::kWasmTableGetFuncRef
                         : wasm::WasmCode::kWasmTableGet;

  return gasm_->CallRuntimeStub(stub, Operator::kNoThrow,
                                gasm_->IntPtrConstant(table_index), index);
}

void WasmGraphBuilder::TableSet(uint32_t table_index, Node* index, Node* val,
                                wasm::WasmCodePosition position) {
  const wasm::WasmTable& table = env_->module->tables[table_index];
  bool is_funcref = IsSubtypeOf(table.type, wasm::kWasmFuncRef, env_->module);
  auto stub = is_funcref ? wasm::WasmCode::kWasmTableSetFuncRef
                         : wasm::WasmCode::kWasmTableSet;

  gasm_->CallRuntimeStub(stub, Operator::kNoThrow,
                         gasm_->IntPtrConstant(table_index), index, val);
}

std::pair<Node*, WasmGraphBuilder::BoundsCheckResult>
WasmGraphBuilder::CheckBoundsAndAlignment(int8_t access_size, Node* index,
                                          uint64_t offset,
                                          wasm::WasmCodePosition position,
                                          EnforceBoundsCheck enforce_check) {
  // Atomic operations need bounds checks until the backend can emit protected
  // loads.
  BoundsCheckResult bounds_check_result;
  std::tie(index, bounds_check_result) =
      BoundsCheckMem(access_size, index, offset, position, enforce_check);

  const uintptr_t align_mask = access_size - 1;

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
  // Don't emit an alignment check if the index is a constant.
  // TODO(wasm): a constant match is also done above in {BoundsCheckMem}.
  UintPtrMatcher match(index);
  if (match.HasResolvedValue()) {
    uintptr_t effective_offset = match.ResolvedValue() + capped_offset;
    if ((effective_offset & align_mask) != 0) {
      // statically known to be unaligned; trap.
      TrapIfEq32(wasm::kTrapUnalignedAccess, Int32Constant(0), 0, position);
    }
    return {index, bounds_check_result};
  }

  // Unlike regular memory accesses, atomic memory accesses should trap if
  // the effective offset is misaligned.
  // TODO(wasm): this addition is redundant with one inserted by {MemBuffer}.
  Node* effective_offset = gasm_->IntAdd(MemBuffer(capped_offset), index);

  Node* cond =
      gasm_->WordAnd(effective_offset, gasm_->IntPtrConstant(align_mask));
  TrapIfFalse(wasm::kTrapUnalignedAccess,
              gasm_->Word32Equal(cond, Int32Constant(0)), position);
  return {index, bounds_check_result};
}

// Insert code to bounds check a memory access if necessary. Return the
// bounds-checked index, which is guaranteed to have (the equivalent of)
// {uintptr_t} representation.
std::pair<Node*, WasmGraphBuilder::BoundsCheckResult>
WasmGraphBuilder::BoundsCheckMem(uint8_t access_size, Node* index,
                                 uint64_t offset,
                                 wasm::WasmCodePosition position,
                                 EnforceBoundsCheck enforce_check) {
  DCHECK_LE(1, access_size);

  // If the offset does not fit in a uintptr_t, this can never succeed on this
  // machine.
  if (offset > std::numeric_limits<uintptr_t>::max() ||
      !base::IsInBounds<uintptr_t>(offset, access_size,
                                   env_->max_memory_size)) {
    // The access will be out of bounds, even for the largest memory.
    TrapIfEq32(wasm::kTrapMemOutOfBounds, Int32Constant(0), 0, position);
    return {gasm_->UintPtrConstant(0), kOutOfBounds};
  }

  // Convert the index to uintptr.
  if (!env_->module->is_memory64) {
    index = gasm_->BuildChangeUint32ToUintPtr(index);
  } else if (kSystemPointerSize == kInt32Size) {
    // In memory64 mode on 32-bit systems, the upper 32 bits need to be zero to
    // succeed the bounds check.
    DCHECK_NE(wasm::kTrapHandler, env_->bounds_checks);
    if (env_->bounds_checks == wasm::kExplicitBoundsChecks) {
      Node* high_word = gasm_->TruncateInt64ToInt32(
          gasm_->Word64Shr(index, Int32Constant(32)));
      TrapIfTrue(wasm::kTrapMemOutOfBounds, high_word, position);
    }
    // Only use the low word for the following bounds check.
    index = gasm_->TruncateInt64ToInt32(index);
  }

  // If no bounds checks should be performed (for testing), just return the
  // converted index and assume it to be in-bounds.
  if (env_->bounds_checks == wasm::kNoBoundsChecks) return {index, kInBounds};

  // The accessed memory is [index + offset, index + end_offset].
  // Check that the last read byte (at {index + end_offset}) is in bounds.
  // 1) Check that {end_offset < mem_size}. This also ensures that we can safely
  //    compute {effective_size} as {mem_size - end_offset)}.
  //    {effective_size} is >= 1 if condition 1) holds.
  // 2) Check that {index + end_offset < mem_size} by
  //    - computing {effective_size} as {mem_size - end_offset} and
  //    - checking that {index < effective_size}.

  uintptr_t end_offset = offset + access_size - 1u;

  UintPtrMatcher match(index);
  if (match.HasResolvedValue() && end_offset <= env_->min_memory_size &&
      match.ResolvedValue() < env_->min_memory_size - end_offset) {
    // The input index is a constant and everything is statically within
    // bounds of the smallest possible memory.
    return {index, kInBounds};
  }

  if (env_->bounds_checks == wasm::kTrapHandler &&
      enforce_check == kCanOmitBoundsCheck) {
    return {index, kTrapHandler};
  }

  Node* mem_size = instance_cache_->mem_size;
  Node* end_offset_node = mcgraph_->UintPtrConstant(end_offset);
  if (end_offset > env_->min_memory_size) {
    // The end offset is larger than the smallest memory.
    // Dynamically check the end offset against the dynamic memory size.
    Node* cond = gasm_->UintLessThan(end_offset_node, mem_size);
    TrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);
  }

  // This produces a positive number since {end_offset <= min_size <= mem_size}.
  Node* effective_size = gasm_->IntSub(mem_size, end_offset_node);

  // Introduce the actual bounds check.
  Node* cond = gasm_->UintLessThan(index, effective_size);
  TrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);
  return {index, kDynamicallyChecked};
}

const Operator* WasmGraphBuilder::GetSafeLoadOperator(int offset,
                                                      wasm::ValueType type) {
  int alignment = offset % type.value_kind_size();
  MachineType mach_type = type.machine_type();
  if (COMPRESS_POINTERS_BOOL && mach_type.IsTagged()) {
    // We are loading tagged value from off-heap location, so we need to load
    // it as a full word otherwise we will not be able to decompress it.
    mach_type = MachineType::Pointer();
  }
  if (alignment == 0 || mcgraph()->machine()->UnalignedLoadSupported(
                            type.machine_representation())) {
    return mcgraph()->machine()->Load(mach_type);
  }
  return mcgraph()->machine()->UnalignedLoad(mach_type);
}

const Operator* WasmGraphBuilder::GetSafeStoreOperator(int offset,
                                                       wasm::ValueType type) {
  int alignment = offset % type.value_kind_size();
  MachineRepresentation rep = type.machine_representation();
  if (COMPRESS_POINTERS_BOOL && IsAnyTagged(rep)) {
    // We are storing tagged value to off-heap location, so we need to store
    // it as a full word otherwise we will not be able to decompress it.
    rep = MachineType::PointerRepresentation();
  }
  if (alignment == 0 || mcgraph()->machine()->UnalignedStoreSupported(rep)) {
    StoreRepresentation store_rep(rep, WriteBarrierKind::kNoWriteBarrier);
    return mcgraph()->machine()->Store(store_rep);
  }
  UnalignedStoreRepresentation store_rep(rep);
  return mcgraph()->machine()->UnalignedStore(store_rep);
}

void WasmGraphBuilder::TraceFunctionEntry(wasm::WasmCodePosition position) {
  Node* call = BuildCallToRuntime(Runtime::kWasmTraceEnter, nullptr, 0);
  SetSourcePosition(call, position);
}

void WasmGraphBuilder::TraceFunctionExit(base::Vector<Node*> vals,
                                         wasm::WasmCodePosition position) {
  Node* info = gasm_->IntPtrConstant(0);
  size_t num_returns = vals.size();
  if (num_returns == 1) {
    wasm::ValueType return_type = sig_->GetReturn(0);
    MachineRepresentation rep = return_type.machine_representation();
    int size = ElementSizeInBytes(rep);
    info = gasm_->StackSlot(size, size);

    gasm_->Store(StoreRepresentation(rep, kNoWriteBarrier), info,
                 Int32Constant(0), vals[0]);
  }

  Node* call = BuildCallToRuntime(Runtime::kWasmTraceExit, &info, 1);
  SetSourcePosition(call, position);
}

void WasmGraphBuilder::TraceMemoryOperation(bool is_store,
                                            MachineRepresentation rep,
                                            Node* index, uintptr_t offset,
                                            wasm::WasmCodePosition position) {
  int kAlign = 4;  // Ensure that the LSB is 0, such that this looks like a Smi.
  TNode<RawPtrT> info =
      gasm_->StackSlot(sizeof(wasm::MemoryTracingInfo), kAlign);

  Node* effective_offset = gasm_->IntAdd(gasm_->UintPtrConstant(offset), index);
  auto store = [&](int field_offset, MachineRepresentation rep, Node* data) {
    gasm_->Store(StoreRepresentation(rep, kNoWriteBarrier), info,
                 Int32Constant(field_offset), data);
  };
  // Store effective_offset, is_store, and mem_rep.
  store(offsetof(wasm::MemoryTracingInfo, offset),
        MachineType::PointerRepresentation(), effective_offset);
  store(offsetof(wasm::MemoryTracingInfo, is_store),
        MachineRepresentation::kWord8, Int32Constant(is_store ? 1 : 0));
  store(offsetof(wasm::MemoryTracingInfo, mem_rep),
        MachineRepresentation::kWord8, Int32Constant(static_cast<int>(rep)));

  Node* args[] = {info};
  Node* call =
      BuildCallToRuntime(Runtime::kWasmTraceMemory, args, arraysize(args));
  SetSourcePosition(call, position);
}

namespace {
LoadTransformation GetLoadTransformation(
    MachineType memtype, wasm::LoadTransformationKind transform) {
  switch (transform) {
    case wasm::LoadTransformationKind::kSplat: {
      if (memtype == MachineType::Int8()) {
        return LoadTransformation::kS128Load8Splat;
      } else if (memtype == MachineType::Int16()) {
        return LoadTransformation::kS128Load16Splat;
      } else if (memtype == MachineType::Int32()) {
        return LoadTransformation::kS128Load32Splat;
      } else if (memtype == MachineType::Int64()) {
        return LoadTransformation::kS128Load64Splat;
      }
      break;
    }
    case wasm::LoadTransformationKind::kExtend: {
      if (memtype == MachineType::Int8()) {
        return LoadTransformation::kS128Load8x8S;
      } else if (memtype == MachineType::Uint8()) {
        return LoadTransformation::kS128Load8x8U;
      } else if (memtype == MachineType::Int16()) {
        return LoadTransformation::kS128Load16x4S;
      } else if (memtype == MachineType::Uint16()) {
        return LoadTransformation::kS128Load16x4U;
      } else if (memtype == MachineType::Int32()) {
        return LoadTransformation::kS128Load32x2S;
      } else if (memtype == MachineType::Uint32()) {
        return LoadTransformation::kS128Load32x2U;
      }
      break;
    }
    case wasm::LoadTransformationKind::kZeroExtend: {
      if (memtype == MachineType::Int32()) {
        return LoadTransformation::kS128Load32Zero;
      } else if (memtype == MachineType::Int64()) {
        return LoadTransformation::kS128Load64Zero;
      }
      break;
    }
  }
  UNREACHABLE();
}

MemoryAccessKind GetMemoryAccessKind(
    MachineGraph* mcgraph, MachineRepresentation memrep,
    WasmGraphBuilder::BoundsCheckResult bounds_check_result) {
  if (bounds_check_result == WasmGraphBuilder::kTrapHandler) {
    // Protected instructions do not come in an 'unaligned' flavor, so the trap
    // handler can currently only be used on systems where all memory accesses
    // are allowed to be unaligned.
    DCHECK(memrep == MachineRepresentation::kWord8 ||
           mcgraph->machine()->UnalignedLoadSupported(memrep));
    return MemoryAccessKind::kProtected;
  }
  if (memrep != MachineRepresentation::kWord8 &&
      !mcgraph->machine()->UnalignedLoadSupported(memrep)) {
    return MemoryAccessKind::kUnaligned;
  }
  return MemoryAccessKind::kNormal;
}
}  // namespace

Node* WasmGraphBuilder::LoadLane(wasm::ValueType type, MachineType memtype,
                                 Node* value, Node* index, uint64_t offset,
                                 uint32_t alignment, uint8_t laneidx,
                                 wasm::WasmCodePosition position) {
  has_simd_ = true;
  Node* load;
  uint8_t access_size = memtype.MemSize();
  BoundsCheckResult bounds_check_result;
  std::tie(index, bounds_check_result) =
      BoundsCheckMem(access_size, index, offset, position, kCanOmitBoundsCheck);

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
  MemoryAccessKind load_kind = GetMemoryAccessKind(
      mcgraph_, memtype.representation(), bounds_check_result);

  load = SetEffect(graph()->NewNode(
      mcgraph()->machine()->LoadLane(load_kind, memtype, laneidx),
      MemBuffer(capped_offset), index, value, effect(), control()));

  if (load_kind == MemoryAccessKind::kProtected) {
    SetSourcePosition(load, position);
  }
  if (v8_flags.trace_wasm_memory) {
    TraceMemoryOperation(false, memtype.representation(), index, capped_offset,
                         position);
  }

  return load;
}

Node* WasmGraphBuilder::LoadTransform(wasm::ValueType type, MachineType memtype,
                                      wasm::LoadTransformationKind transform,
                                      Node* index, uint64_t offset,
                                      uint32_t alignment,
                                      wasm::WasmCodePosition position) {
  has_simd_ = true;

  Node* load;
  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);

  // Wasm semantics throw on OOB. Introduce explicit bounds check and
  // conditioning when not using the trap handler.

  // Load extends always load 8 bytes.
  uint8_t access_size = transform == wasm::LoadTransformationKind::kExtend
                            ? 8
                            : memtype.MemSize();
  BoundsCheckResult bounds_check_result;
  std::tie(index, bounds_check_result) =
      BoundsCheckMem(access_size, index, offset, position, kCanOmitBoundsCheck);

  LoadTransformation transformation = GetLoadTransformation(memtype, transform);
  MemoryAccessKind load_kind = GetMemoryAccessKind(
      mcgraph_, memtype.representation(), bounds_check_result);

  load = SetEffect(graph()->NewNode(
      mcgraph()->machine()->LoadTransform(load_kind, transformation),
      MemBuffer(capped_offset), index, effect(), control()));

  if (load_kind == MemoryAccessKind::kProtected) {
    SetSourcePosition(load, position);
  }

  if (v8_flags.trace_wasm_memory) {
    TraceMemoryOperation(false, memtype.representation(), index, capped_offset,
                         position);
  }
  return load;
}

Node* WasmGraphBuilder::LoadMem(wasm::ValueType type, MachineType memtype,
                                Node* index, uint64_t offset,
                                uint32_t alignment,
                                wasm::WasmCodePosition position) {
  Node* load;

  if (memtype.representation() == MachineRepresentation::kSimd128) {
    has_simd_ = true;
  }

  // Wasm semantics throw on OOB. Introduce explicit bounds check and
  // conditioning when not using the trap handler.
  BoundsCheckResult bounds_check_result;
  std::tie(index, bounds_check_result) = BoundsCheckMem(
      memtype.MemSize(), index, offset, position, kCanOmitBoundsCheck);

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);

  switch (GetMemoryAccessKind(mcgraph_, memtype.representation(),
                              bounds_check_result)) {
    case MemoryAccessKind::kUnaligned:
      load = gasm_->LoadUnaligned(memtype, MemBuffer(capped_offset), index);
      break;
    case MemoryAccessKind::kProtected:
      load = gasm_->ProtectedLoad(memtype, MemBuffer(capped_offset), index);
      SetSourcePosition(load, position);
      break;
    case MemoryAccessKind::kNormal:
      load = gasm_->Load(memtype, MemBuffer(capped_offset), index);
      break;
  }

#if defined(V8_TARGET_BIG_ENDIAN)
  load = BuildChangeEndiannessLoad(load, memtype, type);
#endif

  if (type == wasm::kWasmI64 &&
      ElementSizeInBytes(memtype.representation()) < 8) {
    // TODO(titzer): TF zeroes the upper bits of 64-bit loads for subword sizes.
    load = memtype.IsSigned()
               ? gasm_->ChangeInt32ToInt64(load)     // sign extend
               : gasm_->ChangeUint32ToUint64(load);  // zero extend
  }

  if (v8_flags.trace_wasm_memory) {
    TraceMemoryOperation(false, memtype.representation(), index, capped_offset,
                         position);
  }

  return load;
}

void WasmGraphBuilder::StoreLane(MachineRepresentation mem_rep, Node* index,
                                 uint64_t offset, uint32_t alignment, Node* val,
                                 uint8_t laneidx,
                                 wasm::WasmCodePosition position,
                                 wasm::ValueType type) {
  has_simd_ = true;
  BoundsCheckResult bounds_check_result;
  std::tie(index, bounds_check_result) =
      BoundsCheckMem(i::ElementSizeInBytes(mem_rep), index, offset, position,
                     kCanOmitBoundsCheck);

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
  MemoryAccessKind load_kind =
      GetMemoryAccessKind(mcgraph_, mem_rep, bounds_check_result);

  Node* store = SetEffect(graph()->NewNode(
      mcgraph()->machine()->StoreLane(load_kind, mem_rep, laneidx),
      MemBuffer(capped_offset), index, val, effect(), control()));

  if (load_kind == MemoryAccessKind::kProtected) {
    SetSourcePosition(store, position);
  }
  if (v8_flags.trace_wasm_memory) {
    TraceMemoryOperation(true, mem_rep, index, capped_offset, position);
  }
}

void WasmGraphBuilder::StoreMem(MachineRepresentation mem_rep, Node* index,
                                uint64_t offset, uint32_t alignment, Node* val,
                                wasm::WasmCodePosition position,
                                wasm::ValueType type) {
  if (mem_rep == MachineRepresentation::kSimd128) {
    has_simd_ = true;
  }

  BoundsCheckResult bounds_check_result;
  std::tie(index, bounds_check_result) =
      BoundsCheckMem(i::ElementSizeInBytes(mem_rep), index, offset, position,
                     kCanOmitBoundsCheck);

#if defined(V8_TARGET_BIG_ENDIAN)
  val = BuildChangeEndiannessStore(val, mem_rep, type);
#endif

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);

  switch (GetMemoryAccessKind(mcgraph_, mem_rep, bounds_check_result)) {
    case MemoryAccessKind::kUnaligned:
      gasm_->StoreUnaligned(UnalignedStoreRepresentation{mem_rep},
                            MemBuffer(capped_offset), index, val);
      break;
    case MemoryAccessKind::kProtected:
      SetSourcePosition(
          gasm_->ProtectedStore(mem_rep, MemBuffer(capped_offset), index, val),
          position);
      break;
    case MemoryAccessKind::kNormal:
      gasm_->Store(StoreRepresentation{mem_rep, kNoWriteBarrier},
                   MemBuffer(capped_offset), index, val);
      break;
  }

  if (v8_flags.trace_wasm_memory) {
    TraceMemoryOperation(true, mem_rep, index, capped_offset, position);
  }
}

Node* WasmGraphBuilder::BuildAsmjsLoadMem(MachineType type, Node* index) {
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_start = instance_cache_->mem_start;
  Node* mem_size = instance_cache_->mem_size;
  DCHECK_NOT_NULL(mem_start);
  DCHECK_NOT_NULL(mem_size);

  // Asm.js semantics are defined in terms of typed arrays, hence OOB
  // reads return {undefined} coerced to the result type (0 for integers, NaN
  // for float and double).
  // Note that we check against the memory size ignoring the size of the
  // stored value, which is conservative if misaligned. Technically, asm.js
  // should never have misaligned accesses.
  index = gasm_->BuildChangeUint32ToUintPtr(index);
  Diamond bounds_check(graph(), mcgraph()->common(),
                       gasm_->UintLessThan(index, mem_size), BranchHint::kTrue);
  bounds_check.Chain(control());

  Node* load = graph()->NewNode(mcgraph()->machine()->Load(type), mem_start,
                                index, effect(), bounds_check.if_true);
  SetEffectControl(bounds_check.EffectPhi(load, effect()), bounds_check.merge);

  Node* oob_value;
  switch (type.representation()) {
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      oob_value = Int32Constant(0);
      break;
    case MachineRepresentation::kWord64:
      oob_value = Int64Constant(0);
      break;
    case MachineRepresentation::kFloat32:
      oob_value = Float32Constant(std::numeric_limits<float>::quiet_NaN());
      break;
    case MachineRepresentation::kFloat64:
      oob_value = Float64Constant(std::numeric_limits<double>::quiet_NaN());
      break;
    default:
      UNREACHABLE();
  }

  return bounds_check.Phi(type.representation(), load, oob_value);
}

Node* WasmGraphBuilder::BuildAsmjsStoreMem(MachineType type, Node* index,
                                           Node* val) {
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_start = instance_cache_->mem_start;
  Node* mem_size = instance_cache_->mem_size;
  DCHECK_NOT_NULL(mem_start);
  DCHECK_NOT_NULL(mem_size);

  // Asm.js semantics are to ignore OOB writes.
  // Note that we check against the memory size ignoring the size of the
  // stored value, which is conservative if misaligned. Technically, asm.js
  // should never have misaligned accesses.
  Diamond bounds_check(graph(), mcgraph()->common(),
                       gasm_->Uint32LessThan(index, mem_size),
                       BranchHint::kTrue);
  bounds_check.Chain(control());

  index = gasm_->BuildChangeUint32ToUintPtr(index);
  const Operator* store_op = mcgraph()->machine()->Store(StoreRepresentation(
      type.representation(), WriteBarrierKind::kNoWriteBarrier));
  Node* store = graph()->NewNode(store_op, mem_start, index, val, effect(),
                                 bounds_check.if_true);
  SetEffectControl(bounds_check.EffectPhi(store, effect()), bounds_check.merge);
  return val;
}

Node* WasmGraphBuilder::BuildF64x2Ceil(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f64x2_ceil();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64x2Floor(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f64x2_floor();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64x2Trunc(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f64x2_trunc();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64x2NearestInt(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f64x2_nearest_int();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32x4Ceil(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f32x4_ceil();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32x4Floor(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f32x4_floor();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32x4Trunc(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f32x4_trunc();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32x4NearestInt(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f32x4_nearest_int();
  return BuildCFuncInstruction(ref, type, input);
}

void WasmGraphBuilder::PrintDebugName(Node* node) {
  PrintF("#%d:%s", node->id(), node->op()->mnemonic());
}

Graph* WasmGraphBuilder::graph() { return mcgraph()->graph(); }

Zone* WasmGraphBuilder::graph_zone() { return graph()->zone(); }

namespace {
Signature<MachineRepresentation>* CreateMachineSignature(
    Zone* zone, const wasm::FunctionSig* sig,
    WasmGraphBuilder::CallOrigin origin) {
  Signature<MachineRepresentation>::Builder builder(zone, sig->return_count(),
                                                    sig->parameter_count());
  for (auto ret : sig->returns()) {
    if (origin == WasmGraphBuilder::kCalledFromJS) {
      builder.AddReturn(MachineRepresentation::kTagged);
    } else {
      builder.AddReturn(ret.machine_representation());
    }
  }

  for (auto param : sig->parameters()) {
    if (origin == WasmGraphBuilder::kCalledFromJS) {
      // Parameters coming from JavaScript are always tagged values. Especially
      // when the signature says that it's an I64 value, then a BigInt object is
      // provided by JavaScript, and not two 32-bit parameters.
      builder.AddParam(MachineRepresentation::kTagged);
    } else {
      builder.AddParam(param.machine_representation());
    }
  }
  return builder.Build();
}

}  // namespace

void WasmGraphBuilder::AddInt64LoweringReplacement(
    CallDescriptor* original, CallDescriptor* replacement) {
  if (!lowering_special_case_) {
    lowering_special_case_ = std::make_unique<Int64LoweringSpecialCase>();
  }
  lowering_special_case_->replacements.insert({original, replacement});
}

CallDescriptor* WasmGraphBuilder::GetI32AtomicWaitCallDescriptor() {
  if (i32_atomic_wait_descriptor_) return i32_atomic_wait_descriptor_;

  i32_atomic_wait_descriptor_ = GetBuiltinCallDescriptor(
      Builtin::kWasmI32AtomicWait64, zone_, StubCallMode::kCallWasmRuntimeStub);

  AddInt64LoweringReplacement(
      i32_atomic_wait_descriptor_,
      GetBuiltinCallDescriptor(Builtin::kWasmI32AtomicWait32, zone_,
                               StubCallMode::kCallWasmRuntimeStub));

  return i32_atomic_wait_descriptor_;
}

CallDescriptor* WasmGraphBuilder::GetI64AtomicWaitCallDescriptor() {
  if (i64_atomic_wait_descriptor_) return i64_atomic_wait_descriptor_;

  i64_atomic_wait_descriptor_ = GetBuiltinCallDescriptor(
      Builtin::kWasmI64AtomicWait64, zone_, StubCallMode::kCallWasmRuntimeStub);

  AddInt64LoweringReplacement(
      i64_atomic_wait_descriptor_,
      GetBuiltinCallDescriptor(Builtin::kWasmI64AtomicWait32, zone_,
                               StubCallMode::kCallWasmRuntimeStub));

  return i64_atomic_wait_descriptor_;
}

void WasmGraphBuilder::LowerInt64(Signature<MachineRepresentation>* sig) {
  if (mcgraph()->machine()->Is64()) return;
  Int64Lowering r(mcgraph()->graph(), mcgraph()->machine(), mcgraph()->common(),
                  gasm_->simplified(), mcgraph()->zone(),
                  env_ != nullptr ? env_->module : nullptr, sig,
                  std::move(lowering_special_case_));
  r.LowerGraph();
}

void WasmGraphBuilder::LowerInt64(CallOrigin origin) {
  LowerInt64(CreateMachineSignature(mcgraph()->zone(), sig_, origin));
}

void WasmGraphBuilder::SetSourcePosition(Node* node,
                                         wasm::WasmCodePosition position) {
  DCHECK_NE(position, wasm::kNoCodePosition);
  if (source_position_table_) {
    source_position_table_->SetSourcePosition(node, SourcePosition(position));
  }
}

Node* WasmGraphBuilder::S128Zero() {
  has_simd_ = true;
  return graph()->NewNode(mcgraph()->machine()->S128Zero());
}

Node* WasmGraphBuilder::SimdOp(wasm::WasmOpcode opcode, Node* const* inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprF64x2Splat:
      return graph()->NewNode(mcgraph()->machine()->F64x2Splat(), inputs[0]);
    case wasm::kExprF64x2Abs:
      return graph()->NewNode(mcgraph()->machine()->F64x2Abs(), inputs[0]);
    case wasm::kExprF64x2Neg:
      return graph()->NewNode(mcgraph()->machine()->F64x2Neg(), inputs[0]);
    case wasm::kExprF64x2Sqrt:
      return graph()->NewNode(mcgraph()->machine()->F64x2Sqrt(), inputs[0]);
    case wasm::kExprF64x2Add:
      return graph()->NewNode(mcgraph()->machine()->F64x2Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Sub:
      return graph()->NewNode(mcgraph()->machine()->F64x2Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Mul:
      return graph()->NewNode(mcgraph()->machine()->F64x2Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Div:
      return graph()->NewNode(mcgraph()->machine()->F64x2Div(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Min:
      return graph()->NewNode(mcgraph()->machine()->F64x2Min(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Max:
      return graph()->NewNode(mcgraph()->machine()->F64x2Max(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Eq:
      return graph()->NewNode(mcgraph()->machine()->F64x2Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Ne:
      return graph()->NewNode(mcgraph()->machine()->F64x2Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Lt:
      return graph()->NewNode(mcgraph()->machine()->F64x2Lt(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Le:
      return graph()->NewNode(mcgraph()->machine()->F64x2Le(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Gt:
      return graph()->NewNode(mcgraph()->machine()->F64x2Lt(), inputs[1],
                              inputs[0]);
    case wasm::kExprF64x2Ge:
      return graph()->NewNode(mcgraph()->machine()->F64x2Le(), inputs[1],
                              inputs[0]);
    case wasm::kExprF64x2Qfma:
      return graph()->NewNode(mcgraph()->machine()->F64x2Qfma(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprF64x2Qfms:
      return graph()->NewNode(mcgraph()->machine()->F64x2Qfms(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprF64x2Pmin:
      return graph()->NewNode(mcgraph()->machine()->F64x2Pmin(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Pmax:
      return graph()->NewNode(mcgraph()->machine()->F64x2Pmax(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Ceil:
      // Architecture support for F64x2Ceil and Float64RoundUp is the same.
      if (!mcgraph()->machine()->Float64RoundUp().IsSupported())
        return BuildF64x2Ceil(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F64x2Ceil(), inputs[0]);
    case wasm::kExprF64x2Floor:
      // Architecture support for F64x2Floor and Float64RoundDown is the same.
      if (!mcgraph()->machine()->Float64RoundDown().IsSupported())
        return BuildF64x2Floor(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F64x2Floor(), inputs[0]);
    case wasm::kExprF64x2Trunc:
      // Architecture support for F64x2Trunc and Float64RoundTruncate is the
      // same.
      if (!mcgraph()->machine()->Float64RoundTruncate().IsSupported())
        return BuildF64x2Trunc(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F64x2Trunc(), inputs[0]);
    case wasm::kExprF64x2NearestInt:
      // Architecture support for F64x2NearestInt and Float64RoundTiesEven is
      // the same.
      if (!mcgraph()->machine()->Float64RoundTiesEven().IsSupported())
        return BuildF64x2NearestInt(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F64x2NearestInt(),
                              inputs[0]);
    case wasm::kExprF64x2ConvertLowI32x4S:
      return graph()->NewNode(mcgraph()->machine()->F64x2ConvertLowI32x4S(),
                              inputs[0]);
    case wasm::kExprF64x2ConvertLowI32x4U:
      return graph()->NewNode(mcgraph()->machine()->F64x2ConvertLowI32x4U(),
                              inputs[0]);
    case wasm::kExprF64x2PromoteLowF32x4:
      return graph()->NewNode(mcgraph()->machine()->F64x2PromoteLowF32x4(),
                              inputs[0]);
    case wasm::kExprF32x4Splat:
      return graph()->NewNode(mcgraph()->machine()->F32x4Splat(), inputs[0]);
    case wasm::kExprF32x4SConvertI32x4:
      return graph()->NewNode(mcgraph()->machine()->F32x4SConvertI32x4(),
                              inputs[0]);
    case wasm::kExprF32x4UConvertI32x4:
      return graph()->NewNode(mcgraph()->machine()->F32x4UConvertI32x4(),
                              inputs[0]);
    case wasm::kExprF32x4Abs:
      return graph()->NewNode(mcgraph()->machine()->F32x4Abs(), inputs[0]);
    case wasm::kExprF32x4Neg:
      return graph()->NewNode(mcgraph()->machine()->F32x4Neg(), inputs[0]);
    case wasm::kExprF32x4Sqrt:
      return graph()->NewNode(mcgraph()->machine()->F32x4Sqrt(), inputs[0]);
    case wasm::kExprF32x4Add:
      return graph()->NewNode(mcgraph()->machine()->F32x4Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Sub:
      return graph()->NewNode(mcgraph()->machine()->F32x4Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Mul:
      return graph()->NewNode(mcgraph()->machine()->F32x4Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Div:
      return graph()->NewNode(mcgraph()->machine()->F32x4Div(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Min:
      return graph()->NewNode(mcgraph()->machine()->F32x4Min(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Max:
      return graph()->NewNode(mcgraph()->machine()->F32x4Max(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Eq:
      return graph()->NewNode(mcgraph()->machine()->F32x4Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Ne:
      return graph()->NewNode(mcgraph()->machine()->F32x4Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Lt:
      return graph()->NewNode(mcgraph()->machine()->F32x4Lt(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Le:
      return graph()->NewNode(mcgraph()->machine()->F32x4Le(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Gt:
      return graph()->NewNode(mcgraph()->machine()->F32x4Lt(), inputs[1],
                              inputs[0]);
    case wasm::kExprF32x4Ge:
      return graph()->NewNode(mcgraph()->machine()->F32x4Le(), inputs[1],
                              inputs[0]);
    case wasm::kExprF32x4Qfma:
      return graph()->NewNode(mcgraph()->machine()->F32x4Qfma(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprF32x4Qfms:
      return graph()->NewNode(mcgraph()->machine()->F32x4Qfms(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprF32x4Pmin:
      return graph()->NewNode(mcgraph()->machine()->F32x4Pmin(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Pmax:
      return graph()->NewNode(mcgraph()->machine()->F32x4Pmax(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Ceil:
      // Architecture support for F32x4Ceil and Float32RoundUp is the same.
      if (!mcgraph()->machine()->Float32RoundUp().IsSupported())
        return BuildF32x4Ceil(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F32x4Ceil(), inputs[0]);
    case wasm::kExprF32x4Floor:
      // Architecture support for F32x4Floor and Float32RoundDown is the same.
      if (!mcgraph()->machine()->Float32RoundDown().IsSupported())
        return BuildF32x4Floor(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F32x4Floor(), inputs[0]);
    case wasm::kExprF32x4Trunc:
      // Architecture support for F32x4Trunc and Float32RoundTruncate is the
      // same.
      if (!mcgraph()->machine()->Float32RoundTruncate().IsSupported())
        return BuildF32x4Trunc(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F32x4Trunc(), inputs[0]);
    case wasm::kExprF32x4NearestInt:
      // Architecture support for F32x4NearestInt and Float32RoundTiesEven is
      // the same.
      if (!mcgraph()->machine()->Float32RoundTiesEven().IsSupported())
        return BuildF32x4NearestInt(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F32x4NearestInt(),
                              inputs[0]);
    case wasm::kExprF32x4DemoteF64x2Zero:
      return graph()->NewNode(mcgraph()->machine()->F32x4DemoteF64x2Zero(),
                              inputs[0]);
    case wasm::kExprI64x2Splat:
      return graph()->NewNode(mcgraph()->machine()->I64x2Splat(), inputs[0]);
    case wasm::kExprI64x2Abs:
      return graph()->NewNode(mcgraph()->machine()->I64x2Abs(), inputs[0]);
    case wasm::kExprI64x2Neg:
      return graph()->NewNode(mcgraph()->machine()->I64x2Neg(), inputs[0]);
    case wasm::kExprI64x2SConvertI32x4Low:
      return graph()->NewNode(mcgraph()->machine()->I64x2SConvertI32x4Low(),
                              inputs[0]);
    case wasm::kExprI64x2SConvertI32x4High:
      return graph()->NewNode(mcgraph()->machine()->I64x2SConvertI32x4High(),
                              inputs[0]);
    case wasm::kExprI64x2UConvertI32x4Low:
      return graph()->NewNode(mcgraph()->machine()->I64x2UConvertI32x4Low(),
                              inputs[0]);
    case wasm::kExprI64x2UConvertI32x4High:
      return graph()->NewNode(mcgraph()->machine()->I64x2UConvertI32x4High(),
                              inputs[0]);
    case wasm::kExprI64x2BitMask:
      return graph()->NewNode(mcgraph()->machine()->I64x2BitMask(), inputs[0]);
    case wasm::kExprI64x2Shl:
      return graph()->NewNode(mcgraph()->machine()->I64x2Shl(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2ShrS:
      return graph()->NewNode(mcgraph()->machine()->I64x2ShrS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Add:
      return graph()->NewNode(mcgraph()->machine()->I64x2Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Sub:
      return graph()->NewNode(mcgraph()->machine()->I64x2Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Mul:
      return graph()->NewNode(mcgraph()->machine()->I64x2Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Eq:
      return graph()->NewNode(mcgraph()->machine()->I64x2Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Ne:
      return graph()->NewNode(mcgraph()->machine()->I64x2Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2LtS:
      return graph()->NewNode(mcgraph()->machine()->I64x2GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI64x2LeS:
      return graph()->NewNode(mcgraph()->machine()->I64x2GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI64x2GtS:
      return graph()->NewNode(mcgraph()->machine()->I64x2GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2GeS:
      return graph()->NewNode(mcgraph()->machine()->I64x2GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2ShrU:
      return graph()->NewNode(mcgraph()->machine()->I64x2ShrU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2ExtMulLowI32x4S:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtMulLowI32x4S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI64x2ExtMulHighI32x4S:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtMulHighI32x4S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI64x2ExtMulLowI32x4U:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtMulLowI32x4U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI64x2ExtMulHighI32x4U:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtMulHighI32x4U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4Splat:
      return graph()->NewNode(mcgraph()->machine()->I32x4Splat(), inputs[0]);
    case wasm::kExprI32x4SConvertF32x4:
      return graph()->NewNode(mcgraph()->machine()->I32x4SConvertF32x4(),
                              inputs[0]);
    case wasm::kExprI32x4UConvertF32x4:
      return graph()->NewNode(mcgraph()->machine()->I32x4UConvertF32x4(),
                              inputs[0]);
    case wasm::kExprI32x4SConvertI16x8Low:
      return graph()->NewNode(mcgraph()->machine()->I32x4SConvertI16x8Low(),
                              inputs[0]);
    case wasm::kExprI32x4SConvertI16x8High:
      return graph()->NewNode(mcgraph()->machine()->I32x4SConvertI16x8High(),
                              inputs[0]);
    case wasm::kExprI32x4Neg:
      return graph()->NewNode(mcgraph()->machine()->I32x4Neg(), inputs[0]);
    case wasm::kExprI32x4Shl:
      return graph()->NewNode(mcgraph()->machine()->I32x4Shl(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4ShrS:
      return graph()->NewNode(mcgraph()->machine()->I32x4ShrS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Add:
      return graph()->NewNode(mcgraph()->machine()->I32x4Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Sub:
      return graph()->NewNode(mcgraph()->machine()->I32x4Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Mul:
      return graph()->NewNode(mcgraph()->machine()->I32x4Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MinS:
      return graph()->NewNode(mcgraph()->machine()->I32x4MinS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MaxS:
      return graph()->NewNode(mcgraph()->machine()->I32x4MaxS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Eq:
      return graph()->NewNode(mcgraph()->machine()->I32x4Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Ne:
      return graph()->NewNode(mcgraph()->machine()->I32x4Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4LtS:
      return graph()->NewNode(mcgraph()->machine()->I32x4GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4LeS:
      return graph()->NewNode(mcgraph()->machine()->I32x4GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4GtS:
      return graph()->NewNode(mcgraph()->machine()->I32x4GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4GeS:
      return graph()->NewNode(mcgraph()->machine()->I32x4GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4UConvertI16x8Low:
      return graph()->NewNode(mcgraph()->machine()->I32x4UConvertI16x8Low(),
                              inputs[0]);
    case wasm::kExprI32x4UConvertI16x8High:
      return graph()->NewNode(mcgraph()->machine()->I32x4UConvertI16x8High(),
                              inputs[0]);
    case wasm::kExprI32x4ShrU:
      return graph()->NewNode(mcgraph()->machine()->I32x4ShrU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MinU:
      return graph()->NewNode(mcgraph()->machine()->I32x4MinU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MaxU:
      return graph()->NewNode(mcgraph()->machine()->I32x4MaxU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4LtU:
      return graph()->NewNode(mcgraph()->machine()->I32x4GtU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4LeU:
      return graph()->NewNode(mcgraph()->machine()->I32x4GeU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4GtU:
      return graph()->NewNode(mcgraph()->machine()->I32x4GtU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4GeU:
      return graph()->NewNode(mcgraph()->machine()->I32x4GeU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Abs:
      return graph()->NewNode(mcgraph()->machine()->I32x4Abs(), inputs[0]);
    case wasm::kExprI32x4BitMask:
      return graph()->NewNode(mcgraph()->machine()->I32x4BitMask(), inputs[0]);
    case wasm::kExprI32x4DotI16x8S:
      return graph()->NewNode(mcgraph()->machine()->I32x4DotI16x8S(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4ExtMulLowI16x8S:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtMulLowI16x8S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtMulHighI16x8S:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtMulHighI16x8S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtMulLowI16x8U:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtMulLowI16x8U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtMulHighI16x8U:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtMulHighI16x8U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtAddPairwiseI16x8S:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtAddPairwiseI16x8S(),
                              inputs[0]);
    case wasm::kExprI32x4ExtAddPairwiseI16x8U:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtAddPairwiseI16x8U(),
                              inputs[0]);
    case wasm::kExprI32x4TruncSatF64x2SZero:
      return graph()->NewNode(mcgraph()->machine()->I32x4TruncSatF64x2SZero(),
                              inputs[0]);
    case wasm::kExprI32x4TruncSatF64x2UZero:
      return graph()->NewNode(mcgraph()->machine()->I32x4TruncSatF64x2UZero(),
                              inputs[0]);
    case wasm::kExprI16x8Splat:
      return graph()->NewNode(mcgraph()->machine()->I16x8Splat(), inputs[0]);
    case wasm::kExprI16x8SConvertI8x16Low:
      return graph()->NewNode(mcgraph()->machine()->I16x8SConvertI8x16Low(),
                              inputs[0]);
    case wasm::kExprI16x8SConvertI8x16High:
      return graph()->NewNode(mcgraph()->machine()->I16x8SConvertI8x16High(),
                              inputs[0]);
    case wasm::kExprI16x8Shl:
      return graph()->NewNode(mcgraph()->machine()->I16x8Shl(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8ShrS:
      return graph()->NewNode(mcgraph()->machine()->I16x8ShrS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Neg:
      return graph()->NewNode(mcgraph()->machine()->I16x8Neg(), inputs[0]);
    case wasm::kExprI16x8SConvertI32x4:
      return graph()->NewNode(mcgraph()->machine()->I16x8SConvertI32x4(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8Add:
      return graph()->NewNode(mcgraph()->machine()->I16x8Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8AddSatS:
      return graph()->NewNode(mcgraph()->machine()->I16x8AddSatS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Sub:
      return graph()->NewNode(mcgraph()->machine()->I16x8Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8SubSatS:
      return graph()->NewNode(mcgraph()->machine()->I16x8SubSatS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Mul:
      return graph()->NewNode(mcgraph()->machine()->I16x8Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MinS:
      return graph()->NewNode(mcgraph()->machine()->I16x8MinS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MaxS:
      return graph()->NewNode(mcgraph()->machine()->I16x8MaxS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Eq:
      return graph()->NewNode(mcgraph()->machine()->I16x8Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Ne:
      return graph()->NewNode(mcgraph()->machine()->I16x8Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8LtS:
      return graph()->NewNode(mcgraph()->machine()->I16x8GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8LeS:
      return graph()->NewNode(mcgraph()->machine()->I16x8GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8GtS:
      return graph()->NewNode(mcgraph()->machine()->I16x8GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8GeS:
      return graph()->NewNode(mcgraph()->machine()->I16x8GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8UConvertI8x16Low:
      return graph()->NewNode(mcgraph()->machine()->I16x8UConvertI8x16Low(),
                              inputs[0]);
    case wasm::kExprI16x8UConvertI8x16High:
      return graph()->NewNode(mcgraph()->machine()->I16x8UConvertI8x16High(),
                              inputs[0]);
    case wasm::kExprI16x8UConvertI32x4:
      return graph()->NewNode(mcgraph()->machine()->I16x8UConvertI32x4(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ShrU:
      return graph()->NewNode(mcgraph()->machine()->I16x8ShrU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8AddSatU:
      return graph()->NewNode(mcgraph()->machine()->I16x8AddSatU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8SubSatU:
      return graph()->NewNode(mcgraph()->machine()->I16x8SubSatU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MinU:
      return graph()->NewNode(mcgraph()->machine()->I16x8MinU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MaxU:
      return graph()->NewNode(mcgraph()->machine()->I16x8MaxU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8LtU:
      return graph()->NewNode(mcgraph()->machine()->I16x8GtU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8LeU:
      return graph()->NewNode(mcgraph()->machine()->I16x8GeU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8GtU:
      return graph()->NewNode(mcgraph()->machine()->I16x8GtU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8GeU:
      return graph()->NewNode(mcgraph()->machine()->I16x8GeU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8RoundingAverageU:
      return graph()->NewNode(mcgraph()->machine()->I16x8RoundingAverageU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8Q15MulRSatS:
      return graph()->NewNode(mcgraph()->machine()->I16x8Q15MulRSatS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8RelaxedQ15MulRS:
      return graph()->NewNode(mcgraph()->machine()->I16x8RelaxedQ15MulRS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8DotI8x16I7x16S:
      return graph()->NewNode(mcgraph()->machine()->I16x8DotI8x16I7x16S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4DotI8x16I7x16AddS:
      return graph()->NewNode(mcgraph()->machine()->I32x4DotI8x16I7x16AddS(),
                              inputs[0], inputs[1], inputs[2]);
    case wasm::kExprI16x8Abs:
      return graph()->NewNode(mcgraph()->machine()->I16x8Abs(), inputs[0]);
    case wasm::kExprI16x8BitMask:
      return graph()->NewNode(mcgraph()->machine()->I16x8BitMask(), inputs[0]);
    case wasm::kExprI16x8ExtMulLowI8x16S:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtMulLowI8x16S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtMulHighI8x16S:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtMulHighI8x16S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtMulLowI8x16U:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtMulLowI8x16U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtMulHighI8x16U:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtMulHighI8x16U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtAddPairwiseI8x16S:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtAddPairwiseI8x16S(),
                              inputs[0]);
    case wasm::kExprI16x8ExtAddPairwiseI8x16U:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtAddPairwiseI8x16U(),
                              inputs[0]);
    case wasm::kExprI8x16Splat:
      return graph()->NewNode(mcgraph()->machine()->I8x16Splat(), inputs[0]);
    case wasm::kExprI8x16Neg:
      return graph()->NewNode(mcgraph()->machine()->I8x16Neg(), inputs[0]);
    case wasm::kExprI8x16Shl:
      return graph()->NewNode(mcgraph()->machine()->I8x16Shl(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16ShrS:
      return graph()->NewNode(mcgraph()->machine()->I8x16ShrS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16SConvertI16x8:
      return graph()->NewNode(mcgraph()->machine()->I8x16SConvertI16x8(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Add:
      return graph()->NewNode(mcgraph()->machine()->I8x16Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16AddSatS:
      return graph()->NewNode(mcgraph()->machine()->I8x16AddSatS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Sub:
      return graph()->NewNode(mcgraph()->machine()->I8x16Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16SubSatS:
      return graph()->NewNode(mcgraph()->machine()->I8x16SubSatS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MinS:
      return graph()->NewNode(mcgraph()->machine()->I8x16MinS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MaxS:
      return graph()->NewNode(mcgraph()->machine()->I8x16MaxS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Eq:
      return graph()->NewNode(mcgraph()->machine()->I8x16Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Ne:
      return graph()->NewNode(mcgraph()->machine()->I8x16Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16LtS:
      return graph()->NewNode(mcgraph()->machine()->I8x16GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16LeS:
      return graph()->NewNode(mcgraph()->machine()->I8x16GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16GtS:
      return graph()->NewNode(mcgraph()->machine()->I8x16GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16GeS:
      return graph()->NewNode(mcgraph()->machine()->I8x16GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16ShrU:
      return graph()->NewNode(mcgraph()->machine()->I8x16ShrU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16UConvertI16x8:
      return graph()->NewNode(mcgraph()->machine()->I8x16UConvertI16x8(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16AddSatU:
      return graph()->NewNode(mcgraph()->machine()->I8x16AddSatU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16SubSatU:
      return graph()->NewNode(mcgraph()->machine()->I8x16SubSatU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MinU:
      return graph()->NewNode(mcgraph()->machine()->I8x16MinU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MaxU:
      return graph()->NewNode(mcgraph()->machine()->I8x16MaxU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16LtU:
      return graph()->NewNode(mcgraph()->machine()->I8x16GtU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16LeU:
      return graph()->NewNode(mcgraph()->machine()->I8x16GeU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16GtU:
      return graph()->NewNode(mcgraph()->machine()->I8x16GtU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16GeU:
      return graph()->NewNode(mcgraph()->machine()->I8x16GeU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16RoundingAverageU:
      return graph()->NewNode(mcgraph()->machine()->I8x16RoundingAverageU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Popcnt:
      return graph()->NewNode(mcgraph()->machine()->I8x16Popcnt(), inputs[0]);
    case wasm::kExprI8x16Abs:
      return graph()->NewNode(mcgraph()->machine()->I8x16Abs(), inputs[0]);
    case wasm::kExprI8x16BitMask:
      return graph()->NewNode(mcgraph()->machine()->I8x16BitMask(), inputs[0]);
    case wasm::kExprS128And:
      return graph()->NewNode(mcgraph()->machine()->S128And(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128Or:
      return graph()->NewNode(mcgraph()->machine()->S128Or(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128Xor:
      return graph()->NewNode(mcgraph()->machine()->S128Xor(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128Not:
      return graph()->NewNode(mcgraph()->machine()->S128Not(), inputs[0]);
    case wasm::kExprS128Select:
      return graph()->NewNode(mcgraph()->machine()->S128Select(), inputs[2],
                              inputs[0], inputs[1]);
    case wasm::kExprS128AndNot:
      return graph()->NewNode(mcgraph()->machine()->S128AndNot(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2AllTrue:
      return graph()->NewNode(mcgraph()->machine()->I64x2AllTrue(), inputs[0]);
    case wasm::kExprI32x4AllTrue:
      return graph()->NewNode(mcgraph()->machine()->I32x4AllTrue(), inputs[0]);
    case wasm::kExprI16x8AllTrue:
      return graph()->NewNode(mcgraph()->machine()->I16x8AllTrue(), inputs[0]);
    case wasm::kExprV128AnyTrue:
      return graph()->NewNode(mcgraph()->machine()->V128AnyTrue(), inputs[0]);
    case wasm::kExprI8x16AllTrue:
      return graph()->NewNode(mcgraph()->machine()->I8x16AllTrue(), inputs[0]);
    case wasm::kExprI8x16Swizzle:
      return graph()->NewNode(mcgraph()->machine()->I8x16Swizzle(false),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16RelaxedSwizzle:
      return graph()->NewNode(mcgraph()->machine()->I8x16Swizzle(true),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16RelaxedLaneSelect:
      // Relaxed lane select puts the mask as first input (same as S128Select).
      return graph()->NewNode(mcgraph()->machine()->I8x16RelaxedLaneSelect(),
                              inputs[2], inputs[0], inputs[1]);
    case wasm::kExprI16x8RelaxedLaneSelect:
      return graph()->NewNode(mcgraph()->machine()->I16x8RelaxedLaneSelect(),
                              inputs[2], inputs[0], inputs[1]);
    case wasm::kExprI32x4RelaxedLaneSelect:
      return graph()->NewNode(mcgraph()->machine()->I32x4RelaxedLaneSelect(),
                              inputs[2], inputs[0], inputs[1]);
    case wasm::kExprI64x2RelaxedLaneSelect:
      return graph()->NewNode(mcgraph()->machine()->I64x2RelaxedLaneSelect(),
                              inputs[2], inputs[0], inputs[1]);
    case wasm::kExprF32x4RelaxedMin:
      return graph()->NewNode(mcgraph()->machine()->F32x4RelaxedMin(),
                              inputs[0], inputs[1]);
    case wasm::kExprF32x4RelaxedMax:
      return graph()->NewNode(mcgraph()->machine()->F32x4RelaxedMax(),
                              inputs[0], inputs[1]);
    case wasm::kExprF64x2RelaxedMin:
      return graph()->NewNode(mcgraph()->machine()->F64x2RelaxedMin(),
                              inputs[0], inputs[1]);
    case wasm::kExprF64x2RelaxedMax:
      return graph()->NewNode(mcgraph()->machine()->F64x2RelaxedMax(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4RelaxedTruncF64x2SZero:
      return graph()->NewNode(
          mcgraph()->machine()->I32x4RelaxedTruncF64x2SZero(), inputs[0]);
    case wasm::kExprI32x4RelaxedTruncF64x2UZero:
      return graph()->NewNode(
          mcgraph()->machine()->I32x4RelaxedTruncF64x2UZero(), inputs[0]);
    case wasm::kExprI32x4RelaxedTruncF32x4S:
      return graph()->NewNode(mcgraph()->machine()->I32x4RelaxedTruncF32x4S(),
                              inputs[0]);
    case wasm::kExprI32x4RelaxedTruncF32x4U:
      return graph()->NewNode(mcgraph()->machine()->I32x4RelaxedTruncF32x4U(),
                              inputs[0]);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

Node* WasmGraphBuilder::SimdLaneOp(wasm::WasmOpcode opcode, uint8_t lane,
                                   Node* const* inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprF64x2ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->F64x2ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprF64x2ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->F64x2ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprF32x4ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->F32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprF32x4ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->F32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI64x2ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI64x2ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I64x2ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI32x4ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtractLaneS:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtractLaneS(lane),
                              inputs[0]);
    case wasm::kExprI16x8ExtractLaneU:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtractLaneU(lane),
                              inputs[0]);
    case wasm::kExprI16x8ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I16x8ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16ExtractLaneS:
      return graph()->NewNode(mcgraph()->machine()->I8x16ExtractLaneS(lane),
                              inputs[0]);
    case wasm::kExprI8x16ExtractLaneU:
      return graph()->NewNode(mcgraph()->machine()->I8x16ExtractLaneU(lane),
                              inputs[0]);
    case wasm::kExprI8x16ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I8x16ReplaceLane(lane),
                              inputs[0], inputs[1]);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

Node* WasmGraphBuilder::Simd8x16ShuffleOp(const uint8_t shuffle[16],
                                          Node* const* inputs) {
  has_simd_ = true;
  return graph()->NewNode(mcgraph()->machine()->I8x16Shuffle(shuffle),
                          inputs[0], inputs[1]);
}

Node* WasmGraphBuilder::AtomicOp(wasm::WasmOpcode opcode, Node* const* inputs,
                                 uint32_t alignment, uint64_t offset,
                                 wasm::WasmCodePosition position) {
  struct AtomicOpInfo {
    enum Type : int8_t {
      kNoInput = 0,
      kOneInput = 1,
      kTwoInputs = 2,
      kSpecial
    };

    using OperatorByAtomicOpParams =
        const Operator* (MachineOperatorBuilder::*)(AtomicOpParameters);
    using OperatorByAtomicLoadRep =
        const Operator* (MachineOperatorBuilder::*)(AtomicLoadParameters);
    using OperatorByAtomicStoreRep =
        const Operator* (MachineOperatorBuilder::*)(AtomicStoreParameters);

    const Type type;
    const MachineType machine_type;
    const OperatorByAtomicOpParams operator_by_type = nullptr;
    const OperatorByAtomicLoadRep operator_by_atomic_load_params = nullptr;
    const OperatorByAtomicStoreRep operator_by_atomic_store_rep = nullptr;
    const wasm::ValueType wasm_type;

    constexpr AtomicOpInfo(Type t, MachineType m, OperatorByAtomicOpParams o)
        : type(t), machine_type(m), operator_by_type(o) {}
    constexpr AtomicOpInfo(Type t, MachineType m, OperatorByAtomicLoadRep o,
                           wasm::ValueType v)
        : type(t),
          machine_type(m),
          operator_by_atomic_load_params(o),
          wasm_type(v) {}
    constexpr AtomicOpInfo(Type t, MachineType m, OperatorByAtomicStoreRep o,
                           wasm::ValueType v)
        : type(t),
          machine_type(m),
          operator_by_atomic_store_rep(o),
          wasm_type(v) {}

    // Constexpr, hence just a table lookup in most compilers.
    static constexpr AtomicOpInfo Get(wasm::WasmOpcode opcode) {
      switch (opcode) {
#define CASE(Name, Type, MachType, Op) \
  case wasm::kExpr##Name:              \
    return {Type, MachineType::MachType(), &MachineOperatorBuilder::Op};
#define CASE_LOAD_STORE(Name, Type, MachType, Op, WasmType)             \
  case wasm::kExpr##Name:                                               \
    return {Type, MachineType::MachType(), &MachineOperatorBuilder::Op, \
            WasmType};

        // Binops.
        CASE(I32AtomicAdd, kOneInput, Uint32, Word32AtomicAdd)
        CASE(I64AtomicAdd, kOneInput, Uint64, Word64AtomicAdd)
        CASE(I32AtomicAdd8U, kOneInput, Uint8, Word32AtomicAdd)
        CASE(I32AtomicAdd16U, kOneInput, Uint16, Word32AtomicAdd)
        CASE(I64AtomicAdd8U, kOneInput, Uint8, Word64AtomicAdd)
        CASE(I64AtomicAdd16U, kOneInput, Uint16, Word64AtomicAdd)
        CASE(I64AtomicAdd32U, kOneInput, Uint32, Word64AtomicAdd)
        CASE(I32AtomicSub, kOneInput, Uint32, Word32AtomicSub)
        CASE(I64AtomicSub, kOneInput, Uint64, Word64AtomicSub)
        CASE(I32AtomicSub8U, kOneInput, Uint8, Word32AtomicSub)
        CASE(I32AtomicSub16U, kOneInput, Uint16, Word32AtomicSub)
        CASE(I64AtomicSub8U, kOneInput, Uint8, Word64AtomicSub)
        CASE(I64AtomicSub16U, kOneInput, Uint16, Word64AtomicSub)
        CASE(I64AtomicSub32U, kOneInput, Uint32, Word64AtomicSub)
        CASE(I32AtomicAnd, kOneInput, Uint32, Word32AtomicAnd)
        CASE(I64AtomicAnd, kOneInput, Uint64, Word64AtomicAnd)
        CASE(I32AtomicAnd8U, kOneInput, Uint8, Word32AtomicAnd)
        CASE(I32AtomicAnd16U, kOneInput, Uint16, Word32AtomicAnd)
        CASE(I64AtomicAnd8U, kOneInput, Uint8, Word64AtomicAnd)
        CASE(I64AtomicAnd16U, kOneInput, Uint16, Word64AtomicAnd)
        CASE(I64AtomicAnd32U, kOneInput, Uint32, Word64AtomicAnd)
        CASE(I32AtomicOr, kOneInput, Uint32, Word32AtomicOr)
        CASE(I64AtomicOr, kOneInput, Uint64, Word64AtomicOr)
        CASE(I32AtomicOr8U, kOneInput, Uint8, Word32AtomicOr)
        CASE(I32AtomicOr16U, kOneInput, Uint16, Word32AtomicOr)
        CASE(I64AtomicOr8U, kOneInput, Uint8, Word64AtomicOr)
        CASE(I64AtomicOr16U, kOneInput, Uint16, Word64AtomicOr)
        CASE(I64AtomicOr32U, kOneInput, Uint32, Word64AtomicOr)
        CASE(I32AtomicXor, kOneInput, Uint32, Word32AtomicXor)
        CASE(I64AtomicXor, kOneInput, Uint64, Word64AtomicXor)
        CASE(I32AtomicXor8U, kOneInput, Uint8, Word32AtomicXor)
        CASE(I32AtomicXor16U, kOneInput, Uint16, Word32AtomicXor)
        CASE(I64AtomicXor8U, kOneInput, Uint8, Word64AtomicXor)
        CASE(I64AtomicXor16U, kOneInput, Uint16, Word64AtomicXor)
        CASE(I64AtomicXor32U, kOneInput, Uint32, Word64AtomicXor)
        CASE(I32AtomicExchange, kOneInput, Uint32, Word32AtomicExchange)
        CASE(I64AtomicExchange, kOneInput, Uint64, Word64AtomicExchange)
        CASE(I32AtomicExchange8U, kOneInput, Uint8, Word32AtomicExchange)
        CASE(I32AtomicExchange16U, kOneInput, Uint16, Word32AtomicExchange)
        CASE(I64AtomicExchange8U, kOneInput, Uint8, Word64AtomicExchange)
        CASE(I64AtomicExchange16U, kOneInput, Uint16, Word64AtomicExchange)
        CASE(I64AtomicExchange32U, kOneInput, Uint32, Word64AtomicExchange)

        // Compare-exchange.
        CASE(I32AtomicCompareExchange, kTwoInputs, Uint32,
             Word32AtomicCompareExchange)
        CASE(I64AtomicCompareExchange, kTwoInputs, Uint64,
             Word64AtomicCompareExchange)
        CASE(I32AtomicCompareExchange8U, kTwoInputs, Uint8,
             Word32AtomicCompareExchange)
        CASE(I32AtomicCompareExchange16U, kTwoInputs, Uint16,
             Word32AtomicCompareExchange)
        CASE(I64AtomicCompareExchange8U, kTwoInputs, Uint8,
             Word64AtomicCompareExchange)
        CASE(I64AtomicCompareExchange16U, kTwoInputs, Uint16,
             Word64AtomicCompareExchange)
        CASE(I64AtomicCompareExchange32U, kTwoInputs, Uint32,
             Word64AtomicCompareExchange)

        // Load.
        CASE_LOAD_STORE(I32AtomicLoad, kNoInput, Uint32, Word32AtomicLoad,
                        wasm::kWasmI32)
        CASE_LOAD_STORE(I64AtomicLoad, kNoInput, Uint64, Word64AtomicLoad,
                        wasm::kWasmI64)
        CASE_LOAD_STORE(I32AtomicLoad8U, kNoInput, Uint8, Word32AtomicLoad,
                        wasm::kWasmI32)
        CASE_LOAD_STORE(I32AtomicLoad16U, kNoInput, Uint16, Word32AtomicLoad,
                        wasm::kWasmI32)
        CASE_LOAD_STORE(I64AtomicLoad8U, kNoInput, Uint8, Word64AtomicLoad,
                        wasm::kWasmI64)
        CASE_LOAD_STORE(I64AtomicLoad16U, kNoInput, Uint16, Word64AtomicLoad,
                        wasm::kWasmI64)
        CASE_LOAD_STORE(I64AtomicLoad32U, kNoInput, Uint32, Word64AtomicLoad,
                        wasm::kWasmI64)

        // Store.
        CASE_LOAD_STORE(I32AtomicStore, kOneInput, Uint32, Word32AtomicStore,
                        wasm::kWasmI32)
        CASE_LOAD_STORE(I64AtomicStore, kOneInput, Uint64, Word64AtomicStore,
                        wasm::kWasmI64)
        CASE_LOAD_STORE(I32AtomicStore8U, kOneInput, Uint8, Word32AtomicStore,
                        wasm::kWasmI32)
        CASE_LOAD_STORE(I32AtomicStore16U, kOneInput, Uint16, Word32AtomicStore,
                        wasm::kWasmI32)
        CASE_LOAD_STORE(I64AtomicStore8U, kOneInput, Uint8, Word64AtomicStore,
                        wasm::kWasmI64)
        CASE_LOAD_STORE(I64AtomicStore16U, kOneInput, Uint16, Word64AtomicStore,
                        wasm::kWasmI64)
        CASE_LOAD_STORE(I64AtomicStore32U, kOneInput, Uint32, Word64AtomicStore,
                        wasm::kWasmI64)

#undef CASE
#undef CASE_LOAD_STORE

        case wasm::kExprAtomicNotify:
          return {kSpecial, MachineType::Int32(),
                  OperatorByAtomicOpParams{nullptr}};
        case wasm::kExprI32AtomicWait:
          return {kSpecial, MachineType::Int32(),
                  OperatorByAtomicOpParams{nullptr}};
        case wasm::kExprI64AtomicWait:
          return {kSpecial, MachineType::Int64(),
                  OperatorByAtomicOpParams{nullptr}};
        default:
          UNREACHABLE();
      }
    }
  };

  AtomicOpInfo info = AtomicOpInfo::Get(opcode);

  const auto enforce_bounds_check = info.type != AtomicOpInfo::kSpecial
    ? EnforceBoundsCheck::kCanOmitBoundsCheck
    : EnforceBoundsCheck::kNeedsBoundsCheck;
  Node* index;
  BoundsCheckResult bounds_check_result;
  std::tie(index, bounds_check_result) =
      CheckBoundsAndAlignment(info.machine_type.MemSize(), inputs[0], offset,
                              position, enforce_bounds_check);
  // MemoryAccessKind::kUnalligned is impossible due to explicit aligment check.
  MemoryAccessKind access_kind =
      bounds_check_result == WasmGraphBuilder::kTrapHandler
          ? MemoryAccessKind::kProtected
          : MemoryAccessKind::kNormal;

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
  if (info.type != AtomicOpInfo::kSpecial) {
    const Operator* op;
    if (info.operator_by_type) {
      op = (mcgraph()->machine()->*info.operator_by_type)(
          AtomicOpParameters(info.machine_type,
                             access_kind));
    } else if (info.operator_by_atomic_load_params) {
      op = (mcgraph()->machine()->*info.operator_by_atomic_load_params)(
          AtomicLoadParameters(info.machine_type, AtomicMemoryOrder::kSeqCst,
                               access_kind));
    } else {
      op = (mcgraph()->machine()->*info.operator_by_atomic_store_rep)(
          AtomicStoreParameters(info.machine_type.representation(),
                                WriteBarrierKind::kNoWriteBarrier,
                                AtomicMemoryOrder::kSeqCst,
                                access_kind));
    }

    Node* input_nodes[6] = {MemBuffer(capped_offset), index};
    int num_actual_inputs = info.type;
    std::copy_n(inputs + 1, num_actual_inputs, input_nodes + 2);
    input_nodes[num_actual_inputs + 2] = effect();
    input_nodes[num_actual_inputs + 3] = control();

#ifdef V8_TARGET_BIG_ENDIAN
    // Reverse the value bytes before storing.
    if (info.operator_by_atomic_store_rep) {
      input_nodes[num_actual_inputs + 1] = BuildChangeEndiannessStore(
          input_nodes[num_actual_inputs + 1],
          info.machine_type.representation(), info.wasm_type);
    }
#endif

    Node* result = gasm_->AddNode(
        graph()->NewNode(op, num_actual_inputs + 4, input_nodes));

    if (access_kind == MemoryAccessKind::kProtected) {
      SetSourcePosition(result, position);
    }

#ifdef V8_TARGET_BIG_ENDIAN
    // Reverse the value bytes after load.
    if (info.operator_by_atomic_load_params) {
      result =
          BuildChangeEndiannessLoad(result, info.machine_type, info.wasm_type);
    }
#endif

    return result;
  }

  // After we've bounds-checked, compute the effective offset.
  Node* effective_offset =
      gasm_->IntAdd(gasm_->UintPtrConstant(capped_offset), index);

  switch (opcode) {
    case wasm::kExprAtomicNotify:
      return gasm_->CallRuntimeStub(wasm::WasmCode::kWasmAtomicNotify,
                                    Operator::kNoThrow, effective_offset,
                                    inputs[1]);

    case wasm::kExprI32AtomicWait: {
      auto* call_descriptor = GetI32AtomicWaitCallDescriptor();

      intptr_t target = mcgraph()->machine()->Is64()
                            ? wasm::WasmCode::kWasmI32AtomicWait64
                            : wasm::WasmCode::kWasmI32AtomicWait32;
      Node* call_target = mcgraph()->RelocatableIntPtrConstant(
          target, RelocInfo::WASM_STUB_CALL);

      return gasm_->Call(call_descriptor, call_target, effective_offset,
                         inputs[1], inputs[2]);
    }

    case wasm::kExprI64AtomicWait: {
      auto* call_descriptor = GetI64AtomicWaitCallDescriptor();

      intptr_t target = mcgraph()->machine()->Is64()
                            ? wasm::WasmCode::kWasmI64AtomicWait64
                            : wasm::WasmCode::kWasmI64AtomicWait32;
      Node* call_target = mcgraph()->RelocatableIntPtrConstant(
          target, RelocInfo::WASM_STUB_CALL);

      return gasm_->Call(call_descriptor, call_target, effective_offset,
                         inputs[1], inputs[2]);
    }

    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

void WasmGraphBuilder::AtomicFence() {
  SetEffect(graph()->NewNode(
      mcgraph()->machine()->MemoryBarrier(AtomicMemoryOrder::kSeqCst), effect(),
      control()));
}

void WasmGraphBuilder::MemoryInit(uint32_t data_segment_index, Node* dst,
                                  Node* src, Node* size,
                                  wasm::WasmCodePosition position) {
  // The data segment index must be in bounds since it is required by
  // validation.
  DCHECK_LT(data_segment_index, env_->module->num_declared_data_segments);

  Node* function =
      gasm_->ExternalConstant(ExternalReference::wasm_memory_init());

  MemTypeToUintPtrOrOOBTrap({&dst}, position);

  Node* stack_slot = StoreArgsInStackSlot(
      {{MachineType::PointerRepresentation(), GetInstance()},
       {MachineType::PointerRepresentation(), dst},
       {MachineRepresentation::kWord32, src},
       {MachineRepresentation::kWord32,
        gasm_->Uint32Constant(data_segment_index)},
       {MachineRepresentation::kWord32, size}});

  auto sig = FixedSizeSignature<MachineType>::Returns(MachineType::Int32())
                 .Params(MachineType::Pointer());
  Node* call = BuildCCall(&sig, function, stack_slot);
  // TODO(manoskouk): Also throw kDataSegmentOutOfBounds.
  TrapIfFalse(wasm::kTrapMemOutOfBounds, call, position);
}

void WasmGraphBuilder::DataDrop(uint32_t data_segment_index,
                                wasm::WasmCodePosition position) {
  DCHECK_LT(data_segment_index, env_->module->num_declared_data_segments);

  Node* seg_size_array =
      LOAD_INSTANCE_FIELD(DataSegmentSizes, MachineType::TaggedPointer());
  static_assert(wasm::kV8MaxWasmDataSegments <= kMaxUInt32 >> 2);
  auto access = ObjectAccess(MachineType::Int32(), kNoWriteBarrier);
  gasm_->StoreToObject(
      access, seg_size_array,
      wasm::ObjectAccess::ElementOffsetInTaggedFixedUInt32Array(
          data_segment_index),
      Int32Constant(0));
}

Node* WasmGraphBuilder::StoreArgsInStackSlot(
    std::initializer_list<std::pair<MachineRepresentation, Node*>> args) {
  int slot_size = 0;
  for (auto arg : args) {
    slot_size += ElementSizeInBytes(arg.first);
  }
  DCHECK_LT(0, slot_size);
  Node* stack_slot =
      graph()->NewNode(mcgraph()->machine()->StackSlot(slot_size));

  int offset = 0;
  for (auto arg : args) {
    MachineRepresentation type = arg.first;
    Node* value = arg.second;
    gasm_->StoreUnaligned(type, stack_slot, Int32Constant(offset), value);
    offset += ElementSizeInBytes(type);
  }
  return stack_slot;
}

void WasmGraphBuilder::MemTypeToUintPtrOrOOBTrap(
    std::initializer_list<Node**> nodes, wasm::WasmCodePosition position) {
  if (!env_->module->is_memory64) {
    for (Node** node : nodes) {
      *node = gasm_->BuildChangeUint32ToUintPtr(*node);
    }
    return;
  }
  if (kSystemPointerSize == kInt64Size) return;  // memory64 on 64-bit
  Node* any_high_word = nullptr;
  for (Node** node : nodes) {
    Node* high_word =
        gasm_->TruncateInt64ToInt32(gasm_->Word64Shr(*node, Int32Constant(32)));
    any_high_word =
        any_high_word ? gasm_->Word32Or(any_high_word, high_word) : high_word;
    // Only keep the low word as uintptr_t.
    *node = gasm_->TruncateInt64ToInt32(*node);
  }
  TrapIfTrue(wasm::kTrapMemOutOfBounds, any_high_word, position);
}

void WasmGraphBuilder::MemoryCopy(Node* dst, Node* src, Node* size,
                                  wasm::WasmCodePosition position) {
  Node* function =
      gasm_->ExternalConstant(ExternalReference::wasm_memory_copy());

  MemTypeToUintPtrOrOOBTrap({&dst, &src, &size}, position);

  Node* stack_slot = StoreArgsInStackSlot(
      {{MachineType::PointerRepresentation(), GetInstance()},
       {MachineType::PointerRepresentation(), dst},
       {MachineType::PointerRepresentation(), src},
       {MachineType::PointerRepresentation(), size}});

  auto sig = FixedSizeSignature<MachineType>::Returns(MachineType::Int32())
                 .Params(MachineType::Pointer());
  Node* call = BuildCCall(&sig, function, stack_slot);
  TrapIfFalse(wasm::kTrapMemOutOfBounds, call, position);
}

void WasmGraphBuilder::MemoryFill(Node* dst, Node* value, Node* size,
                                  wasm::WasmCodePosition position) {
  Node* function =
      gasm_->ExternalConstant(ExternalReference::wasm_memory_fill());

  MemTypeToUintPtrOrOOBTrap({&dst, &size}, position);

  Node* stack_slot = StoreArgsInStackSlot(
      {{MachineType::PointerRepresentation(), GetInstance()},
       {MachineType::PointerRepresentation(), dst},
       {MachineRepresentation::kWord32, value},
       {MachineType::PointerRepresentation(), size}});

  auto sig = FixedSizeSignature<MachineType>::Returns(MachineType::Int32())
                 .Params(MachineType::Pointer());
  Node* call = BuildCCall(&sig, function, stack_slot);
  TrapIfFalse(wasm::kTrapMemOutOfBounds, call, position);
}

void WasmGraphBuilder::TableInit(uint32_t table_index,
                                 uint32_t elem_segment_index, Node* dst,
                                 Node* src, Node* size,
                                 wasm::WasmCodePosition position) {
  gasm_->CallRuntimeStub(wasm::WasmCode::kWasmTableInit, Operator::kNoThrow,
                         dst, src, size, gasm_->NumberConstant(table_index),
                         gasm_->NumberConstant(elem_segment_index));
}

void WasmGraphBuilder::ElemDrop(uint32_t elem_segment_index,
                                wasm::WasmCodePosition position) {
  // The elem segment index must be in bounds since it is required by
  // validation.
  DCHECK_LT(elem_segment_index, env_->module->elem_segments.size());

  Node* dropped_elem_segments =
      LOAD_INSTANCE_FIELD(DroppedElemSegments, MachineType::TaggedPointer());
  auto store_rep =
      StoreRepresentation(MachineRepresentation::kWord8, kNoWriteBarrier);
  gasm_->Store(store_rep, dropped_elem_segments,
               wasm::ObjectAccess::ElementOffsetInTaggedFixedUInt8Array(
                   elem_segment_index),
               Int32Constant(1));
}

void WasmGraphBuilder::TableCopy(uint32_t table_dst_index,
                                 uint32_t table_src_index, Node* dst, Node* src,
                                 Node* size, wasm::WasmCodePosition position) {
  gasm_->CallRuntimeStub(wasm::WasmCode::kWasmTableCopy, Operator::kNoThrow,
                         dst, src, size, gasm_->NumberConstant(table_dst_index),
                         gasm_->NumberConstant(table_src_index));
}

Node* WasmGraphBuilder::TableGrow(uint32_t table_index, Node* value,
                                  Node* delta) {
  return gasm_->BuildChangeSmiToInt32(gasm_->CallRuntimeStub(
      wasm::WasmCode::kWasmTableGrow, Operator::kNoThrow,
      graph()->NewNode(mcgraph()->common()->NumberConstant(table_index)), delta,
      value));
}

Node* WasmGraphBuilder::TableSize(uint32_t table_index) {
  Node* tables = LOAD_INSTANCE_FIELD(Tables, MachineType::TaggedPointer());
  Node* table = gasm_->LoadFixedArrayElementAny(tables, table_index);

  int length_field_size = WasmTableObject::kCurrentLengthOffsetEnd -
                          WasmTableObject::kCurrentLengthOffset + 1;
  Node* length_smi = gasm_->LoadFromObject(
      assert_size(length_field_size, MachineType::TaggedSigned()), table,
      wasm::ObjectAccess::ToTagged(WasmTableObject::kCurrentLengthOffset));

  return gasm_->BuildChangeSmiToInt32(length_smi);
}

void WasmGraphBuilder::TableFill(uint32_t table_index, Node* start, Node* value,
                                 Node* count) {
  gasm_->CallRuntimeStub(
      wasm::WasmCode::kWasmTableFill, Operator::kNoThrow,
      graph()->NewNode(mcgraph()->common()->NumberConstant(table_index)), start,
      count, value);
}

Node* WasmGraphBuilder::DefaultValue(wasm::ValueType type) {
  DCHECK(type.is_defaultable());
  switch (type.kind()) {
    case wasm::kI8:
    case wasm::kI16:
    case wasm::kI32:
      return Int32Constant(0);
    case wasm::kI64:
      return Int64Constant(0);
    case wasm::kF32:
      return Float32Constant(0);
    case wasm::kF64:
      return Float64Constant(0);
    case wasm::kS128:
      return S128Zero();
    case wasm::kRefNull:
      return RefNull();
    case wasm::kRtt:
    case wasm::kVoid:
    case wasm::kBottom:
    case wasm::kRef:
      UNREACHABLE();
  }
}

Node* WasmGraphBuilder::StructNew(uint32_t struct_index,
                                  const wasm::StructType* type, Node* rtt,
                                  base::Vector<Node*> fields) {
  int size = WasmStruct::Size(type);
  Node* s = gasm_->Allocate(size);
  gasm_->StoreMap(s, rtt);
  gasm_->InitializeImmutableInObject(
      ObjectAccess(MachineType::TaggedPointer(), kNoWriteBarrier), s,
      wasm::ObjectAccess::ToTagged(JSReceiver::kPropertiesOrHashOffset),
      LOAD_ROOT(EmptyFixedArray, empty_fixed_array));
  for (uint32_t i = 0; i < type->field_count(); i++) {
    gasm_->StoreStructField(s, type, i, fields[i]);
  }
  // If this assert fails then initialization of padding field might be
  // necessary.
  static_assert(Heap::kMinObjectSizeInTaggedWords == 2 &&
                    WasmStruct::kHeaderSize == 2 * kTaggedSize,
                "empty struct might require initialization of padding field");
  return s;
}

Node* WasmGraphBuilder::ArrayNew(uint32_t array_index,
                                 const wasm::ArrayType* type, Node* length,
                                 Node* initial_value, Node* rtt,
                                 wasm::WasmCodePosition position) {
  TrapIfFalse(wasm::kTrapArrayTooLarge,
              gasm_->Uint32LessThanOrEqual(
                  length, gasm_->Uint32Constant(WasmArray::MaxLength(type))),
              position);
  wasm::ValueType element_type = type->element_type();

  // RoundUp(length * value_size, kObjectAlignment) =
  //   RoundDown(length * value_size + kObjectAlignment - 1,
  //             kObjectAlignment);
  Node* padded_length = gasm_->Word32And(
      gasm_->Int32Add(
          gasm_->Int32Mul(length,
                          Int32Constant(element_type.value_kind_size())),
          Int32Constant(kObjectAlignment - 1)),
      Int32Constant(-kObjectAlignment));
  Node* a = gasm_->Allocate(
      gasm_->Int32Add(padded_length, Int32Constant(WasmArray::kHeaderSize)));

  // Initialize the array header.
  gasm_->StoreMap(a, rtt);
  gasm_->InitializeImmutableInObject(
      ObjectAccess(MachineType::TaggedPointer(), kNoWriteBarrier), a,
      wasm::ObjectAccess::ToTagged(JSReceiver::kPropertiesOrHashOffset),
      LOAD_ROOT(EmptyFixedArray, empty_fixed_array));
  gasm_->InitializeImmutableInObject(
      ObjectAccess(MachineType::Uint32(), kNoWriteBarrier), a,
      wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset), length);

  // Initialize the array elements. Use memset for large arrays initialized with
  // zeroes (through an external function), and a loop for all other ones. The
  // size limit was determined by running array-copy-benchmark.js.
  auto done = gasm_->MakeLabel();
  // TODO(manoskouk): If the loop is ever removed here, we have to update
  // ArrayNew() in graph-builder-interface.cc to not mark the current
  // loop as non-innermost.
  auto loop = gasm_->MakeLoopLabel(MachineRepresentation::kWord32);
  Node* start_offset = gasm_->IntPtrConstant(
      wasm::ObjectAccess::ToTagged(WasmArray::kHeaderSize));
  Node* element_size = gasm_->IntPtrConstant(element_type.value_kind_size());
  Node* end_offset =
      gasm_->IntAdd(start_offset, gasm_->IntMul(element_size, length));

  if (initial_value == nullptr && element_type.is_numeric()) {
    constexpr uint32_t kArrayNewMinimumSizeForMemSet = 10;
    gasm_->GotoIf(gasm_->Uint32LessThan(
                      length, Int32Constant(kArrayNewMinimumSizeForMemSet)),
                  &loop, BranchHint::kNone, start_offset);
    Node* function = gasm_->ExternalConstant(
        ExternalReference::wasm_array_fill_with_zeroes());
    MachineType arg_types[]{MachineType::TaggedPointer(), MachineType::Uint32(),
                            MachineType::Uint32()};
    MachineSignature sig(0, 3, arg_types);
    BuildCCall(&sig, function, a, length,
               Int32Constant(element_type.value_kind_size()));
    gasm_->Goto(&done);
  } else {
    gasm_->Goto(&loop, start_offset);
  }
  gasm_->Bind(&loop);
  auto object_access = ObjectAccessForGCStores(element_type);
  if (initial_value == nullptr) {
    initial_value = DefaultValue(element_type);
    object_access.write_barrier_kind = kNoWriteBarrier;
  }
  {
    Node* offset = loop.PhiAt(0);
    Node* check = gasm_->UintLessThan(offset, end_offset);
    gasm_->GotoIfNot(check, &done);
    if (type->mutability()) {
      gasm_->StoreToObject(object_access, a, offset, initial_value);
    } else {
      gasm_->InitializeImmutableInObject(object_access, a, offset,
                                         initial_value);
    }
    offset = gasm_->IntAdd(offset, element_size);
    gasm_->Goto(&loop, offset);
  }
  gasm_->Bind(&done);
  return a;
}

Node* WasmGraphBuilder::ArrayNewFixed(const wasm::ArrayType* type, Node* rtt,
                                      base::Vector<Node*> elements) {
  wasm::ValueType element_type = type->element_type();
  Node* array = gasm_->Allocate(RoundUp(element_type.value_kind_size() *
                                            static_cast<int>(elements.size()),
                                        kObjectAlignment) +
                                WasmArray::kHeaderSize);
  gasm_->StoreMap(array, rtt);
  gasm_->InitializeImmutableInObject(
      ObjectAccess(MachineType::TaggedPointer(), kNoWriteBarrier), array,
      wasm::ObjectAccess::ToTagged(JSReceiver::kPropertiesOrHashOffset),
      LOAD_ROOT(EmptyFixedArray, empty_fixed_array));
  gasm_->InitializeImmutableInObject(
      ObjectAccess(MachineType::Uint32(), kNoWriteBarrier), array,
      wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset),
      Int32Constant(static_cast<int>(elements.size())));
  for (int i = 0; i < static_cast<int>(elements.size()); i++) {
    Node* offset =
        gasm_->WasmArrayElementOffset(Int32Constant(i), element_type);
    if (type->mutability()) {
      gasm_->StoreToObject(ObjectAccessForGCStores(element_type), array, offset,
                           elements[i]);
    } else {
      gasm_->InitializeImmutableInObject(ObjectAccessForGCStores(element_type),
                                         array, offset, elements[i]);
    }
  }
  return array;
}

Node* WasmGraphBuilder::ArrayNewSegment(const wasm::ArrayType* type,
                                        uint32_t data_segment, Node* offset,
                                        Node* length, Node* rtt,
                                        wasm::WasmCodePosition position) {
  return gasm_->CallBuiltin(
      Builtin::kWasmArrayNewSegment, Operator::kNoDeopt | Operator::kNoThrow,
      gasm_->Uint32Constant(data_segment), offset, length, rtt);
}

Node* WasmGraphBuilder::RttCanon(uint32_t type_index) {
  return graph()->NewNode(gasm_->simplified()->RttCanon(type_index));
}

WasmGraphBuilder::Callbacks WasmGraphBuilder::TestCallbacks(
    GraphAssemblerLabel<1>* label) {
  return {// succeed_if
          [=](Node* condition, BranchHint hint) -> void {
            gasm_->GotoIf(condition, label, hint, Int32Constant(1));
          },
          // fail_if
          [=](Node* condition, BranchHint hint) -> void {
            gasm_->GotoIf(condition, label, hint, Int32Constant(0));
          },
          // fail_if_not
          [=](Node* condition, BranchHint hint) -> void {
            gasm_->GotoIfNot(condition, label, hint, Int32Constant(0));
          }};
}

WasmGraphBuilder::Callbacks WasmGraphBuilder::CastCallbacks(
    GraphAssemblerLabel<0>* label, wasm::WasmCodePosition position) {
  return {// succeed_if
          [=](Node* condition, BranchHint hint) -> void {
            gasm_->GotoIf(condition, label, hint);
          },
          // fail_if
          [=](Node* condition, BranchHint hint) -> void {
            TrapIfTrue(wasm::kTrapIllegalCast, condition, position);
          },
          // fail_if_not
          [=](Node* condition, BranchHint hint) -> void {
            TrapIfFalse(wasm::kTrapIllegalCast, condition, position);
          }};
}

WasmGraphBuilder::Callbacks WasmGraphBuilder::BranchCallbacks(
    SmallNodeVector& no_match_controls, SmallNodeVector& no_match_effects,
    SmallNodeVector& match_controls, SmallNodeVector& match_effects) {
  return {
      // succeed_if
      [&](Node* condition, BranchHint hint) -> void {
        Node* branch = graph()->NewNode(mcgraph()->common()->Branch(hint),
                                        condition, control());
        match_controls.emplace_back(
            graph()->NewNode(mcgraph()->common()->IfTrue(), branch));
        match_effects.emplace_back(effect());
        SetControl(graph()->NewNode(mcgraph()->common()->IfFalse(), branch));
      },
      // fail_if
      [&](Node* condition, BranchHint hint) -> void {
        Node* branch = graph()->NewNode(mcgraph()->common()->Branch(hint),
                                        condition, control());
        no_match_controls.emplace_back(
            graph()->NewNode(mcgraph()->common()->IfTrue(), branch));
        no_match_effects.emplace_back(effect());
        SetControl(graph()->NewNode(mcgraph()->common()->IfFalse(), branch));
      },
      // fail_if_not
      [&](Node* condition, BranchHint hint) -> void {
        Node* branch = graph()->NewNode(mcgraph()->common()->Branch(hint),
                                        condition, control());
        no_match_controls.emplace_back(
            graph()->NewNode(mcgraph()->common()->IfFalse(), branch));
        no_match_effects.emplace_back(effect());
        SetControl(graph()->NewNode(mcgraph()->common()->IfTrue(), branch));
      }};
}

void WasmGraphBuilder::DataCheck(Node* object, bool object_can_be_null,
                                 Callbacks callbacks) {
  if (object_can_be_null) {
    callbacks.fail_if(IsNull(object), BranchHint::kFalse);
  }
  callbacks.fail_if(gasm_->IsI31(object), BranchHint::kFalse);
  Node* map = gasm_->LoadMap(object);
  callbacks.fail_if_not(gasm_->IsDataRefMap(map), BranchHint::kTrue);
}

void WasmGraphBuilder::ManagedObjectInstanceCheck(Node* object,
                                                  bool object_can_be_null,
                                                  InstanceType instance_type,
                                                  Callbacks callbacks) {
  if (object_can_be_null) {
    callbacks.fail_if(IsNull(object), BranchHint::kFalse);
  }
  callbacks.fail_if(gasm_->IsI31(object), BranchHint::kFalse);
  callbacks.fail_if_not(gasm_->HasInstanceType(object, instance_type),
                        BranchHint::kTrue);
}

void WasmGraphBuilder::BrOnCastAbs(
    Node** match_control, Node** match_effect, Node** no_match_control,
    Node** no_match_effect, std::function<void(Callbacks)> type_checker) {
  SmallNodeVector no_match_controls, no_match_effects, match_controls,
      match_effects;

  type_checker(BranchCallbacks(no_match_controls, no_match_effects,
                               match_controls, match_effects));

  match_controls.emplace_back(control());
  match_effects.emplace_back(effect());

  // Wire up the control/effect nodes.
  DCHECK_EQ(match_controls.size(), match_effects.size());
  unsigned match_count = static_cast<unsigned>(match_controls.size());
  if (match_count == 1) {
    *match_control = match_controls[0];
    *match_effect = match_effects[0];
  } else {
    *match_control = Merge(match_count, match_controls.data());
    // EffectPhis need their control dependency as an additional input.
    match_effects.emplace_back(*match_control);
    *match_effect = EffectPhi(match_count, match_effects.data());
  }

  DCHECK_EQ(no_match_controls.size(), no_match_effects.size());
  unsigned no_match_count = static_cast<unsigned>(no_match_controls.size());
  if (no_match_count == 1) {
    *no_match_control = no_match_controls[0];
    *no_match_effect = no_match_effects[0];
  } else {
    // Range is 2..4, so casting to unsigned is safe.
    *no_match_control = Merge(no_match_count, no_match_controls.data());
    // EffectPhis need their control dependency as an additional input.
    no_match_effects.emplace_back(*no_match_control);
    *no_match_effect = EffectPhi(no_match_count, no_match_effects.data());
  }
}

Node* WasmGraphBuilder::RefTest(Node* object, Node* rtt,
                                WasmTypeCheckConfig config) {
  return gasm_->WasmTypeCheck(object, rtt, config);
}

Node* WasmGraphBuilder::RefCast(Node* object, Node* rtt,
                                WasmTypeCheckConfig config,
                                wasm::WasmCodePosition position) {
  return gasm_->WasmTypeCast(object, rtt, config);
}

void WasmGraphBuilder::BrOnCast(Node* object, Node* rtt,
                                WasmTypeCheckConfig config,
                                Node** match_control, Node** match_effect,
                                Node** no_match_control,
                                Node** no_match_effect) {
  Node* true_node;
  Node* false_node;
  BranchNoHint(gasm_->WasmTypeCheck(object, rtt, config), &true_node,
               &false_node);

  *match_effect = *no_match_effect = effect();
  *match_control = true_node;
  *no_match_control = false_node;
}

Node* WasmGraphBuilder::RefIsData(Node* object, bool object_can_be_null) {
  auto done = gasm_->MakeLabel(MachineRepresentation::kWord32);
  DataCheck(object, object_can_be_null, TestCallbacks(&done));
  gasm_->Goto(&done, Int32Constant(1));
  gasm_->Bind(&done);
  return done.PhiAt(0);
}

Node* WasmGraphBuilder::RefAsData(Node* object, bool object_can_be_null,
                                  wasm::WasmCodePosition position) {
  auto done = gasm_->MakeLabel();
  DataCheck(object, object_can_be_null, CastCallbacks(&done, position));
  gasm_->Goto(&done);
  gasm_->Bind(&done);
  return object;
}

void WasmGraphBuilder::BrOnData(Node* object, Node* /*rtt*/,
                                WasmTypeCheckConfig config,
                                Node** match_control, Node** match_effect,
                                Node** no_match_control,
                                Node** no_match_effect) {
  BrOnCastAbs(match_control, match_effect, no_match_control, no_match_effect,
              [=](Callbacks callbacks) -> void {
                return DataCheck(object, config.object_can_be_null, callbacks);
              });
}

Node* WasmGraphBuilder::RefIsArray(Node* object, bool object_can_be_null) {
  auto done = gasm_->MakeLabel(MachineRepresentation::kWord32);
  ManagedObjectInstanceCheck(object, object_can_be_null, WASM_ARRAY_TYPE,
                             TestCallbacks(&done));
  gasm_->Goto(&done, Int32Constant(1));
  gasm_->Bind(&done);
  return done.PhiAt(0);
}

Node* WasmGraphBuilder::RefAsArray(Node* object, bool object_can_be_null,
                                   wasm::WasmCodePosition position) {
  auto done = gasm_->MakeLabel();
  ManagedObjectInstanceCheck(object, object_can_be_null, WASM_ARRAY_TYPE,
                             CastCallbacks(&done, position));
  gasm_->Goto(&done);
  gasm_->Bind(&done);
  return object;
}

void WasmGraphBuilder::BrOnArray(Node* object, Node* /*rtt*/,
                                 WasmTypeCheckConfig config,
                                 Node** match_control, Node** match_effect,
                                 Node** no_match_control,
                                 Node** no_match_effect) {
  BrOnCastAbs(match_control, match_effect, no_match_control, no_match_effect,
              [=](Callbacks callbacks) -> void {
                return ManagedObjectInstanceCheck(object,
                                                  config.object_can_be_null,
                                                  WASM_ARRAY_TYPE, callbacks);
              });
}

Node* WasmGraphBuilder::RefIsI31(Node* object) { return gasm_->IsI31(object); }

Node* WasmGraphBuilder::RefAsI31(Node* object,
                                 wasm::WasmCodePosition position) {
  TrapIfFalse(wasm::kTrapIllegalCast, gasm_->IsI31(object), position);
  return object;
}

void WasmGraphBuilder::BrOnI31(Node* object, Node* /* rtt */,
                               WasmTypeCheckConfig /* config */,
                               Node** match_control, Node** match_effect,
                               Node** no_match_control,
                               Node** no_match_effect) {
  gasm_->Branch(gasm_->IsI31(object), match_control, no_match_control,
                BranchHint::kTrue);
  SetControl(*no_match_control);
  *match_effect = effect();
  *no_match_effect = effect();
}

Node* WasmGraphBuilder::TypeGuard(Node* value, wasm::ValueType type) {
  DCHECK_NOT_NULL(env_);
  return SetEffect(graph()->NewNode(mcgraph()->common()->TypeGuard(Type::Wasm(
                                        type, env_->module, graph()->zone())),
                                    value, effect(), control()));
}

Node* WasmGraphBuilder::StructGet(Node* struct_object,
                                  const wasm::StructType* struct_type,
                                  uint32_t field_index, CheckForNull null_check,
                                  bool is_signed,
                                  wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    struct_object = AssertNotNull(struct_object, position);
  }
  // It is not enough to invoke ValueType::machine_type(), because the
  // signedness has to be determined by {is_signed}.
  MachineType machine_type = MachineType::TypeForRepresentation(
      struct_type->field(field_index).machine_representation(), is_signed);
  Node* offset = gasm_->FieldOffset(struct_type, field_index);
  return struct_type->mutability(field_index)
             ? gasm_->LoadFromObject(machine_type, struct_object, offset)
             : gasm_->LoadImmutableFromObject(machine_type, struct_object,
                                              offset);
}

void WasmGraphBuilder::StructSet(Node* struct_object,
                                 const wasm::StructType* struct_type,
                                 uint32_t field_index, Node* field_value,
                                 CheckForNull null_check,
                                 wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    struct_object = AssertNotNull(struct_object, position);
  }
  gasm_->StoreStructField(struct_object, struct_type, field_index, field_value);
}

void WasmGraphBuilder::BoundsCheckArray(Node* array, Node* index,
                                        wasm::WasmCodePosition position) {
  if (V8_UNLIKELY(v8_flags.experimental_wasm_skip_bounds_checks)) return;
  Node* length = gasm_->LoadWasmArrayLength(array);
  TrapIfFalse(wasm::kTrapArrayOutOfBounds, gasm_->Uint32LessThan(index, length),
              position);
}

void WasmGraphBuilder::BoundsCheckArrayCopy(Node* array, Node* index,
                                            Node* length,
                                            wasm::WasmCodePosition position) {
  if (V8_UNLIKELY(v8_flags.experimental_wasm_skip_bounds_checks)) return;
  Node* array_length = gasm_->LoadWasmArrayLength(array);
  Node* range_end = gasm_->Int32Add(index, length);
  Node* range_valid = gasm_->Word32And(
      gasm_->Uint32LessThanOrEqual(range_end, array_length),
      gasm_->Uint32LessThanOrEqual(index, range_end));  // No overflow
  TrapIfFalse(wasm::kTrapArrayOutOfBounds, range_valid, position);
}

Node* WasmGraphBuilder::ArrayGet(Node* array_object,
                                 const wasm::ArrayType* type, Node* index,
                                 CheckForNull null_check, bool is_signed,
                                 wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    array_object = AssertNotNull(array_object, position);
  }
  BoundsCheckArray(array_object, index, position);
  MachineType machine_type = MachineType::TypeForRepresentation(
      type->element_type().machine_representation(), is_signed);
  Node* offset = gasm_->WasmArrayElementOffset(index, type->element_type());
  return type->mutability()
             ? gasm_->LoadFromObject(machine_type, array_object, offset)
             : gasm_->LoadImmutableFromObject(machine_type, array_object,
                                              offset);
}

void WasmGraphBuilder::ArraySet(Node* array_object, const wasm::ArrayType* type,
                                Node* index, Node* value,
                                CheckForNull null_check,
                                wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    array_object = AssertNotNull(array_object, position);
  }
  BoundsCheckArray(array_object, index, position);
  Node* offset = gasm_->WasmArrayElementOffset(index, type->element_type());
  gasm_->StoreToObject(ObjectAccessForGCStores(type->element_type()),
                       array_object, offset, value);
}

Node* WasmGraphBuilder::ArrayLen(Node* array_object, CheckForNull null_check,
                                 wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    array_object = AssertNotNull(array_object, position);
  }
  return gasm_->LoadWasmArrayLength(array_object);
}

// TODO(7748): Add an option to copy in a loop for small array sizes. To find
// the length limit, run test/mjsunit/wasm/array-copy-benchmark.js.
void WasmGraphBuilder::ArrayCopy(Node* dst_array, Node* dst_index,
                                 CheckForNull dst_null_check, Node* src_array,
                                 Node* src_index, CheckForNull src_null_check,
                                 Node* length,
                                 wasm::WasmCodePosition position) {
  if (dst_null_check == kWithNullCheck) {
    dst_array = AssertNotNull(dst_array, position);
  }
  if (src_null_check == kWithNullCheck) {
    src_array = AssertNotNull(src_array, position);
  }
  BoundsCheckArrayCopy(dst_array, dst_index, length, position);
  BoundsCheckArrayCopy(src_array, src_index, length, position);

  auto skip = gasm_->MakeLabel();

  gasm_->GotoIf(gasm_->Word32Equal(length, Int32Constant(0)), &skip,
                BranchHint::kFalse);

  Node* function =
      gasm_->ExternalConstant(ExternalReference::wasm_array_copy());
  MachineType arg_types[]{
      MachineType::TaggedPointer(), MachineType::TaggedPointer(),
      MachineType::Uint32(),        MachineType::TaggedPointer(),
      MachineType::Uint32(),        MachineType::Uint32()};
  MachineSignature sig(0, 6, arg_types);
  BuildCCall(&sig, function, GetInstance(), dst_array, dst_index, src_array,
             src_index, length);
  gasm_->Goto(&skip);
  gasm_->Bind(&skip);
}

Node* WasmGraphBuilder::StringNewWtf8(uint32_t memory,
                                      unibrow::Utf8Variant variant,
                                      Node* offset, Node* size) {
  return gasm_->CallBuiltin(Builtin::kWasmStringNewWtf8, Operator::kNoDeopt,
                            offset, size, gasm_->SmiConstant(memory),
                            gasm_->SmiConstant(static_cast<int32_t>(variant)));
}

Node* WasmGraphBuilder::StringNewWtf8Array(unibrow::Utf8Variant variant,
                                           Node* array, Node* start,
                                           Node* end) {
  return gasm_->CallBuiltin(Builtin::kWasmStringNewWtf8Array,
                            Operator::kNoDeopt, start, end, array,
                            gasm_->SmiConstant(static_cast<int32_t>(variant)));
}

Node* WasmGraphBuilder::StringNewWtf16(uint32_t memory, Node* offset,
                                       Node* size) {
  return gasm_->CallBuiltin(Builtin::kWasmStringNewWtf16, Operator::kNoDeopt,
                            gasm_->Uint32Constant(memory), offset, size);
}

Node* WasmGraphBuilder::StringNewWtf16Array(Node* array, Node* start,
                                            Node* end) {
  return gasm_->CallBuiltin(Builtin::kWasmStringNewWtf16Array,
                            Operator::kNoDeopt, array, start, end);
}

Node* WasmGraphBuilder::StringConst(uint32_t index) {
  return gasm_->CallBuiltin(Builtin::kWasmStringConst, Operator::kNoDeopt,
                            gasm_->Uint32Constant(index));
}

Node* WasmGraphBuilder::StringMeasureUtf8(Node* string, CheckForNull null_check,
                                          wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringMeasureUtf8, Operator::kNoDeopt,
                            string);
}

Node* WasmGraphBuilder::StringMeasureWtf8(Node* string, CheckForNull null_check,
                                          wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringMeasureWtf8, Operator::kNoDeopt,
                            string);
}

Node* WasmGraphBuilder::StringMeasureWtf16(Node* string,
                                           CheckForNull null_check,
                                           wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  return gasm_->LoadImmutableFromObject(
      MachineType::Int32(), string,
      wasm::ObjectAccess::ToTagged(String::kLengthOffset));
}

Node* WasmGraphBuilder::StringEncodeWtf8(uint32_t memory,
                                         unibrow::Utf8Variant variant,
                                         Node* string, CheckForNull null_check,
                                         Node* offset,
                                         wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringEncodeWtf8, Operator::kNoDeopt,
                            string, offset, gasm_->SmiConstant(memory),
                            gasm_->SmiConstant(static_cast<int32_t>(variant)));
}

Node* WasmGraphBuilder::StringEncodeWtf8Array(
    unibrow::Utf8Variant variant, Node* string, CheckForNull string_null_check,
    Node* array, CheckForNull array_null_check, Node* start,
    wasm::WasmCodePosition position) {
  if (string_null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  if (array_null_check == kWithNullCheck) {
    array = AssertNotNull(array, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringEncodeWtf8Array,
                            Operator::kNoDeopt, string, array, start,
                            gasm_->SmiConstant(static_cast<int32_t>(variant)));
}

Node* WasmGraphBuilder::StringEncodeWtf16(uint32_t memory, Node* string,
                                          CheckForNull null_check, Node* offset,
                                          wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringEncodeWtf16, Operator::kNoDeopt,
                            string, offset, gasm_->SmiConstant(memory));
}

Node* WasmGraphBuilder::StringEncodeWtf16Array(
    Node* string, CheckForNull string_null_check, Node* array,
    CheckForNull array_null_check, Node* start,
    wasm::WasmCodePosition position) {
  if (string_null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  if (array_null_check == kWithNullCheck) {
    array = AssertNotNull(array, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringEncodeWtf16Array,
                            Operator::kNoDeopt, string, array, start);
}

Node* WasmGraphBuilder::StringConcat(Node* head, CheckForNull head_null_check,
                                     Node* tail, CheckForNull tail_null_check,
                                     wasm::WasmCodePosition position) {
  if (head_null_check == kWithNullCheck) head = AssertNotNull(head, position);
  if (tail_null_check == kWithNullCheck) tail = AssertNotNull(tail, position);
  return gasm_->CallBuiltin(
      Builtin::kStringAdd_CheckNone, Operator::kEliminatable, head, tail,
      LOAD_INSTANCE_FIELD(NativeContext, MachineType::TaggedPointer()));
}

Node* WasmGraphBuilder::StringEqual(Node* a, CheckForNull a_null_check, Node* b,
                                    CheckForNull b_null_check,
                                    wasm::WasmCodePosition position) {
  auto done = gasm_->MakeLabel(MachineRepresentation::kWord32);
  // Covers "identical string pointer" and "both are null" cases.
  gasm_->GotoIf(gasm_->TaggedEqual(a, b), &done, Int32Constant(1));
  if (a_null_check == kWithNullCheck) {
    gasm_->GotoIf(gasm_->IsNull(a), &done, Int32Constant(0));
  }
  if (b_null_check == kWithNullCheck) {
    gasm_->GotoIf(gasm_->IsNull(b), &done, Int32Constant(0));
  }
  gasm_->Goto(&done, gasm_->CallBuiltin(Builtin::kWasmStringEqual,
                                        Operator::kNoDeopt, a, b));
  gasm_->Bind(&done);
  return done.PhiAt(0);
}

Node* WasmGraphBuilder::StringIsUSVSequence(Node* str, CheckForNull null_check,
                                            wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) str = AssertNotNull(str, position);

  return gasm_->CallBuiltin(Builtin::kWasmStringIsUSVSequence,
                            Operator::kNoDeopt, str);
}

Node* WasmGraphBuilder::StringAsWtf8(Node* str, CheckForNull null_check,
                                     wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) str = AssertNotNull(str, position);

  return gasm_->CallBuiltin(Builtin::kWasmStringAsWtf8, Operator::kNoDeopt,
                            str);
}

Node* WasmGraphBuilder::StringViewWtf8Advance(Node* view,
                                              CheckForNull null_check,
                                              Node* pos, Node* bytes,
                                              wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) view = AssertNotNull(view, position);

  return gasm_->CallBuiltin(Builtin::kWasmStringViewWtf8Advance,
                            Operator::kNoDeopt, view, pos, bytes);
}

void WasmGraphBuilder::StringViewWtf8Encode(
    uint32_t memory, unibrow::Utf8Variant variant, Node* view,
    CheckForNull null_check, Node* addr, Node* pos, Node* bytes,
    Node** next_pos, Node** bytes_written, wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    view = AssertNotNull(view, position);
  }
  Node* pair =
      gasm_->CallBuiltin(Builtin::kWasmStringViewWtf8Encode, Operator::kNoDeopt,
                         addr, pos, bytes, view, gasm_->SmiConstant(memory),
                         gasm_->SmiConstant(static_cast<int32_t>(variant)));
  *next_pos = gasm_->Projection(0, pair);
  *bytes_written = gasm_->Projection(1, pair);
}

Node* WasmGraphBuilder::StringViewWtf8Slice(Node* view, CheckForNull null_check,
                                            Node* pos, Node* bytes,
                                            wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    view = AssertNotNull(view, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringViewWtf8Slice,
                            Operator::kNoDeopt, view, pos, bytes);
}

Node* WasmGraphBuilder::StringViewWtf16GetCodeUnit(
    Node* string, CheckForNull null_check, Node* offset,
    wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringViewWtf16GetCodeUnit,
                            Operator::kNoDeopt, string, offset);
}

Node* WasmGraphBuilder::StringViewWtf16Encode(uint32_t memory, Node* string,
                                              CheckForNull null_check,
                                              Node* offset, Node* start,
                                              Node* codeunits,
                                              wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringViewWtf16Encode,
                            Operator::kNoDeopt, offset, start, codeunits,
                            string, gasm_->SmiConstant(memory));
}

Node* WasmGraphBuilder::StringViewWtf16Slice(Node* string,
                                             CheckForNull null_check,
                                             Node* start, Node* end,
                                             wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    string = AssertNotNull(string, position);
  }
  return gasm_->CallBuiltin(Builtin::kWasmStringViewWtf16Slice,
                            Operator::kNoDeopt, string, start, end);
}

Node* WasmGraphBuilder::StringAsIter(Node* str, CheckForNull null_check,
                                     wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) str = AssertNotNull(str, position);

  return gasm_->CallBuiltin(Builtin::kWasmStringAsIter, Operator::kNoDeopt,
                            str);
}

Node* WasmGraphBuilder::StringViewIterNext(Node* view, CheckForNull null_check,
                                           wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) view = AssertNotNull(view, position);

  return gasm_->CallBuiltin(Builtin::kWasmStringViewIterNext,
                            Operator::kNoDeopt, view);
}

Node* WasmGraphBuilder::StringViewIterAdvance(Node* view,
                                              CheckForNull null_check,
                                              Node* codepoints,
                                              wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) view = AssertNotNull(view, position);

  return gasm_->CallBuiltin(Builtin::kWasmStringViewIterAdvance,
                            Operator::kNoDeopt, view, codepoints);
}

Node* WasmGraphBuilder::StringViewIterRewind(Node* view,
                                             CheckForNull null_check,
                                             Node* codepoints,
                                             wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) view = AssertNotNull(view, position);

  return gasm_->CallBuiltin(Builtin::kWasmStringViewIterRewind,
                            Operator::kNoDeopt, view, codepoints);
}

Node* WasmGraphBuilder::StringViewIterSlice(Node* view, CheckForNull null_check,
                                            Node* codepoints,
                                            wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) view = AssertNotNull(view, position);

  return gasm_->CallBuiltin(Builtin::kWasmStringViewIterSlice,
                            Operator::kNoDeopt, view, codepoints);
}

// 1 bit V8 Smi tag, 31 bits V8 Smi shift, 1 bit i31ref high-bit truncation.
constexpr int kI31To32BitSmiShift = 33;

Node* WasmGraphBuilder::I31New(Node* input) {
  if (SmiValuesAre31Bits()) {
    return gasm_->Word32Shl(input, gasm_->BuildSmiShiftBitsConstant32());
  }
  DCHECK(SmiValuesAre32Bits());
  input = gasm_->BuildChangeInt32ToIntPtr(input);
  return gasm_->WordShl(input, gasm_->IntPtrConstant(kI31To32BitSmiShift));
}

Node* WasmGraphBuilder::I31GetS(Node* input, CheckForNull null_check,
                                wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) input = AssertNotNull(input, position);
  if (SmiValuesAre31Bits()) {
    input = gasm_->BuildTruncateIntPtrToInt32(input);
    return gasm_->Word32SarShiftOutZeros(input,
                                         gasm_->BuildSmiShiftBitsConstant32());
  }
  DCHECK(SmiValuesAre32Bits());
  return gasm_->BuildTruncateIntPtrToInt32(
      gasm_->WordSar(input, gasm_->IntPtrConstant(kI31To32BitSmiShift)));
}

Node* WasmGraphBuilder::I31GetU(Node* input, CheckForNull null_check,
                                wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) input = AssertNotNull(input, position);
  if (SmiValuesAre31Bits()) {
    input = gasm_->BuildTruncateIntPtrToInt32(input);
    return gasm_->Word32Shr(input, gasm_->BuildSmiShiftBitsConstant32());
  }
  DCHECK(SmiValuesAre32Bits());
  return gasm_->BuildTruncateIntPtrToInt32(
      gasm_->WordShr(input, gasm_->IntPtrConstant(kI31To32BitSmiShift)));
}

Node* WasmGraphBuilder::SetType(Node* node, wasm::ValueType type) {
  DCHECK_NOT_NULL(env_);
  if (!compiler::NodeProperties::IsTyped(node)) {
    compiler::NodeProperties::SetType(
        node, compiler::Type::Wasm(type, env_->module, graph_zone()));
  } else {
    // We might try to set the type twice since some nodes are cached in the
    // graph assembler, but we should never change the type.
    DCHECK_EQ(compiler::NodeProperties::GetType(node).AsWasm().type, type);
  }
  return node;
}

class WasmDecorator final : public GraphDecorator {
 public:
  explicit WasmDecorator(NodeOriginTable* origins, wasm::Decoder* decoder)
      : origins_(origins), decoder_(decoder) {}

  void Decorate(Node* node) final {
    origins_->SetNodeOrigin(
        node, NodeOrigin("wasm graph creation", "n/a",
                         NodeOrigin::kWasmBytecode, decoder_->position()));
  }

 private:
  compiler::NodeOriginTable* origins_;
  wasm::Decoder* decoder_;
};

void WasmGraphBuilder::AddBytecodePositionDecorator(
    NodeOriginTable* node_origins, wasm::Decoder* decoder) {
  DCHECK_NULL(decorator_);
  decorator_ = graph()->zone()->New<WasmDecorator>(node_origins, decoder);
  graph()->AddDecorator(decorator_);
}

void WasmGraphBuilder::RemoveBytecodePositionDecorator() {
  DCHECK_NOT_NULL(decorator_);
  graph()->RemoveDecorator(decorator_);
  decorator_ = nullptr;
}

namespace {

// A non-null {isolate} signifies that the generated code is treated as being in
// a JS frame for functions like BuildIsolateRoot().
class WasmWrapperGraphBuilder : public WasmGraphBuilder {
 public:
  WasmWrapperGraphBuilder(Zone* zone, MachineGraph* mcgraph,
                          const wasm::FunctionSig* sig,
                          const wasm::WasmModule* module,
                          Parameter0Mode parameter_mode, Isolate* isolate,
                          compiler::SourcePositionTable* spt,
                          StubCallMode stub_mode, wasm::WasmFeatures features)
      : WasmGraphBuilder(nullptr, zone, mcgraph, sig, spt, parameter_mode,
                         isolate),
        module_(module),
        stub_mode_(stub_mode),
        enabled_features_(features) {}

  CallDescriptor* GetI64ToBigIntCallDescriptor() {
    if (i64_to_bigint_descriptor_) return i64_to_bigint_descriptor_;

    i64_to_bigint_descriptor_ =
        GetBuiltinCallDescriptor(Builtin::kI64ToBigInt, zone_, stub_mode_);

    AddInt64LoweringReplacement(
        i64_to_bigint_descriptor_,
        GetBuiltinCallDescriptor(Builtin::kI32PairToBigInt, zone_, stub_mode_));
    return i64_to_bigint_descriptor_;
  }

  CallDescriptor* GetBigIntToI64CallDescriptor(bool needs_frame_state) {
    if (bigint_to_i64_descriptor_) return bigint_to_i64_descriptor_;

    bigint_to_i64_descriptor_ = GetBuiltinCallDescriptor(
        Builtin::kBigIntToI64, zone_, stub_mode_, needs_frame_state);

    AddInt64LoweringReplacement(
        bigint_to_i64_descriptor_,
        GetBuiltinCallDescriptor(Builtin::kBigIntToI32Pair, zone_, stub_mode_));
    return bigint_to_i64_descriptor_;
  }

  Node* GetTargetForBuiltinCall(wasm::WasmCode::RuntimeStubId wasm_stub,
                                Builtin builtin) {
    return (stub_mode_ == StubCallMode::kCallWasmRuntimeStub)
               ? mcgraph()->RelocatableIntPtrConstant(wasm_stub,
                                                      RelocInfo::WASM_STUB_CALL)
               : gasm_->GetBuiltinPointerTarget(builtin);
  }

  Node* BuildChangeInt32ToNumber(Node* value) {
    // We expect most integers at runtime to be Smis, so it is important for
    // wrapper performance that Smi conversion be inlined.
    if (SmiValuesAre32Bits()) {
      return gasm_->BuildChangeInt32ToSmi(value);
    }
    DCHECK(SmiValuesAre31Bits());

    auto builtin = gasm_->MakeDeferredLabel();
    auto done = gasm_->MakeLabel(MachineRepresentation::kTagged);

    // Double value to test if value can be a Smi, and if so, to convert it.
    Node* add = gasm_->Int32AddWithOverflow(value, value);
    Node* ovf = gasm_->Projection(1, add);
    gasm_->GotoIf(ovf, &builtin);

    // If it didn't overflow, the result is {2 * value} as pointer-sized value.
    Node* smi_tagged =
        gasm_->BuildChangeInt32ToIntPtr(gasm_->Projection(0, add));
    gasm_->Goto(&done, smi_tagged);

    // Otherwise, call builtin, to convert to a HeapNumber.
    gasm_->Bind(&builtin);
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target =
        GetTargetForBuiltinCall(wasm::WasmCode::kWasmInt32ToHeapNumber,
                                Builtin::kWasmInt32ToHeapNumber);
    if (!int32_to_heapnumber_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmInt32ToHeapNumberDescriptor(), 0,
          CallDescriptor::kNoFlags, Operator::kNoProperties, stub_mode_);
      int32_to_heapnumber_operator_.set(common->Call(call_descriptor));
    }
    Node* call =
        gasm_->Call(int32_to_heapnumber_operator_.get(), target, value);
    gasm_->Goto(&done, call);
    gasm_->Bind(&done);
    return done.PhiAt(0);
  }

  Node* BuildChangeTaggedToInt32(Node* value, Node* context,
                                 Node* frame_state) {
    // We expect most integers at runtime to be Smis, so it is important for
    // wrapper performance that Smi conversion be inlined.
    auto builtin = gasm_->MakeDeferredLabel();
    auto done = gasm_->MakeLabel(MachineRepresentation::kWord32);

    gasm_->GotoIfNot(IsSmi(value), &builtin);

    // If Smi, convert to int32.
    Node* smi = gasm_->BuildChangeSmiToInt32(value);
    gasm_->Goto(&done, smi);

    // Otherwise, call builtin which changes non-Smi to Int32.
    gasm_->Bind(&builtin);
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target =
        GetTargetForBuiltinCall(wasm::WasmCode::kWasmTaggedNonSmiToInt32,
                                Builtin::kWasmTaggedNonSmiToInt32);
    if (!tagged_non_smi_to_int32_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmTaggedNonSmiToInt32Descriptor(), 0,
          frame_state ? CallDescriptor::kNeedsFrameState
                      : CallDescriptor::kNoFlags,
          Operator::kNoProperties, stub_mode_);
      tagged_non_smi_to_int32_operator_.set(common->Call(call_descriptor));
    }
    Node* call = frame_state
                     ? gasm_->Call(tagged_non_smi_to_int32_operator_.get(),
                                   target, value, context, frame_state)
                     : gasm_->Call(tagged_non_smi_to_int32_operator_.get(),
                                   target, value, context);
    SetSourcePosition(call, 1);
    gasm_->Goto(&done, call);
    gasm_->Bind(&done);
    return done.PhiAt(0);
  }

  Node* BuildChangeFloat32ToNumber(Node* value) {
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target = GetTargetForBuiltinCall(wasm::WasmCode::kWasmFloat32ToNumber,
                                           Builtin::kWasmFloat32ToNumber);
    if (!float32_to_number_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmFloat32ToNumberDescriptor(), 0,
          CallDescriptor::kNoFlags, Operator::kNoProperties, stub_mode_);
      float32_to_number_operator_.set(common->Call(call_descriptor));
    }
    return gasm_->Call(float32_to_number_operator_.get(), target, value);
  }

  Node* BuildChangeFloat64ToNumber(Node* value) {
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target = GetTargetForBuiltinCall(wasm::WasmCode::kWasmFloat64ToNumber,
                                           Builtin::kWasmFloat64ToNumber);
    if (!float64_to_number_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmFloat64ToNumberDescriptor(), 0,
          CallDescriptor::kNoFlags, Operator::kNoProperties, stub_mode_);
      float64_to_number_operator_.set(common->Call(call_descriptor));
    }
    return gasm_->Call(float64_to_number_operator_.get(), target, value);
  }

  Node* BuildChangeTaggedToFloat64(Node* value, Node* context,
                                   Node* frame_state) {
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target = GetTargetForBuiltinCall(wasm::WasmCode::kWasmTaggedToFloat64,
                                           Builtin::kWasmTaggedToFloat64);
    bool needs_frame_state = frame_state != nullptr;
    if (!tagged_to_float64_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmTaggedToFloat64Descriptor(), 0,
          frame_state ? CallDescriptor::kNeedsFrameState
                      : CallDescriptor::kNoFlags,
          Operator::kNoProperties, stub_mode_);
      tagged_to_float64_operator_.set(common->Call(call_descriptor));
    }
    Node* call = needs_frame_state
                     ? gasm_->Call(tagged_to_float64_operator_.get(), target,
                                   value, context, frame_state)
                     : gasm_->Call(tagged_to_float64_operator_.get(), target,
                                   value, context);
    SetSourcePosition(call, 1);
    return call;
  }

  int AddArgumentNodes(base::Vector<Node*> args, int pos, int param_count,
                       const wasm::FunctionSig* sig, Node* context,
                       wasm::Suspend suspend) {
    // Convert wasm numbers to JS values.
    // Drop the instance node, and possibly the suspender node.
    int param_offset = 1 + suspend;
    for (int i = 0; i < param_count - suspend; ++i) {
      Node* param = Param(i + param_offset);
      args[pos++] = ToJS(param, sig->GetParam(i + suspend), context);
    }
    return pos;
  }

  Node* ToJS(Node* node, wasm::ValueType type, Node* context) {
    switch (type.kind()) {
      case wasm::kI32:
        return BuildChangeInt32ToNumber(node);
      case wasm::kI64:
        return BuildChangeInt64ToBigInt(node);
      case wasm::kF32:
        return BuildChangeFloat32ToNumber(node);
      case wasm::kF64:
        return BuildChangeFloat64ToNumber(node);
      case wasm::kRef:
      case wasm::kRefNull:
        switch (type.heap_representation()) {
          case wasm::HeapType::kFunc: {
            if (type.kind() == wasm::kRefNull) {
              auto done =
                  gasm_->MakeLabel(MachineRepresentation::kTaggedPointer);
              // Do not wrap {null}.
              gasm_->GotoIf(IsNull(node), &done, node);
              gasm_->Goto(&done,
                          gasm_->LoadFromObject(
                              MachineType::TaggedPointer(), node,
                              wasm::ObjectAccess::ToTagged(
                                  WasmInternalFunction::kExternalOffset)));
              gasm_->Bind(&done);
              return done.PhiAt(0);
            } else {
              return gasm_->LoadFromObject(
                  MachineType::TaggedPointer(), node,
                  wasm::ObjectAccess::ToTagged(
                      WasmInternalFunction::kExternalOffset));
            }
          }
          case wasm::HeapType::kEq: {
            // TODO(7748): Update this when JS interop is settled.
            auto done = gasm_->MakeLabel(MachineRepresentation::kTaggedPointer);
            // Do not wrap i31s.
            gasm_->GotoIf(IsSmi(node), &done, node);
            if (type.kind() == wasm::kRefNull) {
              // Do not wrap {null}.
              gasm_->GotoIf(IsNull(node), &done, node);
            }
            gasm_->Goto(&done, BuildAllocateObjectWrapper(node, context));
            gasm_->Bind(&done);
            return done.PhiAt(0);
          }
          case wasm::HeapType::kData:
          case wasm::HeapType::kArray:
            // TODO(7748): Update this when JS interop is settled.
            if (type.kind() == wasm::kRefNull) {
              auto done =
                  gasm_->MakeLabel(MachineRepresentation::kTaggedPointer);
              // Do not wrap {null}.
              gasm_->GotoIf(IsNull(node), &done, node);
              gasm_->Goto(&done, BuildAllocateObjectWrapper(node, context));
              gasm_->Bind(&done);
              return done.PhiAt(0);
            } else {
              return BuildAllocateObjectWrapper(node, context);
            }
          case wasm::HeapType::kString:
            // Either {node} is already a tagged JS string, or if type.kind() is
            // wasm::kRefNull, it's the null object.  Either way it's good to go
            // already to JS.
            return node;
          case wasm::HeapType::kExtern:
            return node;
          case wasm::HeapType::kNone:
          case wasm::HeapType::kNoFunc:
          case wasm::HeapType::kNoExtern:
          case wasm::HeapType::kI31:
          case wasm::HeapType::kAny:
            UNREACHABLE();
          default:
            DCHECK(type.has_index());
            if (module_->has_signature(type.ref_index())) {
              // Typed function. Extract the external function.
              return gasm_->LoadFromObject(
                  MachineType::TaggedPointer(), node,
                  wasm::ObjectAccess::ToTagged(
                      WasmInternalFunction::kExternalOffset));
            }
            // If this is reached, then IsJSCompatibleSignature() is too
            // permissive.
            // TODO(7748): Figure out a JS interop story for arrays and structs.
            UNREACHABLE();
        }
      case wasm::kRtt:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kS128:
      case wasm::kVoid:
      case wasm::kBottom:
        // If this is reached, then IsJSCompatibleSignature() is too permissive.
        UNREACHABLE();
    }
  }

  // TODO(7748): Temporary solution to allow round-tripping of Wasm objects
  // through JavaScript, where they show up as opaque boxes. This will disappear
  // once we have a proper WasmGC <-> JS interaction story.
  Node* BuildAllocateObjectWrapper(Node* input, Node* context) {
    if (v8_flags.wasm_gc_js_interop) return input;
    return gasm_->CallBuiltin(Builtin::kWasmAllocateObjectWrapper,
                              Operator::kEliminatable, input, context);
  }

  enum UnwrapExternalFunctions : bool {
    kUnwrapWasmExternalFunctions = true,
    kLeaveFunctionsAlone = false
  };

  Node* BuildChangeInt64ToBigInt(Node* input) {
    Node* target;
    if (mcgraph()->machine()->Is64()) {
      target = GetTargetForBuiltinCall(wasm::WasmCode::kI64ToBigInt,
                                       Builtin::kI64ToBigInt);
    } else {
      DCHECK(mcgraph()->machine()->Is32());
      // On 32-bit platforms we already set the target to the
      // I32PairToBigInt builtin here, so that we don't have to replace the
      // target in the int64-lowering.
      target = GetTargetForBuiltinCall(wasm::WasmCode::kI32PairToBigInt,
                                       Builtin::kI32PairToBigInt);
    }
    return gasm_->Call(GetI64ToBigIntCallDescriptor(), target, input);
  }

  Node* BuildChangeBigIntToInt64(Node* input, Node* context,
                                 Node* frame_state) {
    Node* target;
    if (mcgraph()->machine()->Is64()) {
      target = GetTargetForBuiltinCall(wasm::WasmCode::kBigIntToI64,
                                       Builtin::kBigIntToI64);
    } else {
      DCHECK(mcgraph()->machine()->Is32());
      // On 32-bit platforms we already set the target to the
      // BigIntToI32Pair builtin here, so that we don't have to replace the
      // target in the int64-lowering.
      target = GetTargetForBuiltinCall(wasm::WasmCode::kBigIntToI32Pair,
                                       Builtin::kBigIntToI32Pair);
    }

    return frame_state ? gasm_->Call(GetBigIntToI64CallDescriptor(true), target,
                                     input, context, frame_state)
                       : gasm_->Call(GetBigIntToI64CallDescriptor(false),
                                     target, input, context);
  }

  Node* BuildCheckString(Node* input, Node* js_context, wasm::ValueType type) {
    auto done = gasm_->MakeLabel(MachineRepresentation::kTagged);
    auto type_error = gasm_->MakeLabel();
    gasm_->GotoIf(IsSmi(input), &type_error, BranchHint::kFalse);
    if (type.is_nullable()) gasm_->GotoIf(IsNull(input), &done, input);
    Node* map = gasm_->LoadMap(input);
    Node* instance_type = gasm_->LoadInstanceType(map);
    Node* check = gasm_->Uint32LessThan(
        instance_type, gasm_->Uint32Constant(FIRST_NONSTRING_TYPE));
    gasm_->GotoIf(check, &done, BranchHint::kTrue, input);
    gasm_->Goto(&type_error);
    gasm_->Bind(&type_error);
    BuildCallToRuntimeWithContext(Runtime::kWasmThrowJSTypeError, js_context,
                                  nullptr, 0);
    TerminateThrow(effect(), control());
    gasm_->Bind(&done);
    return done.PhiAt(0);
  }

  Node* FromJS(Node* input, Node* js_context, wasm::ValueType type,
               Node* frame_state = nullptr) {
    switch (type.kind()) {
      case wasm::kRef:
      case wasm::kRefNull: {
        switch (type.heap_representation()) {
          // Fast paths for extern and string.
          // TODO(7748): Add more/all fast paths?
          case wasm::HeapType::kExtern:
            return input;
          case wasm::HeapType::kString:
            return BuildCheckString(input, js_context, type);
          case wasm::HeapType::kNone:
          case wasm::HeapType::kNoFunc:
          case wasm::HeapType::kNoExtern:
          case wasm::HeapType::kAny:
          case wasm::HeapType::kI31:
            UNREACHABLE();
          case wasm::HeapType::kFunc:
          case wasm::HeapType::kData:
          case wasm::HeapType::kArray:
          case wasm::HeapType::kEq:
          default: {
            // Make sure ValueType fits in a Smi.
            static_assert(wasm::ValueType::kLastUsedBit + 1 <= kSmiValueSize);

            // The instance node is always defined: if an instance is not
            // available, it is the undefined value.
            Node* inputs[] = {GetInstance(), input,
                              mcgraph()->IntPtrConstant(IntToSmi(
                                  static_cast<int>(type.raw_bit_field())))};

            return BuildCallToRuntimeWithContext(Runtime::kWasmJSToWasmObject,
                                                 js_context, inputs, 3);
          }
        }
      }
      case wasm::kF32:
        return gasm_->TruncateFloat64ToFloat32(
            BuildChangeTaggedToFloat64(input, js_context, frame_state));

      case wasm::kF64:
        return BuildChangeTaggedToFloat64(input, js_context, frame_state);

      case wasm::kI32:
        return BuildChangeTaggedToInt32(input, js_context, frame_state);

      case wasm::kI64:
        // i64 values can only come from BigInt.
        return BuildChangeBigIntToInt64(input, js_context, frame_state);

      case wasm::kRtt:
      case wasm::kS128:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kBottom:
      case wasm::kVoid:
        // If this is reached, then IsJSCompatibleSignature() is too permissive.
        UNREACHABLE();
    }
  }

  Node* SmiToFloat32(Node* input) {
    return gasm_->RoundInt32ToFloat32(gasm_->BuildChangeSmiToInt32(input));
  }

  Node* SmiToFloat64(Node* input) {
    return gasm_->ChangeInt32ToFloat64(gasm_->BuildChangeSmiToInt32(input));
  }

  Node* HeapNumberToFloat64(Node* input) {
    return gasm_->LoadFromObject(
        MachineType::Float64(), input,
        wasm::ObjectAccess::ToTagged(HeapNumber::kValueOffset));
  }

  Node* FromJSFast(Node* input, wasm::ValueType type) {
    switch (type.kind()) {
      case wasm::kI32:
        return gasm_->BuildChangeSmiToInt32(input);
      case wasm::kF32: {
        auto done = gasm_->MakeLabel(MachineRepresentation::kFloat32);
        auto heap_number = gasm_->MakeLabel();
        gasm_->GotoIfNot(IsSmi(input), &heap_number);
        gasm_->Goto(&done, SmiToFloat32(input));
        gasm_->Bind(&heap_number);
        Node* value =
            gasm_->TruncateFloat64ToFloat32(HeapNumberToFloat64(input));
        gasm_->Goto(&done, value);
        gasm_->Bind(&done);
        return done.PhiAt(0);
      }
      case wasm::kF64: {
        auto done = gasm_->MakeLabel(MachineRepresentation::kFloat64);
        auto heap_number = gasm_->MakeLabel();
        gasm_->GotoIfNot(IsSmi(input), &heap_number);
        gasm_->Goto(&done, SmiToFloat64(input));
        gasm_->Bind(&heap_number);
        gasm_->Goto(&done, HeapNumberToFloat64(input));
        gasm_->Bind(&done);
        return done.PhiAt(0);
      }
      case wasm::kRef:
      case wasm::kRefNull:
      case wasm::kI64:
      case wasm::kRtt:
      case wasm::kS128:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kBottom:
      case wasm::kVoid:
        UNREACHABLE();
    }
  }

  void BuildModifyThreadInWasmFlagHelper(Node* thread_in_wasm_flag_address,
                                         bool new_value) {
    if (v8_flags.debug_code) {
      Node* flag_value = gasm_->LoadFromObject(MachineType::Pointer(),
                                               thread_in_wasm_flag_address, 0);
      Node* check =
          gasm_->Word32Equal(flag_value, Int32Constant(new_value ? 0 : 1));

      Diamond flag_check(graph(), mcgraph()->common(), check,
                         BranchHint::kTrue);
      flag_check.Chain(control());
      SetControl(flag_check.if_false);
      Node* message_id = gasm_->NumberConstant(static_cast<int32_t>(
          new_value ? AbortReason::kUnexpectedThreadInWasmSet
                    : AbortReason::kUnexpectedThreadInWasmUnset));

      Node* old_effect = effect();
      Node* call = BuildCallToRuntimeWithContext(
          Runtime::kAbort, NoContextConstant(), &message_id, 1);
      flag_check.merge->ReplaceInput(1, call);
      SetEffectControl(flag_check.EffectPhi(old_effect, effect()),
                       flag_check.merge);
    }

    gasm_->StoreToObject(ObjectAccess(MachineType::Int32(), kNoWriteBarrier),
                         thread_in_wasm_flag_address, 0,
                         Int32Constant(new_value ? 1 : 0));
  }

  void BuildModifyThreadInWasmFlag(bool new_value) {
    if (!trap_handler::IsTrapHandlerEnabled()) return;
    Node* isolate_root = BuildLoadIsolateRoot();

    Node* thread_in_wasm_flag_address =
        gasm_->LoadFromObject(MachineType::Pointer(), isolate_root,
                              Isolate::thread_in_wasm_flag_address_offset());

    BuildModifyThreadInWasmFlagHelper(thread_in_wasm_flag_address, new_value);
  }

  class ModifyThreadInWasmFlagScope {
   public:
    ModifyThreadInWasmFlagScope(
        WasmWrapperGraphBuilder* wasm_wrapper_graph_builder,
        WasmGraphAssembler* gasm)
        : wasm_wrapper_graph_builder_(wasm_wrapper_graph_builder) {
      if (!trap_handler::IsTrapHandlerEnabled()) return;
      Node* isolate_root = wasm_wrapper_graph_builder_->BuildLoadIsolateRoot();

      thread_in_wasm_flag_address_ =
          gasm->LoadFromObject(MachineType::Pointer(), isolate_root,
                               Isolate::thread_in_wasm_flag_address_offset());

      wasm_wrapper_graph_builder_->BuildModifyThreadInWasmFlagHelper(
          thread_in_wasm_flag_address_, true);
    }

    ~ModifyThreadInWasmFlagScope() {
      if (!trap_handler::IsTrapHandlerEnabled()) return;

      wasm_wrapper_graph_builder_->BuildModifyThreadInWasmFlagHelper(
          thread_in_wasm_flag_address_, false);
    }

   private:
    WasmWrapperGraphBuilder* wasm_wrapper_graph_builder_;
    Node* thread_in_wasm_flag_address_;
  };

  Node* BuildMultiReturnFixedArrayFromIterable(const wasm::FunctionSig* sig,
                                               Node* iterable, Node* context) {
    Node* length = gasm_->BuildChangeUint31ToSmi(
        mcgraph()->Uint32Constant(static_cast<uint32_t>(sig->return_count())));
    return gasm_->CallBuiltin(Builtin::kIterableToFixedArrayForWasm,
                              Operator::kEliminatable, iterable, length,
                              context);
  }

  // Generate a call to the AllocateJSArray builtin.
  Node* BuildCallAllocateJSArray(Node* array_length, Node* context) {
    // Since we don't check that args will fit in an array,
    // we make sure this is true based on statically known limits.
    static_assert(wasm::kV8MaxWasmFunctionReturns <=
                  JSArray::kInitialMaxFastElementArray);
    return gasm_->CallBuiltin(Builtin::kWasmAllocateJSArray,
                              Operator::kEliminatable, array_length, context);
  }

  Node* BuildCallAndReturn(bool is_import, Node* js_context,
                           Node* function_data,
                           base::SmallVector<Node*, 16> args,
                           bool do_conversion, Node* frame_state) {
    const int rets_count = static_cast<int>(sig_->return_count());
    base::SmallVector<Node*, 1> rets(rets_count);

    // Set the ThreadInWasm flag before we do the actual call.
    {
      ModifyThreadInWasmFlagScope modify_thread_in_wasm_flag_builder(
          this, gasm_.get());

      if (is_import) {
        // Call to an imported function.
        // Load function index from {WasmExportedFunctionData}.
        Node* function_index = gasm_->BuildChangeSmiToInt32(
            gasm_->LoadExportedFunctionIndexAsSmi(function_data));
        BuildImportCall(sig_, base::VectorOf(args), base::VectorOf(rets),
                        wasm::kNoCodePosition, function_index, kCallContinues);
      } else {
        // Call to a wasm function defined in this module.
        // The (cached) call target is the jump table slot for that function.
        Node* internal = gasm_->LoadFromObject(
            MachineType::TaggedPointer(), function_data,
            wasm::ObjectAccess::ToTagged(WasmFunctionData::kInternalOffset));
        args[0] = BuildLoadExternalPointerFromObject(
            internal, WasmInternalFunction::kCallTargetOffset,
            kWasmInternalFunctionCallTargetTag);
        Node* instance_node = gasm_->LoadFromObject(
            MachineType::TaggedPointer(), internal,
            wasm::ObjectAccess::ToTagged(WasmInternalFunction::kRefOffset));
        BuildWasmCall(sig_, base::VectorOf(args), base::VectorOf(rets),
                      wasm::kNoCodePosition, instance_node, frame_state);
      }
    }

    Node* jsval;
    if (sig_->return_count() == 0) {
      jsval = UndefinedValue();
    } else if (sig_->return_count() == 1) {
      jsval = !do_conversion ? rets[0]
                             : ToJS(rets[0], sig_->GetReturn(), js_context);
    } else {
      int32_t return_count = static_cast<int32_t>(sig_->return_count());
      Node* size = gasm_->NumberConstant(return_count);

      jsval = BuildCallAllocateJSArray(size, js_context);

      Node* fixed_array = gasm_->LoadJSArrayElements(jsval);

      for (int i = 0; i < return_count; ++i) {
        Node* value = ToJS(rets[i], sig_->GetReturn(i), js_context);
        gasm_->StoreFixedArrayElementAny(fixed_array, i, value);
      }
    }
    return jsval;
  }

  bool QualifiesForFastTransform(const wasm::FunctionSig*) {
    const int wasm_count = static_cast<int>(sig_->parameter_count());
    for (int i = 0; i < wasm_count; ++i) {
      wasm::ValueType type = sig_->GetParam(i);
      switch (type.kind()) {
        case wasm::kRef:
        case wasm::kRefNull:
        case wasm::kI64:
        case wasm::kRtt:
        case wasm::kS128:
        case wasm::kI8:
        case wasm::kI16:
        case wasm::kBottom:
        case wasm::kVoid:
          return false;
        case wasm::kI32:
        case wasm::kF32:
        case wasm::kF64:
          break;
      }
    }
    return true;
  }

  Node* IsSmi(Node* input) {
    return gasm_->Word32Equal(
        gasm_->Word32And(gasm_->BuildTruncateIntPtrToInt32(input),
                         Int32Constant(kSmiTagMask)),
        Int32Constant(kSmiTag));
  }

  void CanTransformFast(
      Node* input, wasm::ValueType type,
      v8::internal::compiler::GraphAssemblerLabel<0>* slow_path) {
    switch (type.kind()) {
      case wasm::kI32: {
        gasm_->GotoIfNot(IsSmi(input), slow_path);
        return;
      }
      case wasm::kF32:
      case wasm::kF64: {
        auto done = gasm_->MakeLabel();
        gasm_->GotoIf(IsSmi(input), &done);
        Node* map = gasm_->LoadMap(input);
        Node* heap_number_map = LOAD_ROOT(HeapNumberMap, heap_number_map);
#if V8_MAP_PACKING
        Node* is_heap_number = gasm_->WordEqual(heap_number_map, map);
#else
        Node* is_heap_number = gasm_->TaggedEqual(heap_number_map, map);
#endif
        gasm_->GotoIf(is_heap_number, &done);
        gasm_->Goto(slow_path);
        gasm_->Bind(&done);
        return;
      }
      case wasm::kRef:
      case wasm::kRefNull:
      case wasm::kI64:
      case wasm::kRtt:
      case wasm::kS128:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kBottom:
      case wasm::kVoid:
        UNREACHABLE();
    }
  }

  void BuildJSToWasmWrapper(bool is_import, bool do_conversion = true,
                            Node* frame_state = nullptr) {
    const int wasm_param_count = static_cast<int>(sig_->parameter_count());

    // Build the start and the JS parameter nodes.
    Start(wasm_param_count + 5);

    // Create the js_closure and js_context parameters.
    Node* js_closure = Param(Linkage::kJSCallClosureParamIndex, "%closure");
    Node* js_context = Param(
        Linkage::GetJSCallContextParamIndex(wasm_param_count + 1), "%context");
    Node* function_data = gasm_->LoadFunctionDataFromJSFunction(js_closure);

    if (!wasm::IsJSCompatibleSignature(sig_, module_, enabled_features_)) {
      // Throw a TypeError. Use the js_context of the calling javascript
      // function (passed as a parameter), such that the generated code is
      // js_context independent.
      BuildCallToRuntimeWithContext(Runtime::kWasmThrowJSTypeError, js_context,
                                    nullptr, 0);
      TerminateThrow(effect(), control());
      return;
    }

    const int args_count = wasm_param_count + 1;  // +1 for wasm_code.

    // Check whether the signature of the function allows for a fast
    // transformation (if any params exist that need transformation).
    // Create a fast transformation path, only if it does.
    bool include_fast_path = do_conversion && wasm_param_count > 0 &&
                             QualifiesForFastTransform(sig_);

    // Prepare Param() nodes. Param() nodes can only be created once,
    // so we need to use the same nodes along all possible transformation paths.
    base::SmallVector<Node*, 16> params(args_count);
    for (int i = 0; i < wasm_param_count; ++i) params[i + 1] = Param(i + 1);

    auto done = gasm_->MakeLabel(MachineRepresentation::kTagged);
    if (include_fast_path) {
      auto slow_path = gasm_->MakeDeferredLabel();
      // Check if the params received on runtime can be actually transformed
      // using the fast transformation. When a param that cannot be transformed
      // fast is encountered, skip checking the rest and fall back to the slow
      // path.
      for (int i = 0; i < wasm_param_count; ++i) {
        CanTransformFast(params[i + 1], sig_->GetParam(i), &slow_path);
      }
      // Convert JS parameters to wasm numbers using the fast transformation
      // and build the call.
      base::SmallVector<Node*, 16> args(args_count);
      for (int i = 0; i < wasm_param_count; ++i) {
        Node* wasm_param = FromJSFast(params[i + 1], sig_->GetParam(i));
        args[i + 1] = wasm_param;
      }
      Node* jsval = BuildCallAndReturn(is_import, js_context, function_data,
                                       args, do_conversion, frame_state);
      gasm_->Goto(&done, jsval);
      gasm_->Bind(&slow_path);
    }
    // Convert JS parameters to wasm numbers using the default transformation
    // and build the call.
    base::SmallVector<Node*, 16> args(args_count);
    for (int i = 0; i < wasm_param_count; ++i) {
      if (do_conversion) {
        args[i + 1] =
            FromJS(params[i + 1], js_context, sig_->GetParam(i), frame_state);
      } else {
        Node* wasm_param = params[i + 1];

        // For Float32 parameters
        // we set UseInfo::CheckedNumberOrOddballAsFloat64 in
        // simplified-lowering and we need to add here a conversion from Float64
        // to Float32.
        if (sig_->GetParam(i).kind() == wasm::kF32) {
          wasm_param = gasm_->TruncateFloat64ToFloat32(wasm_param);
        }

        args[i + 1] = wasm_param;
      }
    }
    Node* jsval = BuildCallAndReturn(is_import, js_context, function_data, args,
                                     do_conversion, frame_state);
    // If both the default and a fast transformation paths are present,
    // get the return value based on the path used.
    if (include_fast_path) {
      gasm_->Goto(&done, jsval);
      gasm_->Bind(&done);
      Return(done.PhiAt(0));
    } else {
      Return(jsval);
    }
    if (ContainsInt64(sig_)) LowerInt64(kCalledFromJS);
  }

  Node* BuildReceiverNode(Node* callable_node, Node* native_context,
                          Node* undefined_node) {
    // Check function strict bit.
    Node* shared_function_info = gasm_->LoadSharedFunctionInfo(callable_node);
    Node* flags = gasm_->LoadFromObject(
        MachineType::Int32(), shared_function_info,
        wasm::ObjectAccess::FlagsOffsetInSharedFunctionInfo());
    Node* strict_check =
        Binop(wasm::kExprI32And, flags,
              Int32Constant(SharedFunctionInfo::IsNativeBit::kMask |
                            SharedFunctionInfo::IsStrictBit::kMask));

    // Load global receiver if sloppy else use undefined.
    Diamond strict_d(graph(), mcgraph()->common(), strict_check,
                     BranchHint::kNone);
    Node* old_effect = effect();
    SetControl(strict_d.if_false);
    Node* global_proxy = gasm_->LoadFixedArrayElementPtr(
        native_context, Context::GLOBAL_PROXY_INDEX);
    SetEffectControl(strict_d.EffectPhi(old_effect, global_proxy),
                     strict_d.merge);
    return strict_d.Phi(MachineRepresentation::kTagged, undefined_node,
                        global_proxy);
  }

  Node* BuildSuspend(Node* value, Node* suspender, Node* api_function_ref) {
    // If value is a promise, suspend to the js-to-wasm prompt, and resume later
    // with the promise's resolved value.
    auto resume = gasm_->MakeLabel(MachineRepresentation::kTagged);
    // Trap if the suspender argument is not the active suspender or if there is
    // no active suspender.
    auto bad_suspender = gasm_->MakeDeferredLabel();
    Node* native_context = gasm_->Load(
        MachineType::TaggedPointer(), api_function_ref,
        wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kNativeContextOffset));
    Node* active_suspender = LOAD_ROOT(ActiveSuspender, active_suspender);
    gasm_->GotoIf(gasm_->TaggedEqual(active_suspender, UndefinedValue()),
                  &bad_suspender, BranchHint::kFalse);
    gasm_->GotoIfNot(gasm_->TaggedEqual(suspender, active_suspender),
                     &bad_suspender, BranchHint::kFalse);
    gasm_->GotoIf(IsSmi(value), &resume, value);
    gasm_->GotoIfNot(gasm_->HasInstanceType(value, JS_PROMISE_TYPE), &resume,
                     BranchHint::kTrue, value);
    auto* call_descriptor = GetBuiltinCallDescriptor(
        Builtin::kWasmSuspend, zone_, StubCallMode::kCallWasmRuntimeStub);
    Node* call_target = mcgraph()->RelocatableIntPtrConstant(
        wasm::WasmCode::kWasmSuspend, RelocInfo::WASM_STUB_CALL);
    Node* args[] = {value, suspender};
    Node* chained_promise = BuildCallToRuntimeWithContext(
        Runtime::kWasmCreateResumePromise, native_context, args, 2);
    Node* resolved =
        gasm_->Call(call_descriptor, call_target, chained_promise, suspender);
    gasm_->Goto(&resume, resolved);
    gasm_->Bind(&bad_suspender);
    BuildCallToRuntimeWithContext(Runtime::kThrowBadSuspenderError,
                                  native_context, nullptr, 0);
    TerminateThrow(effect(), control());
    gasm_->Bind(&resume);
    return resume.PhiAt(0);
  }

  // For wasm-to-js wrappers, parameter 0 is a WasmApiFunctionRef.
  bool BuildWasmToJSWrapper(WasmImportCallKind kind, int expected_arity,
                            wasm::Suspend suspend) {
    int wasm_count = static_cast<int>(sig_->parameter_count());

    // Build the start and the parameter nodes.
    Start(wasm_count + 3);

    Node* native_context = gasm_->Load(
        MachineType::TaggedPointer(), Param(0),
        wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kNativeContextOffset));

    if (kind == WasmImportCallKind::kRuntimeTypeError) {
      // =======================================================================
      // === Runtime TypeError =================================================
      // =======================================================================
      BuildCallToRuntimeWithContext(Runtime::kWasmThrowJSTypeError,
                                    native_context, nullptr, 0);
      TerminateThrow(effect(), control());
      return false;
    }

    Node* callable_node = gasm_->Load(
        MachineType::TaggedPointer(), Param(0),
        wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kCallableOffset));

    Node* undefined_node = UndefinedValue();

    Node* call = nullptr;

    // Clear the ThreadInWasm flag.
    BuildModifyThreadInWasmFlag(false);

    switch (kind) {
      // =======================================================================
      // === JS Functions with matching arity ==================================
      // =======================================================================
      case WasmImportCallKind::kJSFunctionArityMatch: {
        base::SmallVector<Node*, 16> args(wasm_count + 7 - suspend);
        int pos = 0;
        Node* function_context =
            gasm_->LoadContextFromJSFunction(callable_node);
        args[pos++] = callable_node;  // target callable.

        // Determine receiver at runtime.
        args[pos++] =
            BuildReceiverNode(callable_node, native_context, undefined_node);

        auto call_descriptor = Linkage::GetJSCallDescriptor(
            graph()->zone(), false, wasm_count + 1 - suspend,
            CallDescriptor::kNoFlags);

        // Convert wasm numbers to JS values.
        pos = AddArgumentNodes(base::VectorOf(args), pos, wasm_count, sig_,
                               native_context, suspend);

        args[pos++] = undefined_node;  // new target
        args[pos++] = Int32Constant(
            JSParameterCount(wasm_count - suspend));  // argument count
        args[pos++] = function_context;
        args[pos++] = effect();
        args[pos++] = control();

        DCHECK_EQ(pos, args.size());
        call = gasm_->Call(call_descriptor, pos, args.begin());
        if (suspend) {
          call = BuildSuspend(call, Param(1), Param(0));
        }
        break;
      }
      // =======================================================================
      // === JS Functions with mismatching arity ===============================
      // =======================================================================
      case WasmImportCallKind::kJSFunctionArityMismatch: {
        int pushed_count = std::max(expected_arity, wasm_count - suspend);
        base::SmallVector<Node*, 16> args(pushed_count + 7);
        int pos = 0;

        args[pos++] = callable_node;  // target callable.
        // Determine receiver at runtime.
        args[pos++] =
            BuildReceiverNode(callable_node, native_context, undefined_node);

        // Convert wasm numbers to JS values.
        pos = AddArgumentNodes(base::VectorOf(args), pos, wasm_count, sig_,
                               native_context, suspend);
        for (int i = wasm_count - suspend; i < expected_arity; ++i) {
          args[pos++] = undefined_node;
        }
        args[pos++] = undefined_node;  // new target
        args[pos++] = Int32Constant(
            JSParameterCount(wasm_count - suspend));  // argument count

        Node* function_context =
            gasm_->LoadContextFromJSFunction(callable_node);
        args[pos++] = function_context;
        args[pos++] = effect();
        args[pos++] = control();
        DCHECK_EQ(pos, args.size());

        auto call_descriptor = Linkage::GetJSCallDescriptor(
            graph()->zone(), false, pushed_count + 1, CallDescriptor::kNoFlags);
        call = gasm_->Call(call_descriptor, pos, args.begin());
        if (suspend) {
          call = BuildSuspend(call, Param(1), Param(0));
        }
        break;
      }
      // =======================================================================
      // === General case of unknown callable ==================================
      // =======================================================================
      case WasmImportCallKind::kUseCallBuiltin: {
        base::SmallVector<Node*, 16> args(wasm_count + 7 - suspend);
        int pos = 0;
        args[pos++] =
            gasm_->GetBuiltinPointerTarget(Builtin::kCall_ReceiverIsAny);
        args[pos++] = callable_node;
        args[pos++] = Int32Constant(
            JSParameterCount(wasm_count - suspend));         // argument count
        args[pos++] = undefined_node;                        // receiver

        auto call_descriptor = Linkage::GetStubCallDescriptor(
            graph()->zone(), CallTrampolineDescriptor{},
            wasm_count + 1 - suspend, CallDescriptor::kNoFlags,
            Operator::kNoProperties, StubCallMode::kCallBuiltinPointer);

        // Convert wasm numbers to JS values.
        pos = AddArgumentNodes(base::VectorOf(args), pos, wasm_count, sig_,
                               native_context, suspend);

        // The native_context is sufficient here, because all kind of callables
        // which depend on the context provide their own context. The context
        // here is only needed if the target is a constructor to throw a
        // TypeError, if the target is a native function, or if the target is a
        // callable JSObject, which can only be constructed by the runtime.
        args[pos++] = native_context;
        args[pos++] = effect();
        args[pos++] = control();

        DCHECK_EQ(pos, args.size());
        call = gasm_->Call(call_descriptor, pos, args.begin());
        if (suspend) {
          call = BuildSuspend(call, Param(1), Param(0));
        }
        break;
      }
      default:
        UNREACHABLE();
    }
    DCHECK_NOT_NULL(call);

    SetSourcePosition(call, 0);

    // Convert the return value(s) back.
    if (sig_->return_count() <= 1) {
      Node* val = sig_->return_count() == 0
                      ? Int32Constant(0)
                      : FromJS(call, native_context, sig_->GetReturn());
      BuildModifyThreadInWasmFlag(true);
      Return(val);
    } else {
      Node* fixed_array =
          BuildMultiReturnFixedArrayFromIterable(sig_, call, native_context);
      base::SmallVector<Node*, 8> wasm_values(sig_->return_count());
      for (unsigned i = 0; i < sig_->return_count(); ++i) {
        wasm_values[i] = FromJS(gasm_->LoadFixedArrayElementAny(fixed_array, i),
                                native_context, sig_->GetReturn(i));
      }
      BuildModifyThreadInWasmFlag(true);
      Return(base::VectorOf(wasm_values));
    }

    if (ContainsInt64(sig_)) LowerInt64(kCalledFromWasm);
    return true;
  }

  void BuildCapiCallWrapper() {
    // Set up the graph start.
    Start(static_cast<int>(sig_->parameter_count()) +
          1 /* offset for first parameter index being -1 */ +
          1 /* WasmApiFunctionRef */);
    // Store arguments on our stack, then align the stack for calling to C.
    int param_bytes = 0;
    for (wasm::ValueType type : sig_->parameters()) {
      param_bytes += type.value_kind_size();
    }
    int return_bytes = 0;
    for (wasm::ValueType type : sig_->returns()) {
      return_bytes += type.value_kind_size();
    }

    int stack_slot_bytes = std::max(param_bytes, return_bytes);
    Node* values = stack_slot_bytes == 0
                       ? mcgraph()->IntPtrConstant(0)
                       : graph()->NewNode(mcgraph()->machine()->StackSlot(
                             stack_slot_bytes, kDoubleAlignment));

    int offset = 0;
    int param_count = static_cast<int>(sig_->parameter_count());
    for (int i = 0; i < param_count; ++i) {
      wasm::ValueType type = sig_->GetParam(i);
      // Start from the parameter with index 1 to drop the instance_node.
      // TODO(jkummerow): When a values is a reference type, we should pass it
      // in a GC-safe way, not just as a raw pointer.
      SetEffect(graph()->NewNode(GetSafeStoreOperator(offset, type), values,
                                 Int32Constant(offset), Param(i + 1), effect(),
                                 control()));
      offset += type.value_kind_size();
    }

    Node* function_node = gasm_->Load(
        MachineType::TaggedPointer(), Param(0),
        wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kCallableOffset));
    Node* sfi_data = gasm_->LoadFunctionDataFromJSFunction(function_node);
    Node* host_data_foreign =
        gasm_->Load(MachineType::AnyTagged(), sfi_data,
                    wasm::ObjectAccess::ToTagged(
                        WasmCapiFunctionData::kEmbedderDataOffset));

    BuildModifyThreadInWasmFlag(false);
    Node* isolate_root = BuildLoadIsolateRoot();
    Node* fp_value = graph()->NewNode(mcgraph()->machine()->LoadFramePointer());
    gasm_->Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                     kNoWriteBarrier),
                 isolate_root, Isolate::c_entry_fp_offset(), fp_value);

    Node* function = BuildLoadCallTargetFromExportedFunctionData(sfi_data);

    // Parameters: Address host_data_foreign, Address arguments.
    MachineType host_sig_types[] = {
        MachineType::Pointer(), MachineType::Pointer(), MachineType::Pointer()};
    MachineSignature host_sig(1, 2, host_sig_types);
    Node* return_value =
        BuildCCall(&host_sig, function, host_data_foreign, values);

    BuildModifyThreadInWasmFlag(true);

    Node* old_effect = effect();
    Node* exception_branch = graph()->NewNode(
        mcgraph()->common()->Branch(BranchHint::kTrue),
        gasm_->WordEqual(return_value, mcgraph()->IntPtrConstant(0)),
        control());
    SetControl(
        graph()->NewNode(mcgraph()->common()->IfFalse(), exception_branch));
    WasmRethrowExplicitContextDescriptor interface_descriptor;
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        mcgraph()->zone(), interface_descriptor,
        interface_descriptor.GetStackParameterCount(), CallDescriptor::kNoFlags,
        Operator::kNoProperties, StubCallMode::kCallWasmRuntimeStub);
    Node* call_target = mcgraph()->RelocatableIntPtrConstant(
        wasm::WasmCode::kWasmRethrowExplicitContext, RelocInfo::WASM_STUB_CALL);
    Node* context = gasm_->Load(
        MachineType::TaggedPointer(), Param(0),
        wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kNativeContextOffset));
    gasm_->Call(call_descriptor, call_target, return_value, context);
    TerminateThrow(effect(), control());

    SetEffectControl(old_effect, graph()->NewNode(mcgraph()->common()->IfTrue(),
                                                  exception_branch));
    DCHECK_LT(sig_->return_count(), wasm::kV8MaxWasmFunctionReturns);
    size_t return_count = sig_->return_count();
    if (return_count == 0) {
      Return(Int32Constant(0));
    } else {
      base::SmallVector<Node*, 8> returns(return_count);
      offset = 0;
      for (size_t i = 0; i < return_count; ++i) {
        wasm::ValueType type = sig_->GetReturn(i);
        Node* val = SetEffect(
            graph()->NewNode(GetSafeLoadOperator(offset, type), values,
                             Int32Constant(offset), effect(), control()));
        returns[i] = val;
        offset += type.value_kind_size();
      }
      Return(base::VectorOf(returns));
    }

    if (ContainsInt64(sig_)) LowerInt64(kCalledFromWasm);
  }

  void BuildJSFastApiCallWrapper(Handle<JSReceiver> callable) {
    // Here 'callable_node' must be equal to 'callable' but we cannot pass a
    // HeapConstant(callable) because WasmCode::Validate() fails with
    // Unexpected mode: FULL_EMBEDDED_OBJECT.
    Node* callable_node = gasm_->Load(
        MachineType::TaggedPointer(), Param(0),
        wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kCallableOffset));
    Node* native_context = gasm_->Load(
        MachineType::TaggedPointer(), Param(0),
        wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kNativeContextOffset));
    Node* undefined_node = UndefinedValue();

    BuildModifyThreadInWasmFlag(false);

    Handle<JSFunction> target;
    Node* target_node;
    Node* receiver_node;
    if (callable->IsJSBoundFunction()) {
      target = handle(
          JSFunction::cast(
              Handle<JSBoundFunction>::cast(callable)->bound_target_function()),
          callable->GetIsolate());
      target_node =
          gasm_->Load(MachineType::TaggedPointer(), callable_node,
                      wasm::ObjectAccess::ToTagged(
                          JSBoundFunction::kBoundTargetFunctionOffset));
      receiver_node = gasm_->Load(
          MachineType::TaggedPointer(), callable_node,
          wasm::ObjectAccess::ToTagged(JSBoundFunction::kBoundThisOffset));
    } else {
      DCHECK(callable->IsJSFunction());
      target = Handle<JSFunction>::cast(callable);
      target_node = callable_node;
      receiver_node =
          BuildReceiverNode(callable_node, native_context, undefined_node);
    }

    SharedFunctionInfo shared = target->shared();
    FunctionTemplateInfo api_func_data = shared.get_api_func_data();
    const Address c_address = api_func_data.GetCFunction(0);
    const v8::CFunctionInfo* c_signature = api_func_data.GetCSignature(0);

#ifdef V8_USE_SIMULATOR_WITH_GENERIC_C_CALLS
    Address c_functions[] = {c_address};
    const v8::CFunctionInfo* const c_signatures[] = {c_signature};
    target->GetIsolate()->simulator_data()->RegisterFunctionsAndSignatures(
        c_functions, c_signatures, 1);
#endif  //  V8_USE_SIMULATOR_WITH_GENERIC_C_CALLS

    Node* shared_function_info = gasm_->LoadSharedFunctionInfo(target_node);
    Node* function_template_info = gasm_->Load(
        MachineType::TaggedPointer(), shared_function_info,
        wasm::ObjectAccess::ToTagged(SharedFunctionInfo::kFunctionDataOffset));
    Node* call_code = gasm_->Load(
        MachineType::TaggedPointer(), function_template_info,
        wasm::ObjectAccess::ToTagged(FunctionTemplateInfo::kCallCodeOffset));
    Node* api_data_argument =
        gasm_->Load(MachineType::TaggedPointer(), call_code,
                    wasm::ObjectAccess::ToTagged(CallHandlerInfo::kDataOffset));

    FastApiCallFunctionVector fast_api_call_function_vector(mcgraph()->zone());
    fast_api_call_function_vector.push_back({c_address, c_signature});
    Node* call = fast_api_call::BuildFastApiCall(
        target->GetIsolate(), graph(), gasm_.get(),
        fast_api_call_function_vector, c_signature, api_data_argument,
        // Load and convert parameters passed to C function
        [this, c_signature, receiver_node](
            int param_index,
            fast_api_call::OverloadsResolutionResult& overloads,
            GraphAssemblerLabel<0>*) {
          // Wasm does not currently support overloads
          CHECK(!overloads.is_valid());

          auto store_stack = [this](Node* node) -> Node* {
            constexpr int kAlign = alignof(uintptr_t);
            constexpr int kSize = sizeof(uintptr_t);
            Node* stack_slot = gasm_->StackSlot(kSize, kAlign);
            gasm_->Store(
                StoreRepresentation(MachineType::PointerRepresentation(),
                                    kNoWriteBarrier),
                stack_slot, 0, node);
            return stack_slot;
          };

          if (param_index == 0) {
            return store_stack(receiver_node);
          }
          switch (c_signature->ArgumentInfo(param_index).GetType()) {
            case CTypeInfo::Type::kV8Value:
              return store_stack(Param(param_index));
            default:
              return Param(param_index);
          }
        },
        // Convert return value (no conversion needed for wasm)
        [](const CFunctionInfo* signature, Node* c_return_value) {
          return c_return_value;
        },
        // Initialize wasm-specific callback options fields
        [this](Node* options_stack_slot) {
#ifdef V8_ENABLE_SANDBOX
          Node* mem_start = LOAD_INSTANCE_FIELD_NO_ELIMINATION(
              MemoryStart, MachineType::SandboxedPointer());
#else
          Node* mem_start = LOAD_INSTANCE_FIELD_NO_ELIMINATION(
              MemoryStart, MachineType::UintPtr());
#endif

          Node* mem_size = LOAD_INSTANCE_FIELD_NO_ELIMINATION(
              MemorySize, MachineType::UintPtr());

          constexpr int kSize = sizeof(FastApiTypedArray<uint8_t>);
          constexpr int kAlign = alignof(FastApiTypedArray<uint8_t>);

          Node* stack_slot = gasm_->StackSlot(kSize, kAlign);

          gasm_->Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                           kNoWriteBarrier),
                       stack_slot, 0, mem_size);
          gasm_->Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                           kNoWriteBarrier),
                       stack_slot, sizeof(size_t), mem_start);

          gasm_->Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                           kNoWriteBarrier),
                       options_stack_slot,
                       static_cast<int>(
                           offsetof(v8::FastApiCallbackOptions, wasm_memory)),
                       stack_slot);
        },
        // Generate fallback slow call if fast call fails
        [this, callable_node, native_context, receiver_node]() -> Node* {
          int wasm_count = static_cast<int>(sig_->parameter_count());
          base::SmallVector<Node*, 16> args(wasm_count + 7);
          int pos = 0;
          args[pos++] =
              gasm_->GetBuiltinPointerTarget(Builtin::kCall_ReceiverIsAny);
          args[pos++] = callable_node;
          args[pos++] =
              Int32Constant(JSParameterCount(wasm_count));  // argument count
          args[pos++] = receiver_node;                      // receiver

          auto call_descriptor = Linkage::GetStubCallDescriptor(
              graph()->zone(), CallTrampolineDescriptor{}, wasm_count + 1,
              CallDescriptor::kNoFlags, Operator::kNoProperties,
              StubCallMode::kCallBuiltinPointer);

          // Convert wasm numbers to JS values.
          pos = AddArgumentNodes(base::VectorOf(args), pos, wasm_count, sig_,
                                 native_context, wasm::kNoSuspend);

          // The native_context is sufficient here, because all kind of
          // callables which depend on the context provide their own context.
          // The context here is only needed if the target is a constructor to
          // throw a TypeError, if the target is a native function, or if the
          // target is a callable JSObject, which can only be constructed by the
          // runtime.
          args[pos++] = native_context;
          args[pos++] = effect();
          args[pos++] = control();

          DCHECK_EQ(pos, args.size());
          Node* call = gasm_->Call(call_descriptor, pos, args.begin());
          return sig_->return_count() == 0
                     ? Int32Constant(0)
                     : FromJS(call, native_context, sig_->GetReturn());
        });

    BuildModifyThreadInWasmFlag(true);

    Return(call);
  }

  void BuildJSToJSWrapper() {
    int wasm_count = static_cast<int>(sig_->parameter_count());

    // Build the start and the parameter nodes.
    int param_count = 1 /* closure */ + 1 /* receiver */ + wasm_count +
                      1 /* new.target */ + 1 /* #arg */ + 1 /* context */;
    Start(param_count);
    Node* closure = Param(Linkage::kJSCallClosureParamIndex);
    Node* context = Param(Linkage::GetJSCallContextParamIndex(wasm_count + 1));

    // Throw a TypeError if the signature is incompatible with JavaScript.
    if (!wasm::IsJSCompatibleSignature(sig_, module_, enabled_features_)) {
      BuildCallToRuntimeWithContext(Runtime::kWasmThrowJSTypeError, context,
                                    nullptr, 0);
      TerminateThrow(effect(), control());
      return;
    }

    // Load the original callable from the closure.
    Node* func_data = gasm_->LoadFunctionDataFromJSFunction(closure);
    Node* internal = gasm_->LoadFromObject(
        MachineType::AnyTagged(), func_data,
        wasm::ObjectAccess::ToTagged(WasmFunctionData::kInternalOffset));
    Node* ref = gasm_->LoadFromObject(
        MachineType::AnyTagged(), internal,
        wasm::ObjectAccess::ToTagged(WasmInternalFunction::kRefOffset));
    Node* callable = gasm_->LoadFromObject(
        MachineType::AnyTagged(), ref,
        wasm::ObjectAccess::ToTagged(WasmApiFunctionRef::kCallableOffset));

    // Call the underlying closure.
    base::SmallVector<Node*, 16> args(wasm_count + 7);
    int pos = 0;
    args[pos++] = gasm_->GetBuiltinPointerTarget(Builtin::kCall_ReceiverIsAny);
    args[pos++] = callable;
    args[pos++] =
        Int32Constant(JSParameterCount(wasm_count));  // argument count
    args[pos++] = UndefinedValue();                   // receiver

    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), CallTrampolineDescriptor{}, wasm_count + 1,
        CallDescriptor::kNoFlags, Operator::kNoProperties,
        StubCallMode::kCallBuiltinPointer);

    // Convert parameter JS values to wasm numbers and back to JS values.
    for (int i = 0; i < wasm_count; ++i) {
      Node* param = Param(i + 1);  // Start from index 1 to skip receiver.
      args[pos++] = ToJS(FromJS(param, context, sig_->GetParam(i)),
                         sig_->GetParam(i), context);
    }

    args[pos++] = context;
    args[pos++] = effect();
    args[pos++] = control();

    DCHECK_EQ(pos, args.size());
    Node* call = gasm_->Call(call_descriptor, pos, args.begin());

    // Convert return JS values to wasm numbers and back to JS values.
    Node* jsval;
    if (sig_->return_count() == 0) {
      jsval = UndefinedValue();
    } else if (sig_->return_count() == 1) {
      jsval = ToJS(FromJS(call, context, sig_->GetReturn()), sig_->GetReturn(),
                   context);
    } else {
      Node* fixed_array =
          BuildMultiReturnFixedArrayFromIterable(sig_, call, context);
      int32_t return_count = static_cast<int32_t>(sig_->return_count());
      Node* size = gasm_->NumberConstant(return_count);
      jsval = BuildCallAllocateJSArray(size, context);
      Node* result_fixed_array = gasm_->LoadJSArrayElements(jsval);
      for (unsigned i = 0; i < sig_->return_count(); ++i) {
        const auto& type = sig_->GetReturn(i);
        Node* elem = gasm_->LoadFixedArrayElementAny(fixed_array, i);
        Node* cast = ToJS(FromJS(elem, context, type), type, context);
        gasm_->StoreFixedArrayElementAny(result_fixed_array, i, cast);
      }
    }
    Return(jsval);
  }

  void BuildCWasmEntry() {
    // +1 offset for first parameter index being -1.
    Start(CWasmEntryParameters::kNumParameters + 1);

    Node* code_entry = Param(CWasmEntryParameters::kCodeEntry);
    Node* object_ref = Param(CWasmEntryParameters::kObjectRef);
    Node* arg_buffer = Param(CWasmEntryParameters::kArgumentsBuffer);
    Node* c_entry_fp = Param(CWasmEntryParameters::kCEntryFp);

    Node* fp_value = graph()->NewNode(mcgraph()->machine()->LoadFramePointer());
    gasm_->Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                     kNoWriteBarrier),
                 fp_value, TypedFrameConstants::kFirstPushedFrameValueOffset,
                 c_entry_fp);

    int wasm_arg_count = static_cast<int>(sig_->parameter_count());
    base::SmallVector<Node*, 16> args(wasm_arg_count + 4);

    int pos = 0;
    args[pos++] = code_entry;
    args[pos++] = object_ref;

    int offset = 0;
    for (wasm::ValueType type : sig_->parameters()) {
      Node* arg_load = SetEffect(
          graph()->NewNode(GetSafeLoadOperator(offset, type), arg_buffer,
                           Int32Constant(offset), effect(), control()));
      args[pos++] = arg_load;
      offset += type.value_kind_size();
    }

    args[pos++] = effect();
    args[pos++] = control();

    // Call the wasm code.
    auto call_descriptor = GetWasmCallDescriptor(mcgraph()->zone(), sig_);

    DCHECK_EQ(pos, args.size());
    Node* call = gasm_->Call(call_descriptor, pos, args.begin());

    Node* if_success = graph()->NewNode(mcgraph()->common()->IfSuccess(), call);
    Node* if_exception =
        graph()->NewNode(mcgraph()->common()->IfException(), call, call);

    // Handle exception: return it.
    SetEffectControl(if_exception);
    Return(if_exception);

    // Handle success: store the return value(s).
    SetEffectControl(call, if_success);
    pos = 0;
    offset = 0;
    for (wasm::ValueType type : sig_->returns()) {
      Node* value = sig_->return_count() == 1
                        ? call
                        : graph()->NewNode(mcgraph()->common()->Projection(pos),
                                           call, control());
      SetEffect(graph()->NewNode(GetSafeStoreOperator(offset, type), arg_buffer,
                                 Int32Constant(offset), value, effect(),
                                 control()));
      offset += type.value_kind_size();
      pos++;
    }

    Return(mcgraph()->IntPtrConstant(0));

    if (mcgraph()->machine()->Is32() && ContainsInt64(sig_)) {
      // No special lowering should be requested in the C entry.
      DCHECK_NULL(lowering_special_case_);

      MachineRepresentation sig_reps[] = {
          MachineType::PointerRepresentation(),  // return value
          MachineType::PointerRepresentation(),  // target
          MachineRepresentation::kTagged,        // object_ref
          MachineType::PointerRepresentation(),  // argv
          MachineType::PointerRepresentation()   // c_entry_fp
      };
      Signature<MachineRepresentation> c_entry_sig(1, 4, sig_reps);
      Int64Lowering r(mcgraph()->graph(), mcgraph()->machine(),
                      mcgraph()->common(), gasm_->simplified(),
                      mcgraph()->zone(), module_, &c_entry_sig);
      r.LowerGraph();
    }
  }

 private:
  const wasm::WasmModule* module_;
  StubCallMode stub_mode_;
  SetOncePointer<const Operator> int32_to_heapnumber_operator_;
  SetOncePointer<const Operator> tagged_non_smi_to_int32_operator_;
  SetOncePointer<const Operator> float32_to_number_operator_;
  SetOncePointer<const Operator> float64_to_number_operator_;
  SetOncePointer<const Operator> tagged_to_float64_operator_;
  wasm::WasmFeatures enabled_features_;
  CallDescriptor* bigint_to_i64_descriptor_ = nullptr;
  CallDescriptor* i64_to_bigint_descriptor_ = nullptr;
};

}  // namespace

void BuildInlinedJSToWasmWrapper(
    Zone* zone, MachineGraph* mcgraph, const wasm::FunctionSig* signature,
    const wasm::WasmModule* module, Isolate* isolate,
    compiler::SourcePositionTable* spt, StubCallMode stub_mode,
    wasm::WasmFeatures features, Node* frame_state) {
  WasmWrapperGraphBuilder builder(zone, mcgraph, signature, module,
                                  WasmGraphBuilder::kNoSpecialParameterMode,
                                  isolate, spt, stub_mode, features);
  builder.BuildJSToWasmWrapper(false, false, frame_state);
}

std::unique_ptr<TurbofanCompilationJob> NewJSToWasmCompilationJob(
    Isolate* isolate, const wasm::FunctionSig* sig,
    const wasm::WasmModule* module, bool is_import,
    const wasm::WasmFeatures& enabled_features) {
  //----------------------------------------------------------------------------
  // Create the Graph.
  //----------------------------------------------------------------------------
  std::unique_ptr<Zone> zone = std::make_unique<Zone>(
      wasm::GetWasmEngine()->allocator(), ZONE_NAME, kCompressGraphZone);
  Graph* graph = zone->New<Graph>(zone.get());
  CommonOperatorBuilder* common = zone->New<CommonOperatorBuilder>(zone.get());
  MachineOperatorBuilder* machine = zone->New<MachineOperatorBuilder>(
      zone.get(), MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone->New<MachineGraph>(graph, common, machine);

  WasmWrapperGraphBuilder builder(
      zone.get(), mcgraph, sig, module,
      WasmGraphBuilder::kNoSpecialParameterMode, isolate, nullptr,
      StubCallMode::kCallBuiltinPointer, enabled_features);
  builder.BuildJSToWasmWrapper(is_import);

  //----------------------------------------------------------------------------
  // Create the compilation job.
  //----------------------------------------------------------------------------
  std::unique_ptr<char[]> debug_name = WasmExportedFunction::GetDebugName(sig);

  int params = static_cast<int>(sig->parameter_count());
  CallDescriptor* incoming = Linkage::GetJSCallDescriptor(
      zone.get(), false, params + 1, CallDescriptor::kNoFlags);

  return Pipeline::NewWasmHeapStubCompilationJob(
      isolate, incoming, std::move(zone), graph, CodeKind::JS_TO_WASM_FUNCTION,
      std::move(debug_name), WasmAssemblerOptions());
}

static MachineRepresentation NormalizeFastApiRepresentation(
    const CTypeInfo& info) {
  MachineType t = MachineType::TypeForCType(info);
  // Wasm representation of bool is i32 instead of i1.
  if (t.semantic() == MachineSemantic::kBool) {
    return MachineRepresentation::kWord32;
  }
  return t.representation();
}

static bool IsSupportedWasmFastApiFunction(
    const wasm::FunctionSig* expected_sig, Handle<SharedFunctionInfo> shared) {
  if (!shared->IsApiFunction()) {
    return false;
  }
  if (shared->get_api_func_data().GetCFunctionsCount() == 0) {
    return false;
  }
  if (!shared->get_api_func_data().accept_any_receiver()) {
    return false;
  }
  if (!shared->get_api_func_data().signature().IsUndefined()) {
    // TODO(wasm): CFunctionInfo* signature check.
    return false;
  }
  const CFunctionInfo* info = shared->get_api_func_data().GetCSignature(0);
  if (!fast_api_call::CanOptimizeFastSignature(info)) {
    return false;
  }

  const auto log_imported_function_mismatch = [&shared](const char* reason) {
    if (v8_flags.trace_opt) {
      CodeTracer::Scope scope(shared->GetIsolate()->GetCodeTracer());
      PrintF(scope.file(), "[disabled optimization for ");
      shared->ShortPrint(scope.file());
      PrintF(scope.file(),
             ", reason: the signature of the imported function in the Wasm "
             "module doesn't match that of the Fast API function (%s)]\n",
             reason);
    }
  };

  // C functions only have one return value.
  if (expected_sig->return_count() > 1) {
    // Here and below, we log when the function we call is declared as an Api
    // function but we cannot optimize the call, which might be unxepected. In
    // that case we use the "slow" path making a normal Wasm->JS call and
    // calling the "slow" callback specified in FunctionTemplate::New().
    log_imported_function_mismatch("too many return values");
    return false;
  }
  CTypeInfo return_info = info->ReturnInfo();
  // Unsupported if return type doesn't match.
  if (expected_sig->return_count() == 0 &&
      return_info.GetType() != CTypeInfo::Type::kVoid) {
    log_imported_function_mismatch("too few return values");
    return false;
  }
  // Unsupported if return type doesn't match.
  if (expected_sig->return_count() == 1) {
    if (return_info.GetType() == CTypeInfo::Type::kVoid) {
      log_imported_function_mismatch("too many return values");
      return false;
    }
    if (NormalizeFastApiRepresentation(return_info) !=
        expected_sig->GetReturn(0).machine_type().representation()) {
      log_imported_function_mismatch("mismatching return value");
      return false;
    }
  }
  // Unsupported if arity doesn't match.
  if (expected_sig->parameter_count() != info->ArgumentCount() - 1) {
    log_imported_function_mismatch("mismatched arity");
    return false;
  }
  // Unsupported if any argument types don't match.
  for (unsigned int i = 0; i < expected_sig->parameter_count(); i += 1) {
    // Arg 0 is the receiver, skip over it since wasm doesn't
    // have a concept of receivers.
    CTypeInfo arg = info->ArgumentInfo(i + 1);
    if (NormalizeFastApiRepresentation(arg) !=
        expected_sig->GetParam(i).machine_type().representation()) {
      log_imported_function_mismatch("parameter type mismatch");
      return false;
    }
  }
  return true;
}

bool ResolveBoundJSFastApiFunction(const wasm::FunctionSig* expected_sig,
                                   Handle<JSReceiver> callable) {
  Handle<JSFunction> target;
  if (callable->IsJSBoundFunction()) {
    Handle<JSBoundFunction> bound_target =
        Handle<JSBoundFunction>::cast(callable);
    // Nested bound functions and arguments not supported yet.
    if (bound_target->bound_arguments().length() > 0) {
      return false;
    }
    if (bound_target->bound_target_function().IsJSBoundFunction()) {
      return false;
    }
    Handle<JSReceiver> bound_target_function =
        handle(bound_target->bound_target_function(), callable->GetIsolate());
    if (!bound_target_function->IsJSFunction()) {
      return false;
    }
    target = Handle<JSFunction>::cast(bound_target_function);
  } else if (callable->IsJSFunction()) {
    target = Handle<JSFunction>::cast(callable);
  } else {
    return false;
  }

  Handle<SharedFunctionInfo> shared(target->shared(), target->GetIsolate());
  return IsSupportedWasmFastApiFunction(expected_sig, shared);
}

WasmImportData ResolveWasmImportCall(
    Handle<JSReceiver> callable, const wasm::FunctionSig* expected_sig,
    const wasm::WasmModule* module,
    const wasm::WasmFeatures& enabled_features) {
  Isolate* isolate = callable->GetIsolate();
  if (WasmExportedFunction::IsWasmExportedFunction(*callable)) {
    auto imported_function = Handle<WasmExportedFunction>::cast(callable);
    if (!imported_function->MatchesSignature(module, expected_sig)) {
      return {WasmImportCallKind::kLinkError, callable, wasm::kNoSuspend};
    }
    uint32_t func_index =
        static_cast<uint32_t>(imported_function->function_index());
    if (func_index >=
        imported_function->instance().module()->num_imported_functions) {
      return {WasmImportCallKind::kWasmToWasm, callable, wasm::kNoSuspend};
    }
    // Resolve the shortcut to the underlying callable and continue.
    Handle<WasmInstanceObject> instance(imported_function->instance(), isolate);
    ImportedFunctionEntry entry(instance, func_index);
    callable = handle(entry.callable(), isolate);
  }
  wasm::Suspend suspend = wasm::kNoSuspend;
  if (WasmJSFunction::IsWasmJSFunction(*callable)) {
    auto js_function = Handle<WasmJSFunction>::cast(callable);
    suspend = js_function->GetSuspend();
    if (!js_function->MatchesSignature(expected_sig)) {
      return {WasmImportCallKind::kLinkError, callable, wasm::kNoSuspend};
    }
    // Resolve the short-cut to the underlying callable and continue.
    callable = handle(js_function->GetCallable(), isolate);
  }
  if (WasmCapiFunction::IsWasmCapiFunction(*callable)) {
    auto capi_function = Handle<WasmCapiFunction>::cast(callable);
    if (!capi_function->MatchesSignature(expected_sig)) {
      return {WasmImportCallKind::kLinkError, callable, wasm::kNoSuspend};
    }
    return {WasmImportCallKind::kWasmToCapi, callable, wasm::kNoSuspend};
  }
  // Assuming we are calling to JS, check whether this would be a runtime error.
  if (!wasm::IsJSCompatibleSignature(expected_sig, module, enabled_features)) {
    return {WasmImportCallKind::kRuntimeTypeError, callable, wasm::kNoSuspend};
  }
  // Check if this can be a JS fast API call.
  if (v8_flags.turbo_fast_api_calls &&
      ResolveBoundJSFastApiFunction(expected_sig, callable)) {
    return {WasmImportCallKind::kWasmToJSFastApi, callable, wasm::kNoSuspend};
  }
  // For JavaScript calls, determine whether the target has an arity match.
  if (callable->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(callable);
    Handle<SharedFunctionInfo> shared(function->shared(),
                                      function->GetIsolate());

// Check for math intrinsics.
#define COMPARE_SIG_FOR_BUILTIN(name)                                     \
  {                                                                       \
    const wasm::FunctionSig* sig =                                        \
        wasm::WasmOpcodes::Signature(wasm::kExpr##name);                  \
    if (!sig) sig = wasm::WasmOpcodes::AsmjsSignature(wasm::kExpr##name); \
    DCHECK_NOT_NULL(sig);                                                 \
    if (*expected_sig == *sig) {                                          \
      return {WasmImportCallKind::k##name, callable, wasm::kNoSuspend};   \
    }                                                                     \
  }
#define COMPARE_SIG_FOR_BUILTIN_F64(name) \
  case Builtin::kMath##name:              \
    COMPARE_SIG_FOR_BUILTIN(F64##name);   \
    break;
#define COMPARE_SIG_FOR_BUILTIN_F32_F64(name) \
  case Builtin::kMath##name:                  \
    COMPARE_SIG_FOR_BUILTIN(F64##name);       \
    COMPARE_SIG_FOR_BUILTIN(F32##name);       \
    break;

    if (v8_flags.wasm_math_intrinsics && shared->HasBuiltinId()) {
      switch (shared->builtin_id()) {
        COMPARE_SIG_FOR_BUILTIN_F64(Acos);
        COMPARE_SIG_FOR_BUILTIN_F64(Asin);
        COMPARE_SIG_FOR_BUILTIN_F64(Atan);
        COMPARE_SIG_FOR_BUILTIN_F64(Cos);
        COMPARE_SIG_FOR_BUILTIN_F64(Sin);
        COMPARE_SIG_FOR_BUILTIN_F64(Tan);
        COMPARE_SIG_FOR_BUILTIN_F64(Exp);
        COMPARE_SIG_FOR_BUILTIN_F64(Log);
        COMPARE_SIG_FOR_BUILTIN_F64(Atan2);
        COMPARE_SIG_FOR_BUILTIN_F64(Pow);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Min);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Max);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Abs);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Ceil);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Floor);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Sqrt);
        case Builtin::kMathFround:
          COMPARE_SIG_FOR_BUILTIN(F32ConvertF64);
          break;
        default:
          break;
      }
    }

#undef COMPARE_SIG_FOR_BUILTIN
#undef COMPARE_SIG_FOR_BUILTIN_F64
#undef COMPARE_SIG_FOR_BUILTIN_F32_F64

    if (IsClassConstructor(shared->kind())) {
      // Class constructor will throw anyway.
      return {WasmImportCallKind::kUseCallBuiltin, callable, suspend};
    }

    if (shared->internal_formal_parameter_count_without_receiver() ==
        expected_sig->parameter_count() - suspend) {
      return {WasmImportCallKind::kJSFunctionArityMatch, callable, suspend};
    }

    // If function isn't compiled, compile it now.
    Isolate* isolate = callable->GetIsolate();
    IsCompiledScope is_compiled_scope(shared->is_compiled_scope(isolate));
    if (!is_compiled_scope.is_compiled()) {
      Compiler::Compile(isolate, function, Compiler::CLEAR_EXCEPTION,
                        &is_compiled_scope);
    }

    return {WasmImportCallKind::kJSFunctionArityMismatch, callable, suspend};
  }
  // Unknown case. Use the call builtin.
  return {WasmImportCallKind::kUseCallBuiltin, callable, suspend};
}

namespace {

wasm::WasmOpcode GetMathIntrinsicOpcode(WasmImportCallKind kind,
                                        const char** name_ptr) {
#define CASE(name)                          \
  case WasmImportCallKind::k##name:         \
    *name_ptr = "WasmMathIntrinsic:" #name; \
    return wasm::kExpr##name
  switch (kind) {
    CASE(F64Acos);
    CASE(F64Asin);
    CASE(F64Atan);
    CASE(F64Cos);
    CASE(F64Sin);
    CASE(F64Tan);
    CASE(F64Exp);
    CASE(F64Log);
    CASE(F64Atan2);
    CASE(F64Pow);
    CASE(F64Ceil);
    CASE(F64Floor);
    CASE(F64Sqrt);
    CASE(F64Min);
    CASE(F64Max);
    CASE(F64Abs);
    CASE(F32Min);
    CASE(F32Max);
    CASE(F32Abs);
    CASE(F32Ceil);
    CASE(F32Floor);
    CASE(F32Sqrt);
    CASE(F32ConvertF64);
    default:
      UNREACHABLE();
  }
#undef CASE
}

wasm::WasmCompilationResult CompileWasmMathIntrinsic(
    WasmImportCallKind kind, const wasm::FunctionSig* sig) {
  DCHECK_EQ(1, sig->return_count());

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileWasmMathIntrinsic");

  Zone zone(wasm::GetWasmEngine()->allocator(), ZONE_NAME, kCompressGraphZone);

  // Compile a Wasm function with a single bytecode and let TurboFan
  // generate either inlined machine code or a call to a helper.
  SourcePositionTable* source_positions = nullptr;
  MachineGraph* mcgraph = zone.New<MachineGraph>(
      zone.New<Graph>(&zone), zone.New<CommonOperatorBuilder>(&zone),
      zone.New<MachineOperatorBuilder>(
          &zone, MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements()));

  wasm::CompilationEnv env(
      nullptr, wasm::kNoBoundsChecks,
      wasm::RuntimeExceptionSupport::kNoRuntimeExceptionSupport,
      wasm::WasmFeatures::All(), wasm::kNoDynamicTiering);

  WasmGraphBuilder builder(&env, mcgraph->zone(), mcgraph, sig,
                           source_positions);

  // Set up the graph start.
  builder.Start(static_cast<int>(sig->parameter_count() + 1 + 1));

  // Generate either a unop or a binop.
  Node* node = nullptr;
  const char* debug_name = "WasmMathIntrinsic";
  auto opcode = GetMathIntrinsicOpcode(kind, &debug_name);
  switch (sig->parameter_count()) {
    case 1:
      node = builder.Unop(opcode, builder.Param(1));
      break;
    case 2:
      node = builder.Binop(opcode, builder.Param(1), builder.Param(2));
      break;
    default:
      UNREACHABLE();
  }

  builder.Return(node);

  // Run the compiler pipeline to generate machine code.
  auto call_descriptor = GetWasmCallDescriptor(&zone, sig);
  if (mcgraph->machine()->Is32()) {
    call_descriptor = GetI32WasmCallDescriptor(&zone, call_descriptor);
  }

  // The code does not call to JS, but conceptually it is an import wrapper,
  // hence use {WASM_TO_JS_FUNCTION} here.
  // TODO(wasm): Rename this to {WASM_IMPORT_CALL}?
  return Pipeline::GenerateCodeForWasmNativeStub(
      call_descriptor, mcgraph, CodeKind::WASM_TO_JS_FUNCTION, debug_name,
      WasmStubAssemblerOptions(), source_positions);
}

}  // namespace

wasm::WasmCompilationResult CompileWasmImportCallWrapper(
    wasm::CompilationEnv* env, WasmImportCallKind kind,
    const wasm::FunctionSig* sig, bool source_positions, int expected_arity,
    wasm::Suspend suspend) {
  DCHECK_NE(WasmImportCallKind::kLinkError, kind);
  DCHECK_NE(WasmImportCallKind::kWasmToWasm, kind);
  DCHECK_NE(WasmImportCallKind::kWasmToJSFastApi, kind);

  // Check for math intrinsics first.
  if (v8_flags.wasm_math_intrinsics &&
      kind >= WasmImportCallKind::kFirstMathIntrinsic &&
      kind <= WasmImportCallKind::kLastMathIntrinsic) {
    return CompileWasmMathIntrinsic(kind, sig);
  }

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileWasmImportCallWrapper");
  base::TimeTicks start_time;
  if (V8_UNLIKELY(v8_flags.trace_wasm_compilation_times)) {
    start_time = base::TimeTicks::Now();
  }

  //----------------------------------------------------------------------------
  // Create the Graph
  //----------------------------------------------------------------------------
  Zone zone(wasm::GetWasmEngine()->allocator(), ZONE_NAME, kCompressGraphZone);
  Graph* graph = zone.New<Graph>(&zone);
  CommonOperatorBuilder* common = zone.New<CommonOperatorBuilder>(&zone);
  MachineOperatorBuilder* machine = zone.New<MachineOperatorBuilder>(
      &zone, MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone.New<MachineGraph>(graph, common, machine);

  SourcePositionTable* source_position_table =
      source_positions ? zone.New<SourcePositionTable>(graph) : nullptr;

  WasmWrapperGraphBuilder builder(
      &zone, mcgraph, sig, env->module,
      WasmGraphBuilder::kWasmApiFunctionRefMode, nullptr, source_position_table,
      StubCallMode::kCallWasmRuntimeStub, env->enabled_features);
  builder.BuildWasmToJSWrapper(kind, expected_arity, suspend);

  // Build a name in the form "wasm-to-js-<kind>-<signature>".
  constexpr size_t kMaxNameLen = 128;
  char func_name[kMaxNameLen];
  int name_prefix_len = SNPrintF(base::VectorOf(func_name, kMaxNameLen),
                                 "wasm-to-js-%d-", static_cast<int>(kind));
  PrintSignature(base::VectorOf(func_name, kMaxNameLen) + name_prefix_len, sig,
                 '-');

  // Schedule and compile to machine code.
  CallDescriptor* incoming =
      GetWasmCallDescriptor(&zone, sig, WasmCallKind::kWasmImportWrapper);
  if (machine->Is32()) {
    incoming = GetI32WasmCallDescriptor(&zone, incoming);
  }
  wasm::WasmCompilationResult result = Pipeline::GenerateCodeForWasmNativeStub(
      incoming, mcgraph, CodeKind::WASM_TO_JS_FUNCTION, func_name,
      WasmStubAssemblerOptions(), source_position_table);

  if (V8_UNLIKELY(v8_flags.trace_wasm_compilation_times)) {
    base::TimeDelta time = base::TimeTicks::Now() - start_time;
    int codesize = result.code_desc.body_size();
    StdoutStream{} << "Compiled WasmToJS wrapper " << func_name << ", took "
                   << time.InMilliseconds() << " ms; codesize " << codesize
                   << std::endl;
  }

  return result;
}

wasm::WasmCode* CompileWasmCapiCallWrapper(wasm::NativeModule* native_module,
                                           const wasm::FunctionSig* sig) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileWasmCapiFunction");

  Zone zone(wasm::GetWasmEngine()->allocator(), ZONE_NAME, kCompressGraphZone);

  // TODO(jkummerow): Extract common code into helper method.
  SourcePositionTable* source_positions = nullptr;
  MachineGraph* mcgraph = zone.New<MachineGraph>(
      zone.New<Graph>(&zone), zone.New<CommonOperatorBuilder>(&zone),
      zone.New<MachineOperatorBuilder>(
          &zone, MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements()));

  WasmWrapperGraphBuilder builder(
      &zone, mcgraph, sig, native_module->module(),
      WasmGraphBuilder::kWasmApiFunctionRefMode, nullptr, source_positions,
      StubCallMode::kCallWasmRuntimeStub, native_module->enabled_features());

  builder.BuildCapiCallWrapper();

  // Run the compiler pipeline to generate machine code.
  CallDescriptor* call_descriptor =
      GetWasmCallDescriptor(&zone, sig, WasmCallKind::kWasmCapiFunction);
  if (mcgraph->machine()->Is32()) {
    call_descriptor = GetI32WasmCallDescriptor(&zone, call_descriptor);
  }

  const char* debug_name = "WasmCapiCall";
  wasm::WasmCompilationResult result = Pipeline::GenerateCodeForWasmNativeStub(
      call_descriptor, mcgraph, CodeKind::WASM_TO_CAPI_FUNCTION, debug_name,
      WasmStubAssemblerOptions(), source_positions);
  wasm::WasmCode* published_code;
  {
    wasm::CodeSpaceWriteScope code_space_write_scope(native_module);
    std::unique_ptr<wasm::WasmCode> wasm_code = native_module->AddCode(
        wasm::kAnonymousFuncIndex, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots,
        result.protected_instructions_data.as_vector(),
        result.source_positions.as_vector(), wasm::WasmCode::kWasmToCapiWrapper,
        wasm::ExecutionTier::kNone, wasm::kNoDebugging);
    published_code = native_module->PublishCode(std::move(wasm_code));
  }
  return published_code;
}

wasm::WasmCode* CompileWasmJSFastCallWrapper(wasm::NativeModule* native_module,
                                             const wasm::FunctionSig* sig,
                                             Handle<JSReceiver> callable) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileWasmJSFastCallWrapper");

  Zone zone(wasm::GetWasmEngine()->allocator(), ZONE_NAME, kCompressGraphZone);

  // TODO(jkummerow): Extract common code into helper method.
  SourcePositionTable* source_positions = nullptr;
  MachineGraph* mcgraph = zone.New<MachineGraph>(
      zone.New<Graph>(&zone), zone.New<CommonOperatorBuilder>(&zone),
      zone.New<MachineOperatorBuilder>(
          &zone, MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements()));

  WasmWrapperGraphBuilder builder(
      &zone, mcgraph, sig, native_module->module(),
      WasmGraphBuilder::kWasmApiFunctionRefMode, nullptr, source_positions,
      StubCallMode::kCallWasmRuntimeStub, native_module->enabled_features());

  // Set up the graph start.
  int param_count = static_cast<int>(sig->parameter_count()) +
                    1 /* offset for first parameter index being -1 */ +
                    1 /* Wasm instance */ + 1 /* kExtraCallableParam */;
  builder.Start(param_count);
  builder.BuildJSFastApiCallWrapper(callable);

  // Run the compiler pipeline to generate machine code.
  CallDescriptor* call_descriptor =
      GetWasmCallDescriptor(&zone, sig, WasmCallKind::kWasmImportWrapper);
  if (mcgraph->machine()->Is32()) {
    call_descriptor = GetI32WasmCallDescriptor(&zone, call_descriptor);
  }

  const char* debug_name = "WasmJSFastApiCall";
  wasm::WasmCompilationResult result = Pipeline::GenerateCodeForWasmNativeStub(
      call_descriptor, mcgraph, CodeKind::WASM_TO_JS_FUNCTION, debug_name,
      WasmStubAssemblerOptions(), source_positions);
  {
    wasm::CodeSpaceWriteScope code_space_write_scope(native_module);
    std::unique_ptr<wasm::WasmCode> wasm_code = native_module->AddCode(
        wasm::kAnonymousFuncIndex, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots,
        result.protected_instructions_data.as_vector(),
        result.source_positions.as_vector(), wasm::WasmCode::kWasmToJsWrapper,
        wasm::ExecutionTier::kNone, wasm::kNoDebugging);
    return native_module->PublishCode(std::move(wasm_code));
  }
}

MaybeHandle<Code> CompileWasmToJSWrapper(Isolate* isolate,
                                         const wasm::FunctionSig* sig,
                                         WasmImportCallKind kind,
                                         int expected_arity,
                                         wasm::Suspend suspend) {
  std::unique_ptr<Zone> zone = std::make_unique<Zone>(
      isolate->allocator(), ZONE_NAME, kCompressGraphZone);

  // Create the Graph
  Graph* graph = zone->New<Graph>(zone.get());
  CommonOperatorBuilder* common = zone->New<CommonOperatorBuilder>(zone.get());
  MachineOperatorBuilder* machine = zone->New<MachineOperatorBuilder>(
      zone.get(), MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone->New<MachineGraph>(graph, common, machine);

  WasmWrapperGraphBuilder builder(zone.get(), mcgraph, sig, nullptr,
                                  WasmGraphBuilder::kWasmApiFunctionRefMode,
                                  nullptr, nullptr,
                                  StubCallMode::kCallBuiltinPointer,
                                  wasm::WasmFeatures::FromIsolate(isolate));
  builder.BuildWasmToJSWrapper(kind, expected_arity, suspend);

  // Build a name in the form "wasm-to-js-<kind>-<signature>".
  constexpr size_t kMaxNameLen = 128;
  constexpr size_t kNamePrefixLen = 11;
  auto name_buffer = std::unique_ptr<char[]>(new char[kMaxNameLen]);
  memcpy(name_buffer.get(), "wasm-to-js:", kNamePrefixLen);
  PrintSignature(
      base::VectorOf(name_buffer.get(), kMaxNameLen) + kNamePrefixLen, sig);

  // Generate the call descriptor.
  CallDescriptor* incoming =
      GetWasmCallDescriptor(zone.get(), sig, WasmCallKind::kWasmImportWrapper);

  // Run the compilation job synchronously.
  std::unique_ptr<TurbofanCompilationJob> job(
      Pipeline::NewWasmHeapStubCompilationJob(
          isolate, incoming, std::move(zone), graph,
          CodeKind::WASM_TO_JS_FUNCTION, std::move(name_buffer),
          AssemblerOptions::Default(isolate)));

  // Compile the wrapper
  if (job->ExecuteJob(isolate->counters()->runtime_call_stats()) ==
          CompilationJob::FAILED ||
      job->FinalizeJob(isolate) == CompilationJob::FAILED) {
    return Handle<Code>();
  }
  Handle<Code> code = job->compilation_info()->code();
  return code;
}

MaybeHandle<Code> CompileJSToJSWrapper(Isolate* isolate,
                                       const wasm::FunctionSig* sig,
                                       const wasm::WasmModule* module) {
  std::unique_ptr<Zone> zone = std::make_unique<Zone>(
      isolate->allocator(), ZONE_NAME, kCompressGraphZone);
  Graph* graph = zone->New<Graph>(zone.get());
  CommonOperatorBuilder* common = zone->New<CommonOperatorBuilder>(zone.get());
  MachineOperatorBuilder* machine = zone->New<MachineOperatorBuilder>(
      zone.get(), MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone->New<MachineGraph>(graph, common, machine);

  WasmWrapperGraphBuilder builder(zone.get(), mcgraph, sig, module,
                                  WasmGraphBuilder::kNoSpecialParameterMode,
                                  isolate, nullptr,
                                  StubCallMode::kCallBuiltinPointer,
                                  wasm::WasmFeatures::FromIsolate(isolate));
  builder.BuildJSToJSWrapper();

  int wasm_count = static_cast<int>(sig->parameter_count());
  CallDescriptor* incoming = Linkage::GetJSCallDescriptor(
      zone.get(), false, wasm_count + 1, CallDescriptor::kNoFlags);

  // Build a name in the form "js-to-js:<params>:<returns>".
  constexpr size_t kMaxNameLen = 128;
  constexpr size_t kNamePrefixLen = 9;
  auto name_buffer = std::unique_ptr<char[]>(new char[kMaxNameLen]);
  memcpy(name_buffer.get(), "js-to-js:", kNamePrefixLen);
  PrintSignature(
      base::VectorOf(name_buffer.get(), kMaxNameLen) + kNamePrefixLen, sig);

  // Run the compilation job synchronously.
  std::unique_ptr<TurbofanCompilationJob> job(
      Pipeline::NewWasmHeapStubCompilationJob(
          isolate, incoming, std::move(zone), graph,
          CodeKind::JS_TO_JS_FUNCTION, std::move(name_buffer),
          AssemblerOptions::Default(isolate)));

  if (job->ExecuteJob(isolate->counters()->runtime_call_stats()) ==
          CompilationJob::FAILED ||
      job->FinalizeJob(isolate) == CompilationJob::FAILED) {
    return {};
  }
  Handle<Code> code = job->compilation_info()->code();

  return code;
}

Handle<CodeT> CompileCWasmEntry(Isolate* isolate, const wasm::FunctionSig* sig,
                                const wasm::WasmModule* module) {
  std::unique_ptr<Zone> zone = std::make_unique<Zone>(
      isolate->allocator(), ZONE_NAME, kCompressGraphZone);
  Graph* graph = zone->New<Graph>(zone.get());
  CommonOperatorBuilder* common = zone->New<CommonOperatorBuilder>(zone.get());
  MachineOperatorBuilder* machine = zone->New<MachineOperatorBuilder>(
      zone.get(), MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone->New<MachineGraph>(graph, common, machine);

  WasmWrapperGraphBuilder builder(zone.get(), mcgraph, sig, module,
                                  WasmGraphBuilder::kWasmApiFunctionRefMode,
                                  nullptr, nullptr,
                                  StubCallMode::kCallBuiltinPointer,
                                  wasm::WasmFeatures::FromIsolate(isolate));
  builder.BuildCWasmEntry();

  // Schedule and compile to machine code.
  MachineType sig_types[] = {MachineType::Pointer(),    // return
                             MachineType::Pointer(),    // target
                             MachineType::AnyTagged(),  // object_ref
                             MachineType::Pointer(),    // argv
                             MachineType::Pointer()};   // c_entry_fp
  MachineSignature incoming_sig(1, 4, sig_types);
  // Traps need the root register, for TailCallRuntime to call
  // Runtime::kThrowWasmError.
  CallDescriptor::Flags flags = CallDescriptor::kInitializeRootRegister;
  CallDescriptor* incoming =
      Linkage::GetSimplifiedCDescriptor(zone.get(), &incoming_sig, flags);

  // Build a name in the form "c-wasm-entry:<params>:<returns>".
  constexpr size_t kMaxNameLen = 128;
  constexpr size_t kNamePrefixLen = 13;
  auto name_buffer = std::unique_ptr<char[]>(new char[kMaxNameLen]);
  memcpy(name_buffer.get(), "c-wasm-entry:", kNamePrefixLen);
  PrintSignature(
      base::VectorOf(name_buffer.get(), kMaxNameLen) + kNamePrefixLen, sig);

  // Run the compilation job synchronously.
  std::unique_ptr<TurbofanCompilationJob> job(
      Pipeline::NewWasmHeapStubCompilationJob(
          isolate, incoming, std::move(zone), graph, CodeKind::C_WASM_ENTRY,
          std::move(name_buffer), AssemblerOptions::Default(isolate)));

  CHECK_NE(job->ExecuteJob(isolate->counters()->runtime_call_stats(), nullptr),
           CompilationJob::FAILED);
  CHECK_NE(job->FinalizeJob(isolate), CompilationJob::FAILED);

  return ToCodeT(job->compilation_info()->code(), isolate);
}

namespace {

bool BuildGraphForWasmFunction(wasm::CompilationEnv* env,
                               const wasm::FunctionBody& func_body,
                               int func_index, wasm::WasmFeatures* detected,
                               MachineGraph* mcgraph,
                               std::vector<compiler::WasmLoopInfo>* loop_infos,
                               NodeOriginTable* node_origins,
                               SourcePositionTable* source_positions) {
  // Create a TF graph during decoding.
  WasmGraphBuilder builder(env, mcgraph->zone(), mcgraph, func_body.sig,
                           source_positions);
  auto* allocator = wasm::GetWasmEngine()->allocator();
  wasm::VoidResult graph_construction_result = wasm::BuildTFGraph(
      allocator, env->enabled_features, env->module, &builder, detected,
      func_body, loop_infos, node_origins, func_index, wasm::kRegularFunction);
  if (graph_construction_result.failed()) {
    if (v8_flags.trace_wasm_compiler) {
      StdoutStream{} << "Compilation failed: "
                     << graph_construction_result.error().message()
                     << std::endl;
    }
    return false;
  }

  auto sig = CreateMachineSignature(mcgraph->zone(), func_body.sig,
                                    WasmGraphBuilder::kCalledFromWasm);
  builder.LowerInt64(sig);

  return true;
}

base::Vector<const char> GetDebugName(Zone* zone,
                                      const wasm::WasmModule* module,
                                      const wasm::WireBytesStorage* wire_bytes,
                                      int index) {
  base::Optional<wasm::ModuleWireBytes> module_bytes =
      wire_bytes->GetModuleBytes();
  if (module_bytes.has_value() &&
      (v8_flags.trace_turbo || v8_flags.trace_turbo_scheduled ||
       v8_flags.trace_turbo_graph || v8_flags.print_wasm_code)) {
    wasm::WireBytesRef name = module->lazily_generated_names.LookupFunctionName(
        module_bytes.value(), index);
    if (!name.is_empty()) {
      int name_len = name.length();
      char* index_name = zone->NewArray<char>(name_len);
      memcpy(index_name, module_bytes->start() + name.offset(), name_len);
      return base::Vector<const char>(index_name, name_len);
    }
  }

  constexpr int kBufferLength = 24;

  base::EmbeddedVector<char, kBufferLength> name_vector;
  int name_len = SNPrintF(name_vector, "wasm-function#%d", index);
  DCHECK(name_len > 0 && name_len < name_vector.length());

  char* index_name = zone->NewArray<char>(name_len);
  memcpy(index_name, name_vector.begin(), name_len);
  return base::Vector<const char>(index_name, name_len);
}

}  // namespace

wasm::WasmCompilationResult ExecuteTurbofanWasmCompilation(
    wasm::CompilationEnv* env, const wasm::WireBytesStorage* wire_byte_storage,
    const wasm::FunctionBody& func_body, int func_index, Counters* counters,
    wasm::AssemblerBufferCache* buffer_cache, wasm::WasmFeatures* detected) {
  // Check that we do not accidentally compile a Wasm function to TurboFan if
  // --liftoff-only is set.
  DCHECK(!v8_flags.liftoff_only);

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileTopTier", "func_index", func_index, "body_size",
               func_body.end - func_body.start);
  Zone zone(wasm::GetWasmEngine()->allocator(), ZONE_NAME, kCompressGraphZone);
  MachineGraph* mcgraph = zone.New<MachineGraph>(
      zone.New<Graph>(&zone), zone.New<CommonOperatorBuilder>(&zone),
      zone.New<MachineOperatorBuilder>(
          &zone, MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements()));

  OptimizedCompilationInfo info(
      GetDebugName(&zone, env->module, wire_byte_storage, func_index), &zone,
      CodeKind::WASM_FUNCTION);
  if (env->runtime_exception_support) {
    info.set_wasm_runtime_exception_support();
  }

  if (v8_flags.experimental_wasm_gc) info.set_allocation_folding();

  if (info.trace_turbo_json()) {
    TurboCfgFile tcf;
    tcf << AsC1VCompilation(&info);
  }

  NodeOriginTable* node_origins =
      info.trace_turbo_json() ? zone.New<NodeOriginTable>(mcgraph->graph())
                              : nullptr;
  SourcePositionTable* source_positions =
      mcgraph->zone()->New<SourcePositionTable>(mcgraph->graph());

  std::vector<WasmLoopInfo> loop_infos;

  wasm::WasmFeatures unused_detected_features;
  if (!detected) detected = &unused_detected_features;
  if (!BuildGraphForWasmFunction(env, func_body, func_index, detected, mcgraph,
                                 &loop_infos, node_origins, source_positions)) {
    return wasm::WasmCompilationResult{};
  }

  if (node_origins) {
    node_origins->AddDecorator();
  }

  // Run the compiler pipeline to generate machine code.
  auto call_descriptor = GetWasmCallDescriptor(&zone, func_body.sig);
  if (mcgraph->machine()->Is32()) {
    call_descriptor = GetI32WasmCallDescriptor(&zone, call_descriptor);
  }

  if (ContainsSimd(func_body.sig) && !CpuFeatures::SupportsWasmSimd128()) {
    // Fail compilation if hardware does not support SIMD.
    return wasm::WasmCompilationResult{};
  }

  Pipeline::GenerateCodeForWasmFunction(&info, env, wire_byte_storage, mcgraph,
                                        call_descriptor, source_positions,
                                        node_origins, func_body, env->module,
                                        func_index, &loop_infos, buffer_cache);

  if (counters) {
    int zone_bytes =
        static_cast<int>(mcgraph->graph()->zone()->allocation_size());
    counters->wasm_compile_function_peak_memory_bytes()->AddSample(zone_bytes);
    if (func_body.end - func_body.start >= 100 * KB) {
      counters->wasm_compile_huge_function_peak_memory_bytes()->AddSample(
          zone_bytes);
    }
  }
  // If we tiered up only one function for debugging, dump statistics
  // immediately.
  if (V8_UNLIKELY(v8_flags.turbo_stats_wasm &&
                  v8_flags.wasm_tier_up_filter >= 0)) {
    wasm::GetWasmEngine()->DumpTurboStatistics();
  }
  auto result = info.ReleaseWasmCompilationResult();
  CHECK_NOT_NULL(result);  // Compilation expected to succeed.
  DCHECK_EQ(wasm::ExecutionTier::kTurbofan, result->result_tier);
  return std::move(*result);
}

namespace {
// Helper for allocating either an GP or FP reg, or the next stack slot.
class LinkageLocationAllocator {
 public:
  template <size_t kNumGpRegs, size_t kNumFpRegs>
  constexpr LinkageLocationAllocator(const Register (&gp)[kNumGpRegs],
                                     const DoubleRegister (&fp)[kNumFpRegs],
                                     int slot_offset)
      : allocator_(wasm::LinkageAllocator(gp, fp)), slot_offset_(slot_offset) {}

  LinkageLocation Next(MachineRepresentation rep) {
    MachineType type = MachineType::TypeForRepresentation(rep);
    if (IsFloatingPoint(rep)) {
      if (allocator_.CanAllocateFP(rep)) {
        int reg_code = allocator_.NextFpReg(rep);
        return LinkageLocation::ForRegister(reg_code, type);
      }
    } else if (allocator_.CanAllocateGP()) {
      int reg_code = allocator_.NextGpReg();
      return LinkageLocation::ForRegister(reg_code, type);
    }
    // Cannot use register; use stack slot.
    int index = -1 - (slot_offset_ + allocator_.NextStackSlot(rep));
    return LinkageLocation::ForCallerFrameSlot(index, type);
  }

  int NumStackSlots() const { return allocator_.NumStackSlots(); }
  void EndSlotArea() { allocator_.EndSlotArea(); }

 private:
  wasm::LinkageAllocator allocator_;
  // Since params and returns are in different stack frames, we must allocate
  // them separately. Parameter slots don't need an offset, but return slots
  // must be offset to just before the param slots, using this |slot_offset_|.
  int slot_offset_;
};

const MachineSignature* FunctionSigToMachineSig(Zone* zone,
                                                const wasm::FunctionSig* fsig) {
  MachineSignature::Builder builder(zone, fsig->return_count(),
                                    fsig->parameter_count());
  for (wasm::ValueType ret : fsig->returns()) {
    builder.AddReturn(ret.machine_type());
  }
  for (wasm::ValueType param : fsig->parameters()) {
    builder.AddParam(param.machine_type());
  }
  return builder.Build();
}

LocationSignature* BuildLocations(Zone* zone, const MachineSignature* sig,
                                  bool extra_callable_param,
                                  int* parameter_slots, int* return_slots) {
  int extra_params = extra_callable_param ? 2 : 1;
  LocationSignature::Builder locations(zone, sig->return_count(),
                                       sig->parameter_count() + extra_params);

  // Add register and/or stack parameter(s).
  LinkageLocationAllocator params(
      wasm::kGpParamRegisters, wasm::kFpParamRegisters, 0 /* no slot offset */);

  // The instance object.
  locations.AddParam(params.Next(MachineRepresentation::kTaggedPointer));
  const size_t param_offset = 1;  // Actual params start here.

  // Parameters are separated into two groups (first all untagged, then all
  // tagged parameters). This allows for easy iteration of tagged parameters
  // during frame iteration.
  const size_t parameter_count = sig->parameter_count();
  for (size_t i = 0; i < parameter_count; i++) {
    MachineRepresentation param = sig->GetParam(i).representation();
    // Skip tagged parameters (e.g. any-ref).
    if (IsAnyTagged(param)) continue;
    auto l = params.Next(param);
    locations.AddParamAt(i + param_offset, l);
  }

  // End the untagged area, so tagged slots come after.
  params.EndSlotArea();

  for (size_t i = 0; i < parameter_count; i++) {
    MachineRepresentation param = sig->GetParam(i).representation();
    // Skip untagged parameters.
    if (!IsAnyTagged(param)) continue;
    auto l = params.Next(param);
    locations.AddParamAt(i + param_offset, l);
  }

  // Import call wrappers have an additional (implicit) parameter, the callable.
  // For consistency with JS, we use the JSFunction register.
  if (extra_callable_param) {
    locations.AddParam(LinkageLocation::ForRegister(
        kJSFunctionRegister.code(), MachineType::TaggedPointer()));
  }

  *parameter_slots = AddArgumentPaddingSlots(params.NumStackSlots());

  // Add return location(s).
  LinkageLocationAllocator rets(wasm::kGpReturnRegisters,
                                wasm::kFpReturnRegisters, *parameter_slots);

  const size_t return_count = locations.return_count_;
  for (size_t i = 0; i < return_count; i++) {
    MachineRepresentation ret = sig->GetReturn(i).representation();
    locations.AddReturn(rets.Next(ret));
  }

  *return_slots = rets.NumStackSlots();

  return locations.Build();
}

LocationSignature* BuildLocations(Zone* zone, const wasm::FunctionSig* fsig,
                                  bool extra_callable_param,
                                  int* parameter_slots, int* return_slots) {
  return BuildLocations(zone, FunctionSigToMachineSig(zone, fsig),
                        extra_callable_param, parameter_slots, return_slots);
}
}  // namespace

// General code uses the above configuration data.
CallDescriptor* GetWasmCallDescriptor(Zone* zone, const wasm::FunctionSig* fsig,
                                      WasmCallKind call_kind,
                                      bool need_frame_state) {
  // The extra here is to accomodate the instance object as first parameter
  // and, when specified, the additional callable.
  bool extra_callable_param =
      call_kind == kWasmImportWrapper || call_kind == kWasmCapiFunction;

  int parameter_slots;
  int return_slots;
  LocationSignature* location_sig = BuildLocations(
      zone, fsig, extra_callable_param, &parameter_slots, &return_slots);

  const RegList kCalleeSaveRegisters;
  const DoubleRegList kCalleeSaveFPRegisters;

  // The target for wasm calls is always a code object.
  MachineType target_type = MachineType::Pointer();
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister(target_type);

  CallDescriptor::Kind descriptor_kind;
  if (call_kind == kWasmFunction) {
    descriptor_kind = CallDescriptor::kCallWasmFunction;
  } else if (call_kind == kWasmImportWrapper) {
    descriptor_kind = CallDescriptor::kCallWasmImportWrapper;
  } else {
    DCHECK_EQ(call_kind, kWasmCapiFunction);
    descriptor_kind = CallDescriptor::kCallWasmCapiFunction;
  }

  CallDescriptor::Flags flags = need_frame_state
                                    ? CallDescriptor::kNeedsFrameState
                                    : CallDescriptor::kNoFlags;
  return zone->New<CallDescriptor>(       // --
      descriptor_kind,                    // kind
      target_type,                        // target MachineType
      target_loc,                         // target location
      location_sig,                       // location_sig
      parameter_slots,                    // parameter slot count
      compiler::Operator::kNoProperties,  // properties
      kCalleeSaveRegisters,               // callee-saved registers
      kCalleeSaveFPRegisters,             // callee-saved fp regs
      flags,                              // flags
      "wasm-call",                        // debug name
      StackArgumentOrder::kDefault,       // order of the arguments in the stack
      RegList{},                          // allocatable registers
      return_slots);                      // return slot count
}

namespace {

CallDescriptor* ReplaceTypeInCallDescriptorWith(
    Zone* zone, const CallDescriptor* call_descriptor, size_t num_replacements,
    MachineType from, MachineType to) {
  // The last parameter may be the special callable parameter. In that case we
  // have to preserve it as the last parameter, i.e. we allocate it in the new
  // location signature again in the same register.
  bool extra_callable_param =
      (call_descriptor->GetInputLocation(call_descriptor->InputCount() - 1) ==
       LinkageLocation::ForRegister(kJSFunctionRegister.code(),
                                    MachineType::TaggedPointer()));

  size_t return_count = call_descriptor->ReturnCount();
  // To recover the function parameter count, disregard the instance parameter,
  // and the extra callable parameter if present.
  size_t parameter_count =
      call_descriptor->ParameterCount() - (extra_callable_param ? 2 : 1);

  // Precompute if the descriptor contains {from}.
  bool needs_change = false;
  for (size_t i = 0; !needs_change && i < return_count; i++) {
    needs_change = call_descriptor->GetReturnType(i) == from;
  }
  for (size_t i = 1; !needs_change && i < parameter_count + 1; i++) {
    needs_change = call_descriptor->GetParameterType(i) == from;
  }
  if (!needs_change) return const_cast<CallDescriptor*>(call_descriptor);

  std::vector<MachineType> reps;

  for (size_t i = 0, limit = return_count; i < limit; i++) {
    MachineType initial_type = call_descriptor->GetReturnType(i);
    if (initial_type == from) {
      for (size_t j = 0; j < num_replacements; j++) reps.push_back(to);
      return_count += num_replacements - 1;
    } else {
      reps.push_back(initial_type);
    }
  }

  // Disregard the instance (first) parameter.
  for (size_t i = 1, limit = parameter_count + 1; i < limit; i++) {
    MachineType initial_type = call_descriptor->GetParameterType(i);
    if (initial_type == from) {
      for (size_t j = 0; j < num_replacements; j++) reps.push_back(to);
      parameter_count += num_replacements - 1;
    } else {
      reps.push_back(initial_type);
    }
  }

  MachineSignature sig(return_count, parameter_count, reps.data());

  int parameter_slots;
  int return_slots;
  LocationSignature* location_sig = BuildLocations(
      zone, &sig, extra_callable_param, &parameter_slots, &return_slots);

  return zone->New<CallDescriptor>(               // --
      call_descriptor->kind(),                    // kind
      call_descriptor->GetInputType(0),           // target MachineType
      call_descriptor->GetInputLocation(0),       // target location
      location_sig,                               // location_sig
      parameter_slots,                            // parameter slot count
      call_descriptor->properties(),              // properties
      call_descriptor->CalleeSavedRegisters(),    // callee-saved registers
      call_descriptor->CalleeSavedFPRegisters(),  // callee-saved fp regs
      call_descriptor->flags(),                   // flags
      call_descriptor->debug_name(),              // debug name
      call_descriptor->GetStackArgumentOrder(),   // stack order
      call_descriptor->AllocatableRegisters(),    // allocatable registers
      return_slots);                              // return slot count
}
}  // namespace

void WasmGraphBuilder::StoreCallCount(Node* call, int count) {
  mcgraph()->StoreCallCount(call->id(), count);
}

void WasmGraphBuilder::ReserveCallCounts(size_t num_call_instructions) {
  mcgraph()->ReserveCallCounts(num_call_instructions);
}

CallDescriptor* GetI32WasmCallDescriptor(
    Zone* zone, const CallDescriptor* call_descriptor) {
  return ReplaceTypeInCallDescriptorWith(
      zone, call_descriptor, 2, MachineType::Int64(), MachineType::Int32());
}

namespace {
const wasm::FunctionSig* ReplaceTypeInSig(Zone* zone,
                                          const wasm::FunctionSig* sig,
                                          wasm::ValueType from,
                                          wasm::ValueType to,
                                          size_t num_replacements) {
  size_t param_occurences =
      std::count(sig->parameters().begin(), sig->parameters().end(), from);
  size_t return_occurences =
      std::count(sig->returns().begin(), sig->returns().end(), from);
  if (param_occurences == 0 && return_occurences == 0) return sig;

  wasm::FunctionSig::Builder builder(
      zone, sig->return_count() + return_occurences * (num_replacements - 1),
      sig->parameter_count() + param_occurences * (num_replacements - 1));

  for (wasm::ValueType ret : sig->returns()) {
    if (ret == from) {
      for (size_t i = 0; i < num_replacements; i++) builder.AddReturn(to);
    } else {
      builder.AddReturn(ret);
    }
  }

  for (wasm::ValueType param : sig->parameters()) {
    if (param == from) {
      for (size_t i = 0; i < num_replacements; i++) builder.AddParam(to);
    } else {
      builder.AddParam(param);
    }
  }

  return builder.Build();
}
}  // namespace

const wasm::FunctionSig* GetI32Sig(Zone* zone, const wasm::FunctionSig* sig) {
  return ReplaceTypeInSig(zone, sig, wasm::kWasmI64, wasm::kWasmI32, 2);
}
AssemblerOptions WasmAssemblerOptions() {
  AssemblerOptions options;
  // Relocation info required to serialize {WasmCode} for proper functions.
  options.record_reloc_info_for_serialization = true;
  options.enable_root_relative_access = false;
  return options;
}

AssemblerOptions WasmStubAssemblerOptions() {
  AssemblerOptions options;
  // Relocation info not necessary because stubs are not serialized.
  options.record_reloc_info_for_serialization = false;
  options.enable_root_relative_access = false;
  return options;
}

#undef FATAL_UNSUPPORTED_OPCODE
#undef WASM_INSTANCE_OBJECT_SIZE
#undef LOAD_INSTANCE_FIELD
#undef LOAD_MUTABLE_INSTANCE_FIELD
#undef LOAD_ROOT

}  // namespace compiler
}  // namespace internal
}  // namespace v8
