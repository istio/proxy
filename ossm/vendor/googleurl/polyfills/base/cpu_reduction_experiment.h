// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLYFILLS_BASE_CPU_REDUCTION_EXPERIMENT_H_
#define POLYFILLS_BASE_CPU_REDUCTION_EXPERIMENT_H_

namespace base {

inline bool IsRunningCpuReductionExperiment() { return false; }

class CpuReductionExperimentFilter {
 public:
  bool ShouldLogHistograms() { return false; }
};

}  // namespace base


#endif  /* POLYFILLS_BASE_CPU_REDUCTION_EXPERIMENT_H_ */
