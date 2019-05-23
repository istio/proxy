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

#include "opencensus/stats/internal/measure_registry_impl.h"

#include "opencensus/stats/internal/delta_producer.h"
#include "opencensus/stats/internal/stats_manager.h"
#include "opencensus/stats/measure_descriptor.h"

namespace opencensus {
namespace stats {

namespace {

// Constants for constructing/deconstructing ids.
constexpr uint64_t kIndexMask = 0x3FFFFFFFFFFFFFFFLL;
constexpr uint64_t kValid = 0x8000000000000000ull;
constexpr uint64_t kInvalid = 0x0000000000000000ull;
constexpr uint64_t kTypeMask = 0x4000000000000000ull;
constexpr uint64_t kDoubleType = 0x0000000000000000ull;
constexpr uint64_t kIntType = 0x4000000000000000ull;

}  // namespace

// static
MeasureRegistryImpl* MeasureRegistryImpl::Get() {
  static MeasureRegistryImpl* global_measure_registry_impl =
      new MeasureRegistryImpl();
  return global_measure_registry_impl;
}

template <>
MeasureDouble MeasureRegistryImpl::Register(absl::string_view name,
                                            absl::string_view description,
                                            absl::string_view units) {
  MeasureDouble measure(RegisterImpl(MeasureDescriptor(
      name, description, units, MeasureDescriptor::Type::kDouble)));
  if (measure.IsValid()) {
    StatsManager::Get()->AddMeasure(measure);
    DeltaProducer::Get()->AddMeasure();
  }
  return measure;
}

template <>
MeasureInt64 MeasureRegistryImpl::Register(absl::string_view name,
                                           absl::string_view description,
                                           absl::string_view units) {
  MeasureInt64 measure(RegisterImpl(MeasureDescriptor(
      name, description, units, MeasureDescriptor::Type::kInt64)));
  if (measure.IsValid()) {
    StatsManager::Get()->AddMeasure(measure);
    DeltaProducer::Get()->AddMeasure();
  }
  return measure;
}

uint64_t MeasureRegistryImpl::RegisterImpl(MeasureDescriptor descriptor) {
  if (descriptor.name().empty()) {
    return CreateMeasureId(0, false, descriptor.type());
  }
  const auto it = id_map_.find(descriptor.name());
  if (it != id_map_.end()) {
    return CreateMeasureId(0, false, descriptor.type());
  }
  const uint64_t id =
      CreateMeasureId(registered_descriptors_.size(), true, descriptor.type());
  id_map_.emplace_hint(it, descriptor.name(), id);
  registered_descriptors_.push_back(std::move(descriptor));
  return id;
}

const MeasureDescriptor& MeasureRegistryImpl::GetDescriptorByName(
    absl::string_view name) const {
  const auto it = id_map_.find(std::string(name));
  if (it == id_map_.end()) {
    static const MeasureDescriptor default_descriptor =
        MeasureDescriptor("", "", "", MeasureDescriptor::Type::kDouble);
    return default_descriptor;
  } else {
    return registered_descriptors_[IdToIndex(it->second)];
  }
}

MeasureDouble MeasureRegistryImpl::GetMeasureDoubleByName(
    absl::string_view name) const {
  const auto it = id_map_.find(std::string(name));
  if (it == id_map_.end()) {
    return MeasureDouble(
        CreateMeasureId(0, false, MeasureDescriptor::Type::kDouble));
  } else {
    return MeasureDouble(it->second);
  }
}

MeasureInt64 MeasureRegistryImpl::GetMeasureInt64ByName(
    absl::string_view name) const {
  const auto it = id_map_.find(std::string(name));
  if (it == id_map_.end()) {
    return MeasureInt64(
        CreateMeasureId(0, false, MeasureDescriptor::Type::kDouble));
  } else {
    return MeasureInt64(it->second);
  }
}

uint64_t MeasureRegistryImpl::GetIdByName(absl::string_view name) const {
  const auto it = id_map_.find(std::string(name));
  if (it == id_map_.end()) {
    return CreateMeasureId(0, false, MeasureDescriptor::Type::kDouble);
  } else {
    return it->second;
  }
}

// static
bool MeasureRegistryImpl::IdValid(uint64_t id) { return id & kValid; }

// static
uint64_t MeasureRegistryImpl::IdToIndex(uint64_t id) { return id & kIndexMask; }

// static
MeasureDescriptor::Type MeasureRegistryImpl::IdToType(uint64_t id) {
  if ((id & kTypeMask) == kDoubleType) {
    return MeasureDescriptor::Type::kDouble;
  } else {
    return MeasureDescriptor::Type::kInt64;
  }
}

// static
uint64_t MeasureRegistryImpl::CreateMeasureId(uint64_t index, bool is_valid,
                                              MeasureDescriptor::Type type) {
  return index | (is_valid ? kValid : kInvalid) |
         (type == MeasureDescriptor::Type::kDouble ? kDoubleType : kIntType);
}

}  // namespace stats
}  // namespace opencensus
