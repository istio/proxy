// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/compilation-dependencies.h"

#include "src/base/optional.h"
#include "src/execution/protectors.h"
#include "src/handles/handles-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/internal-index.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

#define DEPENDENCY_LIST(V)              \
  V(ConsistentJSFunctionView)           \
  V(ConstantInDictionaryPrototypeChain) \
  V(ElementsKind)                       \
  V(FieldConstness)                     \
  V(FieldRepresentation)                \
  V(FieldType)                          \
  V(GlobalProperty)                     \
  V(InitialMap)                         \
  V(InitialMapInstanceSizePrediction)   \
  V(OwnConstantDataProperty)            \
  V(OwnConstantDictionaryProperty)      \
  V(OwnConstantElement)                 \
  V(PretenureMode)                      \
  V(Protector)                          \
  V(PrototypeProperty)                  \
  V(StableMap)                          \
  V(Transition)

CompilationDependencies::CompilationDependencies(JSHeapBroker* broker,
                                                 Zone* zone)
    : zone_(zone), broker_(broker), dependencies_(zone) {
  broker->set_dependencies(this);
}

namespace {

enum CompilationDependencyKind {
#define V(Name) k##Name,
  DEPENDENCY_LIST(V)
#undef V
};

#define V(Name) class Name##Dependency;
DEPENDENCY_LIST(V)
#undef V

const char* CompilationDependencyKindToString(CompilationDependencyKind kind) {
#define V(Name) #Name "Dependency",
  static const char* const names[] = {DEPENDENCY_LIST(V)};
#undef V
  return names[kind];
}

class PendingDependencies;

}  // namespace

class CompilationDependency : public ZoneObject {
 public:
  explicit CompilationDependency(CompilationDependencyKind kind) : kind(kind) {}

  virtual bool IsValid() const = 0;
  virtual void PrepareInstall() const {}
  virtual void Install(PendingDependencies* deps) const = 0;

#define V(Name)                                     \
  bool Is##Name() const { return kind == k##Name; } \
  V8_ALLOW_UNUSED const Name##Dependency* As##Name() const;
  DEPENDENCY_LIST(V)
#undef V

  const char* ToString() const {
    return CompilationDependencyKindToString(kind);
  }

  const CompilationDependencyKind kind;

 private:
  virtual size_t Hash() const = 0;
  virtual bool Equals(const CompilationDependency* that) const = 0;
  friend struct CompilationDependencies::CompilationDependencyHash;
  friend struct CompilationDependencies::CompilationDependencyEqual;
};

size_t CompilationDependencies::CompilationDependencyHash::operator()(
    const CompilationDependency* dep) const {
  return base::hash_combine(dep->kind, dep->Hash());
}

bool CompilationDependencies::CompilationDependencyEqual::operator()(
    const CompilationDependency* lhs, const CompilationDependency* rhs) const {
  return lhs->kind == rhs->kind && lhs->Equals(rhs);
}

namespace {

// Dependencies can only be fully deduplicated immediately prior to
// installation (because PrepareInstall may create the object on which the dep
// will be installed). We gather and dedupe deps in this class, and install
// them from here.
class PendingDependencies final {
 public:
  explicit PendingDependencies(Zone* zone) : deps_(zone) {}

  void Register(Handle<HeapObject> object,
                DependentCode::DependencyGroup group) {
    // Code, which are per-local Isolate, cannot depend on objects in the shared
    // heap. Shared heap dependencies are designed to never invalidate
    // assumptions. E.g., maps for shared structs do not have transitions or
    // change the shape of their fields. See
    // DependentCode::DeoptimizeDependencyGroups for corresponding DCHECK.
    if (object->InSharedWritableHeap()) return;
    deps_[object] |= group;
  }

  void InstallAll(Isolate* isolate, Handle<Code> code) {
    if (V8_UNLIKELY(FLAG_predictable)) {
      InstallAllPredictable(isolate, code);
      return;
    }

    // With deduplication done we no longer rely on the object address for
    // hashing.
    AllowGarbageCollection yes_gc;
    for (const auto& o_and_g : deps_) {
      DependentCode::InstallDependency(isolate, code, o_and_g.first,
                                       o_and_g.second);
    }
  }

  void InstallAllPredictable(Isolate* isolate, Handle<Code> code) {
    CHECK(FLAG_predictable);
    // First, guarantee predictable iteration order.
    using HandleAndGroup =
        std::pair<Handle<HeapObject>, DependentCode::DependencyGroups>;
    std::vector<HandleAndGroup> entries(deps_.begin(), deps_.end());

    std::sort(entries.begin(), entries.end(),
              [](const HandleAndGroup& lhs, const HandleAndGroup& rhs) {
                return lhs.first->ptr() < rhs.first->ptr();
              });

    // With deduplication done we no longer rely on the object address for
    // hashing.
    AllowGarbageCollection yes_gc;
    for (const auto& o_and_g : entries) {
      DependentCode::InstallDependency(isolate, code, o_and_g.first,
                                       o_and_g.second);
    }
  }

 private:
  struct HandleHash {
    size_t operator()(const Handle<HeapObject>& x) const {
      return static_cast<size_t>(x->ptr());
    }
  };
  struct HandleEqual {
    bool operator()(const Handle<HeapObject>& lhs,
                    const Handle<HeapObject>& rhs) const {
      return lhs.is_identical_to(rhs);
    }
  };
  ZoneUnorderedMap<Handle<HeapObject>, DependentCode::DependencyGroups,
                   HandleHash, HandleEqual>
      deps_;
  const DisallowGarbageCollection no_gc_;
};

class InitialMapDependency final : public CompilationDependency {
 public:
  InitialMapDependency(JSHeapBroker* broker, const JSFunctionRef& function,
                       const MapRef& initial_map)
      : CompilationDependency(kInitialMap),
        function_(function),
        initial_map_(initial_map) {}

  bool IsValid() const override {
    Handle<JSFunction> function = function_.object();
    return function->has_initial_map() &&
           function->initial_map() == *initial_map_.object();
  }

  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    deps->Register(initial_map_.object(),
                   DependentCode::kInitialMapChangedGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(function_), h(initial_map_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const InitialMapDependency* const zat = that->AsInitialMap();
    return function_.equals(zat->function_) &&
           initial_map_.equals(zat->initial_map_);
  }

  const JSFunctionRef function_;
  const MapRef initial_map_;
};

class PrototypePropertyDependency final : public CompilationDependency {
 public:
  PrototypePropertyDependency(JSHeapBroker* broker,
                              const JSFunctionRef& function,
                              const ObjectRef& prototype)
      : CompilationDependency(kPrototypeProperty),
        function_(function),
        prototype_(prototype) {
    DCHECK(function_.has_instance_prototype(broker->dependencies()));
    DCHECK(!function_.PrototypeRequiresRuntimeLookup(broker->dependencies()));
    DCHECK(function_.instance_prototype(broker->dependencies())
               .equals(prototype_));
  }

  bool IsValid() const override {
    Handle<JSFunction> function = function_.object();
    return function->has_prototype_slot() &&
           function->has_instance_prototype() &&
           !function->PrototypeRequiresRuntimeLookup() &&
           function->instance_prototype() == *prototype_.object();
  }

  void PrepareInstall() const override {
    SLOW_DCHECK(IsValid());
    Handle<JSFunction> function = function_.object();
    if (!function->has_initial_map()) JSFunction::EnsureHasInitialMap(function);
  }

  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    Handle<JSFunction> function = function_.object();
    CHECK(function->has_initial_map());
    Handle<Map> initial_map(function->initial_map(), function_.isolate());
    deps->Register(initial_map, DependentCode::kInitialMapChangedGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(function_), h(prototype_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const PrototypePropertyDependency* const zat = that->AsPrototypeProperty();
    return function_.equals(zat->function_) &&
           prototype_.equals(zat->prototype_);
  }

  const JSFunctionRef function_;
  const ObjectRef prototype_;
};

class StableMapDependency final : public CompilationDependency {
 public:
  explicit StableMapDependency(const MapRef& map)
      : CompilationDependency(kStableMap), map_(map) {}

  bool IsValid() const override {
    // TODO(v8:11670): Consider turn this back into a CHECK inside the
    // constructor and DependOnStableMap, if possible in light of concurrent
    // heap state modifications.
    return !map_.object()->is_dictionary_map() && map_.object()->is_stable();
  }
  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    deps->Register(map_.object(), DependentCode::kPrototypeCheckGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(map_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const StableMapDependency* const zat = that->AsStableMap();
    return map_.equals(zat->map_);
  }

  const MapRef map_;
};

class ConstantInDictionaryPrototypeChainDependency final
    : public CompilationDependency {
 public:
  explicit ConstantInDictionaryPrototypeChainDependency(
      const MapRef receiver_map, const NameRef property_name,
      const ObjectRef constant, PropertyKind kind)
      : CompilationDependency(kConstantInDictionaryPrototypeChain),
        receiver_map_(receiver_map),
        property_name_{property_name},
        constant_{constant},
        kind_{kind} {
    DCHECK(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);
  }

  // Checks that |constant_| is still the value of accessing |property_name_|
  // starting at |receiver_map_|.
  bool IsValid() const override { return !GetHolderIfValid().is_null(); }

  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    Isolate* isolate = receiver_map_.isolate();
    Handle<JSObject> holder = GetHolderIfValid().ToHandleChecked();
    Handle<Map> map = receiver_map_.object();

    while (map->prototype() != *holder) {
      map = handle(map->prototype().map(), isolate);
      DCHECK(map->IsJSObjectMap());  // Due to IsValid holding.
      deps->Register(map, DependentCode::kPrototypeCheckGroup);
    }

    DCHECK(map->prototype().map().IsJSObjectMap());  // Due to IsValid holding.
    deps->Register(handle(map->prototype().map(), isolate),
                   DependentCode::kPrototypeCheckGroup);
  }

 private:
  // If the dependency is still valid, returns holder of the constant. Otherwise
  // returns null.
  // TODO(neis) Currently, invoking IsValid and then Install duplicates the call
  // to GetHolderIfValid. Instead, consider letting IsValid change the state
  // (and store the holder), or merge IsValid and Install.
  MaybeHandle<JSObject> GetHolderIfValid() const {
    DisallowGarbageCollection no_gc;
    Isolate* isolate = receiver_map_.isolate();

    Handle<Object> holder;
    HeapObject prototype = receiver_map_.object()->prototype();

    enum class ValidationResult { kFoundCorrect, kFoundIncorrect, kNotFound };
    auto try_load = [&](auto dictionary) -> ValidationResult {
      InternalIndex entry =
          dictionary.FindEntry(isolate, property_name_.object());
      if (entry.is_not_found()) {
        return ValidationResult::kNotFound;
      }

      PropertyDetails details = dictionary.DetailsAt(entry);
      if (details.constness() != PropertyConstness::kConst) {
        return ValidationResult::kFoundIncorrect;
      }

      Object dictionary_value = dictionary.ValueAt(entry);
      Object value;
      // We must be able to detect the case that the property |property_name_|
      // of |holder_| was originally a plain function |constant_| (when creating
      // this dependency) and has since become an accessor whose getter is
      // |constant_|. Therefore, we cannot just look at the property kind of
      // |details|, because that reflects the current situation, not the one
      // when creating this dependency.
      if (details.kind() != kind_) {
        return ValidationResult::kFoundIncorrect;
      }
      if (kind_ == PropertyKind::kAccessor) {
        if (!dictionary_value.IsAccessorPair()) {
          return ValidationResult::kFoundIncorrect;
        }
        // Only supporting loading at the moment, so we only ever want the
        // getter.
        value = AccessorPair::cast(dictionary_value)
                    .get(AccessorComponent::ACCESSOR_GETTER);
      } else {
        value = dictionary_value;
      }
      return value == *constant_.object() ? ValidationResult::kFoundCorrect
                                          : ValidationResult::kFoundIncorrect;
    };

    while (prototype.IsJSObject()) {
      // We only care about JSObjects because that's the only type of holder
      // (and types of prototypes on the chain to the holder) that
      // AccessInfoFactory::ComputePropertyAccessInfo allows.
      JSObject object = JSObject::cast(prototype);

      // We only support dictionary mode prototypes on the chain for this kind
      // of dependency.
      CHECK(!object.HasFastProperties());

      ValidationResult result =
          V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL
              ? try_load(object.property_dictionary_swiss())
              : try_load(object.property_dictionary());

      if (result == ValidationResult::kFoundCorrect) {
        return handle(object, isolate);
      } else if (result == ValidationResult::kFoundIncorrect) {
        return MaybeHandle<JSObject>();
      }

      // In case of kNotFound, continue walking up the chain.
      prototype = object.map().prototype();
    }

    return MaybeHandle<JSObject>();
  }

  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(receiver_map_), h(property_name_), h(constant_),
                              static_cast<int>(kind_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const ConstantInDictionaryPrototypeChainDependency* const zat =
        that->AsConstantInDictionaryPrototypeChain();
    return receiver_map_.equals(zat->receiver_map_) &&
           property_name_.equals(zat->property_name_) &&
           constant_.equals(zat->constant_) && kind_ == zat->kind_;
  }

  const MapRef receiver_map_;
  const NameRef property_name_;
  const ObjectRef constant_;
  const PropertyKind kind_;
};

class OwnConstantDataPropertyDependency final : public CompilationDependency {
 public:
  OwnConstantDataPropertyDependency(JSHeapBroker* broker,
                                    const JSObjectRef& holder,
                                    const MapRef& map,
                                    Representation representation,
                                    FieldIndex index, const ObjectRef& value)
      : CompilationDependency(kOwnConstantDataProperty),
        broker_(broker),
        holder_(holder),
        map_(map),
        representation_(representation),
        index_(index),
        value_(value) {}

  bool IsValid() const override {
    if (holder_.object()->map() != *map_.object()) {
      TRACE_BROKER_MISSING(broker_,
                           "Map change detected in " << holder_.object());
      return false;
    }
    DisallowGarbageCollection no_heap_allocation;
    Object current_value = holder_.object()->RawFastPropertyAt(index_);
    Object used_value = *value_.object();
    if (representation_.IsDouble()) {
      // Compare doubles by bit pattern.
      if (!current_value.IsHeapNumber() || !used_value.IsHeapNumber() ||
          HeapNumber::cast(current_value).value_as_bits(kRelaxedLoad) !=
              HeapNumber::cast(used_value).value_as_bits(kRelaxedLoad)) {
        TRACE_BROKER_MISSING(broker_,
                             "Constant Double property value changed in "
                                 << holder_.object() << " at FieldIndex "
                                 << index_.property_index());
        return false;
      }
    } else if (current_value != used_value) {
      TRACE_BROKER_MISSING(broker_, "Constant property value changed in "
                                        << holder_.object() << " at FieldIndex "
                                        << index_.property_index());
      return false;
    }
    return true;
  }

  void Install(PendingDependencies* deps) const override {}

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(holder_), h(map_), representation_.kind(),
                              index_.bit_field(), h(value_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const OwnConstantDataPropertyDependency* const zat =
        that->AsOwnConstantDataProperty();
    return holder_.equals(zat->holder_) && map_.equals(zat->map_) &&
           representation_.Equals(zat->representation_) &&
           index_ == zat->index_ && value_.equals(zat->value_);
  }

  JSHeapBroker* const broker_;
  JSObjectRef const holder_;
  MapRef const map_;
  Representation const representation_;
  FieldIndex const index_;
  ObjectRef const value_;
};

class OwnConstantDictionaryPropertyDependency final
    : public CompilationDependency {
 public:
  OwnConstantDictionaryPropertyDependency(JSHeapBroker* broker,
                                          const JSObjectRef& holder,
                                          InternalIndex index,
                                          const ObjectRef& value)
      : CompilationDependency(kOwnConstantDictionaryProperty),
        broker_(broker),
        holder_(holder),
        map_(holder.map()),
        index_(index),
        value_(value) {
    // We depend on map() being cached.
    static_assert(ref_traits<JSObject>::ref_serialization_kind !=
                  RefSerializationKind::kNeverSerialized);
  }

  bool IsValid() const override {
    if (holder_.object()->map() != *map_.object()) {
      TRACE_BROKER_MISSING(broker_,
                           "Map change detected in " << holder_.object());
      return false;
    }

    base::Optional<Object> maybe_value = JSObject::DictionaryPropertyAt(
        holder_.object(), index_, broker_->isolate()->heap());

    if (!maybe_value) {
      TRACE_BROKER_MISSING(
          broker_, holder_.object()
                       << "has a value that might not safe to read at index "
                       << index_.as_int());
      return false;
    }

    if (*maybe_value != *value_.object()) {
      TRACE_BROKER_MISSING(broker_, "Constant property value changed in "
                                        << holder_.object()
                                        << " at InternalIndex "
                                        << index_.as_int());
      return false;
    }
    return true;
  }

  void Install(PendingDependencies* deps) const override {}

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(holder_), h(map_), index_.raw_value(),
                              h(value_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const OwnConstantDictionaryPropertyDependency* const zat =
        that->AsOwnConstantDictionaryProperty();
    return holder_.equals(zat->holder_) && map_.equals(zat->map_) &&
           index_ == zat->index_ && value_.equals(zat->value_);
  }

  JSHeapBroker* const broker_;
  JSObjectRef const holder_;
  MapRef const map_;
  InternalIndex const index_;
  ObjectRef const value_;
};

class ConsistentJSFunctionViewDependency final : public CompilationDependency {
 public:
  explicit ConsistentJSFunctionViewDependency(const JSFunctionRef& function)
      : CompilationDependency(kConsistentJSFunctionView), function_(function) {}

  bool IsValid() const override {
    return function_.IsConsistentWithHeapState();
  }

  void Install(PendingDependencies* deps) const override {}

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(function_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const ConsistentJSFunctionViewDependency* const zat =
        that->AsConsistentJSFunctionView();
    return function_.equals(zat->function_);
  }

  const JSFunctionRef function_;
};

class TransitionDependency final : public CompilationDependency {
 public:
  explicit TransitionDependency(const MapRef& map)
      : CompilationDependency(kTransition), map_(map) {
    DCHECK(map_.CanBeDeprecated());
  }

  bool IsValid() const override { return !map_.object()->is_deprecated(); }

  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    deps->Register(map_.object(), DependentCode::kTransitionGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(map_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const TransitionDependency* const zat = that->AsTransition();
    return map_.equals(zat->map_);
  }

  const MapRef map_;
};

class PretenureModeDependency final : public CompilationDependency {
 public:
  PretenureModeDependency(const AllocationSiteRef& site,
                          AllocationType allocation)
      : CompilationDependency(kPretenureMode),
        site_(site),
        allocation_(allocation) {}

  bool IsValid() const override {
    return allocation_ == site_.object()->GetAllocationType();
  }
  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    deps->Register(site_.object(),
                   DependentCode::kAllocationSiteTenuringChangedGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(site_), allocation_);
  }

  bool Equals(const CompilationDependency* that) const override {
    const PretenureModeDependency* const zat = that->AsPretenureMode();
    return site_.equals(zat->site_) && allocation_ == zat->allocation_;
  }

  const AllocationSiteRef site_;
  const AllocationType allocation_;
};

class FieldRepresentationDependency final : public CompilationDependency {
 public:
  FieldRepresentationDependency(const MapRef& map, InternalIndex descriptor,
                                Representation representation)
      : CompilationDependency(kFieldRepresentation),
        map_(map),
        descriptor_(descriptor),
        representation_(representation) {}

  bool IsValid() const override {
    DisallowGarbageCollection no_heap_allocation;
    if (map_.object()->is_deprecated()) return false;
    return representation_.Equals(map_.object()
                                      ->instance_descriptors(map_.isolate())
                                      .GetDetails(descriptor_)
                                      .representation());
  }

  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    Isolate* isolate = map_.isolate();
    Handle<Map> owner(map_.object()->FindFieldOwner(isolate, descriptor_),
                      isolate);
    CHECK(!owner->is_deprecated());
    CHECK(representation_.Equals(owner->instance_descriptors(isolate)
                                     .GetDetails(descriptor_)
                                     .representation()));
    deps->Register(owner, DependentCode::kFieldRepresentationGroup);
  }

  bool DependsOn(const Handle<Map>& receiver_map) const {
    return map_.object().equals(receiver_map);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(map_), descriptor_.as_int(),
                              representation_.kind());
  }

  bool Equals(const CompilationDependency* that) const override {
    const FieldRepresentationDependency* const zat =
        that->AsFieldRepresentation();
    return map_.equals(zat->map_) && descriptor_ == zat->descriptor_ &&
           representation_.Equals(zat->representation_);
  }

  const MapRef map_;
  const InternalIndex descriptor_;
  const Representation representation_;
};

class FieldTypeDependency final : public CompilationDependency {
 public:
  FieldTypeDependency(const MapRef& map, InternalIndex descriptor,
                      const ObjectRef& type)
      : CompilationDependency(kFieldType),
        map_(map),
        descriptor_(descriptor),
        type_(type) {}

  bool IsValid() const override {
    DisallowGarbageCollection no_heap_allocation;
    if (map_.object()->is_deprecated()) return false;
    return *type_.object() == map_.object()
                                  ->instance_descriptors(map_.isolate())
                                  .GetFieldType(descriptor_);
  }

  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    Isolate* isolate = map_.isolate();
    Handle<Map> owner(map_.object()->FindFieldOwner(isolate, descriptor_),
                      isolate);
    CHECK(!owner->is_deprecated());
    CHECK_EQ(*type_.object(),
             owner->instance_descriptors(isolate).GetFieldType(descriptor_));
    deps->Register(owner, DependentCode::kFieldTypeGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(map_), descriptor_.as_int(), h(type_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const FieldTypeDependency* const zat = that->AsFieldType();
    return map_.equals(zat->map_) && descriptor_ == zat->descriptor_ &&
           type_.equals(zat->type_);
  }

  const MapRef map_;
  const InternalIndex descriptor_;
  const ObjectRef type_;
};

class FieldConstnessDependency final : public CompilationDependency {
 public:
  FieldConstnessDependency(const MapRef& map, InternalIndex descriptor)
      : CompilationDependency(kFieldConstness),
        map_(map),
        descriptor_(descriptor) {}

  bool IsValid() const override {
    DisallowGarbageCollection no_heap_allocation;
    if (map_.object()->is_deprecated()) return false;
    return PropertyConstness::kConst ==
           map_.object()
               ->instance_descriptors(map_.isolate())
               .GetDetails(descriptor_)
               .constness();
  }

  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    Isolate* isolate = map_.isolate();
    Handle<Map> owner(map_.object()->FindFieldOwner(isolate, descriptor_),
                      isolate);
    CHECK(!owner->is_deprecated());
    CHECK_EQ(PropertyConstness::kConst, owner->instance_descriptors(isolate)
                                            .GetDetails(descriptor_)
                                            .constness());
    deps->Register(owner, DependentCode::kFieldConstGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(map_), descriptor_.as_int());
  }

  bool Equals(const CompilationDependency* that) const override {
    const FieldConstnessDependency* const zat = that->AsFieldConstness();
    return map_.equals(zat->map_) && descriptor_ == zat->descriptor_;
  }

  const MapRef map_;
  const InternalIndex descriptor_;
};

class GlobalPropertyDependency final : public CompilationDependency {
 public:
  GlobalPropertyDependency(const PropertyCellRef& cell, PropertyCellType type,
                           bool read_only)
      : CompilationDependency(kGlobalProperty),
        cell_(cell),
        type_(type),
        read_only_(read_only) {
    DCHECK_EQ(type_, cell_.property_details().cell_type());
    DCHECK_EQ(read_only_, cell_.property_details().IsReadOnly());
  }

  bool IsValid() const override {
    Handle<PropertyCell> cell = cell_.object();
    // The dependency is never valid if the cell is 'invalidated'. This is
    // marked by setting the value to the hole.
    if (cell->value() == *(cell_.isolate()->factory()->the_hole_value())) {
      return false;
    }
    return type_ == cell->property_details().cell_type() &&
           read_only_ == cell->property_details().IsReadOnly();
  }
  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    deps->Register(cell_.object(), DependentCode::kPropertyCellChangedGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(cell_), static_cast<int>(type_), read_only_);
  }

  bool Equals(const CompilationDependency* that) const override {
    const GlobalPropertyDependency* const zat = that->AsGlobalProperty();
    return cell_.equals(zat->cell_) && type_ == zat->type_ &&
           read_only_ == zat->read_only_;
  }

  const PropertyCellRef cell_;
  const PropertyCellType type_;
  const bool read_only_;
};

class ProtectorDependency final : public CompilationDependency {
 public:
  explicit ProtectorDependency(const PropertyCellRef& cell)
      : CompilationDependency(kProtector), cell_(cell) {}

  bool IsValid() const override {
    Handle<PropertyCell> cell = cell_.object();
    return cell->value() == Smi::FromInt(Protectors::kProtectorValid);
  }
  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    deps->Register(cell_.object(), DependentCode::kPropertyCellChangedGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(cell_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const ProtectorDependency* const zat = that->AsProtector();
    return cell_.equals(zat->cell_);
  }

  const PropertyCellRef cell_;
};

class ElementsKindDependency final : public CompilationDependency {
 public:
  ElementsKindDependency(const AllocationSiteRef& site, ElementsKind kind)
      : CompilationDependency(kElementsKind), site_(site), kind_(kind) {
    DCHECK(AllocationSite::ShouldTrack(kind_));
  }

  bool IsValid() const override {
    Handle<AllocationSite> site = site_.object();
    ElementsKind kind =
        site->PointsToLiteral()
            ? site->boilerplate(kAcquireLoad).map().elements_kind()
            : site->GetElementsKind();
    return kind_ == kind;
  }
  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    deps->Register(site_.object(),
                   DependentCode::kAllocationSiteTransitionChangedGroup);
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(site_), static_cast<int>(kind_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const ElementsKindDependency* const zat = that->AsElementsKind();
    return site_.equals(zat->site_) && kind_ == zat->kind_;
  }

  const AllocationSiteRef site_;
  const ElementsKind kind_;
};

// Only valid if the holder can use direct reads, since validation uses
// GetOwnConstantElementFromHeap.
class OwnConstantElementDependency final : public CompilationDependency {
 public:
  OwnConstantElementDependency(const JSObjectRef& holder, uint32_t index,
                               const ObjectRef& element)
      : CompilationDependency(kOwnConstantElement),
        holder_(holder),
        index_(index),
        element_(element) {}

  bool IsValid() const override {
    DisallowGarbageCollection no_gc;
    JSObject holder = *holder_.object();
    base::Optional<Object> maybe_element =
        holder_.GetOwnConstantElementFromHeap(holder.elements(),
                                              holder.GetElementsKind(), index_);
    if (!maybe_element.has_value()) return false;

    return maybe_element.value() == *element_.object();
  }
  void Install(PendingDependencies* deps) const override {}

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(holder_), index_, h(element_));
  }

  bool Equals(const CompilationDependency* that) const override {
    const OwnConstantElementDependency* const zat =
        that->AsOwnConstantElement();
    return holder_.equals(zat->holder_) && index_ == zat->index_ &&
           element_.equals(zat->element_);
  }

  const JSObjectRef holder_;
  const uint32_t index_;
  const ObjectRef element_;
};

class InitialMapInstanceSizePredictionDependency final
    : public CompilationDependency {
 public:
  InitialMapInstanceSizePredictionDependency(const JSFunctionRef& function,
                                             int instance_size)
      : CompilationDependency(kInitialMapInstanceSizePrediction),
        function_(function),
        instance_size_(instance_size) {}

  bool IsValid() const override {
    // The dependency is valid if the prediction is the same as the current
    // slack tracking result.
    if (!function_.object()->has_initial_map()) return false;
    int instance_size = function_.object()->ComputeInstanceSizeWithMinSlack(
        function_.isolate());
    return instance_size == instance_size_;
  }

  void PrepareInstall() const override {
    SLOW_DCHECK(IsValid());
    function_.object()->CompleteInobjectSlackTrackingIfActive();
  }

  void Install(PendingDependencies* deps) const override {
    SLOW_DCHECK(IsValid());
    DCHECK(
        !function_.object()->initial_map().IsInobjectSlackTrackingInProgress());
  }

 private:
  size_t Hash() const override {
    ObjectRef::Hash h;
    return base::hash_combine(h(function_), instance_size_);
  }

  bool Equals(const CompilationDependency* that) const override {
    const InitialMapInstanceSizePredictionDependency* const zat =
        that->AsInitialMapInstanceSizePrediction();
    return function_.equals(zat->function_) &&
           instance_size_ == zat->instance_size_;
  }

  const JSFunctionRef function_;
  const int instance_size_;
};

}  // namespace

void CompilationDependencies::RecordDependency(
    CompilationDependency const* dependency) {
  if (dependency != nullptr) dependencies_.insert(dependency);
}

MapRef CompilationDependencies::DependOnInitialMap(
    const JSFunctionRef& function) {
  MapRef map = function.initial_map(this);
  RecordDependency(zone_->New<InitialMapDependency>(broker_, function, map));
  return map;
}

ObjectRef CompilationDependencies::DependOnPrototypeProperty(
    const JSFunctionRef& function) {
  ObjectRef prototype = function.instance_prototype(this);
  RecordDependency(
      zone_->New<PrototypePropertyDependency>(broker_, function, prototype));
  return prototype;
}

void CompilationDependencies::DependOnStableMap(const MapRef& map) {
  if (map.CanTransition()) {
    RecordDependency(zone_->New<StableMapDependency>(map));
  }
}

void CompilationDependencies::DependOnConstantInDictionaryPrototypeChain(
    const MapRef& receiver_map, const NameRef& property_name,
    const ObjectRef& constant, PropertyKind kind) {
  RecordDependency(zone_->New<ConstantInDictionaryPrototypeChainDependency>(
      receiver_map, property_name, constant, kind));
}

AllocationType CompilationDependencies::DependOnPretenureMode(
    const AllocationSiteRef& site) {
  if (!FLAG_allocation_site_pretenuring) return AllocationType::kYoung;
  AllocationType allocation = site.GetAllocationType();
  RecordDependency(zone_->New<PretenureModeDependency>(site, allocation));
  return allocation;
}

PropertyConstness CompilationDependencies::DependOnFieldConstness(
    const MapRef& map, InternalIndex descriptor) {
  PropertyConstness constness = map.GetPropertyDetails(descriptor).constness();
  if (constness == PropertyConstness::kMutable) return constness;

  // If the map can have fast elements transitions, then the field can be only
  // considered constant if the map does not transition.
  if (Map::CanHaveFastTransitionableElementsKind(map.instance_type())) {
    // If the map can already transition away, let us report the field as
    // mutable.
    if (!map.is_stable()) {
      return PropertyConstness::kMutable;
    }
    DependOnStableMap(map);
  }

  DCHECK_EQ(constness, PropertyConstness::kConst);
  RecordDependency(zone_->New<FieldConstnessDependency>(map, descriptor));
  return PropertyConstness::kConst;
}

void CompilationDependencies::DependOnGlobalProperty(
    const PropertyCellRef& cell) {
  PropertyCellType type = cell.property_details().cell_type();
  bool read_only = cell.property_details().IsReadOnly();
  RecordDependency(zone_->New<GlobalPropertyDependency>(cell, type, read_only));
}

bool CompilationDependencies::DependOnProtector(const PropertyCellRef& cell) {
  cell.CacheAsProtector();
  if (cell.value().AsSmi() != Protectors::kProtectorValid) return false;
  RecordDependency(zone_->New<ProtectorDependency>(cell));
  return true;
}

bool CompilationDependencies::DependOnMegaDOMProtector() {
  return DependOnProtector(
      MakeRef(broker_, broker_->isolate()->factory()->mega_dom_protector()));
}

bool CompilationDependencies::DependOnArrayBufferDetachingProtector() {
  return DependOnProtector(MakeRef(
      broker_,
      broker_->isolate()->factory()->array_buffer_detaching_protector()));
}

bool CompilationDependencies::DependOnArrayIteratorProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->array_iterator_protector()));
}

bool CompilationDependencies::DependOnArraySpeciesProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->array_species_protector()));
}

bool CompilationDependencies::DependOnNoElementsProtector() {
  return DependOnProtector(
      MakeRef(broker_, broker_->isolate()->factory()->no_elements_protector()));
}

bool CompilationDependencies::DependOnPromiseHookProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->promise_hook_protector()));
}

bool CompilationDependencies::DependOnPromiseSpeciesProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->promise_species_protector()));
}

bool CompilationDependencies::DependOnPromiseThenProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->promise_then_protector()));
}

void CompilationDependencies::DependOnElementsKind(
    const AllocationSiteRef& site) {
  ElementsKind kind = site.PointsToLiteral()
                          ? site.boilerplate().value().map().elements_kind()
                          : site.GetElementsKind();
  if (AllocationSite::ShouldTrack(kind)) {
    RecordDependency(zone_->New<ElementsKindDependency>(site, kind));
  }
}

void CompilationDependencies::DependOnOwnConstantElement(
    const JSObjectRef& holder, uint32_t index, const ObjectRef& element) {
  RecordDependency(
      zone_->New<OwnConstantElementDependency>(holder, index, element));
}

void CompilationDependencies::DependOnOwnConstantDataProperty(
    const JSObjectRef& holder, const MapRef& map, Representation representation,
    FieldIndex index, const ObjectRef& value) {
  RecordDependency(zone_->New<OwnConstantDataPropertyDependency>(
      broker_, holder, map, representation, index, value));
}

void CompilationDependencies::DependOnOwnConstantDictionaryProperty(
    const JSObjectRef& holder, InternalIndex index, const ObjectRef& value) {
  RecordDependency(zone_->New<OwnConstantDictionaryPropertyDependency>(
      broker_, holder, index, value));
}

V8_INLINE void TraceInvalidCompilationDependency(
    const CompilationDependency* d) {
  DCHECK(FLAG_trace_compilation_dependencies);
  DCHECK(!d->IsValid());
  PrintF("Compilation aborted due to invalid dependency: %s\n", d->ToString());
}

bool CompilationDependencies::Commit(Handle<Code> code) {
  if (!PrepareInstall()) return false;

  {
    PendingDependencies pending_deps(zone_);
    DisallowCodeDependencyChange no_dependency_change;
    for (const CompilationDependency* dep : dependencies_) {
      // Check each dependency's validity again right before installing it,
      // because the first iteration above might have invalidated some
      // dependencies. For example, PrototypePropertyDependency::PrepareInstall
      // can call EnsureHasInitialMap, which can invalidate a
      // StableMapDependency on the prototype object's map.
      if (!dep->IsValid()) {
        if (FLAG_trace_compilation_dependencies) {
          TraceInvalidCompilationDependency(dep);
        }
        dependencies_.clear();
        return false;
      }
      dep->Install(&pending_deps);
    }
    pending_deps.InstallAll(broker_->isolate(), code);
  }

  // It is even possible that a GC during the above installations invalidated
  // one of the dependencies. However, this should only affect
  //
  // 1. pretenure mode dependencies, or
  // 2. function consistency dependencies,
  //
  // which we assert below. It is safe to return successfully in these cases,
  // because
  //
  // 1. once the code gets executed it will do a stack check that triggers its
  //    deoptimization.
  // 2. since the function state was deemed consistent above, that means the
  //    compilation saw a self-consistent state of the jsfunction.
  if (FLAG_stress_gc_during_compilation) {
    broker_->isolate()->heap()->PreciseCollectAllGarbage(
        Heap::kForcedGC, GarbageCollectionReason::kTesting, kNoGCCallbackFlags);
  }
#ifdef DEBUG
  for (auto dep : dependencies_) {
    CHECK_IMPLIES(!dep->IsValid(),
                  dep->IsPretenureMode() || dep->IsConsistentJSFunctionView());
  }
#endif

  dependencies_.clear();
  return true;
}

bool CompilationDependencies::PrepareInstall() {
  if (V8_UNLIKELY(FLAG_predictable)) {
    return PrepareInstallPredictable();
  }

  for (auto dep : dependencies_) {
    if (!dep->IsValid()) {
      if (FLAG_trace_compilation_dependencies) {
        TraceInvalidCompilationDependency(dep);
      }
      dependencies_.clear();
      return false;
    }
    dep->PrepareInstall();
  }
  return true;
}

bool CompilationDependencies::PrepareInstallPredictable() {
  CHECK(FLAG_predictable);

  std::vector<const CompilationDependency*> deps(dependencies_.begin(),
                                                 dependencies_.end());
  std::sort(deps.begin(), deps.end());

  for (auto dep : deps) {
    if (!dep->IsValid()) {
      if (FLAG_trace_compilation_dependencies) {
        TraceInvalidCompilationDependency(dep);
      }
      dependencies_.clear();
      return false;
    }
    dep->PrepareInstall();
  }
  return true;
}

#define V(Name)                                                     \
  const Name##Dependency* CompilationDependency::As##Name() const { \
    DCHECK(Is##Name());                                             \
    return static_cast<const Name##Dependency*>(this);              \
  }
DEPENDENCY_LIST(V)
#undef V

void CompilationDependencies::DependOnStablePrototypeChains(
    ZoneVector<MapRef> const& receiver_maps, WhereToStart start,
    base::Optional<JSObjectRef> last_prototype) {
  for (MapRef receiver_map : receiver_maps) {
    DependOnStablePrototypeChain(receiver_map, start, last_prototype);
  }
}

void CompilationDependencies::DependOnStablePrototypeChain(
    MapRef receiver_map, WhereToStart start,
    base::Optional<JSObjectRef> last_prototype) {
  if (receiver_map.IsPrimitiveMap()) {
    // Perform the implicit ToObject for primitives here.
    // Implemented according to ES6 section 7.3.2 GetV (V, P).
    // Note: Keep sync'd with AccessInfoFactory::ComputePropertyAccessInfo.
    base::Optional<JSFunctionRef> constructor =
        broker_->target_native_context().GetConstructorFunction(receiver_map);
    receiver_map = constructor.value().initial_map(this);
  }
  if (start == kStartAtReceiver) DependOnStableMap(receiver_map);

  MapRef map = receiver_map;
  while (true) {
    HeapObjectRef proto = map.prototype();
    if (!proto.IsJSObject()) {
      CHECK_EQ(proto.map().oddball_type(), OddballType::kNull);
      break;
    }
    map = proto.map();
    DependOnStableMap(map);
    if (last_prototype.has_value() && proto.equals(*last_prototype)) break;
  }
}

void CompilationDependencies::DependOnElementsKinds(
    const AllocationSiteRef& site) {
  AllocationSiteRef current = site;
  while (true) {
    DependOnElementsKind(current);
    if (!current.nested_site().IsAllocationSite()) break;
    current = current.nested_site().AsAllocationSite();
  }
  CHECK_EQ(current.nested_site().AsSmi(), 0);
}

void CompilationDependencies::DependOnConsistentJSFunctionView(
    const JSFunctionRef& function) {
  RecordDependency(zone_->New<ConsistentJSFunctionViewDependency>(function));
}

SlackTrackingPrediction::SlackTrackingPrediction(MapRef initial_map,
                                                 int instance_size)
    : instance_size_(instance_size),
      inobject_property_count_(
          (instance_size >> kTaggedSizeLog2) -
          initial_map.GetInObjectPropertiesStartInWords()) {}

SlackTrackingPrediction
CompilationDependencies::DependOnInitialMapInstanceSizePrediction(
    const JSFunctionRef& function) {
  MapRef initial_map = DependOnInitialMap(function);
  int instance_size = function.InitialMapInstanceSizeWithMinSlack(this);
  // Currently, we always install the prediction dependency. If this turns out
  // to be too expensive, we can only install the dependency if slack
  // tracking is active.
  RecordDependency(zone_->New<InitialMapInstanceSizePredictionDependency>(
      function, instance_size));
  CHECK_LE(instance_size, function.initial_map(this).instance_size());
  return SlackTrackingPrediction(initial_map, instance_size);
}

CompilationDependency const*
CompilationDependencies::TransitionDependencyOffTheRecord(
    const MapRef& target_map) const {
  if (target_map.CanBeDeprecated()) {
    return zone_->New<TransitionDependency>(target_map);
  } else {
    DCHECK(!target_map.is_deprecated());
    return nullptr;
  }
}

CompilationDependency const*
CompilationDependencies::FieldRepresentationDependencyOffTheRecord(
    const MapRef& map, InternalIndex descriptor,
    Representation representation) const {
  return zone_->New<FieldRepresentationDependency>(map, descriptor,
                                                   representation);
}

CompilationDependency const*
CompilationDependencies::FieldTypeDependencyOffTheRecord(
    const MapRef& map, InternalIndex descriptor, const ObjectRef& type) const {
  return zone_->New<FieldTypeDependency>(map, descriptor, type);
}

#ifdef DEBUG
// static
bool CompilationDependencies::IsFieldRepresentationDependencyOnMap(
    const CompilationDependency* dep, const Handle<Map>& receiver_map) {
  return dep->IsFieldRepresentation() &&
         dep->AsFieldRepresentation()->DependsOn(receiver_map);
}
#endif  // DEBUG

#undef DEPENDENCY_LIST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
