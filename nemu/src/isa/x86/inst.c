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
    case TYPE_G2E:  decode_rm(s, rd_, addr, rs, w); src1r(*rs); break;//reg字段是源寄存器编号,看数据流向可以得出，为0则reg为源。适用范围：ADD, OR, ADC, SBB, AND, SUB, XOR, CMP, MOV。
    case TYPE_E2G:  decode_rm(s, rs, addr, rd_, w); break;//reg字段是目的寄存器编号，看opcode低位第2位，为1则reg为目的，不适用范围：LEA, PUSH, POP, INC, DEC 等功能单一的指令。
    case TYPE_I2E:  decode_rm(s, rd_, addr, gp_idx, w); imm(); break;
    case TYPE_O2a:  destr(R_EAX); *addr = x86_inst_fetch(s, 4); break;
    case TYPE_a2O:  *rs = R_EAX;  *addr = x86_inst_fetch(s, 4); break;
    case TYPE_N:    break;
    case TYPE_I:    imm(); break;
    case TYPE_J:    imm(); break;
    case TYPE_SI2E: decode_rm(s, rd_, addr, gp_idx, w); simm(1);break;
    case TYPE_E:    decode_rm(s,   rs, addr, gp_idx, w);break;
    default: panic("Unsupported type = %d", type);
  }
}
static inline void update_eflags(int gp_idx, word_t dest, word_t src, word_t res, int width)
{
  int shift = width * 8 - 1;
  cpu.eflags.ZF = (res == 0);
  cpu.eflags.SF = (res >> shift) & 1;
  // CF & OF: 只有加减法需要
  if (gp_idx == 0) { // ADD
    cpu.eflags.CF = (res < dest); // 无符号溢出
    // 有符号溢出: 两个加数符号相同，但与结果符号不同
    cpu.eflags.OF = ((dest >> shift) == (src >> shift)) && ((dest >> shift) != (res >> shift));
  }
  else if (gp_idx == 5 || gp_idx == 7) { // SUB (cmp 也用这个)
    cpu.eflags.CF = (dest < src); // 无符号借位
    // 有符号溢出: 减数与被减数符号不同，且结果符号与被减数不同
    // (例如: 正 - 负 = 负)
    cpu.eflags.OF = ((dest >> shift) != (src >> shift)) && ((dest >> shift) != (res >> shift));
  }else if (gp_idx == 4)
  {
    cpu.eflags.OF = 0;
    cpu.eflags.CF = 0;
  }

  // AND, OR, XOR 等指令通常会清空 CF/OF，这里暂略
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
    case 0: res = dest + src; RMw(res); update_eflags(0, dest, src, res, w); break; /* ADD */ \
    case 5: res = dest - src; RMw(res); update_eflags(5, dest, src, res, w); break; /* SUB */ \
    case 4: RMw(dest & src);update_eflags(4,dest,src,res,w);break; /* AND */\
    case 7: res = dest - src;  update_eflags(7, dest, src, res, w); break;\
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

#define gp5() do{ \
  switch (gp_idx){ \
    case 2: \
      push(s->snpc); \
      s->dnpc = dsrc1; \
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

#define xor(dest, src) do { \
  word_t res = dest ^ src; \
  RMw(res); \
  cpu.eflags.CF = 0; \
  cpu.eflags.OF = 0; \
  cpu.eflags.ZF = (res == 0); \
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
  INSTPAT("1001 0100", setcc, E, 1, {
     word_t val = cpu.eflags.ZF;
     if (rs != -1) Rw(rs, 1, val); else Mw(addr, 1, val);
     // 宽度强制为 1
 });

  INSTPAT("1011 0110", movzx, E2G, 0, {
    word_t src = (rs != -1 ? Rr(rs, 1) : Mr(addr, 1)); // 强制宽 1
    Rw(rd, w, src); // 零扩展是自动的，因为 src 是 word_t(uint32)，读出来高位就是0
});

  INSTPAT("???? ????", inv,    N,    0, INV(s->pc));
  INSTPAT_END();
}

int isa_exec_once(Decode *s) {
  bool is_operand_size_16 = false;
  uint8_t opcode = 0;

again:
  opcode = x86_inst_fetch(s, 1);

  INSTPAT_START();

  //INSTPAT(模式, 名称, 译码类型, 宽度标志, 执行逻辑);
  /* rd, rs, gp_idx, src1, addr, imm, w这些变量在INSTPAT_MATCH宏中已经被填充了，可以直接用 */

  INSTPAT("0000 1111", 2byte_esc, N,    0, _2byte_esc(s, is_operand_size_16));

  INSTPAT("0110 0110", data_size, N,    0, is_operand_size_16 = true; goto again;);

  INSTPAT("1000 0000", gp1,       I2E,  1, gp1());
  INSTPAT("1000 1000", mov,       G2E,  1, RMw(src1));
  INSTPAT("1000 1001", mov,       G2E,  0, RMw(src1));
  INSTPAT("1000 1010", mov,       E2G,  1, Rw(rd, w, RMr(rs, w)));
  INSTPAT("1000 1011", mov,       E2G,  0, Rw(rd, w, RMr(rs, w)));

  INSTPAT("1010 0000", mov,       O2a,  1, Rw(R_EAX, 1, Mr(addr, 1)));
  INSTPAT("1010 0001", mov,       O2a,  0, Rw(R_EAX, w, Mr(addr, w)));
  INSTPAT("1010 0010", mov,       a2O,  1, Mw(addr, 1, Rr(R_EAX, 1)));
  INSTPAT("1010 0011", mov,       a2O,  0, Mw(addr, w, Rr(R_EAX, w)));

  INSTPAT("1011 0???", mov,       I2r,  1, Rw(rd, 1, imm));
  INSTPAT("1011 1???", mov,       I2r,  0, Rw(rd, w, imm));
  INSTPAT("0101 0???", push,      N,    0, push(Rr(opcode & 0x7,w)));
  INSTPAT("0101 1???", pop,       N,    0, { word_t val; pop(val); Rw(opcode & 0x7, w, val); });
  INSTPAT("0110 1000", push,      I,    0, push(imm));
  INSTPAT("1100 0110", mov,       I2E,  1, RMw(imm));
  INSTPAT("1100 0111", mov,       I2E,  0, RMw(imm));
  INSTPAT("1100 1100", nemu_trap, N,    0, NEMUTRAP(s->pc, cpu.eax));
  INSTPAT("1110 1000", call,      J,    0, push(s->snpc);s->dnpc = s->snpc + imm);
  INSTPAT("1000 0011", gp1,       SI2E, 0, gp1());
  INSTPAT("0011 0001", xor,       G2E,  0, xor(ddest,src1));
  INSTPAT("1000 1111", pop,       E,    0, { word_t val; pop(val); RMw(val); });
  INSTPAT("1100 0011", ret,       N,    0, pop(s->dnpc));
  INSTPAT("1000 1101", lea,       E2G,  0, Rw(rd,w,addr));
  INSTPAT("1111 1111", gp5,       E,    0, gp5());
  INSTPAT("0000 0001", add,       G2E,  0, { word_t dest = ddest; word_t res = dest + src1; RMw(res); update_eflags(0, dest, src1, res, w); });
  INSTPAT("0111 0100", je,        J,    1, if (cpu.eflags.ZF) s->dnpc += (int8_t)imm);
  INSTPAT("1100 1001", leave,     N,    0, cpu.esp = cpu.ebp; pop(cpu.ebp));
  INSTPAT("1000 0100", test,      G2E,  1, test(ddest, src1));
  INSTPAT("0111 0101", jne,       J,    1, if (!cpu.eflags.ZF) s->dnpc += (int8_t)imm);
  INSTPAT("0011 1011", cmp,       E2G,  0, cmp(ddest, src1));
  INSTPAT("0011 1000", cmp,       G2E,  1, cmp(ddest, src1));
  INSTPAT("1000 0101", test,      G2E,  0, test(ddest, src1));
  INSTPAT("0100 0???", inc,       N,    0, {
  int reg = opcode & 0x7;
  word_t src = Rr(reg, w);
  word_t res = src + 1;
  Rw(reg, w, res);
  bool old_cf = cpu.eflags.CF; // 1. 备份 CF
  update_eflags(0, src, 1, res, w); // 2. 用 ADD 的逻辑更新所有标志位 (GP_IDX=0)
  cpu.eflags.CF = old_cf; // 3. 恢复 CF，假装无事发生
  });
  INSTPAT("0010 1001", sub,       G2E,  0, {
  word_t dest = ddest;
  word_t res = dest - src1;
  RMw(res);
  update_eflags(5, dest, src1, res, w);
});

  INSTPAT("???? ????", inv,       N,    0, INV(s->pc));//通配符
  INSTPAT_END();

  return 0;
}
