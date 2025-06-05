// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_DATA_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_DATA_INTERFACE_H_

#include <type_traits>

#include "absl/base/attributes.h"
#include "common/native_type.h"

namespace cel {

class TypeInterface;
class ValueInterface;

namespace common_internal {

class DataInterface;

// `DataInterface` is the abstract base class of `cel::ValueInterface` and
// `cel::TypeInterface`.
class DataInterface {
 public:
  DataInterface(const DataInterface&) = delete;
  DataInterface(DataInterface&&) = delete;

  virtual ~DataInterface() = default;

  DataInterface& operator=(const DataInterface&) = delete;
  DataInterface& operator=(DataInterface&&) = delete;

 protected:
  DataInterface() = default;

 private:
  friend class cel::TypeInterface;
  friend class cel::ValueInterface;
  friend struct NativeTypeTraits<DataInterface>;

  virtual NativeTypeId GetNativeTypeId() const = 0;
};

}  // namespace common_internal

template <>
struct NativeTypeTraits<common_internal::DataInterface> final {
  static NativeTypeId Id(const common_internal::DataInterface& data_interface) {
    return data_interface.GetNativeTypeId();
  }
};

template <typename T>
struct NativeTypeTraits<
    T, std::enable_if_t<std::conjunction_v<
           std::is_base_of<common_internal::DataInterface, T>,
           std::negation<std::is_same<T, common_internal::DataInterface>>>>>
    final {
  static NativeTypeId Id(const common_internal::DataInterface& data_interface) {
    return NativeTypeTraits<common_internal::DataInterface>::Id(data_interface);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_DATA_INTERFACE_H_
