// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/code.h"

#include <iomanip>

#include "src/codegen/assembler-inl.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/reloc-info.h"
#include "src/codegen/safepoint-table.h"
#include "src/codegen/source-position.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/code-kind.h"
#include "src/objects/fixed-array.h"
#include "src/roots/roots-inl.h"
#include "src/snapshot/embedded/embedded-data-inl.h"
#include "src/utils/ostreams.h"

#ifdef ENABLE_DISASSEMBLER
#include "src/codegen/code-comments.h"
#include "src/diagnostics/disasm.h"
#include "src/diagnostics/disassembler.h"
#include "src/diagnostics/eh-frame.h"
#endif

namespace v8 {
namespace internal {

namespace {

// Helper function for getting an EmbeddedData that can handle un-embedded
// builtins when short builtin calls are enabled.
inline EmbeddedData EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(
    HeapObject code) {
#if defined(V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE)
  // GetIsolateFromWritableObject(*this) works for both read-only and writable
  // objects when pointer compression is enabled with a per-Isolate cage.
  return EmbeddedData::FromBlob(GetIsolateFromWritableObject(code));
#elif defined(V8_COMPRESS_POINTERS_IN_SHARED_CAGE)
  // When pointer compression is enabled with a shared cage, there is also a
  // shared CodeRange. When short builtin calls are enabled, there is a single
  // copy of the re-embedded builtins in the shared CodeRange, so use that if
  // it's present.
  if (v8_flags.jitless) return EmbeddedData::FromBlob();
  CodeRange* code_range = CodeRange::GetProcessWideCodeRange().get();
  return (code_range && code_range->embedded_blob_code_copy() != nullptr)
             ? EmbeddedData::FromBlob(code_range)
             : EmbeddedData::FromBlob();
#else
  // Otherwise there is a single copy of the blob across all Isolates, use the
  // global atomic variables.
  return EmbeddedData::FromBlob();
#endif
}

}  // namespace

Address OffHeapInstructionStart(HeapObject code, Builtin builtin) {
  // TODO(11527): Here and below: pass Isolate as an argument for getting
  // the EmbeddedData.
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.InstructionStartOfBuiltin(builtin);
}

Address OffHeapInstructionEnd(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.InstructionStartOfBuiltin(builtin) +
         d.InstructionSizeOfBuiltin(builtin);
}

int OffHeapInstructionSize(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.InstructionSizeOfBuiltin(builtin);
}

Address OffHeapMetadataStart(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.MetadataStartOfBuiltin(builtin);
}

Address OffHeapMetadataEnd(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.MetadataStartOfBuiltin(builtin) + d.MetadataSizeOfBuiltin(builtin);
}

int OffHeapMetadataSize(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.MetadataSizeOfBuiltin(builtin);
}

Address OffHeapSafepointTableAddress(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.SafepointTableStartOf(builtin);
}

int OffHeapSafepointTableSize(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.SafepointTableSizeOf(builtin);
}

Address OffHeapHandlerTableAddress(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.HandlerTableStartOf(builtin);
}

int OffHeapHandlerTableSize(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.HandlerTableSizeOf(builtin);
}

Address OffHeapConstantPoolAddress(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.ConstantPoolStartOf(builtin);
}

int OffHeapConstantPoolSize(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.ConstantPoolSizeOf(builtin);
}

Address OffHeapCodeCommentsAddress(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.CodeCommentsStartOf(builtin);
}

int OffHeapCodeCommentsSize(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.CodeCommentsSizeOf(builtin);
}

Address OffHeapUnwindingInfoAddress(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.UnwindingInfoStartOf(builtin);
}

int OffHeapUnwindingInfoSize(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.UnwindingInfoSizeOf(builtin);
}

int OffHeapStackSlots(HeapObject code, Builtin builtin) {
  EmbeddedData d = EmbeddedDataWithMaybeRemappedEmbeddedBuiltins(code);
  return d.StackSlotsOf(builtin);
}

void Code::ClearEmbeddedObjects(Heap* heap) {
  HeapObject undefined = ReadOnlyRoots(heap).undefined_value();
  int mode_mask = RelocInfo::EmbeddedObjectModeMask();
  for (RelocIterator it(*this, mode_mask); !it.done(); it.next()) {
    DCHECK(RelocInfo::IsEmbeddedObjectMode(it.rinfo()->rmode()));
    it.rinfo()->set_target_object(heap, undefined, SKIP_WRITE_BARRIER);
  }
  set_embedded_objects_cleared(true);
}

void Code::Relocate(intptr_t delta) {
  for (RelocIterator it(*this, RelocInfo::kApplyMask); !it.done(); it.next()) {
    it.rinfo()->apply(delta);
  }
  FlushICache();
}

void Code::FlushICache() const {
  FlushInstructionCache(raw_instruction_start(), raw_instruction_size());
}

void Code::CopyFromNoFlush(ByteArray reloc_info, Heap* heap,
                           const CodeDesc& desc) {
  // Copy code.
  static_assert(kOnHeapBodyIsContiguous);
  CopyBytes(reinterpret_cast<byte*>(raw_instruction_start()), desc.buffer,
            static_cast<size_t>(desc.instr_size));
  // TODO(jgruber,v8:11036): Merge with the above.
  CopyBytes(reinterpret_cast<byte*>(raw_instruction_start() + desc.instr_size),
            desc.unwinding_info, static_cast<size_t>(desc.unwinding_info_size));

  // Copy reloc info.
  CopyRelocInfoToByteArray(reloc_info, desc);

  // Unbox handles and relocate.
  RelocateFromDesc(reloc_info, heap, desc);
}

void Code::RelocateFromDesc(ByteArray reloc_info, Heap* heap,
                            const CodeDesc& desc) {
  // Unbox handles and relocate.
  Assembler* origin = desc.origin;
  const int mode_mask = RelocInfo::PostCodegenRelocationMask();
  for (RelocIterator it(*this, reloc_info, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsEmbeddedObjectMode(mode)) {
      Handle<HeapObject> p = it.rinfo()->target_object_handle(origin);
      it.rinfo()->set_target_object(heap, *p, UPDATE_WRITE_BARRIER,
                                    SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsCodeTargetMode(mode)) {
      // Rewrite code handles to direct pointers to the first instruction in the
      // code object.
      Handle<HeapObject> p = it.rinfo()->target_object_handle(origin);
      DCHECK(p->IsCodeT(GetPtrComprCageBaseSlow(*p)));
      Code code = FromCodeT(CodeT::cast(*p));
      it.rinfo()->set_target_address(code.raw_instruction_start(),
                                     UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsNearBuiltinEntry(mode)) {
      // Rewrite builtin IDs to PC-relative offset to the builtin entry point.
      Builtin builtin = it.rinfo()->target_builtin_at(origin);
      Address p =
          heap->isolate()->builtin_entry_table()[Builtins::ToInt(builtin)];
      it.rinfo()->set_target_address(p, UPDATE_WRITE_BARRIER,
                                     SKIP_ICACHE_FLUSH);
      DCHECK_EQ(p, it.rinfo()->target_address());
    } else if (RelocInfo::IsRuntimeEntry(mode)) {
      Address p = it.rinfo()->target_runtime_entry(origin);
      it.rinfo()->set_target_runtime_entry(p, UPDATE_WRITE_BARRIER,
                                           SKIP_ICACHE_FLUSH);
    } else {
      intptr_t delta =
          raw_instruction_start() - reinterpret_cast<Address>(desc.buffer);
      it.rinfo()->apply(delta);
    }
  }
}

SafepointEntry Code::GetSafepointEntry(Isolate* isolate, Address pc) {
  DCHECK(!is_maglevved());
  SafepointTable table(isolate, pc, *this);
  return table.FindEntry(pc);
}

#ifdef V8_EXTERNAL_CODE_SPACE
SafepointEntry CodeDataContainer::GetSafepointEntry(Isolate* isolate,
                                                    Address pc) {
  DCHECK(!is_maglevved());
  SafepointTable table(isolate, pc, *this);
  return table.FindEntry(pc);
}
#endif  // V8_EXTERNAL_CODE_SPACE

MaglevSafepointEntry Code::GetMaglevSafepointEntry(Isolate* isolate,
                                                   Address pc) {
  DCHECK(is_maglevved());
  MaglevSafepointTable table(isolate, pc, *this);
  return table.FindEntry(pc);
}

#ifdef V8_EXTERNAL_CODE_SPACE
MaglevSafepointEntry CodeDataContainer::GetMaglevSafepointEntry(
    Isolate* isolate, Address pc) {
  DCHECK(is_maglevved());
  MaglevSafepointTable table(isolate, pc, *this);
  return table.FindEntry(pc);
}
#endif  // V8_EXTERNAL_CODE_SPACE

Address Code::OffHeapInstructionStart(Isolate* isolate, Address pc) const {
  DCHECK(is_off_heap_trampoline());
  EmbeddedData d = EmbeddedData::GetEmbeddedDataForPC(isolate, pc);
  return d.InstructionStartOfBuiltin(builtin_id());
}

#ifdef V8_EXTERNAL_CODE_SPACE
Address CodeDataContainer::OffHeapInstructionStart(Isolate* isolate,
                                                   Address pc) const {
  DCHECK(is_off_heap_trampoline());
  EmbeddedData d = EmbeddedData::GetEmbeddedDataForPC(isolate, pc);
  return d.InstructionStartOfBuiltin(builtin_id());
}
#endif

Address Code::OffHeapInstructionEnd(Isolate* isolate, Address pc) const {
  DCHECK(is_off_heap_trampoline());
  EmbeddedData d = EmbeddedData::GetEmbeddedDataForPC(isolate, pc);
  return d.InstructionEndOf(builtin_id());
}

#ifdef V8_EXTERNAL_CODE_SPACE
Address CodeDataContainer::OffHeapInstructionEnd(Isolate* isolate,
                                                 Address pc) const {
  DCHECK(is_off_heap_trampoline());
  EmbeddedData d = EmbeddedData::GetEmbeddedDataForPC(isolate, pc);
  return d.InstructionEndOf(builtin_id());
}
#endif  // V8_EXTERNAL_CODE_SPACE

bool Code::OffHeapBuiltinContains(Isolate* isolate, Address pc) const {
  DCHECK(is_off_heap_trampoline());
  EmbeddedData d = EmbeddedData::GetEmbeddedDataForPC(isolate, pc);
  return d.BuiltinContains(builtin_id(), pc);
}

#ifdef V8_EXTERNAL_CODE_SPACE
bool CodeDataContainer::OffHeapBuiltinContains(Isolate* isolate,
                                               Address pc) const {
  DCHECK(is_off_heap_trampoline());
  EmbeddedData d = EmbeddedData::GetEmbeddedDataForPC(isolate, pc);
  return d.BuiltinContains(builtin_id(), pc);
}
#endif  // V8_EXTERNAL_CODE_SPACE

// TODO(cbruni): Move to BytecodeArray
int AbstractCode::SourcePosition(PtrComprCageBase cage_base, int offset) {
  CHECK_NE(kind(cage_base), CodeKind::BASELINE);
  Object maybe_table = SourcePositionTableInternal(cage_base);
  if (maybe_table.IsException()) return kNoSourcePosition;

  ByteArray source_position_table = ByteArray::cast(maybe_table);
  // Subtract one because the current PC is one instruction after the call site.
  if (IsCode(cage_base)) offset--;
  int position = 0;
  for (SourcePositionTableIterator iterator(
           source_position_table, SourcePositionTableIterator::kJavaScriptOnly,
           SourcePositionTableIterator::kDontSkipFunctionEntry);
       !iterator.done() && iterator.code_offset() <= offset;
       iterator.Advance()) {
    position = iterator.source_position().ScriptOffset();
  }
  return position;
}

// TODO(cbruni): Move to BytecodeArray
int AbstractCode::SourceStatementPosition(PtrComprCageBase cage_base,
                                          int offset) {
  CHECK_NE(kind(cage_base), CodeKind::BASELINE);
  // First find the closest position.
  int position = SourcePosition(cage_base, offset);
  // Now find the closest statement position before the position.
  int statement_position = 0;
  for (SourcePositionTableIterator it(SourcePositionTableInternal(cage_base));
       !it.done(); it.Advance()) {
    if (it.is_statement()) {
      int p = it.source_position().ScriptOffset();
      if (statement_position < p && p <= position) {
        statement_position = p;
      }
    }
  }
  return statement_position;
}

bool Code::CanDeoptAt(Isolate* isolate, Address pc) {
  DeoptimizationData deopt_data =
      DeoptimizationData::cast(deoptimization_data());
  Address code_start_address = InstructionStart(isolate, pc);
  for (int i = 0; i < deopt_data.DeoptCount(); i++) {
    if (deopt_data.Pc(i).value() == -1) continue;
    Address address = code_start_address + deopt_data.Pc(i).value();
    if (address == pc &&
        deopt_data.GetBytecodeOffset(i) != BytecodeOffset::None()) {
      return true;
    }
  }
  return false;
}

bool Code::IsIsolateIndependent(Isolate* isolate) {
  static constexpr int kModeMask =
      RelocInfo::AllRealModesMask() &
      ~RelocInfo::ModeMask(RelocInfo::CONST_POOL) &
      ~RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) &
      ~RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
  static_assert(kModeMask ==
                (RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
                 RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET) |
                 RelocInfo::ModeMask(RelocInfo::COMPRESSED_EMBEDDED_OBJECT) |
                 RelocInfo::ModeMask(RelocInfo::FULL_EMBEDDED_OBJECT) |
                 RelocInfo::ModeMask(RelocInfo::DATA_EMBEDDED_OBJECT) |
                 RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                 RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) |
                 RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) |
                 RelocInfo::ModeMask(RelocInfo::NEAR_BUILTIN_ENTRY) |
                 RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
                 RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
                 RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL)));

#if defined(V8_TARGET_ARCH_PPC) || defined(V8_TARGET_ARCH_PPC64) || \
    defined(V8_TARGET_ARCH_MIPS64)
  return RelocIterator(*this, kModeMask).done();
#elif defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64) ||  \
    defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_S390) ||     \
    defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_RISCV64) || \
    defined(V8_TARGET_ARCH_LOONG64) || defined(V8_TARGET_ARCH_RISCV32)
  for (RelocIterator it(*this, kModeMask); !it.done(); it.next()) {
    // On these platforms we emit relative builtin-to-builtin
    // jumps for isolate independent builtins in the snapshot. They are later
    // rewritten as pc-relative jumps to the off-heap instruction stream and are
    // thus process-independent. See also: FinalizeEmbeddedCodeTargets.
    if (RelocInfo::IsCodeTargetMode(it.rinfo()->rmode())) {
      Address target_address = it.rinfo()->target_address();
      if (OffHeapInstructionStream::PcIsOffHeap(isolate, target_address))
        continue;

      Code target = Code::GetCodeFromTargetAddress(target_address);
      CHECK(target.IsCode());
      if (Builtins::IsIsolateIndependentBuiltin(target)) continue;
    }
    return false;
  }
  return true;
#else
#error Unsupported architecture.
#endif
}

bool Code::Inlines(SharedFunctionInfo sfi) {
  // We can only check for inlining for optimized code.
  DCHECK(is_optimized_code());
  DisallowGarbageCollection no_gc;
  DeoptimizationData const data =
      DeoptimizationData::cast(deoptimization_data());
  if (data.length() == 0) return false;
  if (data.SharedFunctionInfo() == sfi) return true;
  DeoptimizationLiteralArray const literals = data.LiteralArray();
  int const inlined_count = data.InlinedFunctionCount().value();
  for (int i = 0; i < inlined_count; ++i) {
    if (SharedFunctionInfo::cast(literals.get(i)) == sfi) return true;
  }
  return false;
}

Code::OptimizedCodeIterator::OptimizedCodeIterator(Isolate* isolate) {
  isolate_ = isolate;
  Object list = isolate->heap()->native_contexts_list();
  next_context_ =
      list.IsUndefined(isolate_) ? NativeContext() : NativeContext::cast(list);
}

Code Code::OptimizedCodeIterator::Next() {
  do {
    Object next;
    if (!current_code_.is_null()) {
      // Get next code in the linked list.
      next = current_code_.next_code_link();
    } else if (!next_context_.is_null()) {
      // Linked list of code exhausted. Get list of next context.
      next = next_context_.OptimizedCodeListHead();
      Object next_context = next_context_.next_context_link();
      next_context_ = next_context.IsUndefined(isolate_)
                          ? NativeContext()
                          : NativeContext::cast(next_context);
    } else {
      // Exhausted contexts.
      return Code();
    }
    current_code_ =
        next.IsUndefined(isolate_) ? Code() : FromCodeT(CodeT::cast(next));
  } while (current_code_.is_null());
  DCHECK(CodeKindCanDeoptimize(current_code_.kind()));
  return current_code_;
}

Handle<DeoptimizationData> DeoptimizationData::New(Isolate* isolate,
                                                   int deopt_entry_count,
                                                   AllocationType allocation) {
  return Handle<DeoptimizationData>::cast(isolate->factory()->NewFixedArray(
      LengthFor(deopt_entry_count), allocation));
}

Handle<DeoptimizationData> DeoptimizationData::Empty(Isolate* isolate) {
  return Handle<DeoptimizationData>::cast(
      isolate->factory()->empty_fixed_array());
}

SharedFunctionInfo DeoptimizationData::GetInlinedFunction(int index) {
  if (index == -1) {
    return SharedFunctionInfo::cast(SharedFunctionInfo());
  } else {
    return SharedFunctionInfo::cast(LiteralArray().get(index));
  }
}

#ifdef ENABLE_DISASSEMBLER

namespace {

void print_pc(std::ostream& os, int pc) {
  if (pc == -1) {
    os << "NA";
  } else {
    os << std::hex << pc << std::dec;
  }
}
}  // anonymous namespace

void DeoptimizationData::DeoptimizationDataPrint(std::ostream& os) {
  if (length() == 0) {
    os << "Deoptimization Input Data invalidated by lazy deoptimization\n";
    return;
  }

  int const inlined_function_count = InlinedFunctionCount().value();
  os << "Inlined functions (count = " << inlined_function_count << ")\n";
  for (int id = 0; id < inlined_function_count; ++id) {
    Object info = LiteralArray().get(id);
    os << " " << Brief(SharedFunctionInfo::cast(info)) << "\n";
  }
  os << "\n";
  int deopt_count = DeoptCount();
  os << "Deoptimization Input Data (deopt points = " << deopt_count << ")\n";
  if (0 != deopt_count) {
#ifdef DEBUG
    os << " index  bytecode-offset  node-id    pc";
#else   // DEBUG
    os << " index  bytecode-offset    pc";
#endif  // DEBUG
    if (v8_flags.print_code_verbose) os << "  commands";
    os << "\n";
  }
  for (int i = 0; i < deopt_count; i++) {
    os << std::setw(6) << i << "  " << std::setw(15)
       << GetBytecodeOffset(i).ToInt() << "  "
#ifdef DEBUG
       << std::setw(7) << NodeId(i).value() << "  "
#endif  // DEBUG
       << std::setw(4);
    print_pc(os, Pc(i).value());
    os << std::setw(2);

    if (!v8_flags.print_code_verbose) {
      os << "\n";
      continue;
    }

    TranslationArrayPrintSingleFrame(os, TranslationByteArray(),
                                     TranslationIndex(i).value(),
                                     LiteralArray());
  }
}

namespace {

template <typename CodeOrCodeT>
inline void DisassembleCodeRange(Isolate* isolate, std::ostream& os,
                                 CodeOrCodeT code, Address begin, size_t size,
                                 Address current_pc) {
  Address end = begin + size;
  AllowHandleAllocation allow_handles;
  DisallowGarbageCollection no_gc;
  HandleScope handle_scope(isolate);
  Disassembler::Decode(isolate, os, reinterpret_cast<byte*>(begin),
                       reinterpret_cast<byte*>(end),
                       CodeReference(handle(code, isolate)), current_pc);
}

template <typename CodeOrCodeT>
void Disassemble(const char* name, std::ostream& os, Isolate* isolate,
                 CodeOrCodeT code, Address current_pc) {
  CodeKind kind = code.kind();
  os << "kind = " << CodeKindToString(kind) << "\n";
  if (name == nullptr && code.is_builtin()) {
    name = Builtins::name(code.builtin_id());
  }
  if ((name != nullptr) && (name[0] != '\0')) {
    os << "name = " << name << "\n";
  }
  if (CodeKindIsOptimizedJSFunction(kind) && kind != CodeKind::BASELINE) {
    os << "stack_slots = " << code.stack_slots() << "\n";
  }
  os << "compiler = "
     << (code.is_turbofanned()        ? "turbofan"
         : code.is_maglevved()        ? "maglev"
         : kind == CodeKind::BASELINE ? "baseline"
                                      : "unknown")
     << "\n";
  os << "address = " << reinterpret_cast<void*>(code.ptr()) << "\n\n";

  if (code.IsCode() && code.is_off_heap_trampoline()) {
    Code trampoline_code = Code::cast(code);
    int trampoline_size = trampoline_code.raw_instruction_size();
    os << "Trampoline (size = " << trampoline_size << ")\n";
    DisassembleCodeRange(isolate, os, trampoline_code,
                         trampoline_code.raw_instruction_start(),
                         trampoline_size, current_pc);
    os << "\n";
  }

  {
    int code_size = code.InstructionSize();
    os << "Instructions (size = " << code_size << ")\n";
    DisassembleCodeRange(isolate, os, code, code.InstructionStart(), code_size,
                         current_pc);

    if (int pool_size = code.constant_pool_size()) {
      DCHECK_EQ(pool_size & kPointerAlignmentMask, 0);
      os << "\nConstant Pool (size = " << pool_size << ")\n";
      base::Vector<char> buf = base::Vector<char>::New(50);
      intptr_t* ptr = reinterpret_cast<intptr_t*>(code.constant_pool());
      for (int i = 0; i < pool_size; i += kSystemPointerSize, ptr++) {
        SNPrintF(buf, "%4d %08" V8PRIxPTR, i, *ptr);
        os << static_cast<const void*>(ptr) << "  " << buf.begin() << "\n";
      }
    }
  }
  os << "\n";

  // TODO(cbruni): add support for baseline code.
  if (kind != CodeKind::BASELINE) {
    {
      SourcePositionTableIterator it(
          code.source_position_table(),
          SourcePositionTableIterator::kJavaScriptOnly);
      if (!it.done()) {
        os << "Source positions:\n pc offset  position\n";
        for (; !it.done(); it.Advance()) {
          os << std::setw(10) << std::hex << it.code_offset() << std::dec
             << std::setw(10) << it.source_position().ScriptOffset()
             << (it.is_statement() ? "  statement" : "") << "\n";
        }
        os << "\n";
      }
    }

    {
      SourcePositionTableIterator it(
          code.source_position_table(),
          SourcePositionTableIterator::kExternalOnly);
      if (!it.done()) {
        os << "External Source positions:\n pc offset  fileid  line\n";
        for (; !it.done(); it.Advance()) {
          DCHECK(it.source_position().IsExternal());
          os << std::setw(10) << std::hex << it.code_offset() << std::dec
             << std::setw(10) << it.source_position().ExternalFileId()
             << std::setw(10) << it.source_position().ExternalLine() << "\n";
        }
        os << "\n";
      }
    }
  }

  if (CodeKindCanDeoptimize(kind)) {
    DeoptimizationData data =
        DeoptimizationData::cast(code.deoptimization_data());
    data.DeoptimizationDataPrint(os);
  }
  os << "\n";

  if (code.uses_safepoint_table()) {
    if (code.is_maglevved()) {
      MaglevSafepointTable table(isolate, current_pc, code);
      table.Print(os);
    } else {
      SafepointTable table(isolate, current_pc, code);
      table.Print(os);
    }
    os << "\n";
  }

  if (code.has_handler_table()) {
    HandlerTable table(code);
    os << "Handler Table (size = " << table.NumberOfReturnEntries() << ")\n";
    if (CodeKindIsOptimizedJSFunction(kind)) {
      table.HandlerTableReturnPrint(os);
    }
    os << "\n";
  }

  os << "RelocInfo (size = " << code.relocation_size() << ")\n";
  if (code.IsCode()) {
    for (RelocIterator it(Code::cast(code)); !it.done(); it.next()) {
      it.rinfo()->Print(isolate, os);
    }
  }
  os << "\n";

  if (code.has_unwinding_info()) {
    os << "UnwindingInfo (size = " << code.unwinding_info_size() << ")\n";
    EhFrameDisassembler eh_frame_disassembler(
        reinterpret_cast<byte*>(code.unwinding_info_start()),
        reinterpret_cast<byte*>(code.unwinding_info_end()));
    eh_frame_disassembler.DisassembleToStream(os);
    os << "\n";
  }
}

}  // namespace

void Code::Disassemble(const char* name, std::ostream& os, Isolate* isolate,
                       Address current_pc) {
  i::Disassemble(name, os, isolate, *this, current_pc);
}

#ifdef V8_EXTERNAL_CODE_SPACE
void CodeDataContainer::Disassemble(const char* name, std::ostream& os,
                                    Isolate* isolate, Address current_pc) {
  i::Disassemble(name, os, isolate, *this, current_pc);
}
#endif  // V8_EXTERNAL_CODE_SPACE

#endif  // ENABLE_DISASSEMBLER

void BytecodeArray::PrintJson(std::ostream& os) {
  DisallowGarbageCollection no_gc;

  Address base_address = GetFirstBytecodeAddress();
  BytecodeArray handle_storage = *this;
  Handle<BytecodeArray> handle(reinterpret_cast<Address*>(&handle_storage));
  interpreter::BytecodeArrayIterator iterator(handle);
  bool first_data = true;

  os << "{\"data\": [";

  while (!iterator.done()) {
    if (!first_data) os << ", ";
    Address current_address = base_address + iterator.current_offset();
    first_data = false;

    os << "{\"offset\":" << iterator.current_offset() << ", \"disassembly\":\"";
    interpreter::BytecodeDecoder::Decode(
        os, reinterpret_cast<byte*>(current_address), false);

    if (interpreter::Bytecodes::IsJump(iterator.current_bytecode())) {
      os << " (" << iterator.GetJumpTargetOffset() << ")";
    }

    if (interpreter::Bytecodes::IsSwitch(iterator.current_bytecode())) {
      os << " {";
      bool first_entry = true;
      for (interpreter::JumpTableTargetOffset entry :
           iterator.GetJumpTableTargetOffsets()) {
        if (!first_entry) os << ", ";
        first_entry = false;
        os << entry.target_offset;
      }
      os << "}";
    }

    os << "\"}";
    iterator.Advance();
  }

  os << "]";

  int constant_pool_lenght = constant_pool().length();
  if (constant_pool_lenght > 0) {
    os << ", \"constantPool\": [";
    for (int i = 0; i < constant_pool_lenght; i++) {
      Object object = constant_pool().get(i);
      if (i > 0) os << ", ";
      os << "\"" << object << "\"";
    }
    os << "]";
  }

  os << "}";
}

void BytecodeArray::Disassemble(std::ostream& os) {
  DisallowGarbageCollection no_gc;
  // Storage for backing the handle passed to the iterator. This handle won't be
  // updated by the gc, but that's ok because we've disallowed GCs anyway.
  BytecodeArray handle_storage = *this;
  Handle<BytecodeArray> handle(reinterpret_cast<Address*>(&handle_storage));
  Disassemble(handle, os);
}

// static
void BytecodeArray::Disassemble(Handle<BytecodeArray> handle,
                                std::ostream& os) {
  DisallowGarbageCollection no_gc;

  os << "Parameter count " << handle->parameter_count() << "\n";
  os << "Register count " << handle->register_count() << "\n";
  os << "Frame size " << handle->frame_size() << "\n";
  os << "Bytecode age: " << handle->bytecode_age() << "\n";

  Address base_address = handle->GetFirstBytecodeAddress();
  SourcePositionTableIterator source_positions(handle->SourcePositionTable());

  interpreter::BytecodeArrayIterator iterator(handle);
  while (!iterator.done()) {
    if (!source_positions.done() &&
        iterator.current_offset() == source_positions.code_offset()) {
      os << std::setw(5) << source_positions.source_position().ScriptOffset();
      os << (source_positions.is_statement() ? " S> " : " E> ");
      source_positions.Advance();
    } else {
      os << "         ";
    }
    Address current_address = base_address + iterator.current_offset();
    os << reinterpret_cast<const void*>(current_address) << " @ "
       << std::setw(4) << iterator.current_offset() << " : ";
    interpreter::BytecodeDecoder::Decode(
        os, reinterpret_cast<byte*>(current_address));
    if (interpreter::Bytecodes::IsJump(iterator.current_bytecode())) {
      Address jump_target = base_address + iterator.GetJumpTargetOffset();
      os << " (" << reinterpret_cast<void*>(jump_target) << " @ "
         << iterator.GetJumpTargetOffset() << ")";
    }
    if (interpreter::Bytecodes::IsSwitch(iterator.current_bytecode())) {
      os << " {";
      bool first_entry = true;
      for (interpreter::JumpTableTargetOffset entry :
           iterator.GetJumpTableTargetOffsets()) {
        if (first_entry) {
          first_entry = false;
        } else {
          os << ",";
        }
        os << " " << entry.case_value << ": @" << entry.target_offset;
      }
      os << " }";
    }
    os << std::endl;
    iterator.Advance();
  }

  os << "Constant pool (size = " << handle->constant_pool().length() << ")\n";
#ifdef OBJECT_PRINT
  if (handle->constant_pool().length() > 0) {
    handle->constant_pool().Print(os);
  }
#endif

  os << "Handler Table (size = " << handle->handler_table().length() << ")\n";
#ifdef ENABLE_DISASSEMBLER
  if (handle->handler_table().length() > 0) {
    HandlerTable table(*handle);
    table.HandlerTableRangePrint(os);
  }
#endif

  ByteArray source_position_table = handle->SourcePositionTable();
  os << "Source Position Table (size = " << source_position_table.length()
     << ")\n";
#ifdef OBJECT_PRINT
  if (source_position_table.length() > 0) {
    os << Brief(source_position_table) << std::endl;
  }
#endif
}

void BytecodeArray::CopyBytecodesTo(BytecodeArray to) {
  BytecodeArray from = *this;
  DCHECK_EQ(from.length(), to.length());
  CopyBytes(reinterpret_cast<byte*>(to.GetFirstBytecodeAddress()),
            reinterpret_cast<byte*>(from.GetFirstBytecodeAddress()),
            from.length());
}

void BytecodeArray::MakeOlder() {
  // BytecodeArray is aged in concurrent marker.
  // The word must be completely within the byte code array.
  Address age_addr = address() + kBytecodeAgeOffset;
  DCHECK_LE(RoundDown(age_addr, kTaggedSize) + kTaggedSize, address() + Size());
  uint16_t age = bytecode_age();
  if (age < v8_flags.bytecode_old_age) {
    static_assert(kBytecodeAgeSize == kUInt16Size);
    base::AsAtomic16::Relaxed_CompareAndSwap(
        reinterpret_cast<base::Atomic16*>(age_addr), age, age + 1);
  }

  DCHECK_LE(bytecode_age(), v8_flags.bytecode_old_age);
}

bool BytecodeArray::IsOld() const {
  return bytecode_age() >= v8_flags.bytecode_old_age;
}

DependentCode DependentCode::GetDependentCode(HeapObject object) {
  if (object.IsMap()) {
    return Map::cast(object).dependent_code();
  } else if (object.IsPropertyCell()) {
    return PropertyCell::cast(object).dependent_code();
  } else if (object.IsAllocationSite()) {
    return AllocationSite::cast(object).dependent_code();
  }
  UNREACHABLE();
}

void DependentCode::SetDependentCode(Handle<HeapObject> object,
                                     Handle<DependentCode> dep) {
  if (object->IsMap()) {
    Handle<Map>::cast(object)->set_dependent_code(*dep);
  } else if (object->IsPropertyCell()) {
    Handle<PropertyCell>::cast(object)->set_dependent_code(*dep);
  } else if (object->IsAllocationSite()) {
    Handle<AllocationSite>::cast(object)->set_dependent_code(*dep);
  } else {
    UNREACHABLE();
  }
}

namespace {

void PrintDependencyGroups(DependentCode::DependencyGroups groups) {
  while (groups != 0) {
    auto group = static_cast<DependentCode::DependencyGroup>(
        1 << base::bits::CountTrailingZeros(static_cast<uint32_t>(groups)));
    StdoutStream{} << DependentCode::DependencyGroupName(group);
    groups &= ~group;
    if (groups != 0) StdoutStream{} << ",";
  }
}

}  // namespace

void DependentCode::InstallDependency(Isolate* isolate, Handle<Code> code,
                                      Handle<HeapObject> object,
                                      DependencyGroups groups) {
  if (V8_UNLIKELY(v8_flags.trace_compilation_dependencies)) {
    StdoutStream{} << "Installing dependency of [" << code->GetHeapObject()
                   << "] on [" << object << "] in groups [";
    PrintDependencyGroups(groups);
    StdoutStream{} << "]\n";
  }
  Handle<DependentCode> old_deps(DependentCode::GetDependentCode(*object),
                                 isolate);
  Handle<DependentCode> new_deps =
      InsertWeakCode(isolate, old_deps, groups, code);

  // Update the list head if necessary.
  if (!new_deps.is_identical_to(old_deps)) {
    DependentCode::SetDependentCode(object, new_deps);
  }
}

Handle<DependentCode> DependentCode::InsertWeakCode(
    Isolate* isolate, Handle<DependentCode> entries, DependencyGroups groups,
    Handle<Code> code) {
  if (entries->length() == entries->capacity()) {
    // We'd have to grow - try to compact first.
    entries->IterateAndCompact([](CodeT, DependencyGroups) { return false; });
  }

  MaybeObjectHandle code_slot(HeapObjectReference::Weak(ToCodeT(*code)),
                              isolate);
  MaybeObjectHandle group_slot(MaybeObject::FromSmi(Smi::FromInt(groups)),
                               isolate);
  entries = Handle<DependentCode>::cast(
      WeakArrayList::AddToEnd(isolate, entries, code_slot, group_slot));
  return entries;
}

Handle<DependentCode> DependentCode::New(Isolate* isolate,
                                         DependencyGroups groups,
                                         Handle<Code> code) {
  Handle<DependentCode> result = Handle<DependentCode>::cast(
      isolate->factory()->NewWeakArrayList(LengthFor(1), AllocationType::kOld));
  result->Set(0, HeapObjectReference::Weak(ToCodeT(*code)));
  result->Set(1, Smi::FromInt(groups));
  return result;
}

void DependentCode::IterateAndCompact(const IterateAndCompactFn& fn) {
  DisallowGarbageCollection no_gc;

  int len = length();
  if (len == 0) return;

  // We compact during traversal, thus use a somewhat custom loop construct:
  //
  // - Loop back-to-front s.t. trailing cleared entries can simply drop off
  //   the back of the list.
  // - Any cleared slots are filled from the back of the list.
  int i = len - kSlotsPerEntry;
  while (i >= 0) {
    MaybeObject obj = Get(i + kCodeSlotOffset);
    if (obj->IsCleared()) {
      len = FillEntryFromBack(i, len);
      i -= kSlotsPerEntry;
      continue;
    }

    if (fn(CodeT::cast(obj->GetHeapObjectAssumeWeak()),
           static_cast<DependencyGroups>(
               Get(i + kGroupsSlotOffset).ToSmi().value()))) {
      len = FillEntryFromBack(i, len);
    }

    i -= kSlotsPerEntry;
  }

  set_length(len);
}

bool DependentCode::MarkCodeForDeoptimization(
    DependentCode::DependencyGroups deopt_groups) {
  DisallowGarbageCollection no_gc;

  bool marked_something = false;
  IterateAndCompact([&](CodeT code, DependencyGroups groups) {
    if ((groups & deopt_groups) == 0) return false;

    if (!code.marked_for_deoptimization()) {
      code.SetMarkedForDeoptimization("code dependencies");
      marked_something = true;
    }

    return true;
  });

  return marked_something;
}

int DependentCode::FillEntryFromBack(int index, int length) {
  DCHECK_EQ(index % 2, 0);
  DCHECK_EQ(length % 2, 0);
  for (int i = length - kSlotsPerEntry; i > index; i -= kSlotsPerEntry) {
    MaybeObject obj = Get(i + kCodeSlotOffset);
    if (obj->IsCleared()) continue;

    Set(index + kCodeSlotOffset, obj);
    Set(index + kGroupsSlotOffset, Get(i + kGroupsSlotOffset),
        SKIP_WRITE_BARRIER);
    return i;
  }
  return index;  // No non-cleared entry found.
}

void DependentCode::DeoptimizeDependencyGroups(
    Isolate* isolate, DependentCode::DependencyGroups groups) {
  DisallowGarbageCollection no_gc_scope;
  bool marked_something = MarkCodeForDeoptimization(groups);
  if (marked_something) {
    DCHECK(AllowCodeDependencyChange::IsAllowed());
    Deoptimizer::DeoptimizeMarkedCode(isolate);
  }
}

// static
DependentCode DependentCode::empty_dependent_code(const ReadOnlyRoots& roots) {
  return DependentCode::cast(roots.empty_weak_array_list());
}

void Code::SetMarkedForDeoptimization(const char* reason) {
  set_marked_for_deoptimization(true);
  Deoptimizer::TraceMarkForDeoptimization(*this, reason);
}

#ifdef V8_EXTERNAL_CODE_SPACE
void CodeDataContainer::SetMarkedForDeoptimization(const char* reason) {
  set_marked_for_deoptimization(true);
  Deoptimizer::TraceMarkForDeoptimization(FromCodeT(*this), reason);
}
#endif

const char* DependentCode::DependencyGroupName(DependencyGroup group) {
  switch (group) {
    case kTransitionGroup:
      return "transition";
    case kPrototypeCheckGroup:
      return "prototype-check";
    case kPropertyCellChangedGroup:
      return "property-cell-changed";
    case kFieldConstGroup:
      return "field-const";
    case kFieldTypeGroup:
      return "field-type";
    case kFieldRepresentationGroup:
      return "field-representation";
    case kInitialMapChangedGroup:
      return "initial-map-changed";
    case kAllocationSiteTenuringChangedGroup:
      return "allocation-site-tenuring-changed";
    case kAllocationSiteTransitionChangedGroup:
      return "allocation-site-transition-changed";
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
