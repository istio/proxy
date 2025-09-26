#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_

#include <cstdint>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "base/attribute_set.h"
#include "base/function_result_set.h"
#include "common/function_descriptor.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "runtime/internal/attribute_matcher.h"

namespace google::api::expr::runtime {

// Default implementation of the attribute matcher.
// Scans the attribute trail against a list of unknown or missing patterns.
class DefaultAttributeMatcher : public cel::runtime_internal::AttributeMatcher {
 private:
  using MatchResult = cel::runtime_internal::AttributeMatcher::MatchResult;

 public:
  DefaultAttributeMatcher(
      absl::Span<const cel::AttributePattern> unknown_patterns,
      absl::Span<const cel::AttributePattern> missing_patterns);

  DefaultAttributeMatcher();

  MatchResult CheckForUnknown(const cel::Attribute& attr) const override;
  MatchResult CheckForMissing(const cel::Attribute& attr) const override;

 private:
  absl::Span<const cel::AttributePattern> unknown_patterns_;
  absl::Span<const cel::AttributePattern> missing_patterns_;
};

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

    // Add to the accumulated set of unknowns if value is UnknownValue or
    // the attribute trail is (partially) unknown. This version prefers
    // preserving an already present unknown value over a new one matching the
    // attribute trail.
    //
    // Uses partial matching (a pattern matches the attribute or any
    // sub-attribute).
    void MaybeAdd(const cel::Value& v, const AttributeTrail& attr);

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

  AttributeUtility(absl::Span<const cel::AttributePattern> unknown_patterns,
                   absl::Span<const cel::AttributePattern> missing_patterns)
      : default_matcher_(unknown_patterns, missing_patterns),
        matcher_(&default_matcher_) {}

  explicit AttributeUtility(
      const cel::runtime_internal::AttributeMatcher* absl_nonnull matcher)
      : matcher_(matcher) {}

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

  void set_matcher(
      const cel::runtime_internal::AttributeMatcher* absl_nonnull matcher) {
    matcher_ = matcher;
  }

 private:
  // Workaround friend visibility.
  void Add(Accumulator& a, const cel::UnknownValue& v) const;
  void Add(Accumulator& a, const AttributeTrail& attr) const;

  DefaultAttributeMatcher default_matcher_;
  const cel::runtime_internal::AttributeMatcher* absl_nonnull matcher_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_UNKNOWNS_UTILITY_H_
