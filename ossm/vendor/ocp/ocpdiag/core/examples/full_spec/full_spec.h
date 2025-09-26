// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_EXAMPLES_OCP_FULL_SPEC_FULL_SPEC_H_
#define OCPDIAG_CORE_EXAMPLES_OCP_FULL_SPEC_FULL_SPEC_H_

#include <memory>
#include <vector>

#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/dut_info.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/test_run.h"

namespace full_spec {

// Class that outputs the full OCPDiag results spec, using the examples in the
// spec document. This is not intended to be an example of a working diagnostic,
// but instead to give a full example JSON output and to exercise all the
// features of the results library.
class FullSpec {
 public:
  FullSpec(std::unique_ptr<ocpdiag::results::internal::ArtifactWriter> writer =
               nullptr);

  // Executes the test. Real tests would usually return a status here, but this
  // test cannot fail since it is not outputting real information.
  void ExecuteTest();

 private:
  ocpdiag::results::TestRun run_;
  std::vector<ocpdiag::results::RegisteredHardwareInfo> hw_infos_;
  std::vector<ocpdiag::results::RegisteredSoftwareInfo> sw_infos_;

  void AddPreStartArtifacts();
  std::unique_ptr<ocpdiag::results::DutInfo> CreateDutInfo();
  void AddBasicMeasurementAndDiagnosisStep();
  void AddOtherStepArtifactsStep();
  void AddSkippedStep();
  void AddMeasurementSeriesStep();
};

}  // namespace full_spec

#endif  // OCPDIAG_CORE_EXAMPLES_OCP_FULL_SPEC_FULL_SPEC_H_
