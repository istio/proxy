// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_WASM_GRAPH_ASSEMBLER_H_
#define V8_COMPILER_WASM_GRAPH_ASSEMBLER_H_

#include "src/compiler/graph-assembler.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {
namespace compiler {

constexpr Builtin WasmRuntimeStubIdToBuiltinName(
    wasm::WasmCode::RuntimeStubId runtime_stub_id) {
  switch (runtime_stub_id) {
#define DEF_CASE(name)          \
  case wasm::WasmCode::k##name: \
    return Builtin::k##name;
#define DEF_TRAP_CASE(name) DEF_CASE(ThrowWasm##name)
    WASM_RUNTIME_STUB_LIST(DEF_CASE, DEF_TRAP_CASE)
#undef DEF_CASE
#undef DEF_TRAP_CASE
    default:
      UNREACHABLE();
  }
}

CallDescriptor* GetBuiltinCallDescriptor(
    Builtin name, Zone* zone, StubCallMode stub_mode,
    bool needs_frame_state = false,
    Operator::Properties properties = Operator::kNoProperties);

ObjectAccess ObjectAccessForGCStores(wasm::ValueType type);

class WasmGraphAssembler : public GraphAssembler {
 public:
  WasmGraphAssembler(MachineGraph* mcgraph, Zone* zone)
      : GraphAssembler(mcgraph, zone), simplified_(zone) {}

  template <typename... Args>
  Node* CallRuntimeStub(wasm::WasmCode::RuntimeStubId stub_id,
                        Operator::Properties properties, Args*... args) {
    auto* call_descriptor = GetBuiltinCallDescriptor(
        WasmRuntimeStubIdToBuiltinName(stub_id), temp_zone(),
        StubCallMode::kCallWasmRuntimeStub, false, properties);
    // A direct call to a wasm runtime stub defined in this module.
    // Just encode the stub index. This will be patched at relocation.
    Node* call_target = mcgraph()->RelocatableIntPtrConstant(
        stub_id, RelocInfo::WASM_STUB_CALL);
    return Call(call_descriptor, call_target, args...);
  }

  Node* GetBuiltinPointerTarget(Builtin builtin) {
    static_assert(std::is_same<Smi, BuiltinPtr>(), "BuiltinPtr must be Smi");
    return NumberConstant(static_cast<int>(builtin));
  }

  template <typename... Args>
  Node* CallBuiltin(Builtin name, Operator::Properties properties,
                    Args*... args) {
    auto* call_descriptor = GetBuiltinCallDescriptor(
        name, temp_zone(), StubCallMode::kCallBuiltinPointer, false,
        properties);
    Node* call_target = GetBuiltinPointerTarget(name);
    return Call(call_descriptor, call_target, args...);
  }

  // Sets {true_node} and {false_node} to their corresponding Branch outputs.
  // Returns the Branch node. Does not change control().
  Node* Branch(Node* cond, Node** true_node, Node** false_node,
               BranchHint hint);

  Node* NumberConstant(double value) {
    return graph()->NewNode(mcgraph()->common()->NumberConstant(value));
  }

  Node* SmiConstant(Tagged_t value) {
    Address tagged_value = Internals::IntToSmi(static_cast<int>(value));
    return kTaggedSize == kInt32Size
               ? Int32Constant(static_cast<int32_t>(tagged_value))
               : Int64Constant(static_cast<int64_t>(tagged_value));
  }

  void MergeControlToEnd(Node* control) {
    NodeProperties::MergeControlToEnd(graph(), common(), control);
  }

  // Numeric conversions
  Node* BuildTruncateIntPtrToInt32(Node* value);

  Node* BuildChangeInt32ToIntPtr(Node* value);

  Node* BuildChangeIntPtrToInt64(Node* value);

  Node* BuildChangeUint32ToUintPtr(Node* node);

  Node* BuildSmiShiftBitsConstant();

  Node* BuildSmiShiftBitsConstant32();

  Node* BuildChangeInt32ToSmi(Node* value);

  Node* BuildChangeUint31ToSmi(Node* value);

  Node* BuildChangeSmiToInt32(Node* value);

  Node* BuildConvertUint32ToSmiWithSaturation(Node* value, uint32_t maxval);

  Node* BuildChangeSmiToIntPtr(Node* value);

  // Helper functions for dealing with HeapObjects.
  // Rule of thumb: if access to a given field in an object is required in
  // at least two places, put a helper function here.

  Node* Allocate(int size);

  Node* Allocate(Node* size,
                 AllowLargeObjects allow_large = AllowLargeObjects::kTrue);

  Node* LoadFromObject(MachineType type, Node* base, Node* offset);

  Node* LoadFromObject(MachineType type, Node* base, int offset) {
    return LoadFromObject(type, base, IntPtrConstant(offset));
  }

  Node* LoadImmutableFromObject(MachineType type, Node* base, Node* offset);

  Node* LoadImmutableFromObject(MachineType type, Node* base, int offset) {
    return LoadImmutableFromObject(type, base, IntPtrConstant(offset));
  }

  Node* LoadImmutable(LoadRepresentation rep, Node* base, Node* offset);

  Node* LoadImmutable(LoadRepresentation rep, Node* base, int offset) {
    return LoadImmutable(rep, base, IntPtrConstant(offset));
  }

  Node* StoreToObject(ObjectAccess access, Node* base, Node* offset,
                      Node* value);

  Node* StoreToObject(ObjectAccess access, Node* base, int offset,
                      Node* value) {
    return StoreToObject(access, base, IntPtrConstant(offset), value);
  }

  Node* InitializeImmutableInObject(ObjectAccess access, Node* base,
                                    Node* offset, Node* value);

  Node* InitializeImmutableInObject(ObjectAccess access, Node* base, int offset,
                                    Node* value) {
    return InitializeImmutableInObject(access, base, IntPtrConstant(offset),
                                       value);
  }

  Node* IsI31(Node* object);

  // Maps and their contents.

  Node* LoadMap(Node* object);

  void StoreMap(Node* heap_object, Node* map);

  Node* LoadInstanceType(Node* map);

  Node* LoadWasmTypeInfo(Node* map);

  // FixedArrays.

  Node* LoadFixedArrayLengthAsSmi(Node* fixed_array);

  Node* LoadFixedArrayElement(Node* fixed_array, Node* index_intptr,
                              MachineType type = MachineType::AnyTagged());

  Node* LoadImmutableFixedArrayElement(
      Node* fixed_array, Node* index_intptr,
      MachineType type = MachineType::AnyTagged());

  Node* LoadFixedArrayElement(Node* array, int index, MachineType type);

  Node* LoadFixedArrayElementSmi(Node* array, int index) {
    return LoadFixedArrayElement(array, index, MachineType::TaggedSigned());
  }

  Node* LoadFixedArrayElementPtr(Node* array, int index) {
    return LoadFixedArrayElement(array, index, MachineType::TaggedPointer());
  }

  Node* LoadFixedArrayElementAny(Node* array, int index) {
    return LoadFixedArrayElement(array, index, MachineType::AnyTagged());
  }

  Node* StoreFixedArrayElement(Node* array, int index, Node* value,
                               ObjectAccess access);

  Node* StoreFixedArrayElementSmi(Node* array, int index, Node* value) {
    return StoreFixedArrayElement(
        array, index, value,
        ObjectAccess(MachineType::TaggedSigned(), kNoWriteBarrier));
  }

  Node* StoreFixedArrayElementAny(Node* array, int index, Node* value) {
    return StoreFixedArrayElement(
        array, index, value,
        ObjectAccess(MachineType::AnyTagged(), kFullWriteBarrier));
  }

  // Functions, SharedFunctionInfos, FunctionData.

  Node* LoadSharedFunctionInfo(Node* js_function);

  Node* LoadContextFromJSFunction(Node* js_function);

  Node* LoadFunctionDataFromJSFunction(Node* js_function);

  Node* LoadExportedFunctionIndexAsSmi(Node* exported_function_data);

  Node* LoadExportedFunctionInstance(Node* exported_function_data);

  // JavaScript objects.

  Node* LoadJSArrayElements(Node* js_array);

  // WasmGC objects.

  Node* FieldOffset(const wasm::StructType* type, uint32_t field_index);

  Node* StoreStructField(Node* struct_object, const wasm::StructType* type,
                         uint32_t field_index, Node* value);
  Node* WasmArrayElementOffset(Node* index, wasm::ValueType element_type);

  Node* LoadWasmArrayLength(Node* array);

  Node* IsDataRefMap(Node* map);

  Node* WasmTypeCheck(Node* object, Node* rtt, WasmTypeCheckConfig config);

  Node* WasmTypeCast(Node* object, Node* rtt, WasmTypeCheckConfig config);

  Node* Null();

  Node* IsNull(Node* object);

  Node* IsNotNull(Node* object);

  Node* AssertNotNull(Node* object);

  Node* WasmExternInternalize(Node* object);

  Node* WasmExternExternalize(Node* object);

  // Generic helpers.

  Node* HasInstanceType(Node* heap_object, InstanceType type);

  void TrapIf(Node* condition, TrapId reason) {
    AddNode(graph()->NewNode(mcgraph()->common()->TrapIf(reason), condition,
                             effect(), control()));
  }

  void TrapUnless(Node* condition, TrapId reason) {
    AddNode(graph()->NewNode(mcgraph()->common()->TrapUnless(reason), condition,
                             effect(), control()));
  }

  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  SimplifiedOperatorBuilder simplified_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_GRAPH_ASSEMBLER_H_
