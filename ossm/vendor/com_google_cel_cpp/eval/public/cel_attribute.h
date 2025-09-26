#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_ATTRIBUTE_PATTERN_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_ATTRIBUTE_PATTERN_H_

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// CelAttributeQualifier represents a segment in
// attribute resolutuion path. A segment can be qualified by values of
// following types: string/int64_t/uint64_t/bool.
using CelAttributeQualifier = ::cel::AttributeQualifier;

// CelAttribute represents resolved attribute path.
using CelAttribute = ::cel::Attribute;

// CelAttributeQualifierPattern matches a segment in
// attribute resolutuion path. CelAttributeQualifierPattern is capable of
// matching path elements of types string/int64_t/uint64_t/bool.
using CelAttributeQualifierPattern = ::cel::AttributeQualifierPattern;

// CelAttributePattern is a fully-qualified absolute attribute path pattern.
// Supported segments steps in the path are:
// - field selection;
// - map lookup by key;
// - list access by index.
using CelAttributePattern = ::cel::AttributePattern;

CelAttributeQualifierPattern CreateCelAttributeQualifierPattern(
    const CelValue& value);

CelAttributeQualifier CreateCelAttributeQualifier(const CelValue& value);

// Short-hand helper for creating |CelAttributePattern|s. string_view arguments
// must outlive the returned pattern.
CelAttributePattern CreateCelAttributePattern(
    absl::string_view variable,
    std::initializer_list<absl::variant<absl::string_view, int64_t, uint64_t,
                                        bool, CelAttributeQualifierPattern>>
        path_spec = {});

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CEL_ATTRIBUTE_PATTERN_H_
