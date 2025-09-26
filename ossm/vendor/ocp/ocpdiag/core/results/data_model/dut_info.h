// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_DUT_INFO_H_
#define OCPDIAG_CORE_RESULTS_OCP_DUT_INFO_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/int_incrementer.h"

namespace ocpdiag::results {

// Singleton class that contains informantion about the device under test and
// that provides unique references to hardware and software info for future use
// in measurements, diagnoses, and errors.
class DutInfo {
 public:
  DutInfo(absl::string_view name, absl::string_view id);
  ~DutInfo();

  std::string name() const { return name_; }

  std::string id() const { return id_; }

  [[nodiscard]] RegisteredHardwareInfo AddHardwareInfo(
      const HardwareInfo& hardware_info);
  const std::vector<HardwareInfoOutput>& GetHardwareInfos()
      const {
    return hardware_infos_;
  }

  [[nodiscard]] RegisteredSoftwareInfo AddSoftwareInfo(
      const SoftwareInfo& software_info);
  const std::vector<SoftwareInfoOutput>& GetSoftwareInfos()
      const {
    return software_infos_;
  }

  void AddPlatformInfo(const PlatformInfo& platform_info);
  const std::vector<PlatformInfo>& GetPlatformInfos() const {
    return platform_infos_;
  }

  void SetMetadataJson(absl::string_view json) { metadata_json_ = json; }
  std::string GetMetadataJson() const { return metadata_json_; }

 private:
  std::string name_;
  std::string id_;
  std::string metadata_json_;
  std::vector<HardwareInfoOutput> hardware_infos_;
  std::vector<SoftwareInfoOutput> software_infos_;
  std::vector<PlatformInfo> platform_infos_;
  internal::IntIncrementer hardware_info_id_;
  internal::IntIncrementer softare_info_id_;
};

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OCP_DUT_INFO_H_
