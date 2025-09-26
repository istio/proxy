// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_LIB_RESULTS_INTERNAL_INT_INCREMENTER_H_
#define OCPDIAG_LIB_RESULTS_INTERNAL_INT_INCREMENTER_H_

#include "absl/synchronization/mutex.h"

namespace ocpdiag::results::internal {

// Threadsafe class that generates monotonically increasing integers,
// starting from zero. Values are not globally unique, but are unique amongst
// all users of a shared instance.
class IntIncrementer {
 public:
  IntIncrementer() : count_(0) {}

  // Returns then increments count
  int Next() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock l(&mutex_);
    return count_++;
  }

  // This class shall not allow reading the value of count_ without also
  // incrementing it.

 private:
  absl::Mutex mutex_;
  int count_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace ocpdiag::results::internal

#endif  // OCPDIAG_LIB_RESULTS_INTERNAL_INT_INCREMENTER_H_
