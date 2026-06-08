// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include <cstdlib>

#include "absl/flags/parse.h"
#include "ocpdiag/core/examples/full_spec/full_spec.h"

// Main entrypoint for the full spec generation example.
int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  full_spec::FullSpec().ExecuteTest();
  return EXIT_SUCCESS;
}
