// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLYFILLS_BASE_LOGGING_H_
#define POLYFILLS_BASE_LOGGING_H_

// The upstream header includes this, and some of the copied files actually rely
// on this.
#include <string.h>

class GurlFakeLogSink {
 public:
  template <typename T1>
  GurlFakeLogSink(T1) {}
  template <typename T1, typename T2>
  GurlFakeLogSink(T1, T2) {}

  template<typename T>
  GurlFakeLogSink& operator<<(const T&) { return *this; }
};

#define GURL_CHECK_GE(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_CHECK_LE(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_CHECK_LT(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_CHECK_NE(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_CHECK_EQ(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_CHECK(statement) GurlFakeLogSink({statement})
#define GURL_DCHECK_EQ(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_DCHECK_GE(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_DCHECK_GT(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_DCHECK_IS_ON() false
#define GURL_DCHECK_LE(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_DCHECK_LT(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_DCHECK_NE(statement, statement2) GurlFakeLogSink({statement, statement2})
#define GURL_DCHECK(statement) GurlFakeLogSink({statement})
#define GURL_DLOG(severity) GurlFakeLogSink(true)
#define GURL_LOG(severity) GurlFakeLogSink(true)
#define GURL_NOTREACHED()

#endif /* POLYFILLS_BASE_LOGGING_H_ */
