// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "include/libplatform/libplatform.h"
#include "include/v8-initialization.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/module-decoder-impl.h"
#include "src/wasm/names-provider.h"
#include "src/wasm/string-builder-multiline.h"
#include "src/wasm/wasm-disassembler-impl.h"
#include "src/wasm/wasm-opcodes-inl.h"

#if V8_OS_POSIX
#include <unistd.h>
#endif

int PrintHelp(char** argv) {
  std::cerr << "Usage: Specify an action and a module in any order.\n"
            << "The action can be any of:\n"

            << " --help\n"
            << "     Print this help and exit.\n"

            << " --list-functions\n"
            << "     List functions in the given module\n"

            << " --section-stats\n"
            << "     Show information about sections in the given module\n"

            << " --instruction-stats\n"
            << "     Show information about instructions in the given module\n"

            << " --single-wat FUNC_INDEX\n"
            << "     Print function FUNC_INDEX in .wat format\n"

            << " --full-wat\n"
            << "     Print full module in .wat format\n"

            << " --single-hexdump FUNC_INDEX\n"
            << "     Print function FUNC_INDEX in annotated hex format\n"

            << " --full-hexdump\n"
            << "     Print full module in annotated hex format\n"

            << " --strip\n"
            << "     Dump the module, in binary format, without its Name"
            << " section (requires using -o as well)\n"

            << "\n"
            << " -o OUTFILE or --output OUTFILE\n"
            << "     Send output to OUTFILE instead of <stdout>\n";
  return 1;
}

namespace v8 {
namespace internal {
namespace wasm {

enum class OutputMode { kWat, kHexDump };
static constexpr char kHexChars[] = "0123456789abcdef";

char* PrintHexBytesCore(char* ptr, uint32_t num_bytes, const byte* start) {
  for (uint32_t i = 0; i < num_bytes; i++) {
    byte b = *(start + i);
    *(ptr++) = '0';
    *(ptr++) = 'x';
    *(ptr++) = kHexChars[b >> 4];
    *(ptr++) = kHexChars[b & 0xF];
    *(ptr++) = ',';
    *(ptr++) = ' ';
  }
  return ptr;
}

// Computes the number of decimal digits required to print {value}.
int GetNumDigits(uint32_t value) {
  int digits = 1;
  for (uint32_t compare = 10; value >= compare; compare *= 10) digits++;
  return digits;
}

class InstructionStatistics {
 public:
  void Record(WasmOpcode opcode, uint32_t size) {
    Entry& entry = entries[opcode];
    entry.opcode = opcode;
    entry.count++;
    entry.total_size += size;
  }

  void RecordImmediate(WasmOpcode opcode, int imm_value) {
    OpcodeImmediates& map = immediates[opcode];
    map[imm_value]++;
  }

  void RecordCodeSize(size_t chunk) { total_code_size_ += chunk; }

  void RecordLocals(uint32_t count, uint32_t size) {
    locals_count_ += count;
    locals_size_ += size;
  }

  void WriteTo(std::ostream& out) {
    // Sort by number of occurrences.
    std::vector<Entry> sorted;
    sorted.reserve(entries.size());
    for (const auto& e : entries) sorted.push_back(e.second);
    std::sort(sorted.begin(), sorted.end(),
              [](const Entry& a, const Entry& b) { return a.count > b.count; });

    // Prepare column widths.
    int longest_mnemo = 0;
    for (const Entry& e : sorted) {
      int s = static_cast<int>(strlen(WasmOpcodes::OpcodeName(e.opcode)));
      if (s > longest_mnemo) longest_mnemo = s;
    }
    constexpr int kSpacing = 2;
    longest_mnemo =
        std::max(longest_mnemo, static_cast<int>(strlen("Instruction"))) +
        kSpacing;
    uint32_t highest_count = sorted[0].count;
    int count_digits = GetNumDigits(highest_count);
    count_digits = std::max(count_digits, static_cast<int>(strlen("count")));

    // Print headline.
    out << std::setw(longest_mnemo) << std::left << "Instruction";
    out << std::setw(count_digits) << std::right << "count";
    out << std::setw(kSpacing) << " ";
    out << std::setw(8) << "tot.size";
    out << std::setw(kSpacing) << " ";
    out << std::setw(8) << "avg.size";
    out << std::setw(kSpacing) << " ";
    out << std::setw(8) << "% of code\n";

    // Print instruction counts.
    auto PrintLine = [&](const char* name, uint32_t count,
                         uint32_t total_size) {
      out << std::setw(longest_mnemo) << std::left << name;
      out << std::setw(count_digits) << std::right << count;
      out << std::setw(kSpacing) << " ";
      out << std::setw(8) << total_size;
      out << std::setw(kSpacing) << " ";
      out << std::fixed << std::setprecision(2) << std::setw(8)
          << static_cast<double>(total_size) / count;
      out << std::setw(kSpacing) << " ";
      out << std::fixed << std::setprecision(1) << std::setw(8)
          << 100.0 * total_size / this->total_code_size_ << "%\n";
    };
    for (const Entry& e : sorted) {
      PrintLine(WasmOpcodes::OpcodeName(e.opcode), e.count, e.total_size);
    }
    out << "\n";
    PrintLine("locals", locals_count_, locals_size_);

    // Print most common immediate values.
    for (const auto& imm : immediates) {
      WasmOpcode opcode = imm.first;
      out << "\nMost common immediates for " << WasmOpcodes::OpcodeName(opcode)
          << ":\n";
      std::vector<std::pair<int, int>> counts;
      counts.reserve(imm.second.size());
      for (const auto& pair : imm.second) {
        counts.push_back(std::make_pair(pair.first, pair.second));
      }
      std::sort(counts.begin(), counts.end(),
                [](const std::pair<int, uint32_t>& a,
                   const std::pair<int, uint32_t>& b) {
                  return a.second > b.second;
                });
      constexpr int kImmLen = 9;  // Length of "Immediate".
      int count_len = std::max(GetNumDigits(counts[0].second),
                               static_cast<int>(strlen("count")));
      // How many most-common values to show.
      size_t print_top = std::min(size_t{10}, counts.size());
      out << std::setw(kImmLen) << "Immediate";
      out << std::setw(kSpacing) << " ";
      out << std::setw(count_len) << "count"
          << "\n";
      for (size_t i = 0; i < print_top; i++) {
        out << std::setw(kImmLen) << counts[i].first;
        out << std::setw(kSpacing) << " ";
        out << std::setw(count_len) << counts[i].second << "\n";
      }
    }
  }

 private:
  struct Entry {
    WasmOpcode opcode;
    uint32_t count = 0;
    uint32_t total_size = 0;
  };

  // First: immediate value, second: count.
  using OpcodeImmediates = std::map<int, uint32_t>;

  std::unordered_map<WasmOpcode, Entry> entries;
  std::map<WasmOpcode, OpcodeImmediates> immediates;
  size_t total_code_size_ = 0;
  uint32_t locals_count_ = 0;
  uint32_t locals_size_ = 0;
};

// A variant of FunctionBodyDisassembler that can produce "annotated hex dump"
// format, e.g.:
//     0xfb, 0x07, 0x01,  // struct.new $type1
class ExtendedFunctionDis : public FunctionBodyDisassembler {
 public:
  ExtendedFunctionDis(Zone* zone, const WasmModule* module, uint32_t func_index,
                      WasmFeatures* detected, const FunctionSig* sig,
                      const byte* start, const byte* end, uint32_t offset,
                      NamesProvider* names)
      : FunctionBodyDisassembler(zone, module, func_index, detected, sig, start,
                                 end, offset, names) {}

  static constexpr uint32_t kWeDontCareAboutByteCodeOffsetsHere = 0;

  void HexDump(MultiLineStringBuilder& out, FunctionHeader include_header) {
    out_ = &out;
    if (!more()) return;  // Fuzzers...
    // Print header.
    if (include_header == kPrintHeader) {
      out << "  // func ";
      names_->PrintFunctionName(out, func_index_, NamesProvider::kDevTools);
      PrintSignatureOneLine(out, sig_, func_index_, names_, true,
                            NamesProvider::kIndexAsComment);
      out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
    }

    // Decode and print locals.
    uint32_t locals_length;
    InitializeLocalsFromSig();
    DecodeLocals(pc_, &locals_length);
    if (failed()) {
      // TODO(jkummerow): Better error handling.
      out << "Failed to decode locals";
      return;
    }
    uint32_t total_length = 0;
    uint32_t length;
    uint32_t entries = read_u32v<validate>(pc_, &length);
    PrintHexBytes(out, length, pc_, 4);
    out << " // " << entries << " entries in locals list";
    out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
    total_length += length;
    while (entries-- > 0) {
      uint32_t count_length;
      uint32_t count = read_u32v<validate>(pc_ + total_length, &count_length);
      uint32_t type_length;
      ValueType type = value_type_reader::read_value_type<validate>(
          this, pc_ + total_length + count_length, &type_length, nullptr,
          WasmFeatures::All());
      PrintHexBytes(out, count_length + type_length, pc_ + total_length, 4);
      out << " // " << count << (count != 1 ? " locals" : " local")
          << " of type ";
      names_->PrintValueType(out, type);
      out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
      total_length += count_length + type_length;
    }

    consume_bytes(locals_length);

    // Main loop.
    while (pc_ < end_) {
      WasmOpcode opcode = GetOpcode();
      current_opcode_ = opcode;  // Some immediates need to know this.
      StringBuilder immediates;
      uint32_t length = PrintImmediatesAndGetLength(immediates);
      PrintHexBytes(out, length, pc_, 4);
      if (opcode == kExprEnd) {
        out << " // end";
        if (label_stack_.size() > 0) {
          const LabelInfo& label = label_stack_.back();
          if (label.start != nullptr) {
            out << " ";
            out.write(label.start, label.length);
          }
          label_stack_.pop_back();
        }
      } else {
        out << " // " << WasmOpcodes::OpcodeName(opcode);
      }
      out.write(immediates.start(), immediates.length());
      if (opcode == kExprBlock || opcode == kExprIf || opcode == kExprLoop ||
          opcode == kExprTry) {
        label_stack_.emplace_back(out.line_number(), out.length(),
                                  label_occurrence_index_++);
      }
      out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
      pc_ += length;
    }

    if (pc_ != end_) {
      // TODO(jkummerow): Better error handling.
      out << "Beyond end of code\n";
    }
  }

  void HexdumpConstantExpression(MultiLineStringBuilder& out) {
    while (pc_ < end_) {
      WasmOpcode opcode = GetOpcode();
      current_opcode_ = opcode;  // Some immediates need to know this.
      StringBuilder immediates;
      uint32_t length = PrintImmediatesAndGetLength(immediates);
      // Don't print the final "end" separately.
      if (pc_ + length + 1 == end_ && *(pc_ + length) == kExprEnd) {
        length++;
      }
      PrintHexBytes(out, length, pc_, 4);
      out << " // " << WasmOpcodes::OpcodeName(opcode);
      out.write(immediates.start(), immediates.length());
      out.NextLine(kWeDontCareAboutByteCodeOffsetsHere);
      pc_ += length;
    }
  }

  void PrintHexBytes(StringBuilder& out, uint32_t num_bytes, const byte* start,
                     uint32_t fill_to_minimum = 0) {
    constexpr int kCharsPerByte = 6;  // Length of "0xFF, ".
    uint32_t max = std::max(num_bytes, fill_to_minimum) * kCharsPerByte + 2;
    char* ptr = out.allocate(max);
    *(ptr++) = ' ';
    *(ptr++) = ' ';
    ptr = PrintHexBytesCore(ptr, num_bytes, start);
    if (fill_to_minimum > num_bytes) {
      memset(ptr, ' ', (fill_to_minimum - num_bytes) * kCharsPerByte);
    }
  }

  void CollectInstructionStats(InstructionStatistics& stats) {
    uint32_t locals_length;
    DecodeLocals(pc_, &locals_length);
    if (failed()) return;
    stats.RecordLocals(num_locals(), locals_length);
    consume_bytes(locals_length);
    while (pc_ < end_) {
      WasmOpcode opcode = GetOpcode();
      if (opcode == kExprI32Const) {
        ImmI32Immediate<Decoder::kNoValidation> imm(this, pc_ + 1);
        stats.RecordImmediate(opcode, imm.value);
      } else if (opcode == kExprLocalGet || opcode == kExprGlobalGet) {
        IndexImmediate<Decoder::kNoValidation> imm(this, pc_ + 1, "");
        stats.RecordImmediate(opcode, static_cast<int>(imm.index));
      }
      uint32_t length = WasmDecoder::OpcodeLength(this, pc_);
      stats.Record(opcode, length);
      pc_ += length;
    }
  }
};

// A variant of ModuleDisassembler that produces "annotated hex dump" format,
// e.g.:
//     0x01, 0x70, 0x00,  // table count 1: funcref no maximum
class HexDumpModuleDis;
class DumpingModuleDecoder : public ModuleDecoderTemplate<HexDumpModuleDis> {
 public:
  DumpingModuleDecoder(const ModuleWireBytes wire_bytes,
                       HexDumpModuleDis* module_dis)
      : ModuleDecoderTemplate<HexDumpModuleDis>(
            WasmFeatures::All(), wire_bytes.start(), wire_bytes.end(),
            kWasmOrigin, *module_dis) {}

  void onFirstError() override {
    // Pretend we've reached the end of the section, but contrary to the
    // superclass implementation do so without moving {pc_}, so whatever
    // bytes caused the failure can still be dumped correctly.
    end_ = pc_;
  }
};
class HexDumpModuleDis {
 public:
  HexDumpModuleDis(MultiLineStringBuilder& out, const WasmModule* module,
                   NamesProvider* names, const ModuleWireBytes wire_bytes,
                   AccountingAllocator* allocator)
      : out_(out),
        module_(module),
        names_(names),
        wire_bytes_(wire_bytes),
        allocator_(allocator),
        zone_(allocator, "disassembler") {}

  // Public entrypoint.
  void PrintModule() {
    DumpingModuleDecoder decoder(wire_bytes_, this);
    decoder_ = &decoder;

    // If the module failed validation, create fakes to allow us to print
    // what we can.
    std::unique_ptr<WasmModule> fake_module;
    std::unique_ptr<NamesProvider> names_provider;
    if (!names_) {
      fake_module.reset(
          new WasmModule(std::make_unique<Zone>(allocator_, "fake module")));
      names_provider.reset(
          new NamesProvider(fake_module.get(), wire_bytes_.module_bytes()));
      names_ = names_provider.get();
    }

    out_ << "[";
    out_.NextLine(0);
    constexpr bool verify_functions = false;
    decoder.DecodeModule(nullptr, allocator_, verify_functions);
    out_ << "]";

    if (total_bytes_ != wire_bytes_.length()) {
      std::cerr << "WARNING: OUTPUT INCOMPLETE. Disassembled " << total_bytes_
                << " out of " << wire_bytes_.length() << " bytes.\n";
    }

    // For cleanliness, reset {names_} if it's pointing at a fake.
    if (names_ == names_provider.get()) {
      names_ = nullptr;
    }
  }

  // Tracer hooks.
  void Bytes(const byte* start, uint32_t count) {
    if (count > kMaxBytesPerLine) {
      DCHECK_EQ(queue_, nullptr);
      queue_ = start;
      queue_length_ = count;
      total_bytes_ += count;
      return;
    }
    if (line_bytes_ == 0) out_ << "  ";
    PrintHexBytes(out_, count, start);
    line_bytes_ += count;
    total_bytes_ += count;
  }

  void Description(const char* desc) { description_ << desc; }
  void Description(const char* desc, size_t length) {
    description_.write(desc, length);
  }
  void Description(uint32_t number) {
    if (description_.length() != 0) description_ << " ";
    description_ << number;
  }
  void Description(ValueType type) {
    if (description_.length() != 0) description_ << " ";
    names_->PrintValueType(description_, type);
  }
  void Description(HeapType type) {
    if (description_.length() != 0) description_ << " ";
    names_->PrintHeapType(description_, type);
  }
  void Description(const FunctionSig* sig) {
    PrintSignatureOneLine(description_, sig, 0 /* ignored */, names_, false);
  }
  void FunctionName(uint32_t func_index) {
    description_ << func_index << " ";
    names_->PrintFunctionName(description_, func_index,
                              NamesProvider::kDevTools);
  }

  void NextLineIfFull() {
    if (queue_ || line_bytes_ >= kPadBytes) NextLine();
  }
  void NextLineIfNonEmpty() {
    if (queue_ || line_bytes_ > 0) NextLine();
  }
  void NextLine() {
    if (queue_) {
      // Print queued hex bytes first, unless there have also been unqueued
      // bytes.
      if (line_bytes_ > 0) {
        // Keep the queued bytes together on the next line.
        for (; line_bytes_ < kPadBytes; line_bytes_++) {
          out_ << "      ";
        }
        out_ << " // ";
        out_.write(description_.start(), description_.length());
        out_.NextLine(kDontCareAboutOffsets);
      }
      while (queue_length_ > kMaxBytesPerLine) {
        out_ << "  ";
        PrintHexBytes(out_, kMaxBytesPerLine, queue_);
        out_.NextLine(kDontCareAboutOffsets);
        queue_length_ -= kMaxBytesPerLine;
        queue_ += kMaxBytesPerLine;
      }
      if (queue_length_ > 0) {
        out_ << "  ";
        PrintHexBytes(out_, queue_length_, queue_);
      }
      if (line_bytes_ == 0) {
        if (queue_length_ > kPadBytes) {
          out_.NextLine(kDontCareAboutOffsets);
          out_ << "                           // ";
        } else {
          for (uint32_t i = queue_length_; i < kPadBytes; i++) {
            out_ << "      ";
          }
          out_ << " // ";
        }
        out_.write(description_.start(), description_.length());
      }
      queue_ = nullptr;
    } else {
      // No queued bytes; just write the accumulated description.
      if (description_.length() != 0) {
        if (line_bytes_ == 0) out_ << "  ";
        for (; line_bytes_ < kPadBytes; line_bytes_++) {
          out_ << "      ";
        }
        out_ << " // ";
        out_.write(description_.start(), description_.length());
      }
    }
    out_.NextLine(kDontCareAboutOffsets);
    line_bytes_ = 0;
    description_.rewind_to_start();
  }

  // We don't care about offsets, but we can use these hooks to provide
  // helpful indexing comments in long lists.
  void TypeOffset(uint32_t offset) {
    if (!module_ || module_->types.size() > 3) {
      description_ << "type #" << next_type_index_ << " ";
      names_->PrintTypeName(description_, next_type_index_);
      next_type_index_++;
    }
  }
  void ImportOffset(uint32_t offset) {
    description_ << "import #" << next_import_index_++;
    NextLine();
  }
  void ImportsDone() {
    const WasmModule* module = decoder_->shared_module().get();
    next_table_index_ = static_cast<uint32_t>(module->tables.size());
    next_global_index_ = static_cast<uint32_t>(module->globals.size());
    next_tag_index_ = static_cast<uint32_t>(module->tags.size());
  }
  void TableOffset(uint32_t offset) {
    if (!module_ || module_->tables.size() > 3) {
      description_ << "table #" << next_table_index_++;
    }
  }
  void MemoryOffset(uint32_t offset) {}
  void TagOffset(uint32_t offset) {
    if (!module_ || module_->tags.size() > 3) {
      description_ << "tag #" << next_tag_index_++ << ":";
    }
  }
  void GlobalOffset(uint32_t offset) {
    description_ << "global #" << next_global_index_++ << ":";
  }
  void StartOffset(uint32_t offset) {}
  void ElementOffset(uint32_t offset) {
    if (!module_ || module_->elem_segments.size() > 3) {
      description_ << "segment #" << next_segment_index_++;
      NextLine();
    }
  }
  void DataOffset(uint32_t offset) {
    if (!module_ || module_->data_segments.size() > 3) {
      description_ << "data segment #" << next_data_segment_index_++;
      NextLine();
    }
  }

  // The following two hooks give us an opportunity to call the hex-dumping
  // function body disassembler for initializers and functions.
  void InitializerExpression(const byte* start, const byte* end,
                             ValueType expected_type) {
    WasmFeatures detected;
    auto sig = FixedSizeSignature<ValueType>::Returns(expected_type);
    uint32_t offset = decoder_->pc_offset();
    const WasmModule* module = module_;
    if (!module) module = decoder_->shared_module().get();
    ExtendedFunctionDis d(&zone_, module, 0, &detected, &sig, start, end,
                          offset, names_);
    d.HexdumpConstantExpression(out_);
    total_bytes_ += static_cast<size_t>(end - start);
  }

  void FunctionBody(const WasmFunction* func, const byte* start) {
    const byte* end = start + func->code.length();
    WasmFeatures detected;
    uint32_t offset = static_cast<uint32_t>(start - decoder_->start());
    const WasmModule* module = module_;
    if (!module) module = decoder_->shared_module().get();
    ExtendedFunctionDis d(&zone_, module, func->func_index, &detected,
                          func->sig, start, end, offset, names_);
    d.HexDump(out_, FunctionBodyDisassembler::kSkipHeader);
    total_bytes_ += func->code.length();
  }

  // We have to do extra work for the name section here, because the regular
  // decoder mostly just skips over it.
  void NameSection(const byte* start, const byte* end, uint32_t offset) {
    Decoder decoder(start, end, offset);
    while (decoder.ok() && decoder.more()) {
      uint8_t name_type = decoder.consume_u8("name type: ", *this);
      Description(NameTypeName(name_type));
      NextLine();
      uint32_t payload_length = decoder.consume_u32v("payload length:", *this);
      Description(payload_length);
      NextLine();
      if (!decoder.checkAvailable(payload_length)) break;
      switch (name_type) {
        case kModuleCode:
          consume_string(&decoder, unibrow::Utf8Variant::kLossyUtf8,
                         "module name", *this);
          break;
        case kFunctionCode:
        case kTypeCode:
        case kTableCode:
        case kMemoryCode:
        case kGlobalCode:
        case kElementSegmentCode:
        case kDataSegmentCode:
        case kTagCode:
          DumpNameMap(decoder);
          break;
        case kLocalCode:
        case kLabelCode:
        case kFieldCode:
          DumpIndirectNameMap(decoder);
          break;
        default:
          Bytes(decoder.pc(), payload_length);
          NextLine();
          decoder.consume_bytes(payload_length);
          break;
      }
    }
  }

 private:
  static constexpr uint32_t kDontCareAboutOffsets = 0;
  static constexpr uint32_t kMaxBytesPerLine = 8;
  static constexpr uint32_t kPadBytes = 4;

  void PrintHexBytes(StringBuilder& out, uint32_t num_bytes,
                     const byte* start) {
    char* ptr = out.allocate(num_bytes * 6);
    PrintHexBytesCore(ptr, num_bytes, start);
  }

  void DumpNameMap(Decoder& decoder) {
    uint32_t count = decoder.consume_u32v("names count", *this);
    Description(count);
    NextLine();
    for (uint32_t i = 0; i < count; i++) {
      uint32_t index = decoder.consume_u32v("index", *this);
      Description(index);
      Description(" ");
      consume_string(&decoder, unibrow::Utf8Variant::kLossyUtf8, "name", *this);
      if (!decoder.ok()) break;
    }
  }

  void DumpIndirectNameMap(Decoder& decoder) {
    uint32_t outer_count = decoder.consume_u32v("outer count", *this);
    Description(outer_count);
    NextLine();
    for (uint32_t i = 0; i < outer_count; i++) {
      uint32_t outer_index = decoder.consume_u32v("outer index", *this);
      Description(outer_index);
      uint32_t inner_count = decoder.consume_u32v(" inner count", *this);
      Description(inner_count);
      NextLine();
      for (uint32_t j = 0; j < inner_count; j++) {
        uint32_t inner_index = decoder.consume_u32v("inner index", *this);
        Description(inner_index);
        Description(" ");
        consume_string(&decoder, unibrow::Utf8Variant::kLossyUtf8, "name",
                       *this);
        if (!decoder.ok()) break;
      }
      if (!decoder.ok()) break;
    }
  }

  static constexpr const char* NameTypeName(uint8_t name_type) {
    switch (name_type) {
      // clang-format off
      case kModuleCode:         return "module";
      case kFunctionCode:       return "function";
      case kTypeCode:           return "type";
      case kTableCode:          return "table";
      case kMemoryCode:         return "memory";
      case kGlobalCode:         return "global";
      case kElementSegmentCode: return "element segment";
      case kDataSegmentCode:    return "data segment";
      case kTagCode:            return "tag";
      case kLocalCode:          return "local";
      case kLabelCode:          return "label";
      case kFieldCode:          return "field";
      default:                  return "unknown";
        // clang-format on
    }
  }
  MultiLineStringBuilder& out_;
  const WasmModule* module_;
  NamesProvider* names_;
  const ModuleWireBytes wire_bytes_;
  AccountingAllocator* allocator_;
  Zone zone_;

  StringBuilder description_;
  const byte* queue_{nullptr};
  uint32_t queue_length_{0};
  uint32_t line_bytes_{0};
  size_t total_bytes_{0};
  DumpingModuleDecoder* decoder_{nullptr};

  uint32_t next_type_index_{0};
  uint32_t next_import_index_{0};
  uint32_t next_table_index_{0};
  uint32_t next_global_index_{0};
  uint32_t next_tag_index_{0};
  uint32_t next_segment_index_{0};
  uint32_t next_data_segment_index_{0};
};

////////////////////////////////////////////////////////////////////////////////

class FormatConverter {
 public:
  enum Status { kNotReady, kIoInitialized, kModuleReady };

  explicit FormatConverter(const char* input, const char* output)
      : output_(output), out_(output_.get()) {
    if (!output_.ok()) return;
    if (!LoadFile(input)) return;
    base::Vector<const byte> wire_bytes(raw_bytes_.data(), raw_bytes_.size());
    wire_bytes_ = ModuleWireBytes({raw_bytes_.data(), raw_bytes_.size()});
    status_ = kIoInitialized;
    ModuleResult result =
        DecodeWasmModuleForDisassembler(start(), end(), &allocator_);
    if (result.failed()) {
      WasmError error = result.error();
      std::cerr << "Decoding error: " << error.message() << " at offset "
                << error.offset() << "\n";
      return;
    }
    status_ = kModuleReady;
    module_ = result.value();
    names_provider_ =
        std::make_unique<NamesProvider>(module_.get(), wire_bytes);
  }

  Status status() const { return status_; }

  void ListFunctions() {
    DCHECK_EQ(status_, kModuleReady);
    const WasmModule* m = module();
    uint32_t num_functions = static_cast<uint32_t>(m->functions.size());
    out_ << "There are " << num_functions << " functions ("
         << m->num_imported_functions << " imported, "
         << m->num_declared_functions
         << " locally defined); the following have names:\n";
    for (uint32_t i = 0; i < num_functions; i++) {
      StringBuilder sb;
      names()->PrintFunctionName(sb, i);
      if (sb.length() == 0) continue;
      std::string name(sb.start(), sb.length());
      out_ << i << " " << name << "\n";
    }
  }

  void SectionStats() {
    DCHECK_EQ(status_, kModuleReady);
    Decoder decoder(start(), end());
    decoder.consume_bytes(kModuleHeaderSize, "module header");

    uint32_t module_size = static_cast<uint32_t>(end() - start());
    int digits = GetNumDigits(module_size);
    size_t kMinNameLength = 8;
    // 18 = kMinNameLength + strlen(" section: ").
    out_ << std::setw(18) << std::left << "Module size: ";
    out_ << std::setw(digits) << std::right << module_size << " bytes\n";
    NoTracer no_tracer;
    for (WasmSectionIterator it(&decoder, no_tracer); it.more();
         it.advance(true)) {
      const char* name = SectionName(it.section_code());
      size_t name_len = strlen(name);
      out_ << SectionName(it.section_code()) << " section: ";
      for (; name_len < kMinNameLength; name_len++) out_ << " ";

      uint32_t length = it.section_length();
      out_ << std::setw(name_len > kMinNameLength ? 0 : digits) << length
           << " bytes / ";

      out_ << std::fixed << std::setprecision(1) << std::setw(4)
           << 100.0 * length / module_size;
      out_ << "% of total\n";
    }
  }

  void Strip() {
    DCHECK_EQ(status_, kModuleReady);
    Decoder decoder(start(), end());
    out_.write(reinterpret_cast<const char*>(decoder.pc()), kModuleHeaderSize);
    decoder.consume_bytes(kModuleHeaderSize);
    NoTracer no_tracer;
    for (WasmSectionIterator it(&decoder, no_tracer); it.more();
         it.advance(true)) {
      if (it.section_code() == kNameSectionCode) continue;
      out_.write(reinterpret_cast<const char*>(it.section_start()),
                 it.section_length());
    }
  }

  void InstructionStats() {
    DCHECK_EQ(status_, kModuleReady);
    Zone zone(&allocator_, "disassembler");
    InstructionStatistics stats;
    for (uint32_t i = module()->num_imported_functions;
         i < module()->functions.size(); i++) {
      const WasmFunction* func = &module()->functions[i];
      WasmFeatures detected;
      base::Vector<const byte> code = wire_bytes_.GetFunctionBytes(func);
      ExtendedFunctionDis d(&zone, module(), i, &detected, func->sig,
                            code.begin(), code.end(), func->code.offset(),
                            names());
      d.CollectInstructionStats(stats);
      stats.RecordCodeSize(code.size());
    }
    stats.WriteTo(out_);
  }

  void DisassembleFunction(uint32_t func_index, OutputMode mode) {
    DCHECK_EQ(status_, kModuleReady);
    MultiLineStringBuilder sb;
    if (func_index >= module()->functions.size()) {
      sb << "Invalid function index!\n";
      return;
    }
    if (func_index < module()->num_imported_functions) {
      sb << "Can't disassemble imported functions!\n";
      return;
    }
    const WasmFunction* func = &module()->functions[func_index];
    Zone zone(&allocator_, "disassembler");
    WasmFeatures detected;
    base::Vector<const byte> code = wire_bytes_.GetFunctionBytes(func);

    ExtendedFunctionDis d(&zone, module(), func_index, &detected, func->sig,
                          code.begin(), code.end(), func->code.offset(),
                          names());
    if (mode == OutputMode::kWat) {
      d.DecodeAsWat(sb, {0, 1});
    } else if (mode == OutputMode::kHexDump) {
      d.HexDump(sb, FunctionBodyDisassembler::kPrintHeader);
    }

    // Print any types that were used by the function.
    sb.NextLine(0);
    ModuleDisassembler md(sb, module(), names(), wire_bytes_, &allocator_);
    for (uint32_t type_index : d.used_types()) {
      md.PrintTypeDefinition(type_index, {0, 1},
                             NamesProvider::kIndexAsComment);
    }
    sb.WriteTo(out_);
  }

  void WatForModule() {
    DCHECK_EQ(status_, kModuleReady);
    MultiLineStringBuilder sb;
    ModuleDisassembler md(sb, module(), names(), wire_bytes_, &allocator_);
    md.PrintModule({0, 2});
    sb.WriteTo(out_);
  }

  void HexdumpForModule() {
    DCHECK_NE(status_, kNotReady);
    DCHECK_IMPLIES(status_ == kIoInitialized,
                   module() == nullptr && names() == nullptr);
    MultiLineStringBuilder sb;
    HexDumpModuleDis md(sb, module(), names(), wire_bytes_, &allocator_);
    md.PrintModule();
    sb.WriteTo(out_);
  }

 private:
  static constexpr int kModuleHeaderSize = 8;

  class Output {
   public:
    explicit Output(const char* filename) {
      if (strcmp(filename, "-") == 0) {
        mode_ = kStdout;
      } else {
        mode_ = kFile;
        filestream_.emplace(filename, std::ios::out | std::ios::binary);
        if (!filestream_->is_open()) {
          std::cerr << "Failed to open " << filename << " for writing!\n";
          mode_ = kError;
        }
      }
    }

    ~Output() {
      if (mode_ == kFile) filestream_->close();
    }

    bool ok() { return mode_ != kError; }

    std::ostream& get() {
      return mode_ == kFile ? filestream_.value() : std::cout;
    }

   private:
    enum Mode { kFile, kStdout, kError };
    base::Optional<std::ofstream> filestream_;
    Mode mode_;
  };

  bool LoadFile(std::string path) {
    if (path == "-") return LoadFileFromStream(std::cin);

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
      std::cerr << "Failed to open " << path << "!\n";
      return false;
    }
    return LoadFileFromStream(input);
  }

  bool LoadFileFromStream(std::istream& input) {
    int c0 = input.get();
    int c1 = input.get();
    int c2 = input.get();
    int c3 = input.peek();
    input.putback(c2);
    input.putback(c1);
    input.putback(c0);
    if (c0 == 0 && c1 == 'a' && c2 == 's' && c3 == 'm') {
      // Wasm binary module.
      raw_bytes_ = std::vector<byte>(std::istreambuf_iterator<char>(input), {});
      return true;
    }
    if (TryParseLiteral(input, raw_bytes_)) return true;
    std::cerr << "That's not a Wasm module!\n";
    return false;
  }

  bool IsWhitespace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v';
  }

  // Attempts to read a module in "array literal" syntax:
  // - Bytes must be separated by ',', may be specified in decimal or hex.
  // - The whole module must be enclosed in '[]', anything outside these
  //   braces is ignored.
  // - Whitespace, line comments, and block comments are ignored.
  // So in particular, this can consume what --full-hexdump produces.
  bool TryParseLiteral(std::istream& input, std::vector<byte>& output_bytes) {
    int c = input.get();
    // Skip anything before the first opening '['.
    while (c != '[' && c != EOF) c = input.get();
    enum State { kBeforeValue = 0, kAfterValue = 1, kDecimal = 10, kHex = 16 };
    State state = kBeforeValue;
    int value = 0;
    while (true) {
      c = input.get();
      // Skip whitespace, except inside values.
      if (state < kDecimal) {
        while (IsWhitespace(c)) c = input.get();
      }
      // End of file before ']' is unexpected = invalid.
      if (c == EOF) return false;
      // Skip comments.
      if (c == '/' && input.peek() == '/') {
        // Line comment. Skip until '\n'.
        do {
          c = input.get();
        } while (c != '\n' && c != EOF);
        continue;
      }
      if (c == '/' && input.peek() == '*') {
        // Block comment. Skip until "*/".
        input.get();  // Consume '*' of opening "/*".
        do {
          c = input.get();
          if (c == '*' && input.peek() == '/') {
            input.get();  // Consume '/'.
            break;
          }
        } while (c != EOF);
        continue;
      }
      if (state == kBeforeValue) {
        if (c == '0' && (input.peek() == 'x' || input.peek() == 'X')) {
          input.get();  // Consume the 'x'.
          state = kHex;
          continue;
        }
        if (c >= '0' && c <= '9') {
          state = kDecimal;
          // Fall through to handling kDecimal below.
        } else if (c == ']') {
          return true;
        } else {
          return false;
        }
      }
      DCHECK(state == kDecimal || state == kHex || state == kAfterValue);
      if (c == ',') {
        DCHECK_LT(value, 256);
        output_bytes.push_back(static_cast<byte>(value));
        state = kBeforeValue;
        value = 0;
        continue;
      }
      if (c == ']') {
        DCHECK_LT(value, 256);
        output_bytes.push_back(static_cast<byte>(value));
        return true;
      }
      if (state == kAfterValue) {
        // Didn't take the ',' or ']' paths above, anything else is invalid.
        DCHECK(c != ',' && c != ']');
        return false;
      }
      DCHECK(state == kDecimal || state == kHex);
      if (IsWhitespace(c)) {
        state = kAfterValue;
        continue;
      }
      int v;
      if (c >= '0' && c <= '9') {
        v = c - '0';
      } else if (state == kHex && (c | 0x20) >= 'a' && (c | 0x20) <= 'f') {
        // Setting the "0x20" bit maps uppercase onto lowercase letters.
        v = (c | 0x20) - 'a' + 10;
      } else {
        return false;
      }
      value = value * state + v;
      if (value > 0xFF) return false;
    }
  }

  byte* start() { return raw_bytes_.data(); }
  byte* end() { return start() + raw_bytes_.size(); }
  const WasmModule* module() { return module_.get(); }
  NamesProvider* names() { return names_provider_.get(); }

  AccountingAllocator allocator_;
  Output output_;
  std::ostream& out_;
  Status status_{kNotReady};
  std::vector<byte> raw_bytes_;
  ModuleWireBytes wire_bytes_{{}};
  std::shared_ptr<WasmModule> module_;
  std::unique_ptr<NamesProvider> names_provider_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

using FormatConverter = v8::internal::wasm::FormatConverter;
using OutputMode = v8::internal::wasm::OutputMode;
using MultiLineStringBuilder = v8::internal::wasm::MultiLineStringBuilder;

enum class Action {
  kUnset,
  kHelp,
  kListFunctions,
  kSectionStats,
  kInstructionStats,
  kFullWat,
  kFullHexdump,
  kSingleWat,
  kSingleHexdump,
  kStrip,
};

struct Options {
  const char* input = nullptr;
  const char* output = nullptr;
  Action action = Action::kUnset;
  int func_index = -1;
};

bool ParseInt(char* s, int* out) {
  char* end;
  if (s[0] == '\0') return false;
  errno = 0;
  long l = strtol(s, &end, 10);
  if (errno != 0 || *end != '\0' || l > std::numeric_limits<int>::max() ||
      l < std::numeric_limits<int>::min()) {
    return false;
  }
  *out = static_cast<int>(l);
  return true;
}

int ParseOptions(int argc, char** argv, Options* options) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ||
        strcmp(argv[i], "help") == 0) {
      options->action = Action::kHelp;
    } else if (strcmp(argv[i], "--list-functions") == 0) {
      options->action = Action::kListFunctions;
    } else if (strcmp(argv[i], "--section-stats") == 0) {
      options->action = Action::kSectionStats;
    } else if (strcmp(argv[i], "--instruction-stats") == 0) {
      options->action = Action::kInstructionStats;
    } else if (strcmp(argv[i], "--full-wat") == 0) {
      options->action = Action::kFullWat;
    } else if (strcmp(argv[i], "--full-hexdump") == 0) {
      options->action = Action::kFullHexdump;
    } else if (strcmp(argv[i], "--single-wat") == 0) {
      options->action = Action::kSingleWat;
      if (i == argc - 1 || !ParseInt(argv[++i], &options->func_index)) {
        return PrintHelp(argv);
      }
    } else if (strncmp(argv[i], "--single-wat=", 13) == 0) {
      options->action = Action::kSingleWat;
      if (!ParseInt(argv[i] + 13, &options->func_index)) return PrintHelp(argv);
    } else if (strcmp(argv[i], "--single-hexdump") == 0) {
      options->action = Action::kSingleHexdump;
      if (i == argc - 1 || !ParseInt(argv[++i], &options->func_index)) {
        return PrintHelp(argv);
      }
    } else if (strncmp(argv[i], "--single-hexdump=", 17) == 0) {
      if (!ParseInt(argv[i] + 17, &options->func_index)) return PrintHelp(argv);
    } else if (strcmp(argv[i], "--strip") == 0) {
      options->action = Action::kStrip;
    } else if (strcmp(argv[i], "-o") == 0) {
      if (i == argc - 1) return PrintHelp(argv);
      options->output = argv[++i];
    } else if (strncmp(argv[i], "-o=", 3) == 0) {
      options->output = argv[i] + 3;
    } else if (strcmp(argv[i], "--output") == 0) {
      if (i == argc - 1) return PrintHelp(argv);
      options->output = argv[++i];
    } else if (strncmp(argv[i], "--output=", 9) == 0) {
      options->output = argv[i] + 9;
    } else if (options->input != nullptr) {
      return PrintHelp(argv);
    } else {
      options->input = argv[i];
    }
  }

#if V8_OS_POSIX
  // When piping data into wami, specifying the input as "-" is optional.
  if (options->input == nullptr && !isatty(STDIN_FILENO)) {
    options->input = "-";
  }
#endif

  if (options->output == nullptr) {
    // Refuse to send binary data to the terminal.
    if (options->action == Action::kStrip) {
#if V8_OS_POSIX
      // Piping binary output to another command is okay.
      if (isatty(STDOUT_FILENO)) return PrintHelp(argv);
#else
      return PrintHelp(argv);
#endif
    }
    options->output = "-";  // Default output: stdout.
  }

  if (options->action == Action::kUnset || options->input == nullptr) {
    return PrintHelp(argv);
  }
  return 0;
}

int main(int argc, char** argv) {
  Options options;
  if (ParseOptions(argc, argv, &options) != 0) return 1;
  if (options.action == Action::kHelp) {
    PrintHelp(argv);
    return 0;
  }

  // Bootstrap the basics.
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  FormatConverter fc(options.input, options.output);
  if (fc.status() == FormatConverter::kNotReady) return 1;
  // Allow hex dumping invalid modules.
  if (fc.status() != FormatConverter::kModuleReady &&
      options.action != Action::kFullHexdump) {
    std::cerr << "Consider using --full-hexdump to learn more.\n";
    return 1;
  }
  switch (options.action) {
    case Action::kListFunctions:
      fc.ListFunctions();
      break;
    case Action::kSectionStats:
      fc.SectionStats();
      break;
    case Action::kInstructionStats:
      fc.InstructionStats();
      break;
    case Action::kSingleWat:
      fc.DisassembleFunction(options.func_index, OutputMode::kWat);
      break;
    case Action::kSingleHexdump:
      fc.DisassembleFunction(options.func_index, OutputMode::kHexDump);
      break;
    case Action::kFullWat:
      fc.WatForModule();
      break;
    case Action::kFullHexdump:
      fc.HexdumpForModule();
      break;
    case Action::kStrip:
      fc.Strip();
      break;
    case Action::kHelp:
    case Action::kUnset:
      UNREACHABLE();
  }

  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  return 0;
}
