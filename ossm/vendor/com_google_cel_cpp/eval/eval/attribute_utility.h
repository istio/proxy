#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "base/attribute_set.h"
#include "base/function_descriptor.h"
#include "base/function_result_set.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/attribute_trail.h"

namespace google::api::expr::runtime {

// Helper class for handling unknowns and missing attribute logic. Provides
// helpers for merging unknown sets from arguments on the stack and for
// identifying unknown/missing attributes based on the patterns for a given
// Evaluation.
// Neither moveable nor copyable.
class AttributeUtility {
 public:
  class Accumulator {
   public:
    Accumulator(const Accumulator&) = delete;
    Accumulator& operator=(const Accumulator&) = delete;
    Accumulator(Accumulator&&) = delete;
    Accumulator& operator=(Accumulator&&) = delete;

    // Add to the accumulated unknown attributes and functions.
    void Add(const cel::UnknownValue& v);
    void Add(const AttributeTrail& attr);

    // Add to the accumulated set of unknowns if value is UnknownValue.
    void MaybeAdd(const cel::Value& v);

    bool IsEmpty() const;

    cel::UnknownValue Build() &&;

   private:
    explicit Accumulator(const AttributeUtility& parent)
        : parent_(parent), unknown_present_(false) {}

    friend class AttributeUtility;
    const AttributeUtility& parent_;

    cel::AttributeSet attribute_set_;
    cel::FunctionResultSet function_result_set_;

    // Some tests will use an empty unknown set as a sentinel.
    // Preserve forwarding behavior.
    bool unknown_present_;
  };

  AttributeUtility(
      absl::Span<const cel::AttributePattern> unknown_patterns,
      absl::Span<const cel::AttributePattern> missing_attribute_patterns,
      cel::ValueManager& value_factory)
      : unknown_patterns_(unknown_patterns),
        missing_attribute_patterns_(missing_attribute_patterns),
        value_factory_(value_factory) {}

  AttributeUtility(const AttributeUtility&) = delete;
  AttributeUtility& operator=(const AttributeUtility&) = delete;
  AttributeUtility(AttributeUtility&&) = delete;
  AttributeUtility& operator=(AttributeUtility&&) = delete;

  // Checks whether particular corresponds to any patterns that define missing
  // attribute.
  bool CheckForMissingAttribute(const AttributeTrail& trail) const;

  // Checks whether trail corresponds to any patterns that define unknowns.
  bool CheckForUnknown(const AttributeTrail& trail, bool use_partial) const;

  // Checks whether trail corresponds to any patterns that identify
  // unknowns. Only matches exactly (exact attribute match for self or parent).
  bool CheckForUnknownExact(const AttributeTrail& trail) const {
    return CheckForUnknown(trail, false);
  }

  // Checks whether trail corresponds to any patterns that define unknowns.
  // Matches if a parent or any descendant (select or index of) the attribute.
  bool CheckForUnknownPartial(const AttributeTrail& trail) const {
    return CheckForUnknown(trail, true);
  }

  // Creates merged UnknownAttributeSet.
  // Scans over the args collection, determines if there matches to unknown
  // patterns and returns the (possibly empty) collection.
  cel::AttributeSet CheckForUnknowns(absl::Span<const AttributeTrail> args,
                                     bool use_partial) const;

  // Creates merged UnknownValue.
  // Scans over the args collection, merges any UnknownValues found.
  // Returns the merged UnknownValue or nullopt if not found.
  absl::optional<cel::UnknownValue> MergeUnknowns(
      absl::Span<const cel::Value> args) const;

  // Creates a merged UnknownValue from two unknown values.
  cel::UnknownValue MergeUnknownValues(const cel::UnknownValue& left,
                                       const cel::UnknownValue& right) const;

  // Creates merged UnknownValue.
  // Merges together UnknownValues found in the args
  // along with attributes from attr that match the configured unknown patterns
  // Returns returns the merged UnknownValue if available or nullopt.
  absl::optional<cel::UnknownValue> IdentifyAndMergeUnknowns(
      absl::Span<const cel::Value> args, absl::Span<const AttributeTrail> attrs,
      bool use_partial) const;

  // Create an initial UnknownSet from a single attribute.
  cel::UnknownValue CreateUnknownSet(cel::Attribute attr) const;

  // Factory function for missing attribute errors.
  absl::StatusOr<cel::ErrorValue> CreateMissingAttributeError(
      const cel::Attribute& attr) const;

  // Create an initial UnknownSet from a single missing function call.
  cel::UnknownValue CreateUnknownSet(
      const cel::FunctionDescriptor& fn_descriptor, int64_t expr_id,
      absl::Span<const cel::Value> args) const;

  Accumulator CreateAccumulator() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return Accumulator(*this);
  }

 private:
  cel::ValueManager& value_manager() const { return value_factory_; }

  // Workaround friend visibility.
  void Add(Accumulator& a, const cel::UnknownValue& v) const;
  void Add(Accumulator& a, const AttributeTrail& attr) const;

  absl::Span<const cel::AttributePattern> unknown_patterns_;
  absl::Span<const cel::AttributePattern> missing_attribute_patterns_;
  cel::ValueManager& value_factory_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
