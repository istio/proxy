// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLYFILLS_BASE_TRACE_EVENT_MEMORY_USAGE_ESTIMATOR_H_
#define POLYFILLS_BASE_TRACE_EVENT_MEMORY_USAGE_ESTIMATOR_H_

namespace gurl_base {
namespace trace_event {

template <class T>
size_t EstimateMemoryUsage(const T& object) { return 0; }

}  // namespace trace_event
}  // namespace base

#endif  /* POLYFILLS_BASE_TRACE_EVENT_MEMORY_USAGE_ESTIMATOR_H_ */
