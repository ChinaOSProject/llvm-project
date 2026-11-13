# REQUIRES: loongarch
# RUN: split-file %s %t
# RUN: llvm-mc --filetype=obj --triple=loongarch64 %t/ops.s -o %t/ops.o
# RUN: ld.lld %t/ops.o -o %t/ops.out
# RUN: llvm-objdump -s --section=.data %t/ops.out | FileCheck %s --check-prefix=DATA
# RUN: llvm-mc --filetype=obj --triple=loongarch64 %t/underflow.s -o %t/underflow.o
# RUN: not --crash ld.lld %t/underflow.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=UNDERFLOW
# RUN: llvm-mc --filetype=obj --triple=loongarch64 %t/overflow.s -o %t/overflow.o
# RUN: not --crash ld.lld %t/overflow.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=OVERFLOW
# RUN: llvm-mc --filetype=obj --triple=loongarch64 %t/assert.s -o %t/assert.o
# RUN: not --crash ld.lld %t/assert.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=ASSERT
# RUN: llvm-mc --filetype=obj --triple=loongarch64 %t/shift.s -o %t/shift.o
# RUN: not --crash ld.lld %t/shift.o -o /dev/null 2>&1 | FileCheck %s --check-prefix=SHIFT

# DATA: 0c000000 01000000 0c000000 0c000000
# UNDERFLOW: LoongArch SOP operand underflow
# OVERFLOW: LoongArch SOP operand overflow
# ASSERT: LoongArch SOP assertion failed
# SHIFT: LoongArch SOP shift count is out of range

#--- ops.s
.text
.global _start
_start:
  nop
.data
dup:
  .long 0
  .reloc dup, R_LARCH_SOP_PUSH_ABSOLUTE, 6
  .reloc dup, R_LARCH_SOP_PUSH_DUP
  .reloc dup, R_LARCH_SOP_ADD
  .reloc dup, R_LARCH_SOP_POP_32_U
invert:
  .long 0
  .reloc invert, R_LARCH_SOP_PUSH_ABSOLUTE, 0
  .reloc invert, R_LARCH_SOP_NOT
  .reloc invert, R_LARCH_SOP_POP_32_U
select:
  .long 0
  .reloc select, R_LARCH_SOP_PUSH_ABSOLUTE, 0
  .reloc select, R_LARCH_SOP_PUSH_ABSOLUTE, 99
  .reloc select, R_LARCH_SOP_PUSH_ABSOLUTE, 12
  .reloc select, R_LARCH_SOP_IF_ELSE
  .reloc select, R_LARCH_SOP_POP_32_U
check:
  .long 0
  .reloc check, R_LARCH_SOP_PUSH_ABSOLUTE, 1
  .reloc check, R_LARCH_SOP_ASSERT
  .reloc check, R_LARCH_SOP_PUSH_ABSOLUTE, 12
  .reloc check, R_LARCH_SOP_POP_32_U

#--- underflow.s
.text
.global _start
_start:
  .long 0
  .reloc _start, R_LARCH_SOP_POP_32_U

#--- overflow.s
.text
.global _start
_start:
  .long 0
  .rept 17
  .reloc _start, R_LARCH_SOP_PUSH_ABSOLUTE, 1
  .endr

#--- assert.s
.text
.global _start
_start:
  .long 0
  .reloc _start, R_LARCH_SOP_PUSH_ABSOLUTE, 0
  .reloc _start, R_LARCH_SOP_ASSERT

#--- shift.s
.text
.global _start
_start:
  .long 0
  .reloc _start, R_LARCH_SOP_PUSH_ABSOLUTE, 1
  .reloc _start, R_LARCH_SOP_PUSH_ABSOLUTE, 64
  .reloc _start, R_LARCH_SOP_SL
