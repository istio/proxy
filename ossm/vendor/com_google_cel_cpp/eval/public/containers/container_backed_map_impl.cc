#include "eval/public/containers/container_backed_map_impl.h"

#include <memory>
#include <utility>

#include "absl/container/node_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

// Helper classes for CelValue hasher function
// We care only for hash operations for integral/string types, but others
// should be present as well for CelValue::Visit(HasherOp) to compile.
class HasherOp {
 public:
  template <class T>
  size_t operator()(const T& arg) {
    return std::hash<T>()(arg);
  }

  size_t operator()(const absl::Time arg) {
    return absl::Hash<absl::Time>()(arg);
  }

  size_t operator()(const absl::Duration arg) {
    return absl::Hash<absl::Duration>()(arg);
  }

  size_t operator()(const CelValue::StringHolder& arg) {
    return absl::Hash<absl::string_view>()(arg.value());
  }

  size_t operator()(const CelValue::BytesHolder& arg) {
    return absl::Hash<absl::string_view>()(arg.value());
  }

  size_t operator()(const CelValue::CelTypeHolder& arg) {
    return absl::Hash<absl::string_view>()(arg.value());
  }

  // Needed for successful compilation resolution.
  size_t operator()(const std::nullptr_t&) { return 0; }
};

// Helper classes to provide CelValue equality comparison operation
template <class T>
class EqualOp {
 public:
  explicit EqualOp(const T& arg) : arg_(arg) {}

  template <class U>
  bool operator()(const U&) const {
    return false;
  }

  bool operator()(const T& other) const { return other == arg_; }

 private:
  const T& arg_;
};

class CelValueEq {
 public:
  explicit CelValueEq(const CelValue& other) : other_(other) {}

  template <class Type>
  bool operator()(const Type& arg) {
    return other_.template Visit<bool>(EqualOp<Type>(arg));
  }

 private:
  const CelValue& other_;
};

}  // namespace

// Map element access operator.
absl::optional<CelValue> CelMapBuilder::operator[](CelValue cel_key) const {
  auto item = values_map_.find(cel_key);
  if (item == values_map_.end()) {
    return absl::nullopt;
  }
  return item->second;
}

absl::Status CelMapBuilder::Add(CelValue key, CelValue value) {
  auto [unused, inserted] = values_map_.emplace(key, value);

  if (!inserted) {
    return absl::InvalidArgumentError("duplicate map keys");
  }
  key_list_.Add(key);
  return absl::OkStatus();
}

// CelValue hasher functor.
size_t CelMapBuilder::Hasher::operator()(const CelValue& key) const {
  return key.template Visit<size_t>(HasherOp());
}

bool CelMapBuilder::Equal::operator()(const CelValue& key1,
                                      const CelValue& key2) const {
  if (key1.type() != key2.type()) {
    return false;
  }
  return key1.template Visit<bool>(CelValueEq(key2));
}

absl::StatusOr<std::unique_ptr<CelMap>> CreateContainerBackedMap(
    absl::Span<const std::pair<CelValue, CelValue>> key_values) {
  auto map = std::make_unique<CelMapBuilder>();
  for (const auto& key_value : key_values) {
    CEL_RETURN_IF_ERROR(map->Add(key_value.first, key_value.second));
  }
  return map;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
