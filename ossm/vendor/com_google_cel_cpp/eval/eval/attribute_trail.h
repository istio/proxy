#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_ATTRIBUTE_TRAIL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_ATTRIBUTE_TRAIL_H_

#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "absl/utility/utility.h"
#include "base/attribute.h"

namespace google::api::expr::runtime {

// AttributeTrail reflects current attribute path.
// It is functionally similar to cel::Attribute, yet intended to have better
// complexity on attribute path increment operations.
// TODO Current AttributeTrail implementation is equivalent to
// cel::Attribute - improve it.
// Intended to be used in conjunction with cel::Value, describing the attribute
// value originated from.
// Empty AttributeTrail denotes object with attribute path not defined
// or supported.
class AttributeTrail {
 public:
  AttributeTrail() : attribute_(absl::nullopt) {}

  explicit AttributeTrail(std::string variable_name)
      : attribute_(absl::in_place, std::move(variable_name)) {}

  explicit AttributeTrail(cel::Attribute attribute)
      : attribute_(std::move(attribute)) {}

  AttributeTrail(const AttributeTrail&) = default;
  AttributeTrail& operator=(const AttributeTrail&) = default;
  AttributeTrail(AttributeTrail&&) = default;
  AttributeTrail& operator=(AttributeTrail&&) = default;

  // Creates AttributeTrail with attribute path incremented by "qualifier".
  AttributeTrail Step(cel::AttributeQualifier qualifier) const;

  // Creates AttributeTrail with attribute path incremented by "qualifier".
  AttributeTrail Step(const std::string* qualifier) const {
    return Step(cel::AttributeQualifier::OfString(*qualifier));
  }

  // Returns CelAttribute that corresponds to content of AttributeTrail.
  const cel::Attribute& attribute() const { return attribute_.value(); }

  bool empty() const { return !attribute_.has_value(); }

 private:
  absl::optional<cel::Attribute> attribute_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_ATTRIBUTE_TRAIL_H_
