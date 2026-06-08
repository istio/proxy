// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_BUILDER_H_

#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/status.h"

namespace cel::internal {

class StatusBuilder;

template <typename Fn, typename Arg, typename Expected>
inline constexpr bool StatusBuilderResultMatches =
    std::is_same_v<std::decay_t<std::invoke_result_t<Fn, Arg>>, Expected>;

template <typename Adaptor, typename Builder>
using StatusBuilderPurePolicy = std::enable_if_t<
    StatusBuilderResultMatches<Adaptor, Builder, StatusBuilder>,
    std::invoke_result_t<Adaptor, Builder>>;

template <typename Adaptor, typename Builder>
using StatusBuilderSideEffect =
    std::enable_if_t<StatusBuilderResultMatches<Adaptor, Builder, absl::Status>,
                     std::invoke_result_t<Adaptor, Builder>>;

template <typename Adaptor, typename Builder>
using StatusBuilderConversion = std::enable_if_t<
    !StatusBuilderResultMatches<Adaptor, Builder, StatusBuilder> &&
        !StatusBuilderResultMatches<Adaptor, Builder, absl::Status>,
    std::invoke_result_t<Adaptor, Builder>>;

class StatusBuilder final {
 public:
  StatusBuilder() = default;

  explicit StatusBuilder(const absl::Status& status) : status_(status) {}

  StatusBuilder(const StatusBuilder&) = default;

  StatusBuilder(StatusBuilder&&) = default;

  ~StatusBuilder() = default;

  StatusBuilder& operator=(const StatusBuilder&) = default;

  StatusBuilder& operator=(StatusBuilder&&) = default;

  bool ok() const { return status_.ok(); }

  absl::StatusCode code() const { return status_.code(); }

  operator absl::Status() const& { return status_; }  // NOLINT

  operator absl::Status() && { return std::move(status_); }  // NOLINT

  template <typename Adaptor>
  auto With(
      Adaptor&& adaptor) & -> StatusBuilderPurePolicy<Adaptor, StatusBuilder&> {
    return std::forward<Adaptor>(adaptor)(*this);
  }
  template <typename Adaptor>
  ABSL_MUST_USE_RESULT auto With(
      Adaptor&&
          adaptor) && -> StatusBuilderPurePolicy<Adaptor, StatusBuilder&&> {
    return std::forward<Adaptor>(adaptor)(std::move(*this));
  }

  template <typename Adaptor>
  auto With(
      Adaptor&& adaptor) & -> StatusBuilderSideEffect<Adaptor, StatusBuilder&> {
    return std::forward<Adaptor>(adaptor)(*this);
  }
  template <typename Adaptor>
  ABSL_MUST_USE_RESULT auto With(
      Adaptor&&
          adaptor) && -> StatusBuilderSideEffect<Adaptor, StatusBuilder&&> {
    return std::forward<Adaptor>(adaptor)(std::move(*this));
  }

  template <typename Adaptor>
  auto With(
      Adaptor&& adaptor) & -> StatusBuilderConversion<Adaptor, StatusBuilder&> {
    return std::forward<Adaptor>(adaptor)(*this);
  }
  template <typename Adaptor>
  ABSL_MUST_USE_RESULT auto With(
      Adaptor&&
          adaptor) && -> StatusBuilderConversion<Adaptor, StatusBuilder&&> {
    return std::forward<Adaptor>(adaptor)(std::move(*this));
  }

 private:
  absl::Status status_;
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_STATUS_BUILDER_H_
