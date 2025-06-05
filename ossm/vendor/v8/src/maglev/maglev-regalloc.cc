// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-regalloc.h"

#include <sstream>

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/compiler/backend/instruction.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-regalloc-data.h"

namespace v8 {
namespace internal {

namespace maglev {

namespace {

constexpr RegisterStateFlags initialized_node{true, false};
constexpr RegisterStateFlags initialized_merge{true, true};

using BlockReverseIterator = std::vector<BasicBlock>::reverse_iterator;

// A target is a fallthrough of a control node if its ID is the next ID
// after the control node.
//
// TODO(leszeks): Consider using the block iterator instead.
bool IsTargetOfNodeFallthrough(ControlNode* node, BasicBlock* target) {
  return node->id() + 1 == target->first_id();
}

ControlNode* NearestPostDominatingHole(ControlNode* node) {
  // Conditional control nodes don't cause holes themselves. So, the nearest
  // post-dominating hole is the conditional control node's next post-dominating
  // hole.
  if (node->Is<BranchControlNode>()) {
    return node->next_post_dominating_hole();
  }

  // If the node is a Jump, it may be a hole, but only if it is not a
  // fallthrough (jump to the immediately next block). Otherwise, it will point
  // to the nearest post-dominating hole in its own "next" field.
  if (Jump* jump = node->TryCast<Jump>()) {
    if (IsTargetOfNodeFallthrough(jump, jump->target())) {
      return jump->next_post_dominating_hole();
    }
  }

  // If the node is a Switch, it can only have a hole if there is no
  // fallthrough.
  if (Switch* _switch = node->TryCast<Switch>()) {
    if (_switch->has_fallthrough()) {
      return _switch->next_post_dominating_hole();
    }
  }

  return node;
}

ControlNode* HighestPostDominatingHole(ControlNode* first,
                                       ControlNode* second) {
  // Either find the merge-point of both branches, or the highest reachable
  // control-node of the longest branch after the last node of the shortest
  // branch.

  // As long as there's no merge-point.
  while (first != second) {
    // Walk the highest branch to find where it goes.
    if (first->id() > second->id()) std::swap(first, second);

    // If the first branch terminates or jumps back, we've found highest
    // reachable control-node of the longest branch (the second control
    // node).
    if (first->Is<TerminalControlNode>() || first->Is<JumpLoop>()) {
      return second;
    }

    // Continue one step along the highest branch. This may cross over the
    // lowest branch in case it returns or loops. If labelled blocks are
    // involved such swapping of which branch is the highest branch can
    // occur multiple times until a return/jumploop/merge is discovered.
    first = first->next_post_dominating_hole();
  }

  // Once the branches merged, we've found the gap-chain that's relevant
  // for the control node.
  return first;
}

template <size_t kSize>
ControlNode* HighestPostDominatingHole(
    base::SmallVector<ControlNode*, kSize>& holes) {
  // Sort them from highest to shortest.
  std::sort(holes.begin(), holes.end(),
            [](ControlNode* first, ControlNode* second) {
              return first->id() > second->id();
            });
  DCHECK_GT(holes.size(), 1);
  // Find the highest post dominating hole.
  ControlNode* post_dominating_hole = holes.back();
  holes.pop_back();
  while (holes.size() > 0) {
    ControlNode* next_hole = holes.back();
    holes.pop_back();
    post_dominating_hole =
        HighestPostDominatingHole(post_dominating_hole, next_hole);
  }
  return post_dominating_hole;
}

bool IsLiveAtTarget(ValueNode* node, ControlNode* source, BasicBlock* target) {
  DCHECK_NOT_NULL(node);
  DCHECK(!node->is_dead());

  // If we're looping, a value can only be live if it was live before the loop.
  if (target->control_node()->id() <= source->id()) {
    // Gap moves may already be inserted in the target, so skip over those.
    return node->id() < target->FirstNonGapMoveId();
  }
  // TODO(verwaest): This should be true but isn't because we don't yet
  // eliminate dead code.
  // DCHECK_GT(node->next_use, source->id());
  // TODO(verwaest): Since we don't support deopt yet we can only deal with
  // direct branches. Add support for holes.
  return node->live_range().end >= target->first_id();
}

template <typename RegisterT>
void ClearDeadFallthroughRegisters(RegisterFrameState<RegisterT> registers,
                                   ConditionalControlNode* control_node,
                                   BasicBlock* target) {
  RegListBase<RegisterT> list = registers.used();
  while (list != registers.empty()) {
    RegisterT reg = list.PopFirst();
    ValueNode* node = registers.GetValue(reg);
    if (!IsLiveAtTarget(node, control_node, target)) {
      registers.FreeRegistersUsedBy(node);
      // Update the registers we're visiting to avoid revisiting this node.
      list.clear(registers.free());
    }
  }
}
}  // namespace

StraightForwardRegisterAllocator::StraightForwardRegisterAllocator(
    MaglevCompilationInfo* compilation_info, Graph* graph)
    : compilation_info_(compilation_info), graph_(graph) {
  ComputePostDominatingHoles();
  AllocateRegisters();
  graph_->set_tagged_stack_slots(tagged_.top);
  graph_->set_untagged_stack_slots(untagged_.top);
}

StraightForwardRegisterAllocator::~StraightForwardRegisterAllocator() = default;

// Compute, for all forward control nodes (i.e. excluding Return and JumpLoop) a
// tree of post-dominating control flow holes.
//
// Control flow which interrupts linear control flow fallthrough for basic
// blocks is considered to introduce a control flow "hole".
//
//                   A──────┐                │
//                   │ Jump │                │
//                   └──┬───┘                │
//                  {   │  B──────┐          │
//     Control flow {   │  │ Jump │          │ Linear control flow
//     hole after A {   │  └─┬────┘          │
//                  {   ▼    ▼ Fallthrough   │
//                     C──────┐              │
//                     │Return│              │
//                     └──────┘              ▼
//
// It is interesting, for each such hole, to know what the next hole will be
// that we will unconditionally reach on our way to an exit node. Such
// subsequent holes are in "post-dominators" of the current block.
//
// As an example, consider the following CFG, with the annotated holes. The
// post-dominating hole tree is the transitive closure of the post-dominator
// tree, up to nodes which are holes (in this example, A, D, F and H).
//
//                       CFG               Immediate       Post-dominating
//                                      post-dominators          holes
//                   A──────┐
//                   │ Jump │               A                 A
//                   └──┬───┘               │                 │
//                  {   │  B──────┐         │                 │
//     Control flow {   │  │ Jump │         │   B             │       B
//     hole after A {   │  └─┬────┘         │   │             │       │
//                  {   ▼    ▼              │   │             │       │
//                     C──────┐             │   │             │       │
//                     │Branch│             └►C◄┘             │   C   │
//                     └┬────┬┘               │               │   │   │
//                      ▼    │                │               │   │   │
//                   D──────┐│                │               │   │   │
//                   │ Jump ││              D │               │ D │   │
//                   └──┬───┘▼              │ │               │ │ │   │
//                  {   │  E──────┐         │ │               │ │ │   │
//     Control flow {   │  │ Jump │         │ │ E             │ │ │ E │
//     hole after D {   │  └─┬────┘         │ │ │             │ │ │ │ │
//                  {   ▼    ▼              │ │ │             │ │ │ │ │
//                     F──────┐             │ ▼ │             │ │ ▼ │ │
//                     │ Jump │             └►F◄┘             └─┴►F◄┴─┘
//                     └─────┬┘               │                   │
//                  {        │  G──────┐      │                   │
//     Control flow {        │  │ Jump │      │ G                 │ G
//     hole after F {        │  └─┬────┘      │ │                 │ │
//                  {        ▼    ▼           │ │                 │ │
//                          H──────┐          ▼ │                 ▼ │
//                          │Return│          H◄┘                 H◄┘
//                          └──────┘
//
// Since we only care about forward control, loop jumps are treated the same as
// returns -- they terminate the post-dominating hole chain.
//
void StraightForwardRegisterAllocator::ComputePostDominatingHoles() {
  // For all blocks, find the list of jumps that jump over code unreachable from
  // the block. Such a list of jumps terminates in return or jumploop.
  for (BasicBlock* block : base::Reversed(*graph_)) {
    ControlNode* control = block->control_node();
    if (auto node = control->TryCast<Jump>()) {
      // If the current control node is a jump, prepend it to the list of jumps
      // at the target.
      control->set_next_post_dominating_hole(
          NearestPostDominatingHole(node->target()->control_node()));
    } else if (auto node = control->TryCast<BranchControlNode>()) {
      ControlNode* first =
          NearestPostDominatingHole(node->if_true()->control_node());
      ControlNode* second =
          NearestPostDominatingHole(node->if_false()->control_node());
      control->set_next_post_dominating_hole(
          HighestPostDominatingHole(first, second));
    } else if (auto node = control->TryCast<Switch>()) {
      int num_targets = node->size() + (node->has_fallthrough() ? 1 : 0);
      if (num_targets == 1) {
        // If we have a single target, the next post dominating hole
        // is the same one as the target.
        DCHECK(!node->has_fallthrough());
        control->set_next_post_dominating_hole(NearestPostDominatingHole(
            node->targets()[0].block_ptr()->control_node()));
        continue;
      }
      // Calculate the post dominating hole for each target.
      base::SmallVector<ControlNode*, 16> holes(num_targets);
      for (int i = 0; i < node->size(); i++) {
        holes[i] = NearestPostDominatingHole(
            node->targets()[i].block_ptr()->control_node());
      }
      if (node->has_fallthrough()) {
        holes[node->size()] =
            NearestPostDominatingHole(node->fallthrough()->control_node());
      }
      control->set_next_post_dominating_hole(HighestPostDominatingHole(holes));
    }
  }
}

void StraightForwardRegisterAllocator::PrintLiveRegs() const {
  bool first = true;
  auto print = [&](auto reg, ValueNode* node) {
    if (first) {
      first = false;
    } else {
      printing_visitor_->os() << ", ";
    }
    printing_visitor_->os() << reg << "=v" << node->id();
  };
  general_registers_.ForEachUsedRegister(print);
  double_registers_.ForEachUsedRegister(print);
}

void StraightForwardRegisterAllocator::AllocateRegisters() {
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_.reset(new MaglevPrintingVisitor(
        compilation_info_->graph_labeller(), std::cout));
    printing_visitor_->PreProcessGraph(graph_);
  }

  for (const auto& [ref, constant] : graph_->constants()) {
    constant->SetConstantLocation();
    USE(ref);
  }
  for (const auto& [index, constant] : graph_->root()) {
    constant->SetConstantLocation();
    USE(index);
  }
  for (const auto& [value, constant] : graph_->smi()) {
    constant->SetConstantLocation();
    USE(value);
  }
  for (const auto& [value, constant] : graph_->int32()) {
    constant->SetConstantLocation();
    USE(value);
  }
  for (const auto& [value, constant] : graph_->float64()) {
    constant->SetConstantLocation();
    USE(value);
  }

  for (block_it_ = graph_->begin(); block_it_ != graph_->end(); ++block_it_) {
    BasicBlock* block = *block_it_;
    current_node_ = nullptr;

    // Restore mergepoint state.
    if (block->has_state()) {
      if (block->state()->is_exception_handler()) {
        // Exceptions start from a blank state of register values.
        ClearRegisterValues();
      } else {
        InitializeRegisterValues(block->state()->register_state());
      }
    } else if (block->is_empty_block()) {
      InitializeRegisterValues(block->empty_block_register_state());
    }

    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->PreProcessBasicBlock(block);
      printing_visitor_->os() << "live regs: ";
      PrintLiveRegs();

      ControlNode* control = NearestPostDominatingHole(block->control_node());
      if (!control->Is<JumpLoop>()) {
        printing_visitor_->os() << "\n[holes:";
        while (true) {
          if (control->Is<Jump>()) {
            BasicBlock* target = control->Cast<Jump>()->target();
            printing_visitor_->os()
                << " " << control->id() << "-" << target->first_id();
            control = control->next_post_dominating_hole();
            DCHECK_NOT_NULL(control);
            continue;
          } else if (control->Is<Switch>()) {
            Switch* _switch = control->Cast<Switch>();
            DCHECK(!_switch->has_fallthrough());
            DCHECK_GE(_switch->size(), 1);
            BasicBlock* first_target = _switch->targets()[0].block_ptr();
            printing_visitor_->os()
                << " " << control->id() << "-" << first_target->first_id();
            control = control->next_post_dominating_hole();
            DCHECK_NOT_NULL(control);
            continue;
          } else if (control->Is<Return>()) {
            printing_visitor_->os() << " " << control->id() << ".";
            break;
          } else if (control->Is<Deopt>() || control->Is<Abort>()) {
            printing_visitor_->os() << " " << control->id() << "✖️";
            break;
          } else if (control->Is<JumpLoop>()) {
            printing_visitor_->os() << " " << control->id() << "↰";
            break;
          }
          UNREACHABLE();
        }
        printing_visitor_->os() << "]";
      }
      printing_visitor_->os() << std::endl;
    }

    // Activate phis.
    if (block->has_phi()) {
      // Firstly, make the phi live, and try to assign it to an input
      // location.
      for (Phi* phi : *block->phis()) {
        // Ignore dead phis.
        // TODO(leszeks): We should remove dead phis entirely and turn this
        // into a DCHECK.
        if (!phi->has_valid_live_range()) continue;
        phi->SetNoSpillOrHint();
        TryAllocateToInput(phi);
      }
      if (block->is_exception_handler_block()) {
        // If we are in exception handler block, then we find the ExceptionPhi
        // (the first one by default) that is marked with the
        // virtual_accumulator and force kReturnRegister0. This corresponds to
        // the exception message object.
        Phi* phi = block->phis()->first();
        DCHECK_EQ(phi->input_count(), 0);
        if (phi->owner() == interpreter::Register::virtual_accumulator() &&
            !phi->is_dead()) {
          phi->result().SetAllocated(ForceAllocate(kReturnRegister0, phi));
          if (FLAG_trace_maglev_regalloc) {
            printing_visitor_->Process(phi, ProcessingState(block_it_));
            printing_visitor_->os() << "phi (exception message object) "
                                    << phi->result().operand() << std::endl;
          }
        }
      }
      // Secondly try to assign the phi to a free register.
      for (Phi* phi : *block->phis()) {
        // Ignore dead phis.
        // TODO(leszeks): We should remove dead phis entirely and turn this into
        // a DCHECK.
        if (!phi->has_valid_live_range()) continue;
        if (phi->result().operand().IsAllocated()) continue;
        // We assume that Phis are always untagged, and so are always allocated
        // in a general register.
        if (!general_registers_.UnblockedFreeIsEmpty()) {
          compiler::AllocatedOperand allocation =
              general_registers_.AllocateRegister(phi);
          phi->result().SetAllocated(allocation);
          if (FLAG_trace_maglev_regalloc) {
            printing_visitor_->Process(phi, ProcessingState(block_it_));
            printing_visitor_->os()
                << "phi (new reg) " << phi->result().operand() << std::endl;
          }
        }
      }
      // Finally just use a stack slot.
      for (Phi* phi : *block->phis()) {
        // Ignore dead phis.
        // TODO(leszeks): We should remove dead phis entirely and turn this into
        // a DCHECK.
        if (!phi->has_valid_live_range()) continue;
        if (phi->result().operand().IsAllocated()) continue;
        AllocateSpillSlot(phi);
        // TODO(verwaest): Will this be used at all?
        phi->result().SetAllocated(phi->spill_slot());
        if (FLAG_trace_maglev_regalloc) {
          printing_visitor_->Process(phi, ProcessingState(block_it_));
          printing_visitor_->os()
              << "phi (stack) " << phi->result().operand() << std::endl;
        }
      }

      if (FLAG_trace_maglev_regalloc) {
        printing_visitor_->os() << "live regs: ";
        PrintLiveRegs();
        printing_visitor_->os() << std::endl;
      }
      general_registers_.clear_blocked();
      double_registers_.clear_blocked();
    }
    VerifyRegisterState();

    node_it_ = block->nodes().begin();
    for (; node_it_ != block->nodes().end(); ++node_it_) {
      AllocateNode(*node_it_);
    }
    AllocateControlNode(block->control_node(), block);
  }
}

void StraightForwardRegisterAllocator::FreeRegistersUsedBy(ValueNode* node) {
  if (node->use_double_register()) {
    double_registers_.FreeRegistersUsedBy(node);
  } else {
    general_registers_.FreeRegistersUsedBy(node);
  }
}

void StraightForwardRegisterAllocator::UpdateUse(
    ValueNode* node, InputLocation* input_location) {
  DCHECK(!node->is_dead());

  // Update the next use.
  node->set_next_use(input_location->next_use_id());

  if (!node->is_dead()) return;

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "  freeing " << PrintNodeLabel(graph_labeller(), node) << "\n";
  }

  // If a value is dead, make sure it's cleared.
  FreeRegistersUsedBy(node);

  // If the stack slot is a local slot, free it so it can be reused.
  if (node->is_spilled()) {
    compiler::AllocatedOperand slot = node->spill_slot();
    if (slot.index() > 0) {
      SpillSlots& slots =
          slot.representation() == MachineRepresentation::kTagged ? tagged_
                                                                  : untagged_;
      DCHECK_IMPLIES(
          slots.free_slots.size() > 0,
          slots.free_slots.back().freed_at_position <= node->live_range().end);
      slots.free_slots.emplace_back(slot.index(), node->live_range().end);
    }
  }
}

void StraightForwardRegisterAllocator::UpdateUse(
    const EagerDeoptInfo& deopt_info) {
  detail::DeepForEachInput(
      &deopt_info,
      [&](ValueNode* node, interpreter::Register reg, InputLocation* input) {
        if (FLAG_trace_maglev_regalloc) {
          printing_visitor_->os()
              << "- using " << PrintNodeLabel(graph_labeller(), node) << "\n";
        }
        // We might have dropped this node without spilling it. Spill it now.
        if (!node->has_register() && !node->is_loadable()) {
          Spill(node);
        }
        input->InjectLocation(node->allocation());
        UpdateUse(node, input);
      });
}

void StraightForwardRegisterAllocator::UpdateUse(
    const LazyDeoptInfo& deopt_info) {
  const CompactInterpreterFrameState* checkpoint_state =
      deopt_info.state.register_frame;
  int index = 0;
  // TODO(leszeks): This is missing parent recursion, fix it.
  // See also: UpdateUse(EagerDeoptInfo&).
  checkpoint_state->ForEachValue(
      deopt_info.unit, [&](ValueNode* node, interpreter::Register reg) {
        // Skip over the result location since it is irrelevant for lazy deopts
        // (unoptimized code will recreate the result).
        if (deopt_info.IsResultRegister(reg)) return;
        if (FLAG_trace_maglev_regalloc) {
          printing_visitor_->os()
              << "- using " << PrintNodeLabel(graph_labeller(), node) << "\n";
        }
        InputLocation* input = &deopt_info.input_locations[index++];
        // We might have dropped this node without spilling it. Spill it now.
        if (!node->has_register() && !node->is_loadable()) {
          Spill(node);
        }
        input->InjectLocation(node->allocation());
        UpdateUse(node, input);
      });
}

#ifdef DEBUG
namespace {
Register GetNodeResultRegister(Node* node) {
  ValueNode* value_node = node->TryCast<ValueNode>();
  if (!value_node) return Register::no_reg();
  if (!value_node->result().operand().IsRegister()) return Register::no_reg();
  return value_node->result().AssignedGeneralRegister();
}
}  // namespace
#endif  // DEBUG

void StraightForwardRegisterAllocator::AllocateNode(Node* node) {
  // We shouldn't be visiting any gap moves during allocation, we should only
  // have inserted gap moves in past visits.
  DCHECK(!node->Is<GapMove>());
  DCHECK(!node->Is<ConstantGapMove>());

  current_node_ = node;
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "Allocating " << PrintNodeLabel(graph_labeller(), node)
        << " inputs...\n";
  }
  AssignInputs(node);
  VerifyInputs(node);

  if (node->properties().is_call()) SpillAndClearRegisters();

  // Allocate node output.
  if (node->Is<ValueNode>()) {
    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->os() << "Allocating result...\n";
    }
    AllocateNodeResult(node->Cast<ValueNode>());
  }

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os() << "Updating uses...\n";
  }

  // Update uses only after allocating the node result. This order is necessary
  // to avoid emitting input-clobbering gap moves during node result allocation.
  if (node->properties().can_eager_deopt()) {
    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->os() << "Using eager deopt nodes...\n";
    }
    UpdateUse(*node->eager_deopt_info());
  }
  for (Input& input : *node) {
    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->os()
          << "Using input " << PrintNodeLabel(graph_labeller(), input.node())
          << "...\n";
    }
    UpdateUse(&input);
  }

  // Lazy deopts are semantically after the node, so update them last.
  if (node->properties().can_lazy_deopt()) {
    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->os() << "Using lazy deopt nodes...\n";
    }
    UpdateUse(*node->lazy_deopt_info());
  }

  if (node->properties().needs_register_snapshot()) SaveRegisterSnapshot(node);

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->Process(node, ProcessingState(block_it_));
    printing_visitor_->os() << "live regs: ";
    PrintLiveRegs();
    printing_visitor_->os() << "\n";
  }

  // All the temporaries should be free by the end. The exception is the node
  // result, which could be written into a register that was previously
  // considered a temporary.
  DCHECK_EQ(general_registers_.free() |
                (node->temporaries() - GetNodeResultRegister(node)),
            general_registers_.free());
  general_registers_.clear_blocked();
  double_registers_.clear_blocked();
  VerifyRegisterState();
}

template <typename RegisterT>
void StraightForwardRegisterAllocator::DropRegisterValueAtEnd(RegisterT reg) {
  RegisterFrameState<RegisterT>& list = GetRegisterFrameState<RegisterT>();
  list.unblock(reg);
  if (!list.free().has(reg)) {
    ValueNode* node = list.GetValue(reg);
    // If the is not live after the current node, just remove its value.
    if (node->live_range().end == current_node_->id()) {
      node->RemoveRegister(reg);
    } else {
      DropRegisterValue(list, reg);
    }
    list.AddToFree(reg);
  }
}

void StraightForwardRegisterAllocator::AllocateNodeResult(ValueNode* node) {
  DCHECK(!node->Is<Phi>());

  node->SetNoSpillOrHint();

  compiler::UnallocatedOperand operand =
      compiler::UnallocatedOperand::cast(node->result().operand());

  if (operand.basic_policy() == compiler::UnallocatedOperand::FIXED_SLOT) {
    DCHECK(node->Is<InitialValue>());
    DCHECK_LT(operand.fixed_slot_index(), 0);
    // Set the stack slot to exactly where the value is.
    compiler::AllocatedOperand location(compiler::AllocatedOperand::STACK_SLOT,
                                        node->GetMachineRepresentation(),
                                        operand.fixed_slot_index());
    node->result().SetAllocated(location);
    node->Spill(location);
    return;
  }

  switch (operand.extended_policy()) {
    case compiler::UnallocatedOperand::FIXED_REGISTER: {
      Register r = Register::from_code(operand.fixed_register_index());
      DropRegisterValueAtEnd(r);
      node->result().SetAllocated(ForceAllocate(r, node));
      break;
    }

    case compiler::UnallocatedOperand::MUST_HAVE_REGISTER:
      node->result().SetAllocated(AllocateRegisterAtEnd(node));
      break;

    case compiler::UnallocatedOperand::SAME_AS_INPUT: {
      Input& input = node->input(operand.input_index());
      node->result().SetAllocated(ForceAllocate(input, node));
      break;
    }

    case compiler::UnallocatedOperand::FIXED_FP_REGISTER: {
      DoubleRegister r =
          DoubleRegister::from_code(operand.fixed_register_index());
      DropRegisterValueAtEnd(r);
      node->result().SetAllocated(ForceAllocate(r, node));
      break;
    }

    case compiler::UnallocatedOperand::NONE:
      DCHECK(IsConstantNode(node->opcode()));
      break;

    case compiler::UnallocatedOperand::MUST_HAVE_SLOT:
    case compiler::UnallocatedOperand::REGISTER_OR_SLOT:
    case compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT:
      UNREACHABLE();
  }

  // Immediately kill the register use if the node doesn't have a valid
  // live-range.
  // TODO(verwaest): Remove once we can avoid allocating such registers.
  if (!node->has_valid_live_range() &&
      node->result().operand().IsAnyRegister()) {
    DCHECK(node->has_register());
    FreeRegistersUsedBy(node);
    DCHECK(!node->has_register());
    DCHECK(node->is_dead());
  }
}

template <typename RegisterT>
void StraightForwardRegisterAllocator::DropRegisterValue(
    RegisterFrameState<RegisterT>& registers, RegisterT reg) {
  // The register should not already be free.
  DCHECK(!registers.free().has(reg));
  // We are only allowed to allocated blocked registers at the end.
  DCHECK(!registers.is_blocked(reg));

  ValueNode* node = registers.GetValue(reg);

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os() << "  dropping " << reg << " value "
                            << PrintNodeLabel(graph_labeller(), node) << "\n";
  }

  MachineRepresentation mach_repr = node->GetMachineRepresentation();

  // Remove the register from the node's list.
  node->RemoveRegister(reg);
  // Return if the removed value already has another register or is loadable
  // from memory.
  if (node->has_register() || node->is_loadable()) return;
  // Try to move the value to another register. Do so without blocking that
  // register, as we may still want to use it elsewhere.
  if (!registers.UnblockedFreeIsEmpty()) {
    RegisterT target_reg = registers.unblocked_free().first();
    registers.RemoveFromFree(target_reg);
    registers.SetValueWithoutBlocking(target_reg, node);
    // Emit a gapmove.
    compiler::AllocatedOperand source(compiler::LocationOperand::REGISTER,
                                      mach_repr, reg.code());
    compiler::AllocatedOperand target(compiler::LocationOperand::REGISTER,
                                      mach_repr, target_reg.code());
    AddMoveBeforeCurrentNode(node, source, target);
    return;
  }

  // If all else fails, spill the value.
  Spill(node);
}

void StraightForwardRegisterAllocator::DropRegisterValue(Register reg) {
  DropRegisterValue<Register>(general_registers_, reg);
}

void StraightForwardRegisterAllocator::DropRegisterValue(DoubleRegister reg) {
  DropRegisterValue<DoubleRegister>(double_registers_, reg);
}

void StraightForwardRegisterAllocator::InitializeBranchTargetPhis(
    int predecessor_id, BasicBlock* target) {
  DCHECK(!target->is_empty_block());

  if (!target->has_phi()) return;

  // Phi moves are emitted by resolving all phi moves as a single parallel move,
  // which means we shouldn't update register state as we go (as if we were
  // emitting a series of serialised moves) but rather take 'old' register
  // state as the phi input.
  Phi::List* phis = target->phis();
  for (Phi* phi : *phis) {
    // Ignore dead phis.
    // TODO(leszeks): We should remove dead phis entirely and turn this into a
    // DCHECK.
    if (!phi->has_valid_live_range()) continue;

    Input& input = phi->input(predecessor_id);
    input.InjectLocation(input.node()->allocation());
  }
  for (Phi* phi : *phis) UpdateUse(&phi->input(predecessor_id));
}

void StraightForwardRegisterAllocator::InitializeConditionalBranchTarget(
    ConditionalControlNode* control_node, BasicBlock* target) {
  DCHECK(!target->has_phi());

  if (target->has_state()) {
    // Not a fall-through branch, copy the state over.
    return InitializeBranchTargetRegisterValues(control_node, target);
  }
  if (target->is_empty_block()) {
    return InitializeEmptyBlockRegisterValues(control_node, target);
  }

  // Clear dead fall-through registers.
  DCHECK_EQ(control_node->id() + 1, target->first_id());
  ClearDeadFallthroughRegisters<Register>(general_registers_, control_node,
                                          target);
  ClearDeadFallthroughRegisters<DoubleRegister>(double_registers_, control_node,
                                                target);
}

#ifdef DEBUG
namespace {

bool IsReachable(BasicBlock* source_block, BasicBlock* target_block,
                 std::set<BasicBlock*>& visited) {
  if (source_block == target_block) return true;
  if (!visited.insert(source_block).second) return false;

  ControlNode* control_node = source_block->control_node();
  if (UnconditionalControlNode* unconditional =
          control_node->TryCast<UnconditionalControlNode>()) {
    return IsReachable(unconditional->target(), target_block, visited);
  }
  if (BranchControlNode* branch = control_node->TryCast<BranchControlNode>()) {
    return IsReachable(branch->if_true(), target_block, visited) ||
           IsReachable(branch->if_true(), target_block, visited);
  }
  if (Switch* switch_node = control_node->TryCast<Switch>()) {
    const BasicBlockRef* targets = switch_node->targets();
    for (int i = 0; i < switch_node->size(); i++) {
      if (IsReachable(source_block, targets[i].block_ptr(), visited)) {
        return true;
      }
    }
    if (switch_node->has_fallthrough()) {
      if (IsReachable(source_block, switch_node->fallthrough(), visited)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

// Complex predicate for a JumpLoop lifetime extension DCHECK, see comments
// in AllocateControlNode.
bool IsValueFromGeneratorResumeThatDoesNotReachJumpLoop(
    Graph* graph, ValueNode* input_node, BasicBlock* jump_loop_block) {
  // The given node _must_ be created in the generator resume block. This is
  // always the third block -- the first is inital values, the second is the
  // test for an undefined generator, and the third is the generator resume
  // machinery.
  DCHECK_GE(graph->num_blocks(), 3);
  BasicBlock* generator_block = *(graph->begin() + 2);
  DCHECK_EQ(generator_block->control_node()->opcode(), Opcode::kSwitch);

  bool found_node = false;
  for (Node* node : generator_block->nodes()) {
    if (node == input_node) {
      found_node = true;
      break;
    }
  }
  DCHECK(found_node);

  std::set<BasicBlock*> visited;
  bool jump_loop_block_is_reachable_from_generator_block =
      IsReachable(generator_block, jump_loop_block, visited);
  DCHECK(!jump_loop_block_is_reachable_from_generator_block);

  return true;
}
}  // namespace
#endif

void StraightForwardRegisterAllocator::AllocateControlNode(ControlNode* node,
                                                           BasicBlock* block) {
  current_node_ = node;

  // Control nodes can't lazy deopt at the moment.
  DCHECK(!node->properties().can_lazy_deopt());

  if (node->Is<JumpToInlined>() || node->Is<Abort>()) {
    // Do nothing.
    DCHECK(node->temporaries().is_empty());
    DCHECK_EQ(node->num_temporaries_needed(), 0);
    DCHECK_EQ(node->input_count(), 0);
    DCHECK_EQ(node->properties(), OpProperties(0));

    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->Process(node, ProcessingState(block_it_));
    }
  } else if (node->Is<Deopt>()) {
    // No fixed temporaries.
    DCHECK(node->temporaries().is_empty());
    DCHECK_EQ(node->num_temporaries_needed(), 0);
    DCHECK_EQ(node->input_count(), 0);
    DCHECK_EQ(node->properties(), OpProperties::EagerDeopt());

    UpdateUse(*node->eager_deopt_info());

    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->Process(node, ProcessingState(block_it_));
    }
  } else if (auto unconditional = node->TryCast<UnconditionalControlNode>()) {
    // No fixed temporaries.
    DCHECK(node->temporaries().is_empty());
    DCHECK_EQ(node->num_temporaries_needed(), 0);
    DCHECK_EQ(node->input_count(), 0);
    DCHECK(!node->properties().can_eager_deopt());
    DCHECK(!node->properties().can_lazy_deopt());
    DCHECK(!node->properties().needs_register_snapshot());
    DCHECK(!node->properties().is_call());

    auto predecessor_id = block->predecessor_id();
    auto target = unconditional->target();

    InitializeBranchTargetPhis(predecessor_id, target);
    MergeRegisterValues(unconditional, target, predecessor_id);

    // For JumpLoops, now update the uses of any node used in, but not defined
    // in the loop. This makes sure that such nodes' lifetimes are extended to
    // the entire body of the loop. This must be after phi initialisation so
    // that value dropping in the phi initialisation doesn't think these
    // extended lifetime nodes are dead.
    if (auto jump_loop = node->TryCast<JumpLoop>()) {
      for (Input& input : jump_loop->used_nodes()) {
        // Since the value is used by the loop, it must be live somewhere (
        // either in a register or loadable). The exception is when this value
        // is created in a generator resume, and the use of it cannot reach the
        // JumpLoop (e.g. because it returns or deopts on resume).
        DCHECK_IMPLIES(
            !input.node()->has_register() && !input.node()->is_loadable(),
            IsValueFromGeneratorResumeThatDoesNotReachJumpLoop(
                graph_, input.node(), block));
        UpdateUse(&input);
      }
    }

    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->Process(node, ProcessingState(block_it_));
    }
  } else {
    DCHECK(node->Is<ConditionalControlNode>() || node->Is<Return>());
    AssignInputs(node);
    VerifyInputs(node);

    DCHECK(!node->properties().can_eager_deopt());
    for (Input& input : *node) UpdateUse(&input);
    DCHECK(!node->properties().can_lazy_deopt());

    if (node->properties().is_call()) SpillAndClearRegisters();

    DCHECK(!node->properties().needs_register_snapshot());

    DCHECK_EQ(general_registers_.free() | node->temporaries(),
              general_registers_.free());

    general_registers_.clear_blocked();
    double_registers_.clear_blocked();
    VerifyRegisterState();

    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->Process(node, ProcessingState(block_it_));
    }

    // Finally, initialize the merge states of branch targets, including the
    // fallthrough, with the final state after all allocation
    if (auto conditional = node->TryCast<BranchControlNode>()) {
      InitializeConditionalBranchTarget(conditional, conditional->if_true());
      InitializeConditionalBranchTarget(conditional, conditional->if_false());
    } else if (Switch* control_node = node->TryCast<Switch>()) {
      const BasicBlockRef* targets = control_node->targets();
      for (int i = 0; i < control_node->size(); i++) {
        InitializeConditionalBranchTarget(control_node, targets[i].block_ptr());
      }
      if (control_node->has_fallthrough()) {
        InitializeConditionalBranchTarget(control_node,
                                          control_node->fallthrough());
      }
    }
  }

  VerifyRegisterState();
}

void StraightForwardRegisterAllocator::TryAllocateToInput(Phi* phi) {
  // Try allocate phis to a register used by any of the inputs.
  for (Input& input : *phi) {
    if (input.operand().IsRegister()) {
      // We assume Phi nodes only point to tagged values, and so they use a
      // general register.
      Register reg = input.AssignedGeneralRegister();
      if (general_registers_.unblocked_free().has(reg)) {
        phi->result().SetAllocated(ForceAllocate(reg, phi));
        DCHECK_EQ(general_registers_.GetValue(reg), phi);
        if (FLAG_trace_maglev_regalloc) {
          printing_visitor_->Process(phi, ProcessingState(block_it_));
          printing_visitor_->os()
              << "phi (reuse) " << input.operand() << std::endl;
        }
        return;
      }
    }
  }
}

void StraightForwardRegisterAllocator::AddMoveBeforeCurrentNode(
    ValueNode* node, compiler::InstructionOperand source,
    compiler::AllocatedOperand target) {
  Node* gap_move;
  if (source.IsConstant()) {
    DCHECK(IsConstantNode(node->opcode()));
    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->os()
          << "  constant gap move: " << target << " ← "
          << PrintNodeLabel(graph_labeller(), node) << std::endl;
    }
    gap_move =
        Node::New<ConstantGapMove>(compilation_info_->zone(), {}, node, target);
  } else {
    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->os() << "  gap move: " << target << " ← "
                              << PrintNodeLabel(graph_labeller(), node) << ":"
                              << source << std::endl;
    }
    gap_move =
        Node::New<GapMove>(compilation_info_->zone(), {},
                           compiler::AllocatedOperand::cast(source), target);
  }
  if (compilation_info_->has_graph_labeller()) {
    graph_labeller()->RegisterNode(gap_move);
  }
  if (*node_it_ == nullptr) {
    DCHECK(current_node_->Is<ControlNode>());
    // We're at the control node, so append instead.
    (*block_it_)->nodes().Add(gap_move);
    node_it_ = (*block_it_)->nodes().end();
  } else {
    DCHECK_NE(node_it_, (*block_it_)->nodes().end());
    // We should not add any gap move before a GetSecondReturnedValue.
    DCHECK_NE(node_it_->opcode(), Opcode::kGetSecondReturnedValue);
    node_it_.InsertBefore(gap_move);
  }
}

void StraightForwardRegisterAllocator::Spill(ValueNode* node) {
  if (node->is_loadable()) return;
  AllocateSpillSlot(node);
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "  spill: " << node->spill_slot() << " ← "
        << PrintNodeLabel(graph_labeller(), node) << std::endl;
  }
}

void StraightForwardRegisterAllocator::AssignFixedInput(Input& input) {
  compiler::UnallocatedOperand operand =
      compiler::UnallocatedOperand::cast(input.operand());
  ValueNode* node = input.node();
  compiler::InstructionOperand location = node->allocation();

  switch (operand.extended_policy()) {
    case compiler::UnallocatedOperand::MUST_HAVE_REGISTER:
      // Allocated in AssignArbitraryRegisterInput.
      if (FLAG_trace_maglev_regalloc) {
        printing_visitor_->os()
            << "- " << PrintNodeLabel(graph_labeller(), input.node())
            << " has arbitrary register\n";
      }
      return;

    case compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT:
      // Allocated in AssignAnyInput.
      if (FLAG_trace_maglev_regalloc) {
        printing_visitor_->os()
            << "- " << PrintNodeLabel(graph_labeller(), input.node())
            << " has arbitrary location\n";
      }
      return;

    case compiler::UnallocatedOperand::FIXED_REGISTER: {
      Register reg = Register::from_code(operand.fixed_register_index());
      input.SetAllocated(ForceAllocate(reg, node));
      break;
    }

    case compiler::UnallocatedOperand::FIXED_FP_REGISTER: {
      DoubleRegister reg =
          DoubleRegister::from_code(operand.fixed_register_index());
      input.SetAllocated(ForceAllocate(reg, node));
      break;
    }

    case compiler::UnallocatedOperand::REGISTER_OR_SLOT:
    case compiler::UnallocatedOperand::SAME_AS_INPUT:
    case compiler::UnallocatedOperand::NONE:
    case compiler::UnallocatedOperand::MUST_HAVE_SLOT:
      UNREACHABLE();
  }
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "- " << PrintNodeLabel(graph_labeller(), input.node())
        << " in forced " << input.operand() << "\n";
  }

  compiler::AllocatedOperand allocated =
      compiler::AllocatedOperand::cast(input.operand());
  if (location != allocated) {
    AddMoveBeforeCurrentNode(node, location, allocated);
  }
}

void StraightForwardRegisterAllocator::AssignArbitraryRegisterInput(
    Input& input) {
  // Already assigned in AssignFixedInput
  if (!input.operand().IsUnallocated()) return;

  compiler::UnallocatedOperand operand =
      compiler::UnallocatedOperand::cast(input.operand());
  if (operand.extended_policy() ==
      compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT) {
    // Allocated in AssignAnyInput.
    return;
  }

  DCHECK_EQ(operand.extended_policy(),
            compiler::UnallocatedOperand::MUST_HAVE_REGISTER);

  ValueNode* node = input.node();
  compiler::InstructionOperand location = node->allocation();

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "- " << PrintNodeLabel(graph_labeller(), input.node()) << " in "
        << location << "\n";
  }

  if (location.IsAnyRegister()) {
    compiler::AllocatedOperand location =
        node->use_double_register()
            ? double_registers_.ChooseInputRegister(node)
            : general_registers_.ChooseInputRegister(node);
    input.SetAllocated(location);
  } else {
    compiler::AllocatedOperand allocation = AllocateRegister(node);
    input.SetAllocated(allocation);
    DCHECK_NE(location, allocation);
    AddMoveBeforeCurrentNode(node, location, allocation);
  }
}

void StraightForwardRegisterAllocator::AssignAnyInput(Input& input) {
  // Already assigned in AssignFixedInput or AssignArbitraryRegisterInput.
  if (!input.operand().IsUnallocated()) return;

  DCHECK_EQ(
      compiler::UnallocatedOperand::cast(input.operand()).extended_policy(),
      compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT);

  ValueNode* node = input.node();
  compiler::InstructionOperand location = node->allocation();

  input.InjectLocation(location);
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "- " << PrintNodeLabel(graph_labeller(), input.node())
        << " in original " << location << "\n";
  }
}

void StraightForwardRegisterAllocator::AssignInputs(NodeBase* node) {
  // We allocate arbitrary register inputs after fixed inputs, since the fixed
  // inputs may clobber the arbitrarily chosen ones. Finally we assign the
  // location for the remaining inputs. Since inputs can alias a node, one of
  // the inputs could be assigned a register in AssignArbitraryRegisterInput
  // (and respectivelly its node location), therefore we wait until all
  // registers are allocated before assigning any location for these inputs.
  for (Input& input : *node) AssignFixedInput(input);
  AssignFixedTemporaries(node);
  for (Input& input : *node) AssignArbitraryRegisterInput(input);
  AssignArbitraryTemporaries(node);
  for (Input& input : *node) AssignAnyInput(input);
}

void StraightForwardRegisterAllocator::VerifyInputs(NodeBase* node) {
#ifdef DEBUG
  for (Input& input : *node) {
    if (input.operand().IsRegister()) {
      Register reg =
          compiler::AllocatedOperand::cast(input.operand()).GetRegister();
      if (general_registers_.GetValue(reg) != input.node()) {
        FATAL("Input node n%d is not in expected register %s",
              graph_labeller()->NodeId(input.node()), RegisterName(reg));
      }
    } else if (input.operand().IsDoubleRegister()) {
      DoubleRegister reg =
          compiler::AllocatedOperand::cast(input.operand()).GetDoubleRegister();
      if (double_registers_.GetValue(reg) != input.node()) {
        FATAL("Input node n%d is not in expected register %s",
              graph_labeller()->NodeId(input.node()), RegisterName(reg));
      }
    } else {
      if (input.operand() != input.node()->allocation()) {
        std::stringstream ss;
        ss << input.operand();
        FATAL("Input node n%d is not in operand %s",
              graph_labeller()->NodeId(input.node()), ss.str().c_str());
      }
    }
  }
#endif
}

void StraightForwardRegisterAllocator::VerifyRegisterState() {
#ifdef DEBUG
  // We shouldn't have any blocked registers by now.
  DCHECK(general_registers_.blocked().is_empty());
  DCHECK(double_registers_.blocked().is_empty());

  auto NodeNameForFatal = [&](ValueNode* node) {
    std::stringstream ss;
    if (compilation_info_->has_graph_labeller()) {
      ss << PrintNodeLabel(compilation_info_->graph_labeller(), node);
    } else {
      ss << "<" << node << ">";
    }
    return ss.str();
  };

  for (Register reg : general_registers_.used()) {
    ValueNode* node = general_registers_.GetValue(reg);
    if (!node->is_in_register(reg)) {
      FATAL("Node %s doesn't think it is in register %s",
            NodeNameForFatal(node).c_str(), RegisterName(reg));
    }
  }
  for (DoubleRegister reg : double_registers_.used()) {
    ValueNode* node = double_registers_.GetValue(reg);
    if (!node->is_in_register(reg)) {
      FATAL("Node %s doesn't think it is in register %s",
            NodeNameForFatal(node).c_str(), RegisterName(reg));
    }
  }

  auto ValidateValueNode = [this, NodeNameForFatal](ValueNode* node) {
    if (node->use_double_register()) {
      for (DoubleRegister reg : node->result_registers<DoubleRegister>()) {
        if (double_registers_.unblocked_free().has(reg)) {
          FATAL("Node %s thinks it's in register %s but it's free",
                NodeNameForFatal(node).c_str(), RegisterName(reg));
        } else if (double_registers_.GetValue(reg) != node) {
          FATAL("Node %s thinks it's in register %s but it contains %s",
                NodeNameForFatal(node).c_str(), RegisterName(reg),
                NodeNameForFatal(double_registers_.GetValue(reg)).c_str());
        }
      }
    } else {
      for (Register reg : node->result_registers<Register>()) {
        if (general_registers_.unblocked_free().has(reg)) {
          FATAL("Node %s thinks it's in register %s but it's free",
                NodeNameForFatal(node).c_str(), RegisterName(reg));
        } else if (general_registers_.GetValue(reg) != node) {
          FATAL("Node %s thinks it's in register %s but it contains %s",
                NodeNameForFatal(node).c_str(), RegisterName(reg),
                NodeNameForFatal(general_registers_.GetValue(reg)).c_str());
        }
      }
    }
  };

  for (BasicBlock* block : *graph_) {
    if (block->has_phi()) {
      for (Phi* phi : *block->phis()) {
        // Ignore dead phis.
        // TODO(leszeks): We should remove dead phis entirely and turn this into
        // a DCHECK.
        if (!phi->has_valid_live_range()) continue;
        ValidateValueNode(phi);
      }
    }
    for (Node* node : block->nodes()) {
      if (ValueNode* value_node = node->TryCast<ValueNode>()) {
        ValidateValueNode(value_node);
      }
    }
  }

#endif
}

void StraightForwardRegisterAllocator::SpillRegisters() {
  auto spill = [&](auto reg, ValueNode* node) { Spill(node); };
  general_registers_.ForEachUsedRegister(spill);
  double_registers_.ForEachUsedRegister(spill);
}

template <typename RegisterT>
void StraightForwardRegisterAllocator::SpillAndClearRegisters(
    RegisterFrameState<RegisterT>& registers) {
  while (registers.used() != registers.empty()) {
    RegisterT reg = registers.used().first();
    ValueNode* node = registers.GetValue(reg);
    if (FLAG_trace_maglev_regalloc) {
      printing_visitor_->os() << "  clearing registers with "
                              << PrintNodeLabel(graph_labeller(), node) << "\n";
    }
    Spill(node);
    registers.FreeRegistersUsedBy(node);
    DCHECK(!registers.used().has(reg));
  }
}

void StraightForwardRegisterAllocator::SpillAndClearRegisters() {
  SpillAndClearRegisters(general_registers_);
  SpillAndClearRegisters(double_registers_);
}

void StraightForwardRegisterAllocator::SaveRegisterSnapshot(NodeBase* node) {
  RegisterSnapshot snapshot;
  general_registers_.ForEachUsedRegister([&](Register reg, ValueNode* node) {
    if (node->properties().value_representation() ==
        ValueRepresentation::kTagged) {
      snapshot.live_tagged_registers.set(reg);
    }
  });
  snapshot.live_registers = general_registers_.used();
  snapshot.live_double_registers = double_registers_.used();
  node->set_register_snapshot(snapshot);
}

void StraightForwardRegisterAllocator::AllocateSpillSlot(ValueNode* node) {
  DCHECK(!node->is_loadable());
  uint32_t free_slot;
  bool is_tagged = (node->properties().value_representation() ==
                    ValueRepresentation::kTagged);
  // TODO(v8:7700): We will need a new class of SpillSlots for doubles in 32-bit
  // architectures.
  SpillSlots& slots = is_tagged ? tagged_ : untagged_;
  MachineRepresentation representation = node->GetMachineRepresentation();
  if (!FLAG_maglev_reuse_stack_slots || slots.free_slots.empty()) {
    free_slot = slots.top++;
  } else {
    NodeIdT start = node->live_range().start;
    auto it =
        std::upper_bound(slots.free_slots.begin(), slots.free_slots.end(),
                         start, [](NodeIdT s, const SpillSlotInfo& slot_info) {
                           return slot_info.freed_at_position < s;
                         });
    if (it != slots.free_slots.end()) {
      free_slot = it->slot_index;
      slots.free_slots.erase(it);
    } else {
      free_slot = slots.top++;
    }
  }
  node->Spill(compiler::AllocatedOperand(compiler::AllocatedOperand::STACK_SLOT,
                                         representation, free_slot));
}

template <typename RegisterT>
RegisterT StraightForwardRegisterAllocator::PickRegisterToFree(
    RegListBase<RegisterT> reserved) {
  RegisterFrameState<RegisterT>& registers = GetRegisterFrameState<RegisterT>();
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os() << "  need to free a register... ";
  }
  int furthest_use = 0;
  RegisterT best = RegisterT::no_reg();
  for (RegisterT reg : (registers.used() - reserved)) {
    ValueNode* value = registers.GetValue(reg);

    // The cheapest register to clear is a register containing a value that's
    // contained in another register as well. Since we found the register while
    // looping over unblocked registers, we can simply use this register.
    if (value->num_registers() > 1) {
      best = reg;
      break;
    }
    int use = value->next_use();
    if (use > furthest_use) {
      furthest_use = use;
      best = reg;
    }
  }
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "  chose " << best << " with next use " << furthest_use << "\n";
  }
  return best;
}

template <typename RegisterT>
RegisterT StraightForwardRegisterAllocator::FreeUnblockedRegister() {
  RegisterFrameState<RegisterT>& registers = GetRegisterFrameState<RegisterT>();
  RegisterT best = PickRegisterToFree<RegisterT>(registers.blocked());
  DCHECK(best.is_valid());
  DropRegisterValue(registers, best);
  registers.AddToFree(best);
  return best;
}

compiler::AllocatedOperand StraightForwardRegisterAllocator::AllocateRegister(
    ValueNode* node) {
  compiler::InstructionOperand allocation;
  if (node->use_double_register()) {
    if (double_registers_.UnblockedFreeIsEmpty()) {
      FreeUnblockedRegister<DoubleRegister>();
    }
    return double_registers_.AllocateRegister(node);
  } else {
    if (general_registers_.UnblockedFreeIsEmpty()) {
      FreeUnblockedRegister<Register>();
    }
    return general_registers_.AllocateRegister(node);
  }
}

template <typename RegisterT>
void StraightForwardRegisterAllocator::EnsureFreeRegisterAtEnd() {
  RegisterFrameState<RegisterT>& registers = GetRegisterFrameState<RegisterT>();
  // If we still have free registers, pick one of those.
  if (!registers.free().is_empty()) {
    // Make sure that at least one of the free registers is not blocked; this
    // effectively means freeing up a temporary.
    if (registers.unblocked_free().is_empty()) {
      registers.unblock(registers.free().first());
    }
    return;
  }

  // If the current node is a last use of an input, pick a register containing
  // the input.
  for (RegisterT reg : registers.blocked()) {
    if (registers.GetValue(reg)->live_range().end == current_node_->id()) {
      DropRegisterValueAtEnd(reg);
      return;
    }
  }

  // Pick any input-blocked register based on regular heuristics.
  RegisterT reg = PickRegisterToFree<RegisterT>(registers.empty());
  DropRegisterValueAtEnd(reg);
}

compiler::AllocatedOperand
StraightForwardRegisterAllocator::AllocateRegisterAtEnd(ValueNode* node) {
  if (node->use_double_register()) {
    EnsureFreeRegisterAtEnd<DoubleRegister>();
    return double_registers_.AllocateRegister(node);
  } else {
    EnsureFreeRegisterAtEnd<Register>();
    return general_registers_.AllocateRegister(node);
  }
}

template <typename RegisterT>
compiler::AllocatedOperand StraightForwardRegisterAllocator::ForceAllocate(
    RegisterFrameState<RegisterT>& registers, RegisterT reg, ValueNode* node) {
  DCHECK(!registers.is_blocked(reg));
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "  forcing " << reg << " to "
        << PrintNodeLabel(graph_labeller(), node) << "...\n";
  }
  if (registers.free().has(reg)) {
    // If it's already free, remove it from the free list.
    registers.RemoveFromFree(reg);
  } else if (registers.GetValue(reg) == node) {
    registers.block(reg);
    return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                      node->GetMachineRepresentation(),
                                      reg.code());
  } else {
    DropRegisterValue(registers, reg);
  }
#ifdef DEBUG
  DCHECK(!registers.free().has(reg));
#endif
  registers.unblock(reg);
  registers.SetValue(reg, node);
  return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                    node->GetMachineRepresentation(),
                                    reg.code());
}

compiler::AllocatedOperand StraightForwardRegisterAllocator::ForceAllocate(
    Register reg, ValueNode* node) {
  DCHECK(!node->use_double_register());
  return ForceAllocate<Register>(general_registers_, reg, node);
}

compiler::AllocatedOperand StraightForwardRegisterAllocator::ForceAllocate(
    DoubleRegister reg, ValueNode* node) {
  DCHECK(node->use_double_register());
  return ForceAllocate<DoubleRegister>(double_registers_, reg, node);
}

compiler::AllocatedOperand StraightForwardRegisterAllocator::ForceAllocate(
    const Input& input, ValueNode* node) {
  if (input.IsDoubleRegister()) {
    DoubleRegister reg = input.AssignedDoubleRegister();
    DropRegisterValueAtEnd(reg);
    return ForceAllocate(reg, node);
  } else {
    Register reg = input.AssignedGeneralRegister();
    DropRegisterValueAtEnd(reg);
    return ForceAllocate(reg, node);
  }
}

template <typename RegisterT>
compiler::AllocatedOperand RegisterFrameState<RegisterT>::ChooseInputRegister(
    ValueNode* node) {
  RegTList blocked = node->result_registers<RegisterT>() & blocked_;
  if (blocked.Count() > 0) {
    return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                      node->GetMachineRepresentation(),
                                      blocked.first().code());
  }
  compiler::AllocatedOperand allocation =
      compiler::AllocatedOperand::cast(node->allocation());
  if constexpr (std::is_same<RegisterT, DoubleRegister>::value) {
    block(allocation.GetDoubleRegister());
  } else {
    block(allocation.GetRegister());
  }
  return allocation;
}

template <typename RegisterT>
compiler::AllocatedOperand RegisterFrameState<RegisterT>::AllocateRegister(
    ValueNode* node) {
  DCHECK(!unblocked_free().is_empty());
  RegisterT reg = unblocked_free().first();
  RemoveFromFree(reg);

  // Allocation succeeded. This might have found an existing allocation.
  // Simply update the state anyway.
  SetValue(reg, node);
  return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                    node->GetMachineRepresentation(),
                                    reg.code());
}

void StraightForwardRegisterAllocator::AssignFixedTemporaries(NodeBase* node) {
  // TODO(victorgomes): Support double registers as temporaries.
  RegList fixed_temporaries = node->temporaries();

  // Make sure that any initially set temporaries are definitely free.
  for (Register reg : fixed_temporaries) {
    DCHECK(!general_registers_.is_blocked(reg));
    if (!general_registers_.free().has(reg)) {
      DropRegisterValue(general_registers_, reg);
      general_registers_.AddToFree(reg);
    }
    general_registers_.block(reg);
  }

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os()
        << "Fixed temporaries: " << fixed_temporaries << "\n";
  }
}

void StraightForwardRegisterAllocator::AssignArbitraryTemporaries(
    NodeBase* node) {
  int num_temporaries_needed = node->num_temporaries_needed();
  if (num_temporaries_needed == 0) return;

  RegList temporaries = node->temporaries();

  // TODO(victorgomes): Support double registers as temporaries.
  for (Register reg : general_registers_.unblocked_free()) {
    general_registers_.block(reg);
    DCHECK(!temporaries.has(reg));
    temporaries.set(reg);
    if (--num_temporaries_needed == 0) break;
  }

  // Free extra registers if necessary.
  for (int i = 0; i < num_temporaries_needed; ++i) {
    DCHECK(general_registers_.UnblockedFreeIsEmpty());
    Register reg = FreeUnblockedRegister<Register>();
    general_registers_.block(reg);
    DCHECK(!temporaries.has(reg));
    temporaries.set(reg);
  }

  DCHECK_GE(temporaries.Count(), node->num_temporaries_needed());
  node->assign_temporaries(temporaries);
  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os() << "Temporaries: " << temporaries << "\n";
  }
}

namespace {
template <typename RegisterT>
void ClearRegisterState(RegisterFrameState<RegisterT>& registers) {
  while (!registers.used().is_empty()) {
    RegisterT reg = registers.used().first();
    ValueNode* node = registers.GetValue(reg);
    registers.FreeRegistersUsedBy(node);
    DCHECK(!registers.used().has(reg));
  }
}
}  // namespace

template <typename Function>
void StraightForwardRegisterAllocator::ForEachMergePointRegisterState(
    MergePointRegisterState& merge_point_state, Function&& f) {
  merge_point_state.ForEachGeneralRegister(
      [&](Register reg, RegisterState& state) {
        f(general_registers_, reg, state);
      });
  merge_point_state.ForEachDoubleRegister(
      [&](DoubleRegister reg, RegisterState& state) {
        f(double_registers_, reg, state);
      });
}

void StraightForwardRegisterAllocator::ClearRegisterValues() {
  ClearRegisterState(general_registers_);
  ClearRegisterState(double_registers_);

  // All registers should be free by now.
  DCHECK_EQ(general_registers_.unblocked_free(), kAllocatableGeneralRegisters);
  DCHECK_EQ(double_registers_.unblocked_free(), kAllocatableDoubleRegisters);
}

void StraightForwardRegisterAllocator::InitializeRegisterValues(
    MergePointRegisterState& target_state) {
  // First clear the register state.
  ClearRegisterValues();

  // Then fill it in with target information.
  auto fill = [&](auto& registers, auto reg, RegisterState& state) {
    ValueNode* node;
    RegisterMerge* merge;
    LoadMergeState(state, &node, &merge);
    if (node != nullptr) {
      registers.RemoveFromFree(reg);
      registers.SetValue(reg, node);
    } else {
      DCHECK(!state.GetPayload().is_merge);
    }
  };
  ForEachMergePointRegisterState(target_state, fill);

  // SetValue will have blocked registers, unblock them.
  general_registers_.clear_blocked();
  double_registers_.clear_blocked();
}

#ifdef DEBUG
bool StraightForwardRegisterAllocator::IsInRegister(
    MergePointRegisterState& target_state, ValueNode* incoming) {
  bool found = false;
  auto find = [&found, &incoming](auto reg, RegisterState& state) {
    ValueNode* node;
    RegisterMerge* merge;
    LoadMergeState(state, &node, &merge);
    if (node == incoming) found = true;
  };
  if (incoming->use_double_register()) {
    target_state.ForEachDoubleRegister(find);
  } else {
    target_state.ForEachGeneralRegister(find);
  }
  return found;
}
#endif

void StraightForwardRegisterAllocator::InitializeBranchTargetRegisterValues(
    ControlNode* source, BasicBlock* target) {
  MergePointRegisterState& target_state = target->state()->register_state();
  DCHECK(!target_state.is_initialized());
  auto init = [&](auto& registers, auto reg, RegisterState& state) {
    ValueNode* node = nullptr;
    DCHECK(registers.blocked().is_empty());
    if (!registers.free().has(reg)) {
      node = registers.GetValue(reg);
      if (!IsLiveAtTarget(node, source, target)) node = nullptr;
    }
    state = {node, initialized_node};
  };
  ForEachMergePointRegisterState(target_state, init);
}

void StraightForwardRegisterAllocator::InitializeEmptyBlockRegisterValues(
    ControlNode* source, BasicBlock* target) {
  DCHECK(target->is_empty_block());
  MergePointRegisterState* register_state =
      compilation_info_->zone()->New<MergePointRegisterState>();

  DCHECK(!register_state->is_initialized());
  auto init = [&](auto& registers, auto reg, RegisterState& state) {
    ValueNode* node = nullptr;
    DCHECK(registers.blocked().is_empty());
    if (!registers.free().has(reg)) {
      node = registers.GetValue(reg);
      if (!IsLiveAtTarget(node, source, target)) node = nullptr;
    }
    state = {node, initialized_node};
  };
  ForEachMergePointRegisterState(*register_state, init);

  target->set_empty_block_register_state(register_state);
}

void StraightForwardRegisterAllocator::MergeRegisterValues(ControlNode* control,
                                                           BasicBlock* target,
                                                           int predecessor_id) {
  if (target->is_empty_block()) {
    return InitializeEmptyBlockRegisterValues(control, target);
  }

  MergePointRegisterState& target_state = target->state()->register_state();
  if (!target_state.is_initialized()) {
    // This is the first block we're merging, initialize the values.
    return InitializeBranchTargetRegisterValues(control, target);
  }

  if (FLAG_trace_maglev_regalloc) {
    printing_visitor_->os() << "Merging registers...\n";
  }

  int predecessor_count = target->state()->predecessor_count();
  auto merge = [&](auto& registers, auto reg, RegisterState& state) {
    ValueNode* node;
    RegisterMerge* merge;
    LoadMergeState(state, &node, &merge);

    // This isn't quite the right machine representation for Int32 nodes, but
    // those are stored in the same registers as Tagged nodes so in this case it
    // doesn't matter.
    MachineRepresentation mach_repr = std::is_same_v<decltype(reg), Register>
                                          ? MachineRepresentation::kTagged
                                          : MachineRepresentation::kFloat64;
    compiler::AllocatedOperand register_info = {
        compiler::LocationOperand::REGISTER, mach_repr, reg.code()};

    ValueNode* incoming = nullptr;
    DCHECK(registers.blocked().is_empty());
    if (!registers.free().has(reg)) {
      incoming = registers.GetValue(reg);
      if (!IsLiveAtTarget(incoming, control, target)) {
        if (FLAG_trace_maglev_regalloc) {
          printing_visitor_->os() << "  " << reg << " - incoming node "
                                  << PrintNodeLabel(graph_labeller(), incoming)
                                  << " dead at target\n";
        }
        incoming = nullptr;
      }
    }

    if (incoming == node) {
      // We're using the same register as the target already has. If registers
      // are merged, add input information.
      if (FLAG_trace_maglev_regalloc) {
        if (node) {
          printing_visitor_->os()
              << "  " << reg << " - incoming node same as node: "
              << PrintNodeLabel(graph_labeller(), node) << "\n";
        }
      }
      if (merge) merge->operand(predecessor_id) = register_info;
      return;
    }

    if (merge) {
      // The register is already occupied with a different node. Figure out
      // where that node is allocated on the incoming branch.
      merge->operand(predecessor_id) = node->allocation();
      if (FLAG_trace_maglev_regalloc) {
        printing_visitor_->os() << "  " << reg << " - merge: loading "
                                << PrintNodeLabel(graph_labeller(), node)
                                << " from " << node->allocation() << " \n";
      }

      // If there's a value in the incoming state, that value is either
      // already spilled or in another place in the merge state.
      if (incoming != nullptr && !incoming->is_loadable()) {
        DCHECK(IsInRegister(target_state, incoming));
      }
      return;
    }

    DCHECK_IMPLIES(node == nullptr, incoming != nullptr);
    if (node == nullptr && !incoming->is_loadable()) {
      // If the register is unallocated at the merge point, and the incoming
      // value isn't spilled, that means we must have seen it already in a
      // different register.
      // This maybe not be true for conversion nodes, as they can split and take
      // over the liveness of the node they are converting.
      // TODO(v8:7700): This DCHECK is overeager, {incoming} can be a Phi node
      // containing conversion nodes.
      // DCHECK_IMPLIES(!IsInRegister(target_state, incoming),
      //                incoming->properties().is_conversion());
      if (FLAG_trace_maglev_regalloc) {
        printing_visitor_->os()
            << "  " << reg << " - can't load incoming "
            << PrintNodeLabel(graph_labeller(), node) << ", bailing out\n";
      }
      return;
    }

    if (node != nullptr && !node->is_loadable() && !node->has_register()) {
      // If we have a node already, but can't load it here, we must be in a
      // liveness hole for it, so nuke the merge state.
      // This can only happen for conversion nodes, as they can split and take
      // over the liveness of the node they are converting.
      // TODO(v8:7700): Overeager DCHECK.
      // DCHECK(node->properties().is_conversion());
      if (FLAG_trace_maglev_regalloc) {
        printing_visitor_->os() << "  " << reg << " - can't load "
                                << PrintNodeLabel(graph_labeller(), node)
                                << ", dropping the merge\n";
      }
      state = {nullptr, initialized_node};
      return;
    }

    const size_t size = sizeof(RegisterMerge) +
                        predecessor_count * sizeof(compiler::AllocatedOperand);
    void* buffer = compilation_info_->zone()->Allocate<void*>(size);
    merge = new (buffer) RegisterMerge();
    merge->node = node == nullptr ? incoming : node;

    // If the register is unallocated at the merge point, allocation so far
    // is the loadable slot for the incoming value. Otherwise all incoming
    // branches agree that the current node is in the register info.
    compiler::InstructionOperand info_so_far =
        node == nullptr ? incoming->loadable_slot() : register_info;

    // Initialize the entire array with info_so_far since we don't know in
    // which order we've seen the predecessors so far. Predecessors we
    // haven't seen yet will simply overwrite their entry later.
    for (int i = 0; i < predecessor_count; i++) {
      merge->operand(i) = info_so_far;
    }
    // If the register is unallocated at the merge point, fill in the
    // incoming value. Otherwise find the merge-point node in the incoming
    // state.
    if (node == nullptr) {
      merge->operand(predecessor_id) = register_info;
      if (FLAG_trace_maglev_regalloc) {
        printing_visitor_->os() << "  " << reg << " - new merge: loading new "
                                << PrintNodeLabel(graph_labeller(), incoming)
                                << " from " << register_info << " \n";
      }
    } else {
      merge->operand(predecessor_id) = node->allocation();
      if (FLAG_trace_maglev_regalloc) {
        printing_visitor_->os() << "  " << reg << " - new merge: loading "
                                << PrintNodeLabel(graph_labeller(), node)
                                << " from " << node->allocation() << " \n";
      }
    }
    state = {merge, initialized_merge};
  };
  ForEachMergePointRegisterState(target_state, merge);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
