// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_EMBEDDED_EMBEDDED_DATA_H_
#define V8_SNAPSHOT_EMBEDDED_EMBEDDED_DATA_H_

#include "src/base/macros.h"
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/code-range.h"

namespace v8 {
namespace internal {

class Code;
class Isolate;

// Wraps an off-heap instruction stream.
// TODO(jgruber,v8:6666): Remove this class.
class OffHeapInstructionStream final : public AllStatic {
 public:
  // Returns true, iff the given pc points into an off-heap instruction stream.
  static bool PcIsOffHeap(Isolate* isolate, Address pc);

  // If the address belongs to the embedded code blob, predictably converts it
  // to uint32 by calculating offset from the embedded code blob start and
  // returns true, and false otherwise.
  static bool TryGetAddressForHashing(Isolate* isolate, Address address,
                                      uint32_t* hashable_address);

  // Returns the corresponding builtin ID if lookup succeeds, and kNoBuiltinId
  // otherwise.
  static Builtin TryLookupCode(Isolate* isolate, Address address);

  // During snapshot creation, we first create an executable off-heap area
  // containing all off-heap code. The area is guaranteed to be contiguous.
  // Note that this only applies when building the snapshot, e.g. for
  // mksnapshot. Otherwise, off-heap code is embedded directly into the binary.
  static void CreateOffHeapOffHeapInstructionStream(Isolate* isolate,
                                                    uint8_t** code,
                                                    uint32_t* code_size,
                                                    uint8_t** data,
                                                    uint32_t* data_size);
  static void FreeOffHeapOffHeapInstructionStream(uint8_t* code,
                                                  uint32_t code_size,
                                                  uint8_t* data,
                                                  uint32_t data_size);
};

class EmbeddedData final {
 public:
  static EmbeddedData FromIsolate(Isolate* isolate);

  static EmbeddedData FromBlob() {
    return EmbeddedData(Isolate::CurrentEmbeddedBlobCode(),
                        Isolate::CurrentEmbeddedBlobCodeSize(),
                        Isolate::CurrentEmbeddedBlobData(),
                        Isolate::CurrentEmbeddedBlobDataSize());
  }

  static EmbeddedData FromBlob(Isolate* isolate) {
    return EmbeddedData(
        isolate->embedded_blob_code(), isolate->embedded_blob_code_size(),
        isolate->embedded_blob_data(), isolate->embedded_blob_data_size());
  }

  static EmbeddedData FromBlob(CodeRange* code_range) {
    return EmbeddedData(code_range->embedded_blob_code_copy(),
                        Isolate::CurrentEmbeddedBlobCodeSize(),
                        Isolate::CurrentEmbeddedBlobData(),
                        Isolate::CurrentEmbeddedBlobDataSize());
  }

  const uint8_t* code() const { return code_; }
  uint32_t code_size() const { return code_size_; }
  const uint8_t* data() const { return data_; }
  uint32_t data_size() const { return data_size_; }

  bool IsInCodeRange(Address pc) const {
    Address start = reinterpret_cast<Address>(code_);
    return (start <= pc) && (pc < start + code_size_);
  }

  // When short builtin calls optimization is enabled for the Isolate, there
  // will be two builtins instruction streams executed: the embedded one and
  // the one un-embedded into the per-Isolate code range. In most of the cases,
  // the per-Isolate instructions will be used but in some cases (like builtin
  // calls from Wasm) the embedded instruction stream could be used.
  // If the requested PC belongs to the embedded code blob - it'll be returned,
  // and the per-Isolate blob otherwise.
  // See http://crbug.com/v8/11527 for details.
  inline static EmbeddedData GetEmbeddedDataForPC(Isolate* isolate,
                                                  Address maybe_builtin_pc) {
    EmbeddedData d = EmbeddedData::FromBlob(isolate);
    if (isolate->is_short_builtin_calls_enabled() &&
        !d.IsInCodeRange(maybe_builtin_pc)) {
      EmbeddedData global_d = EmbeddedData::FromBlob();
      // If the pc does not belong to the embedded code blob we should be using
      // the un-embedded one.
      if (global_d.IsInCodeRange(maybe_builtin_pc)) return global_d;
    }
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
    if (V8_SHORT_BUILTIN_CALLS_BOOL && !d.IsInCodeRange(maybe_builtin_pc)) {
      // When shared pointer compression cage is enabled and it has the embedded
      // code blob copy then it could have been used regardless of whether the
      // isolate uses it or knows about it or not (see
      // Code::OffHeapInstructionStart()).
      // So, this blob has to be checked too.
      CodeRange* code_range = CodeRange::GetProcessWideCodeRange().get();
      if (code_range && code_range->embedded_blob_code_copy() != nullptr) {
        EmbeddedData remapped_d = EmbeddedData::FromBlob(code_range);
        // If the pc does not belong to the embedded code blob we should be
        // using the un-embedded one.
        if (remapped_d.IsInCodeRange(maybe_builtin_pc)) return remapped_d;
      }
    }
#endif
    return d;
  }

  void Dispose() {
    delete[] code_;
    code_ = nullptr;
    delete[] data_;
    data_ = nullptr;
  }

  inline bool BuiltinContains(Builtin builtin, Address pc) const;

  // TODO(ishell): rename XyzOfBuiltin() to XyzOf().
  inline Address InstructionStartOfBuiltin(Builtin builtin) const;
  inline Address InstructionEndOf(Builtin builtin) const;
  inline uint32_t InstructionSizeOfBuiltin(Builtin builtin) const;

  inline Address InstructionStartOfBytecodeHandlers() const;
  inline Address InstructionEndOfBytecodeHandlers() const;

  inline Address MetadataStartOfBuiltin(Builtin builtin) const;
  inline uint32_t MetadataSizeOfBuiltin(Builtin builtin) const;

  inline Address SafepointTableStartOf(Builtin builtin) const;
  inline uint32_t SafepointTableSizeOf(Builtin builtin) const;

  inline Address HandlerTableStartOf(Builtin builtin) const;
  inline uint32_t HandlerTableSizeOf(Builtin builtin) const;

  inline Address ConstantPoolStartOf(Builtin builtin) const;
  inline uint32_t ConstantPoolSizeOf(Builtin builtin) const;

  inline Address CodeCommentsStartOf(Builtin builtin) const;
  inline uint32_t CodeCommentsSizeOf(Builtin builtin) const;

  inline Address UnwindingInfoStartOf(Builtin builtin) const;
  inline uint32_t UnwindingInfoSizeOf(Builtin builtin) const;

  inline uint32_t StackSlotsOf(Builtin builtin) const;

  uint32_t AddressForHashing(Address addr) {
    DCHECK(IsInCodeRange(addr));
    Address start = reinterpret_cast<Address>(code_);
    return static_cast<uint32_t>(addr - start);
  }

  // Padded with kCodeAlignment.
  // TODO(v8:11045): Consider removing code alignment.
  inline uint32_t PaddedInstructionSizeOfBuiltin(Builtin builtin) const;

  size_t CreateEmbeddedBlobDataHash() const;
  size_t CreateEmbeddedBlobCodeHash() const;
  size_t EmbeddedBlobDataHash() const {
    return *reinterpret_cast<const size_t*>(data_ +
                                            EmbeddedBlobDataHashOffset());
  }
  size_t EmbeddedBlobCodeHash() const {
    return *reinterpret_cast<const size_t*>(data_ +
                                            EmbeddedBlobCodeHashOffset());
  }

  size_t IsolateHash() const {
    return *reinterpret_cast<const size_t*>(data_ + IsolateHashOffset());
  }

  // Blob layout information for a single instruction stream. Corresponds
  // roughly to Code object layout (see the instruction and metadata area).
  struct LayoutDescription {
    // The offset and (unpadded) length of this builtin's instruction area
    // from the start of the embedded code section.
    uint32_t instruction_offset;
    uint32_t instruction_length;
    // The offset and (unpadded) length of this builtin's metadata area
    // from the start of the embedded data section.
    uint32_t metadata_offset;
    uint32_t metadata_length;

    // The offsets describing inline metadata tables, relative to the start
    // of the embedded data section.
    uint32_t handler_table_offset;
#if V8_EMBEDDED_CONSTANT_POOL
    uint32_t constant_pool_offset;
#endif
    uint32_t code_comments_offset_offset;
    uint32_t unwinding_info_offset_offset;

    uint32_t stack_slots;
  };
  static_assert(offsetof(LayoutDescription, instruction_offset) ==
                0 * kUInt32Size);
  static_assert(offsetof(LayoutDescription, instruction_length) ==
                1 * kUInt32Size);
  static_assert(offsetof(LayoutDescription, metadata_offset) ==
                2 * kUInt32Size);
  static_assert(offsetof(LayoutDescription, metadata_length) ==
                3 * kUInt32Size);
  static_assert(offsetof(LayoutDescription, handler_table_offset) ==
                4 * kUInt32Size);
#if V8_EMBEDDED_CONSTANT_POOL
  static_assert(offsetof(LayoutDescription, constant_pool_offset) ==
                5 * kUInt32Size);
  static_assert(offsetof(LayoutDescription, code_comments_offset_offset) ==
                6 * kUInt32Size);
  static_assert(offsetof(LayoutDescription, unwinding_info_offset_offset) ==
                7 * kUInt32Size);
  static_assert(offsetof(LayoutDescription, stack_slots) == 8 * kUInt32Size);
  static_assert(sizeof(LayoutDescription) == 9 * kUInt32Size);
#else
  static_assert(offsetof(LayoutDescription, code_comments_offset_offset) ==
                5 * kUInt32Size);
  static_assert(offsetof(LayoutDescription, unwinding_info_offset_offset) ==
                6 * kUInt32Size);
  static_assert(offsetof(LayoutDescription, stack_slots) == 7 * kUInt32Size);
  static_assert(sizeof(LayoutDescription) == 8 * kUInt32Size);
#endif

  // The layout of the blob is as follows:
  //
  // data:
  // [0] hash of the data section
  // [1] hash of the code section
  // [2] hash of embedded-blob-relevant heap objects
  // [3] layout description of instruction stream 0
  // ... layout descriptions
  // [x] metadata section of builtin 0
  // ... metadata sections
  //
  // code:
  // [0] instruction section of builtin 0
  // ... instruction sections

  static constexpr uint32_t kTableSize = Builtins::kBuiltinCount;
  static constexpr uint32_t EmbeddedBlobDataHashOffset() { return 0; }
  static constexpr uint32_t EmbeddedBlobDataHashSize() { return kSizetSize; }
  static constexpr uint32_t EmbeddedBlobCodeHashOffset() {
    return EmbeddedBlobDataHashOffset() + EmbeddedBlobDataHashSize();
  }
  static constexpr uint32_t EmbeddedBlobCodeHashSize() { return kSizetSize; }
  static constexpr uint32_t IsolateHashOffset() {
    return EmbeddedBlobCodeHashOffset() + EmbeddedBlobCodeHashSize();
  }
  static constexpr uint32_t IsolateHashSize() { return kSizetSize; }
  static constexpr uint32_t LayoutDescriptionTableOffset() {
    return IsolateHashOffset() + IsolateHashSize();
  }
  static constexpr uint32_t LayoutDescriptionTableSize() {
    return sizeof(struct LayoutDescription) * kTableSize;
  }
  static constexpr uint32_t FixedDataSize() {
    return LayoutDescriptionTableOffset() + LayoutDescriptionTableSize();
  }
  // The variable-size data section starts here.
  static constexpr uint32_t RawMetadataOffset() { return FixedDataSize(); }

  // Code is in its own dedicated section.
  static constexpr uint32_t RawCodeOffset() { return 0; }

 private:
  EmbeddedData(const uint8_t* code, uint32_t code_size, const uint8_t* data,
               uint32_t data_size)
      : code_(code), code_size_(code_size), data_(data), data_size_(data_size) {
    DCHECK_NOT_NULL(code);
    DCHECK_LT(0, code_size);
    DCHECK_NOT_NULL(data);
    DCHECK_LT(0, data_size);
  }

  const uint8_t* RawCode() const { return code_ + RawCodeOffset(); }

  const LayoutDescription& LayoutDescription(Builtin builtin) const {
    const struct LayoutDescription* descs =
        reinterpret_cast<const struct LayoutDescription*>(
            data_ + LayoutDescriptionTableOffset());
    return descs[static_cast<int>(builtin)];
  }
  const uint8_t* RawMetadata() const { return data_ + RawMetadataOffset(); }

  static constexpr int PadAndAlignCode(int size) {
    // Ensure we have at least one byte trailing the actual builtin
    // instructions which we can later fill with int3.
    return RoundUp<kCodeAlignment>(size + 1);
  }
  static constexpr int PadAndAlignData(int size) {
    // Ensure we have at least one byte trailing the actual builtin
    // instructions which we can later fill with int3.
    return RoundUp<Code::kMetadataAlignment>(size);
  }

  void PrintStatistics() const;

  // The code section contains instruction streams. It is guaranteed to have
  // execute permissions, and may have read permissions.
  const uint8_t* code_;
  uint32_t code_size_;

  // The data section contains both descriptions of the code section (hashes,
  // offsets, sizes) and metadata describing Code objects (see
  // Code::MetadataStart()). It is guaranteed to have read permissions.
  const uint8_t* data_;
  uint32_t data_size_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_EMBEDDED_EMBEDDED_DATA_H_
