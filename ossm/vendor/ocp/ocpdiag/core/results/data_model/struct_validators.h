// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

// Functions that ensure that the structs that contain the results output data
// are valid according to the OCP spec.
#ifndef OCPDIAG_CORE_RESULTS_OCP_STRUCT_VALIDATORS_H_
#define OCPDIAG_CORE_RESULTS_OCP_STRUCT_VALIDATORS_H_

#include "ocpdiag/core/results/data_model/input_model.h"

namespace ocpdiag::results {

void ValidateStructOrDie(const Validator& validator);
void ValidateStructOrDie(const HardwareInfo& hardware_info);
void ValidateStructOrDie(const SoftwareInfo& software_info);
void ValidateStructOrDie(const PlatformInfo& platform_info);
void ValidateStructOrDie(
    const MeasurementSeriesStart& measurement_series_start);
void ValidateStructOrDie(const Measurement& measurement);
void ValidateStructOrDie(const Diagnosis& diagnosis);
void ValidateStructOrDie(const Error& error);
void ValidateStructOrDie(const Log& log);
void ValidateStructOrDie(const File& file);
void ValidateStructOrDie(const TestRunStart& test_run_info);
void ValidateStructOrDie(const Extension& extension);

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OCP_STRUCT_VALIDATORS_H_
