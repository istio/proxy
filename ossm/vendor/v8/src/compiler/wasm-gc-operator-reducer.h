// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_WASM_GC_OPERATOR_REDUCER_H_
#define V8_COMPILER_WASM_GC_OPERATOR_REDUCER_H_

#include "src/compiler/control-path-state.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/wasm-graph-assembler.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8 {
namespace internal {
namespace compiler {

class MachineGraph;

struct NodeWithType {
  NodeWithType() : node(nullptr), type(wasm::kWasmVoid, nullptr) {}
  NodeWithType(Node* node, wasm::TypeInModule type) : node(node), type(type) {}

  bool operator==(const NodeWithType& other) const {
    return node == other.node && type == other.type;
  }
  bool operator!=(const NodeWithType& other) const { return !(*this == other); }

  bool IsSet() { return node != nullptr; }

  Node* node;
  wasm::TypeInModule type;
};

// This class optimizes away wasm-gc nodes based on the types of their
// arguments. Although types have been assigned to nodes already, this class
// also tracks additional type information along control paths.
class WasmGCOperatorReducer final
    : public AdvancedReducerWithControlPathState<NodeWithType,
                                                 kMultipleInstances> {
 public:
  WasmGCOperatorReducer(Editor* editor, Zone* temp_zone_, MachineGraph* mcgraph,
                        const wasm::WasmModule* module);

  const char* reducer_name() const override { return "WasmGCOperatorReducer"; }

  Reduction Reduce(Node* node) final;

 private:
  using ControlPathTypes = ControlPathState<NodeWithType, kMultipleInstances>;

  Reduction ReduceAssertNotNull(Node* node);
  Reduction ReduceCheckNull(Node* node);
  Reduction ReduceWasmTypeCheck(Node* node);
  Reduction ReduceWasmTypeCast(Node* node);
  Reduction ReduceMerge(Node* node);
  Reduction ReduceIf(Node* node, bool condition);
  Reduction ReduceStart(Node* node);

  Node* SetType(Node* node, wasm::ValueType type);
  wasm::TypeInModule ObjectTypeFromContext(Node* object, Node* control);
  Reduction UpdateNodeAndAliasesTypes(Node* state_owner,
                                      ControlPathTypes parent_state, Node* node,
                                      wasm::TypeInModule type,
                                      bool in_new_block);

  Graph* graph() { return mcgraph_->graph(); }
  CommonOperatorBuilder* common() { return mcgraph_->common(); }

  MachineGraph* mcgraph_;
  WasmGraphAssembler gasm_;
  const wasm::WasmModule* module_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_GC_OPERATOR_REDUCER_H_
