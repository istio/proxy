// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_CODE_GENERATOR_H_
#define V8_MAGLEV_MAGLEV_CODE_GENERATOR_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {
namespace maglev {

class Graph;
class MaglevCompilationInfo;

class MaglevCodeGenerator : public AllStatic {
 public:
  static MaybeHandle<Code> Generate(MaglevCompilationInfo* compilation_info,
                                    Graph* graph);
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_CODE_GENERATOR_H_
