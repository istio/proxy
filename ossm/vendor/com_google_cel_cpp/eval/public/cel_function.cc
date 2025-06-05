#include "eval/public/cel_function.h"

#include <cstddef>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "base/function.h"
#include "common/value.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

using ::cel::FunctionEvaluationContext;

using ::cel::Value;
using ::cel::extensions::ProtoMemoryManagerArena;
using ::cel::interop_internal::ToLegacyValue;

bool CelFunction::MatchArguments(absl::Span<const CelValue> arguments) const {
  auto types_size = descriptor().types().size();

  if (types_size != arguments.size()) {
    return false;
  }
  for (size_t i = 0; i < types_size; i++) {
    const auto& value = arguments[i];
    CelValue::Type arg_type = descriptor().types()[i];
    if (value.type() != arg_type && arg_type != CelValue::Type::kAny) {
      return false;
    }
  }

  return true;
}

bool CelFunction::MatchArguments(absl::Span<const cel::Value> arguments) const {
  auto types_size = descriptor().types().size();

  if (types_size != arguments.size()) {
    return false;
  }
  for (size_t i = 0; i < types_size; i++) {
    const auto& value = arguments[i];
    CelValue::Type arg_type = descriptor().types()[i];
    if (value->kind() != arg_type && arg_type != CelValue::Type::kAny) {
      return false;
    }
  }

  return true;
}

absl::StatusOr<Value> CelFunction::Invoke(
    const FunctionEvaluationContext& context,
    absl::Span<const Value> arguments) const {
  google::protobuf::Arena* arena =
      ProtoMemoryManagerArena(context.value_factory().GetMemoryManager());
  std::vector<CelValue> legacy_args;
  legacy_args.reserve(arguments.size());

  // Users shouldn't be able to create expressions that call registered
  // functions with unconvertible types, but it's possible to create an AST that
  // can trigger this by making an unexpected call on a value that the
  // interpreter expects to only be used with internal program steps.
  for (const auto& arg : arguments) {
    CEL_ASSIGN_OR_RETURN(legacy_args.emplace_back(),
                         ToLegacyValue(arena, arg, true));
  }

  CelValue legacy_result;

  CEL_RETURN_IF_ERROR(Evaluate(legacy_args, &legacy_result, arena));

  return cel::interop_internal::LegacyValueToModernValueOrDie(
      arena, legacy_result, /*unchecked=*/true);
}

}  // namespace google::api::expr::runtime
