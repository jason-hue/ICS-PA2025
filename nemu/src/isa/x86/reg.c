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
#include "local-include/reg.h"
#include <ctype.h>

const char *regsl[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
const char *regsw[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
const char *regsb[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};

void reg_test() {
  word_t sample[8];
  word_t pc_sample = rand();
  cpu.pc = pc_sample;

  int i;
  for (i = R_EAX; i <= R_EDI; i ++) {
    sample[i] = rand();
    reg_l(i) = sample[i];
    assert(reg_w(i) == (sample[i] & 0xffff));
  }

  assert(reg_b(R_AL) == (sample[R_EAX] & 0xff));
  assert(reg_b(R_AH) == ((sample[R_EAX] >> 8) & 0xff));
  assert(reg_b(R_BL) == (sample[R_EBX] & 0xff));
  assert(reg_b(R_BH) == ((sample[R_EBX] >> 8) & 0xff));
  assert(reg_b(R_CL) == (sample[R_ECX] & 0xff));
  assert(reg_b(R_CH) == ((sample[R_ECX] >> 8) & 0xff));
  assert(reg_b(R_DL) == (sample[R_EDX] & 0xff));
  assert(reg_b(R_DH) == ((sample[R_EDX] >> 8) & 0xff));

  assert(sample[R_EAX] == cpu.eax);
  assert(sample[R_ECX] == cpu.ecx);
  assert(sample[R_EDX] == cpu.edx);
  assert(sample[R_EBX] == cpu.ebx);
  assert(sample[R_ESP] == cpu.esp);
  assert(sample[R_EBP] == cpu.ebp);
  assert(sample[R_ESI] == cpu.esi);
  assert(sample[R_EDI] == cpu.edi);

  assert(pc_sample == cpu.pc);

  /* Test isa_reg_str2val */
  bool success;
  assert(isa_reg_str2val("$eax", &success) == cpu.eax && success);
  assert(isa_reg_str2val("$ax", &success) == (cpu.eax & 0xffff) && success);
  assert(isa_reg_str2val("$al", &success) == (cpu.eax & 0xff) && success);
  assert(isa_reg_str2val("$pc", &success) == cpu.pc && success);
}

void isa_reg_display() {
  printf("=== Register Information ===\n");
  for (int i = R_EAX; i <= R_EDI; i++) {
    printf("%-3s = 0x%08x\n", regsl[i], reg_l(i));
  }
  printf("pc  = 0x%08x\n", cpu.pc);
}

static void to_lower(char *str) {
  if (str == NULL) return;
  while (*str != '\0') {
    *str = tolower((unsigned char)*str);
    str++;
  }
}

word_t isa_reg_str2val(const char *s, bool *success) {
  // Skip the '$' prefix
  if (s[0] != '$') {
    if (success) *success = false;
    return 0;
  }
  
  char reg_name_buf[16];
  strncpy(reg_name_buf, s + 1, 15);
  reg_name_buf[15] = '\0';
  to_lower(reg_name_buf);
  const char *reg_name = reg_name_buf;

  // 1. Check 32-bit registers (regsl)
  for (int i = 0; i < sizeof(regsl)/sizeof(regsl[0]); i++) {
    if (strcmp(reg_name, regsl[i]) == 0) {
      if (success) *success = true;
      return reg_l(i);
    }
  }

  // 2. Check 16-bit registers (regsw)
  for (int i = 0; i < sizeof(regsw)/sizeof(regsw[0]); i++) {
    if (strcmp(reg_name, regsw[i]) == 0) {
      if (success) *success = true;
      return reg_w(i) & 0xffff;
    }
  }

  // 3. Check 8-bit registers (regsb)
  for (int i = 0; i < sizeof(regsb)/sizeof(regsb[0]); i++) {
    if (strcmp(reg_name, regsb[i]) == 0) {
      if (success) *success = true;
      return reg_b(i) & 0xff;
    }
  }

  // 4. Check pc
  if (strcmp(reg_name, "pc") == 0) {
    if (success) *success = true;
    return cpu.pc;
  }
  
  if (success) *success = false;
  return 0;
}
