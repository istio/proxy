// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/pointer-authentication.h"

namespace v8 {
namespace internal {

const int Deoptimizer::kEagerDeoptExitSize = kInstrSize;
#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
const int Deoptimizer::kLazyDeoptExitSize = 2 * kInstrSize;
#else
const int Deoptimizer::kLazyDeoptExitSize = 1 * kInstrSize;
#endif

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  return Float32::FromBits(
      static_cast<uint32_t>(double_registers_[n].get_bits()));
}

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  Address new_context =
      static_cast<Address>(GetTop()) + offset + kPCOnStackSize;
  value = PointerAuthentication::SignAndCheckPC(isolate_, value, new_context);
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No embedded constant pool support.
  UNREACHABLE();
}

void FrameDescription::SetPc(intptr_t pc) {
  // TODO(v8:10026): We need to sign pointers to the embedded blob, which are
  // stored in the isolate and code range objects.
  if (ENABLE_CONTROL_FLOW_INTEGRITY_BOOL) {
    CHECK(Deoptimizer::IsValidReturnAddress(PointerAuthentication::StripPAC(pc),
                                            isolate_));
  }
  pc_ = pc;
}

}  // namespace internal
}  // namespace v8
