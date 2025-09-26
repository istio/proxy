#include "eval/eval/attribute_trail.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/attribute.h"

namespace google::api::expr::runtime {

// Creates AttributeTrail with attribute path incremented by "qualifier".
AttributeTrail AttributeTrail::Step(cel::AttributeQualifier qualifier) const {
  // Cannot continue void trail
  if (empty()) return AttributeTrail();

  std::vector<cel::AttributeQualifier> qualifiers;
  qualifiers.reserve(attribute_->qualifier_path().size() + 1);
  std::copy_n(attribute_->qualifier_path().begin(),
              attribute_->qualifier_path().size(),
              std::back_inserter(qualifiers));
  qualifiers.push_back(std::move(qualifier));
  return AttributeTrail(cel::Attribute(std::string(attribute_->variable_name()),
                                       std::move(qualifiers)));
}

}  // namespace google::api::expr::runtime
