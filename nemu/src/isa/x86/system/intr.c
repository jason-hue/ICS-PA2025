/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <memory/vaddr.h>

#include "../../../../../abstract-machine/am/src/x86/x86.h"

#define push32(val) do { cpu.esp -= 4; vaddr_write(cpu.esp, 4, val); } while (0)

word_t isa_raise_intr(word_t NO, vaddr_t ret_addr) {
  /* TODO: Trigger an interrupt/exception with ``NO''.
   * That is, use ``NO'' to index the IDT.
   */
  push32(cpu.eflags.val);
  push32(cpu.cs);
  push32(ret_addr);

  vaddr_t gate_addr = cpu.idtr.base + NO * sizeof(GateDesc32);
  GateDesc32 gate;
  uint32_t *p = (uint32_t *)&gate;
  p[0] = vaddr_read(gate_addr, 4);
  p[1] = vaddr_read(gate_addr + 4, 4);

  if (!gate.p) {
    panic("IDT entry %d is not present!", NO);
  }

  uint32_t target_addr = (gate.off_31_16 << 16) | gate.off_15_0;
  cpu.cs = gate.cs;
  if (gate.type == STS_IG) cpu.eflags.IF = 0;

  etrace_write(NO, ret_addr, target_addr);

#ifdef DEBUG
  Log("Interrupt #%d: jumping to 0x%08x", NO, target_addr);
#endif
  return target_addr;
}

word_t isa_query_intr() {
  return INTR_EMPTY;
}
