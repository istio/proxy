// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_VARIANT_H_
#define OCPDIAG_CORE_RESULTS_OCP_VARIANT_H_

#include <string>
#include <variant>

#include "absl/strings/string_view.h"

namespace ocpdiag::results {

// This class exists as a solution to an issue in the std::variant class in
// C++17.
//
class Variant : public std::variant<std::string, double, bool> {
 public:
  Variant(const char* value)
      : std::variant<std::string, double, bool>(std::string(value)) {}
  Variant(absl::string_view value)
      : std::variant<std::string, double, bool>(std::string(value)) {}
  Variant(bool value) : std::variant<std::string, double, bool>(value) {}
  Variant(double value) : std::variant<std::string, double, bool>(value) {}
};

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OCP_VARIANT_H_
