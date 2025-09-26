#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_REGISTRY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_REGISTRY_H_

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "runtime/function.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_registry.h"

namespace google::api::expr::runtime {

// CelFunctionRegistry class allows to register builtin or custom
// CelFunction handlers with it and look them up when creating
// CelExpression objects from Expr ASTs.
class CelFunctionRegistry {
 public:
  // Represents a single overload for a lazily provided function.
  using LazyOverload = cel::FunctionRegistry::LazyOverload;

  CelFunctionRegistry() = default;

  ~CelFunctionRegistry() = default;

  using Registrar = absl::Status (*)(CelFunctionRegistry*,
                                     const InterpreterOptions&);

  // Register CelFunction object. Object ownership is
  // passed to registry.
  // Function registration should be performed prior to
  // CelExpression creation.
  absl::Status Register(std::unique_ptr<CelFunction> function) {
    // We need to copy the descriptor, otherwise there is no guarantee that the
    // lvalue reference to the descriptor is valid as function may be destroyed.
    auto descriptor = function->descriptor();
    return Register(descriptor, std::move(function));
  }

  absl::Status Register(const cel::FunctionDescriptor& descriptor,
                        std::unique_ptr<cel::Function> implementation) {
    return modern_registry_.Register(descriptor, std::move(implementation));
  }

  absl::Status RegisterAll(std::initializer_list<Registrar> registrars,
                           const InterpreterOptions& opts);

  // Register a lazily provided function. This overload uses a default provider
  // that delegates to the activation at evaluation time.
  absl::Status RegisterLazyFunction(const CelFunctionDescriptor& descriptor) {
    return modern_registry_.RegisterLazyFunction(descriptor);
  }

  // Find a subset of CelFunction that match overload conditions
  // As types may not be available during expression compilation,
  // further narrowing of this subset will happen at evaluation stage.
  // name - the name of CelFunction;
  // receiver_style - indicates whether function has receiver style;
  // types - argument types. If  type is not known during compilation,
  // DYN value should be passed.
  //
  // Results refer to underlying registry entries by pointer. Results are
  // invalid after the registry is deleted.
  std::vector<const CelFunction*> FindOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<CelValue::Type>& types) const;

  std::vector<cel::FunctionOverloadReference> FindStaticOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<cel::Kind>& types) const {
    return modern_registry_.FindStaticOverloads(name, receiver_style, types);
  }

  // Find subset of CelFunction providers that match overload conditions
  // As types may not be available during expression compilation,
  // further narrowing of this subset will happen at evaluation stage.
  // name - the name of CelFunction;
  // receiver_style - indicates whether function has receiver style;
  // types - argument types. If  type is not known during compilation,
  // DYN value should be passed.
  std::vector<const CelFunctionDescriptor*> FindLazyOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<CelValue::Type>& types) const;

  // Find subset of CelFunction providers that match overload conditions
  // As types may not be available during expression compilation,
  // further narrowing of this subset will happen at evaluation stage.
  // name - the name of CelFunction;
  // receiver_style - indicates whether function has receiver style;
  // types - argument types. If  type is not known during compilation,
  // DYN value should be passed.
  std::vector<LazyOverload> ModernFindLazyOverloads(
      absl::string_view name, bool receiver_style,
      const std::vector<CelValue::Type>& types) const {
    return modern_registry_.FindLazyOverloads(name, receiver_style, types);
  }

  // Retrieve list of registered function descriptors. This includes both
  // static and lazy functions.
  absl::node_hash_map<std::string, std::vector<const cel::FunctionDescriptor*>>
  ListFunctions() const {
    return modern_registry_.ListFunctions();
  }

  // cel internal accessor for returning backing modern registry.
  //
  // This is intended to allow migrating the CEL evaluator internals while
  // maintaining the existing CelRegistry API.
  //
  // CEL users should not use this.
  const cel::FunctionRegistry& InternalGetRegistry() const {
    return modern_registry_;
  }

  cel::FunctionRegistry& InternalGetRegistry() { return modern_registry_; }

 private:
  cel::FunctionRegistry modern_registry_;

  // Maintain backwards compatibility for callers expecting CelFunction
  // interface.
  // This is not used internally, but some client tests check that a specific
  // CelFunction overload is used.
  // Lazily initialized.
  mutable absl::Mutex mu_;
  mutable absl::flat_hash_map<const cel::Function*,
                              std::unique_ptr<CelFunction>>
      functions_ ABSL_GUARDED_BY(mu_);
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_FUNCTION_REGISTRY_H_
