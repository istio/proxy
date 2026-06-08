#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BASE_ACTIVATION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BASE_ACTIVATION_H_

#include <vector>

#include "google/protobuf/field_mask.pb.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"
#include "runtime/internal/attribute_matcher.h"

namespace cel::runtime_internal {
class ActivationAttributeMatcherAccess;
}

namespace google::api::expr::runtime {

// Base class for an activation.
class BaseActivation {
 public:
  BaseActivation() = default;

  // Non-copyable/non-assignable
  BaseActivation(const BaseActivation&) = delete;
  BaseActivation& operator=(const BaseActivation&) = delete;

  // Move-constructible/move-assignable
  BaseActivation(BaseActivation&& other) = default;
  BaseActivation& operator=(BaseActivation&& other) = default;

  // Return a list of function overloads for the given name.
  virtual std::vector<const CelFunction*> FindFunctionOverloads(
      absl::string_view) const = 0;

  // Provide the value that is bound to the name, if found.
  // arena parameter is provided to support the case when we want to pass the
  // ownership of returned object ( Message/List/Map ) to Evaluator.
  virtual absl::optional<CelValue> FindValue(absl::string_view,
                                             google::protobuf::Arena*) const = 0;

  // Return the collection of attribute patterns that determine missing
  // attributes.
  virtual const std::vector<CelAttributePattern>& missing_attribute_patterns()
      const {
    static const std::vector<CelAttributePattern>* empty =
        new std::vector<CelAttributePattern>({});
    return *empty;
  }

  // Return the collection of attribute patterns that determine "unknown"
  // values.
  virtual const std::vector<CelAttributePattern>& unknown_attribute_patterns()
      const {
    static const std::vector<CelAttributePattern>* empty =
        new std::vector<CelAttributePattern>({});
    return *empty;
  }

  virtual ~BaseActivation() = default;

 private:
  friend class cel::runtime_internal::ActivationAttributeMatcherAccess;

  // Internal getter for overriding the attribute matching behavior.
  virtual const cel::runtime_internal::AttributeMatcher* absl_nullable
  GetAttributeMatcher() const {
    return nullptr;
  }
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_BASE_ACTIVATION_H_
