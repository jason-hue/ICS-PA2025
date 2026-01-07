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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ,
  TK_NUM,
  TK_REG,
  TK_LPAREN,
  TK_RPAREN

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"0[xX][0-9a-fA-F]+", TK_NUM},
  {"[0-9]+", TK_NUM},
  {"\\$[a-zA-Z]+", TK_REG},
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  {"\\-", '-'},         // minus
  {"\\*", '*'},         // multiply
  {"\\/", '/'},         // divide
  {"\\(", TK_LPAREN},
  {"\\)", TK_RPAREN}
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

static void test_tokens();

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }

  test_tokens();
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        if (nr_token >= 32) {
          printf("Error: Too many tokens (max 32)\n");
          return false;
        }
        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;

          case '+':
          case '-':
          case '*':
          case '/':
          case TK_EQ:
          case TK_LPAREN:
          case TK_RPAREN:
            tokens[nr_token].type = rules[i].token_type;
            nr_token++;
            break;

          case TK_NUM:
          case TK_REG:
            tokens[nr_token].type = rules[i].token_type;
            int copy_len = substr_len < 32 ? substr_len : 31;
            strncpy(tokens[nr_token].str, substr_start, copy_len);
            tokens[nr_token].str[copy_len] = '\0';
            nr_token++;
            break;

          default:
            printf("Error: Unknown token type %d\n", rules[i].token_type);
            return false;
            break;
        }
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("DEBUG: No match found!\n");
      printf("  position=%d, char='%c' (0x%02x)\n", position, e[position], (unsigned char)e[position]);
      printf("  remaining string: '%s'\n", e + position);
      printf("Available rules:\n");
      for (int j = 0; j < NR_REGEX; j++) {
        printf("  [%d] \"%s\" -> %d\n", j, rules[j].regex, rules[j].token_type);
      }
      printf("%s\n%*.s^\n", e, position, "");
      return false;
    }
  }

  return true;
}

static void test_tokens() {
  const char *numbers[] = {"123", "456", "789", "0x123", "0xABC", "0xff"};
  const char *registers[] = {"$eax", "$ebx", "$ecx", "$edx", "$esp", "$ebp"};
  const char *operators[] = {"+", "-", "*", "/", "=="};
  
  int num_tests = 50;
  int passed = 0;
  
  srand(12345);
  
  printf("Running %d random tokenization tests...\n", num_tests);
  
  for (int i = 0; i < num_tests; i++) {
    char test_expr[256] = {0};
    
    int num_parts = rand() % 4 + 2;
    
    for (int j = 0; j < num_parts; j++) {
      if (j > 0) {
        int op_idx = rand() % (sizeof(operators)/sizeof(operators[0]));
        strcat(test_expr, operators[op_idx]);
        strcat(test_expr, " ");
      }
      
      int choice = rand() % 3;
      if (choice == 0) {
        int num_idx = rand() % (sizeof(numbers)/sizeof(numbers[0]));
        strcat(test_expr, numbers[num_idx]);
      } else if (choice == 1) {
        int reg_idx = rand() % (sizeof(registers)/sizeof(registers[0]));
        strcat(test_expr, registers[reg_idx]);
      } else {
        strcat(test_expr, "(");
        int num_idx = rand() % (sizeof(numbers)/sizeof(numbers[0]));
        strcat(test_expr, numbers[num_idx]);
        strcat(test_expr, ")");
        j++;
      }
    }
    
    bool success = make_token(test_expr);
    if (success) {
      printf("Test %3d: PASS - '%s' -> %d tokens\n", i+1, test_expr, nr_token);
      passed++;
    } else {
      printf("Test %3d: FAIL - '%s' (tokenization failed)\n", i+1, test_expr);
    }
  }
  
  printf("\nRandom test results: %d/%d passed (%.1f%%)\n", 
         passed, num_tests, (float)passed/num_tests * 100);
         
  if (passed == num_tests) {
    printf("All random tests passed!\n");
  } else {
    printf("Some tests failed. Check the implementation.\n");
  }
}

static bool check_parentheses_match(int p, int q) {
  int balance = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_LPAREN) {
      balance++;
    } else if (tokens[i].type == TK_RPAREN) {
      balance--;
    }
    if (balance < 0) return false;
  }
  return balance == 0;
}

static int get_precedence(int op_type) {
  switch (op_type) {
    case TK_EQ: return 1;
    case '+': case '-': return 2;
    case '*': case '/': return 3;
    default: return 0;
  }
}

static int find_main_operator(int p, int q) {
  int min_prec = 100;
  int main_op = -1;
  int paren_balance = 0;
  
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == TK_LPAREN) {
      paren_balance++;
    } else if (tokens[i].type == TK_RPAREN) {
      paren_balance--;
    }
    
    if (paren_balance == 0 && 
        (tokens[i].type == '+' || tokens[i].type == '-' || 
         tokens[i].type == '*' || tokens[i].type == '/' || 
         tokens[i].type == TK_EQ)) {
      int prec = get_precedence(tokens[i].type);
      if (prec <= min_prec) {
        min_prec = prec;
        main_op = i;
      }
    }
  }
  return main_op;
}

static word_t get_token_value(int idx, bool *success) {
  if (idx < 0 || idx >= nr_token) {
    *success = false;
    return 0;
  }
  
  if (tokens[idx].type == TK_NUM) {
    char *endptr;
    word_t val = strtoul(tokens[idx].str, &endptr, 0);
    if (*endptr != '\0') {
      *success = false;
      return 0;
    }
    *success = true;
    return val;
} else if (tokens[idx].type == TK_REG) {
    bool reg_success;
    word_t val = isa_reg_str2val(tokens[idx].str + 1, &reg_success);
    if (reg_success) {
      *success = true;
      return val;
    } else {
      *success = false;
      return 0;
    }
  } else {
    *success = false;
    return 0;
}
  
  *success = false;
  return 0;
}

static word_t compute_operator(word_t val1, int op_type, word_t val2, bool *success) {
  switch (op_type) {
    case '+':
      *success = true;
      return val1 + val2;
    case '-':
      *success = true;
      return val1 - val2;
    case '*':
      *success = true;
      return val1 * val2;
    case '/':
      if (val2 == 0) {
        *success = false;
        return 0;
      }
      *success = true;
      return val1 / val2;
    case TK_EQ:
      *success = true;
      return val1 == val2;
    default:
      *success = false;
      return 0;
  }
}

static word_t eval(int p, int q, bool *success) {
  if (p > q) {
    *success = false;
    return 0;
  }
  
  if (p == q) {
    return get_token_value(p, success);
  }
  
  if (tokens[p].type == TK_LPAREN && tokens[q].type == TK_RPAREN) {
    if (check_parentheses_match(p, q)) {
      return eval(p + 1, q - 1, success);
    }
  }
  
  int main_op = find_main_operator(p, q);
  if (main_op != -1) {
    word_t val1 = eval(p, main_op - 1, success);
    if (!*success) return 0;
    
    word_t val2 = eval(main_op + 1, q, success);
    if (!*success) return 0;
    
    return compute_operator(val1, tokens[main_op].type, val2, success);
  }
  
  *success = false;
  return 0;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  if (nr_token == 0) {
    *success = false;
    return 0;
  }

  return eval(0, nr_token - 1, success);
}
