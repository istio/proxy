// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-code-manager.h"

#include <algorithm>
#include <iomanip>
#include <numeric>

#include "src/base/atomicops.h"
#include "src/base/build_config.h"
#include "src/base/iterator.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"
#include "src/base/small-vector.h"
#include "src/base/string-format.h"
#include "src/base/vector.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/common/code-memory-access.h"
#include "src/common/globals.h"
#include "src/diagnostics/disassembler.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/objects-inl.h"
#include "src/snapshot/embedded/embedded-data-inl.h"
#include "src/utils/ostreams.h"
#include "src/wasm/code-space-access.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/names-provider.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-import-wrapper-cache.h"
#include "src/wasm/wasm-module-sourcemap.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-objects.h"

#if defined(V8_OS_WIN64)
#include "src/diagnostics/unwinding-info-win64.h"
#endif  // V8_OS_WIN64

#define TRACE_HEAP(...)                                       \
  do {                                                        \
    if (v8_flags.trace_wasm_native_heap) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace wasm {

using trap_handler::ProtectedInstructionData;

base::AddressRegion DisjointAllocationPool::Merge(
    base::AddressRegion new_region) {
  // Find the possible insertion position by identifying the first region whose
  // start address is not less than that of {new_region}. Since there cannot be
  // any overlap between regions, this also means that the start of {above} is
  // bigger or equal than the *end* of {new_region}.
  auto above = regions_.lower_bound(new_region);
  DCHECK(above == regions_.end() || above->begin() >= new_region.end());

  // Check whether to merge with {above}.
  if (above != regions_.end() && new_region.end() == above->begin()) {
    base::AddressRegion merged_region{new_region.begin(),
                                      new_region.size() + above->size()};
    DCHECK_EQ(merged_region.end(), above->end());
    // Check whether to also merge with the region below.
    if (above != regions_.begin()) {
      auto below = above;
      --below;
      if (below->end() == new_region.begin()) {
        merged_region = {below->begin(), below->size() + merged_region.size()};
        regions_.erase(below);
      }
    }
    auto insert_pos = regions_.erase(above);
    regions_.insert(insert_pos, merged_region);
    return merged_region;
  }

  // No element below, and not adjavent to {above}: insert and done.
  if (above == regions_.begin()) {
    regions_.insert(above, new_region);
    return new_region;
  }

  auto below = above;
  --below;
  // Consistency check:
  DCHECK(above == regions_.end() || below->end() < above->begin());

  // Adjacent to {below}: merge and done.
  if (below->end() == new_region.begin()) {
    base::AddressRegion merged_region{below->begin(),
                                      below->size() + new_region.size()};
    DCHECK_EQ(merged_region.end(), new_region.end());
    regions_.erase(below);
    regions_.insert(above, merged_region);
    return merged_region;
  }

  // Not adjacent to any existing region: insert between {below} and {above}.
  DCHECK_LT(below->end(), new_region.begin());
  regions_.insert(above, new_region);
  return new_region;
}

base::AddressRegion DisjointAllocationPool::Allocate(size_t size) {
  return AllocateInRegion(size,
                          {kNullAddress, std::numeric_limits<size_t>::max()});
}

base::AddressRegion DisjointAllocationPool::AllocateInRegion(
    size_t size, base::AddressRegion region) {
  // Get an iterator to the first contained region whose start address is not
  // smaller than the start address of {region}. Start the search from the
  // region one before that (the last one whose start address is smaller).
  auto it = regions_.lower_bound(region);
  if (it != regions_.begin()) --it;

  for (auto end = regions_.end(); it != end; ++it) {
    base::AddressRegion overlap = it->GetOverlap(region);
    if (size > overlap.size()) continue;
    base::AddressRegion ret{overlap.begin(), size};
    base::AddressRegion old = *it;
    auto insert_pos = regions_.erase(it);
    if (size == old.size()) {
      // We use the full region --> nothing to add back.
    } else if (ret.begin() == old.begin()) {
      // We return a region at the start --> shrink old region from front.
      regions_.insert(insert_pos, {old.begin() + size, old.size() - size});
    } else if (ret.end() == old.end()) {
      // We return a region at the end --> shrink remaining region.
      regions_.insert(insert_pos, {old.begin(), old.size() - size});
    } else {
      // We return something in the middle --> split the remaining region
      // (insert the region with smaller address first).
      regions_.insert(insert_pos, {old.begin(), ret.begin() - old.begin()});
      regions_.insert(insert_pos, {ret.end(), old.end() - ret.end()});
    }
    return ret;
  }
  return {};
}

Address WasmCode::constant_pool() const {
  if (v8_flags.enable_embedded_constant_pool) {
    if (constant_pool_offset_ < code_comments_offset_) {
      return instruction_start() + constant_pool_offset_;
    }
  }
  return kNullAddress;
}

Address WasmCode::handler_table() const {
  return instruction_start() + handler_table_offset_;
}

int WasmCode::handler_table_size() const {
  DCHECK_GE(constant_pool_offset_, handler_table_offset_);
  return static_cast<int>(constant_pool_offset_ - handler_table_offset_);
}

Address WasmCode::code_comments() const {
  return instruction_start() + code_comments_offset_;
}

int WasmCode::code_comments_size() const {
  DCHECK_GE(unpadded_binary_size_, code_comments_offset_);
  return static_cast<int>(unpadded_binary_size_ - code_comments_offset_);
}

std::unique_ptr<const byte[]> WasmCode::ConcatenateBytes(
    std::initializer_list<base::Vector<const byte>> vectors) {
  size_t total_size = 0;
  for (auto& vec : vectors) total_size += vec.size();
  // Use default-initialization (== no initialization).
  std::unique_ptr<byte[]> result{new byte[total_size]};
  byte* ptr = result.get();
  for (auto& vec : vectors) {
    if (vec.empty()) continue;  // Avoid nullptr in {memcpy}.
    memcpy(ptr, vec.begin(), vec.size());
    ptr += vec.size();
  }
  return result;
}

void WasmCode::RegisterTrapHandlerData() {
  DCHECK(!has_trap_handler_index());
  if (kind() != WasmCode::kWasmFunction) return;
  if (protected_instructions_size_ == 0) return;

  Address base = instruction_start();

  size_t size = instructions().size();
  auto protected_instruction_data = this->protected_instructions();
  const int index =
      RegisterHandlerData(base, size, protected_instruction_data.size(),
                          protected_instruction_data.begin());

  // TODO(eholk): if index is negative, fail.
  CHECK_LE(0, index);
  set_trap_handler_index(index);
  DCHECK(has_trap_handler_index());
}

bool WasmCode::ShouldBeLogged(Isolate* isolate) {
  // The return value is cached in {WasmEngine::IsolateData::log_codes}. Ensure
  // to call {WasmEngine::EnableCodeLogging} if this return value would change
  // for any isolate. Otherwise we might lose code events.
  return isolate->v8_file_logger()->is_listening_to_code_events() ||
         isolate->logger()->is_listening_to_code_events() ||
         isolate->is_profiling();
}

std::string WasmCode::DebugName() const {
  if (IsAnonymous()) {
    return "anonymous function";
  }

  ModuleWireBytes wire_bytes(native_module()->wire_bytes());
  const WasmModule* module = native_module()->module();
  WireBytesRef name_ref =
      module->lazily_generated_names.LookupFunctionName(wire_bytes, index());
  WasmName name = wire_bytes.GetNameOrNull(name_ref);
  std::string name_buffer;
  if (kind() == kWasmToJsWrapper) {
    name_buffer = "wasm-to-js:";
    size_t prefix_len = name_buffer.size();
    constexpr size_t kMaxSigLength = 128;
    name_buffer.resize(prefix_len + kMaxSigLength);
    const FunctionSig* sig = module->functions[index()].sig;
    size_t sig_length = PrintSignature(
        base::VectorOf(&name_buffer[prefix_len], kMaxSigLength), sig);
    name_buffer.resize(prefix_len + sig_length);
    // If the import has a name, also append that (separated by "-").
    if (!name.empty()) {
      name_buffer += '-';
      name_buffer.append(name.begin(), name.size());
    }
  } else if (name.empty()) {
    name_buffer.resize(32);
    name_buffer.resize(
        SNPrintF(base::VectorOf(&name_buffer.front(), name_buffer.size()),
                 "wasm-function[%d]", index()));
  } else {
    name_buffer.append(name.begin(), name.end());
  }
  return name_buffer;
}

void WasmCode::LogCode(Isolate* isolate, const char* source_url,
                       int script_id) const {
  DCHECK(ShouldBeLogged(isolate));
  if (IsAnonymous()) return;

  ModuleWireBytes wire_bytes(native_module_->wire_bytes());
  const WasmModule* module = native_module_->module();
  std::string fn_name = DebugName();
  WasmName name = base::VectorOf(fn_name);

  const WasmDebugSymbols& debug_symbols = module->debug_symbols;
  auto load_wasm_source_map = isolate->wasm_load_source_map_callback();
  auto source_map = native_module_->GetWasmSourceMap();
  if (!source_map && debug_symbols.type == WasmDebugSymbols::Type::SourceMap &&
      !debug_symbols.external_url.is_empty() && load_wasm_source_map) {
    WasmName external_url =
        wire_bytes.GetNameOrNull(debug_symbols.external_url);
    std::string external_url_string(external_url.data(), external_url.size());
    HandleScope scope(isolate);
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);
    Local<v8::String> source_map_str =
        load_wasm_source_map(v8_isolate, external_url_string.c_str());
    native_module_->SetWasmSourceMap(
        std::make_unique<WasmModuleSourceMap>(v8_isolate, source_map_str));
  }

  // Record source positions before adding code, otherwise when code is added,
  // there are no source positions to associate with the added code.
  if (!source_positions().empty()) {
    LOG_CODE_EVENT(isolate, WasmCodeLinePosInfoRecordEvent(instruction_start(),
                                                           source_positions()));
  }

  int code_offset = module->functions[index_].code.offset();
  PROFILE(isolate, CodeCreateEvent(LogEventListener::CodeTag::kFunction, this,
                                   name, source_url, code_offset, script_id));
}

void WasmCode::Validate() const {
  // The packing strategy for {tagged_parameter_slots} only works if both the
  // max number of parameters and their max combined stack slot usage fits into
  // their respective half of the result value.
  static_assert(wasm::kV8MaxWasmFunctionParams <
                std::numeric_limits<uint16_t>::max());
  static constexpr int kMaxSlotsPerParam = 4;  // S128 on 32-bit platforms.
  static_assert(wasm::kV8MaxWasmFunctionParams * kMaxSlotsPerParam <
                std::numeric_limits<uint16_t>::max());

#ifdef DEBUG
  // Scope for foreign WasmCode pointers.
  WasmCodeRefScope code_ref_scope;
  // We expect certain relocation info modes to never appear in {WasmCode}
  // objects or to be restricted to a small set of valid values. Hence the
  // iteration below does not use a mask, but visits all relocation data.
  for (RelocIterator it(instructions(), reloc_info(), constant_pool());
       !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    switch (mode) {
      case RelocInfo::WASM_CALL: {
        Address target = it.rinfo()->wasm_call_address();
        WasmCode* code = native_module_->Lookup(target);
        CHECK_NOT_NULL(code);
        CHECK_EQ(WasmCode::kJumpTable, code->kind());
        CHECK(code->contains(target));
        break;
      }
      case RelocInfo::WASM_STUB_CALL: {
        Address target = it.rinfo()->wasm_stub_call_address();
        WasmCode* code = native_module_->Lookup(target);
        CHECK_NOT_NULL(code);
        CHECK_EQ(WasmCode::kJumpTable, code->kind());
        CHECK(code->contains(target));
        break;
      }
      case RelocInfo::INTERNAL_REFERENCE:
      case RelocInfo::INTERNAL_REFERENCE_ENCODED: {
        Address target = it.rinfo()->target_internal_reference();
        CHECK(contains(target));
        break;
      }
      case RelocInfo::EXTERNAL_REFERENCE:
      case RelocInfo::CONST_POOL:
      case RelocInfo::VENEER_POOL:
        // These are OK to appear.
        break;
      default:
        FATAL("Unexpected mode: %d", mode);
    }
  }
#endif
}

void WasmCode::MaybePrint() const {
  // Determines whether flags want this code to be printed.
  bool function_index_matches =
      (!IsAnonymous() &&
       v8_flags.print_wasm_code_function_index == static_cast<int>(index()));
  if (v8_flags.print_code ||
      (kind() == kWasmFunction
           ? (v8_flags.print_wasm_code || function_index_matches)
           : v8_flags.print_wasm_stub_code.value())) {
    std::string name = DebugName();
    Print(name.c_str());
  }
}

void WasmCode::Print(const char* name) const {
  StdoutStream os;
  os << "--- WebAssembly code ---\n";
  Disassemble(name, os);
  if (native_module_->HasDebugInfo()) {
    if (auto* debug_side_table =
            native_module_->GetDebugInfo()->GetDebugSideTableIfExists(this)) {
      debug_side_table->Print(os);
    }
  }
  os << "--- End code ---\n";
}

void WasmCode::Disassemble(const char* name, std::ostream& os,
                           Address current_pc) const {
  if (name) os << "name: " << name << "\n";
  if (!IsAnonymous()) os << "index: " << index() << "\n";
  os << "kind: " << GetWasmCodeKindAsString(kind()) << "\n";
  if (kind() == kWasmFunction) {
    DCHECK(is_liftoff() || tier() == ExecutionTier::kTurbofan);
    const char* compiler =
        is_liftoff() ? (for_debugging() ? "Liftoff (debug)" : "Liftoff")
                     : "TurboFan";
    os << "compiler: " << compiler << "\n";
  }
  size_t padding = instructions().size() - unpadded_binary_size_;
  os << "Body (size = " << instructions().size() << " = "
     << unpadded_binary_size_ << " + " << padding << " padding)\n";

  int instruction_size = unpadded_binary_size_;
  if (constant_pool_offset_ < instruction_size) {
    instruction_size = constant_pool_offset_;
  }
  if (safepoint_table_offset_ && safepoint_table_offset_ < instruction_size) {
    instruction_size = safepoint_table_offset_;
  }
  if (handler_table_offset_ < instruction_size) {
    instruction_size = handler_table_offset_;
  }
  DCHECK_LT(0, instruction_size);

#ifdef ENABLE_DISASSEMBLER
  os << "Instructions (size = " << instruction_size << ")\n";
  Disassembler::Decode(nullptr, os, instructions().begin(),
                       instructions().begin() + instruction_size,
                       CodeReference(this), current_pc);
  os << "\n";

  if (handler_table_size() > 0) {
    HandlerTable table(this);
    os << "Exception Handler Table (size = " << table.NumberOfReturnEntries()
       << "):\n";
    table.HandlerTableReturnPrint(os);
    os << "\n";
  }

  if (protected_instructions_size_ > 0) {
    os << "Protected instructions:\n pc offset  land pad\n";
    for (auto& data : protected_instructions()) {
      os << std::setw(10) << std::hex << data.instr_offset << std::setw(10)
         << std::hex << data.landing_offset << "\n";
    }
    os << "\n";
  }

  if (!source_positions().empty()) {
    os << "Source positions:\n pc offset  position\n";
    for (SourcePositionTableIterator it(source_positions()); !it.done();
         it.Advance()) {
      os << std::setw(10) << std::hex << it.code_offset() << std::dec
         << std::setw(10) << it.source_position().ScriptOffset()
         << (it.is_statement() ? "  statement" : "") << "\n";
    }
    os << "\n";
  }

  if (safepoint_table_offset_ > 0) {
    SafepointTable table(this);
    table.Print(os);
    os << "\n";
  }

  os << "RelocInfo (size = " << reloc_info().size() << ")\n";
  for (RelocIterator it(instructions(), reloc_info(), constant_pool());
       !it.done(); it.next()) {
    it.rinfo()->Print(nullptr, os);
  }
  os << "\n";
#else   // !ENABLE_DISASSEMBLER
  os << "Instructions (size = " << instruction_size << ", "
     << static_cast<void*>(instructions().begin()) << "-"
     << static_cast<void*>(instructions().begin() + instruction_size) << ")\n";
#endif  // !ENABLE_DISASSEMBLER
}

const char* GetWasmCodeKindAsString(WasmCode::Kind kind) {
  switch (kind) {
    case WasmCode::kWasmFunction:
      return "wasm function";
    case WasmCode::kWasmToCapiWrapper:
      return "wasm-to-capi";
    case WasmCode::kWasmToJsWrapper:
      return "wasm-to-js";
    case WasmCode::kJumpTable:
      return "jump table";
  }
  return "unknown kind";
}

WasmCode::~WasmCode() {
  if (has_trap_handler_index()) {
    trap_handler::ReleaseHandlerData(trap_handler_index());
  }
}

V8_WARN_UNUSED_RESULT bool WasmCode::DecRefOnPotentiallyDeadCode() {
  if (GetWasmEngine()->AddPotentiallyDeadCode(this)) {
    // The code just became potentially dead. The ref count we wanted to
    // decrement is now transferred to the set of potentially dead code, and
    // will be decremented when the next GC is run.
    return false;
  }
  // If we reach here, the code was already potentially dead. Decrement the ref
  // count, and return true if it drops to zero.
  return DecRefOnDeadCode();
}

// static
void WasmCode::DecrementRefCount(base::Vector<WasmCode* const> code_vec) {
  // Decrement the ref counter of all given code objects. Keep the ones whose
  // ref count drops to zero.
  WasmEngine::DeadCodeMap dead_code;
  for (WasmCode* code : code_vec) {
    if (!code->DecRef()) continue;  // Remaining references.
    dead_code[code->native_module()].push_back(code);
  }

  if (dead_code.empty()) return;

  GetWasmEngine()->FreeDeadCode(dead_code);
}

int WasmCode::GetSourcePositionBefore(int offset) {
  int position = kNoSourcePosition;
  for (SourcePositionTableIterator iterator(source_positions());
       !iterator.done() && iterator.code_offset() < offset;
       iterator.Advance()) {
    position = iterator.source_position().ScriptOffset();
  }
  return position;
}

// static
constexpr size_t WasmCodeAllocator::kMaxCodeSpaceSize;

WasmCodeAllocator::WasmCodeAllocator(std::shared_ptr<Counters> async_counters)
    : protect_code_memory_(!V8_HAS_PTHREAD_JIT_WRITE_PROTECT &&
                           v8_flags.wasm_write_protect_code_memory &&
                           !WasmCodeManager::MemoryProtectionKeysEnabled()),
      async_counters_(std::move(async_counters)) {
  owned_code_space_.reserve(4);
}

WasmCodeAllocator::~WasmCodeAllocator() {
  GetWasmCodeManager()->FreeNativeModule(base::VectorOf(owned_code_space_),
                                         committed_code_space());
}

void WasmCodeAllocator::Init(VirtualMemory code_space) {
  DCHECK(owned_code_space_.empty());
  DCHECK(free_code_space_.IsEmpty());
  free_code_space_.Merge(code_space.region());
  owned_code_space_.emplace_back(std::move(code_space));
  async_counters_->wasm_module_num_code_spaces()->AddSample(1);
}

namespace {
// On Windows, we cannot commit a region that straddles different reservations
// of virtual memory. Because we bump-allocate, and because, if we need more
// memory, we append that memory at the end of the owned_code_space_ list, we
// traverse that list in reverse order to find the reservation(s) that guide how
// to chunk the region to commit.
#if V8_OS_WIN
constexpr bool kNeedsToSplitRangeByReservations = true;
#else
constexpr bool kNeedsToSplitRangeByReservations = false;
#endif

base::SmallVector<base::AddressRegion, 1> SplitRangeByReservationsIfNeeded(
    base::AddressRegion range,
    const std::vector<VirtualMemory>& owned_code_space) {
  if (!kNeedsToSplitRangeByReservations) return {range};

  base::SmallVector<base::AddressRegion, 1> split_ranges;
  size_t missing_begin = range.begin();
  size_t missing_end = range.end();
  for (auto& vmem : base::Reversed(owned_code_space)) {
    Address overlap_begin = std::max(missing_begin, vmem.address());
    Address overlap_end = std::min(missing_end, vmem.end());
    if (overlap_begin >= overlap_end) continue;
    split_ranges.emplace_back(overlap_begin, overlap_end - overlap_begin);
    // Opportunistically reduce the missing range. This might terminate the loop
    // early.
    if (missing_begin == overlap_begin) missing_begin = overlap_end;
    if (missing_end == overlap_end) missing_end = overlap_begin;
    if (missing_begin >= missing_end) break;
  }
#ifdef ENABLE_SLOW_DCHECKS
  // The returned vector should cover the full range.
  size_t total_split_size = 0;
  for (auto split : split_ranges) total_split_size += split.size();
  DCHECK_EQ(range.size(), total_split_size);
#endif
  return split_ranges;
}

int NumWasmFunctionsInFarJumpTable(uint32_t num_declared_functions) {
  return NativeModule::kNeedsFarJumpsBetweenCodeSpaces
             ? static_cast<int>(num_declared_functions)
             : 0;
}

// Returns an overapproximation of the code size overhead per new code space
// created by the jump tables.
size_t OverheadPerCodeSpace(uint32_t num_declared_functions) {
  // Overhead for the jump table.
  size_t overhead = RoundUp<kCodeAlignment>(
      JumpTableAssembler::SizeForNumberOfSlots(num_declared_functions));

#if defined(V8_OS_WIN64)
  // On Win64, we need to reserve some pages at the beginning of an executable
  // space. See {AddCodeSpace}.
  overhead += Heap::GetCodeRangeReservedAreaSize();
#endif  // V8_OS_WIN64

  // Overhead for the far jump table.
  overhead +=
      RoundUp<kCodeAlignment>(JumpTableAssembler::SizeForNumberOfFarJumpSlots(
          WasmCode::kRuntimeStubCount,
          NumWasmFunctionsInFarJumpTable(num_declared_functions)));

  return overhead;
}

// Returns an estimate how much code space should be reserved.
size_t ReservationSize(size_t code_size_estimate, int num_declared_functions,
                       size_t total_reserved) {
  size_t overhead = OverheadPerCodeSpace(num_declared_functions);

  // Reserve the maximum of
  //   a) needed size + overhead (this is the minimum needed)
  //   b) 2 * overhead (to not waste too much space by overhead)
  //   c) 1/4 of current total reservation size (to grow exponentially)
  size_t minimum_size = 2 * overhead;
  size_t suggested_size =
      std::max(std::max(RoundUp<kCodeAlignment>(code_size_estimate) + overhead,
                        minimum_size),
               total_reserved / 4);

  if (V8_UNLIKELY(minimum_size > WasmCodeAllocator::kMaxCodeSpaceSize)) {
    auto oom_detail = base::FormattedString{}
                      << "required reservation minimum (" << minimum_size
                      << ") is bigger than supported maximum ("
                      << WasmCodeAllocator::kMaxCodeSpaceSize << ")";
    V8::FatalProcessOutOfMemory(nullptr,
                                "Exceeding maximum wasm code space size",
                                oom_detail.PrintToArray().data());
    UNREACHABLE();
  }

  // Limit by the maximum supported code space size.
  size_t reserve_size =
      std::min(WasmCodeAllocator::kMaxCodeSpaceSize, suggested_size);

  return reserve_size;
}

#ifdef DEBUG
// Check postconditions when returning from this method:
// 1) {region} must be fully contained in {writable_memory_};
// 2) {writable_memory_} must be a maximally merged ordered set of disjoint
//    non-empty regions.
class CheckWritableMemoryRegions {
 public:
  CheckWritableMemoryRegions(
      std::set<base::AddressRegion, base::AddressRegion::StartAddressLess>&
          writable_memory,
      base::AddressRegion new_region, size_t& new_writable_memory)
      : writable_memory_(writable_memory),
        new_region_(new_region),
        new_writable_memory_(new_writable_memory),
        old_writable_size_(std::accumulate(
            writable_memory_.begin(), writable_memory_.end(), size_t{0},
            [](size_t old, base::AddressRegion region) {
              return old + region.size();
            })) {}

  ~CheckWritableMemoryRegions() {
    // {new_region} must be contained in {writable_memory_}.
    DCHECK(std::any_of(
        writable_memory_.begin(), writable_memory_.end(),
        [this](auto region) { return region.contains(new_region_); }));

    // The new total size of writable memory must have increased by
    // {new_writable_memory}.
    size_t total_writable_size = std::accumulate(
        writable_memory_.begin(), writable_memory_.end(), size_t{0},
        [](size_t old, auto region) { return old + region.size(); });
    DCHECK_EQ(old_writable_size_ + new_writable_memory_, total_writable_size);

    // There are no empty regions.
    DCHECK(std::none_of(writable_memory_.begin(), writable_memory_.end(),
                        [](auto region) { return region.is_empty(); }));

    // Regions are sorted and disjoint. (std::accumulate has nodiscard on msvc
    // so USE is required to prevent build failures in debug builds).
    USE(std::accumulate(writable_memory_.begin(), writable_memory_.end(),
                        Address{0}, [](Address previous_end, auto region) {
                          DCHECK_LT(previous_end, region.begin());
                          return region.end();
                        }));
  }

 private:
  const std::set<base::AddressRegion, base::AddressRegion::StartAddressLess>&
      writable_memory_;
  const base::AddressRegion new_region_;
  const size_t& new_writable_memory_;
  const size_t old_writable_size_;
};
#else   // !DEBUG
class CheckWritableMemoryRegions {
 public:
  template <typename... Args>
  explicit CheckWritableMemoryRegions(Args...) {}
};
#endif  // !DEBUG

// Sentinel value to be used for {AllocateForCodeInRegion} for specifying no
// restriction on the region to allocate in.
constexpr base::AddressRegion kUnrestrictedRegion{
    kNullAddress, std::numeric_limits<size_t>::max()};

}  // namespace

base::Vector<byte> WasmCodeAllocator::AllocateForCode(
    NativeModule* native_module, size_t size) {
  return AllocateForCodeInRegion(native_module, size, kUnrestrictedRegion);
}

base::Vector<byte> WasmCodeAllocator::AllocateForCodeInRegion(
    NativeModule* native_module, size_t size, base::AddressRegion region) {
  DCHECK_LT(0, size);
  auto* code_manager = GetWasmCodeManager();
  size = RoundUp<kCodeAlignment>(size);
  base::AddressRegion code_space =
      free_code_space_.AllocateInRegion(size, region);
  if (V8_UNLIKELY(code_space.is_empty())) {
    // Only allocations without a specific region are allowed to fail. Otherwise
    // the region must have been allocated big enough to hold all initial
    // allocations (jump tables etc).
    CHECK_EQ(kUnrestrictedRegion, region);

    Address hint = owned_code_space_.empty() ? kNullAddress
                                             : owned_code_space_.back().end();

    size_t total_reserved = 0;
    for (auto& vmem : owned_code_space_) total_reserved += vmem.size();
    size_t reserve_size = ReservationSize(
        size, native_module->module()->num_declared_functions, total_reserved);
    VirtualMemory new_mem =
        code_manager->TryAllocate(reserve_size, reinterpret_cast<void*>(hint));
    if (!new_mem.IsReserved()) {
      auto oom_detail = base::FormattedString{}
                        << "cannot allocate more code space (" << reserve_size
                        << " bytes, currently " << total_reserved << ")";
      V8::FatalProcessOutOfMemory(nullptr, "Grow wasm code space",
                                  oom_detail.PrintToArray().data());
      UNREACHABLE();
    }

    base::AddressRegion new_region = new_mem.region();
    code_manager->AssignRange(new_region, native_module);
    free_code_space_.Merge(new_region);
    owned_code_space_.emplace_back(std::move(new_mem));
    native_module->AddCodeSpaceLocked(new_region);

    code_space = free_code_space_.Allocate(size);
    DCHECK(!code_space.is_empty());
    async_counters_->wasm_module_num_code_spaces()->AddSample(
        static_cast<int>(owned_code_space_.size()));
  }
  const Address commit_page_size = CommitPageSize();
  Address commit_start = RoundUp(code_space.begin(), commit_page_size);
  if (commit_start != code_space.begin()) {
    MakeWritable({commit_start - commit_page_size, commit_page_size});
  }

  Address commit_end = RoundUp(code_space.end(), commit_page_size);
  // {commit_start} will be either code_space.start or the start of the next
  // page. {commit_end} will be the start of the page after the one in which
  // the allocation ends.
  // We start from an aligned start, and we know we allocated vmem in
  // page multiples.
  // We just need to commit what's not committed. The page in which we
  // start is already committed (or we start at the beginning of a page).
  // The end needs to be committed all through the end of the page.
  if (commit_start < commit_end) {
    for (base::AddressRegion split_range : SplitRangeByReservationsIfNeeded(
             {commit_start, commit_end - commit_start}, owned_code_space_)) {
      code_manager->Commit(split_range);
    }
    committed_code_space_.fetch_add(commit_end - commit_start);
    // Committed code cannot grow bigger than maximum code space size.
    DCHECK_LE(committed_code_space_.load(), v8_flags.wasm_max_code_space * MB);
    if (protect_code_memory_) {
      DCHECK_LT(0, writers_count_);
      InsertIntoWritableRegions({commit_start, commit_end - commit_start},
                                false);
    }
  }
  DCHECK(IsAligned(code_space.begin(), kCodeAlignment));
  allocated_code_space_.Merge(code_space);
  generated_code_size_.fetch_add(code_space.size(), std::memory_order_relaxed);

  TRACE_HEAP("Code alloc for %p: 0x%" PRIxPTR ",+%zu\n", this,
             code_space.begin(), size);
  return {reinterpret_cast<byte*>(code_space.begin()), code_space.size()};
}

// TODO(dlehmann): Ensure that {AddWriter()} is always paired up with a
// {RemoveWriter}, such that eventually the code space is write protected.
// One solution is to make the API foolproof by hiding {SetWritable()} and
// allowing change of permissions only through {CodeSpaceWriteScope}.
// TODO(dlehmann): Add tests that ensure the code space is eventually write-
// protected.
void WasmCodeAllocator::AddWriter() {
  DCHECK(protect_code_memory_);
  ++writers_count_;
}

void WasmCodeAllocator::RemoveWriter() {
  DCHECK(protect_code_memory_);
  DCHECK_GT(writers_count_, 0);
  if (--writers_count_ > 0) return;

  // Switch all memory to non-writable.
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  for (base::AddressRegion writable : writable_memory_) {
    for (base::AddressRegion split_range :
         SplitRangeByReservationsIfNeeded(writable, owned_code_space_)) {
      TRACE_HEAP("Set 0x%" V8PRIxPTR ":0x%" V8PRIxPTR " to RX\n",
                 split_range.begin(), split_range.end());
      CHECK(SetPermissions(page_allocator, split_range.begin(),
                           split_range.size(), PageAllocator::kReadExecute));
    }
  }
  writable_memory_.clear();
}

void WasmCodeAllocator::MakeWritable(base::AddressRegion region) {
  if (!protect_code_memory_) return;
  DCHECK_LT(0, writers_count_);
  DCHECK(!region.is_empty());
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();

  // Align to commit page size.
  size_t commit_page_size = page_allocator->CommitPageSize();
  DCHECK(base::bits::IsPowerOfTwo(commit_page_size));
  Address begin = RoundDown(region.begin(), commit_page_size);
  Address end = RoundUp(region.end(), commit_page_size);
  region = base::AddressRegion(begin, end - begin);

  InsertIntoWritableRegions(region, true);
}

void WasmCodeAllocator::FreeCode(base::Vector<WasmCode* const> codes) {
  // Zap code area and collect freed code regions.
  DisjointAllocationPool freed_regions;
  size_t code_size = 0;
  for (WasmCode* code : codes) {
    code_size += code->instructions().size();
    freed_regions.Merge(base::AddressRegion{code->instruction_start(),
                                            code->instructions().size()});
  }
  freed_code_size_.fetch_add(code_size);

  // Merge {freed_regions} into {freed_code_space_} and put all ranges of full
  // pages to decommit into {regions_to_decommit} (decommitting is expensive,
  // so try to merge regions before decommitting).
  DisjointAllocationPool regions_to_decommit;
  size_t commit_page_size = CommitPageSize();
  for (auto region : freed_regions.regions()) {
    auto merged_region = freed_code_space_.Merge(region);
    Address discard_start =
        std::max(RoundUp(merged_region.begin(), commit_page_size),
                 RoundDown(region.begin(), commit_page_size));
    Address discard_end =
        std::min(RoundDown(merged_region.end(), commit_page_size),
                 RoundUp(region.end(), commit_page_size));
    if (discard_start >= discard_end) continue;
    regions_to_decommit.Merge({discard_start, discard_end - discard_start});
  }

  auto* code_manager = GetWasmCodeManager();
  for (auto region : regions_to_decommit.regions()) {
    size_t old_committed = committed_code_space_.fetch_sub(region.size());
    DCHECK_GE(old_committed, region.size());
    USE(old_committed);
    for (base::AddressRegion split_range :
         SplitRangeByReservationsIfNeeded(region, owned_code_space_)) {
      code_manager->Decommit(split_range);
    }
  }
}

size_t WasmCodeAllocator::GetNumCodeSpaces() const {
  return owned_code_space_.size();
}

void WasmCodeAllocator::InsertIntoWritableRegions(base::AddressRegion region,
                                                  bool switch_to_writable) {
  size_t new_writable_memory = 0;

  CheckWritableMemoryRegions check_on_return{writable_memory_, region,
                                             new_writable_memory};

  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  // Subroutine to make a non-writable region writable (if {switch_to_writable}
  // is {true}) and insert it into {writable_memory_}.
  auto make_writable = [&](decltype(writable_memory_)::iterator insert_pos,
                           base::AddressRegion region) {
    new_writable_memory += region.size();
    if (switch_to_writable) {
      for (base::AddressRegion split_range :
           SplitRangeByReservationsIfNeeded(region, owned_code_space_)) {
        TRACE_HEAP("Set 0x%" V8PRIxPTR ":0x%" V8PRIxPTR " to RWX\n",
                   split_range.begin(), split_range.end());
        CHECK(SetPermissions(page_allocator, split_range.begin(),
                             split_range.size(),
                             PageAllocator::kReadWriteExecute));
      }
    }

    // Insert {region} into {writable_memory_} before {insert_pos}, potentially
    // merging it with the surrounding regions.
    if (insert_pos != writable_memory_.begin()) {
      auto previous = insert_pos;
      --previous;
      if (previous->end() == region.begin()) {
        region = {previous->begin(), previous->size() + region.size()};
        writable_memory_.erase(previous);
      }
    }
    if (insert_pos != writable_memory_.end() &&
        region.end() == insert_pos->begin()) {
      region = {region.begin(), insert_pos->size() + region.size()};
      insert_pos = writable_memory_.erase(insert_pos);
    }
    writable_memory_.insert(insert_pos, region);
  };

  DCHECK(!region.is_empty());
  // Find a possible insertion position by identifying the first region whose
  // start address is not less than that of {new_region}, and the starting the
  // merge from the existing region before that.
  auto it = writable_memory_.lower_bound(region);
  if (it != writable_memory_.begin()) --it;
  for (;; ++it) {
    if (it == writable_memory_.end() || it->begin() >= region.end()) {
      // No overlap; add before {it}.
      make_writable(it, region);
      return;
    }
    if (it->end() <= region.begin()) continue;  // Continue after {it}.
    base::AddressRegion overlap = it->GetOverlap(region);
    DCHECK(!overlap.is_empty());
    if (overlap.begin() == region.begin()) {
      if (overlap.end() == region.end()) return;  // Fully contained already.
      // Remove overlap (which is already writable) and continue.
      region = {overlap.end(), region.end() - overlap.end()};
      continue;
    }
    if (overlap.end() == region.end()) {
      // Remove overlap (which is already writable), then make the remaining
      // region writable.
      region = {region.begin(), overlap.begin() - region.begin()};
      make_writable(it, region);
      return;
    }
    // Split {region}, make the split writable, and continue with the rest.
    base::AddressRegion split = {region.begin(),
                                 overlap.begin() - region.begin()};
    make_writable(it, split);
    region = {overlap.end(), region.end() - overlap.end()};
  }
}

namespace {
BoundsCheckStrategy GetBoundsChecks(const WasmModule* module) {
  if (!v8_flags.wasm_bounds_checks) return kNoBoundsChecks;
  if (v8_flags.wasm_enforce_bounds_checks) return kExplicitBoundsChecks;
  // We do not have trap handler support for memory64 yet.
  if (module->is_memory64) return kExplicitBoundsChecks;
  if (trap_handler::IsTrapHandlerEnabled()) return kTrapHandler;
  return kExplicitBoundsChecks;
}
}  // namespace

NativeModule::NativeModule(const WasmFeatures& enabled,
                           DynamicTiering dynamic_tiering,
                           VirtualMemory code_space,
                           std::shared_ptr<const WasmModule> module,
                           std::shared_ptr<Counters> async_counters,
                           std::shared_ptr<NativeModule>* shared_this)
    : engine_scope_(
          GetWasmEngine()->GetBarrierForBackgroundCompile()->TryLock()),
      code_allocator_(async_counters),
      enabled_features_(enabled),
      module_(std::move(module)),
      import_wrapper_cache_(std::unique_ptr<WasmImportWrapperCache>(
          new WasmImportWrapperCache())),
      bounds_checks_(GetBoundsChecks(module_.get())) {
  DCHECK(engine_scope_);
  // We receive a pointer to an empty {std::shared_ptr}, and install ourselve
  // there.
  DCHECK_NOT_NULL(shared_this);
  DCHECK_NULL(*shared_this);
  shared_this->reset(this);
  compilation_state_ = CompilationState::New(
      *shared_this, std::move(async_counters), dynamic_tiering);
  compilation_state_->InitCompileJob();
  DCHECK_NOT_NULL(module_);
  if (module_->num_declared_functions > 0) {
    code_table_ =
        std::make_unique<WasmCode*[]>(module_->num_declared_functions);
    tiering_budgets_ =
        std::make_unique<uint32_t[]>(module_->num_declared_functions);

    std::fill_n(tiering_budgets_.get(), module_->num_declared_functions,
                v8_flags.wasm_tiering_budget);
  }
  // Even though there cannot be another thread using this object (since we are
  // just constructing it), we need to hold the mutex to fulfill the
  // precondition of {WasmCodeAllocator::Init}, which calls
  // {NativeModule::AddCodeSpaceLocked}.
  base::RecursiveMutexGuard guard{&allocation_mutex_};
  auto initial_region = code_space.region();
  code_allocator_.Init(std::move(code_space));
  AddCodeSpaceLocked(initial_region);
}

void NativeModule::ReserveCodeTableForTesting(uint32_t max_functions) {
  WasmCodeRefScope code_ref_scope;
  DCHECK_LE(module_->num_declared_functions, max_functions);
  auto new_table = std::make_unique<WasmCode*[]>(max_functions);
  if (module_->num_declared_functions > 0) {
    memcpy(new_table.get(), code_table_.get(),
           module_->num_declared_functions * sizeof(WasmCode*));
  }
  code_table_ = std::move(new_table);

  base::AddressRegion single_code_space_region;
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  CHECK_EQ(1, code_space_data_.size());
  single_code_space_region = code_space_data_[0].region;
  // Re-allocate jump table.
  main_jump_table_ = CreateEmptyJumpTableInRegionLocked(
      JumpTableAssembler::SizeForNumberOfSlots(max_functions),
      single_code_space_region);
  code_space_data_[0].jump_table = main_jump_table_;
}

void NativeModule::LogWasmCodes(Isolate* isolate, Script script) {
  DisallowGarbageCollection no_gc;
  if (!WasmCode::ShouldBeLogged(isolate)) return;

  TRACE_EVENT1("v8.wasm", "wasm.LogWasmCodes", "functions",
               module_->num_declared_functions);

  Object url_obj = script.name();
  DCHECK(url_obj.IsString() || url_obj.IsUndefined());
  std::unique_ptr<char[]> source_url =
      url_obj.IsString() ? String::cast(url_obj).ToCString() : nullptr;

  // Log all owned code, not just the current entries in the code table. This
  // will also include import wrappers.
  WasmCodeRefScope code_ref_scope;
  for (auto& code : SnapshotAllOwnedCode()) {
    code->LogCode(isolate, source_url.get(), script.id());
  }
}

CompilationEnv NativeModule::CreateCompilationEnv() const {
  return {module(), bounds_checks_, kRuntimeExceptionSupport, enabled_features_,
          compilation_state()->dynamic_tiering()};
}

WasmCode* NativeModule::AddCodeForTesting(Handle<Code> code) {
  CodeSpaceWriteScope code_space_write_scope(this);
  const size_t relocation_size = code->relocation_size();
  base::OwnedVector<byte> reloc_info;
  if (relocation_size > 0) {
    reloc_info = base::OwnedVector<byte>::Of(
        base::Vector<byte>{code->relocation_start(), relocation_size});
  }
  Handle<ByteArray> source_pos_table(code->source_position_table(),
                                     code->GetIsolate());
  base::OwnedVector<byte> source_pos =
      base::OwnedVector<byte>::NewForOverwrite(source_pos_table->length());
  if (source_pos_table->length() > 0) {
    source_pos_table->copy_out(0, source_pos.start(),
                               source_pos_table->length());
  }
  CHECK(!code->is_off_heap_trampoline());
  static_assert(Code::kOnHeapBodyIsContiguous);
  base::Vector<const byte> instructions(
      reinterpret_cast<byte*>(code->raw_body_start()),
      static_cast<size_t>(code->raw_body_size()));
  const int stack_slots = code->stack_slots();

  // Metadata offsets in Code objects are relative to the start of the metadata
  // section, whereas WasmCode expects offsets relative to InstructionStart.
  const int base_offset = code->raw_instruction_size();
  // TODO(jgruber,v8:8758): Remove this translation. It exists only because
  // Code objects contains real offsets but WasmCode expects an offset of 0 to
  // mean 'empty'.
  const int safepoint_table_offset =
      code->has_safepoint_table() ? base_offset + code->safepoint_table_offset()
                                  : 0;
  const int handler_table_offset = base_offset + code->handler_table_offset();
  const int constant_pool_offset = base_offset + code->constant_pool_offset();
  const int code_comments_offset = base_offset + code->code_comments_offset();

  base::RecursiveMutexGuard guard{&allocation_mutex_};
  base::Vector<uint8_t> dst_code_bytes =
      code_allocator_.AllocateForCode(this, instructions.size());
  memcpy(dst_code_bytes.begin(), instructions.begin(), instructions.size());

  // Apply the relocation delta by iterating over the RelocInfo.
  intptr_t delta = reinterpret_cast<Address>(dst_code_bytes.begin()) -
                   code->raw_instruction_start();
  int mode_mask =
      RelocInfo::kApplyMask | RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL);
  auto jump_tables_ref =
      FindJumpTablesForRegionLocked(base::AddressRegionOf(dst_code_bytes));
  Address dst_code_addr = reinterpret_cast<Address>(dst_code_bytes.begin());
  Address constant_pool_start = dst_code_addr + constant_pool_offset;
  RelocIterator orig_it(*code, mode_mask);
  for (RelocIterator it(dst_code_bytes, reloc_info.as_vector(),
                        constant_pool_start, mode_mask);
       !it.done(); it.next(), orig_it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmStubCall(mode)) {
      uint32_t stub_call_tag = orig_it.rinfo()->wasm_call_tag();
      DCHECK_LT(stub_call_tag, WasmCode::kRuntimeStubCount);
      Address entry = GetNearRuntimeStubEntry(
          static_cast<WasmCode::RuntimeStubId>(stub_call_tag), jump_tables_ref);
      it.rinfo()->set_wasm_stub_call_address(entry, SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta);
    }
  }

  // Flush the i-cache after relocation.
  FlushInstructionCache(dst_code_bytes.begin(), dst_code_bytes.size());

  std::unique_ptr<WasmCode> new_code{
      new WasmCode{this,                     // native_module
                   kAnonymousFuncIndex,      // index
                   dst_code_bytes,           // instructions
                   stack_slots,              // stack_slots
                   0,                        // tagged_parameter_slots
                   safepoint_table_offset,   // safepoint_table_offset
                   handler_table_offset,     // handler_table_offset
                   constant_pool_offset,     // constant_pool_offset
                   code_comments_offset,     // code_comments_offset
                   instructions.length(),    // unpadded_binary_size
                   {},                       // protected_instructions
                   reloc_info.as_vector(),   // reloc_info
                   source_pos.as_vector(),   // source positions
                   WasmCode::kWasmFunction,  // kind
                   ExecutionTier::kNone,     // tier
                   kNoDebugging}};           // for_debugging
  new_code->MaybePrint();
  new_code->Validate();

  return PublishCodeLocked(std::move(new_code));
}

void NativeModule::UseLazyStub(uint32_t func_index) {
  DCHECK_LE(module_->num_imported_functions, func_index);
  DCHECK_LT(func_index,
            module_->num_imported_functions + module_->num_declared_functions);
  // Avoid opening a new write scope per function. The caller should hold the
  // scope instead.
  DCHECK(CodeSpaceWriteScope::IsInScope());

  base::RecursiveMutexGuard guard(&allocation_mutex_);
  if (!lazy_compile_table_) {
    uint32_t num_slots = module_->num_declared_functions;
    WasmCodeRefScope code_ref_scope;
    lazy_compile_table_ = CreateEmptyJumpTableLocked(
        JumpTableAssembler::SizeForNumberOfLazyFunctions(num_slots));
    Address compile_lazy_address = GetNearRuntimeStubEntry(
        WasmCode::kWasmCompileLazy,
        FindJumpTablesForRegionLocked(
            base::AddressRegionOf(lazy_compile_table_->instructions())));
    JumpTableAssembler::GenerateLazyCompileTable(
        lazy_compile_table_->instruction_start(), num_slots,
        module_->num_imported_functions, compile_lazy_address);
  }

  // Add jump table entry for jump to the lazy compile stub.
  uint32_t slot_index = declared_function_index(module(), func_index);
  DCHECK_NULL(code_table_[slot_index]);
  Address lazy_compile_target =
      lazy_compile_table_->instruction_start() +
      JumpTableAssembler::LazyCompileSlotIndexToOffset(slot_index);
  PatchJumpTablesLocked(slot_index, lazy_compile_target);
}

std::unique_ptr<WasmCode> NativeModule::AddCode(
    int index, const CodeDesc& desc, int stack_slots,
    uint32_t tagged_parameter_slots,
    base::Vector<const byte> protected_instructions_data,
    base::Vector<const byte> source_position_table, WasmCode::Kind kind,
    ExecutionTier tier, ForDebugging for_debugging) {
  base::Vector<byte> code_space;
  NativeModule::JumpTablesRef jump_table_ref;
  {
    base::RecursiveMutexGuard guard{&allocation_mutex_};
    code_space = code_allocator_.AllocateForCode(this, desc.instr_size);
    jump_table_ref =
        FindJumpTablesForRegionLocked(base::AddressRegionOf(code_space));
  }
  return AddCodeWithCodeSpace(index, desc, stack_slots, tagged_parameter_slots,
                              protected_instructions_data,
                              source_position_table, kind, tier, for_debugging,
                              code_space, jump_table_ref);
}

std::unique_ptr<WasmCode> NativeModule::AddCodeWithCodeSpace(
    int index, const CodeDesc& desc, int stack_slots,
    uint32_t tagged_parameter_slots,
    base::Vector<const byte> protected_instructions_data,
    base::Vector<const byte> source_position_table, WasmCode::Kind kind,
    ExecutionTier tier, ForDebugging for_debugging,
    base::Vector<uint8_t> dst_code_bytes, const JumpTablesRef& jump_tables) {
  base::Vector<byte> reloc_info{
      desc.buffer + desc.buffer_size - desc.reloc_size,
      static_cast<size_t>(desc.reloc_size)};
  UpdateCodeSize(desc.instr_size, tier, for_debugging);

  // TODO(jgruber,v8:8758): Remove this translation. It exists only because
  // CodeDesc contains real offsets but WasmCode expects an offset of 0 to mean
  // 'empty'.
  const int safepoint_table_offset =
      desc.safepoint_table_size == 0 ? 0 : desc.safepoint_table_offset;
  const int handler_table_offset = desc.handler_table_offset;
  const int constant_pool_offset = desc.constant_pool_offset;
  const int code_comments_offset = desc.code_comments_offset;
  const int instr_size = desc.instr_size;

  memcpy(dst_code_bytes.begin(), desc.buffer,
         static_cast<size_t>(desc.instr_size));

  // Apply the relocation delta by iterating over the RelocInfo.
  intptr_t delta = dst_code_bytes.begin() - desc.buffer;
  int mode_mask = RelocInfo::kApplyMask |
                  RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
                  RelocInfo::ModeMask(RelocInfo::WASM_STUB_CALL);
  Address code_start = reinterpret_cast<Address>(dst_code_bytes.begin());
  Address constant_pool_start = code_start + constant_pool_offset;
  for (RelocIterator it(dst_code_bytes, reloc_info, constant_pool_start,
                        mode_mask);
       !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (RelocInfo::IsWasmCall(mode)) {
      uint32_t call_tag = it.rinfo()->wasm_call_tag();
      Address target = GetNearCallTargetForFunction(call_tag, jump_tables);
      it.rinfo()->set_wasm_call_address(target, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsWasmStubCall(mode)) {
      uint32_t stub_call_tag = it.rinfo()->wasm_call_tag();
      DCHECK_LT(stub_call_tag, WasmCode::kRuntimeStubCount);
      Address entry = GetNearRuntimeStubEntry(
          static_cast<WasmCode::RuntimeStubId>(stub_call_tag), jump_tables);
      it.rinfo()->set_wasm_stub_call_address(entry, SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta);
    }
  }

  // Flush the i-cache after relocation.
  FlushInstructionCache(dst_code_bytes.begin(), dst_code_bytes.size());

  // Liftoff code will not be relocated or serialized, thus do not store any
  // relocation information.
  if (tier == ExecutionTier::kLiftoff) reloc_info = {};

  std::unique_ptr<WasmCode> code{new WasmCode{
      this, index, dst_code_bytes, stack_slots, tagged_parameter_slots,
      safepoint_table_offset, handler_table_offset, constant_pool_offset,
      code_comments_offset, instr_size, protected_instructions_data, reloc_info,
      source_position_table, kind, tier, for_debugging}};

  code->MaybePrint();
  code->Validate();

  return code;
}

WasmCode* NativeModule::PublishCode(std::unique_ptr<WasmCode> code) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.PublishCode");
  base::RecursiveMutexGuard lock(&allocation_mutex_);
  CodeSpaceWriteScope code_space_write_scope(this);
  return PublishCodeLocked(std::move(code));
}

std::vector<WasmCode*> NativeModule::PublishCode(
    base::Vector<std::unique_ptr<WasmCode>> codes) {
  // Publishing often happens in a loop, so the caller should hold the
  // {CodeSpaceWriteScope} outside of such a loop.
  DCHECK(CodeSpaceWriteScope::IsInScope());
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.PublishCode", "number", codes.size());
  std::vector<WasmCode*> published_code;
  published_code.reserve(codes.size());
  base::RecursiveMutexGuard lock(&allocation_mutex_);
  // The published code is put into the top-most surrounding {WasmCodeRefScope}.
  for (auto& code : codes) {
    published_code.push_back(PublishCodeLocked(std::move(code)));
  }
  return published_code;
}

WasmCode::Kind GetCodeKind(const WasmCompilationResult& result) {
  switch (result.kind) {
    case WasmCompilationResult::kWasmToJsWrapper:
      return WasmCode::Kind::kWasmToJsWrapper;
    case WasmCompilationResult::kFunction:
      return WasmCode::Kind::kWasmFunction;
    default:
      UNREACHABLE();
  }
}

WasmCode* NativeModule::PublishCodeLocked(
    std::unique_ptr<WasmCode> owned_code) {
  allocation_mutex_.AssertHeld();

  WasmCode* code = owned_code.get();
  new_owned_code_.emplace_back(std::move(owned_code));

  // Add the code to the surrounding code ref scope, so the returned pointer is
  // guaranteed to be valid.
  WasmCodeRefScope::AddRef(code);

  if (code->index() < static_cast<int>(module_->num_imported_functions)) {
    return code;
  }

  DCHECK_LT(code->index(), num_functions());

  code->RegisterTrapHandlerData();

  // Put the code in the debugging cache, if needed.
  if (V8_UNLIKELY(cached_code_)) InsertToCodeCache(code);

  // Assume an order of execution tiers that represents the quality of their
  // generated code.
  static_assert(ExecutionTier::kNone < ExecutionTier::kLiftoff &&
                    ExecutionTier::kLiftoff < ExecutionTier::kTurbofan,
                "Assume an order on execution tiers");

  uint32_t slot_idx = declared_function_index(module(), code->index());
  WasmCode* prior_code = code_table_[slot_idx];
  // If we are tiered down, install all debugging code (except for stepping
  // code, which is only used for a single frame and never installed in the
  // code table of jump table). Otherwise, install code if it was compiled
  // with a higher tier.
  static_assert(
      kForDebugging > kNoDebugging && kWithBreakpoints > kForDebugging,
      "for_debugging is ordered");
  const bool update_code_table =
      // Never install stepping code.
      code->for_debugging() != kForStepping &&
      (!prior_code ||
       (tiering_state_ == kTieredDown
            // Tiered down: Install breakpoints over normal debug code.
            ? prior_code->for_debugging() <= code->for_debugging()
            // Tiered up: Install if the tier is higher than before or we
            // replace debugging code with non-debugging code.
            : (prior_code->tier() < code->tier() ||
               (prior_code->for_debugging() && !code->for_debugging()))));
  if (update_code_table) {
    code_table_[slot_idx] = code;
    if (prior_code) {
      WasmCodeRefScope::AddRef(prior_code);
      // The code is added to the current {WasmCodeRefScope}, hence the ref
      // count cannot drop to zero here.
      prior_code->DecRefOnLiveCode();
    }

    PatchJumpTablesLocked(slot_idx, code->instruction_start());
  } else {
    // The code tables does not hold a reference to the code, hence decrement
    // the initial ref count of 1. The code was added to the
    // {WasmCodeRefScope} though, so it cannot die here.
    code->DecRefOnLiveCode();
  }

  return code;
}

void NativeModule::ReinstallDebugCode(WasmCode* code) {
  base::RecursiveMutexGuard lock(&allocation_mutex_);

  DCHECK_EQ(this, code->native_module());
  DCHECK_EQ(kWithBreakpoints, code->for_debugging());
  DCHECK(!code->IsAnonymous());
  DCHECK_LE(module_->num_imported_functions, code->index());
  DCHECK_LT(code->index(), num_functions());

  // If the module is tiered up by now, do not reinstall debug code.
  if (tiering_state_ != kTieredDown) return;

  uint32_t slot_idx = declared_function_index(module(), code->index());
  if (WasmCode* prior_code = code_table_[slot_idx]) {
    WasmCodeRefScope::AddRef(prior_code);
    // The code is added to the current {WasmCodeRefScope}, hence the ref
    // count cannot drop to zero here.
    prior_code->DecRefOnLiveCode();
  }
  code_table_[slot_idx] = code;
  code->IncRef();

  CodeSpaceWriteScope code_space_write_scope(this);
  PatchJumpTablesLocked(slot_idx, code->instruction_start());
}

std::pair<base::Vector<uint8_t>, NativeModule::JumpTablesRef>
NativeModule::AllocateForDeserializedCode(size_t total_code_size) {
  base::RecursiveMutexGuard guard{&allocation_mutex_};
  base::Vector<uint8_t> code_space =
      code_allocator_.AllocateForCode(this, total_code_size);
  auto jump_tables =
      FindJumpTablesForRegionLocked(base::AddressRegionOf(code_space));
  return {code_space, jump_tables};
}

std::unique_ptr<WasmCode> NativeModule::AddDeserializedCode(
    int index, base::Vector<byte> instructions, int stack_slots,
    uint32_t tagged_parameter_slots, int safepoint_table_offset,
    int handler_table_offset, int constant_pool_offset,
    int code_comments_offset, int unpadded_binary_size,
    base::Vector<const byte> protected_instructions_data,
    base::Vector<const byte> reloc_info,
    base::Vector<const byte> source_position_table, WasmCode::Kind kind,
    ExecutionTier tier) {
  UpdateCodeSize(instructions.size(), tier, kNoDebugging);

  return std::unique_ptr<WasmCode>{new WasmCode{
      this, index, instructions, stack_slots, tagged_parameter_slots,
      safepoint_table_offset, handler_table_offset, constant_pool_offset,
      code_comments_offset, unpadded_binary_size, protected_instructions_data,
      reloc_info, source_position_table, kind, tier, kNoDebugging}};
}

std::vector<WasmCode*> NativeModule::SnapshotCodeTable() const {
  base::RecursiveMutexGuard lock(&allocation_mutex_);
  WasmCode** start = code_table_.get();
  WasmCode** end = start + module_->num_declared_functions;
  for (WasmCode* code : base::VectorOf(start, end - start)) {
    if (code) WasmCodeRefScope::AddRef(code);
  }
  return std::vector<WasmCode*>{start, end};
}

std::vector<WasmCode*> NativeModule::SnapshotAllOwnedCode() const {
  base::RecursiveMutexGuard lock(&allocation_mutex_);
  if (!new_owned_code_.empty()) TransferNewOwnedCodeLocked();

  std::vector<WasmCode*> all_code(owned_code_.size());
  std::transform(owned_code_.begin(), owned_code_.end(), all_code.begin(),
                 [](auto& entry) { return entry.second.get(); });
  std::for_each(all_code.begin(), all_code.end(), WasmCodeRefScope::AddRef);
  return all_code;
}

WasmCode* NativeModule::GetCode(uint32_t index) const {
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  WasmCode* code = code_table_[declared_function_index(module(), index)];
  if (code) WasmCodeRefScope::AddRef(code);
  return code;
}

bool NativeModule::HasCode(uint32_t index) const {
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  return code_table_[declared_function_index(module(), index)] != nullptr;
}

bool NativeModule::HasCodeWithTier(uint32_t index, ExecutionTier tier) const {
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  return code_table_[declared_function_index(module(), index)] != nullptr &&
         code_table_[declared_function_index(module(), index)]->tier() == tier;
}

void NativeModule::SetWasmSourceMap(
    std::unique_ptr<WasmModuleSourceMap> source_map) {
  source_map_ = std::move(source_map);
}

WasmModuleSourceMap* NativeModule::GetWasmSourceMap() const {
  return source_map_.get();
}

WasmCode* NativeModule::CreateEmptyJumpTableLocked(int jump_table_size) {
  return CreateEmptyJumpTableInRegionLocked(jump_table_size,
                                            kUnrestrictedRegion);
}

WasmCode* NativeModule::CreateEmptyJumpTableInRegionLocked(
    int jump_table_size, base::AddressRegion region) {
  allocation_mutex_.AssertHeld();
  // Only call this if we really need a jump table.
  DCHECK_LT(0, jump_table_size);
  CodeSpaceWriteScope code_space_write_scope(this);
  base::Vector<uint8_t> code_space =
      code_allocator_.AllocateForCodeInRegion(this, jump_table_size, region);
  DCHECK(!code_space.empty());
  UpdateCodeSize(jump_table_size, ExecutionTier::kNone, kNoDebugging);
  ZapCode(reinterpret_cast<Address>(code_space.begin()), code_space.size());
  std::unique_ptr<WasmCode> code{
      new WasmCode{this,                  // native_module
                   kAnonymousFuncIndex,   // index
                   code_space,            // instructions
                   0,                     // stack_slots
                   0,                     // tagged_parameter_slots
                   0,                     // safepoint_table_offset
                   jump_table_size,       // handler_table_offset
                   jump_table_size,       // constant_pool_offset
                   jump_table_size,       // code_comments_offset
                   jump_table_size,       // unpadded_binary_size
                   {},                    // protected_instructions
                   {},                    // reloc_info
                   {},                    // source_pos
                   WasmCode::kJumpTable,  // kind
                   ExecutionTier::kNone,  // tier
                   kNoDebugging}};        // for_debugging
  return PublishCodeLocked(std::move(code));
}

void NativeModule::UpdateCodeSize(size_t size, ExecutionTier tier,
                                  ForDebugging for_debugging) {
  if (for_debugging != kNoDebugging) return;
  // Count jump tables (ExecutionTier::kNone) for both Liftoff and TurboFan as
  // this is shared code.
  if (tier != ExecutionTier::kTurbofan) liftoff_code_size_.fetch_add(size);
  if (tier != ExecutionTier::kLiftoff) turbofan_code_size_.fetch_add(size);
}

void NativeModule::PatchJumpTablesLocked(uint32_t slot_index, Address target) {
  allocation_mutex_.AssertHeld();

  for (auto& code_space_data : code_space_data_) {
    DCHECK_IMPLIES(code_space_data.jump_table, code_space_data.far_jump_table);
    if (!code_space_data.jump_table) continue;
    PatchJumpTableLocked(code_space_data, slot_index, target);
  }
}

void NativeModule::PatchJumpTableLocked(const CodeSpaceData& code_space_data,
                                        uint32_t slot_index, Address target) {
  allocation_mutex_.AssertHeld();

  DCHECK_NOT_NULL(code_space_data.jump_table);
  DCHECK_NOT_NULL(code_space_data.far_jump_table);

  // Jump tables are often allocated next to each other, so we can switch
  // permissions on both at the same time.
  if (code_space_data.jump_table->instructions().end() ==
      code_space_data.far_jump_table->instructions().begin()) {
    base::Vector<uint8_t> jump_tables_space = base::VectorOf(
        code_space_data.jump_table->instructions().begin(),
        code_space_data.jump_table->instructions().size() +
            code_space_data.far_jump_table->instructions().size());
    code_allocator_.MakeWritable(AddressRegionOf(jump_tables_space));
  } else {
    code_allocator_.MakeWritable(
        AddressRegionOf(code_space_data.jump_table->instructions()));
    code_allocator_.MakeWritable(
        AddressRegionOf(code_space_data.far_jump_table->instructions()));
  }

  DCHECK_LT(slot_index, module_->num_declared_functions);
  Address jump_table_slot =
      code_space_data.jump_table->instruction_start() +
      JumpTableAssembler::JumpSlotIndexToOffset(slot_index);
  uint32_t far_jump_table_offset = JumpTableAssembler::FarJumpSlotIndexToOffset(
      WasmCode::kRuntimeStubCount + slot_index);
  // Only pass the far jump table start if the far jump table actually has a
  // slot for this function index (i.e. does not only contain runtime stubs).
  bool has_far_jump_slot =
      far_jump_table_offset <
      code_space_data.far_jump_table->instructions().size();
  Address far_jump_table_start =
      code_space_data.far_jump_table->instruction_start();
  Address far_jump_table_slot =
      has_far_jump_slot ? far_jump_table_start + far_jump_table_offset
                        : kNullAddress;
  JumpTableAssembler::PatchJumpTableSlot(jump_table_slot, far_jump_table_slot,
                                         target);
}

void NativeModule::AddCodeSpaceLocked(base::AddressRegion region) {
  allocation_mutex_.AssertHeld();

  // Each code space must be at least twice as large as the overhead per code
  // space. Otherwise, we are wasting too much memory.
  DCHECK_GE(region.size(),
            2 * OverheadPerCodeSpace(module()->num_declared_functions));

  CodeSpaceWriteScope code_space_write_scope(this);
#if defined(V8_OS_WIN64)
  // On some platforms, specifically Win64, we need to reserve some pages at
  // the beginning of an executable space.
  // See src/heap/spaces.cc, MemoryAllocator::InitializeCodePageAllocator() and
  // https://cs.chromium.org/chromium/src/components/crash/content/app/crashpad_win.cc?rcl=fd680447881449fba2edcf0589320e7253719212&l=204
  // for details.
  if (WasmCodeManager::CanRegisterUnwindInfoForNonABICompliantCodeRange()) {
    size_t size = Heap::GetCodeRangeReservedAreaSize();
    DCHECK_LT(0, size);
    base::Vector<byte> padding =
        code_allocator_.AllocateForCodeInRegion(this, size, region);
    CHECK_EQ(reinterpret_cast<Address>(padding.begin()), region.begin());
    win64_unwindinfo::RegisterNonABICompliantCodeRange(
        reinterpret_cast<void*>(region.begin()), region.size());
  }
#endif  // V8_OS_WIN64

  WasmCodeRefScope code_ref_scope;
  WasmCode* jump_table = nullptr;
  WasmCode* far_jump_table = nullptr;
  const uint32_t num_wasm_functions = module_->num_declared_functions;
  const bool is_first_code_space = code_space_data_.empty();
  // We always need a far jump table, because it contains the runtime stubs.
  const bool needs_far_jump_table =
      !FindJumpTablesForRegionLocked(region).is_valid();
  const bool needs_jump_table = num_wasm_functions > 0 && needs_far_jump_table;

  if (needs_jump_table) {
    jump_table = CreateEmptyJumpTableInRegionLocked(
        JumpTableAssembler::SizeForNumberOfSlots(num_wasm_functions), region);
    CHECK(region.contains(jump_table->instruction_start()));
  }

  if (needs_far_jump_table) {
    int num_function_slots = NumWasmFunctionsInFarJumpTable(num_wasm_functions);
    far_jump_table = CreateEmptyJumpTableInRegionLocked(
        JumpTableAssembler::SizeForNumberOfFarJumpSlots(
            WasmCode::kRuntimeStubCount,
            NumWasmFunctionsInFarJumpTable(num_function_slots)),
        region);
    CHECK(region.contains(far_jump_table->instruction_start()));
    EmbeddedData embedded_data = EmbeddedData::FromBlob();
#define RUNTIME_STUB(Name) Builtin::k##Name,
#define RUNTIME_STUB_TRAP(Name) RUNTIME_STUB(ThrowWasm##Name)
    Builtin stub_names[WasmCode::kRuntimeStubCount] = {
        WASM_RUNTIME_STUB_LIST(RUNTIME_STUB, RUNTIME_STUB_TRAP)};
#undef RUNTIME_STUB
#undef RUNTIME_STUB_TRAP
    static_assert(Builtins::kAllBuiltinsAreIsolateIndependent);
    Address builtin_addresses[WasmCode::kRuntimeStubCount];
    for (int i = 0; i < WasmCode::kRuntimeStubCount; ++i) {
      Builtin builtin = stub_names[i];
      builtin_addresses[i] = embedded_data.InstructionStartOfBuiltin(builtin);
    }
    JumpTableAssembler::GenerateFarJumpTable(
        far_jump_table->instruction_start(), builtin_addresses,
        WasmCode::kRuntimeStubCount, num_function_slots);
  }

  if (is_first_code_space) {
    // This can be updated and accessed without locks, since the addition of the
    // first code space happens during initialization of the {NativeModule},
    // where no concurrent accesses are possible.
    main_jump_table_ = jump_table;
    main_far_jump_table_ = far_jump_table;
  }

  code_space_data_.push_back(CodeSpaceData{region, jump_table, far_jump_table});

  if (jump_table && !is_first_code_space) {
    // Patch the new jump table(s) with existing functions. If this is the first
    // code space, there cannot be any functions that have been compiled yet.
    const CodeSpaceData& new_code_space_data = code_space_data_.back();
    for (uint32_t slot_index = 0; slot_index < num_wasm_functions;
         ++slot_index) {
      if (code_table_[slot_index]) {
        PatchJumpTableLocked(new_code_space_data, slot_index,
                             code_table_[slot_index]->instruction_start());
      } else if (lazy_compile_table_) {
        Address lazy_compile_target =
            lazy_compile_table_->instruction_start() +
            JumpTableAssembler::LazyCompileSlotIndexToOffset(slot_index);
        PatchJumpTableLocked(new_code_space_data, slot_index,
                             lazy_compile_target);
      }
    }
  }
}

namespace {
class NativeModuleWireBytesStorage final : public WireBytesStorage {
 public:
  explicit NativeModuleWireBytesStorage(
      std::shared_ptr<base::OwnedVector<const uint8_t>> wire_bytes)
      : wire_bytes_(std::move(wire_bytes)) {}

  base::Vector<const uint8_t> GetCode(WireBytesRef ref) const final {
    return std::atomic_load(&wire_bytes_)
        ->as_vector()
        .SubVector(ref.offset(), ref.end_offset());
  }

  base::Optional<ModuleWireBytes> GetModuleBytes() const final {
    return base::Optional<ModuleWireBytes>(
        std::atomic_load(&wire_bytes_)->as_vector());
  }

 private:
  const std::shared_ptr<base::OwnedVector<const uint8_t>> wire_bytes_;
};
}  // namespace

void NativeModule::SetWireBytes(base::OwnedVector<const uint8_t> wire_bytes) {
  auto shared_wire_bytes =
      std::make_shared<base::OwnedVector<const uint8_t>>(std::move(wire_bytes));
  std::atomic_store(&wire_bytes_, shared_wire_bytes);
  if (!shared_wire_bytes->empty()) {
    compilation_state_->SetWireBytesStorage(
        std::make_shared<NativeModuleWireBytesStorage>(
            std::move(shared_wire_bytes)));
  }
}

void NativeModule::UpdateCPUDuration(size_t cpu_duration, ExecutionTier tier) {
  if (!compilation_state_->baseline_compilation_finished()) {
    baseline_compilation_cpu_duration_.fetch_add(cpu_duration,
                                                 std::memory_order_relaxed);
  } else if (tier == ExecutionTier::kTurbofan) {
    tier_up_cpu_duration_.fetch_add(cpu_duration, std::memory_order_relaxed);
  }
}

void NativeModule::AddLazyCompilationTimeSample(int64_t sample_in_micro_sec) {
  num_lazy_compilations_.fetch_add(1, std::memory_order_relaxed);
  sum_lazy_compilation_time_in_micro_sec_.fetch_add(sample_in_micro_sec,
                                                    std::memory_order_relaxed);
  int64_t max =
      max_lazy_compilation_time_in_micro_sec_.load(std::memory_order_relaxed);
  while (sample_in_micro_sec > max &&
         !max_lazy_compilation_time_in_micro_sec_.compare_exchange_weak(
             max, sample_in_micro_sec, std::memory_order_relaxed,
             std::memory_order_relaxed)) {
    // Repeat until we set the new maximum sucessfully.
  }
}

void NativeModule::TransferNewOwnedCodeLocked() const {
  allocation_mutex_.AssertHeld();
  DCHECK(!new_owned_code_.empty());
  // Sort the {new_owned_code_} vector reversed, such that the position of the
  // previously inserted element can be used as a hint for the next element. If
  // elements in {new_owned_code_} are adjacent, this will guarantee
  // constant-time insertion into the map.
  std::sort(new_owned_code_.begin(), new_owned_code_.end(),
            [](const std::unique_ptr<WasmCode>& a,
               const std::unique_ptr<WasmCode>& b) {
              return a->instruction_start() > b->instruction_start();
            });
  auto insertion_hint = owned_code_.end();
  for (auto& code : new_owned_code_) {
    DCHECK_EQ(0, owned_code_.count(code->instruction_start()));
    // Check plausibility of the insertion hint.
    DCHECK(insertion_hint == owned_code_.end() ||
           insertion_hint->first > code->instruction_start());
    insertion_hint = owned_code_.emplace_hint(
        insertion_hint, code->instruction_start(), std::move(code));
  }
  new_owned_code_.clear();
}

void NativeModule::InsertToCodeCache(WasmCode* code) {
  allocation_mutex_.AssertHeld();
  DCHECK_NOT_NULL(cached_code_);
  if (code->IsAnonymous()) return;
  // Only cache Liftoff debugging code or TurboFan code (no breakpoints or
  // stepping).
  if (code->tier() == ExecutionTier::kLiftoff &&
      code->for_debugging() != kForDebugging) {
    return;
  }
  auto key = std::make_pair(code->tier(), code->index());
  if (cached_code_->insert(std::make_pair(key, code)).second) {
    code->IncRef();
  }
}

WasmCode* NativeModule::Lookup(Address pc) const {
  base::RecursiveMutexGuard lock(&allocation_mutex_);
  if (!new_owned_code_.empty()) TransferNewOwnedCodeLocked();
  auto iter = owned_code_.upper_bound(pc);
  if (iter == owned_code_.begin()) return nullptr;
  --iter;
  WasmCode* candidate = iter->second.get();
  DCHECK_EQ(candidate->instruction_start(), iter->first);
  if (!candidate->contains(pc)) return nullptr;
  WasmCodeRefScope::AddRef(candidate);
  return candidate;
}

NativeModule::JumpTablesRef NativeModule::FindJumpTablesForRegionLocked(
    base::AddressRegion code_region) const {
  allocation_mutex_.AssertHeld();
  auto jump_table_usable = [code_region](const WasmCode* jump_table) {
    Address table_start = jump_table->instruction_start();
    Address table_end = table_start + jump_table->instructions().size();
    // Compute the maximum distance from anywhere in the code region to anywhere
    // in the jump table, avoiding any underflow.
    size_t max_distance = std::max(
        code_region.end() > table_start ? code_region.end() - table_start : 0,
        table_end > code_region.begin() ? table_end - code_region.begin() : 0);
    // We can allow a max_distance that is equal to kMaxCodeSpaceSize, because
    // every call or jump will target an address *within* the region, but never
    // exactly the end of the region. So all occuring offsets are actually
    // smaller than max_distance.
    return max_distance <= WasmCodeAllocator::kMaxCodeSpaceSize;
  };

  for (auto& code_space_data : code_space_data_) {
    DCHECK_IMPLIES(code_space_data.jump_table, code_space_data.far_jump_table);
    if (!code_space_data.far_jump_table) continue;
    // Only return these jump tables if they are reachable from the whole
    // {code_region}.
    if (kNeedsFarJumpsBetweenCodeSpaces &&
        (!jump_table_usable(code_space_data.far_jump_table) ||
         (code_space_data.jump_table &&
          !jump_table_usable(code_space_data.jump_table)))) {
      continue;
    }
    return {code_space_data.jump_table
                ? code_space_data.jump_table->instruction_start()
                : kNullAddress,
            code_space_data.far_jump_table->instruction_start()};
  }
  return {};
}

Address NativeModule::GetNearCallTargetForFunction(
    uint32_t func_index, const JumpTablesRef& jump_tables) const {
  DCHECK(jump_tables.is_valid());
  uint32_t slot_offset = JumpTableOffset(module(), func_index);
  return jump_tables.jump_table_start + slot_offset;
}

Address NativeModule::GetNearRuntimeStubEntry(
    WasmCode::RuntimeStubId index, const JumpTablesRef& jump_tables) const {
  DCHECK(jump_tables.is_valid());
  auto offset = JumpTableAssembler::FarJumpSlotIndexToOffset(index);
  return jump_tables.far_jump_table_start + offset;
}

uint32_t NativeModule::GetFunctionIndexFromJumpTableSlot(
    Address slot_address) const {
  WasmCodeRefScope code_refs;
  WasmCode* code = Lookup(slot_address);
  DCHECK_NOT_NULL(code);
  DCHECK_EQ(WasmCode::kJumpTable, code->kind());
  uint32_t slot_offset =
      static_cast<uint32_t>(slot_address - code->instruction_start());
  uint32_t slot_idx = JumpTableAssembler::SlotOffsetToIndex(slot_offset);
  DCHECK_LT(slot_idx, module_->num_declared_functions);
  DCHECK_EQ(slot_address,
            code->instruction_start() +
                JumpTableAssembler::JumpSlotIndexToOffset(slot_idx));
  return module_->num_imported_functions + slot_idx;
}

WasmCode::RuntimeStubId NativeModule::GetRuntimeStubId(Address target) const {
  base::RecursiveMutexGuard guard(&allocation_mutex_);

  for (auto& code_space_data : code_space_data_) {
    if (code_space_data.far_jump_table != nullptr &&
        code_space_data.far_jump_table->contains(target)) {
      uint32_t offset = static_cast<uint32_t>(
          target - code_space_data.far_jump_table->instruction_start());
      uint32_t index = JumpTableAssembler::FarJumpSlotOffsetToIndex(offset);
      if (index >= WasmCode::kRuntimeStubCount) continue;
      if (JumpTableAssembler::FarJumpSlotIndexToOffset(index) != offset) {
        continue;
      }
      return static_cast<WasmCode::RuntimeStubId>(index);
    }
  }

  // Invalid address.
  return WasmCode::kRuntimeStubCount;
}

NativeModule::~NativeModule() {
  TRACE_HEAP("Deleting native module: %p\n", this);
  // Cancel all background compilation before resetting any field of the
  // NativeModule or freeing anything.
  compilation_state_->CancelCompilation();
  GetWasmEngine()->FreeNativeModule(this);
  // Free the import wrapper cache before releasing the {WasmCode} objects in
  // {owned_code_}. The destructor of {WasmImportWrapperCache} still needs to
  // decrease reference counts on the {WasmCode} objects.
  import_wrapper_cache_.reset();

  // If experimental PGO support is enabled, serialize the PGO data now.
  if (V8_UNLIKELY(FLAG_experimental_wasm_pgo_to_file)) {
    DumpProfileToFile(module_.get(), wire_bytes());
  }
}

WasmCodeManager::WasmCodeManager()
    : max_committed_code_space_(v8_flags.wasm_max_code_space * MB),
      critical_committed_code_space_(max_committed_code_space_ / 2) {}

WasmCodeManager::~WasmCodeManager() {
  // No more committed code space.
  DCHECK_EQ(0, total_committed_code_space_.load());
}

#if defined(V8_OS_WIN64)
// static
bool WasmCodeManager::CanRegisterUnwindInfoForNonABICompliantCodeRange() {
  return win64_unwindinfo::CanRegisterUnwindInfoForNonABICompliantCodeRange() &&
         v8_flags.win64_unwinding_info;
}
#endif  // V8_OS_WIN64

void WasmCodeManager::Commit(base::AddressRegion region) {
  // TODO(v8:8462): Remove eager commit once perf supports remapping.
  if (v8_flags.perf_prof) return;
  DCHECK(IsAligned(region.begin(), CommitPageSize()));
  DCHECK(IsAligned(region.size(), CommitPageSize()));
  // Reserve the size. Use CAS loop to avoid overflow on
  // {total_committed_code_space_}.
  size_t old_value = total_committed_code_space_.load();
  while (true) {
    DCHECK_GE(max_committed_code_space_, old_value);
    if (region.size() > max_committed_code_space_ - old_value) {
      auto oom_detail = base::FormattedString{}
                        << "trying to commit " << region.size()
                        << ", already committed " << old_value;
      V8::FatalProcessOutOfMemory(nullptr,
                                  "Exceeding maximum wasm committed code space",
                                  oom_detail.PrintToArray().data());
      UNREACHABLE();
    }
    if (total_committed_code_space_.compare_exchange_weak(
            old_value, old_value + region.size())) {
      break;
    }
  }
  // Even when we employ W^X with v8_flags.wasm_write_protect_code_memory ==
  // true, code pages need to be initially allocated with RWX permission because
  // of concurrent compilation/execution. For this reason there is no
  // distinction here based on v8_flags.wasm_write_protect_code_memory.
  // TODO(dlehmann): This allocates initially as writable and executable, and
  // as such is not safe-by-default. In particular, if
  // {WasmCodeAllocator::SetWritable(false)} is never called afterwards (e.g.,
  // because no {CodeSpaceWriteScope} is created), the writable permission is
  // never withdrawn.
  // One potential fix is to allocate initially with kReadExecute only, which
  // forces all compilation threads to add the missing {CodeSpaceWriteScope}s
  // before modification; and/or adding DCHECKs that {CodeSpaceWriteScope} is
  // open when calling this method.
  PageAllocator::Permission permission = PageAllocator::kReadWriteExecute;

  bool success = false;
  if (MemoryProtectionKeysEnabled()) {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
    TRACE_HEAP(
        "Setting rwx permissions and memory protection key for 0x%" PRIxPTR
        ":0x%" PRIxPTR "\n",
        region.begin(), region.end());
    success = base::MemoryProtectionKey::SetPermissionsAndKey(
        GetPlatformPageAllocator(), region, permission,
        RwxMemoryWriteScope::memory_protection_key());
#else
    UNREACHABLE();
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT
  } else {
    TRACE_HEAP("Setting rwx permissions for 0x%" PRIxPTR ":0x%" PRIxPTR "\n",
               region.begin(), region.end());
    success = SetPermissions(GetPlatformPageAllocator(), region.begin(),
                             region.size(), permission);
  }

  if (V8_UNLIKELY(!success)) {
    auto oom_detail = base::FormattedString{} << "region size: "
                                              << region.size();
    V8::FatalProcessOutOfMemory(nullptr, "Commit wasm code space",
                                oom_detail.PrintToArray().data());
    UNREACHABLE();
  }
}

void WasmCodeManager::Decommit(base::AddressRegion region) {
  // TODO(v8:8462): Remove this once perf supports remapping.
  if (v8_flags.perf_prof) return;
  PageAllocator* allocator = GetPlatformPageAllocator();
  DCHECK(IsAligned(region.begin(), allocator->CommitPageSize()));
  DCHECK(IsAligned(region.size(), allocator->CommitPageSize()));
  size_t old_committed = total_committed_code_space_.fetch_sub(region.size());
  DCHECK_LE(region.size(), old_committed);
  USE(old_committed);
  TRACE_HEAP("Decommitting system pages 0x%" PRIxPTR ":0x%" PRIxPTR "\n",
             region.begin(), region.end());
  CHECK(allocator->DecommitPages(reinterpret_cast<void*>(region.begin()),
                                 region.size()));
}

void WasmCodeManager::AssignRange(base::AddressRegion region,
                                  NativeModule* native_module) {
  base::MutexGuard lock(&native_modules_mutex_);
  lookup_map_.insert(std::make_pair(
      region.begin(), std::make_pair(region.end(), native_module)));
}

VirtualMemory WasmCodeManager::TryAllocate(size_t size, void* hint) {
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  DCHECK_GT(size, 0);
  size_t allocate_page_size = page_allocator->AllocatePageSize();
  size = RoundUp(size, allocate_page_size);
  if (hint == nullptr) hint = page_allocator->GetRandomMmapAddr();

  // When we start exposing Wasm in jitless mode, then the jitless flag
  // will have to determine whether we set kMapAsJittable or not.
  DCHECK(!v8_flags.jitless);
  VirtualMemory mem(page_allocator, size, hint, allocate_page_size,
                    JitPermission::kMapAsJittable);
  if (!mem.IsReserved()) return {};
  TRACE_HEAP("VMem alloc: 0x%" PRIxPTR ":0x%" PRIxPTR " (%zu)\n", mem.address(),
             mem.end(), mem.size());

  // TODO(v8:8462): Remove eager commit once perf supports remapping.
  if (v8_flags.perf_prof) {
    SetPermissions(GetPlatformPageAllocator(), mem.address(), mem.size(),
                   PageAllocator::kReadWriteExecute);
  }
  return mem;
}

namespace {
// The numbers here are rough estimates, used to calculate the size of the
// initial code reservation and for estimating the amount of external memory
// reported to the GC.
// They do not need to be accurate. Choosing them too small will result in
// separate code spaces being allocated (compile time and runtime overhead),
// choosing them too large results in over-reservation (virtual address space
// only).
// In doubt, choose the numbers slightly too large on 64-bit systems (where
// {kNeedsFarJumpsBetweenCodeSpaces} is {true}). Over-reservation is less
// critical in a 64-bit address space, but separate code spaces cause overhead.
// On 32-bit systems (where {kNeedsFarJumpsBetweenCodeSpaces} is {false}), the
// opposite is true: Multiple code spaces are cheaper, and address space is
// scarce, hence choose numbers slightly too small.
//
// Numbers can be determined by running benchmarks with
// --trace-wasm-compilation-times, and piping the output through
// tools/wasm/code-size-factors.py.
#if V8_TARGET_ARCH_X64
constexpr size_t kTurbofanFunctionOverhead = 24;
constexpr size_t kTurbofanCodeSizeMultiplier = 3;
constexpr size_t kLiftoffFunctionOverhead = 56;
constexpr size_t kLiftoffCodeSizeMultiplier = 4;
constexpr size_t kImportSize = 640;
#elif V8_TARGET_ARCH_IA32
constexpr size_t kTurbofanFunctionOverhead = 20;
constexpr size_t kTurbofanCodeSizeMultiplier = 3;
constexpr size_t kLiftoffFunctionOverhead = 48;
constexpr size_t kLiftoffCodeSizeMultiplier = 3;
constexpr size_t kImportSize = 600;
#elif V8_TARGET_ARCH_ARM
constexpr size_t kTurbofanFunctionOverhead = 44;
constexpr size_t kTurbofanCodeSizeMultiplier = 3;
constexpr size_t kLiftoffFunctionOverhead = 96;
constexpr size_t kLiftoffCodeSizeMultiplier = 5;
constexpr size_t kImportSize = 550;
#elif V8_TARGET_ARCH_ARM64
constexpr size_t kTurbofanFunctionOverhead = 40;
constexpr size_t kTurbofanCodeSizeMultiplier = 3;
constexpr size_t kLiftoffFunctionOverhead = 68;
constexpr size_t kLiftoffCodeSizeMultiplier = 4;
constexpr size_t kImportSize = 750;
#else
// Other platforms should add their own estimates for best performance. Numbers
// below are the maximum of other architectures.
constexpr size_t kTurbofanFunctionOverhead = 44;
constexpr size_t kTurbofanCodeSizeMultiplier = 4;
constexpr size_t kLiftoffFunctionOverhead = 96;
constexpr size_t kLiftoffCodeSizeMultiplier = 5;
constexpr size_t kImportSize = 750;
#endif
}  // namespace

// static
size_t WasmCodeManager::EstimateLiftoffCodeSize(int body_size) {
  return kLiftoffFunctionOverhead + kCodeAlignment / 2 +
         body_size * kLiftoffCodeSizeMultiplier;
}

// static
size_t WasmCodeManager::EstimateNativeModuleCodeSize(
    const WasmModule* module, bool include_liftoff,
    DynamicTiering dynamic_tiering) {
  int num_functions = static_cast<int>(module->num_declared_functions);
  int num_imported_functions = static_cast<int>(module->num_imported_functions);
  int code_section_length = 0;
  if (num_functions > 0) {
    DCHECK_EQ(module->functions.size(), num_imported_functions + num_functions);
    auto* first_fn = &module->functions[module->num_imported_functions];
    auto* last_fn = &module->functions.back();
    code_section_length =
        static_cast<int>(last_fn->code.end_offset() - first_fn->code.offset());
  }
  return EstimateNativeModuleCodeSize(num_functions, num_imported_functions,
                                      code_section_length, include_liftoff,
                                      dynamic_tiering);
}

// static
size_t WasmCodeManager::EstimateNativeModuleCodeSize(
    int num_functions, int num_imported_functions, int code_section_length,
    bool include_liftoff, DynamicTiering dynamic_tiering) {
  // The size for the jump table and far jump table is added later, per code
  // space (see {OverheadPerCodeSpace}). We still need to add the overhead for
  // the lazy compile table once, though. There are configurations where we do
  // not need it (non-asm.js, no dynamic tiering and no lazy compilation), but
  // we ignore this here as most of the time we will need it.
  const size_t lazy_compile_table_size =
      JumpTableAssembler::SizeForNumberOfLazyFunctions(num_functions);

  const size_t size_of_imports = kImportSize * num_imported_functions;

  const size_t overhead_per_function_turbofan =
      kTurbofanFunctionOverhead + kCodeAlignment / 2;
  size_t size_of_turbofan = overhead_per_function_turbofan * num_functions +
                            kTurbofanCodeSizeMultiplier * code_section_length;

  const size_t overhead_per_function_liftoff =
      kLiftoffFunctionOverhead + kCodeAlignment / 2;
  const size_t size_of_liftoff =
      include_liftoff ? overhead_per_function_liftoff * num_functions +
                            kLiftoffCodeSizeMultiplier * code_section_length
                      : 0;

  // With dynamic tiering we don't expect to compile more than 25% with
  // TurboFan. If there is no liftoff though then all code will get generated
  // by TurboFan.
  if (include_liftoff && dynamic_tiering) size_of_turbofan /= 4;

  return lazy_compile_table_size + size_of_imports + size_of_liftoff +
         size_of_turbofan;
}

// static
size_t WasmCodeManager::EstimateNativeModuleMetaDataSize(
    const WasmModule* module) {
  size_t wasm_module_estimate = EstimateStoredSize(module);

  uint32_t num_wasm_functions = module->num_declared_functions;

  // TODO(wasm): Include wire bytes size.
  size_t native_module_estimate =
      sizeof(NativeModule) +                      // NativeModule struct
      (sizeof(WasmCode*) * num_wasm_functions) +  // code table size
      (sizeof(WasmCode) * num_wasm_functions);    // code object size

  size_t jump_table_size = RoundUp<kCodeAlignment>(
      JumpTableAssembler::SizeForNumberOfSlots(num_wasm_functions));
  size_t far_jump_table_size =
      RoundUp<kCodeAlignment>(JumpTableAssembler::SizeForNumberOfFarJumpSlots(
          WasmCode::kRuntimeStubCount,
          NumWasmFunctionsInFarJumpTable(num_wasm_functions)));

  return wasm_module_estimate + native_module_estimate + jump_table_size +
         far_jump_table_size;
}

// static
bool WasmCodeManager::HasMemoryProtectionKeySupport() {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  return RwxMemoryWriteScope::IsSupported();
#else
  return false;
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT
}

// static
bool WasmCodeManager::MemoryProtectionKeysEnabled() {
  return HasMemoryProtectionKeySupport() &&
         v8_flags.wasm_memory_protection_keys;
}

// static
bool WasmCodeManager::MemoryProtectionKeyWritable() {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  return RwxMemoryWriteScope::IsPKUWritable();
#else
  return false;
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT
}

base::AddressRegion WasmCodeManager::AllocateAssemblerBufferSpace(int size) {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  if (MemoryProtectionKeysEnabled()) {
    auto* page_allocator = GetPlatformPageAllocator();
    size_t page_size = page_allocator->AllocatePageSize();
    size = RoundUp(size, page_size);
    void* mapped = AllocatePages(page_allocator, nullptr, size, page_size,
                                 PageAllocator::kNoAccess);
    if (V8_UNLIKELY(!mapped)) {
      auto oom_detail = base::FormattedString{}
                        << "cannot allocate " << size
                        << " more bytes for assembler buffers";
      V8::FatalProcessOutOfMemory(nullptr,
                                  "Allocate protected assembler buffer space",
                                  oom_detail.PrintToArray().data());
      UNREACHABLE();
    }
    auto region =
        base::AddressRegionOf(reinterpret_cast<uint8_t*>(mapped), size);
    CHECK(base::MemoryProtectionKey::SetPermissionsAndKey(
        page_allocator, region, PageAllocator::kReadWrite,
        RwxMemoryWriteScope::memory_protection_key()));
    return region;
  }
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT
  DCHECK(!MemoryProtectionKeysEnabled());
  return base::AddressRegionOf(new uint8_t[size], size);
}

void WasmCodeManager::FreeAssemblerBufferSpace(base::AddressRegion region) {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
  if (MemoryProtectionKeysEnabled()) {
    auto* page_allocator = GetPlatformPageAllocator();
    FreePages(page_allocator, reinterpret_cast<void*>(region.begin()),
              region.size());
    return;
  }
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT
  DCHECK(!MemoryProtectionKeysEnabled());
  delete[] reinterpret_cast<uint8_t*>(region.begin());
}

std::shared_ptr<NativeModule> WasmCodeManager::NewNativeModule(
    Isolate* isolate, const WasmFeatures& enabled, size_t code_size_estimate,
    std::shared_ptr<const WasmModule> module) {
  if (total_committed_code_space_.load() >
      critical_committed_code_space_.load()) {
    (reinterpret_cast<v8::Isolate*>(isolate))
        ->MemoryPressureNotification(MemoryPressureLevel::kCritical);
    size_t committed = total_committed_code_space_.load();
    DCHECK_GE(max_committed_code_space_, committed);
    critical_committed_code_space_.store(
        committed + (max_committed_code_space_ - committed) / 2);
  }

  size_t code_vmem_size =
      ReservationSize(code_size_estimate, module->num_declared_functions, 0);

  // The '--wasm-max-initial-code-space-reservation' testing flag can be used to
  // reduce the maximum size of the initial code space reservation (in MB).
  if (v8_flags.wasm_max_initial_code_space_reservation > 0) {
    size_t flag_max_bytes =
        static_cast<size_t>(v8_flags.wasm_max_initial_code_space_reservation) *
        MB;
    if (flag_max_bytes < code_vmem_size) code_vmem_size = flag_max_bytes;
  }

  // Try up to two times; getting rid of dead JSArrayBuffer allocations might
  // require two GCs because the first GC maybe incremental and may have
  // floating garbage.
  static constexpr int kAllocationRetries = 2;
  VirtualMemory code_space;
  for (int retries = 0;; ++retries) {
    code_space = TryAllocate(code_vmem_size);
    if (code_space.IsReserved()) break;
    if (retries == kAllocationRetries) {
      auto oom_detail = base::FormattedString{}
                        << "NewNativeModule cannot allocate code space of "
                        << code_vmem_size << " bytes";
      V8::FatalProcessOutOfMemory(isolate, "Allocate initial wasm code space",
                                  oom_detail.PrintToArray().data());
      UNREACHABLE();
    }
    // Run one GC, then try the allocation again.
    isolate->heap()->MemoryPressureNotification(MemoryPressureLevel::kCritical,
                                                true);
  }

  Address start = code_space.address();
  size_t size = code_space.size();
  Address end = code_space.end();
  std::shared_ptr<NativeModule> ret;
  new NativeModule(enabled,
                   DynamicTiering{v8_flags.wasm_dynamic_tiering.value()},
                   std::move(code_space), std::move(module),
                   isolate->async_counters(), &ret);
  // The constructor initialized the shared_ptr.
  DCHECK_NOT_NULL(ret);
  TRACE_HEAP("New NativeModule %p: Mem: 0x%" PRIxPTR ",+%zu\n", ret.get(),
             start, size);

  base::MutexGuard lock(&native_modules_mutex_);
  lookup_map_.insert(std::make_pair(start, std::make_pair(end, ret.get())));
  return ret;
}

void NativeModule::SampleCodeSize(
    Counters* counters, NativeModule::CodeSamplingTime sampling_time) const {
  size_t code_size = sampling_time == kSampling
                         ? code_allocator_.committed_code_space()
                         : code_allocator_.generated_code_size();
  int code_size_mb = static_cast<int>(code_size / MB);
  Histogram* histogram = nullptr;
  switch (sampling_time) {
    case kAfterBaseline:
      histogram = counters->wasm_module_code_size_mb_after_baseline();
      break;
    case kSampling: {
      histogram = counters->wasm_module_code_size_mb();
      // If this is a wasm module of >= 2MB, also sample the freed code size,
      // absolute and relative. Code GC does not happen on asm.js modules, and
      // small modules will never trigger GC anyway.
      size_t generated_size = code_allocator_.generated_code_size();
      if (generated_size >= 2 * MB && module()->origin == kWasmOrigin) {
        size_t freed_size = code_allocator_.freed_code_size();
        DCHECK_LE(freed_size, generated_size);
        int freed_percent = static_cast<int>(100 * freed_size / generated_size);
        counters->wasm_module_freed_code_size_percent()->AddSample(
            freed_percent);
      }
      break;
    }
  }
  histogram->AddSample(code_size_mb);
}

std::unique_ptr<WasmCode> NativeModule::AddCompiledCode(
    WasmCompilationResult result) {
  std::vector<std::unique_ptr<WasmCode>> code = AddCompiledCode({&result, 1});
  return std::move(code[0]);
}

std::vector<std::unique_ptr<WasmCode>> NativeModule::AddCompiledCode(
    base::Vector<WasmCompilationResult> results) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.AddCompiledCode", "num", results.size());
  DCHECK(!results.empty());
  // First, allocate code space for all the results.
  size_t total_code_space = 0;
  for (auto& result : results) {
    DCHECK(result.succeeded());
    total_code_space += RoundUp<kCodeAlignment>(result.code_desc.instr_size);
    if (result.result_tier == ExecutionTier::kLiftoff) {
      int index = result.func_index;
      int* slots = &module()->functions[index].feedback_slots;
#if DEBUG
      int current_value = base::Relaxed_Load(slots);
      DCHECK(current_value == 0 ||
             current_value == result.feedback_vector_slots);
#endif
      base::Relaxed_Store(slots, result.feedback_vector_slots);
    }
  }
  base::Vector<byte> code_space;
  NativeModule::JumpTablesRef jump_tables;
  CodeSpaceWriteScope code_space_write_scope(this);
  {
    base::RecursiveMutexGuard guard{&allocation_mutex_};
    code_space = code_allocator_.AllocateForCode(this, total_code_space);
    // Lookup the jump tables to use once, then use for all code objects.
    jump_tables =
        FindJumpTablesForRegionLocked(base::AddressRegionOf(code_space));
  }
  // If we happen to have a {total_code_space} which is bigger than
  // {kMaxCodeSpaceSize}, we would not find valid jump tables for the whole
  // region. If this ever happens, we need to handle this case (by splitting the
  // {results} vector in smaller chunks).
  CHECK(jump_tables.is_valid());

  std::vector<std::unique_ptr<WasmCode>> generated_code;
  generated_code.reserve(results.size());

  // Now copy the generated code into the code space and relocate it.
  for (auto& result : results) {
    DCHECK_EQ(result.code_desc.buffer, result.instr_buffer->start());
    size_t code_size = RoundUp<kCodeAlignment>(result.code_desc.instr_size);
    base::Vector<byte> this_code_space = code_space.SubVector(0, code_size);
    code_space += code_size;
    generated_code.emplace_back(AddCodeWithCodeSpace(
        result.func_index, result.code_desc, result.frame_slot_count,
        result.tagged_parameter_slots,
        result.protected_instructions_data.as_vector(),
        result.source_positions.as_vector(), GetCodeKind(result),
        result.result_tier, result.for_debugging, this_code_space,
        jump_tables));
  }
  DCHECK_EQ(0, code_space.size());

  return generated_code;
}

void NativeModule::SetTieringState(TieringState new_tiering_state) {
  // Do not tier down asm.js (just never change the tiering state).
  if (module()->origin != kWasmOrigin) return;

  base::RecursiveMutexGuard lock(&allocation_mutex_);
  tiering_state_ = new_tiering_state;
}

bool NativeModule::IsTieredDown() {
  base::RecursiveMutexGuard lock(&allocation_mutex_);
  return tiering_state_ == kTieredDown;
}

void NativeModule::RecompileForTiering() {
  // If baseline compilation is not finished yet, we do not tier down now. This
  // would be tricky because not all code is guaranteed to be available yet.
  // Instead, we tier down after streaming compilation finished.
  if (!compilation_state_->baseline_compilation_finished()) return;

  // Read the tiering state under the lock, then trigger recompilation after
  // releasing the lock. If the tiering state was changed when the triggered
  // compilation units finish, code installation will handle that correctly.
  TieringState current_state;
  {
    base::RecursiveMutexGuard lock(&allocation_mutex_);
    current_state = tiering_state_;

    // Initialize {cached_code_} to signal that this cache should get filled
    // from now on.
    if (!cached_code_) {
      cached_code_ = std::make_unique<
          std::map<std::pair<ExecutionTier, int>, WasmCode*>>();
      // Fill with existing code.
      for (auto& code_entry : owned_code_) {
        InsertToCodeCache(code_entry.second.get());
      }
    }
  }
  RecompileNativeModule(this, current_state);
}

std::vector<int> NativeModule::FindFunctionsToRecompile(
    TieringState new_tiering_state) {
  WasmCodeRefScope code_ref_scope;
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  // Get writable permission already here (and not inside the loop in
  // {PatchJumpTablesLocked}), to avoid switching for each slot individually.
  CodeSpaceWriteScope code_space_write_scope(this);
  std::vector<int> function_indexes;
  int imported = module()->num_imported_functions;
  int declared = module()->num_declared_functions;
  const bool tier_down = new_tiering_state == kTieredDown;
  for (int slot_index = 0; slot_index < declared; ++slot_index) {
    int function_index = imported + slot_index;
    WasmCode* old_code = code_table_[slot_index];
    bool code_is_good =
        tier_down ? old_code && old_code->for_debugging()
                  : old_code && old_code->tier() == ExecutionTier::kTurbofan;
    if (code_is_good) continue;
    DCHECK_NOT_NULL(cached_code_);
    auto cache_it = cached_code_->find(std::make_pair(
        tier_down ? ExecutionTier::kLiftoff : ExecutionTier::kTurbofan,
        function_index));
    if (cache_it != cached_code_->end()) {
      WasmCode* cached_code = cache_it->second;
      if (old_code) {
        WasmCodeRefScope::AddRef(old_code);
        // The code is added to the current {WasmCodeRefScope}, hence the ref
        // count cannot drop to zero here.
        old_code->DecRefOnLiveCode();
      }
      code_table_[slot_index] = cached_code;
      PatchJumpTablesLocked(slot_index, cached_code->instruction_start());
      cached_code->IncRef();
      continue;
    }
    // Otherwise add the function to the set of functions to recompile.
    function_indexes.push_back(function_index);
  }
  return function_indexes;
}

void NativeModule::FreeCode(base::Vector<WasmCode* const> codes) {
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  // Free the code space.
  code_allocator_.FreeCode(codes);

  if (!new_owned_code_.empty()) TransferNewOwnedCodeLocked();
  DebugInfo* debug_info = debug_info_.get();
  // Free the {WasmCode} objects. This will also unregister trap handler data.
  for (WasmCode* code : codes) {
    DCHECK_EQ(1, owned_code_.count(code->instruction_start()));
    owned_code_.erase(code->instruction_start());
  }
  // Remove debug side tables for all removed code objects, after releasing our
  // lock. This is to avoid lock order inversion.
  if (debug_info) debug_info->RemoveDebugSideTables(codes);
}

size_t NativeModule::GetNumberOfCodeSpacesForTesting() const {
  base::RecursiveMutexGuard guard{&allocation_mutex_};
  return code_allocator_.GetNumCodeSpaces();
}

bool NativeModule::HasDebugInfo() const {
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  return debug_info_ != nullptr;
}

DebugInfo* NativeModule::GetDebugInfo() {
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  if (!debug_info_) debug_info_ = std::make_unique<DebugInfo>(this);
  return debug_info_.get();
}

NamesProvider* NativeModule::GetNamesProvider() {
  DCHECK(HasWireBytes());
  base::RecursiveMutexGuard guard(&allocation_mutex_);
  if (!names_provider_) {
    names_provider_ =
        std::make_unique<NamesProvider>(module_.get(), wire_bytes());
  }
  return names_provider_.get();
}

void WasmCodeManager::FreeNativeModule(
    base::Vector<VirtualMemory> owned_code_space, size_t committed_size) {
  base::MutexGuard lock(&native_modules_mutex_);
  for (auto& code_space : owned_code_space) {
    DCHECK(code_space.IsReserved());
    TRACE_HEAP("VMem Release: 0x%" PRIxPTR ":0x%" PRIxPTR " (%zu)\n",
               code_space.address(), code_space.end(), code_space.size());

#if defined(V8_OS_WIN64)
    if (CanRegisterUnwindInfoForNonABICompliantCodeRange()) {
      win64_unwindinfo::UnregisterNonABICompliantCodeRange(
          reinterpret_cast<void*>(code_space.address()));
    }
#endif  // V8_OS_WIN64

    lookup_map_.erase(code_space.address());
    code_space.Free();
    DCHECK(!code_space.IsReserved());
  }

  DCHECK(IsAligned(committed_size, CommitPageSize()));
  // TODO(v8:8462): Remove this once perf supports remapping.
  if (!v8_flags.perf_prof) {
    size_t old_committed =
        total_committed_code_space_.fetch_sub(committed_size);
    DCHECK_LE(committed_size, old_committed);
    USE(old_committed);
  }
}

NativeModule* WasmCodeManager::LookupNativeModule(Address pc) const {
  base::MutexGuard lock(&native_modules_mutex_);
  if (lookup_map_.empty()) return nullptr;

  auto iter = lookup_map_.upper_bound(pc);
  if (iter == lookup_map_.begin()) return nullptr;
  --iter;
  Address region_start = iter->first;
  Address region_end = iter->second.first;
  NativeModule* candidate = iter->second.second;

  DCHECK_NOT_NULL(candidate);
  return region_start <= pc && pc < region_end ? candidate : nullptr;
}

WasmCode* WasmCodeManager::LookupCode(Address pc) const {
  NativeModule* candidate = LookupNativeModule(pc);
  return candidate ? candidate->Lookup(pc) : nullptr;
}

namespace {
thread_local WasmCodeRefScope* current_code_refs_scope = nullptr;
}  // namespace

WasmCodeRefScope::WasmCodeRefScope()
    : previous_scope_(current_code_refs_scope) {
  current_code_refs_scope = this;
}

WasmCodeRefScope::~WasmCodeRefScope() {
  DCHECK_EQ(this, current_code_refs_scope);
  current_code_refs_scope = previous_scope_;
  WasmCode::DecrementRefCount(base::VectorOf(code_ptrs_));
}

// static
void WasmCodeRefScope::AddRef(WasmCode* code) {
  DCHECK_NOT_NULL(code);
  WasmCodeRefScope* current_scope = current_code_refs_scope;
  DCHECK_NOT_NULL(current_scope);
  current_scope->code_ptrs_.push_back(code);
  code->IncRef();
}

Builtin RuntimeStubIdToBuiltinName(WasmCode::RuntimeStubId stub_id) {
#define RUNTIME_STUB_NAME(Name) Builtin::k##Name,
#define RUNTIME_STUB_NAME_TRAP(Name) Builtin::kThrowWasm##Name,
  constexpr Builtin builtin_names[] = {
      WASM_RUNTIME_STUB_LIST(RUNTIME_STUB_NAME, RUNTIME_STUB_NAME_TRAP)};
#undef RUNTIME_STUB_NAME
#undef RUNTIME_STUB_NAME_TRAP
  static_assert(arraysize(builtin_names) == WasmCode::kRuntimeStubCount);

  DCHECK_GT(arraysize(builtin_names), stub_id);
  return builtin_names[stub_id];
}

const char* GetRuntimeStubName(WasmCode::RuntimeStubId stub_id) {
#define RUNTIME_STUB_NAME(Name) #Name,
#define RUNTIME_STUB_NAME_TRAP(Name) "ThrowWasm" #Name,
  constexpr const char* runtime_stub_names[] = {WASM_RUNTIME_STUB_LIST(
      RUNTIME_STUB_NAME, RUNTIME_STUB_NAME_TRAP) "<unknown>"};
#undef RUNTIME_STUB_NAME
#undef RUNTIME_STUB_NAME_TRAP
  static_assert(arraysize(runtime_stub_names) ==
                WasmCode::kRuntimeStubCount + 1);

  DCHECK_GT(arraysize(runtime_stub_names), stub_id);
  return runtime_stub_names[stub_id];
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
#undef TRACE_HEAP
