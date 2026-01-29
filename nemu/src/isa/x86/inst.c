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

#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>

uint32_t pio_read(ioaddr_t addr, int len);
void pio_write(ioaddr_t addr, int len, uint32_t data);

typedef union {
  struct {
    uint8_t R_M		:3;
    uint8_t reg		:3;
    uint8_t mod		:2;
  };//1 个字节放置三个成员（总计 8 bits）,因为定义了位域，不再按8位对齐了
  struct {
    uint8_t dont_care	:3;
    uint8_t opcode		:3;
  };
  uint8_t val;
} ModR_M;

typedef union {
  struct {
    uint8_t base	:3;
    uint8_t index	:3;
    uint8_t ss		:2;
  };
  uint8_t val;
} SIB;

static word_t x86_inst_fetch(Decode *s, int len) {
#if defined(CONFIG_ITRACE) || defined(CONFIG_IQUEUE)
  uint8_t *p = &s->isa.inst[s->snpc - s->pc];
  word_t ret = inst_fetch(&s->snpc, len);//包装的paddr_read，并更新了pc的值。
  word_t ret_save = ret;
  int i;
  assert(s->snpc - s->pc < sizeof(s->isa.inst));
  for (i = 0; i < len; i ++) {
    p[i] = ret & 0xff;
    ret >>= 8;
  }
  return ret_save;
#else
  return inst_fetch(&s->snpc, len);
#endif
}

word_t reg_read(int idx, int width) {
  switch (width) {
    case 4: return reg_l(idx);
    case 1: return reg_b(idx);
    case 2: return reg_w(idx);
    default: assert(0);
  }
}

static void reg_write(int idx, int width, word_t data) {
  switch (width) {
    case 4: reg_l(idx) = data; return;
    case 1: reg_b(idx) = data; return;
    case 2: reg_w(idx) = data; return;
    default: assert(0);
  }
}

static void load_addr(Decode *s, ModR_M *m, word_t *rm_addr) {
  assert(m->mod != 3);

  sword_t disp = 0;
  int disp_size = 4;
  int base_reg = -1, index_reg = -1, scale = 0;

  //验证是否存在SIB字节，R_M要为4,（100），mod!=3
  if (m->R_M == R_ESP) {
    SIB sib;
    sib.val = x86_inst_fetch(s, 1);//再顺位取一个字节，SIB字节
    base_reg = sib.base;
    scale = sib.ss;

    if (sib.index != R_ESP) { index_reg = sib.index; }
  }
  else { base_reg = m->R_M; } /* no SIB */

  if (m->mod == 0) {
    // Mod = 00: [Base] 模式 (通常没有 Disp)
    // 特殊情况：如果 Base 是 EBP (101)，但 Mod=00，实际上是指 [Disp32] (直接寻址)
    if (base_reg == R_EBP) { base_reg = -1; }
    else { disp_size = 0; }
  }
  else if (m->mod == 1) { disp_size = 1; }

  if (disp_size != 0) { /* has disp */
    disp = x86_inst_fetch(s, disp_size);
    if (disp_size == 1) { disp = (int8_t)disp; }
  }

  word_t addr = disp;
  if (base_reg != -1)  addr += reg_l(base_reg);
  if (index_reg != -1) addr += reg_l(index_reg) << scale;
  *rm_addr = addr;
}

static void decode_rm(Decode *s, int *rm_reg, word_t *rm_addr, int *reg, int width) {
  ModR_M m;
  m.val = x86_inst_fetch(s, 1);//读的ModR/M 字节
  if (reg != NULL) *reg = m.reg;//因为Mod_M m是联合体，当给 m.val 赋值的那一刻，m.reg，m.R_M 就已经被填满了
  if (m.mod == 3) *rm_reg = m.R_M;
  else { load_addr(s, &m, rm_addr); *rm_reg = -1; }
}

#define Rr reg_read
#define Rw reg_write
#define Mr vaddr_read
#define Mw vaddr_write
#define RMr(reg, w)  (reg != -1 ? Rr(reg, w) : Mr(addr, w))
#define RMw(data) do { if (rd != -1) Rw(rd, w, data); else Mw(addr, w, data); } while (0)

#define destr(r)  do { *rd_ = (r); } while (0)
#define src1r(r)  do { *src1 = Rr(r, w); } while (0)
#define imm()     do { *imm = x86_inst_fetch(s, w); } while (0)
#define simm(w)   do { *imm = SEXT(x86_inst_fetch(s, w), w * 8); } while (0)
#define ddest (rd != -1 ? Rr(rd, w) : Mr(addr, w))//根据目的操作数的mod位是否为3决定读寄存器还是读内存
#define dsrc1 (rs != -1 ? Rr(rs, w) : Mr(addr, w))//根据源操作数的mod位是否为3决定读寄存器还是读内存

enum {
  TYPE_r, TYPE_I, TYPE_SI, TYPE_J, TYPE_E,
  TYPE_I2r,  // XX <- Ib / eXX <- Iv
  TYPE_I2a,  // AL <- Ib / eAX <- Iv
  TYPE_G2E,  // Eb <- Gb / Ev <- Gv
  TYPE_E2G,  // Gb <- Eb / Gv <- Ev
  TYPE_I2E,  // Eb <- Ib / Ev <- Iv
  TYPE_Ib2E, TYPE_cl2E, TYPE_1_E, TYPE_SI2E,
  TYPE_Eb2G, TYPE_Ew2G,
  TYPE_O2a, TYPE_a2O,
  TYPE_I_E2G,  // Gv <- EvIb / Gv <- EvIv // use for imul
  TYPE_SI_E2G,  // Gv <- EvIb / Gv <- EvIv // use for imul
  TYPE_Ib_G2E, // Ev <- GvIb // use for shld/shrd
  TYPE_cl_G2E, // Ev <- GvCL // use for shld/shrd
  TYPE_N, // none
};

#define INSTPAT_INST(s) opcode
#define INSTPAT_MATCH(s, name, type, width, ... /* execute body */ ) { \
  int rd = 0, rs = 0, gp_idx = 0; \
  word_t src1 = 0, addr = 0, imm = 0; \
  int w = width == 0 ? (is_operand_size_16 ? 2 : 4) : width; \
  decode_operand(s, opcode, &rd, &src1, &addr, &rs, &gp_idx, &imm, w, concat(TYPE_, type)); \
  s->dnpc = s->snpc; \
  __VA_ARGS__ ; \
}//action代码会在最后一行代码被执行

static void decode_operand(Decode *s, uint8_t opcode, int *rd_, word_t *src1,
    word_t *addr, int *rs, int *gp_idx, word_t *imm, int w, int type) {
  switch (type) {
    case TYPE_I2r:  destr(opcode & 0x7); imm(); break;
    case TYPE_I2a:  destr(R_EAX); imm(); break;
    case TYPE_G2E:  decode_rm(s, rd_, addr, rs, w); src1r(*rs); break;//reg字段是源寄存器编号,看数据流向可以得出，为0则reg为源。适用范围：ADD, OR, ADC, SBB, AND, SUB, XOR, CMP, MOV。
    case TYPE_E2G:  decode_rm(s, rs, addr, rd_, w); break;//reg字段是目的寄存器编号，看opcode低位第2位，为1则reg为目的，不适用范围：LEA, PUSH, POP, INC, DEC 等功能单一的指令。
    case TYPE_I2E:  decode_rm(s, rd_, addr, gp_idx, w); imm(); break;
    case TYPE_O2a:  destr(R_EAX); *addr = x86_inst_fetch(s, 4); break;
    case TYPE_a2O:  *rs = R_EAX;  *addr = x86_inst_fetch(s, 4); break;
    case TYPE_N:    break;
    case TYPE_I:    imm(); break;
    case TYPE_J:    imm(); break;
    case TYPE_SI2E: decode_rm(s, rd_, addr, gp_idx, w); simm(1);break;
    case TYPE_E:    decode_rm(s,   rd_, addr, gp_idx, w); *rs = *rd_; break;
    case TYPE_1_E:  decode_rm(s, rd_, addr, gp_idx, w); *imm = 1; break;
    case TYPE_cl2E: decode_rm(s, rd_, addr, gp_idx, w); *imm = reg_b(R_CL); break;
    case TYPE_Ib2E: decode_rm(s, rd_, addr, gp_idx, w); *imm = x86_inst_fetch(s, 1); break;
    case TYPE_Ib_G2E: decode_rm(s, rd_, addr, rs, w); src1r(*rs); *imm = x86_inst_fetch(s, 1); break;
    case TYPE_cl_G2E: decode_rm(s, rd_, addr, rs, w); src1r(*rs); *imm = reg_b(R_CL); break;
    case TYPE_SI_E2G: decode_rm(s, rs, addr, rd_, w); simm(1); break;
    case TYPE_I_E2G: decode_rm(s, rs, addr, rd_, w); imm(); break;
    default: panic("Unsupported type = %d", type);
  }
}
static inline void update_eflags(int gp_idx, word_t dest, word_t src, word_t res, int width)
{
  int shift = width * 8 - 1;
  if (width == 1) {
    res &= 0xff;
    dest &= 0xff;
    src &= 0xff;
  }
  else if (width == 2) {
    res &= 0xffff;
    dest &= 0xffff;
    src &= 0xffff;
  }
  
  cpu.eflags.ZF = (res == 0);
  cpu.eflags.SF = (res >> shift) & 1;

  // Parity Flag (PF): Set if the least-significant byte of the result contains an even number of 1 bits.
  uint8_t low_byte = res & 0xff;
  // Brian Kernighan's algorithm to count set bits
  int count = 0;
  for (int i = 0; i < 8; i++) {
      if ((low_byte >> i) & 1) count++;
  }
  cpu.eflags.PF = (count % 2 == 0);

  // CF & OF: 只有加减法需要
  if (gp_idx == 0) { // ADD
    cpu.eflags.CF = (res < dest); // 无符号溢出
    // 有符号溢出: 两个加数符号相同，但与结果符号不同
    cpu.eflags.OF = (((dest >> shift) & 1) == ((src >> shift) & 1)) && (((dest >> shift) & 1) != ((res >> shift) & 1));
  }
  else if (gp_idx == 5 || gp_idx == 7) { // SUB (cmp 也用这个)
    cpu.eflags.CF = (dest < src); // 无符号借位
    // 有符号溢出: 减数与被减数符号不同，且结果符号与被减数不同
    // (例如: 正 - 负 = 负)
    cpu.eflags.OF = (((dest >> shift) & 1) != ((src >> shift) & 1)) && (((dest >> shift) & 1) != ((res >> shift) & 1));
  }else if (gp_idx == 4)
  {
    cpu.eflags.OF = 0;
    cpu.eflags.CF = 0;
  }

  if (gp_idx == 1 || gp_idx == 4 || gp_idx == 6) {
    cpu.eflags.CF = 0;
    cpu.eflags.OF = 0;
  }
}

#define push(val) do { \
cpu.esp -= w; \
Mw(cpu.esp, w, val); \
} while (0)

#define pop(dest) do { \
dest = Mr(cpu.esp,w);\
cpu.esp += w;\
} while (0)

#define gp1() do { \
  word_t dest = (rd != -1 ? Rr(rd, w) : Mr(addr, w)); \
  word_t src = imm; \
  word_t res = 0; \
  switch (gp_idx) { \
    case 0: res = dest + src; RMw(res); update_eflags(0, dest, src, res, w); break; \
    case 1: res = dest | src; RMw(res); update_eflags(1, dest, src, res, w); break; \
    case 2: adc(dest, src); break; \
    case 3: sbb(dest, src); break; \
    case 4: res = dest & src; RMw(res); update_eflags(4, dest, src, res, w); break; \
    case 5: res = dest - src; RMw(res); update_eflags(5, dest, src, res, w); break; \
    case 6: res = dest ^ src; RMw(res); update_eflags(6, dest, src, res, w); break; \
    case 7: res = dest - src; update_eflags(7, dest, src, res, w); break; \
    default: INV(s->pc); \
  }; \
} while (0)
/*
* 在 x86 指令集中，有很多运算指令（如 ADD, SUB, AND, OR, XOR, CMP）的操作码是共享的。
例如：
*   0x80: Op r/m8, imm8
*   0x81: Op r/m32, imm32
*   0x83: Op r/m32, imm8 (符号扩展)
这里的 Op 到底是哪个运算，不是由 Opcode 决定的，而是由 ModR/M 字节中间的 3 位（也就是我们代码里的 gp_idx）决定的：
 */

#define gp4() do{ \
  switch (gp_idx){ \
    case 0: { \
      word_t dest = ddest; \
      word_t res = dest + 1; \
      if (w == 1) res &= 0xff; \
      else if (w == 2) res &= 0xffff; \
      RMw(res); \
      bool old_cf = cpu.eflags.CF; \
      update_eflags(0, dest, 1, res, w); \
      cpu.eflags.CF = old_cf; \
      break; \
    } \
    case 1: { \
      word_t dest = ddest; \
      word_t res = dest - 1; \
      if (w == 1) res &= 0xff; \
      else if (w == 2) res &= 0xffff; \
      RMw(res); \
      bool old_cf = cpu.eflags.CF; \
      update_eflags(5, dest, 1, res, w); \
      cpu.eflags.CF = old_cf; \
      break; \
    } \
    default: INV(s->pc); \
  };\
}while(0)

#define gp5() do{ \
  switch (gp_idx){ \
    case 0: { \
      word_t dest = ddest; \
      word_t res = dest + 1; \
      if (w == 1) res &= 0xff; \
      else if (w == 2) res &= 0xffff; \
      RMw(res); \
      bool old_cf = cpu.eflags.CF; \
      update_eflags(0, dest, 1, res, w); \
      cpu.eflags.CF = old_cf; \
      break; \
    } \
    case 1: { \
      word_t dest = ddest; \
      word_t res = dest - 1; \
      if (w == 1) res &= 0xff; \
      else if (w == 2) res &= 0xffff; \
      RMw(res); \
      bool old_cf = cpu.eflags.CF; \
      update_eflags(5, dest, 1, res, w); \
      cpu.eflags.CF = old_cf; \
      break; \
    } \
    case 2: \
      push(s->snpc); \
      s->dnpc = dsrc1; \
      IFDEF(CONFIG_FTRACE, ftrace_write(s->pc, s->dnpc, true)); \
      break; \
    case 4: \
      s->dnpc = dsrc1; \
      break; \
    case 6: push(dsrc1);break; \
    default: INV(s->pc); \
  };\
}while(0)
/*
* gp5 (Group 5) 指令组，Opcode 0xFF。
根据 ModR/M 中间 3 位 (gp_idx) 区分功能：
*   /0: inc r/m (自增 1)
*   /1: dec r/m (自减 1)
*   /2: call r/m (绝对间接调用, near)
*   /3: call m16:32 (远调用, far - PA不需要)
*   /4: jmp r/m (绝对间接跳转, near)
*   /5: jmp m16:32 (远跳转, far - PA不需要)
*   /6: push r/m (压栈)
*   /7: (Reserved)
 */

#define gp2() do { \
  uint32_t count = imm & 0x1f; \
  word_t dest = ddest; \
  word_t res = dest; \
  switch (gp_idx) { \
    case 0: /* rol */ \
      if (count > 0) { \
        res = (dest << count) | (dest >> (w*8 - count)); \
        if (w == 1) res &= 0xff; else if (w == 2) res &= 0xffff; \
        RMw(res); \
        cpu.eflags.CF = res & 1; \
        if (count == 1) cpu.eflags.OF = (res >> (w*8-1)) ^ cpu.eflags.CF; \
      } \
      break; \
    case 1: /* ror */ \
      if (count > 0) { \
        res = (dest >> count) | (dest << (w*8 - count)); \
        if (w == 1) res &= 0xff; else if (w == 2) res &= 0xffff; \
        RMw(res); \
        cpu.eflags.CF = (res >> (w*8-1)) & 1; \
        if (count == 1) cpu.eflags.OF = ((res >> (w*8-1)) ^ (res >> (w*8-2))) & 1; \
      } \
      break; \
    case 2: /* rcl */ \
      if (count > 0) { \
        word_t next_cf = cpu.eflags.CF; \
        for (int i = 0; i < count; i++) { \
          word_t t = (dest >> (w * 8 - 1)) & 1; \
          dest = (dest << 1) | next_cf; \
          next_cf = t; \
        } \
        if (w == 1) dest &= 0xff; else if (w == 2) dest &= 0xffff; \
        res = dest; \
        RMw(res); \
        cpu.eflags.CF = next_cf; \
        if (count == 1) cpu.eflags.OF = (res >> (w * 8 - 1)) ^ cpu.eflags.CF; \
      } \
      break; \
    case 3: /* rcr */ \
      if (count > 0) { \
        word_t next_cf = cpu.eflags.CF; \
        for (int i = 0; i < count; i++) { \
          word_t t = dest & 1; \
          dest = (dest >> 1) | (next_cf << (w * 8 - 1)); \
          next_cf = t; \
        } \
        if (w == 1) dest &= 0xff; else if (w == 2) dest &= 0xffff; \
        res = dest; \
        RMw(res); \
        cpu.eflags.CF = next_cf; \
        if (count == 1) cpu.eflags.OF = ((res >> (w * 8 - 1)) ^ (res >> (w * 8 - 2))) & 1; \
      } \
      break; \
    case 4: \
      res = dest << count; \
      RMw(res); \
      cpu.eflags.ZF = (res == 0); \
      cpu.eflags.SF = (res >> (w*8-1)) & 1; \
      if (count > 0) cpu.eflags.CF = (dest >> (w*8 - count)) & 1; \
      if (count == 1) { \
        cpu.eflags.OF = ( (dest >> (w*8-1)) ^ (res >> (w*8-1)) ) & 1; \
      } \
      break; \
    case 5: \
      res = dest >> count; \
      RMw(res); \
      cpu.eflags.ZF = (res == 0); \
      cpu.eflags.SF = (res >> (w*8-1)) & 1; \
      if (count > 0) cpu.eflags.CF = (dest >> (count - 1)) & 1; \
      if (count == 1) { \
        cpu.eflags.OF = (dest >> (w*8-1)) & 1; \
      } \
      break; \
    case 7: \
      if (w == 1) res = (int8_t)dest >> count; \
      else if (w == 2) res = (int16_t)dest >> count; \
      else res = (int32_t)dest >> count; \
      RMw(res); \
      cpu.eflags.ZF = (res == 0); \
      cpu.eflags.SF = (res >> (w*8-1)) & 1; \
      if (count > 0) cpu.eflags.CF = (dest >> (count - 1)) & 1; \
      if (count == 1) { \
        cpu.eflags.OF = 0; \
      } \
      break; \
    default: panic("gp2: %d", gp_idx); \
  } \
} while (0)

#define gp3() do { \
  switch (gp_idx) { \
    case 0: { word_t imm_val = x86_inst_fetch(s, w); s->dnpc = s->snpc; test(ddest, imm_val); break; } \
    case 2: RMw(~ddest); break; /* not */ \
    case 3: { \
      word_t dest = ddest; \
      word_t res = -dest; \
      RMw(res); \
      update_eflags(5, 0, dest, res, w); /* neg is like 0 - dest */ \
      if (dest != 0) cpu.eflags.CF = 1; else cpu.eflags.CF = 0; \
      break; \
    } \
    case 4: /* mul */ \
      if (w == 1) { \
        uint16_t res = (uint16_t)reg_b(R_AL) * (uint16_t)ddest; \
        reg_w(R_AX) = res; \
        cpu.eflags.CF = cpu.eflags.OF = (reg_b(R_AH) != 0); \
      } else { \
        word_t src = ddest; \
        if (w == 2) src &= 0xffff; \
        uint64_t res = (uint64_t)reg_l(R_EAX) * (uint64_t)src; \
        if (w == 2) { \
          reg_w(R_AX) = res & 0xffff; \
          reg_w(R_DX) = res >> 16; \
          cpu.eflags.CF = cpu.eflags.OF = (reg_w(R_DX) != 0); \
        } else { \
          reg_l(R_EAX) = res & 0xffffffff; \
          reg_l(R_EDX) = res >> 32; \
          cpu.eflags.CF = cpu.eflags.OF = (reg_l(R_EDX) != 0); \
        } \
      } \
      break; \
    case 5: /* imul */ \
      if (w == 1) { \
        int16_t res = (int16_t)(int8_t)reg_b(R_AL) * (int16_t)(int8_t)ddest; \
        reg_w(R_AX) = res; \
        cpu.eflags.CF = cpu.eflags.OF = (res != (int8_t)res); \
      } else { \
        int64_t src = (w == 2 ? (int16_t)ddest : (int32_t)ddest); \
        int64_t val = (w == 2 ? (int16_t)reg_l(R_EAX) : (int32_t)reg_l(R_EAX)); \
        int64_t res = val * src; \
        if (w == 2) { \
          reg_w(R_AX) = res & 0xffff; \
          reg_w(R_DX) = res >> 16; \
          cpu.eflags.CF = cpu.eflags.OF = (res != (int16_t)res); \
        } else { \
          reg_l(R_EAX) = res & 0xffffffff; \
          reg_l(R_EDX) = res >> 32; \
          cpu.eflags.CF = cpu.eflags.OF = (res != (int32_t)res); \
        } \
      } \
      break; \
    case 6: /* div */ \
      if (w == 1) { \
        uint16_t val = reg_w(R_AX); \
        uint8_t src = ddest; \
        if (src == 0) panic("Divide by zero"); \
        reg_b(R_AL) = val / src; \
        reg_b(R_AH) = val % src; \
      } else { \
        uint64_t val = (w == 2 ? (((uint32_t)reg_w(R_DX) << 16) | (uint32_t)reg_w(R_AX)) : (((uint64_t)reg_l(R_EDX) << 32) | (uint64_t)reg_l(R_EAX))); \
        word_t src = ddest; \
        if (w == 2) src &= 0xffff; \
        if (src == 0) panic("Divide by zero"); \
        uint64_t quot = val / src; \
        uint64_t rem = val % src; \
        if (w == 2) { reg_w(R_AX) = (uint16_t)quot; reg_w(R_DX) = (uint16_t)rem; } \
        else { reg_l(R_EAX) = (uint32_t)quot; reg_l(R_EDX) = (uint32_t)rem; } \
      } \
      break; \
    case 7: /* idiv */ \
      if (w == 1) { \
        int16_t val = (int16_t)reg_w(R_AX); \
        int8_t src = (int8_t)ddest; \
        if (src == 0) panic("Divide by zero"); \
        reg_b(R_AL) = val / src; \
        reg_b(R_AH) = val % src; \
      } else { \
        int64_t val = (w == 2 ? ((int32_t)(int16_t)reg_w(R_DX) << 16) | (uint16_t)reg_w(R_AX) : ((int64_t)(int32_t)reg_l(R_EDX) << 32) | reg_l(R_EAX)); \
        int64_t src = (w == 2 ? (int16_t)ddest : (int32_t)ddest); \
        if (src == 0) panic("Divide by zero"); \
        int64_t quot = val / src; \
        int64_t rem = val % src; \
        if (w == 2) { reg_w(R_AX) = quot; reg_w(R_DX) = rem; } \
        else { reg_l(R_EAX) = quot; reg_l(R_EDX) = rem; } \
      } \
      break; \
    default: INV(s->pc); \
  }; \
} while (0)

#define or(dest, src) do { \
  word_t res = dest | src; \
  RMw(res); \
  update_eflags(1, dest, src, res, w); \
} while(0)

#define and(dest, src) do { \
  word_t res = dest & src; \
  RMw(res); \
  update_eflags(4, dest, src, res, w); \
} while(0)

#define xor(dest, src) do { \
  word_t res = dest ^ src; \
  RMw(res); \
  update_eflags(6, dest, src, res, w); \
} while (0)

#define adc(dest, src) do { \
  word_t d = (dest); \
  word_t carry = cpu.eflags.CF; \
  uint64_t full = (uint64_t)d + (uint64_t)src + carry; \
  word_t res = (word_t)full; \
  RMw(res); \
  cpu.eflags.CF = (full >> (w * 8)) & 1; \
  word_t src_with_carry = src + carry; \
  word_t sign_mask = (word_t)1 << (w * 8 - 1); \
  cpu.eflags.OF = (~(d ^ src_with_carry) & (d ^ res) & sign_mask) != 0; \
  word_t res_masked = res; \
  if (w == 1) res_masked &= 0xff; \
  else if (w == 2) res_masked &= 0xffff; \
  cpu.eflags.ZF = (res_masked == 0); \
  cpu.eflags.SF = (res >> (w * 8 - 1)) & 1; \
} while (0)

#define sbb(dest, src) do { \
  word_t d = (dest); \
  word_t borrow = cpu.eflags.CF; \
  uint64_t sub = (uint64_t)src + borrow; \
  word_t res = d - sub; \
  RMw(res); \
  cpu.eflags.CF = d < sub; \
  word_t src_with_borrow = src + borrow; \
  word_t sign_mask = (word_t)1 << (w * 8 - 1); \
  cpu.eflags.OF = ((d ^ src_with_borrow) & (d ^ res) & sign_mask) != 0; \
  word_t res_masked = res; \
  if (w == 1) res_masked &= 0xff; \
  else if (w == 2) res_masked &= 0xffff; \
  cpu.eflags.ZF = (res_masked == 0); \
  cpu.eflags.SF = (res >> (w * 8 - 1)) & 1; \
} while (0)

#define test(dest, src) do { \
word_t res = dest & src; \
update_eflags(4, dest, src, res, w); \
} while (0)

#define cmp(dest, src) do { \
word_t res = dest - src; \
update_eflags(5, dest, src, res, w); \
} while (0)

void _2byte_esc(Decode *s, bool is_operand_size_16) {
  uint8_t opcode = x86_inst_fetch(s, 1);
  INSTPAT_START();

  INSTPAT("1001 ????", setcc, E, 1, {
    int cond = opcode & 0xf;
    bool set = false;
    switch (cond) {
      case 0: set = cpu.eflags.OF; break;
      case 1: set = !cpu.eflags.OF; break;
      case 2: set = cpu.eflags.CF; break;
      case 3: set = !cpu.eflags.CF; break;
      case 4: set = cpu.eflags.ZF; break;
      case 5: set = !cpu.eflags.ZF; break;
      case 6: set = cpu.eflags.CF || cpu.eflags.ZF; break;
      case 7: set = !cpu.eflags.CF && !cpu.eflags.ZF; break;
      case 8: set = cpu.eflags.SF; break;
      case 9: set = !cpu.eflags.SF; break;
      case 10: set = cpu.eflags.PF; break;
      case 11: set = !cpu.eflags.PF; break;
      case 12: set = cpu.eflags.SF != cpu.eflags.OF; break;
      case 13: set = cpu.eflags.SF == cpu.eflags.OF; break;
      case 14: set = cpu.eflags.ZF || (cpu.eflags.SF != cpu.eflags.OF); break;
      case 15: set = !cpu.eflags.ZF && (cpu.eflags.SF == cpu.eflags.OF); break;
    }
    RMw(set ? 1 : 0);
  });

  INSTPAT("1011 0110", movzx, E2G, 0, {
    word_t src = (rs != -1 ? Rr(rs, 1) : Mr(addr, 1)); // 强制宽 1
    Rw(rd, w, src); // 零扩展是自动的，因为 src 是 word_t(uint32)，读出来高位就是0
});
  INSTPAT("1011 0111", movzx, E2G, 0, {
    word_t src = (rs != -1 ? Rr(rs, 2) : Mr(addr, 2));
    Rw(rd, w, src);
  });
  INSTPAT("1011 1110", movsx, E2G, 0, {
    word_t src = (rs != -1 ? Rr(rs, 1) : Mr(addr, 1));
    Rw(rd, w, SEXT(src, 8));
  });
  INSTPAT("1011 1111", movsx, E2G, 0, {
    word_t src = (rs != -1 ? Rr(rs, 2) : Mr(addr, 2));
    Rw(rd, w, SEXT(src, 16));
  });
  INSTPAT("1010 0101", shld, cl_G2E, 0, {
    word_t count = imm & 0x1f;
    if (count > 0) {
      word_t dest = ddest;
      word_t src = dsrc1;
      word_t res = (dest << count) | (src >> (w * 8 - count));
      RMw(res);
      cpu.eflags.CF = (dest >> (w * 8 - count)) & 1;
      cpu.eflags.ZF = (res == 0);
      cpu.eflags.SF = (res >> (w * 8 - 1)) & 1;
    }
  });
  INSTPAT("1010 0100", shld, Ib_G2E, 0, {
    word_t count = imm & 0x1f;
    if (count > 0) {
      word_t dest = ddest;
      word_t src = dsrc1;
      word_t res = (dest << count) | (src >> (w * 8 - count));
      RMw(res);
      cpu.eflags.CF = (dest >> (w * 8 - count)) & 1;
      cpu.eflags.ZF = (res == 0);
      cpu.eflags.SF = (res >> (w * 8 - 1)) & 1;
    }
  });
  INSTPAT("1010 1101", shrd, cl_G2E, 0, {
    word_t count = imm & 0x1f;
    if (count > 0) {
      word_t dest = ddest;
      word_t src = dsrc1;
      word_t res = (dest >> count) | (src << (w * 8 - count));
      RMw(res);
      cpu.eflags.CF = (dest >> (count - 1)) & 1;
      cpu.eflags.ZF = (res == 0);
      cpu.eflags.SF = (res >> (w * 8 - 1)) & 1;
    }
  });
  INSTPAT("1010 1100", shrd, Ib_G2E, 0, {
    word_t count = imm & 0x1f;
    if (count > 0) {
      word_t dest = ddest;
      word_t src = dsrc1;
      word_t res = (dest >> count) | (src << (w * 8 - count));
      RMw(res);
      cpu.eflags.CF = (dest >> (count - 1)) & 1;
      cpu.eflags.ZF = (res == 0);
      cpu.eflags.SF = (res >> (w * 8 - 1)) & 1;
    }
  });
  INSTPAT("1010 0011", bt, G2E, 0, {
    word_t dest = ddest;
    word_t src = dsrc1 & (w * 8 - 1);
    cpu.eflags.CF = (dest >> src) & 1;
  });
  INSTPAT("1011 1010", gp7, Ib2E, 0, {
    word_t dest = ddest;
    word_t src = imm & (w * 8 - 1);
    switch (gp_idx) {
      case 4: cpu.eflags.CF = (dest >> src) & 1; break;
      case 5: cpu.eflags.CF = (dest >> src) & 1; RMw(dest | (1 << src)); break;
      case 6: cpu.eflags.CF = (dest >> src) & 1; RMw(dest & ~(1 << src)); break;
      case 7: cpu.eflags.CF = (dest >> src) & 1; RMw(dest ^ (1 << src)); break;
      default: INV(s->pc);
    }
  });
  INSTPAT("1010 1111", imul2, E2G, 0, {
    word_t src = dsrc1;
    word_t dest = ddest;
    int64_t src_s, dest_s;
    if (w == 1) { src_s = (int8_t)src; dest_s = (int8_t)dest; }
    else if (w == 2) { src_s = (int16_t)src; dest_s = (int16_t)dest; }
    else { src_s = (int32_t)src; dest_s = (int32_t)dest; }

    int64_t full = src_s * dest_s;
    word_t res = (word_t)full;
    Rw(rd, w, res);
    
    if (w == 1) cpu.eflags.CF = cpu.eflags.OF = (full != (int64_t)(int8_t)res);
    else if (w == 2) cpu.eflags.CF = cpu.eflags.OF = (full != (int64_t)(int16_t)res);
    else cpu.eflags.CF = cpu.eflags.OF = (full != (int64_t)(int32_t)res);
  });
  INSTPAT("1000 ????", jcc, J, 0, {

    int cond = opcode & 0xf;
    bool jump = false;
    switch(cond) {
      case 0: jump = cpu.eflags.OF; break; // jo
      case 1: jump = !cpu.eflags.OF; break; // jno
      case 2: jump = cpu.eflags.CF; break; // jb
      case 3: jump = !cpu.eflags.CF; break; // jae
      case 4: jump = cpu.eflags.ZF; break; // je
      case 5: jump = !cpu.eflags.ZF; break; // jne
      case 6: jump = cpu.eflags.CF || cpu.eflags.ZF; break; // jbe
      case 7: jump = !cpu.eflags.CF && !cpu.eflags.ZF; break; // ja
      case 8: jump = cpu.eflags.SF; break; // js
      case 9: jump = !cpu.eflags.SF; break; // jns
      case 10: jump = cpu.eflags.PF; break; // jp
      case 11: jump = !cpu.eflags.PF; break; // jnp
      case 12: jump = cpu.eflags.SF != cpu.eflags.OF; break; // jl
      case 13: jump = cpu.eflags.SF == cpu.eflags.OF; break; // jge
      case 14: jump = cpu.eflags.ZF || (cpu.eflags.SF != cpu.eflags.OF); break; // jle
      case 15: jump = !cpu.eflags.ZF && (cpu.eflags.SF == cpu.eflags.OF); break; // jg
    }
    if (jump) s->dnpc += imm;
  });
  INSTPAT("0000 0001", system_ins, E, 0, {
    switch (gp_idx)
    {
    case 3:
      cpu.idtr.limit = Mr(addr, 2);
      cpu.idtr.base = Mr(addr + 2, 4);
      break;
    }
  });

  INSTPAT("???? ????", inv,    N,    0, INV(s->pc));

  INSTPAT_END();
}

int isa_exec_once(Decode *s) {
  bool is_operand_size_16 = false;
  bool has_rep = false;
  uint8_t opcode = 0;

again:
  opcode = x86_inst_fetch(s, 1);

  INSTPAT_START();

  //INSTPAT(模式, 名称, 译码类型, 宽度标志, 执行逻辑);
  /* rd, rs, gp_idx, src1, addr, imm, w这些变量在INSTPAT_MATCH宏中已经被填充了，可以直接用 */

  INSTPAT("0000 1111", 2byte_esc, N,    0, _2byte_esc(s, is_operand_size_16));

  INSTPAT("0110 0110", data_size, N,    0, is_operand_size_16 = true; goto again;);

  INSTPAT("1111 0011", rep,       N,    0, has_rep = true; goto again;);
  INSTPAT("1001 0000", nop,       N,    0, );
  INSTPAT("0011 1010", cmp,       E2G,  1, cmp(ddest, dsrc1));
  INSTPAT("1000 0110", xchg,      G2E,  1, { word_t temp = ddest; RMw(src1); Rw(rd, 1, temp); });
  INSTPAT("1000 0111", xchg,      G2E,  0, { word_t temp = ddest; RMw(src1); Rw(rd, w, temp); });
  INSTPAT("1001 0???", xchg,      N,    0, { int reg = opcode & 0x7; word_t temp = reg_read(reg, w); reg_write(reg, w, reg_read(R_EAX, w)); reg_write(R_EAX, w, temp); });
  INSTPAT("1001 1000", cbw,       N,    0, { if (is_operand_size_16) reg_w(R_AX) = (int16_t)(int8_t)reg_b(R_AL); else reg_l(R_EAX) = (int32_t)(int16_t)reg_w(R_AX); });

  INSTPAT("1001 1001", cltd,      N,    0, { if (w==4) cpu.edx = (int32_t)cpu.eax >> 31; else cpu.dx = (int16_t)cpu.ax >> 15; });
  INSTPAT("1111 0110", gp3,       E,    1, gp3());
  INSTPAT("1111 0111", gp3,       E,    0, gp3());
  INSTPAT("1100 0000", gp2,       Ib2E,  1, gp2());
  INSTPAT("1100 0001", gp2,       Ib2E,  0, gp2());
  INSTPAT("1101 0000", gp2,       1_E,  1, gp2());
  INSTPAT("1101 0001", gp2,       1_E,  0, gp2());
  INSTPAT("1101 0010", gp2,       cl2E, 1, gp2());
  INSTPAT("1101 0011", gp2,       cl2E,  0, gp2());
  INSTPAT("1000 0000", gp1,       I2E,  1, gp1());
  INSTPAT("1000 0001", gp1,       I2E,  0, gp1());
  INSTPAT("1000 1000", mov,       G2E,  1, RMw(src1));
  INSTPAT("1000 1001", mov,       G2E,  0, RMw(src1));
  INSTPAT("1000 1010", mov,       E2G,  1, Rw(rd, w, RMr(rs, w)));
  INSTPAT("1000 1011", mov,       E2G,  0, Rw(rd, w, RMr(rs, w)));

  INSTPAT("1010 0000", mov,       O2a,  1, Rw(R_EAX, 1, Mr(addr, 1)));
  INSTPAT("1010 0001", mov,       O2a,  0, Rw(R_EAX, w, Mr(addr, w)));
  INSTPAT("1010 0010", mov,       a2O,  1, Mw(addr, 1, Rr(R_EAX, 1)));
  INSTPAT("1010 0011", mov,       a2O,  0, Mw(addr, w, Rr(R_EAX, w)));

  INSTPAT("1010 1000", test,      I2a,  1, { word_t temp_imm = imm & 0xff; test(Rr(R_EAX, 1), temp_imm); });
  INSTPAT("1010 1001", test,      I2a,  0, test(Rr(R_EAX, w), imm));
  INSTPAT("0000 1100", or,        I2a,  1, or(Rr(R_EAX, 1), imm));
  INSTPAT("0000 1101", or,        I2a,  0, or(Rr(R_EAX, w), imm));
  INSTPAT("0011 0100", xor,       I2a,  1, xor(Rr(R_EAX, 1), imm));
  INSTPAT("0011 0101", xor,       I2a,  0, xor(Rr(R_EAX, w), imm));
  INSTPAT("0110 1011", imul3,     SI_E2G, 0, {
    word_t src = (rs != -1 ? Rr(rs, w) : Mr(addr, w));
    int64_t full = (int64_t)(int32_t)src * (int64_t)(int32_t)imm;
    word_t res = (word_t)full;
    Rw(rd, w, res);
    cpu.eflags.CF = cpu.eflags.OF = (full != (int64_t)(int32_t)res);
  });
  INSTPAT("0110 1001", imul3,     I_E2G, 0, {
    word_t src = (rs != -1 ? Rr(rs, w) : Mr(addr, w));
    int64_t full = (int64_t)(int32_t)src * (int64_t)(int32_t)imm;
    word_t res = (word_t)full;
    Rw(rd, w, res);
    cpu.eflags.CF = cpu.eflags.OF = (full != (int64_t)(int32_t)res);
  });

  INSTPAT("1011 0???", mov,       I2r,  1, Rw(rd, 1, imm));
  INSTPAT("1011 1???", mov,       I2r,  0, Rw(rd, w, imm));
  INSTPAT("0101 0???", push,      N,    0, push(Rr(opcode & 0x7,w)));
  INSTPAT("0101 1???", pop,       N,    0, { word_t val; pop(val); Rw(opcode & 0x7, w, val); });
  INSTPAT("0110 1000", push,      I,    0, push(imm));
  INSTPAT("0110 1010", push,      N,    0, { word_t val = (int8_t)x86_inst_fetch(s, 1); push(val); s->dnpc = s->snpc; });
  INSTPAT("1100 0110", mov,       I2E,  1, RMw(imm));
  INSTPAT("1100 0111", mov,       I2E,  0, RMw(imm));
  INSTPAT("0110 0001", popa, N, 0, {
    pop(cpu.edi);
    pop(cpu.esi);
    pop(cpu.ebp);
    word_t dummy; pop(dummy); (void)dummy;
    pop(cpu.ebx);
    pop(cpu.edx);
    pop(cpu.ecx);
    pop(cpu.eax);
  });
  INSTPAT("1100 1111", iret, N, 0, {
    pop(s->dnpc);
    pop(cpu.cs);
    pop(cpu.eflags.val);
    etrace_write(INTR_EMPTY, s->pc, s->dnpc);
  });
  INSTPAT("1100 1100", nemu_trap, N,    0, NEMUTRAP(s->pc, cpu.eax));
  INSTPAT("1110 1000", call,      J,    0, push(s->snpc);s->dnpc = s->snpc + imm; IFDEF(CONFIG_FTRACE, ftrace_write(s->pc, s->dnpc, true)););
  INSTPAT("1110 1001", jmp,       J,    0, s->dnpc += imm);
  INSTPAT("1110 1011", jmp,       J,    1, s->dnpc += (int8_t)imm);
  INSTPAT("1110 0100", in,        N,    1, { ioaddr_t port = x86_inst_fetch(s, 1); s->dnpc = s->snpc; (void)port; word_t val = 0; IFDEF(CONFIG_DEVICE, val = pio_read(port, 1)); Rw(R_EAX, 1, val); });
  INSTPAT("1110 0101", in,        N,    0, { ioaddr_t port = x86_inst_fetch(s, 1); s->dnpc = s->snpc; (void)port; word_t val = 0; IFDEF(CONFIG_DEVICE, val = pio_read(port, w)); Rw(R_EAX, w, val); });
  INSTPAT("1110 0110", out,       N,    1, { ioaddr_t port = x86_inst_fetch(s, 1); s->dnpc = s->snpc; (void)port; IFDEF(CONFIG_DEVICE, pio_write(port, 1, Rr(R_EAX, 1))); });
  INSTPAT("1110 0111", out,       N,    0, { ioaddr_t port = x86_inst_fetch(s, 1); s->dnpc = s->snpc; (void)port; IFDEF(CONFIG_DEVICE, pio_write(port, w, Rr(R_EAX, w))); });
  INSTPAT("1110 1100", in,        N,    1, { ioaddr_t port = cpu.edx; (void)port; word_t val = 0; IFDEF(CONFIG_DEVICE, val = pio_read(port, 1)); Rw(R_EAX, 1, val); });
  INSTPAT("1110 1101", in,        N,    0, { ioaddr_t port = cpu.edx; (void)port; word_t val = 0; IFDEF(CONFIG_DEVICE, val = pio_read(port, w)); Rw(R_EAX, w, val); });
  INSTPAT("1110 1110", out,       N,    1, { ioaddr_t port = cpu.edx; (void)port; IFDEF(CONFIG_DEVICE, pio_write(port, 1, Rr(R_EAX, 1))); });
  INSTPAT("1110 1111", out,       N,    0, { ioaddr_t port = cpu.edx; (void)port; IFDEF(CONFIG_DEVICE, pio_write(port, w, Rr(R_EAX, w))); });
  INSTPAT("1000 0011", gp1,       SI2E, 0, gp1());
  INSTPAT("0000 1000", or,        G2E,  1, or(ddest, dsrc1));
  INSTPAT("0000 1001", or,        G2E,  0, or(ddest, dsrc1));
  INSTPAT("0000 1010", or,        E2G,  1, or(ddest, dsrc1));
  INSTPAT("0000 1011", or,        E2G,  0, or(ddest, dsrc1));
  INSTPAT("0010 0000", and,       G2E,  1, and(ddest, dsrc1));
  INSTPAT("0010 0001", and,       G2E,  0, and(ddest, dsrc1));
  INSTPAT("0010 0010", and,       E2G,  1, and(ddest, dsrc1));
  INSTPAT("0010 0011", and,       E2G,  0, and(ddest, dsrc1));
  INSTPAT("0010 0100", and,       I2a,  1, and(Rr(R_EAX, 1), imm));
  INSTPAT("0010 0101", and,       I2a,  0, and(Rr(R_EAX, w), imm));
  INSTPAT("0011 0000", xor,       G2E,  1, xor(ddest,dsrc1));
  INSTPAT("0011 0001", xor,       G2E,  0, xor(ddest,dsrc1));
  INSTPAT("0011 0010", xor,       E2G,  1, xor(ddest,dsrc1));
  INSTPAT("0011 0011", xor,       E2G,  0, xor(ddest,dsrc1));
  INSTPAT("1000 1111", pop,       E,    0, { word_t val; pop(val); RMw(val); });
  INSTPAT("1100 0011", ret,       N,    0, pop(s->dnpc); IFDEF(CONFIG_FTRACE, ftrace_write(s->pc, s->dnpc, false)););
  INSTPAT("1000 1101", lea,       E2G,  0, Rw(rd,w,addr));
  INSTPAT("1111 1110", gp4,       E,    1, gp4());
  INSTPAT("1111 1111", gp5,       E,    0, gp5());
  INSTPAT("0000 0000", add,       G2E,  1, { word_t dest = ddest; word_t src = dsrc1; word_t res = dest + src; RMw(res); update_eflags(0, dest, src, res, w); });
  INSTPAT("0000 0001", add,       G2E,  0, { word_t dest = ddest; word_t src = dsrc1; word_t res = dest + src; RMw(res); update_eflags(0, dest, src, res, w); });
  INSTPAT("0000 0010", add,       E2G,  1, { word_t dest = ddest; word_t src = dsrc1; word_t res = dest + src; Rw(rd, w, res); update_eflags(0, dest, src, res, w); });
  INSTPAT("0000 0011", add,       E2G,  0, { word_t dest = ddest; word_t src = dsrc1; word_t res = dest + src; Rw(rd, w, res); update_eflags(0, dest, src, res, w); });
  INSTPAT("0000 0100", add,       I2a,  1, { word_t dest = Rr(R_EAX, 1); word_t src = imm; word_t res = dest + src; Rw(R_EAX, 1, res); update_eflags(0, dest, src, res, 1); });
  INSTPAT("0000 0101", add,       I2a,  0, { word_t dest = Rr(R_EAX, w); word_t src = imm; word_t res = dest + src; Rw(R_EAX, w, res); update_eflags(0, dest, src, res, w); });
  INSTPAT("0001 0000", adc,       G2E,  1, adc(ddest, dsrc1));
  INSTPAT("0001 0001", adc,       G2E,  0, adc(ddest, dsrc1));
  INSTPAT("0001 0010", adc,       E2G,  1, adc(ddest, dsrc1));
  INSTPAT("0001 0011", adc,       E2G,  0, adc(ddest, dsrc1));
  INSTPAT("0001 0100", adc,       I2a,  1, adc(Rr(R_EAX, 1), imm));
  INSTPAT("0001 0101", adc,       I2a,  0, adc(Rr(R_EAX, w), imm));
  INSTPAT("0001 1000", sbb,       G2E,  1, sbb(ddest, dsrc1));
  INSTPAT("0001 1001", sbb,       G2E,  0, sbb(ddest, dsrc1));
  INSTPAT("0001 1010", sbb,       E2G,  1, sbb(ddest, dsrc1));
  INSTPAT("0001 1011", sbb,       E2G,  0, sbb(ddest, dsrc1));
  INSTPAT("0001 1100", sbb,       I2a,  1, sbb(Rr(R_EAX, 1), imm));
  INSTPAT("0001 1101", sbb,       I2a,  0, sbb(Rr(R_EAX, w), imm));
  INSTPAT("1001 0000", nop,       N,    0, );

  INSTPAT("1010 0100", movs,      N,    1, { if (has_rep) { while (cpu.ecx != 0) { Mw(cpu.edi, 1, Mr(cpu.esi, 1)); cpu.esi += (cpu.eflags.DF ? -1 : 1); cpu.edi += (cpu.eflags.DF ? -1 : 1); cpu.ecx --; } } else { Mw(cpu.edi, 1, Mr(cpu.esi, 1)); cpu.esi += (cpu.eflags.DF ? -1 : 1); cpu.edi += (cpu.eflags.DF ? -1 : 1); } });
  INSTPAT("1010 0101", movs,      N,    0, { if (has_rep) { while (cpu.ecx != 0) { Mw(cpu.edi, w, Mr(cpu.esi, w)); cpu.esi += (cpu.eflags.DF ? -w : w); cpu.edi += (cpu.eflags.DF ? -w : w); cpu.ecx --; } } else { Mw(cpu.edi, w, Mr(cpu.esi, w)); cpu.esi += (cpu.eflags.DF ? -w : w); cpu.edi += (cpu.eflags.DF ? -w : w); } });
  INSTPAT("1010 1010", stos,      N,    1, { if (has_rep) { while (cpu.ecx != 0) { Mw(cpu.edi, 1, reg_b(R_AL)); cpu.edi += (cpu.eflags.DF ? -1 : 1); cpu.ecx --; } } else { Mw(cpu.edi, 1, reg_b(R_AL)); cpu.edi += (cpu.eflags.DF ? -1 : 1); } });
  INSTPAT("1010 1011", stos,      N,    0, { if (has_rep) { while (cpu.ecx != 0) { Mw(cpu.edi, w, reg_read(R_EAX, w)); cpu.edi += (cpu.eflags.DF ? -w : w); cpu.ecx --; } } else { Mw(cpu.edi, w, reg_read(R_EAX, w)); cpu.edi += (cpu.eflags.DF ? -w : w); } });

  INSTPAT("0111 ????", jcc, J, 1, {
    int cond = opcode & 0xf;
    bool jump = false;
    switch(cond) {
      case 0: jump = cpu.eflags.OF; break; // jo
      case 1: jump = !cpu.eflags.OF; break; // jno
      case 2: jump = cpu.eflags.CF; break; // jb
      case 3: jump = !cpu.eflags.CF; break; // jae
      case 4: jump = cpu.eflags.ZF; break; // je
      case 5: jump = !cpu.eflags.ZF; break; // jne
      case 6: jump = cpu.eflags.CF || cpu.eflags.ZF; break; // jbe
      case 7: jump = !cpu.eflags.CF && !cpu.eflags.ZF; break; // ja
      case 8: jump = cpu.eflags.SF; break; // js
      case 9: jump = !cpu.eflags.SF; break; // jns
      case 10: panic("JP not implemented"); break; // jp
      case 11: panic("JNP not implemented"); break; // jnp
      case 12: jump = cpu.eflags.SF != cpu.eflags.OF; break; // jl
      case 13: jump = cpu.eflags.SF == cpu.eflags.OF; break; // jge
      case 14: jump = cpu.eflags.ZF || (cpu.eflags.SF != cpu.eflags.OF); break; // jle
      case 15: jump = !cpu.eflags.ZF && (cpu.eflags.SF == cpu.eflags.OF); break; // jg
    }
    if (jump) s->dnpc += (int8_t)imm;
  });
  INSTPAT("1100 1001", leave,     N,    0, cpu.esp = cpu.ebp; pop(cpu.ebp));
  INSTPAT("0011 1011", cmp,       E2G,  0, cmp(ddest, dsrc1));
  INSTPAT("0011 1001", cmp,       G2E,  0, cmp(ddest, dsrc1));
  INSTPAT("0011 1000", cmp,       G2E,  1, cmp(ddest, dsrc1));
  INSTPAT("0011 1100", cmp,       I2a,  1, cmp(Rr(R_EAX, 1), imm));
  INSTPAT("0011 1101", cmp,       I2a,  0, cmp(Rr(R_EAX, w), imm));
  INSTPAT("1000 0100", test,      G2E,  1, test(ddest, dsrc1));
  INSTPAT("1000 0101", test,      G2E,  0, test(ddest, dsrc1));
  INSTPAT("0100 0???", inc,       N,    0, {
  int reg = opcode & 0x7;
  word_t src = Rr(reg, w);
  word_t res = src + 1;
  Rw(reg, w, res);
  bool old_cf = cpu.eflags.CF; // 1. 备份 CF
  update_eflags(0, src, 1, res, w); // 2. 用 ADD 的逻辑更新所有标志位 (GP_IDX=0)
  cpu.eflags.CF = old_cf; // 3. 恢复 CF，假装无事发生
  });
  INSTPAT("0100 1???", dec,       N,    0, {
  int reg = opcode & 0x7;
  word_t src = Rr(reg, w);
  word_t res = src - 1;
  Rw(reg, w, res);
  bool old_cf = cpu.eflags.CF;
  update_eflags(5, src, 1, res, w); // SUB logic
  cpu.eflags.CF = old_cf;
  });
  INSTPAT("0010 1000", sub,       G2E,  1, {
  word_t dest = ddest;
  word_t src = dsrc1;
  word_t res = dest - src;
  RMw(res);
  update_eflags(5, dest, src, res, w);
});
  INSTPAT("0010 1001", sub,       G2E,  0, {
  word_t dest = ddest;
  word_t src = dsrc1;
  word_t res = dest - src;
  RMw(res);
  update_eflags(5, dest, src, res, w);
});
  INSTPAT("0010 1010", sub,       E2G,  1, {
  word_t dest = ddest;
  word_t src = dsrc1;
  word_t res = dest - src;
  Rw(rd, w, res);
  update_eflags(5, dest, src, res, w);
});
  INSTPAT("0010 1011", sub,       E2G,  0, {
  word_t dest = ddest;
  word_t src = dsrc1;
  word_t res = dest - src;
  Rw(rd, w, res);
  update_eflags(5, dest, src, res, w);
});
  INSTPAT("0010 1100", sub,       I2a,  1, {
  word_t dest = Rr(R_EAX, 1);
  word_t src = imm;
  word_t res = dest - src;
  Rw(R_EAX, 1, res);
  update_eflags(5, dest, src, res, 1);
});
  INSTPAT("0010 1101", sub,       I2a,  0, {
  word_t dest = Rr(R_EAX, w);
  word_t src = imm;
  word_t res = dest - src;
  Rw(R_EAX, w, res);
  update_eflags(5, dest, src, res, w);
});
  INSTPAT("1100 1101", int,        I,   1, {
    s->dnpc = isa_raise_intr(imm, s->snpc);
  });
  INSTPAT("0110 0000", pusha, N, 0, {
    word_t temp_esp = cpu.esp;
    push(cpu.eax);
    push(cpu.ecx);
    push(cpu.edx);
    push(cpu.ebx);
    push(temp_esp); // 压入的是执行 pusha 之前的 esp
    push(cpu.ebp);
    push(cpu.esi);
    push(cpu.edi);
  });

  INSTPAT("1111 1000", clc, N, 0, cpu.eflags.CF = 0);
  INSTPAT("1111 1001", stc, N, 0, cpu.eflags.CF = 1);
  INSTPAT("1111 1010", cli, N, 0, cpu.eflags.IF = 0);
  INSTPAT("1111 1011", sti, N, 0, cpu.eflags.IF = 1);
  INSTPAT("1111 1100", cld, N, 0, cpu.eflags.DF = 0);
  INSTPAT("1111 1101", std, N, 0, cpu.eflags.DF = 1);
  INSTPAT("???? ????", inv,       N,    0, INV(s->pc));//通配符
  INSTPAT_END();

  return 0;
}
