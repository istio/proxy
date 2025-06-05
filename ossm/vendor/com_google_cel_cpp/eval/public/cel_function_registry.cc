#include "eval/public/cel_function_registry.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "base/function.h"
#include "base/function_descriptor.h"
#include "base/type_provider.h"
#include "common/type_manager.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/legacy_value_manager.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "runtime/function_overload_reference.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::extensions::ProtoMemoryManagerRef;

// Legacy cel function that proxies to the modern cel::Function interface.
//
// This is used to wrap new-style cel::Functions for clients consuming
// legacy CelFunction-based APIs. The evaluate implementation on this class
// should not be called by the CEL evaluator, but a sensible result is returned
// for unit tests that haven't been migrated to the new APIs yet.
class ProxyToModernCelFunction : public CelFunction {
 public:
  ProxyToModernCelFunction(const cel::FunctionDescriptor& descriptor,
                           const cel::Function& implementation)
      : CelFunction(descriptor), implementation_(&implementation) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    // This is only safe for use during interop where the MemoryManager is
    // assumed to always be backed by a google::protobuf::Arena instance. After all
    // dependencies on legacy CelFunction are removed, we can remove this
    // implementation.
    auto memory_manager = ProtoMemoryManagerRef(arena);
    cel::common_internal::LegacyValueManager manager(
        memory_manager, cel::TypeProvider::Builtin());
    cel::FunctionEvaluationContext context(manager);

    std::vector<cel::Value> modern_args =
        cel::interop_internal::LegacyValueToModernValueOrDie(arena, args);

    CEL_ASSIGN_OR_RETURN(auto modern_result,
                         implementation_->Invoke(context, modern_args));

    *result = cel::interop_internal::ModernValueToLegacyValueOrDie(
        arena, modern_result);

    return absl::OkStatus();
  }

 private:
  // owned by the registry
  const cel::Function* implementation_;
};

}  // namespace

absl::Status CelFunctionRegistry::RegisterAll(
    std::initializer_list<Registrar> registrars,
    const InterpreterOptions& opts) {
  for (Registrar registrar : registrars) {
    CEL_RETURN_IF_ERROR(registrar(this, opts));
  }
  return absl::OkStatus();
}

std::vector<const CelFunction*> CelFunctionRegistry::FindOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<CelValue::Type>& types) const {
  std::vector<cel::FunctionOverloadReference> matched_funcs =
      modern_registry_.FindStaticOverloads(name, receiver_style, types);

  // For backwards compatibility, lazily initialize a legacy CEL function
  // if required.
  // The registry should remain add-only until migration to the new type is
  // complete, so this should work whether the function was introduced via
  // the modern registry or the old registry wrapping a modern instance.
  std::vector<const CelFunction*> results;
  results.reserve(matched_funcs.size());

  {
    absl::MutexLock lock(&mu_);
    for (cel::FunctionOverloadReference entry : matched_funcs) {
      std::unique_ptr<CelFunction>& legacy_impl =
          functions_[&entry.implementation];

      if (legacy_impl == nullptr) {
        legacy_impl = std::make_unique<ProxyToModernCelFunction>(
            entry.descriptor, entry.implementation);
      }
      results.push_back(legacy_impl.get());
    }
  }
  return results;
}

std::vector<const CelFunctionDescriptor*>
CelFunctionRegistry::FindLazyOverloads(
    absl::string_view name, bool receiver_style,
    const std::vector<CelValue::Type>& types) const {
  std::vector<LazyOverload> lazy_overloads =
      modern_registry_.FindLazyOverloads(name, receiver_style, types);
  std::vector<const CelFunctionDescriptor*> result;
  result.reserve(lazy_overloads.size());

  for (const LazyOverload& overload : lazy_overloads) {
    result.push_back(&overload.descriptor);
  }
  return result;
}

}  // namespace google::api::expr::runtime
