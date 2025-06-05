#include "eval/public/activation.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_function.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

absl::optional<CelValue> Activation::FindValue(absl::string_view name,
                                               google::protobuf::Arena* arena) const {
  auto entry = value_map_.find(name);

  // No entry found.
  if (entry == value_map_.end()) {
    return {};
  }

  return entry->second.RetrieveValue(arena);
}

absl::Status Activation::InsertFunction(std::unique_ptr<CelFunction> function) {
  auto& overloads = function_map_[function->descriptor().name()];
  for (const auto& overload : overloads) {
    if (overload->descriptor().ShapeMatches(function->descriptor())) {
      return absl::InvalidArgumentError(
          "Function with same shape already defined in activation");
    }
  }
  overloads.emplace_back(std::move(function));
  return absl::OkStatus();
}

std::vector<const CelFunction*> Activation::FindFunctionOverloads(
    absl::string_view name) const {
  const auto map_entry = function_map_.find(name);
  std::vector<const CelFunction*> overloads;
  if (map_entry == function_map_.end()) {
    return overloads;
  }
  overloads.resize(map_entry->second.size());
  std::transform(map_entry->second.begin(), map_entry->second.end(),
                 overloads.begin(),
                 [](const auto& func) { return func.get(); });
  return overloads;
}

bool Activation::RemoveFunctionEntries(
    const CelFunctionDescriptor& descriptor) {
  auto map_entry = function_map_.find(descriptor.name());
  if (map_entry == function_map_.end()) {
    return false;
  }
  std::vector<std::unique_ptr<CelFunction>>& overloads = map_entry->second;
  bool funcs_removed = false;
  auto func_iter = overloads.begin();
  while (func_iter != overloads.end()) {
    if (descriptor.ShapeMatches(func_iter->get()->descriptor())) {
      func_iter = overloads.erase(func_iter);
      funcs_removed = true;
    } else {
      ++func_iter;
    }
  }

  if (overloads.empty()) {
    function_map_.erase(map_entry);
  }

  return funcs_removed;
}

void Activation::InsertValue(absl::string_view name, const CelValue& value) {
  value_map_.try_emplace(name, ValueEntry(value));
}

void Activation::InsertValueProducer(
    absl::string_view name, std::unique_ptr<CelValueProducer> value_producer) {
  value_map_.try_emplace(name, ValueEntry(std::move(value_producer)));
}

bool Activation::RemoveValueEntry(absl::string_view name) {
  return value_map_.erase(name);
}

bool Activation::ClearValueEntry(absl::string_view name) {
  auto entry = value_map_.find(name);

  // No entry found.
  if (entry == value_map_.end()) {
    return false;
  }

  return entry->second.ClearValue();
}

int Activation::ClearCachedValues() {
  int n = 0;
  for (auto& entry : value_map_) {
    if (entry.second.HasProducer()) {
      if (entry.second.ClearValue()) {
        n++;
      }
    }
  }
  return n;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
