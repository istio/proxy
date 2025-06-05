// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_RISCV_LIFTOFF_ASSEMBLER_RISCV32_H_
#define V8_WASM_BASELINE_RISCV_LIFTOFF_ASSEMBLER_RISCV32_H_

#include "src/heap/memory-chunk.h"
#include "src/wasm/baseline/liftoff-assembler.h"
#include "src/wasm/baseline/riscv/liftoff-assembler-riscv.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace liftoff {

// Liftoff Frames.
//
//  slot      Frame
//       +--------------------+---------------------------
//  n+4  | optional padding slot to keep the stack 16 byte aligned.
//  n+3  |   parameter n      |
//  ...  |       ...          |
//   4   |   parameter 1      | or parameter 2
//   3   |   parameter 0      | or parameter 1
//   2   |  (result address)  | or parameter 0
//  -----+--------------------+---------------------------
//   1   | return addr (ra)   |
//   0   | previous frame (fp)|
//  -----+--------------------+  <-- frame ptr (fp)
//  -1   | StackFrame::WASM   |
//  -2   |     instance       |
//  -3   |     feedback vector|
//  -4   |     tiering budget |
//  -----+--------------------+---------------------------
//  -5   |     slot 0         |   ^
//  -6   |     slot 1         |   |
//       |                    | Frame slots
//       |                    |   |
//       |                    |   v
//       | optional padding slot to keep the stack 16 byte aligned.
//  -----+--------------------+  <-- stack ptr (sp)
//

#if defined(V8_TARGET_BIG_ENDIAN)
constexpr int32_t kLowWordOffset = 4;
constexpr int32_t kHighWordOffset = 0;
#else
constexpr int32_t kLowWordOffset = 0;
constexpr int32_t kHighWordOffset = 4;
#endif

inline MemOperand GetHalfStackSlot(int offset, RegPairHalf half) {
  int32_t half_offset =
      half == kLowWord ? 0 : LiftoffAssembler::kStackSlotSize / 2;
  return MemOperand(offset > 0 ? fp : sp, -offset + half_offset);
}

inline MemOperand GetMemOp(LiftoffAssembler* assm, Register addr,
                           Register offset, uintptr_t offset_imm,
                           Register scratch, unsigned shift_amount = 0) {
  DCHECK_NE(scratch, kScratchReg2);
  DCHECK_NE(addr, kScratchReg2);
  DCHECK_NE(offset, kScratchReg2);
  if (offset != no_reg) {
    if (shift_amount != 0) {
      assm->CalcScaledAddress(scratch, addr, offset, shift_amount);
    } else {
      assm->AddWord(scratch, offset, addr);
    }
    addr = scratch;
  }
  if (is_int31(offset_imm)) {
    int32_t offset_imm32 = static_cast<int32_t>(offset_imm);
    return MemOperand(addr, offset_imm32);
  } else {
    assm->li(kScratchReg2, Operand(offset_imm));
    assm->AddWord(kScratchReg2, addr, kScratchReg2);
    return MemOperand(kScratchReg2, 0);
  }
}

inline void Load(LiftoffAssembler* assm, LiftoffRegister dst, Register base,
                 int32_t offset, ValueKind kind) {
  MemOperand src(base, offset);

  switch (kind) {
    case kI32:
    case kRef:
    case kRefNull:
    case kRtt:
      assm->Lw(dst.gp(), src);
      break;
    case kI64:
      assm->Lw(dst.low_gp(),
               MemOperand(base, offset + liftoff::kLowWordOffset));
      assm->Lw(dst.high_gp(),
               MemOperand(base, offset + liftoff::kHighWordOffset));
      break;
    case kF32:
      assm->LoadFloat(dst.fp(), src);
      break;
    case kF64:
      assm->LoadDouble(dst.fp(), src);
      break;
    default:
      UNREACHABLE();
  }
}

inline void Store(LiftoffAssembler* assm, Register base, int32_t offset,
                  LiftoffRegister src, ValueKind kind) {
  MemOperand dst(base, offset);
  switch (kind) {
    case kI32:
    case kRefNull:
    case kRef:
    case kRtt:
      assm->Sw(src.gp(), dst);
      break;
    case kI64:
      assm->Sw(src.low_gp(),
               MemOperand(base, offset + liftoff::kLowWordOffset));
      assm->Sw(src.high_gp(),
               MemOperand(base, offset + liftoff::kHighWordOffset));
      break;
    case kF32:
      assm->StoreFloat(src.fp(), dst);
      break;
    case kF64:
      assm->StoreDouble(src.fp(), dst);
      break;
    default:
      UNREACHABLE();
  }
}

inline void push(LiftoffAssembler* assm, LiftoffRegister reg, ValueKind kind) {
  switch (kind) {
    case kI32:
    case kRefNull:
    case kRef:
    case kRtt:
      assm->addi(sp, sp, -kSystemPointerSize);
      assm->Sw(reg.gp(), MemOperand(sp, 0));
      break;
    case kI64:
      assm->Push(reg.high_gp(), reg.low_gp());
      break;
    case kF32:
      assm->addi(sp, sp, -kSystemPointerSize);
      assm->StoreFloat(reg.fp(), MemOperand(sp, 0));
      break;
    case kF64:
      assm->addi(sp, sp, -kDoubleSize);
      assm->StoreDouble(reg.fp(), MemOperand(sp, 0));
      break;
    default:
      UNREACHABLE();
  }
}

inline Register EnsureNoAlias(Assembler* assm, Register reg,
                              LiftoffRegister must_not_alias,
                              UseScratchRegisterScope* temps) {
  if (reg != must_not_alias.low_gp() && reg != must_not_alias.high_gp())
    return reg;
  Register tmp = temps->Acquire();
  DCHECK_NE(must_not_alias.low_gp(), tmp);
  DCHECK_NE(must_not_alias.high_gp(), tmp);
  assm->mv(tmp, reg);
  return tmp;
}
}  // namespace liftoff

void LiftoffAssembler::LoadConstant(LiftoffRegister reg, WasmValue value,
                                    RelocInfo::Mode rmode) {
  switch (value.type().kind()) {
    case kI32:
      TurboAssembler::li(reg.gp(), Operand(value.to_i32(), rmode));
      break;
    case kI64: {
      DCHECK(RelocInfo::IsNoInfo(rmode));
      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::li(reg.low_gp(), Operand(low_word));
      TurboAssembler::li(reg.high_gp(), Operand(high_word));
      break;
    }
    case kF32:
      TurboAssembler::LoadFPRImmediate(reg.fp(),
                                       value.to_f32_boxed().get_bits());
      break;
    case kF64:
      TurboAssembler::LoadFPRImmediate(reg.fp(),
                                       value.to_f64_boxed().get_bits());
      break;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::LoadTaggedPointer(Register dst, Register src_addr,
                                         Register offset_reg,
                                         int32_t offset_imm, bool needs_shift) {
  static_assert(kTaggedSize == kSystemPointerSize);
  Load(LiftoffRegister(dst), src_addr, offset_reg,
       static_cast<uint32_t>(offset_imm), LoadType::kI32Load, nullptr, false,
       false, needs_shift);
}

void LiftoffAssembler::LoadFullPointer(Register dst, Register src_addr,
                                       int32_t offset_imm) {
  MemOperand src_op = MemOperand(src_addr, offset_imm);
  LoadWord(dst, src_op);
}

void LiftoffAssembler::StoreTaggedPointer(Register dst_addr,
                                          Register offset_reg,
                                          int32_t offset_imm,
                                          LiftoffRegister src,
                                          LiftoffRegList pinned,
                                          SkipWriteBarrier skip_write_barrier) {
  Register scratch = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  MemOperand dst_op =
      liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm, scratch);
  StoreTaggedField(src.gp(), dst_op);

  if (skip_write_barrier || v8_flags.disable_write_barriers) return;

  Label write_barrier;
  Label exit;
  CheckPageFlag(dst_addr, kScratchReg,
                MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                &write_barrier);
  Branch(&exit);
  bind(&write_barrier);
  JumpIfSmi(src.gp(), &exit);
  CheckPageFlag(src.gp(), kScratchReg,
                MemoryChunk::kPointersToHereAreInterestingMask, eq, &exit);
  AddWord(scratch, dst_op.rm(), dst_op.offset());
  CallRecordWriteStubSaveRegisters(dst_addr, scratch, SaveFPRegsMode::kSave,
                                   StubCallMode::kCallWasmRuntimeStub);
  bind(&exit);
}

void LiftoffAssembler::Load(LiftoffRegister dst, Register src_addr,
                            Register offset_reg, uintptr_t offset_imm,
                            LoadType type, uint32_t* protected_load_pc,
                            bool /* is_load_mem */, bool /* i64_offset */,
                            bool needs_shift) {
  unsigned shift_amount = needs_shift ? type.size_log_2() : 0;
  MemOperand src_op = liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm,
                                        kScratchReg, shift_amount);

  if (protected_load_pc) *protected_load_pc = pc_offset();
  switch (type.value()) {
    case LoadType::kI32Load8U:
      Lbu(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8U:
      Lbu(dst.low_gp(), src_op);
      TurboAssembler::mv(dst.high_gp(), zero_reg);
      break;
    case LoadType::kI32Load8S:
      Lb(dst.gp(), src_op);
      break;
    case LoadType::kI64Load8S:
      Lb(dst.low_gp(), src_op);
      TurboAssembler::srai(dst.high_gp(), dst.low_gp(), 31);
      break;
    case LoadType::kI32Load16U:
      TurboAssembler::Lhu(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16U:
      TurboAssembler::Lhu(dst.low_gp(), src_op);
      TurboAssembler::mv(dst.high_gp(), zero_reg);
      break;
    case LoadType::kI32Load16S:
      TurboAssembler::Lh(dst.gp(), src_op);
      break;
    case LoadType::kI64Load16S:
      TurboAssembler::Lh(dst.low_gp(), src_op);
      TurboAssembler::srai(dst.high_gp(), dst.low_gp(), 31);
      break;
    case LoadType::kI64Load32U:
      TurboAssembler::Lw(dst.low_gp(), src_op);
      TurboAssembler::mv(dst.high_gp(), zero_reg);
      break;
    case LoadType::kI64Load32S:
      TurboAssembler::Lw(dst.low_gp(), src_op);
      TurboAssembler::srai(dst.high_gp(), dst.low_gp(), 31);
      break;
    case LoadType::kI32Load:
      TurboAssembler::Lw(dst.gp(), src_op);
      break;
    case LoadType::kI64Load: {
      Lw(dst.low_gp(), src_op);
      src_op = liftoff::GetMemOp(this, src_addr, offset_reg,
                                 offset_imm + kSystemPointerSize, kScratchReg);
      Lw(dst.high_gp(), src_op);
    } break;
    case LoadType::kF32Load:
      TurboAssembler::LoadFloat(dst.fp(), src_op);
      break;
    case LoadType::kF64Load:
      TurboAssembler::LoadDouble(dst.fp(), src_op);
      break;
    case LoadType::kS128Load: {
      VU.set(kScratchReg, E8, m1);
      Register src_reg = src_op.offset() == 0 ? src_op.rm() : kScratchReg;
      if (src_op.offset() != 0) {
        TurboAssembler::AddWord(src_reg, src_op.rm(), src_op.offset());
      }
      vl(dst.fp().toV(), src_reg, 0, E8);
      break;
    }
    default:
      UNREACHABLE();
  }

#if defined(V8_TARGET_BIG_ENDIAN)
  if (is_load_mem) {
    pinned.set(src_op.rm());
    liftoff::ChangeEndiannessLoad(this, dst, type, pinned);
  }
#endif
}

void LiftoffAssembler::Store(Register dst_addr, Register offset_reg,
                             uintptr_t offset_imm, LiftoffRegister src,
                             StoreType type, LiftoffRegList pinned,
                             uint32_t* protected_store_pc, bool is_store_mem,
                             bool i64_offset) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MemOperand dst_op =
      liftoff::GetMemOp(this, dst_addr, offset_reg, offset_imm, scratch);

#if defined(V8_TARGET_BIG_ENDIAN)
  if (is_store_mem) {
    pinned.set(dst_op.rm());
    LiftoffRegister tmp = GetUnusedRegister(src.reg_class(), pinned);
    // Save original value.
    Move(tmp, src, type.value_type());

    src = tmp;
    pinned.set(tmp);
    liftoff::ChangeEndiannessStore(this, src, type, pinned);
  }
#endif

  if (protected_store_pc) *protected_store_pc = pc_offset();

  switch (type.value()) {
    case StoreType::kI32Store8:
      Sb(src.gp(), dst_op);
      break;
    case StoreType::kI64Store8:
      Sb(src.low_gp(), dst_op);
      break;
    case StoreType::kI32Store16:
      TurboAssembler::Sh(src.gp(), dst_op);
      break;
    case StoreType::kI64Store16:
      TurboAssembler::Sh(src.low_gp(), dst_op);
      break;
    case StoreType::kI32Store:
      TurboAssembler::Sw(src.gp(), dst_op);
      break;
    case StoreType::kI64Store32:
      TurboAssembler::Sw(src.low_gp(), dst_op);
      break;
    case StoreType::kI64Store: {
      TurboAssembler::Sw(src.low_gp(), dst_op);
      dst_op = liftoff::GetMemOp(this, dst_addr, offset_reg,
                                 offset_imm + kSystemPointerSize, scratch);
      TurboAssembler::Sw(src.high_gp(), dst_op);
      break;
    }
    case StoreType::kF32Store:
      TurboAssembler::StoreFloat(src.fp(), dst_op);
      break;
    case StoreType::kF64Store:
      TurboAssembler::StoreDouble(src.fp(), dst_op);
      break;
    case StoreType::kS128Store: {
      VU.set(kScratchReg, E8, m1);
      Register dst_reg = dst_op.offset() == 0 ? dst_op.rm() : kScratchReg;
      if (dst_op.offset() != 0) {
        AddWord(kScratchReg, dst_op.rm(), dst_op.offset());
      }
      vs(src.fp().toV(), dst_reg, 0, VSew::E8);
      break;
    }
    default:
      UNREACHABLE();
  }
}

namespace liftoff {
#define __ lasm->

inline Register CalculateActualAddress(LiftoffAssembler* lasm,
                                       Register addr_reg, Register offset_reg,
                                       uintptr_t offset_imm,
                                       Register result_reg) {
  DCHECK_NE(offset_reg, no_reg);
  DCHECK_NE(addr_reg, no_reg);
  __ AddWord(result_reg, addr_reg, Operand(offset_reg));
  if (offset_imm != 0) {
    __ AddWord(result_reg, result_reg, Operand(offset_imm));
  }
  return result_reg;
}

enum class Binop { kAdd, kSub, kAnd, kOr, kXor, kExchange };

inline void AtomicBinop(LiftoffAssembler* lasm, Register dst_addr,
                        Register offset_reg, uintptr_t offset_imm,
                        LiftoffRegister value, LiftoffRegister result,
                        StoreType type, Binop op) {
  LiftoffRegList pinned{dst_addr, offset_reg, value, result};
  Register store_result = pinned.set(__ GetUnusedRegister(kGpReg, pinned)).gp();

  // Make sure that {result} is unique.
  Register result_reg = no_reg;
  Register value_reg = no_reg;
  bool change_result = false;
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI64Store16:
    case StoreType::kI64Store32:
      __ LoadConstant(result.high(), WasmValue(0));
      result_reg = result.low_gp();
      value_reg = value.low_gp();
      break;
    case StoreType::kI32Store8:
    case StoreType::kI32Store16:
    case StoreType::kI32Store:
      result_reg = result.gp();
      value_reg = value.gp();
      break;
    default:
      UNREACHABLE();
  }
  if (result_reg == value_reg || result_reg == dst_addr ||
      result_reg == offset_reg) {
    result_reg = __ GetUnusedRegister(kGpReg, pinned).gp();
    change_result = true;
  }

  UseScratchRegisterScope temps(lasm);
  Register actual_addr = liftoff::CalculateActualAddress(
      lasm, dst_addr, offset_reg, offset_imm, temps.Acquire());

  // Allocate an additional {temp} register to hold the result that should be
  // stored to memory. Note that {temp} and {store_result} are not allowed to be
  // the same register.
  Register temp = temps.Acquire();

  Label retry;
  __ bind(&retry);
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      __ lbu(result_reg, actual_addr, 0);
      __ sync();
      break;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      __ lhu(result_reg, actual_addr, 0);
      __ sync();
      break;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      __ lr_w(true, false, result_reg, actual_addr);
      break;
    default:
      UNREACHABLE();
  }

  switch (op) {
    case Binop::kAdd:
      __ add(temp, result_reg, value_reg);
      break;
    case Binop::kSub:
      __ sub(temp, result_reg, value_reg);
      break;
    case Binop::kAnd:
      __ and_(temp, result_reg, value_reg);
      break;
    case Binop::kOr:
      __ or_(temp, result_reg, value_reg);
      break;
    case Binop::kXor:
      __ xor_(temp, result_reg, value_reg);
      break;
    case Binop::kExchange:
      __ mv(temp, value_reg);
      break;
  }
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      __ sync();
      __ sb(temp, actual_addr, 0);
      __ sync();
      __ mv(store_result, zero_reg);
      break;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      __ sync();
      __ sh(temp, actual_addr, 0);
      __ sync();
      __ mv(store_result, zero_reg);
      break;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      __ sc_w(false, true, store_result, actual_addr, temp);
      break;
    default:
      UNREACHABLE();
  }

  __ bnez(store_result, &retry);
  if (change_result) {
    switch (type.value()) {
      case StoreType::kI64Store8:
      case StoreType::kI64Store16:
      case StoreType::kI64Store32:
        __ mv(result.low_gp(), result_reg);
        break;
      case StoreType::kI32Store8:
      case StoreType::kI32Store16:
      case StoreType::kI32Store:
        __ mv(result.gp(), result_reg);
        break;
      default:
        UNREACHABLE();
    }
  }
}

#undef __
}  // namespace liftoff

void LiftoffAssembler::AtomicLoad(LiftoffRegister dst, Register src_addr,
                                  Register offset_reg, uintptr_t offset_imm,
                                  LoadType type, LiftoffRegList pinned) {
  UseScratchRegisterScope temps(this);
  Register src_reg = liftoff::CalculateActualAddress(
      this, src_addr, offset_reg, offset_imm, temps.Acquire());
  Register dst_reg = no_reg;
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI32Load16U:
    case LoadType::kI32Load:
      dst_reg = dst.gp();
      break;
    case LoadType::kI64Load8U:
    case LoadType::kI64Load16U:
    case LoadType::kI64Load32U:
      dst_reg = dst.low_gp();
      LoadConstant(dst.high(), WasmValue(0));
      break;
    default:
      break;
  }
  switch (type.value()) {
    case LoadType::kI32Load8U:
    case LoadType::kI64Load8U:
      fence(PSR | PSW, PSR | PSW);
      lbu(dst_reg, src_reg, 0);
      fence(PSR, PSR | PSW);
      return;
    case LoadType::kI32Load16U:
    case LoadType::kI64Load16U:
      fence(PSR | PSW, PSR | PSW);
      lhu(dst_reg, src_reg, 0);
      fence(PSR, PSR | PSW);
      return;
    case LoadType::kI32Load:
    case LoadType::kI64Load32U:
      fence(PSR | PSW, PSR | PSW);
      lw(dst_reg, src_reg, 0);
      fence(PSR, PSR | PSW);
      return;
    case LoadType::kI64Load:
      fence(PSR | PSW, PSR | PSW);
      lw(dst.low_gp(), src_reg, liftoff::kLowWordOffset);
      lw(dst.high_gp(), src_reg, liftoff::kHighWordOffset);
      fence(PSR, PSR | PSW);
      return;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicStore(Register dst_addr, Register offset_reg,
                                   uintptr_t offset_imm, LiftoffRegister src,
                                   StoreType type, LiftoffRegList pinned) {
  UseScratchRegisterScope temps(this);
  Register dst_reg = liftoff::CalculateActualAddress(
      this, dst_addr, offset_reg, offset_imm, temps.Acquire());
  Register src_reg = no_reg;
  switch (type.value()) {
    case StoreType::kI32Store8:
    case StoreType::kI32Store16:
    case StoreType::kI32Store:
      src_reg = src.gp();
      break;
    case StoreType::kI64Store8:
    case StoreType::kI64Store16:
    case StoreType::kI64Store32:
      src_reg = src.low_gp();
      break;
    default:
      break;
  }
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      fence(PSR | PSW, PSW);
      sb(src_reg, dst_reg, 0);
      return;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      fence(PSR | PSW, PSW);
      sh(src_reg, dst_reg, 0);
      return;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      fence(PSR | PSW, PSW);
      sw(src_reg, dst_reg, 0);
      return;
    case StoreType::kI64Store:
      fence(PSR | PSW, PSW);
      sw(src.low_gp(), dst_reg, liftoff::kLowWordOffset);
      sw(src.high_gp(), dst_reg, liftoff::kHighWordOffset);
      return;
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::AtomicAdd(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  if (type.value() == StoreType::kI64Store) {
    bailout(kAtomics, "Atomic64");
  }
  if (type.value() == StoreType::kI32Store ||
      type.value() == StoreType::kI64Store32) {
    UseScratchRegisterScope temps(this);
    Register actual_addr = liftoff::CalculateActualAddress(
        this, dst_addr, offset_reg, offset_imm, temps.Acquire());
    if (type.value() == StoreType::kI64Store32) {
      result = result.low();
      value = value.low();
    }
    amoadd_w(true, true, result.gp(), actual_addr, value.gp());
    return;
  }

  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kAdd);
}

void LiftoffAssembler::AtomicSub(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  if (type.value() == StoreType::kI64Store) {
    bailout(kAtomics, "Atomic64");
  }
  if (type.value() == StoreType::kI32Store ||
      type.value() == StoreType::kI64Store32) {
    UseScratchRegisterScope temps(this);
    Register actual_addr = liftoff::CalculateActualAddress(
        this, dst_addr, offset_reg, offset_imm, temps.Acquire());
    if (type.value() == StoreType::kI64Store32) {
      result = result.low();
      value = value.low();
    }
    sub(kScratchReg, zero_reg, value.gp());
    amoadd_w(true, true, result.gp(), actual_addr, kScratchReg);
    return;
  }
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kSub);
}

void LiftoffAssembler::AtomicAnd(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  if (type.value() == StoreType::kI64Store) {
    bailout(kAtomics, "Atomic64");
  }
  if (type.value() == StoreType::kI32Store ||
      type.value() == StoreType::kI64Store32) {
    UseScratchRegisterScope temps(this);
    Register actual_addr = liftoff::CalculateActualAddress(
        this, dst_addr, offset_reg, offset_imm, temps.Acquire());
    if (type.value() == StoreType::kI64Store32) {
      result = result.low();
      value = value.low();
    }
    amoand_w(true, true, result.gp(), actual_addr, value.gp());
    return;
  }
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kAnd);
}

void LiftoffAssembler::AtomicOr(Register dst_addr, Register offset_reg,
                                uint32_t offset_imm, LiftoffRegister value,
                                LiftoffRegister result, StoreType type) {
  if (type.value() == StoreType::kI64Store) {
    bailout(kAtomics, "Atomic64");
  }
  if (type.value() == StoreType::kI32Store ||
      type.value() == StoreType::kI64Store32) {
    UseScratchRegisterScope temps(this);
    Register actual_addr = liftoff::CalculateActualAddress(
        this, dst_addr, offset_reg, offset_imm, temps.Acquire());
    if (type.value() == StoreType::kI64Store32) {
      result = result.low();
      value = value.low();
    }
    amoor_w(true, true, result.gp(), actual_addr, value.gp());
    return;
  }
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kOr);
}

void LiftoffAssembler::AtomicXor(Register dst_addr, Register offset_reg,
                                 uint32_t offset_imm, LiftoffRegister value,
                                 LiftoffRegister result, StoreType type) {
  if (type.value() == StoreType::kI64Store) {
    bailout(kAtomics, "Atomic64");
  }
  if (type.value() == StoreType::kI32Store ||
      type.value() == StoreType::kI64Store32) {
    UseScratchRegisterScope temps(this);
    Register actual_addr = liftoff::CalculateActualAddress(
        this, dst_addr, offset_reg, offset_imm, temps.Acquire());
    if (type.value() == StoreType::kI64Store32) {
      result = result.low();
      value = value.low();
    }
    amoxor_w(true, true, result.gp(), actual_addr, value.gp());
    return;
  }
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kXor);
}

void LiftoffAssembler::AtomicExchange(Register dst_addr, Register offset_reg,
                                      uint32_t offset_imm,
                                      LiftoffRegister value,
                                      LiftoffRegister result, StoreType type) {
  if (type.value() == StoreType::kI64Store) {
    bailout(kAtomics, "Atomic64");
  }
  if (type.value() == StoreType::kI32Store ||
      type.value() == StoreType::kI64Store32) {
    UseScratchRegisterScope temps(this);
    Register actual_addr = liftoff::CalculateActualAddress(
        this, dst_addr, offset_reg, offset_imm, temps.Acquire());
    if (type.value() == StoreType::kI64Store32) {
      result = result.low();
      value = value.low();
    }
    amoswap_w(true, true, result.gp(), actual_addr, value.gp());
    return;
  }
  liftoff::AtomicBinop(this, dst_addr, offset_reg, offset_imm, value, result,
                       type, liftoff::Binop::kExchange);
}

void LiftoffAssembler::AtomicCompareExchange(
    Register dst_addr, Register offset_reg, uintptr_t offset_imm,
    LiftoffRegister expected, LiftoffRegister new_value, LiftoffRegister result,
    StoreType type) {
  ASM_CODE_COMMENT(this);
  LiftoffRegList pinned{dst_addr, offset_reg, expected, new_value, result};

  if (type.value() == StoreType::kI64Store) {
    Register actual_addr = liftoff::CalculateActualAddress(
        this, dst_addr, offset_reg, offset_imm, kScratchReg);
    Mv(a0, actual_addr);
    FrameScope scope(this, StackFrame::MANUAL);
    PushCallerSaved(SaveFPRegsMode::kIgnore, a0, a1);
    PrepareCallCFunction(5, 0, kScratchReg);
    CallCFunction(ExternalReference::atomic_pair_compare_exchange_function(), 5,
                  0);
    PopCallerSaved(SaveFPRegsMode::kIgnore, a0, a1);
    Mv(result.low_gp(), a0);
    Mv(result.high_gp(), a1);
    return;
  }

  // Make sure that {result} is unique.
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI64Store16:
    case StoreType::kI64Store32:
      LoadConstant(result.high(), WasmValue(0));
      result = result.low();
      new_value = new_value.low();
      expected = expected.low();
      break;
    case StoreType::kI32Store8:
    case StoreType::kI32Store16:
    case StoreType::kI32Store:
      break;
    default:
      UNREACHABLE();
  }

  UseScratchRegisterScope temps(this);
  Register actual_addr = liftoff::CalculateActualAddress(
      this, dst_addr, offset_reg, offset_imm, kScratchReg);

  Register temp0 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp1 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();
  Register temp2 = pinned.set(GetUnusedRegister(kGpReg, pinned)).gp();

  if (type.value() != StoreType::kI32Store &&
      type.value() != StoreType::kI64Store32) {
    And(temp1, actual_addr, 0x3);
    SubWord(temp0, actual_addr, Operand(temp1));
    SllWord(temp1, temp1, 3);
  }
  Label retry;
  Label done;
  bind(&retry);
  switch (type.value()) {
    case StoreType::kI64Store8:
    case StoreType::kI32Store8:
      lr_w(true, true, temp2, temp0);
      ExtractBits(result.gp(), temp2, temp1, 8, false);
      ExtractBits(temp2, expected.gp(), zero_reg, 8, false);
      Branch(&done, ne, temp2, Operand(result.gp()));
      InsertBits(temp2, new_value.gp(), temp1, 8);
      sc_w(true, true, temp2, temp0, temp2);
      break;
    case StoreType::kI64Store16:
    case StoreType::kI32Store16:
      lr_w(true, true, temp2, temp0);
      ExtractBits(result.gp(), temp2, temp1, 16, false);
      ExtractBits(temp2, expected.gp(), zero_reg, 16, false);
      Branch(&done, ne, temp2, Operand(result.gp()));
      InsertBits(temp2, new_value.gp(), temp1, 16);
      sc_w(true, true, temp2, temp0, temp2);
      break;
    case StoreType::kI64Store32:
    case StoreType::kI32Store:
      lr_w(true, true, result.gp(), actual_addr);
      Branch(&done, ne, result.gp(), Operand(expected.gp()));
      sc_w(true, true, temp2, actual_addr, new_value.gp());
      break;
    default:
      UNREACHABLE();
  }
  bnez(temp2, &retry);
  bind(&done);
}

void LiftoffAssembler::AtomicFence() { sync(); }

void LiftoffAssembler::LoadCallerFrameSlot(LiftoffRegister dst,
                                           uint32_t caller_slot_idx,
                                           ValueKind kind) {
  int32_t offset = kSystemPointerSize * (caller_slot_idx + 1);
  liftoff::Load(this, dst, fp, offset, kind);
}

void LiftoffAssembler::StoreCallerFrameSlot(LiftoffRegister src,
                                            uint32_t caller_slot_idx,
                                            ValueKind kind) {
  int32_t offset = kSystemPointerSize * (caller_slot_idx + 1);
  liftoff::Store(this, fp, offset, src, kind);
}

void LiftoffAssembler::LoadReturnStackSlot(LiftoffRegister dst, int offset,
                                           ValueKind kind) {
  liftoff::Load(this, dst, sp, offset, kind);
}

void LiftoffAssembler::MoveStackValue(uint32_t dst_offset, uint32_t src_offset,
                                      ValueKind kind) {
  DCHECK_NE(dst_offset, src_offset);

  MemOperand src = liftoff::GetStackSlot(src_offset);
  MemOperand dst = liftoff::GetStackSlot(dst_offset);
  switch (kind) {
    case kI32:
      Lw(kScratchReg, src);
      Sw(kScratchReg, dst);
      break;
    case kI64:
    case kRef:
    case kRefNull:
    case kRtt:
      Lw(kScratchReg, src);
      Sw(kScratchReg, dst);
      src = liftoff::GetStackSlot(src_offset - 4);
      dst = liftoff::GetStackSlot(dst_offset - 4);
      Lw(kScratchReg, src);
      Sw(kScratchReg, dst);
      break;
    case kF32:
      LoadFloat(kScratchDoubleReg, src);
      StoreFloat(kScratchDoubleReg, dst);
      break;
    case kF64:
      TurboAssembler::LoadDouble(kScratchDoubleReg, src);
      TurboAssembler::StoreDouble(kScratchDoubleReg, dst);
      break;
    case kS128: {
      VU.set(kScratchReg, E8, m1);
      Register src_reg = src.offset() == 0 ? src.rm() : kScratchReg;
      if (src.offset() != 0) {
        TurboAssembler::AddWord(src_reg, src.rm(), src.offset());
      }
      vl(kSimd128ScratchReg, src_reg, 0, E8);
      Register dst_reg = dst.offset() == 0 ? dst.rm() : kScratchReg;
      if (dst.offset() != 0) {
        AddWord(kScratchReg, dst.rm(), dst.offset());
      }
      vs(kSimd128ScratchReg, dst_reg, 0, VSew::E8);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Move(Register dst, Register src, ValueKind kind) {
  DCHECK_NE(dst, src);
  // TODO(ksreten): Handle different sizes here.
  TurboAssembler::Move(dst, src);
}

void LiftoffAssembler::Move(DoubleRegister dst, DoubleRegister src,
                            ValueKind kind) {
  DCHECK_NE(dst, src);
  if (kind != kS128) {
    TurboAssembler::Move(dst, src);
  } else {
    TurboAssembler::vmv_vv(dst.toV(), dst.toV());
  }
}

void LiftoffAssembler::Spill(int offset, LiftoffRegister reg, ValueKind kind) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  switch (kind) {
    case kI32:
    case kRef:
    case kRefNull:
    case kRtt:
      Sw(reg.gp(), dst);
      break;
    case kI64:
      Sw(reg.low_gp(), liftoff::GetHalfStackSlot(offset, kLowWord));
      Sw(reg.high_gp(), liftoff::GetHalfStackSlot(offset, kHighWord));
      break;
    case kF32:
      StoreFloat(reg.fp(), dst);
      break;
    case kF64:
      TurboAssembler::StoreDouble(reg.fp(), dst);
      break;
    case kS128: {
      VU.set(kScratchReg, E8, m1);
      Register dst_reg = dst.offset() == 0 ? dst.rm() : kScratchReg;
      if (dst.offset() != 0) {
        AddWord(kScratchReg, dst.rm(), dst.offset());
      }
      vs(reg.fp().toV(), dst_reg, 0, VSew::E8);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::Spill(int offset, WasmValue value) {
  RecordUsedSpillOffset(offset);
  MemOperand dst = liftoff::GetStackSlot(offset);
  switch (value.type().kind()) {
    case kI32:
    case kRef:
    case kRefNull: {
      LiftoffRegister tmp = GetUnusedRegister(kGpReg, {});
      TurboAssembler::li(tmp.gp(), Operand(value.to_i32()));
      Sw(tmp.gp(), dst);
      break;
    }
    case kI64: {
      LiftoffRegister tmp = GetUnusedRegister(kGpRegPair, {});

      int32_t low_word = value.to_i64();
      int32_t high_word = value.to_i64() >> 32;
      TurboAssembler::li(tmp.low_gp(), Operand(low_word));
      TurboAssembler::li(tmp.high_gp(), Operand(high_word));

      Sw(tmp.low_gp(), liftoff::GetHalfStackSlot(offset, kLowWord));
      Sw(tmp.high_gp(), liftoff::GetHalfStackSlot(offset, kHighWord));
      break;
      break;
    }
    default:
      // kWasmF32 and kWasmF64 are unreachable, since those
      // constants are not tracked.
      UNREACHABLE();
  }
}

void LiftoffAssembler::Fill(LiftoffRegister reg, int offset, ValueKind kind) {
  MemOperand src = liftoff::GetStackSlot(offset);
  switch (kind) {
    case kI32:
    case kRef:
    case kRefNull:
      Lw(reg.gp(), src);
      break;
    case kI64:
      Lw(reg.low_gp(), liftoff::GetHalfStackSlot(offset, kLowWord));
      Lw(reg.high_gp(), liftoff::GetHalfStackSlot(offset, kHighWord));
      break;
    case kF32:
      LoadFloat(reg.fp(), src);
      break;
    case kF64:
      TurboAssembler::LoadDouble(reg.fp(), src);
      break;
    case kS128: {
      VU.set(kScratchReg, E8, m1);
      Register src_reg = src.offset() == 0 ? src.rm() : kScratchReg;
      if (src.offset() != 0) {
        TurboAssembler::AddWord(src_reg, src.rm(), src.offset());
      }
      vl(reg.fp().toV(), src_reg, 0, E8);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void LiftoffAssembler::FillI64Half(Register reg, int offset, RegPairHalf half) {
  Lw(reg, liftoff::GetHalfStackSlot(offset, half));
}

void LiftoffAssembler::FillStackSlotsWithZero(int start, int size) {
  DCHECK_LT(0, size);
  RecordUsedSpillOffset(start + size);

  // TODO(riscv32): check

  if (size <= 12 * kStackSlotSize) {
    // Special straight-line code for up to 12 slots. Generates one
    // instruction per slot (<= 12 instructions total).
    uint32_t remainder = size;
    for (; remainder >= kStackSlotSize; remainder -= kStackSlotSize) {
      Sw(zero_reg, liftoff::GetStackSlot(start + remainder));
      Sw(zero_reg, liftoff::GetStackSlot(start + remainder - 4));
    }
    DCHECK(remainder == 4 || remainder == 0);
    if (remainder) {
      Sw(zero_reg, liftoff::GetStackSlot(start + remainder));
    }
  } else {
    // General case for bigger counts (12 instructions).
    // Use a0 for start address (inclusive), a1 for end address (exclusive).
    Push(a1, a0);
    AddWord(a0, fp, Operand(-start - size));
    AddWord(a1, fp, Operand(-start));

    Label loop;
    bind(&loop);
    Sw(zero_reg, MemOperand(a0));
    addi(a0, a0, kSystemPointerSize);
    BranchShort(&loop, ne, a0, Operand(a1));

    Pop(a1, a0);
  }
}

void LiftoffAssembler::emit_i64_clz(LiftoffRegister dst, LiftoffRegister src) {
  // return high == 0 ? 32 + CLZ32(low) : CLZ32(high);
  Label done;
  Label high_is_zero;
  Branch(&high_is_zero, eq, src.high_gp(), Operand(zero_reg));

  Clz32(dst.low_gp(), src.high_gp());
  jmp(&done);

  bind(&high_is_zero);
  Clz32(dst.low_gp(), src.low_gp());
  AddWord(dst.low_gp(), dst.low_gp(), Operand(32));

  bind(&done);
  mv(dst.high_gp(), zero_reg);  // High word of result is always 0.
}

void LiftoffAssembler::emit_i64_ctz(LiftoffRegister dst, LiftoffRegister src) {
  // return low == 0 ? 32 + CTZ32(high) : CTZ32(low);
  Label done;
  Label low_is_zero;
  Branch(&low_is_zero, eq, src.low_gp(), Operand(zero_reg));

  Ctz32(dst.low_gp(), src.low_gp());
  jmp(&done);

  bind(&low_is_zero);
  Ctz32(dst.low_gp(), src.high_gp());
  AddWord(dst.low_gp(), dst.low_gp(), Operand(32));

  bind(&done);
  mv(dst.high_gp(), zero_reg);  // High word of result is always 0.
}

bool LiftoffAssembler::emit_i64_popcnt(LiftoffRegister dst,
                                       LiftoffRegister src) {
  // Produce partial popcnts in the two dst registers.
  Register src1 = src.high_gp() == dst.low_gp() ? src.high_gp() : src.low_gp();
  Register src2 = src.high_gp() == dst.low_gp() ? src.low_gp() : src.high_gp();
  TurboAssembler::Popcnt32(dst.low_gp(), src1, kScratchReg);
  TurboAssembler::Popcnt32(dst.high_gp(), src2, kScratchReg);
  // Now add the two into the lower dst reg and clear the higher dst reg.
  AddWord(dst.low_gp(), dst.low_gp(), dst.high_gp());
  mv(dst.high_gp(), zero_reg);
  return true;
}

void LiftoffAssembler::emit_i32_mul(Register dst, Register lhs, Register rhs) {
  TurboAssembler::Mul(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divs(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));

  // Check if lhs == kMinInt and rhs == -1, since this case is unrepresentable.
  TurboAssembler::CompareI(kScratchReg, lhs, Operand(kMinInt), ne);
  TurboAssembler::CompareI(kScratchReg2, rhs, Operand(-1), ne);
  add(kScratchReg, kScratchReg, kScratchReg2);
  TurboAssembler::Branch(trap_div_unrepresentable, eq, kScratchReg,
                         Operand(zero_reg));

  TurboAssembler::Div(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_divu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Divu(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_rems(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Mod(dst, lhs, rhs);
}

void LiftoffAssembler::emit_i32_remu(Register dst, Register lhs, Register rhs,
                                     Label* trap_div_by_zero) {
  TurboAssembler::Branch(trap_div_by_zero, eq, rhs, Operand(zero_reg));
  TurboAssembler::Modu(dst, lhs, rhs);
}

#define I32_BINOP(name, instruction)                                 \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register lhs, \
                                         Register rhs) {             \
    instruction(dst, lhs, rhs);                                      \
  }

// clang-format off
I32_BINOP(add, add)
I32_BINOP(sub, sub)
I32_BINOP(and, and_)
I32_BINOP(or, or_)
I32_BINOP(xor, xor_)
// clang-format on

#undef I32_BINOP

#define I32_BINOP_I(name, instruction)                                  \
  void LiftoffAssembler::emit_i32_##name##i(Register dst, Register lhs, \
                                            int32_t imm) {              \
    instruction(dst, lhs, Operand(imm));                                \
  }

// clang-format off
I32_BINOP_I(add, AddWord)
I32_BINOP_I(sub, SubWord)
I32_BINOP_I(and, And)
I32_BINOP_I(or, Or)
I32_BINOP_I(xor, Xor)
// clang-format on

#undef I32_BINOP_I

void LiftoffAssembler::emit_i32_clz(Register dst, Register src) {
  TurboAssembler::Clz32(dst, src);
}

void LiftoffAssembler::emit_i32_ctz(Register dst, Register src) {
  TurboAssembler::Ctz32(dst, src);
}

bool LiftoffAssembler::emit_i32_popcnt(Register dst, Register src) {
  TurboAssembler::Popcnt32(dst, src, kScratchReg);
  return true;
}

#define I32_SHIFTOP(name, instruction)                               \
  void LiftoffAssembler::emit_i32_##name(Register dst, Register src, \
                                         Register amount) {          \
    instruction(dst, src, amount);                                   \
  }
#define I32_SHIFTOP_I(name, instruction)                                \
  void LiftoffAssembler::emit_i32_##name##i(Register dst, Register src, \
                                            int amount) {               \
    instruction(dst, src, amount & 31);                                 \
  }

I32_SHIFTOP(shl, sll)
I32_SHIFTOP(sar, sra)
I32_SHIFTOP(shr, srl)

I32_SHIFTOP_I(shl, slli)
I32_SHIFTOP_I(sar, srai)
I32_SHIFTOP_I(shr, srli)

#undef I32_SHIFTOP
#undef I32_SHIFTOP_I

void LiftoffAssembler::emit_i64_mul(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  TurboAssembler::MulPair(dst.low_gp(), dst.high_gp(), lhs.low_gp(),
                          lhs.high_gp(), rhs.low_gp(), rhs.high_gp(),
                          kScratchReg, kScratchReg2);
}

bool LiftoffAssembler::emit_i64_divs(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero,
                                     Label* trap_div_unrepresentable) {
  return false;
}

bool LiftoffAssembler::emit_i64_divu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  return false;
}

bool LiftoffAssembler::emit_i64_rems(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  return false;
}

bool LiftoffAssembler::emit_i64_remu(LiftoffRegister dst, LiftoffRegister lhs,
                                     LiftoffRegister rhs,
                                     Label* trap_div_by_zero) {
  return false;
}

namespace liftoff {

inline bool IsRegInRegPair(LiftoffRegister pair, Register reg) {
  DCHECK(pair.is_gp_pair());
  return pair.low_gp() == reg || pair.high_gp() == reg;
}

inline void Emit64BitShiftOperation(
    LiftoffAssembler* assm, LiftoffRegister dst, LiftoffRegister src,
    Register amount,
    void (TurboAssembler::*emit_shift)(Register, Register, Register, Register,
                                       Register, Register, Register)) {
  LiftoffRegList pinned{dst, src, amount};

  // If some of destination registers are in use, get another, unused pair.
  // That way we prevent overwriting some input registers while shifting.
  // Do this before any branch so that the cache state will be correct for
  // all conditions.
  Register amount_capped =
      pinned.set(assm->GetUnusedRegister(kGpReg, pinned).gp());
  assm->And(amount_capped, amount, Operand(63));
  if (liftoff::IsRegInRegPair(dst, amount) || dst.overlaps(src)) {
    // Do the actual shift.
    LiftoffRegister tmp = assm->GetUnusedRegister(kGpRegPair, pinned);
    (assm->*emit_shift)(tmp.low_gp(), tmp.high_gp(), src.low_gp(),
                        src.high_gp(), amount_capped, kScratchReg,
                        kScratchReg2);

    // Place result in destination register.
    assm->TurboAssembler::Move(dst.high_gp(), tmp.high_gp());
    assm->TurboAssembler::Move(dst.low_gp(), tmp.low_gp());
  } else {
    (assm->*emit_shift)(dst.low_gp(), dst.high_gp(), src.low_gp(),
                        src.high_gp(), amount_capped, kScratchReg,
                        kScratchReg2);
  }
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_add(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  TurboAssembler::AddPair(dst.low_gp(), dst.high_gp(), lhs.low_gp(),
                          lhs.high_gp(), rhs.low_gp(), rhs.high_gp(),
                          kScratchReg, kScratchReg2);
}

void LiftoffAssembler::emit_i64_addi(LiftoffRegister dst, LiftoffRegister lhs,
                                     int64_t imm) {
  LiftoffRegister imm_reg =
      GetUnusedRegister(kGpRegPair, LiftoffRegList{dst, lhs});
  int32_t imm_low_word = static_cast<int32_t>(imm);
  int32_t imm_high_word = static_cast<int32_t>(imm >> 32);

  // TODO(riscv32): are there some optimization we can make without
  // materializing?
  TurboAssembler::li(imm_reg.low_gp(), imm_low_word);
  TurboAssembler::li(imm_reg.high_gp(), imm_high_word);
  TurboAssembler::AddPair(dst.low_gp(), dst.high_gp(), lhs.low_gp(),
                          lhs.high_gp(), imm_reg.low_gp(), imm_reg.high_gp(),
                          kScratchReg, kScratchReg2);
}

void LiftoffAssembler::emit_i64_sub(LiftoffRegister dst, LiftoffRegister lhs,
                                    LiftoffRegister rhs) {
  TurboAssembler::SubPair(dst.low_gp(), dst.high_gp(), lhs.low_gp(),
                          lhs.high_gp(), rhs.low_gp(), rhs.high_gp(),
                          kScratchReg, kScratchReg2);
}

void LiftoffAssembler::emit_i64_shl(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  ASM_CODE_COMMENT(this);
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShlPair);
}

void LiftoffAssembler::emit_i64_shli(LiftoffRegister dst, LiftoffRegister src,
                                     int amount) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  LiftoffRegister temp = GetUnusedRegister(kGpReg, LiftoffRegList{dst, src});
  temps.Include(temp.gp());
  // {src.low_gp()} will still be needed after writing {dst.high_gp()} and
  // {dst.low_gp()}.
  Register src_low = liftoff::EnsureNoAlias(this, src.low_gp(), dst, &temps);
  Register src_high = liftoff::EnsureNoAlias(this, src.high_gp(), dst, &temps);
  // {src.high_gp()} will still be needed after writing {dst.high_gp()}.
  DCHECK_NE(dst.low_gp(), kScratchReg);
  DCHECK_NE(dst.high_gp(), kScratchReg);

  TurboAssembler::ShlPair(dst.low_gp(), dst.high_gp(), src_low, src_high,
                          amount, kScratchReg, kScratchReg2);
}

void LiftoffAssembler::emit_i64_sar(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::SarPair);
}

void LiftoffAssembler::emit_i64_sari(LiftoffRegister dst, LiftoffRegister src,
                                     int amount) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  LiftoffRegister temp = GetUnusedRegister(kGpReg, LiftoffRegList{dst, src});
  temps.Include(temp.gp());
  // {src.low_gp()} will still be needed after writing {dst.high_gp()} and
  // {dst.low_gp()}.
  Register src_low = liftoff::EnsureNoAlias(this, src.low_gp(), dst, &temps);
  Register src_high = liftoff::EnsureNoAlias(this, src.high_gp(), dst, &temps);
  DCHECK_NE(dst.low_gp(), kScratchReg);
  DCHECK_NE(dst.high_gp(), kScratchReg);

  TurboAssembler::SarPair(dst.low_gp(), dst.high_gp(), src_low, src_high,
                          amount, kScratchReg, kScratchReg2);
}

void LiftoffAssembler::emit_i64_shr(LiftoffRegister dst, LiftoffRegister src,
                                    Register amount) {
  liftoff::Emit64BitShiftOperation(this, dst, src, amount,
                                   &TurboAssembler::ShrPair);
}

void LiftoffAssembler::emit_i64_shri(LiftoffRegister dst, LiftoffRegister src,
                                     int amount) {
  ASM_CODE_COMMENT(this);
  UseScratchRegisterScope temps(this);
  LiftoffRegister temp = GetUnusedRegister(kGpReg, LiftoffRegList{dst, src});
  temps.Include(temp.gp());
  // {src.low_gp()} will still be needed after writing {dst.high_gp()} and
  // {dst.low_gp()}.
  Register src_low = liftoff::EnsureNoAlias(this, src.low_gp(), dst, &temps);
  Register src_high = liftoff::EnsureNoAlias(this, src.high_gp(), dst, &temps);
  DCHECK_NE(dst.low_gp(), kScratchReg);
  DCHECK_NE(dst.high_gp(), kScratchReg);

  TurboAssembler::ShrPair(dst.low_gp(), dst.high_gp(), src_low, src_high,
                          amount, kScratchReg, kScratchReg2);
}

#define FP_UNOP_RETURN_FALSE(name)                                             \
  bool LiftoffAssembler::emit_##name(DoubleRegister dst, DoubleRegister src) { \
    return false;                                                              \
  }

FP_UNOP_RETURN_FALSE(f64_ceil)
FP_UNOP_RETURN_FALSE(f64_floor)
FP_UNOP_RETURN_FALSE(f64_trunc)
FP_UNOP_RETURN_FALSE(f64_nearest_int)

#undef FP_UNOP_RETURN_FALSE

bool LiftoffAssembler::emit_type_conversion(WasmOpcode opcode,
                                            LiftoffRegister dst,
                                            LiftoffRegister src, Label* trap) {
  switch (opcode) {
    case kExprI32ConvertI64:
      TurboAssembler::Move(dst.gp(), src.low_gp());
      return true;
    case kExprI32SConvertF32:
    case kExprI32UConvertF32:
    case kExprI32SConvertF64:
    case kExprI32UConvertF64:
    case kExprI64SConvertF32:
    case kExprI64UConvertF32:
    case kExprI64SConvertF64:
    case kExprI64UConvertF64:
    case kExprF32ConvertF64: {
      // real conversion, if src is out-of-bound of target integer types,
      // kScratchReg is set to 0
      switch (opcode) {
        case kExprI32SConvertF32:
          Trunc_w_s(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI32UConvertF32:
          Trunc_uw_s(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI32SConvertF64:
          Trunc_w_d(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprI32UConvertF64:
          Trunc_uw_d(dst.gp(), src.fp(), kScratchReg);
          break;
        case kExprF32ConvertF64:
          fcvt_s_d(dst.fp(), src.fp());
          break;
        case kExprI64SConvertF32:
        case kExprI64UConvertF32:
        case kExprI64SConvertF64:
        case kExprI64UConvertF64:
          return false;
        default:
          UNREACHABLE();
      }

      // Checking if trap.
      if (trap != nullptr) {
        TurboAssembler::Branch(trap, eq, kScratchReg, Operand(zero_reg));
      }

      return true;
    }
    case kExprI32ReinterpretF32:
      TurboAssembler::ExtractLowWordFromF64(dst.gp(), src.fp());
      return true;
    case kExprI64SConvertI32:
      TurboAssembler::Move(dst.low_gp(), src.gp());
      TurboAssembler::Move(dst.high_gp(), src.gp());
      srai(dst.high_gp(), dst.high_gp(), 31);
      return true;
    case kExprI64UConvertI32:
      TurboAssembler::Move(dst.low_gp(), src.gp());
      TurboAssembler::Move(dst.high_gp(), zero_reg);
      return true;
    case kExprI64ReinterpretF64:
      SubWord(sp, sp, kDoubleSize);
      StoreDouble(src.fp(), MemOperand(sp, 0));
      Lw(dst.low_gp(), MemOperand(sp, 0));
      Lw(dst.high_gp(), MemOperand(sp, 4));
      AddWord(sp, sp, kDoubleSize);
      return true;
    case kExprF32SConvertI32: {
      TurboAssembler::Cvt_s_w(dst.fp(), src.gp());
      return true;
    }
    case kExprF32UConvertI32:
      TurboAssembler::Cvt_s_uw(dst.fp(), src.gp());
      return true;
    case kExprF32ReinterpretI32:
      fmv_w_x(dst.fp(), src.gp());
      return true;
    case kExprF64SConvertI32: {
      TurboAssembler::Cvt_d_w(dst.fp(), src.gp());
      return true;
    }
    case kExprF64UConvertI32:
      TurboAssembler::Cvt_d_uw(dst.fp(), src.gp());
      return true;
    case kExprF64ConvertF32:
      fcvt_d_s(dst.fp(), src.fp());
      return true;
    case kExprF64ReinterpretI64:
      SubWord(sp, sp, kDoubleSize);
      Sw(src.low_gp(), MemOperand(sp, 0));
      Sw(src.high_gp(), MemOperand(sp, 4));
      LoadDouble(dst.fp(), MemOperand(sp, 0));
      AddWord(sp, sp, kDoubleSize);
      return true;
    case kExprI32SConvertSatF32: {
      fcvt_w_s(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_s(dst.gp(), src.fp());
      return true;
    }
    case kExprI32UConvertSatF32: {
      fcvt_wu_s(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_s(dst.gp(), src.fp());
      return true;
    }
    case kExprI32SConvertSatF64: {
      fcvt_w_d(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_d(dst.gp(), src.fp());
      return true;
    }
    case kExprI32UConvertSatF64: {
      fcvt_wu_d(dst.gp(), src.fp(), RTZ);
      Clear_if_nan_d(dst.gp(), src.fp());
      return true;
    }
    case kExprI64SConvertSatF32:
    case kExprI64UConvertSatF32:
    case kExprI64SConvertSatF64:
    case kExprI64UConvertSatF64:
      return false;
    default:
      return false;
  }
}

void LiftoffAssembler::emit_i32_signextend_i8(Register dst, Register src) {
  slli(dst, src, 32 - 8);
  srai(dst, dst, 32 - 8);
}

void LiftoffAssembler::emit_i32_signextend_i16(Register dst, Register src) {
  slli(dst, src, 32 - 16);
  srai(dst, dst, 32 - 16);
}

void LiftoffAssembler::emit_i64_signextend_i8(LiftoffRegister dst,
                                              LiftoffRegister src) {
  emit_i32_signextend_i8(dst.low_gp(), src.low_gp());
  srai(dst.high_gp(), dst.low_gp(), 31);
}

void LiftoffAssembler::emit_i64_signextend_i16(LiftoffRegister dst,
                                               LiftoffRegister src) {
  emit_i32_signextend_i16(dst.low_gp(), src.low_gp());
  srai(dst.high_gp(), dst.low_gp(), 31);
}

void LiftoffAssembler::emit_i64_signextend_i32(LiftoffRegister dst,
                                               LiftoffRegister src) {
  mv(dst.low_gp(), src.low_gp());
  srai(dst.high_gp(), src.low_gp(), 31);
}

void LiftoffAssembler::emit_jump(Label* label) {
  TurboAssembler::Branch(label);
}

void LiftoffAssembler::emit_jump(Register target) {
  TurboAssembler::Jump(target);
}

void LiftoffAssembler::emit_cond_jump(LiftoffCondition liftoff_cond,
                                      Label* label, ValueKind kind,
                                      Register lhs, Register rhs,
                                      const FreezeCacheState& frozen) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  if (rhs == no_reg) {
    DCHECK(kind == kI32);
    TurboAssembler::Branch(label, cond, lhs, Operand(zero_reg));
  } else {
    DCHECK((kind == kI32) ||
           (is_reference(kind) &&
            (liftoff_cond == kEqual || liftoff_cond == kUnequal)));
    TurboAssembler::Branch(label, cond, lhs, Operand(rhs));
  }
}

void LiftoffAssembler::emit_i32_cond_jumpi(LiftoffCondition liftoff_cond,
                                           Label* label, Register lhs,
                                           int32_t imm,
                                           const FreezeCacheState& frozen) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  TurboAssembler::Branch(label, cond, lhs, Operand(imm));
}

void LiftoffAssembler::emit_i32_subi_jump_negative(
    Register value, int subtrahend, Label* result_negative,
    const FreezeCacheState& frozen) {
  SubWord(value, value, Operand(subtrahend));
  TurboAssembler::Branch(result_negative, lt, value, Operand(zero_reg));
}

void LiftoffAssembler::emit_i32_eqz(Register dst, Register src) {
  TurboAssembler::Sltu(dst, src, 1);
}

void LiftoffAssembler::emit_i32_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, Register lhs,
                                         Register rhs) {
  Condition cond = liftoff::ToCondition(liftoff_cond);
  TurboAssembler::CompareI(dst, lhs, Operand(rhs), cond);
}

void LiftoffAssembler::emit_i64_eqz(Register dst, LiftoffRegister src) {
  Register tmp = GetUnusedRegister(kGpReg, LiftoffRegList{src, dst}).gp();
  Sltu(tmp, src.low_gp(), 1);
  Sltu(dst, src.high_gp(), 1);
  and_(dst, dst, tmp);
}

namespace liftoff {
inline LiftoffCondition cond_make_unsigned(LiftoffCondition cond) {
  switch (cond) {
    case kSignedLessThan:
      return kUnsignedLessThan;
    case kSignedLessEqual:
      return kUnsignedLessEqual;
    case kSignedGreaterThan:
      return kUnsignedGreaterThan;
    case kSignedGreaterEqual:
      return kUnsignedGreaterEqual;
    default:
      return cond;
  }
}
}  // namespace liftoff

void LiftoffAssembler::emit_i64_set_cond(LiftoffCondition liftoff_cond,
                                         Register dst, LiftoffRegister lhs,
                                         LiftoffRegister rhs) {
  ASM_CODE_COMMENT(this);
  Condition cond = liftoff::ToCondition(liftoff_cond);
  Label low, cont;

  // For signed i64 comparisons, we still need to use unsigned comparison for
  // the low word (the only bit carrying signedness information is the MSB in
  // the high word).
  Condition unsigned_cond =
      liftoff::ToCondition(liftoff::cond_make_unsigned(liftoff_cond));

  Register tmp = dst;
  if (liftoff::IsRegInRegPair(lhs, dst) || liftoff::IsRegInRegPair(rhs, dst)) {
    tmp = GetUnusedRegister(kGpReg, LiftoffRegList{dst, lhs, rhs}).gp();
  }

  // Write 1 initially in tmp register.
  TurboAssembler::li(tmp, 1);

  // If high words are equal, then compare low words, else compare high.
  Branch(&low, eq, lhs.high_gp(), Operand(rhs.high_gp()));

  Branch(&cont, cond, lhs.high_gp(), Operand(rhs.high_gp()));
  mv(tmp, zero_reg);
  Branch(&cont);

  bind(&low);
  if (unsigned_cond == cond) {
    Branch(&cont, cond, lhs.low_gp(), Operand(rhs.low_gp()));
    mv(tmp, zero_reg);
  } else {
    Label lt_zero;
    Branch(&lt_zero, lt, lhs.high_gp(), Operand(zero_reg));
    Branch(&cont, unsigned_cond, lhs.low_gp(), Operand(rhs.low_gp()));
    mv(tmp, zero_reg);
    Branch(&cont);
    bind(&lt_zero);
    Branch(&cont, unsigned_cond, rhs.low_gp(), Operand(lhs.low_gp()));
    mv(tmp, zero_reg);
    Branch(&cont);
  }
  bind(&cont);
  // Move result to dst register if needed.
  TurboAssembler::Move(dst, tmp);
}

void LiftoffAssembler::IncrementSmi(LiftoffRegister dst, int offset) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  SmiUntag(scratch, MemOperand(dst.gp(), offset));
  AddWord(scratch, scratch, Operand(1));
  SmiTag(scratch);
  Sw(scratch, MemOperand(dst.gp(), offset));
}

void LiftoffAssembler::LoadTransform(LiftoffRegister dst, Register src_addr,
                                     Register offset_reg, uintptr_t offset_imm,
                                     LoadType type,
                                     LoadTransformationKind transform,
                                     uint32_t* protected_load_pc) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MemOperand src_op =
      liftoff::GetMemOp(this, src_addr, offset_reg, offset_imm, scratch);
  VRegister dst_v = dst.fp().toV();
  *protected_load_pc = pc_offset();

  MachineType memtype = type.mem_type();
  if (transform == LoadTransformationKind::kExtend) {
    Lw(scratch, src_op);
    if (memtype == MachineType::Int8()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      VU.set(kScratchReg, E16, m1);
      vsext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Uint8()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      VU.set(kScratchReg, E16, m1);
      vzext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Int16()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      VU.set(kScratchReg, E32, m1);
      vsext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Uint16()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      VU.set(kScratchReg, E32, m1);
      vzext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Int32()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      vsext_vf2(dst_v, kSimd128ScratchReg);
    } else if (memtype == MachineType::Uint32()) {
      VU.set(kScratchReg, E64, m1);
      vmv_vx(kSimd128ScratchReg, scratch);
      vzext_vf2(dst_v, kSimd128ScratchReg);
    }
  } else if (transform == LoadTransformationKind::kZeroExtend) {
    vxor_vv(dst_v, dst_v, dst_v);
    if (memtype == MachineType::Int32()) {
      VU.set(kScratchReg, E32, m1);
      Lw(scratch, src_op);
      vmv_sx(dst_v, scratch);
    } else {
      // TODO(RISCV): need review
      DCHECK_EQ(MachineType::Int64(), memtype);
      VU.set(kScratchReg, E64, m1);
      Lw(scratch, src_op);
      vmv_sx(dst_v, scratch);
    }
  } else {
    DCHECK_EQ(LoadTransformationKind::kSplat, transform);
    if (memtype == MachineType::Int8()) {
      VU.set(kScratchReg, E8, m1);
      Lb(scratch, src_op);
      vmv_vx(dst_v, scratch);
    } else if (memtype == MachineType::Int16()) {
      VU.set(kScratchReg, E16, m1);
      Lh(scratch, src_op);
      vmv_vx(dst_v, scratch);
    } else if (memtype == MachineType::Int32()) {
      VU.set(kScratchReg, E32, m1);
      Lw(scratch, src_op);
      vmv_vx(dst_v, scratch);
    } else if (memtype == MachineType::Int64()) {
      // TODO(RISCV): need review
      VU.set(kScratchReg, E64, m1);
      Lw(scratch, src_op);
      vmv_vx(dst_v, scratch);
    }
  }
}

void LiftoffAssembler::LoadLane(LiftoffRegister dst, LiftoffRegister src,
                                Register addr, Register offset_reg,
                                uintptr_t offset_imm, LoadType type,
                                uint8_t laneidx, uint32_t* protected_load_pc) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MemOperand src_op =
      liftoff::GetMemOp(this, addr, offset_reg, offset_imm, scratch);
  MachineType mem_type = type.mem_type();
  *protected_load_pc = pc_offset();
  if (mem_type == MachineType::Int8()) {
    Lbu(scratch, src_op);
    VU.set(kScratchReg, E64, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    VU.set(kScratchReg, E8, m1);
    vmerge_vx(dst.fp().toV(), scratch, dst.fp().toV());
  } else if (mem_type == MachineType::Int16()) {
    Lhu(scratch, src_op);
    VU.set(kScratchReg, E16, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    vmerge_vx(dst.fp().toV(), scratch, dst.fp().toV());
  } else if (mem_type == MachineType::Int32()) {
    Lw(scratch, src_op);
    VU.set(kScratchReg, E32, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    vmerge_vx(dst.fp().toV(), scratch, dst.fp().toV());
  } else if (mem_type == MachineType::Int64()) {
    Lw(scratch, src_op);
    VU.set(kScratchReg, E32, m1);
    li(kScratchReg, 0x1 << laneidx);
    vmv_sx(v0, kScratchReg);
    vmerge_vx(dst.fp().toV(), scratch, dst.fp().toV());
  } else {
    UNREACHABLE();
  }
}

void LiftoffAssembler::StoreLane(Register dst, Register offset,
                                 uintptr_t offset_imm, LiftoffRegister src,
                                 StoreType type, uint8_t lane,
                                 uint32_t* protected_store_pc) {
  UseScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  MemOperand dst_op = liftoff::GetMemOp(this, dst, offset, offset_imm, scratch);
  if (protected_store_pc) *protected_store_pc = pc_offset();
  MachineRepresentation rep = type.mem_rep();
  if (rep == MachineRepresentation::kWord8) {
    VU.set(kScratchReg, E8, m1);
    vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), lane);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sb(kScratchReg, dst_op);
  } else if (rep == MachineRepresentation::kWord16) {
    VU.set(kScratchReg, E16, m1);
    vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), lane);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sh(kScratchReg, dst_op);
  } else if (rep == MachineRepresentation::kWord32) {
    VU.set(kScratchReg, E32, m1);
    vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), lane);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sw(kScratchReg, dst_op);
  } else {
    DCHECK_EQ(MachineRepresentation::kWord64, rep);
    VU.set(kScratchReg, E64, m1);
    vslidedown_vi(kSimd128ScratchReg, src.fp().toV(), lane);
    vmv_xs(kScratchReg, kSimd128ScratchReg);
    Sw(kScratchReg, dst_op);
  }
}

void LiftoffAssembler::emit_i8x16_shuffle(LiftoffRegister dst,
                                          LiftoffRegister lhs,
                                          LiftoffRegister rhs,
                                          const uint8_t shuffle[16],
                                          bool is_swizzle) {
  // VRegister dst_v = dst.fp().toV();
  // VRegister lhs_v = lhs.fp().toV();
  // VRegister rhs_v = rhs.fp().toV();

  // uint64_t imm1 = *(reinterpret_cast<const uint64_t*>(shuffle));
  // uint64_t imm2 = *((reinterpret_cast<const uint64_t*>(shuffle)) + 1);
  // VU.set(kScratchReg, VSew::E64, Vlmul::m1);
  // li(kScratchReg, imm2);
  // vmv_sx(kSimd128ScratchReg2, kScratchReg);
  // vslideup_vi(kSimd128ScratchReg, kSimd128ScratchReg2, 1);
  // li(kScratchReg, imm1);
  // vmv_sx(kSimd128ScratchReg, kScratchReg);

  // VU.set(kScratchReg, E8, m1);
  // VRegister temp =
  //     GetUnusedRegister(kFpReg, LiftoffRegList{lhs, rhs}).fp().toV();
  // if (dst_v == lhs_v) {
  //   vmv_vv(temp, lhs_v);
  //   lhs_v = temp;
  // } else if (dst_v == rhs_v) {
  //   vmv_vv(temp, rhs_v);
  //   rhs_v = temp;
  // }
  // vrgather_vv(dst_v, lhs_v, kSimd128ScratchReg);
  // vadd_vi(kSimd128ScratchReg, kSimd128ScratchReg,
  //         -16);  // The indices in range [16, 31] select the i - 16-th
  //         element
  //                // of rhs
  // vrgather_vv(kSimd128ScratchReg2, rhs_v, kSimd128ScratchReg);
  // vor_vv(dst_v, dst_v, kSimd128ScratchReg2);
  bailout(kSimd, "emit_i8x16_shuffle");
}

void LiftoffAssembler::emit_f64x2_splat(LiftoffRegister dst,
                                        LiftoffRegister src) {
  // VU.set(kScratchReg, E64, m1);
  // fmv_x_d(kScratchReg, src.fp());
  // vmv_vx(dst.fp().toV(), kScratchReg);
  bailout(kSimd, "emit_f64x2_splat");
}

void LiftoffAssembler::emit_f64x2_min(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  // VU.set(kScratchReg, E64, m1);
  // const int64_t kNaN = 0x7ff8000000000000L;
  // vmfeq_vv(v0, lhs.fp().toV(), lhs.fp().toV());
  // vmfeq_vv(kSimd128ScratchReg, rhs.fp().toV(), rhs.fp().toV());
  // vand_vv(v0, v0, kSimd128ScratchReg);
  // li(kScratchReg, kNaN);
  // vmv_vx(kSimd128ScratchReg, kScratchReg);
  // vfmin_vv(kSimd128ScratchReg, rhs.fp().toV(), lhs.fp().toV(), Mask);
  // vmv_vv(dst.fp().toV(), kSimd128ScratchReg);
  bailout(kSimd, "emit_f64x2_min");
}

void LiftoffAssembler::emit_f64x2_max(LiftoffRegister dst, LiftoffRegister lhs,
                                      LiftoffRegister rhs) {
  // VU.set(kScratchReg, E64, m1);
  // const int64_t kNaN = 0x7ff8000000000000L;
  // vmfeq_vv(v0, lhs.fp().toV(), lhs.fp().toV());
  // vmfeq_vv(kSimd128ScratchReg, rhs.fp().toV(), rhs.fp().toV());
  // vand_vv(v0, v0, kSimd128ScratchReg);
  // li(kScratchReg, kNaN);
  // vmv_vx(kSimd128ScratchReg, kScratchReg);
  // vfmax_vv(kSimd128ScratchReg, rhs.fp().toV(), lhs.fp().toV(), Mask);
  // vmv_vv(dst.fp().toV(), kSimd128ScratchReg);
  bailout(kSimd, "emit_f64x2_max");
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  // VU.set(kScratchReg, E64, m1);
  // li(kScratchReg, 0x0006000400020000);
  // vmv_sx(kSimd128ScratchReg, kScratchReg);
  // li(kScratchReg, 0x0007000500030001);
  // vmv_sx(kSimd128ScratchReg3, kScratchReg);
  // VU.set(kScratchReg, E16, m1);
  // vrgather_vv(kSimd128ScratchReg2, src.fp().toV(), kSimd128ScratchReg);
  // vrgather_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg3);
  // VU.set(kScratchReg, E16, mf2);
  // vwadd_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
  bailout(kSimd, "emit_i32x4_extadd_pairwise_i16x8_s");
}

void LiftoffAssembler::emit_i32x4_extadd_pairwise_i16x8_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  // VU.set(kScratchReg, E64, m1);
  // li(kScratchReg, 0x0006000400020000);
  // vmv_sx(kSimd128ScratchReg, kScratchReg);
  // li(kScratchReg, 0x0007000500030001);
  // vmv_sx(kSimd128ScratchReg3, kScratchReg);
  // VU.set(kScratchReg, E16, m1);
  // vrgather_vv(kSimd128ScratchReg2, src.fp().toV(), kSimd128ScratchReg);
  // vrgather_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg3);
  // VU.set(kScratchReg, E16, mf2);
  // vwaddu_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
  bailout(kSimd, "emit_i32x4_extadd_pairwise_i16x8_u");
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_s(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  // VU.set(kScratchReg, E64, m1);
  // li(kScratchReg, 0x0E0C0A0806040200);
  // vmv_sx(kSimd128ScratchReg, kScratchReg);
  // li(kScratchReg, 0x0F0D0B0907050301);
  // vmv_sx(kSimd128ScratchReg3, kScratchReg);
  // VU.set(kScratchReg, E8, m1);
  // vrgather_vv(kSimd128ScratchReg2, src.fp().toV(), kSimd128ScratchReg);
  // vrgather_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg3);
  // VU.set(kScratchReg, E8, mf2);
  // vwadd_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
  bailout(kSimd, "emit_i16x8_extadd_pairwise_i8x16_s");
}

void LiftoffAssembler::emit_i16x8_extadd_pairwise_i8x16_u(LiftoffRegister dst,
                                                          LiftoffRegister src) {
  // VU.set(kScratchReg, E64, m1);
  // li(kScratchReg, 0x0E0C0A0806040200);
  // vmv_sx(kSimd128ScratchReg, kScratchReg);
  // li(kScratchReg, 0x0F0D0B0907050301);
  // vmv_sx(kSimd128ScratchReg3, kScratchReg);
  // VU.set(kScratchReg, E8, m1);
  // vrgather_vv(kSimd128ScratchReg2, src.fp().toV(), kSimd128ScratchReg);
  // vrgather_vv(kSimd128ScratchReg, src.fp().toV(), kSimd128ScratchReg3);
  // VU.set(kScratchReg, E8, mf2);
  // vwaddu_vv(dst.fp().toV(), kSimd128ScratchReg, kSimd128ScratchReg2);
  bailout(kSimd, "emit_i16x8_extadd_pairwise_i8x16_u");
}

void LiftoffAssembler::emit_f64x2_replace_lane(LiftoffRegister dst,
                                               LiftoffRegister src1,
                                               LiftoffRegister src2,
                                               uint8_t imm_lane_idx) {
  // VU.set(kScratchReg, E64, m1);
  // li(kScratchReg, 0x1 << imm_lane_idx);
  // vmv_sx(v0, kScratchReg);
  // fmv_x_d(kScratchReg, src2.fp());
  // vmerge_vx(dst.fp().toV(), kScratchReg, src1.fp().toV());
  bailout(kSimd, "emit_f64x2_replace_lane");
}

void LiftoffAssembler::CallC(const ValueKindSig* sig,
                             const LiftoffRegister* args,
                             const LiftoffRegister* rets,
                             ValueKind out_argument_kind, int stack_bytes,
                             ExternalReference ext_ref) {
  AddWord(sp, sp, Operand(-stack_bytes));

  int arg_bytes = 0;
  for (ValueKind param_kind : sig->parameters()) {
    liftoff::Store(this, sp, arg_bytes, *args++, param_kind);
    arg_bytes += value_kind_size(param_kind);
  }
  DCHECK_LE(arg_bytes, stack_bytes);

  // Pass a pointer to the buffer with the arguments to the C function.
  // On RISC-V, the first argument is passed in {a0}.
  constexpr Register kFirstArgReg = a0;
  mv(kFirstArgReg, sp);

  // Now call the C function.
  constexpr int kNumCCallArgs = 1;
  PrepareCallCFunction(kNumCCallArgs, kScratchReg);
  CallCFunction(ext_ref, kNumCCallArgs);

  // Move return value to the right register.
  const LiftoffRegister* next_result_reg = rets;
  if (sig->return_count() > 0) {
    DCHECK_EQ(1, sig->return_count());
    constexpr Register kReturnReg = a0;
    if (kReturnReg != next_result_reg->gp()) {
      Move(*next_result_reg, LiftoffRegister(kReturnReg), sig->GetReturn(0));
    }
    ++next_result_reg;
  }

  // Load potential output value from the buffer on the stack.
  if (out_argument_kind != kVoid) {
    liftoff::Load(this, *next_result_reg, sp, 0, out_argument_kind);
  }

  AddWord(sp, sp, Operand(stack_bytes));
}

void LiftoffStackSlots::Construct(int param_slots) {
  ASM_CODE_COMMENT(asm_);
  DCHECK_LT(0, slots_.size());
  SortInPushOrder();
  int last_stack_slot = param_slots;
  for (auto& slot : slots_) {
    const int stack_slot = slot.dst_slot_;
    int stack_decrement = (last_stack_slot - stack_slot) * kSystemPointerSize;
    DCHECK_LT(0, stack_decrement);
    last_stack_slot = stack_slot;
    const LiftoffAssembler::VarState& src = slot.src_;
    switch (src.loc()) {
      case LiftoffAssembler::VarState::kStack: {
        if (src.kind() == kF64) {
          asm_->AllocateStackSpace(stack_decrement - kDoubleSize);
          DCHECK_EQ(kLowWord, slot.half_);
          asm_->Lw(kScratchReg,
                   liftoff::GetHalfStackSlot(slot.src_offset_, kHighWord));
          asm_->push(kScratchReg);
          asm_->Lw(kScratchReg,
                   liftoff::GetHalfStackSlot(slot.src_offset_, kLowWord));
          asm_->push(kScratchReg);
        } else if (src.kind() != kS128) {
          asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
          asm_->Lw(kScratchReg, liftoff::GetStackSlot(slot.src_offset_));
          asm_->push(kScratchReg);
        } else {
          asm_->AllocateStackSpace(stack_decrement - kSimd128Size);
          asm_->Lw(kScratchReg, liftoff::GetStackSlot(slot.src_offset_ - 8));
          asm_->push(kScratchReg);
          asm_->Lw(kScratchReg, liftoff::GetStackSlot(slot.src_offset_));
          asm_->push(kScratchReg);
        }
        break;
      }
      case LiftoffAssembler::VarState::kRegister: {
        int pushed_bytes = SlotSizeInBytes(slot);
        asm_->AllocateStackSpace(stack_decrement - pushed_bytes);
        if (src.kind() == kI64) {
          liftoff::push(
              asm_, slot.half_ == kLowWord ? src.reg().low() : src.reg().high(),
              kI32);
        } else {
          liftoff::push(asm_, src.reg(), src.kind());
        }
        break;
      }
      case LiftoffAssembler::VarState::kIntConst: {
        asm_->AllocateStackSpace(stack_decrement - kSystemPointerSize);
        asm_->li(kScratchReg, Operand(src.i32_const()));
        asm_->push(kScratchReg);
        break;
      }
    }
  }
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_RISCV_LIFTOFF_ASSEMBLER_RISCV32_H_
