#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONTAINER_BACKED_MAP_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONTAINER_BACKED_MAP_IMPL_H_

#include <memory>
#include <utility>

#include "absl/container/node_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// CelMap implementation that uses STL map container as backing storage.
// KeyType is the type of key values stored in CelValue.
// After building, upcast to CelMap to prevent further additions.
class CelMapBuilder : public CelMap {
 public:
  CelMapBuilder() {}

  // Try to insert a key value pair into the map. Returns a status if key
  // already exists.
  absl::Status Add(CelValue key, CelValue value);

  int size() const override { return values_map_.size(); }

  absl::optional<CelValue> operator[](CelValue cel_key) const override;

  absl::StatusOr<bool> Has(const CelValue& cel_key) const override {
    return values_map_.contains(cel_key);
  }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return &key_list_;
  }

 private:
  // Custom CelList implementation for maintaining key list.
  class KeyList : public CelList {
   public:
    KeyList() {}

    int size() const override { return keys_.size(); }

    CelValue operator[](int index) const override { return keys_[index]; }

    void Add(const CelValue& key) { keys_.push_back(key); }

   private:
    std::vector<CelValue> keys_;
  };

  struct Hasher {
    size_t operator()(const CelValue& key) const;
  };
  struct Equal {
    bool operator()(const CelValue& key1, const CelValue& key2) const;
  };

  absl::node_hash_map<CelValue, CelValue, Hasher, Equal> values_map_;
  KeyList key_list_;
};

// Factory method creating container-backed CelMap.
absl::StatusOr<std::unique_ptr<CelMap>> CreateContainerBackedMap(
    absl::Span<const std::pair<CelValue, CelValue>> key_values);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_CONTAINER_BACKED_MAP_IMPL_H_
