// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/trap-handler/trap-handler-simulator.h"

#include <cstdint>

#include "include/v8-initialization.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/execution/simulator.h"
#include "src/trap-handler/trap-handler.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"

#if !V8_HOST_ARCH_X64 || !V8_TARGET_ARCH_ARM64
#error "Only include this file on arm64 simulator builds on x64."
#endif

namespace v8 {
namespace internal {
namespace trap_handler {

constexpr uintptr_t kFakePc = 11;

class SimulatorTrapHandlerTest : public TestWithIsolate {
 public:
  ~SimulatorTrapHandlerTest() {
    if (inaccessible_memory_) {
      auto* page_allocator = GetPlatformPageAllocator();
      CHECK(page_allocator->FreePages(inaccessible_memory_,
                                      page_allocator->AllocatePageSize()));
    }
  }

  void SetThreadInWasm() {
    EXPECT_EQ(0, *thread_in_wasm);
    *thread_in_wasm = 1;
  }

  void ResetThreadInWasm() {
    EXPECT_EQ(1, *thread_in_wasm);
    *thread_in_wasm = 0;
  }

  uintptr_t InaccessibleMemoryPtr() {
    if (!inaccessible_memory_) {
      auto* page_allocator = GetPlatformPageAllocator();
      size_t page_size = page_allocator->AllocatePageSize();
      inaccessible_memory_ =
          reinterpret_cast<uint8_t*>(page_allocator->AllocatePages(
              nullptr, /* size */ page_size, /* align */ page_size,
              PageAllocator::kNoAccess));
      CHECK_NOT_NULL(inaccessible_memory_);
    }
    return reinterpret_cast<uintptr_t>(inaccessible_memory_);
  }

  int* thread_in_wasm = trap_handler::GetThreadInWasmThreadLocalAddress();

 private:
  uint8_t* inaccessible_memory_ = nullptr;
};

TEST_F(SimulatorTrapHandlerTest, ProbeMemorySuccess) {
  int x = 47;
  EXPECT_EQ(0u, ProbeMemory(reinterpret_cast<uintptr_t>(&x), kFakePc));
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryFailNullptr) {
  constexpr uintptr_t kNullAddress = 0;
  EXPECT_DEATH_IF_SUPPORTED(ProbeMemory(kNullAddress, kFakePc), "");
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryFailInaccessible) {
  EXPECT_DEATH_IF_SUPPORTED(ProbeMemory(InaccessibleMemoryPtr(), kFakePc), "");
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryFailWhileInWasm) {
  // Test that we still crash if the trap handler is set up and the "thread in
  // wasm" flag is set, but the PC is not registered as a protected instruction.
  constexpr bool kUseDefaultHandler = true;
  CHECK(v8::V8::EnableWebAssemblyTrapHandler(kUseDefaultHandler));

  SetThreadInWasm();
  EXPECT_DEATH_IF_SUPPORTED(ProbeMemory(InaccessibleMemoryPtr(), kFakePc), "");
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryWithTrapHandled) {
  constexpr uintptr_t kFakeLandingPad = 19;

  constexpr bool kUseDefaultHandler = true;
  CHECK(v8::V8::EnableWebAssemblyTrapHandler(kUseDefaultHandler));

  ProtectedInstructionData fake_protected_instruction{kFakePc, kFakeLandingPad};
  int handler_data_index =
      RegisterHandlerData(0, 128, 1, &fake_protected_instruction);

  SetThreadInWasm();
  EXPECT_EQ(kFakeLandingPad, ProbeMemory(InaccessibleMemoryPtr(), kFakePc));

  // Reset everything.
  ResetThreadInWasm();
  ReleaseHandlerData(handler_data_index);
  RemoveTrapHandler();
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryWithLandingPad) {
  EXPECT_EQ(0u, GetRecoveredTrapCount());

  // Test that the trap handler can recover a memory access violation in
  // wasm code (we fake the wasm code and the access violation).
  std::unique_ptr<TestingAssemblerBuffer> buffer = AllocateAssemblerBuffer();
  constexpr Register scratch = x0;
  MacroAssembler masm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  // Generate an illegal memory access.
  masm.Mov(scratch, InaccessibleMemoryPtr());
  uint32_t crash_offset = masm.pc_offset();
  masm.Str(scratch, MemOperand(scratch, 0));  // load from inaccessible memory.
  uint32_t recovery_offset = masm.pc_offset();
  // Return.
  masm.Ret();

  CodeDesc desc;
  masm.GetCode(nullptr, &desc);

  constexpr bool kUseDefaultHandler = true;
  CHECK(v8::V8::EnableWebAssemblyTrapHandler(kUseDefaultHandler));

  ProtectedInstructionData protected_instruction{crash_offset, recovery_offset};
  int handler_data_index =
      RegisterHandlerData(reinterpret_cast<Address>(desc.buffer),
                          desc.instr_size, 1, &protected_instruction);

  // Now execute the code.
  buffer->MakeExecutable();
  GeneratedCode<void> code = GeneratedCode<void>::FromAddress(
      i_isolate(), reinterpret_cast<Address>(desc.buffer));

  SetThreadInWasm();
  code.Call();
  ResetThreadInWasm();

  ReleaseHandlerData(handler_data_index);
  RemoveTrapHandler();

  EXPECT_EQ(1u, GetRecoveredTrapCount());
}

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8
