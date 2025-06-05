// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-subtyping.h"

#include "src/wasm/canonical-types.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

V8_INLINE bool EquivalentIndices(uint32_t index1, uint32_t index2,
                                 const WasmModule* module1,
                                 const WasmModule* module2) {
  DCHECK(index1 != index2 || module1 != module2);
  if (!v8_flags.wasm_type_canonicalization) return false;
  return module1->isorecursive_canonical_type_ids[index1] ==
         module2->isorecursive_canonical_type_ids[index2];
}

bool ValidStructSubtypeDefinition(uint32_t subtype_index,
                                  uint32_t supertype_index,
                                  const WasmModule* sub_module,
                                  const WasmModule* super_module) {
  const StructType* sub_struct = sub_module->types[subtype_index].struct_type;
  const StructType* super_struct =
      super_module->types[supertype_index].struct_type;

  if (sub_struct->field_count() < super_struct->field_count()) {
    return false;
  }

  for (uint32_t i = 0; i < super_struct->field_count(); i++) {
    bool sub_mut = sub_struct->mutability(i);
    bool super_mut = super_struct->mutability(i);
    if (sub_mut != super_mut ||
        (sub_mut &&
         !EquivalentTypes(sub_struct->field(i), super_struct->field(i),
                          sub_module, super_module)) ||
        (!sub_mut && !IsSubtypeOf(sub_struct->field(i), super_struct->field(i),
                                  sub_module, super_module))) {
      return false;
    }
  }
  return true;
}

bool ValidArraySubtypeDefinition(uint32_t subtype_index,
                                 uint32_t supertype_index,
                                 const WasmModule* sub_module,
                                 const WasmModule* super_module) {
  const ArrayType* sub_array = sub_module->types[subtype_index].array_type;
  const ArrayType* super_array =
      super_module->types[supertype_index].array_type;
  bool sub_mut = sub_array->mutability();
  bool super_mut = super_array->mutability();

  return (sub_mut && super_mut &&
          EquivalentTypes(sub_array->element_type(),
                          super_array->element_type(), sub_module,
                          super_module)) ||
         (!sub_mut && !super_mut &&
          IsSubtypeOf(sub_array->element_type(), super_array->element_type(),
                      sub_module, super_module));
}

bool ValidFunctionSubtypeDefinition(uint32_t subtype_index,
                                    uint32_t supertype_index,
                                    const WasmModule* sub_module,
                                    const WasmModule* super_module) {
  const FunctionSig* sub_func = sub_module->types[subtype_index].function_sig;
  const FunctionSig* super_func =
      super_module->types[supertype_index].function_sig;

  if (sub_func->parameter_count() != super_func->parameter_count() ||
      sub_func->return_count() != super_func->return_count()) {
    return false;
  }

  for (uint32_t i = 0; i < sub_func->parameter_count(); i++) {
    // Contravariance for params.
    if (!IsSubtypeOf(super_func->parameters()[i], sub_func->parameters()[i],
                     super_module, sub_module)) {
      return false;
    }
  }
  for (uint32_t i = 0; i < sub_func->return_count(); i++) {
    // Covariance for returns.
    if (!IsSubtypeOf(sub_func->returns()[i], super_func->returns()[i],
                     sub_module, super_module)) {
      return false;
    }
  }

  return true;
}

HeapType::Representation NullSentinelImpl(TypeInModule type) {
  switch (type.type.heap_type().representation()) {
    case HeapType::kI31:
    case HeapType::kNone:
    case HeapType::kEq:
    case HeapType::kData:
    case HeapType::kArray:
    case HeapType::kAny:
    case HeapType::kString:
    case HeapType::kStringViewWtf8:
    case HeapType::kStringViewWtf16:
    case HeapType::kStringViewIter:
      return HeapType::kNone;
    case HeapType::kExtern:
    case HeapType::kNoExtern:
      return HeapType::kNoExtern;
    case HeapType::kFunc:
    case HeapType::kNoFunc:
      return HeapType::kNoFunc;
    default:
      return type.module->has_signature(type.type.ref_index())
                 ? HeapType::kNoFunc
                 : HeapType::kNone;
  }
}

bool IsNullSentinel(HeapType type) {
  switch (type.representation()) {
    case HeapType::kNone:
    case HeapType::kNoExtern:
    case HeapType::kNoFunc:
      return true;
    default:
      return false;
  }
}

}  // namespace

bool ValidSubtypeDefinition(uint32_t subtype_index, uint32_t supertype_index,
                            const WasmModule* sub_module,
                            const WasmModule* super_module) {
  TypeDefinition::Kind sub_kind = sub_module->types[subtype_index].kind;
  TypeDefinition::Kind super_kind = super_module->types[supertype_index].kind;
  if (sub_kind != super_kind) return false;
  switch (sub_kind) {
    case TypeDefinition::kFunction:
      return ValidFunctionSubtypeDefinition(subtype_index, supertype_index,
                                            sub_module, super_module);
    case TypeDefinition::kStruct:
      return ValidStructSubtypeDefinition(subtype_index, supertype_index,
                                          sub_module, super_module);
    case TypeDefinition::kArray:
      return ValidArraySubtypeDefinition(subtype_index, supertype_index,
                                         sub_module, super_module);
  }
}

V8_NOINLINE V8_EXPORT_PRIVATE bool IsSubtypeOfImpl(
    ValueType subtype, ValueType supertype, const WasmModule* sub_module,
    const WasmModule* super_module) {
  DCHECK(subtype != supertype || sub_module != super_module);

  switch (subtype.kind()) {
    case kI32:
    case kI64:
    case kF32:
    case kF64:
    case kS128:
    case kI8:
    case kI16:
    case kVoid:
    case kBottom:
      return subtype == supertype;
    case kRtt:
      return supertype.kind() == kRtt &&
             EquivalentIndices(subtype.ref_index(), supertype.ref_index(),
                               sub_module, super_module);
    case kRef:
    case kRefNull:
      break;
  }

  DCHECK(subtype.is_object_reference());

  bool compatible_references = subtype.is_nullable()
                                   ? supertype.is_nullable()
                                   : supertype.is_object_reference();
  if (!compatible_references) return false;

  DCHECK(supertype.is_object_reference());

  // Now check that sub_heap and super_heap are subtype-related.

  HeapType sub_heap = subtype.heap_type();
  HeapType super_heap = supertype.heap_type();

  return IsHeapSubtypeOfImpl(sub_heap, super_heap, sub_module, super_module);
}

V8_NOINLINE V8_EXPORT_PRIVATE bool IsHeapSubtypeOfImpl(
    HeapType sub_heap, HeapType super_heap, const WasmModule* sub_module,
    const WasmModule* super_module) {
  switch (sub_heap.representation()) {
    case HeapType::kFunc:
      return sub_heap == super_heap;
    case HeapType::kEq:
      return sub_heap == super_heap || super_heap == HeapType::kAny;
    case HeapType::kAny:
      return super_heap == HeapType::kAny;
    case HeapType::kExtern:
      return super_heap == HeapType::kExtern;
    case HeapType::kI31:
    case HeapType::kData:
      return super_heap == sub_heap || super_heap == HeapType::kEq ||
             super_heap == HeapType::kAny;
    case HeapType::kArray:
      return super_heap == HeapType::kArray || super_heap == HeapType::kData ||
             super_heap == HeapType::kEq || super_heap == HeapType::kAny;
    case HeapType::kString:
      // stringref is a subtype of anyref under wasm-gc.
      return sub_heap == super_heap ||
             (v8_flags.experimental_wasm_gc && super_heap == HeapType::kAny);
    case HeapType::kStringViewWtf8:
    case HeapType::kStringViewWtf16:
    case HeapType::kStringViewIter:
      return sub_heap == super_heap;
    case HeapType::kBottom:
      UNREACHABLE();
    case HeapType::kNone:
      // none is a subtype of every non-func, non-extern reference type under
      // wasm-gc.
      if (super_heap.is_index()) {
        return !super_module->has_signature(super_heap.ref_index());
      }
      return super_heap != HeapType::kFunc && super_heap != HeapType::kNoFunc &&
             super_heap != HeapType::kExtern &&
             super_heap != HeapType::kNoExtern;
    case HeapType::kNoExtern:
      return super_heap == HeapType::kNoExtern ||
             super_heap == HeapType::kExtern;
    case HeapType::kNoFunc:
      // nofunc is a subtype of every funcref type under wasm-gc.
      if (super_heap.is_index()) {
        return super_module->has_signature(super_heap.ref_index());
      }
      return super_heap == HeapType::kNoFunc || super_heap == HeapType::kFunc;
    default:
      break;
  }

  DCHECK(sub_heap.is_index());
  uint32_t sub_index = sub_heap.ref_index();
  DCHECK(sub_module->has_type(sub_index));

  switch (super_heap.representation()) {
    case HeapType::kFunc:
      return sub_module->has_signature(sub_index);
    case HeapType::kEq:
    case HeapType::kData:
    case HeapType::kAny:
      return !sub_module->has_signature(sub_index);
    case HeapType::kArray:
      return sub_module->has_array(sub_index);
    case HeapType::kI31:
      return false;
    case HeapType::kExtern:
      return false;
    case HeapType::kString:
    case HeapType::kStringViewWtf8:
    case HeapType::kStringViewWtf16:
    case HeapType::kStringViewIter:
      return false;
    case HeapType::kBottom:
      UNREACHABLE();
    case HeapType::kNone:
    case HeapType::kNoExtern:
    case HeapType::kNoFunc:
      // Abstract null types are not supertypes for any index type.
      return false;
    default:
      break;
  }

  DCHECK(super_heap.is_index());
  uint32_t super_index = super_heap.ref_index();
  DCHECK(super_module->has_type(super_index));
  // The {IsSubtypeOf} entry point already has a fast path checking ValueType
  // equality; here we catch (ref $x) being a subtype of (ref null $x).
  if (sub_module == super_module && sub_index == super_index) return true;

  if (v8_flags.wasm_type_canonicalization) {
    return GetTypeCanonicalizer()->IsCanonicalSubtype(sub_index, super_index,
                                                      sub_module, super_module);
  } else {
    uint32_t explicit_super = sub_module->supertype(sub_index);
    while (true) {
      if (explicit_super == super_index) return true;
      // Reached the end of the explicitly defined inheritance chain.
      if (explicit_super == kNoSuperType) return false;
      explicit_super = sub_module->supertype(explicit_super);
    }
  }
}

V8_NOINLINE bool EquivalentTypes(ValueType type1, ValueType type2,
                                 const WasmModule* module1,
                                 const WasmModule* module2) {
  if (type1 == type2 && module1 == module2) return true;
  if (!type1.has_index() || !type2.has_index()) return type1 == type2;
  if (type1.kind() != type2.kind()) return false;

  DCHECK(type1 != type2 || module1 != module2);
  DCHECK(type1.has_index() && module1->has_type(type1.ref_index()) &&
         type2.has_index() && module2->has_type(type2.ref_index()));

  return EquivalentIndices(type1.ref_index(), type2.ref_index(), module1,
                           module2);
}

namespace {
// Returns the least common ancestor of two type indices, as a type index in
// {module1}.
HeapType::Representation CommonAncestor(uint32_t type_index1,
                                        uint32_t type_index2,
                                        const WasmModule* module1,
                                        const WasmModule* module2) {
  TypeDefinition::Kind kind1 = module1->types[type_index1].kind;
  TypeDefinition::Kind kind2 = module2->types[type_index2].kind;
  {
    int depth1 = GetSubtypingDepth(module1, type_index1);
    int depth2 = GetSubtypingDepth(module2, type_index2);
    while (depth1 > depth2) {
      type_index1 = module1->supertype(type_index1);
      depth1--;
    }
    while (depth2 > depth1) {
      type_index2 = module2->supertype(type_index2);
      depth2--;
    }
  }
  DCHECK_NE(type_index1, kNoSuperType);
  DCHECK_NE(type_index2, kNoSuperType);
  while (type_index1 != kNoSuperType &&
         !(type_index1 == type_index2 && module1 == module2) &&
         !EquivalentIndices(type_index1, type_index2, module1, module2)) {
    type_index1 = module1->supertype(type_index1);
    type_index2 = module2->supertype(type_index2);
  }
  DCHECK_EQ(type_index1 == kNoSuperType, type_index2 == kNoSuperType);
  if (type_index1 != kNoSuperType) {
    return static_cast<HeapType::Representation>(type_index1);
  }
  switch (kind1) {
    case TypeDefinition::kFunction:
      DCHECK_EQ(kind2, kind1);
      return HeapType::kFunc;
    case TypeDefinition::kStruct:
      DCHECK_NE(kind2, TypeDefinition::kFunction);
      return HeapType::kData;
    case TypeDefinition::kArray:
      switch (kind2) {
        case TypeDefinition::kFunction:
          UNREACHABLE();
        case TypeDefinition::kStruct:
          return HeapType::kData;
        case TypeDefinition::kArray:
          return HeapType::kArray;
      }
  }
}

// Returns the least common ancestor of a generic HeapType {heap1}, and
// another HeapType {heap2}.
HeapType::Representation CommonAncestorWithGeneric(HeapType heap1,
                                                   HeapType heap2,
                                                   const WasmModule* module2) {
  DCHECK(heap1.is_generic());
  switch (heap1.representation()) {
    case HeapType::kFunc:
      DCHECK(IsHeapSubtypeOf(heap2, heap1, module2, module2));
      return HeapType::kFunc;
    case HeapType::kEq: {
      return IsHeapSubtypeOf(heap2, heap1, module2, module2)
                 ? heap1.representation()
                 : HeapType::kAny;
    }
    case HeapType::kI31:
      switch (heap2.representation()) {
        case HeapType::kI31:
        case HeapType::kNone:
          return HeapType::kI31;
        case HeapType::kEq:
        case HeapType::kData:
        case HeapType::kArray:
          return HeapType::kEq;
        case HeapType::kAny:
          return HeapType::kAny;
        case HeapType::kFunc:
        case HeapType::kExtern:
        case HeapType::kNoExtern:
        case HeapType::kNoFunc:
          UNREACHABLE();
        default:
          return module2->has_signature(heap2.ref_index()) ? HeapType::kBottom
                                                           : HeapType::kEq;
      }
    case HeapType::kData:
      switch (heap2.representation()) {
        case HeapType::kData:
        case HeapType::kArray:
        case HeapType::kNone:
          return HeapType::kData;
        case HeapType::kI31:
        case HeapType::kEq:
          return HeapType::kEq;
        case HeapType::kAny:
          return HeapType::kAny;
        case HeapType::kFunc:
        case HeapType::kExtern:
        case HeapType::kNoExtern:
        case HeapType::kNoFunc:
          UNREACHABLE();
        default:
          return module2->has_signature(heap2.ref_index()) ? HeapType::kBottom
                                                           : HeapType::kData;
      }
    case HeapType::kArray:
      switch (heap2.representation()) {
        case HeapType::kArray:
        case HeapType::kNone:
          return HeapType::kArray;
        case HeapType::kData:
          return HeapType::kData;
        case HeapType::kI31:
        case HeapType::kEq:
          return HeapType::kEq;
        case HeapType::kAny:
          return HeapType::kAny;
        case HeapType::kFunc:
        case HeapType::kExtern:
        case HeapType::kNoExtern:
        case HeapType::kNoFunc:
          UNREACHABLE();
        default:
          return module2->has_array(heap2.ref_index())    ? HeapType::kArray
                 : module2->has_struct(heap2.ref_index()) ? HeapType::kData
                                                          : HeapType::kBottom;
      }
    case HeapType::kAny:
      return HeapType::kAny;
    case HeapType::kBottom:
      return HeapType::kBottom;
    case HeapType::kNone:
      return heap2.representation();
    case HeapType::kNoFunc:
      switch (heap2.representation()) {
        case HeapType::kArray:
        case HeapType::kNone:
        case HeapType::kData:
        case HeapType::kI31:
        case HeapType::kEq:
        case HeapType::kAny:
        case HeapType::kExtern:
        case HeapType::kNoExtern:
          UNREACHABLE();
        case HeapType::kNoFunc:
          return HeapType::kNoFunc;
        case HeapType::kFunc:
          return HeapType::kFunc;
        default:
          return module2->has_signature(heap2.ref_index())
                     ? heap2.representation()
                     : HeapType::kBottom;
      }
    case HeapType::kNoExtern:
      return heap2.representation() == HeapType::kExtern ? HeapType::kExtern
                                                         : HeapType::kNoExtern;
    case HeapType::kExtern:
      return HeapType::kExtern;
    case HeapType::kString:
    case HeapType::kStringViewIter:
      return heap1 == heap2 ? heap1.representation() : HeapType::kBottom;
    default:
      UNREACHABLE();
  }
}
}  // namespace

V8_EXPORT_PRIVATE TypeInModule Union(ValueType type1, ValueType type2,
                                     const WasmModule* module1,
                                     const WasmModule* module2) {
  if (!type1.is_object_reference() || !type2.is_object_reference()) {
    return {
        EquivalentTypes(type1, type2, module1, module2) ? type1 : kWasmBottom,
        module1};
  }
  Nullability nullability =
      type1.is_nullable() || type2.is_nullable() ? kNullable : kNonNullable;
  HeapType heap1 = type1.heap_type();
  HeapType heap2 = type2.heap_type();
  if (heap1 == heap2 && module1 == module2) {
    return {ValueType::RefMaybeNull(heap1, nullability), module1};
  }
  if (heap1.is_generic()) {
    return {ValueType::RefMaybeNull(
                CommonAncestorWithGeneric(heap1, heap2, module2), nullability),
            module1};
  } else if (heap2.is_generic()) {
    return {ValueType::RefMaybeNull(
                CommonAncestorWithGeneric(heap2, heap1, module1), nullability),
            module1};
  } else {
    return {ValueType::RefMaybeNull(
                CommonAncestor(heap1.ref_index(), heap2.ref_index(), module1,
                               module2),
                nullability),
            module1};
  }
}

TypeInModule Intersection(ValueType type1, ValueType type2,
                          const WasmModule* module1,
                          const WasmModule* module2) {
  if (!type1.is_object_reference() || !type2.is_object_reference()) {
    return {
        EquivalentTypes(type1, type2, module1, module2) ? type1 : kWasmBottom,
        module1};
  }
  Nullability nullability =
      type1.is_nullable() && type2.is_nullable() ? kNullable : kNonNullable;
  // non-nullable null type is not a valid type.
  if (nullability == kNonNullable && (IsNullSentinel(type1.heap_type()) ||
                                      IsNullSentinel(type2.heap_type()))) {
    return {kWasmBottom, module1};
  }
  if (IsHeapSubtypeOf(type1.heap_type(), type2.heap_type(), module1, module2)) {
    return TypeInModule{ValueType::RefMaybeNull(type1.heap_type(), nullability),
                        module1};
  }
  if (IsHeapSubtypeOf(type2.heap_type(), type1.heap_type(), module2, module1)) {
    return TypeInModule{ValueType::RefMaybeNull(type2.heap_type(), nullability),
                        module2};
  }
  if (nullability == kNonNullable) {
    return {kWasmBottom, module1};
  }
  // Check for common null representation.
  ValueType null_type1 = ToNullSentinel({type1, module1});
  if (null_type1 == ToNullSentinel({type2, module2})) {
    return {null_type1, module1};
  }
  return {kWasmBottom, module1};
}

ValueType ToNullSentinel(TypeInModule type) {
  HeapType::Representation null_heap = NullSentinelImpl(type);
  DCHECK(
      IsHeapSubtypeOf(HeapType(null_heap), type.type.heap_type(), type.module));
  return ValueType::RefNull(null_heap);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
