// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLYFILLS_THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_VALUE_H_
#define POLYFILLS_THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_VALUE_H_

namespace perfetto {

class TracedValue {
 public:
  void WriteString(const std::string&) && {}
};

template <typename T>
void WriteIntoTracedValue(TracedValue /*context*/, T&& /*value*/) {}

template <typename T, typename ResultType = void, class = void>
struct check_traced_value_support {
  static constexpr bool value = true;
  using type = ResultType;
};

}  // namespace perfetto

#endif  // POLYFILLS_THIRD_PARTY_PERFETTO_INCLUDE_PERFETTO_TRACING_TRACED_VALUE_H_
