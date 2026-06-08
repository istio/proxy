// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_STRUCTS_H_
#define OCPDIAG_CORE_RESULTS_OCP_STRUCTS_H_

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "google/protobuf/util/time_util.h"  // Included to properly import the timeval struct
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/variant.h"

namespace ocpdiag::results {

// The structs in this file are output only and will not need to be filled out
// by the user. Note that in many cases these are distinct from the input types
// because they contain additional info that is provided by the results library
// itself.
enum class TestResult {
  kNotApplicable = 0,
  kPass = 1,
  kFail = 2,
};

enum class TestStatus { kUnknown = 0, kComplete = 1, kError = 2, kSkip = 3 };

// Alias types that are the same for input and ouput for clarity
typedef struct Subcomponent SubcomponentOutput;
typedef struct Validator ValidatorOutput;
typedef struct Log LogOutput;
typedef struct File FileOutput;
typedef struct Extension ExtensionOutput;
typedef struct PlatformInfo PlatformInfoOutput;

struct MeasurementSeriesStartOutput {
  std::string measurement_series_id;
  std::string name;
  std::string unit;
  std::string hardware_info_id;
  std::optional<SubcomponentOutput> subcomponent;
  std::vector<ValidatorOutput> validators;
  std::string metadata_json;

  auto tie() const {
    return std::tie(measurement_series_id, name, unit, hardware_info_id,
                    subcomponent, validators, metadata_json);
  }

  bool operator==(const MeasurementSeriesStartOutput& rhs) const {
    return tie() == rhs.tie();
  }
};

struct MeasurementSeriesEndOutput {
  std::string measurement_series_id;
  int total_count;

  auto tie() const { return std::tie(measurement_series_id, total_count); }

  bool operator==(const MeasurementSeriesEndOutput& rhs) const {
    return tie() == rhs.tie();
  }
};

struct MeasurementSeriesElementOutput {
  int index;
  std::string measurement_series_id;
  Variant value;
  timeval timestamp;
  std::string metadata_json;

  bool operator==(const MeasurementSeriesElementOutput& rhs) const {
    return index == rhs.index &&
           measurement_series_id == rhs.measurement_series_id &&
           value == rhs.value && timestamp.tv_sec == rhs.timestamp.tv_sec &&
           timestamp.tv_usec == rhs.timestamp.tv_usec &&
           metadata_json == rhs.metadata_json;
  }
};

struct MeasurementOutput {
  std::string name;
  std::string unit;
  std::string hardware_info_id;
  std::optional<SubcomponentOutput> subcomponent;
  std::vector<ValidatorOutput> validators;
  Variant value;
  std::string metadata_json;

  auto tie() const {
    return std::tie(name, unit, hardware_info_id, subcomponent, validators,
                    value, metadata_json);
  }

  bool operator==(const MeasurementOutput& rhs) const {
    return tie() == rhs.tie();
  }
};

struct TestStepStartOutput {
  std::string name;

  bool operator==(const TestStepStartOutput& rhs) const {
    return name == rhs.name;
  }
};

struct TestStepEndOutput {
  TestStatus status;

  bool operator==(const TestStepEndOutput& rhs) const {
    return status == rhs.status;
  }
};

struct DiagnosisOutput {
  std::string verdict;
  DiagnosisType type;
  std::string message;
  std::string hardware_info_id;
  std::optional<SubcomponentOutput> subcomponent;

  auto tie() const {
    return std::tie(verdict, type, message, hardware_info_id, subcomponent);
  }

  bool operator==(const DiagnosisOutput& rhs) const {
    return tie() == rhs.tie();
  }
};

struct ErrorOutput {
  std::string symptom;
  std::string message;
  std::vector<std::string> software_info_ids;

  auto tie() const { return std::tie(symptom, message, software_info_ids); }

  bool operator==(const ErrorOutput& rhs) const { return tie() == rhs.tie(); }
};

struct HardwareInfoOutput : HardwareInfo {
  std::string hardware_info_id;

  HardwareInfoOutput(const HardwareInfo& hw_info, absl::string_view id)
      : HardwareInfo(hw_info), hardware_info_id(id) {}

  auto tie() const {
    return std::tie(hardware_info_id, name, computer_system, location, odata_id,
                    part_number, serial_number, manager, manufacturer,
                    manufacturer_part_number, part_type, version, revision);
  }

  bool operator==(const HardwareInfoOutput& rhs) const {
    return tie() == rhs.tie();
  }
};

struct SoftwareInfoOutput : SoftwareInfo {
  std::string software_info_id;

  SoftwareInfoOutput(const SoftwareInfo& sw_info, absl::string_view id)
      : SoftwareInfo(sw_info), software_info_id(id) {}

  auto tie() const {
    return std::tie(software_info_id, name, computer_system, version, revision,
                    software_type);
  }

  bool operator==(const SoftwareInfoOutput& rhs) const {
    return tie() == rhs.tie();
  }
};

struct DutInfoOutput {
  std::string dut_info_id;
  std::string name;
  std::string metadata_json;
  std::vector<PlatformInfoOutput> platform_infos;
  std::vector<HardwareInfoOutput> hardware_infos;
  std::vector<SoftwareInfoOutput> software_infos;

  auto tie() const {
    return std::tie(dut_info_id, name, metadata_json, platform_infos,
                    hardware_infos, software_infos);
  }

  bool operator==(const DutInfoOutput& rhs) const { return tie() == rhs.tie(); }
};

struct TestRunStartOutput : TestRunStart {
  DutInfoOutput dut_info;

  TestRunStartOutput() = default;

  TestRunStartOutput(const TestRunStart& test_run_start,
                     const DutInfoOutput& dut_info)
      : TestRunStart(test_run_start), dut_info(dut_info) {}

  auto tie() const {
    return std::tie(name, version, command_line, parameters_json, dut_info,
                    metadata_json);
  }

  bool operator==(const TestRunStartOutput& rhs) const {
    return tie() == rhs.tie();
  }
};

struct TestRunEndOutput {
  TestStatus status;
  TestResult result;

  auto tie() const { return std::tie(status, result); }

  bool operator==(const TestRunEndOutput& rhs) const {
    return tie() == rhs.tie();
  }
};

struct SchemaVersionOutput {
  int major;
  int minor;

  auto tie() const { return std::tie(major, minor); }

  bool operator==(const SchemaVersionOutput& rhs) const {
    return tie() == rhs.tie();
  }

  bool operator!=(const SchemaVersionOutput& rhs) const {
    return !(*this == rhs);
  }
};

// These types represent the structured output of the entire test, organized
// into logical groupings.
struct MeasurementSeriesModel {
  MeasurementSeriesStartOutput start;
  MeasurementSeriesEndOutput end;
  std::vector<MeasurementSeriesElementOutput> elements;
};

struct TestStepModel {
  std::string test_step_id;
  TestStepStartOutput start;
  TestStepEndOutput end;
  std::vector<LogOutput> logs;
  std::vector<ErrorOutput> errors;
  std::vector<FileOutput> files;
  std::vector<ExtensionOutput> extensions;
  std::vector<MeasurementSeriesModel> measurement_series;
  std::vector<MeasurementOutput> measurements;
  std::vector<DiagnosisOutput> diagnoses;
};

struct TestRunModel {
  TestRunStartOutput start;
  TestRunEndOutput end;
  std::vector<LogOutput> pre_start_logs;
  std::vector<ErrorOutput> pre_start_errors;
};

struct OutputModel {
  SchemaVersionOutput schema_version;
  TestRunModel test_run;
  std::vector<TestStepModel> test_steps;
};

// These types represent the output of the test artifact by artifact, as it is
// produced.
typedef std::variant<TestStepStartOutput, TestStepEndOutput, MeasurementOutput,
                     MeasurementSeriesStartOutput, MeasurementSeriesEndOutput,
                     MeasurementSeriesElementOutput, DiagnosisOutput,
                     ErrorOutput, FileOutput, LogOutput, ExtensionOutput>
    TestStepVariant;

struct TestStepArtifact {
  TestStepVariant artifact;
  std::string test_step_id;

  auto tie() const { return std::tie(artifact, test_step_id); }

  bool operator==(const TestStepArtifact& rhs) const {
    return tie() == rhs.tie();
  }
};

typedef std::variant<TestRunStartOutput, TestRunEndOutput, LogOutput,
                     ErrorOutput>
    TestRunVariant;

struct TestRunArtifact {
  TestRunVariant artifact;

  bool operator==(const TestRunArtifact& rhs) const {
    return artifact == rhs.artifact;
  }
};

typedef std::variant<SchemaVersionOutput, TestRunArtifact, TestStepArtifact>
    OutputVariant;

struct OutputArtifact {
  OutputVariant artifact;
  int sequence_number;
  timeval timestamp;

  bool operator==(const OutputArtifact& rhs) const {
    return artifact == rhs.artifact && sequence_number == rhs.sequence_number &&
           timestamp.tv_sec == rhs.timestamp.tv_sec &&
           timestamp.tv_usec == rhs.timestamp.tv_usec;
  }
};

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OCP_STRUCTS_H_
