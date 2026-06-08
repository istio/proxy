// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/dut_info.h"

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/data_model/struct_validators.h"

namespace ocpdiag::results {

namespace {

ABSL_CONST_INIT absl::Mutex singleton_mutex(absl::kConstInit);
bool singleton_initialized ABSL_GUARDED_BY(singleton_mutex) = false;

}  // namespace

DutInfo::DutInfo(absl::string_view name, absl::string_view id)
    : name_(name), id_(id) {
  CHECK(!name_.empty()) << "Must specify a name for the DutInfo";
  CHECK(!id_.empty()) << "Must specify an id for the DutInfo";

  absl::MutexLock lock(&singleton_mutex);
  CHECK(!singleton_initialized)
      << "Only one DutInfo instance can exist at a time";
  singleton_initialized = true;
}

RegisteredHardwareInfo DutInfo::AddHardwareInfo(
    const HardwareInfo& hardware_info) {
  ValidateStructOrDie(hardware_info);
  RegisteredHardwareInfo reference;
  reference.id_ = absl::StrCat(hardware_info_id_.Next());
  hardware_infos_.push_back({hardware_info, reference.id_});
  return reference;
}

RegisteredSoftwareInfo DutInfo::AddSoftwareInfo(
    const SoftwareInfo& software_info) {
  ValidateStructOrDie(software_info);
  RegisteredSoftwareInfo reference;
  reference.id_ = absl::StrCat(softare_info_id_.Next());
  software_infos_.push_back({software_info, reference.id_});
  return reference;
}

void DutInfo::AddPlatformInfo(const PlatformInfo& platform_info) {
  ValidateStructOrDie(platform_info);
  platform_infos_.push_back(platform_info);
}

DutInfo::~DutInfo() {
  absl::MutexLock lock(&singleton_mutex);
  singleton_initialized = false;
}

}  // namespace ocpdiag::results
