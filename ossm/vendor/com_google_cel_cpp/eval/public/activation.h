#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/util/field_mask_util.h"
#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "eval/public/base_activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"
#include "eval/public/cel_value_producer.h"

namespace google::api::expr::runtime {

// Instance of Activation class is used by evaluator.
// It provides binding between references used in expressions
// and actual values.
class Activation : public BaseActivation {
 public:
  Activation() = default;

  // Non-copyable/non-assignable
  Activation(const Activation&) = delete;
  Activation& operator=(const Activation&) = delete;

  // Move-constructible/move-assignable
  Activation(Activation&& other) = default;
  Activation& operator=(Activation&& other) = default;

  // BaseActivation
  std::vector<const CelFunction*> FindFunctionOverloads(
      absl::string_view name) const override;

  absl::optional<CelValue> FindValue(absl::string_view name,
                                     google::protobuf::Arena* arena) const override;

  // Insert a function into the activation (ie a lazily bound function). Returns
  // a status if the name and shape of the function matches another one that has
  // already been bound.
  absl::Status InsertFunction(std::unique_ptr<CelFunction> function);

  // Insert value into Activation.
  void InsertValue(absl::string_view name, const CelValue& value);

  // Insert ValueProducer into Activation.
  void InsertValueProducer(absl::string_view name,
                           std::unique_ptr<CelValueProducer> value_producer);

  // Remove functions that have the same name and shape as descriptor. Returns
  // true if matching functions were found and removed.
  bool RemoveFunctionEntries(const CelFunctionDescriptor& descriptor);

  // Removes value or producer, returns true if entry with the name was found
  bool RemoveValueEntry(absl::string_view name);

  // Clears a cached value for a value producer, returns if true if entry was
  // found and cleared.
  bool ClearValueEntry(absl::string_view name);

  // Clears all cached values for value producers. Returns the number of entries
  // cleared.
  int ClearCachedValues();

  // Set missing attribute patterns for evaluation.
  //
  // If a field access is found to match any of the provided patterns, the
  // result is treated as a missing attribute error.
  void set_missing_attribute_patterns(
      std::vector<CelAttributePattern> missing_attribute_patterns) {
    missing_attribute_patterns_ = std::move(missing_attribute_patterns);
  }

  // Return FieldMask defining the list of unknown paths.
  const std::vector<CelAttributePattern>& missing_attribute_patterns()
      const override {
    return missing_attribute_patterns_;
  }

  // Sets the collection of attribute patterns that will be recognized as
  // "unknown" values during expression evaluation.
  void set_unknown_attribute_patterns(
      std::vector<CelAttributePattern> unknown_attribute_patterns) {
    unknown_attribute_patterns_ = std::move(unknown_attribute_patterns);
  }

  // Return the collection of attribute patterns that determine "unknown"
  // values.
  const std::vector<CelAttributePattern>& unknown_attribute_patterns()
      const override {
    return unknown_attribute_patterns_;
  }

 private:
  class ValueEntry {
   public:
    explicit ValueEntry(std::unique_ptr<CelValueProducer> prod)
        : value_(), producer_(std::move(prod)) {}

    explicit ValueEntry(const CelValue& value) : value_(value), producer_() {}

    // Retrieve associated CelValue.
    // If the value is not set and producer is set,
    // obtain and cache value from producer.
    absl::optional<CelValue> RetrieveValue(google::protobuf::Arena* arena) const {
      if (!value_.has_value()) {
        if (producer_) {
          value_ = producer_->Produce(arena);
        }
      }

      return value_;
    }

    bool ClearValue() {
      bool result = value_.has_value();
      value_.reset();
      return result;
    }

    bool HasProducer() const { return producer_ != nullptr; }

   private:
    mutable absl::optional<CelValue> value_;
    std::unique_ptr<CelValueProducer> producer_;
  };

  absl::flat_hash_map<std::string, ValueEntry> value_map_;
  absl::flat_hash_map<std::string, std::vector<std::unique_ptr<CelFunction>>>
      function_map_;

  std::vector<CelAttributePattern> missing_attribute_patterns_;
  std::vector<CelAttributePattern> unknown_attribute_patterns_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_H_
