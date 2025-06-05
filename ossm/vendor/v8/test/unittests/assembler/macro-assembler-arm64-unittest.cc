// Copyright 2019 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "src/codegen/arm64/assembler-arm64-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/heap/factory.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace test_macro_assembler_arm64 {

using MacroAssemblerArm64Test = TestWithIsolate;

using F0 = int();

#define __ masm.

TEST_F(MacroAssemblerArm64Test, EmbeddedObj) {
#ifdef V8_COMPRESS_POINTERS
  Isolate* isolate = i_isolate();
  HandleScope handles(isolate);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate, v8::internal::CodeObjectRequired::kYes,
                      buffer->CreateView());

  AssemblerBufferWriteScope rw_scope(*buffer);

  Handle<HeapObject> old_array = isolate->factory()->NewFixedArray(2000);
  Handle<HeapObject> my_array = isolate->factory()->NewFixedArray(1000);
  __ Mov(w4, Immediate(my_array, RelocInfo::COMPRESSED_EMBEDDED_OBJECT));
  __ Mov(x5, old_array);
  __ ret(x5);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef DEBUG
  StdoutStream os;
  code->Print(os);
#endif

  // Collect garbage to ensure reloc info can be walked by the heap.
  CollectAllGarbage();
  CollectAllGarbage();
  CollectAllGarbage();

  PtrComprCageBase cage_base(isolate);

  // Test the user-facing reloc interface.
  const int mode_mask = RelocInfo::EmbeddedObjectModeMask();
  for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsCompressedEmbeddedObject(mode)) {
      CHECK_EQ(*my_array, it.rinfo()->target_object(cage_base));
    } else {
      CHECK(RelocInfo::IsFullEmbeddedObject(mode));
      CHECK_EQ(*old_array, it.rinfo()->target_object(cage_base));
    }
  }
#endif  // V8_COMPRESS_POINTERS
}

TEST_F(MacroAssemblerArm64Test, DeoptExitSizeIsFixed) {
  Isolate* isolate = i_isolate();
  HandleScope handles(isolate);
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate, v8::internal::CodeObjectRequired::kYes,
                      buffer->CreateView());

  AssemblerBufferWriteScope rw_scope(*buffer);

  static_assert(static_cast<int>(kFirstDeoptimizeKind) == 0);
  for (int i = 0; i < kDeoptimizeKindCount; i++) {
    DeoptimizeKind kind = static_cast<DeoptimizeKind>(i);
    Label before_exit;
    Builtin target = Deoptimizer::GetDeoptimizationEntry(kind);
    // Mirroring logic in code-generator.cc.
    if (kind == DeoptimizeKind::kLazy) {
      // CFI emits an extra instruction here.
      masm.BindExceptionHandler(&before_exit);
    } else {
      masm.bind(&before_exit);
    }
    masm.CallForDeoptimization(target, 42, &before_exit, kind, &before_exit,
                               &before_exit);
    CHECK_EQ(masm.SizeOfCodeGeneratedSince(&before_exit),
             kind == DeoptimizeKind::kLazy ? Deoptimizer::kLazyDeoptExitSize
                                           : Deoptimizer::kEagerDeoptExitSize);
  }
}

#undef __

}  // namespace test_macro_assembler_arm64
}  // namespace internal
}  // namespace v8
