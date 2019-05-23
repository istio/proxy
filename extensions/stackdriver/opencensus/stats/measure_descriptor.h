// Copyright 2017, OpenCensus Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENCENSUS_STATS_MEASURE_DESCRIPTOR_H_
#define OPENCENSUS_STATS_MEASURE_DESCRIPTOR_H_

#include <string>

#include "absl/strings/string_view.h"

namespace opencensus {
namespace stats {

// MeasureDescriptor is system-stable metadata (or a subset of it) for a
// Measure.
// MeasureDescriptor is immutable.
class MeasureDescriptor final {
 public:
  // The type of values recorded under this Measure.
  enum class Type {
    kDouble,
    kInt64,
  };

  // See documentation on MeasureRegistry::Register*() for details of these
  // fields.
  const std::string& name() const { return name_; }
  const std::string& description() const { return description_; }
  const std::string& units() const { return units_; }
  Type type() const { return type_; }

  std::string DebugString() const;

  bool operator==(const MeasureDescriptor& other) const {
    return name_ == other.name_ && description_ == other.description_ &&
           units_ == other.units_ && type_ == other.type_;
  }
  bool operator!=(const MeasureDescriptor& other) const {
    return !(*this == other);
  }

 private:
  // Only MeasureRegistryImpl can construct this--users should call the
  // MeasureRegistry::Register*() functions.
  friend class MeasureRegistryImpl;
  MeasureDescriptor(absl::string_view name, absl::string_view description,
                    absl::string_view units, Type type)
      : name_(name), description_(description), units_(units), type_(type) {}

  const std::string name_;
  const std::string description_;
  const std::string units_;
  const Type type_;
};

}  // namespace stats
}  // namespace opencensus

#endif  // OPENCENSUS_STATS_MEASURE_DESCRIPTOR_H_
