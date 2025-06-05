// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/recreate-schedule.h"

#include "src/base/logging.h"
#include "src/base/safe_conversions.h"
#include "src/base/small-vector.h"
#include "src/base/template-utils.h"
#include "src/base/vector.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/turboshaft/deopt-data.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/utils/utils.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::compiler::turboshaft {

namespace {

struct ScheduleBuilder {
  const Graph& input_graph;
  CallDescriptor* call_descriptor;
  Zone* graph_zone;
  Zone* phase_zone;
  SourcePositionTable* source_positions;
  NodeOriginTable* origins;

  const size_t node_count_estimate =
      static_cast<size_t>(1.1 * input_graph.op_id_count());
  Schedule* const schedule =
      graph_zone->New<Schedule>(graph_zone, node_count_estimate);
  compiler::Graph* const tf_graph =
      graph_zone->New<compiler::Graph>(graph_zone);
  compiler::MachineOperatorBuilder machine{
      graph_zone, MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements()};
  compiler::CommonOperatorBuilder common{graph_zone};
  compiler::SimplifiedOperatorBuilder simplified{graph_zone};
  compiler::BasicBlock* current_block = schedule->start();
  const Block* current_input_block = nullptr;
  ZoneUnorderedMap<int, Node*> parameters{phase_zone};
  ZoneUnorderedMap<int, Node*> osr_values{phase_zone};
  std::vector<BasicBlock*> blocks = {};
  std::vector<Node*> nodes{input_graph.op_id_count()};
  std::vector<std::pair<Node*, OpIndex>> loop_phis = {};

  RecreateScheduleResult Run();
  Node* MakeNode(const Operator* op, base::Vector<Node* const> inputs);
  Node* MakeNode(const Operator* op, std::initializer_list<Node*> inputs) {
    return MakeNode(op, base::VectorOf(inputs));
  }
  Node* AddNode(const Operator* op, base::Vector<Node* const> inputs);
  Node* AddNode(const Operator* op, std::initializer_list<Node*> inputs) {
    return AddNode(op, base::VectorOf(inputs));
  }
  Node* GetNode(OpIndex i) { return nodes[i.id()]; }
  BasicBlock* GetBlock(const Block& block) {
    return blocks[block.index().id()];
  }
  Node* IntPtrConstant(intptr_t value) {
    return AddNode(machine.Is64() ? common.Int64Constant(value)
                                  : common.Int32Constant(
                                        base::checked_cast<int32_t>(value)),
                   {});
  }
  Node* IntPtrAdd(Node* a, Node* b) {
    return AddNode(machine.Is64() ? machine.Int64Add() : machine.Int32Add(),
                   {a, b});
  }
  Node* IntPtrShl(Node* a, Node* b) {
    return AddNode(machine.Is64() ? machine.Word64Shl() : machine.Word32Shl(),
                   {a, b});
  }
  void ProcessOperation(const Operation& op);
#define DECL_PROCESS_OPERATION(Name) Node* ProcessOperation(const Name##Op& op);
  TURBOSHAFT_OPERATION_LIST(DECL_PROCESS_OPERATION)
#undef DECL_PROCESS_OPERATION

  std::pair<Node*, MachineType> BuildDeoptInput(FrameStateData::Iterator* it);
  Node* BuildStateValues(FrameStateData::Iterator* it, int32_t size);
  Node* BuildTaggedInput(FrameStateData::Iterator* it);
};

Node* ScheduleBuilder::MakeNode(const Operator* op,
                                base::Vector<Node* const> inputs) {
  Node* node = tf_graph->NewNodeUnchecked(op, static_cast<int>(inputs.size()),
                                          inputs.data());
  return node;
}
Node* ScheduleBuilder::AddNode(const Operator* op,
                               base::Vector<Node* const> inputs) {
  DCHECK_NOT_NULL(current_block);
  Node* node = MakeNode(op, inputs);
  schedule->AddNode(current_block, node);
  return node;
}

RecreateScheduleResult ScheduleBuilder::Run() {
  DCHECK_GE(input_graph.block_count(), 1);
  blocks.reserve(input_graph.block_count());
  blocks.push_back(current_block);
  for (size_t i = 1; i < input_graph.block_count(); ++i) {
    blocks.push_back(schedule->NewBasicBlock());
  }
  // The value output count of the start node does not actually matter.
  tf_graph->SetStart(tf_graph->NewNode(common.Start(0)));
  tf_graph->SetEnd(tf_graph->NewNode(common.End(0)));

  for (const Block& block : input_graph.blocks()) {
    current_input_block = &block;
    current_block = GetBlock(block);
    current_block->set_deferred(current_input_block->IsDeferred());
    for (OpIndex op : input_graph.OperationIndices(block)) {
      DCHECK_NOT_NULL(current_block);
      ProcessOperation(input_graph.Get(op));
    }
  }

  for (auto& p : loop_phis) {
    p.first->ReplaceInput(1, GetNode(p.second));
  }

  DCHECK(schedule->rpo_order()->empty());
  Scheduler::ComputeSpecialRPO(phase_zone, schedule);
  Scheduler::GenerateDominatorTree(schedule);
  return {tf_graph, schedule};
}

void ScheduleBuilder::ProcessOperation(const Operation& op) {
  Node* node;
  switch (op.opcode) {
#define SWITCH_CASE(Name)                         \
  case Opcode::k##Name:                           \
    node = ProcessOperation(op.Cast<Name##Op>()); \
    break;
    TURBOSHAFT_OPERATION_LIST(SWITCH_CASE)
#undef SWITCH_CASE
  }
  OpIndex index = input_graph.Index(op);
  DCHECK_LT(index.id(), nodes.size());
  nodes[index.id()] = node;
  if (source_positions->IsEnabled() && node) {
    source_positions->SetSourcePosition(node,
                                        input_graph.source_positions()[index]);
  }
  if (origins && node) {
    origins->SetNodeOrigin(node->id(), index.id());
  }
}

Node* ScheduleBuilder::ProcessOperation(const WordBinopOp& op) {
  using Kind = WordBinopOp::Kind;
  const Operator* o;
  switch (op.rep) {
    case MachineRepresentation::kWord32:
      switch (op.kind) {
        case Kind::kAdd:
          o = machine.Int32Add();
          break;
        case Kind::kSub:
          o = machine.Int32Sub();
          break;
        case Kind::kMul:
          o = machine.Int32Mul();
          break;
        case Kind::kSignedMulOverflownBits:
          o = machine.Int32MulHigh();
          break;
        case Kind::kUnsignedMulOverflownBits:
          o = machine.Uint32MulHigh();
          break;
        case Kind::kSignedDiv:
          o = machine.Int32Div();
          break;
        case Kind::kUnsignedDiv:
          o = machine.Uint32Div();
          break;
        case Kind::kSignedMod:
          o = machine.Int32Mod();
          break;
        case Kind::kUnsignedMod:
          o = machine.Uint32Mod();
          break;
        case Kind::kBitwiseAnd:
          o = machine.Word32And();
          break;
        case Kind::kBitwiseOr:
          o = machine.Word32Or();
          break;
        case Kind::kBitwiseXor:
          o = machine.Word32Xor();
          break;
      }
      break;
    case MachineRepresentation::kWord64:
      switch (op.kind) {
        case Kind::kAdd:
          o = machine.Int64Add();
          break;
        case Kind::kSub:
          o = machine.Int64Sub();
          break;
        case Kind::kMul:
          o = machine.Int64Mul();
          break;
        case Kind::kSignedDiv:
          o = machine.Int64Div();
          break;
        case Kind::kUnsignedDiv:
          o = machine.Uint64Div();
          break;
        case Kind::kSignedMod:
          o = machine.Int64Mod();
          break;
        case Kind::kUnsignedMod:
          o = machine.Uint64Mod();
          break;
        case Kind::kBitwiseAnd:
          o = machine.Word64And();
          break;
        case Kind::kBitwiseOr:
          o = machine.Word64Or();
          break;
        case Kind::kBitwiseXor:
          o = machine.Word64Xor();
          break;
        case Kind::kSignedMulOverflownBits:
        case Kind::kUnsignedMulOverflownBits:
          UNREACHABLE();
      }
      break;
    default:
      UNREACHABLE();
  }
  return AddNode(o, {GetNode(op.left()), GetNode(op.right())});
}
Node* ScheduleBuilder::ProcessOperation(const FloatBinopOp& op) {
  using Kind = FloatBinopOp::Kind;
  const Operator* o;
  switch (op.rep) {
    case MachineRepresentation::kFloat32:
      switch (op.kind) {
        case Kind::kAdd:
          o = machine.Float32Add();
          break;
        case Kind::kSub:
          o = machine.Float32Sub();
          break;
        case Kind::kMul:
          o = machine.Float32Mul();
          break;
        case Kind::kDiv:
          o = machine.Float32Div();
          break;
        case Kind::kMin:
          o = machine.Float32Min();
          break;
        case Kind::kMax:
          o = machine.Float32Max();
          break;
        case Kind::kPower:
        case Kind::kAtan2:
        case Kind::kMod:
          UNREACHABLE();
      }
      break;
    case MachineRepresentation::kFloat64:
      switch (op.kind) {
        case Kind::kAdd:
          o = machine.Float64Add();
          break;
        case Kind::kSub:
          o = machine.Float64Sub();
          break;
        case Kind::kMul:
          o = machine.Float64Mul();
          break;
        case Kind::kDiv:
          o = machine.Float64Div();
          break;
        case Kind::kMod:
          o = machine.Float64Mod();
          break;
        case Kind::kMin:
          o = machine.Float64Min();
          break;
        case Kind::kMax:
          o = machine.Float64Max();
          break;
        case Kind::kPower:
          o = machine.Float64Pow();
          break;
        case Kind::kAtan2:
          o = machine.Float64Atan2();
          break;
      }
      break;
    default:
      UNREACHABLE();
  }
  return AddNode(o, {GetNode(op.left()), GetNode(op.right())});
}

Node* ScheduleBuilder::ProcessOperation(const OverflowCheckedBinopOp& op) {
  const Operator* o;
  switch (op.rep) {
    case MachineRepresentation::kWord32:
      switch (op.kind) {
        case OverflowCheckedBinopOp::Kind::kSignedAdd:
          o = machine.Int32AddWithOverflow();
          break;
        case OverflowCheckedBinopOp::Kind::kSignedSub:
          o = machine.Int32SubWithOverflow();
          break;
        case OverflowCheckedBinopOp::Kind::kSignedMul:
          o = machine.Int32MulWithOverflow();
          break;
      }
      break;
    case MachineRepresentation::kWord64:
      switch (op.kind) {
        case OverflowCheckedBinopOp::Kind::kSignedAdd:
          o = machine.Int64AddWithOverflow();
          break;
        case OverflowCheckedBinopOp::Kind::kSignedSub:
          o = machine.Int64SubWithOverflow();
          break;
        case OverflowCheckedBinopOp::Kind::kSignedMul:
          UNREACHABLE();
      }
      break;
    default:
      UNREACHABLE();
  }
  return AddNode(o, {GetNode(op.left()), GetNode(op.right())});
}
Node* ScheduleBuilder::ProcessOperation(const WordUnaryOp& op) {
  DCHECK(op.rep == MachineRepresentation::kWord32 ||
         op.rep == MachineRepresentation::kWord64);
  bool word64 = op.rep == MachineRepresentation::kWord64;
  const Operator* o;
  switch (op.kind) {
    case WordUnaryOp::Kind::kReverseBytes:
      o = word64 ? machine.Word64ReverseBytes() : machine.Word32ReverseBytes();
      break;
    case WordUnaryOp::Kind::kCountLeadingZeros:
      o = word64 ? machine.Word64Clz() : machine.Word32Clz();
      break;
  }
  return AddNode(o, {GetNode(op.input())});
}
Node* ScheduleBuilder::ProcessOperation(const FloatUnaryOp& op) {
  DCHECK(op.rep == MachineRepresentation::kFloat32 ||
         op.rep == MachineRepresentation::kFloat64);
  bool float64 = op.rep == MachineRepresentation::kFloat64;
  const Operator* o;
  switch (op.kind) {
    case FloatUnaryOp::Kind::kAbs:
      o = float64 ? machine.Float64Abs() : machine.Float32Abs();
      break;
    case FloatUnaryOp::Kind::kNegate:
      o = float64 ? machine.Float64Neg() : machine.Float32Neg();
      break;
    case FloatUnaryOp::Kind::kRoundDown:
      o = float64 ? machine.Float64RoundDown().op()
                  : machine.Float32RoundDown().op();
      break;
    case FloatUnaryOp::Kind::kRoundUp:
      o = float64 ? machine.Float64RoundUp().op()
                  : machine.Float32RoundUp().op();
      break;
    case FloatUnaryOp::Kind::kRoundToZero:
      o = float64 ? machine.Float64RoundTruncate().op()
                  : machine.Float32RoundTruncate().op();
      break;
    case FloatUnaryOp::Kind::kRoundTiesEven:
      o = float64 ? machine.Float64RoundTiesEven().op()
                  : machine.Float32RoundTiesEven().op();
      break;
    case FloatUnaryOp::Kind::kSqrt:
      o = float64 ? machine.Float64Sqrt() : machine.Float32Sqrt();
      break;
    case FloatUnaryOp::Kind::kSilenceNaN:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64SilenceNaN();
      break;
    case FloatUnaryOp::Kind::kLog:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Log();
      break;
    case FloatUnaryOp::Kind::kExp:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Exp();
      break;
    case FloatUnaryOp::Kind::kExpm1:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Expm1();
      break;
    case FloatUnaryOp::Kind::kSin:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Sin();
      break;
    case FloatUnaryOp::Kind::kCos:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Cos();
      break;
    case FloatUnaryOp::Kind::kAsin:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Asin();
      break;
    case FloatUnaryOp::Kind::kAcos:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Acos();
      break;
    case FloatUnaryOp::Kind::kSinh:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Sinh();
      break;
    case FloatUnaryOp::Kind::kCosh:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Cosh();
      break;
    case FloatUnaryOp::Kind::kAsinh:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Asinh();
      break;
    case FloatUnaryOp::Kind::kAcosh:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Acosh();
      break;
    case FloatUnaryOp::Kind::kTan:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Tan();
      break;
    case FloatUnaryOp::Kind::kTanh:
      DCHECK_EQ(op.rep, MachineRepresentation::kFloat64);
      o = machine.Float64Tanh();
      break;
  }
  return AddNode(o, {GetNode(op.input())});
}
Node* ScheduleBuilder::ProcessOperation(const ShiftOp& op) {
  DCHECK(op.rep == MachineRepresentation::kWord32 ||
         op.rep == MachineRepresentation::kWord64);
  bool word64 = op.rep == MachineRepresentation::kWord64;
  const Operator* o;
  switch (op.kind) {
    case ShiftOp::Kind::kShiftRightArithmeticShiftOutZeros:
      o = word64 ? machine.Word64SarShiftOutZeros()
                 : machine.Word32SarShiftOutZeros();
      break;
    case ShiftOp::Kind::kShiftRightArithmetic:
      o = word64 ? machine.Word64Sar() : machine.Word32Sar();
      break;
    case ShiftOp::Kind::kShiftRightLogical:
      o = word64 ? machine.Word64Shr() : machine.Word32Shr();
      break;
    case ShiftOp::Kind::kShiftLeft:
      o = word64 ? machine.Word64Shl() : machine.Word32Shl();
      break;
    case ShiftOp::Kind::kRotateLeft:
      o = word64 ? machine.Word64Rol().op() : machine.Word32Rol().op();
      break;
    case ShiftOp::Kind::kRotateRight:
      o = word64 ? machine.Word64Ror() : machine.Word32Ror();
      break;
  }
  return AddNode(o, {GetNode(op.left()), GetNode(op.right())});
}
Node* ScheduleBuilder::ProcessOperation(const EqualOp& op) {
  const Operator* o;
  switch (op.rep) {
    case MachineRepresentation::kWord32:
      o = machine.Word32Equal();
      break;
    case MachineRepresentation::kWord64:
      o = machine.Word64Equal();
      break;
    case MachineRepresentation::kFloat32:
      o = machine.Float32Equal();
      break;
    case MachineRepresentation::kFloat64:
      o = machine.Float64Equal();
      break;
    default:
      UNREACHABLE();
  }
  return AddNode(o, {GetNode(op.left()), GetNode(op.right())});
}
Node* ScheduleBuilder::ProcessOperation(const ComparisonOp& op) {
  const Operator* o;
  switch (op.rep) {
    case MachineRepresentation::kWord32:
      switch (op.kind) {
        case ComparisonOp::Kind::kSignedLessThan:
          o = machine.Int32LessThan();
          break;
        case ComparisonOp::Kind::kSignedLessThanOrEqual:
          o = machine.Int32LessThanOrEqual();
          break;
        case ComparisonOp::Kind::kUnsignedLessThan:
          o = machine.Uint32LessThan();
          break;
        case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
          o = machine.Uint32LessThanOrEqual();
          break;
      }
      break;
    case MachineRepresentation::kWord64:
      switch (op.kind) {
        case ComparisonOp::Kind::kSignedLessThan:
          o = machine.Int64LessThan();
          break;
        case ComparisonOp::Kind::kSignedLessThanOrEqual:
          o = machine.Int64LessThanOrEqual();
          break;
        case ComparisonOp::Kind::kUnsignedLessThan:
          o = machine.Uint64LessThan();
          break;
        case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
          o = machine.Uint64LessThanOrEqual();
          break;
      }
      break;
    case MachineRepresentation::kFloat32:
      switch (op.kind) {
        case ComparisonOp::Kind::kSignedLessThan:
          o = machine.Float32LessThan();
          break;
        case ComparisonOp::Kind::kSignedLessThanOrEqual:
          o = machine.Float32LessThanOrEqual();
          break;
        case ComparisonOp::Kind::kUnsignedLessThan:
        case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
          UNREACHABLE();
      }
      break;
    case MachineRepresentation::kFloat64:
      switch (op.kind) {
        case ComparisonOp::Kind::kSignedLessThan:
          o = machine.Float64LessThan();
          break;
        case ComparisonOp::Kind::kSignedLessThanOrEqual:
          o = machine.Float64LessThanOrEqual();
          break;
        case ComparisonOp::Kind::kUnsignedLessThan:
        case ComparisonOp::Kind::kUnsignedLessThanOrEqual:
          UNREACHABLE();
      }
      break;
    default:
      UNREACHABLE();
  }
  return AddNode(o, {GetNode(op.left()), GetNode(op.right())});
}
Node* ScheduleBuilder::ProcessOperation(const ChangeOp& op) {
  const Operator* o;
  switch (op.kind) {
    using Kind = ChangeOp::Kind;
    case Kind::kFloatConversion:
      if (op.from == MachineRepresentation::kFloat64 &&
          op.to == MachineRepresentation::kFloat32) {
        o = machine.TruncateFloat64ToFloat32();
      } else if (op.from == MachineRepresentation::kFloat32 &&
                 op.to == MachineRepresentation::kFloat64) {
        o = machine.ChangeFloat32ToFloat64();
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kSignedFloatTruncate:
      if (op.from == MachineRepresentation::kFloat64 &&
          op.to == MachineRepresentation::kWord64) {
        o = machine.TruncateFloat64ToInt64(TruncateKind::kArchitectureDefault);
      } else if (op.from == MachineRepresentation::kFloat64 &&
                 op.to == MachineRepresentation::kWord32) {
        o = machine.RoundFloat64ToInt32();
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kSignedFloatTruncateOverflowToMin:
      if (op.from == MachineRepresentation::kFloat64 &&
          op.to == MachineRepresentation::kWord64) {
        o = machine.TruncateFloat64ToInt64(TruncateKind::kSetOverflowToMin);
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kJSFloatTruncate:
      if (op.from == MachineRepresentation::kFloat64 &&
          op.to == MachineRepresentation::kWord32) {
        o = machine.TruncateFloat64ToWord32();
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kSignedToFloat:
      if (op.from == MachineRepresentation::kWord32 &&
          op.to == MachineRepresentation::kFloat64) {
        o = machine.ChangeInt32ToFloat64();
      } else if (op.from == MachineRepresentation::kWord64 &&
                 op.to == MachineRepresentation::kFloat64) {
        o = machine.ChangeInt64ToFloat64();
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kUnsignedToFloat:
      if (op.from == MachineRepresentation::kWord32 &&
          op.to == MachineRepresentation::kFloat64) {
        o = machine.ChangeUint32ToFloat64();
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kExtractHighHalf:
      DCHECK_EQ(op.from, MachineRepresentation::kFloat64);
      DCHECK_EQ(op.to, MachineRepresentation::kWord32);
      o = machine.Float64ExtractHighWord32();
      break;
    case Kind::kExtractLowHalf:
      DCHECK_EQ(op.from, MachineRepresentation::kFloat64);
      DCHECK_EQ(op.to, MachineRepresentation::kWord32);
      o = machine.Float64ExtractLowWord32();
      break;
    case Kind::kBitcast:
      if (op.from == MachineRepresentation::kWord32 &&
          op.to == MachineRepresentation::kWord64) {
        o = machine.BitcastWord32ToWord64();
      } else if (op.from == MachineRepresentation::kFloat32 &&
                 op.to == MachineRepresentation::kWord32) {
        o = machine.BitcastFloat32ToInt32();
      } else if (op.from == MachineRepresentation::kWord32 &&
                 op.to == MachineRepresentation::kFloat32) {
        o = machine.BitcastInt32ToFloat32();
      } else if (op.from == MachineRepresentation::kFloat64 &&
                 op.to == MachineRepresentation::kWord64) {
        o = machine.BitcastFloat64ToInt64();
      } else if (op.from == MachineRepresentation::kWord64 &&
                 op.to == MachineRepresentation::kFloat64) {
        o = machine.BitcastInt64ToFloat64();
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kSignExtend:
      if (op.from == MachineRepresentation::kWord32 &&
          op.to == MachineRepresentation::kWord64) {
        o = machine.ChangeInt32ToInt64();
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kZeroExtend:
      if (op.from == MachineRepresentation::kWord32 &&
          op.to == MachineRepresentation::kWord64) {
        o = machine.ChangeUint32ToUint64();
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kSignedNarrowing:
      if (op.from == MachineRepresentation::kFloat64 &&
          op.to == MachineRepresentation::kWord64) {
        o = machine.ChangeFloat64ToInt64();
      } else if (op.from == MachineRepresentation::kFloat64 &&
                 op.to == MachineRepresentation::kWord32) {
        o = machine.ChangeFloat64ToInt32();
      } else {
        UNIMPLEMENTED();
      }
      break;
    case Kind::kUnsignedNarrowing:
      if (op.from == MachineRepresentation::kFloat64 &&
          op.to == MachineRepresentation::kWord64) {
        o = machine.ChangeFloat64ToUint64();
      } else if (op.from == MachineRepresentation::kFloat64 &&
                 op.to == MachineRepresentation::kWord32) {
        o = machine.ChangeFloat64ToUint32();
      } else {
        UNIMPLEMENTED();
      }
      break;
  }
  return AddNode(o, {GetNode(op.input())});
}
Node* ScheduleBuilder::ProcessOperation(const Float64InsertWord32Op& op) {
  switch (op.kind) {
    case Float64InsertWord32Op::Kind::kHighHalf:
      return AddNode(machine.Float64InsertHighWord32(),
                     {GetNode(op.float64()), GetNode(op.word32())});
    case Float64InsertWord32Op::Kind::kLowHalf:
      return AddNode(machine.Float64InsertLowWord32(),
                     {GetNode(op.float64()), GetNode(op.word32())});
  }
}
Node* ScheduleBuilder::ProcessOperation(const TaggedBitcastOp& op) {
  const Operator* o;
  if (op.from == MachineRepresentation::kTagged &&
      op.to == MachineType::PointerRepresentation()) {
    o = machine.BitcastTaggedToWord();
  } else if (op.from == MachineType::PointerRepresentation() &&
             op.to == MachineRepresentation::kTagged) {
    o = machine.BitcastWordToTagged();
  } else {
    UNIMPLEMENTED();
  }
  return AddNode(o, {GetNode(op.input())});
}
Node* ScheduleBuilder::ProcessOperation(const PendingLoopPhiOp& op) {
  UNREACHABLE();
}
Node* ScheduleBuilder::ProcessOperation(const TupleOp& op) {
  // Tuples are only used for lowerings during reduction. Therefore, we can
  // assume that it is unused if it occurs at this point.
  return nullptr;
}
Node* ScheduleBuilder::ProcessOperation(const ConstantOp& op) {
  switch (op.kind) {
    case ConstantOp::Kind::kWord32:
      return AddNode(common.Int32Constant(static_cast<int32_t>(op.word32())),
                     {});
    case ConstantOp::Kind::kWord64:
      return AddNode(common.Int64Constant(static_cast<int64_t>(op.word64())),
                     {});
    case ConstantOp::Kind::kExternal:
      return AddNode(common.ExternalConstant(op.external_reference()), {});
    case ConstantOp::Kind::kHeapObject:
      return AddNode(common.HeapConstant(op.handle()), {});
    case ConstantOp::Kind::kCompressedHeapObject:
      return AddNode(common.CompressedHeapConstant(op.handle()), {});
    case ConstantOp::Kind::kNumber:
      return AddNode(common.NumberConstant(op.number()), {});
    case ConstantOp::Kind::kTaggedIndex:
      return AddNode(common.TaggedIndexConstant(op.tagged_index()), {});
    case ConstantOp::Kind::kFloat64:
      return AddNode(common.Float64Constant(op.float64()), {});
    case ConstantOp::Kind::kFloat32:
      return AddNode(common.Float32Constant(op.float32()), {});
  }
}
Node* ScheduleBuilder::ProcessOperation(const LoadOp& op) {
  intptr_t offset = op.offset;
  if (op.kind == LoadOp::Kind::kTaggedBase) {
    CHECK_GE(offset, std::numeric_limits<int32_t>::min() + kHeapObjectTag);
    offset -= kHeapObjectTag;
  }
  Node* base = GetNode(op.base());
  return AddNode(IsAlignedAccess(op.kind)
                     ? machine.Load(op.loaded_rep)
                     : machine.UnalignedLoad(op.loaded_rep),
                 {base, IntPtrConstant(offset)});
}
Node* ScheduleBuilder::ProcessOperation(const IndexedLoadOp& op) {
  intptr_t offset = op.offset;
  if (op.kind == LoadOp::Kind::kTaggedBase) {
    CHECK_GE(offset, std::numeric_limits<int32_t>::min() + kHeapObjectTag);
    offset -= kHeapObjectTag;
  }
  Node* base = GetNode(op.base());
  Node* index = GetNode(op.index());
  if (op.element_size_log2 != 0) {
    index = IntPtrShl(index, IntPtrConstant(op.element_size_log2));
  }
  if (offset != 0) {
    index = IntPtrAdd(index, IntPtrConstant(offset));
  }
  return AddNode(IsAlignedAccess(op.kind)
                     ? machine.Load(op.loaded_rep)
                     : machine.UnalignedLoad(op.loaded_rep),
                 {base, index});
}
Node* ScheduleBuilder::ProcessOperation(const StoreOp& op) {
  intptr_t offset = op.offset;
  if (op.kind == StoreOp::Kind::kTaggedBase) {
    CHECK(offset >= std::numeric_limits<int32_t>::min() + kHeapObjectTag);
    offset -= kHeapObjectTag;
  }
  Node* base = GetNode(op.base());
  Node* value = GetNode(op.value());
  const Operator* o;
  if (IsAlignedAccess(op.kind)) {
    o = machine.Store(StoreRepresentation(op.stored_rep, op.write_barrier));
  } else {
    DCHECK_EQ(op.write_barrier, WriteBarrierKind::kNoWriteBarrier);
    o = machine.UnalignedStore(op.stored_rep);
  }
  return AddNode(o, {base, IntPtrConstant(offset), value});
}
Node* ScheduleBuilder::ProcessOperation(const IndexedStoreOp& op) {
  intptr_t offset = op.offset;
  if (op.kind == IndexedStoreOp::Kind::kTaggedBase) {
    CHECK(offset >= std::numeric_limits<int32_t>::min() + kHeapObjectTag);
    offset -= kHeapObjectTag;
  }
  Node* base = GetNode(op.base());
  Node* index = GetNode(op.index());
  Node* value = GetNode(op.value());
  if (op.element_size_log2 != 0) {
    index = IntPtrShl(index, IntPtrConstant(op.element_size_log2));
  }
  if (offset != 0) {
    index = IntPtrAdd(index, IntPtrConstant(offset));
  }
  const Operator* o;
  if (IsAlignedAccess(op.kind)) {
    o = machine.Store(StoreRepresentation(op.stored_rep, op.write_barrier));
  } else {
    DCHECK_EQ(op.write_barrier, WriteBarrierKind::kNoWriteBarrier);
    o = machine.UnalignedStore(op.stored_rep);
  }
  return AddNode(o, {base, index, value});
}
Node* ScheduleBuilder::ProcessOperation(const RetainOp& op) {
  return AddNode(common.Retain(), {GetNode(op.retained())});
}
Node* ScheduleBuilder::ProcessOperation(const ParameterOp& op) {
  // Parameters need to be cached because the register allocator assumes that
  // there are no duplicate nodes for the same parameter.
  if (parameters.count(op.parameter_index)) {
    return parameters[op.parameter_index];
  }
  Node* parameter = MakeNode(
      common.Parameter(static_cast<int>(op.parameter_index), op.debug_name),
      {tf_graph->start()});
  schedule->AddNode(schedule->start(), parameter);
  parameters[op.parameter_index] = parameter;
  return parameter;
}
Node* ScheduleBuilder::ProcessOperation(const OsrValueOp& op) {
  // OSR values behave like parameters, so they also need to be cached.
  if (osr_values.count(op.index)) {
    return osr_values[op.index];
  }
  Node* osr_value = MakeNode(common.OsrValue(static_cast<int>(op.index)),
                             {tf_graph->start()});
  schedule->AddNode(schedule->start(), osr_value);
  osr_values[op.index] = osr_value;
  return osr_value;
}
Node* ScheduleBuilder::ProcessOperation(const GotoOp& op) {
  schedule->AddGoto(current_block, blocks[op.destination->index().id()]);
  current_block = nullptr;
  return nullptr;
}
Node* ScheduleBuilder::ProcessOperation(const StackPointerGreaterThanOp& op) {
  return AddNode(machine.StackPointerGreaterThan(op.kind),
                 {GetNode(op.stack_limit())});
}
Node* ScheduleBuilder::ProcessOperation(const StackSlotOp& op) {
  return AddNode(machine.StackSlot(op.size, op.alignment), {});
}
Node* ScheduleBuilder::ProcessOperation(const FrameConstantOp& op) {
  switch (op.kind) {
    case FrameConstantOp::Kind::kStackCheckOffset:
      return AddNode(machine.LoadStackCheckOffset(), {});
    case FrameConstantOp::Kind::kFramePointer:
      return AddNode(machine.LoadFramePointer(), {});
    case FrameConstantOp::Kind::kParentFramePointer:
      return AddNode(machine.LoadParentFramePointer(), {});
  }
}
Node* ScheduleBuilder::ProcessOperation(const CheckLazyDeoptOp& op) {
  Node* call = GetNode(op.call());
  Node* frame_state = GetNode(op.frame_state());
  call->AppendInput(graph_zone, frame_state);
  return nullptr;
}
Node* ScheduleBuilder::ProcessOperation(const DeoptimizeIfOp& op) {
  Node* condition = GetNode(op.condition());
  Node* frame_state = GetNode(op.frame_state());
  const Operator* o = op.negated
                          ? common.DeoptimizeUnless(op.parameters->reason(),
                                                    op.parameters->feedback())
                          : common.DeoptimizeIf(op.parameters->reason(),
                                                op.parameters->feedback());
  return AddNode(o, {condition, frame_state});
}
Node* ScheduleBuilder::ProcessOperation(const DeoptimizeOp& op) {
  Node* frame_state = GetNode(op.frame_state());
  const Operator* o =
      common.Deoptimize(op.parameters->reason(), op.parameters->feedback());
  Node* node = MakeNode(o, {frame_state});
  schedule->AddDeoptimize(current_block, node);
  current_block = nullptr;
  return nullptr;
}
Node* ScheduleBuilder::ProcessOperation(const PhiOp& op) {
  if (current_input_block->IsLoop()) {
    DCHECK_EQ(op.input_count, 2);
    Node* input = GetNode(op.input(0));
    // The second `input` is a placeholder that is patched when we process the
    // backedge.
    Node* node = AddNode(common.Phi(op.rep, 2), {input, input});
    loop_phis.emplace_back(node, op.input(1));
    return node;
  } else {
    base::SmallVector<Node*, 8> inputs;
    for (OpIndex i : op.inputs()) {
      inputs.push_back(GetNode(i));
    }
    return AddNode(common.Phi(op.rep, op.input_count), base::VectorOf(inputs));
  }
}
Node* ScheduleBuilder::ProcessOperation(const ProjectionOp& op) {
  return AddNode(common.Projection(op.index), {GetNode(op.input())});
}

std::pair<Node*, MachineType> ScheduleBuilder::BuildDeoptInput(
    FrameStateData::Iterator* it) {
  switch (it->current_instr()) {
    using Instr = FrameStateData::Instr;
    case Instr::kInput: {
      MachineType type;
      OpIndex input;
      it->ConsumeInput(&type, &input);
      return {GetNode(input), type};
    }
    case Instr::kDematerializedObject: {
      uint32_t obj_id;
      uint32_t field_count;
      it->ConsumeDematerializedObject(&obj_id, &field_count);
      base::SmallVector<Node*, 16> fields;
      ZoneVector<MachineType>& field_types =
          *tf_graph->zone()->New<ZoneVector<MachineType>>(field_count,
                                                          tf_graph->zone());
      for (uint32_t i = 0; i < field_count; ++i) {
        std::pair<Node*, MachineType> p = BuildDeoptInput(it);
        fields.push_back(p.first);
        field_types[i] = p.second;
      }
      return {AddNode(common.TypedObjectState(obj_id, &field_types),
                      base::VectorOf(fields)),
              MachineType::AnyTagged()};
    }
    case Instr::kDematerializedObjectReference: {
      uint32_t obj_id;
      it->ConsumeDematerializedObjectReference(&obj_id);
      return {AddNode(common.ObjectId(obj_id), {}), MachineType::AnyTagged()};
    }
    case Instr::kArgumentsElements: {
      CreateArgumentsType type;
      it->ConsumeArgumentsElements(&type);
      return {AddNode(common.ArgumentsElementsState(type), {}),
              MachineType::AnyTagged()};
    }
    case Instr::kArgumentsLength: {
      it->ConsumeArgumentsLength();
      return {AddNode(common.ArgumentsLengthState(), {}),
              MachineType::AnyTagged()};
    }
    case Instr::kUnusedRegister:
      UNREACHABLE();
  }
}

// Create a mostly balanced tree of `StateValues` nodes.
Node* ScheduleBuilder::BuildStateValues(FrameStateData::Iterator* it,
                                        int32_t size) {
  constexpr int32_t kMaxStateValueInputCount = 8;

  base::SmallVector<Node*, kMaxStateValueInputCount> inputs;
  base::SmallVector<MachineType, kMaxStateValueInputCount> types;
  SparseInputMask::BitMaskType input_mask = 0;
  int32_t child_size =
      (size + kMaxStateValueInputCount - 1) / kMaxStateValueInputCount;
  // `state_value_inputs` counts the number of inputs used for the current
  // `StateValues` node. It is gradually adjusted as nodes are shifted to lower
  // levels in the tree.
  int32_t state_value_inputs = size;
  int32_t mask_size = 0;
  for (int32_t i = 0; i < state_value_inputs; ++i) {
    DCHECK_LT(i, kMaxStateValueInputCount);
    ++mask_size;
    if (state_value_inputs <= kMaxStateValueInputCount) {
      // All the remaining inputs fit at the current level.
      if (it->current_instr() == FrameStateData::Instr::kUnusedRegister) {
        it->ConsumeUnusedRegister();
      } else {
        std::pair<Node*, MachineType> p = BuildDeoptInput(it);
        input_mask |= SparseInputMask::BitMaskType{1} << i;
        inputs.push_back(p.first);
        types.push_back(p.second);
      }
    } else {
      // We have too many inputs, so recursively create another `StateValues`
      // node.
      input_mask |= SparseInputMask::BitMaskType{1} << i;
      int32_t actual_child_size = std::min(child_size, state_value_inputs - i);
      inputs.push_back(BuildStateValues(it, actual_child_size));
      // This is a dummy type that shouldn't matter.
      types.push_back(MachineType::AnyTagged());
      // `child_size`-many inputs were shifted to the next level, being replaced
      // with 1 `StateValues` node.
      state_value_inputs = state_value_inputs - actual_child_size + 1;
    }
  }
  input_mask |= SparseInputMask::kEndMarker << mask_size;
  return AddNode(
      common.TypedStateValues(graph_zone->New<ZoneVector<MachineType>>(
                                  types.begin(), types.end(), graph_zone),
                              SparseInputMask(input_mask)),
      base::VectorOf(inputs));
}

Node* ScheduleBuilder::BuildTaggedInput(FrameStateData::Iterator* it) {
  std::pair<Node*, MachineType> p = BuildDeoptInput(it);
  DCHECK(p.second.IsTagged());
  return p.first;
}

Node* ScheduleBuilder::ProcessOperation(const FrameStateOp& op) {
  const FrameStateInfo& info = op.data->frame_state_info;
  auto it = op.data->iterator(op.state_values());

  Node* parameter_state_values = BuildStateValues(&it, info.parameter_count());
  Node* register_state_values = BuildStateValues(&it, info.local_count());
  Node* accumulator_state_values = BuildStateValues(&it, info.stack_count());
  Node* context = BuildTaggedInput(&it);
  Node* closure = BuildTaggedInput(&it);
  Node* parent =
      op.inlined ? GetNode(op.parent_frame_state()) : tf_graph->start();

  return AddNode(common.FrameState(info.bailout_id(), info.state_combine(),
                                   info.function_info()),
                 {parameter_state_values, register_state_values,
                  accumulator_state_values, context, closure, parent});
}
Node* ScheduleBuilder::ProcessOperation(const CallOp& op) {
  base::SmallVector<Node*, 16> inputs;
  inputs.push_back(GetNode(op.callee()));
  for (OpIndex i : op.arguments()) {
    inputs.push_back(GetNode(i));
  }
  return AddNode(common.Call(op.descriptor), base::VectorOf(inputs));
}
Node* ScheduleBuilder::ProcessOperation(const UnreachableOp& op) {
  Node* node = MakeNode(common.Throw(), {});
  schedule->AddThrow(current_block, node);
  current_block = nullptr;
  return nullptr;
}
Node* ScheduleBuilder::ProcessOperation(const ReturnOp& op) {
  base::SmallVector<Node*, 8> inputs = {GetNode(op.pop_count())};
  for (OpIndex i : op.return_values()) {
    inputs.push_back(GetNode(i));
  }
  Node* node =
      MakeNode(common.Return(static_cast<int>(op.return_values().size())),
               base::VectorOf(inputs));
  schedule->AddReturn(current_block, node);
  current_block = nullptr;
  return nullptr;
}
Node* ScheduleBuilder::ProcessOperation(const BranchOp& op) {
  Node* branch =
      MakeNode(common.Branch(BranchHint::kNone), {GetNode(op.condition())});
  BasicBlock* true_block = GetBlock(*op.if_true);
  BasicBlock* false_block = GetBlock(*op.if_false);
  schedule->AddBranch(current_block, branch, true_block, false_block);
  schedule->AddNode(true_block, MakeNode(common.IfTrue(), {branch}));
  schedule->AddNode(false_block, MakeNode(common.IfFalse(), {branch}));
  current_block = nullptr;
  return nullptr;
}
Node* ScheduleBuilder::ProcessOperation(const CatchExceptionOp& op) {
  Node* call = GetNode(op.call());
  BasicBlock* success_block = GetBlock(*op.if_success);
  BasicBlock* exception_block = GetBlock(*op.if_exception);
  schedule->AddCall(current_block, call, success_block, exception_block);
  Node* if_success = MakeNode(common.IfSuccess(), {call});
  Node* if_exception = MakeNode(common.IfException(), {call, call});
  schedule->AddNode(success_block, if_success);
  // Pass `call` as both the effect and control input of `IfException`.
  schedule->AddNode(exception_block, if_exception);
  current_block = nullptr;
  return if_exception;
}
Node* ScheduleBuilder::ProcessOperation(const SwitchOp& op) {
  size_t succ_count = op.cases.size() + 1;
  Node* switch_node =
      MakeNode(common.Switch(succ_count), {GetNode(op.input())});

  base::SmallVector<BasicBlock*, 16> successors;
  for (SwitchOp::Case c : op.cases) {
    BasicBlock* case_block = GetBlock(*c.destination);
    successors.push_back(case_block);
    Node* case_node = MakeNode(common.IfValue(c.value), {switch_node});
    schedule->AddNode(case_block, case_node);
  }
  BasicBlock* default_block = GetBlock(*op.default_case);
  successors.push_back(default_block);
  schedule->AddNode(default_block, MakeNode(common.IfDefault(), {switch_node}));

  schedule->AddSwitch(current_block, switch_node, successors.data(),
                      successors.size());
  current_block = nullptr;
  return nullptr;
}

}  // namespace

RecreateScheduleResult RecreateSchedule(const Graph& graph,
                                        CallDescriptor* call_descriptor,
                                        Zone* graph_zone, Zone* phase_zone,
                                        SourcePositionTable* source_positions,
                                        NodeOriginTable* origins) {
  ScheduleBuilder builder{graph,      call_descriptor,  graph_zone,
                          phase_zone, source_positions, origins};
  return builder.Run();
}

}  // namespace v8::internal::compiler::turboshaft
