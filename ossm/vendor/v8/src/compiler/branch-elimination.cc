// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/branch-elimination.h"

#include "src/base/small-vector.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

BranchElimination::BranchElimination(Editor* editor, JSGraph* js_graph,
                                     Zone* zone, Phase phase)
    : AdvancedReducerWithControlPathState(editor, zone, js_graph->graph()),
      jsgraph_(js_graph),
      dead_(js_graph->Dead()),
      phase_(phase) {}

BranchElimination::~BranchElimination() = default;

Reduction BranchElimination::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kDead:
      return NoChange();
    case IrOpcode::kDeoptimizeIf:
    case IrOpcode::kDeoptimizeUnless:
      return ReduceDeoptimizeConditional(node);
    case IrOpcode::kMerge:
      return ReduceMerge(node);
    case IrOpcode::kLoop:
      return ReduceLoop(node);
    case IrOpcode::kBranch:
      return ReduceBranch(node);
    case IrOpcode::kIfFalse:
      return ReduceIf(node, false);
    case IrOpcode::kIfTrue:
      return ReduceIf(node, true);
    case IrOpcode::kTrapIf:
    case IrOpcode::kTrapUnless:
      return ReduceTrapConditional(node);
    case IrOpcode::kStart:
      return ReduceStart(node);
    default:
      if (node->op()->ControlOutputCount() > 0) {
        return ReduceOtherControl(node);
      } else {
        return NoChange();
      }
  }
}

void BranchElimination::SimplifyBranchCondition(Node* branch) {
  // Try to use a phi as a branch condition if the control flow from the branch
  // is known from previous branches. For example, in the graph below, the
  // control flow of the second_branch is predictable because the first_branch
  // use the same branch condition. In such case, create a new phi with constant
  // inputs and let the second branch use the phi as its branch condition. From
  // this transformation, more branch folding opportunities would be exposed to
  // later passes through branch cloning in effect-control-linearizer.
  //
  // condition                             condition
  //    |   \                                   |
  //    |  first_branch                        first_branch
  //    |   /          \                       /          \
  //    |  /            \                     /            \
  //    |first_true  first_false           first_true  first_false
  //    |  \           /                      \           /
  //    |   \         /                        \         /
  //    |  first_merge           ==>          first_merge
  //    |       |                              /    |
  //   second_branch                    1  0  /     |
  //    /          \                     \ | /      |
  //   /            \                     phi       |
  // second_true  second_false              \       |
  //                                      second_branch
  //                                      /          \
  //                                     /            \
  //                                   second_true  second_false
  //

  DCHECK_EQ(IrOpcode::kBranch, branch->opcode());
  Node* merge = NodeProperties::GetControlInput(branch);
  if (merge->opcode() != IrOpcode::kMerge) return;

  Node* condition = branch->InputAt(0);
  Graph* graph = jsgraph()->graph();
  base::SmallVector<Node*, 2> phi_inputs;

  Node::Inputs inputs = merge->inputs();
  int input_count = inputs.count();
  for (int i = 0; i != input_count; ++i) {
    Node* input = inputs[i];
    ControlPathConditions from_input = GetState(input);

    BranchCondition branch_condition = from_input.LookupState(condition);
    if (!branch_condition.IsSet()) return;
    bool condition_value = branch_condition.is_true;

    if (phase_ == kEARLY) {
      phi_inputs.emplace_back(condition_value ? jsgraph()->TrueConstant()
                                              : jsgraph()->FalseConstant());
    } else {
      phi_inputs.emplace_back(
          condition_value
              ? graph->NewNode(jsgraph()->common()->Int32Constant(1))
              : graph->NewNode(jsgraph()->common()->Int32Constant(0)));
    }
  }
  phi_inputs.emplace_back(merge);
  Node* new_phi = graph->NewNode(
      common()->Phi(phase_ == kEARLY ? MachineRepresentation::kTagged
                                     : MachineRepresentation::kWord32,
                    input_count),
      input_count + 1, &phi_inputs.at(0));

  // Replace the branch condition with the new phi.
  NodeProperties::ReplaceValueInput(branch, new_phi, 0);
}

Reduction BranchElimination::ReduceBranch(Node* node) {
  Node* condition = node->InputAt(0);
  Node* control_input = NodeProperties::GetControlInput(node, 0);
  if (!IsReduced(control_input)) return NoChange();
  ControlPathConditions from_input = GetState(control_input);
  // If we know the condition we can discard the branch.
  BranchCondition branch_condition = from_input.LookupState(condition);
  if (branch_condition.IsSet()) {
    bool condition_value = branch_condition.is_true;
    for (Node* const use : node->uses()) {
      switch (use->opcode()) {
        case IrOpcode::kIfTrue:
          Replace(use, condition_value ? control_input : dead());
          break;
        case IrOpcode::kIfFalse:
          Replace(use, condition_value ? dead() : control_input);
          break;
        default:
          UNREACHABLE();
      }
    }
    return Replace(dead());
  }
  SimplifyBranchCondition(node);
  // Trigger revisits of the IfTrue/IfFalse projections, since they depend on
  // the branch condition.
  for (Node* const use : node->uses()) {
    Revisit(use);
  }
  return TakeStatesFromFirstControl(node);
}

Reduction BranchElimination::ReduceTrapConditional(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kTrapIf ||
         node->opcode() == IrOpcode::kTrapUnless);
  bool trapping_condition = node->opcode() == IrOpcode::kTrapIf;
  Node* condition = node->InputAt(0);
  Node* control_input = NodeProperties::GetControlInput(node, 0);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (!IsReduced(control_input)) return NoChange();

  ControlPathConditions from_input = GetState(control_input);

  BranchCondition branch_condition = from_input.LookupState(condition);
  if (branch_condition.IsSet()) {
    bool condition_value = branch_condition.is_true;
    if (condition_value == trapping_condition) {
      // This will always trap. Mark its outputs as dead and connect it to
      // graph()->end().
      ReplaceWithValue(node, dead(), dead(), dead());
      Node* control = graph()->NewNode(common()->Throw(), node, node);
      NodeProperties::MergeControlToEnd(graph(), common(), control);
      Revisit(graph()->end());
      return Changed(node);
    } else {
      // This will not trap, remove it by relaxing effect/control.
      RelaxEffectsAndControls(node);
      Node* control = NodeProperties::GetControlInput(node);
      node->Kill();
      return Replace(control);  // Irrelevant argument
    }
  }
  return UpdateStatesHelper(node, from_input, condition, node,
                            !trapping_condition, false);
}

Reduction BranchElimination::ReduceDeoptimizeConditional(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kDeoptimizeIf ||
         node->opcode() == IrOpcode::kDeoptimizeUnless);
  bool condition_is_true = node->opcode() == IrOpcode::kDeoptimizeUnless;
  DeoptimizeParameters p = DeoptimizeParametersOf(node->op());
  Node* condition = NodeProperties::GetValueInput(node, 0);
  Node* frame_state = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (!IsReduced(control)) {
    return NoChange();
  }

  ControlPathConditions conditions = GetState(control);
  BranchCondition branch_condition = conditions.LookupState(condition);
  if (branch_condition.IsSet()) {
    // If we know the condition we can discard the branch.
    bool condition_value = branch_condition.is_true;
    if (condition_is_true == condition_value) {
      // We don't update the conditions here, because we're replacing {node}
      // with the {control} node that already contains the right information.
      ReplaceWithValue(node, dead(), effect, control);
    } else {
      control = graph()->NewNode(common()->Deoptimize(p.reason(), p.feedback()),
                                 frame_state, effect, control);
      // TODO(bmeurer): This should be on the AdvancedReducer somehow.
      NodeProperties::MergeControlToEnd(graph(), common(), control);
      Revisit(graph()->end());
    }
    return Replace(dead());
  }
  return UpdateStatesHelper(node, conditions, condition, node,
                            condition_is_true, false);
}

Reduction BranchElimination::ReduceIf(Node* node, bool is_true_branch) {
  // Add the condition to the list arriving from the input branch.
  Node* branch = NodeProperties::GetControlInput(node, 0);
  ControlPathConditions from_branch = GetState(branch);
  // If we do not know anything about the predecessor, do not propagate just
  // yet because we will have to recompute anyway once we compute the
  // predecessor.
  if (!IsReduced(branch)) {
    return NoChange();
  }
  Node* condition = branch->InputAt(0);
  return UpdateStatesHelper(node, from_branch, condition, branch,
                            is_true_branch, true);
}

Reduction BranchElimination::ReduceLoop(Node* node) {
  // Here we rely on having only reducible loops:
  // The loop entry edge always dominates the header, so we can just use
  // the information from the loop entry edge.
  return TakeStatesFromFirstControl(node);
}

Reduction BranchElimination::ReduceMerge(Node* node) {
  // Shortcut for the case when we do not know anything about some
  // input.
  Node::Inputs inputs = node->inputs();
  for (Node* input : inputs) {
    if (!IsReduced(input)) {
      return NoChange();
    }
  }

  auto input_it = inputs.begin();

  DCHECK_GT(inputs.count(), 0);

  ControlPathConditions conditions = GetState(*input_it);
  ++input_it;
  // Merge the first input's conditions with the conditions from the other
  // inputs.
  auto input_end = inputs.end();
  for (; input_it != input_end; ++input_it) {
    // Change the current condition block list to a longest common tail of this
    // condition list and the other list. (The common tail should correspond to
    // the list from the common dominator.)
    conditions.ResetToCommonAncestor(GetState(*input_it));
  }
  return UpdateStates(node, conditions);
}

Reduction BranchElimination::ReduceStart(Node* node) {
  return UpdateStates(node, ControlPathConditions(zone()));
}

Reduction BranchElimination::ReduceOtherControl(Node* node) {
  DCHECK_EQ(1, node->op()->ControlInputCount());
  return TakeStatesFromFirstControl(node);
}

Graph* BranchElimination::graph() const { return jsgraph()->graph(); }

Isolate* BranchElimination::isolate() const { return jsgraph()->isolate(); }

CommonOperatorBuilder* BranchElimination::common() const {
  return jsgraph()->common();
}

// Workaround a gcc bug causing link errors.
// Related issue: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=105848
template bool DefaultConstruct<bool>(Zone* zone);

}  // namespace compiler
}  // namespace internal
}  // namespace v8
