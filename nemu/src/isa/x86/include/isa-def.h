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

#ifndef __ISA_X86_H__
#define __ISA_X86_H__

#include <common.h>


/* TODO: Re-organize the `CPU_state' structure to match the register
 * encoding scheme in i386 instruction format. For example, if we
 * access cpu.gpr[3]._16, we will get the `bx' register; if we access
 * cpu.gpr[1]._8[1], we will get the 'ch' register. Hint: Use `union'.
 * For more details about the register encoding scheme, see i386 manual.
 */

typedef struct {
  union
  {
    struct {
      union
      {
        uint32_t _32;
        uint16_t _16;
        uint8_t _8[2];
      };
    } gpr[8];
    /* Do NOT change the order of the GPRs' definitions. */

    struct {
      union { uint32_t eax; uint16_t ax; struct { uint8_t al; uint8_t ah; }; };
      union { uint32_t ecx; uint16_t cx; struct { uint8_t cl; uint8_t ch; }; };
      union { uint32_t edx; uint16_t dx; struct { uint8_t dl; uint8_t dh; }; };
      union { uint32_t ebx; uint16_t bx; struct { uint8_t bl; uint8_t bh; }; };
      union { uint32_t esp; uint16_t sp; };
      union { uint32_t ebp; uint16_t bp; };
      union { uint32_t esi; uint16_t si; };
      union { uint32_t edi; uint16_t di; };
    };
  };

  vaddr_t pc;

  union
  {
    uint32_t val;
    struct
    {
      uint32_t CF : 1;
      uint32_t    : 1;
      uint32_t PF : 1;
      uint32_t    : 3;
      uint32_t ZF : 1;
      uint32_t SF : 1;
      uint32_t    : 1;
      uint32_t IF : 1;
      uint32_t    : 1;
      uint32_t OF : 1;
      uint32_t    : 20;
    };
  }eflags;

  struct {
    uint32_t base;
    uint16_t limit;
  } idtr;

  uint16_t cs;

} x86_CPU_state;

// decode
typedef struct {
  uint8_t inst[16];
  uint8_t *p_inst;
} x86_ISADecodeInfo;

enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI };
enum { R_AX, R_CX, R_DX, R_BX, R_SP, R_BP, R_SI, R_DI };
enum { R_AL, R_CL, R_DL, R_BL, R_AH, R_CH, R_DH, R_BH };

#define isa_mmu_check(vaddr, len, type) (MMU_DIRECT)
#endif
