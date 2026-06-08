// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_DATA_MODEL_INPUT_MODEL_HELPERS_H_
#define OCPDIAG_CORE_RESULTS_OCP_DATA_MODEL_INPUT_MODEL_HELPERS_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "ocpdiag/core/results/data_model/input_model.h"

namespace ocpdiag::results {

// This files contains helper functions to help create the inputs to the results
// lib.

// Creates a string with the commandline invocation for the test from the main
// function arguments.
std::string CommandLineStringFromMainArgs(int argc, const char* argv[]);

// Creates a JSON string contaning the commandline arguments passed to the test
// as key value pairs from the main function arguments.
std::string ParameterJsonFromMainArgs(int argc, const char* argv[]);

// Creates a vector of validators requiring that the associated measurement(s)
// be within the specified inclusive limits. If a name is specified, the two
// validators returned will have "Lower" and "Upper" appended to their names,
// respectively. This will cause the test to die if lower limit is larger than
// the upper limit.
std::vector<Validator> ValidateWithinInclusiveLimits(
    double lower_limit, double upper_limit, absl::string_view name = "");

// Creates a vector of validators requiring that the associated measurement(s)
// be within the specified exclusive limits. If a name is specified, the two
// validators returned will have "Lower" and "Upper" appended to their names,
// respectively. This will cause the test to die if lower limit is larger than
// the upper limit.
std::vector<Validator> ValidateWithinExclusiveLimits(
    double lower_limit, double upper_limit, absl::string_view name = "");

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OCP_DATA_MODEL_INPUT_MODEL_HELPERS_H_
