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
  bool success;
  char *test_expr;
  
  test_expr = "12";
  success = make_token(test_expr);
  if (!success) {
    printf("ERROR: Tokenization failed for '%s'\n", test_expr);
    return;
  }
  if (nr_token != 1) {
    printf("ERROR: Expected 1 token, got %d for '%s'\n", nr_token, test_expr);
    return;
  }
  printf("SUCCESS: Tokenized '%s' into %d tokens\n", test_expr, nr_token);
  
  test_expr = "1+2";
  success = make_token(test_expr);
  if (!success) {
    printf("ERROR: Tokenization failed for '%s'\n", test_expr);
    return;
  }
  if (nr_token != 3) {
    printf("ERROR: Expected 3 tokens, got %d for '%s'\n", nr_token, test_expr);
    return;
  }
  printf("SUCCESS: Tokenized '%s' into %d tokens\n", test_expr, nr_token);
  
  test_expr = "0x123+456";
  success = make_token(test_expr);
  assert(success);
  assert(nr_token == 3);
  assert(tokens[0].type == TK_NUM && strcmp(tokens[0].str, "0x123") == 0);
  assert(tokens[1].type == '+');
  assert(tokens[2].type == TK_NUM && strcmp(tokens[2].str, "456") == 0);
  
  test_expr = "0x123+456";
  success = make_token(test_expr);
  assert(success);
  assert(nr_token == 3);
  assert(tokens[0].type == TK_NUM && strcmp(tokens[0].str, "0x123") == 0);
  assert(tokens[1].type == '+');
  assert(tokens[2].type == TK_NUM && strcmp(tokens[2].str, "456") == 0);
  
  test_expr = "$eax+$ecx";
  success = make_token(test_expr);
  assert(success);
  assert(nr_token == 3);
  assert(tokens[0].type == TK_REG && strcmp(tokens[0].str, "$eax") == 0);
  assert(tokens[1].type == '+');
  assert(tokens[2].type == TK_REG && strcmp(tokens[2].str, "$ecx") == 0);
  
  test_expr = " ( $eax + 0x10 ) * 2 ";
  success = make_token(test_expr);
  assert(success);
  assert(nr_token == 7);
  assert(tokens[0].type == TK_LPAREN);
  assert(tokens[1].type == TK_REG && strcmp(tokens[1].str, "$eax") == 0);
  assert(tokens[2].type == '+');
  assert(tokens[3].type == TK_NUM && strcmp(tokens[3].str, "0x10") == 0);
  assert(tokens[4].type == TK_RPAREN);
  assert(tokens[5].type == '*');
  assert(tokens[6].type == TK_NUM && strcmp(tokens[6].str, "2") == 0);
  
  test_expr = "5==6";
  success = make_token(test_expr);
  assert(success);
  assert(nr_token == 3);
  assert(tokens[0].type == TK_NUM && strcmp(tokens[0].str, "5") == 0);
  assert(tokens[1].type == TK_EQ);
  assert(tokens[2].type == TK_NUM && strcmp(tokens[2].str, "6") == 0);
  
  test_expr = "0xFF + $eax * (123 - 456) / 789";
  success = make_token(test_expr);
  assert(success);
  assert(nr_token == 11);
  assert(tokens[0].type == TK_NUM && strcmp(tokens[0].str, "0xFF") == 0);
  assert(tokens[1].type == '+');
  assert(tokens[2].type == TK_REG && strcmp(tokens[2].str, "$eax") == 0);
  assert(tokens[3].type == '*');
  assert(tokens[4].type == TK_LPAREN);
  assert(tokens[5].type == TK_NUM && strcmp(tokens[5].str, "123") == 0);
  assert(tokens[6].type == '-');
  assert(tokens[7].type == TK_NUM && strcmp(tokens[7].str, "456") == 0);
  assert(tokens[8].type == TK_RPAREN);
  assert(tokens[9].type == '/');
  assert(tokens[10].type == TK_NUM && strcmp(tokens[10].str, "789") == 0);
  
  printf("All token tests passed!\n");
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  *success = false;
  return 0;
}
