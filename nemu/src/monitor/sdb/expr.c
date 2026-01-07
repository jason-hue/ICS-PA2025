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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ, TK_NOT_EQ, TK_AND,
  TK_NUM,
  TK_REG,
  TK_LPAREN,
  TK_RPAREN,
  TK_DEREF,
  TK_NEGATIVE

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
  {"!=", TK_NOT_EQ},    // not equal
  {"&&", TK_AND},       // and
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

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
        //     i, rules[i].regex, position, substr_len, substr_len, substr_start);

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
  printf("Testing tokens...\n");
  int nr_test = 50;
  for (int i = 0; i < nr_test; i++) {
    char buf[256] = "";
    int expected_types[32];
    char expected_strs[32][32];
    int expected_nr = 0;
    // Generate random expression by concatenating tokens
    while (expected_nr < 30) {
      int r = rand() % NR_REGEX;
      int type = rules[r].token_type;
      char token_str[64] = "";
      if (type == TK_NOTYPE) {
        strcat(buf, " ");
        continue;
      }
      switch (type) {
        case TK_NUM:
          if (rand() % 2 == 0) sprintf(token_str, "%u", rand() % 1000000);
          else sprintf(token_str, "0x%x", rand() % 0xffffff);
          break;
        case TK_REG: {
          const char *regs[] = {"$eax", "$ecx", "$edx", "$ebx", "$esp", "$ebp", "$esi", "$edi", "$pc", "$zero", "$ra", "$sp"};
          strcpy(token_str, regs[rand() % 12]);
          break;
        }
        case '+': strcpy(token_str, "+"); break;
        case '-': strcpy(token_str, "-"); break;
        case '*': strcpy(token_str, "*"); break;
        case '/': strcpy(token_str, "/"); break;
        case TK_EQ: strcpy(token_str, "=="); break;
        case TK_LPAREN: strcpy(token_str, "("); break;
        case TK_RPAREN: strcpy(token_str, ")"); break;
        default: continue;
      }
      // Check if buf has enough space
      if (strlen(buf) + strlen(token_str) + 2 >= sizeof(buf)) break;
      strcat(buf, token_str);
      strcat(buf, " "); // Use space as delimiter to avoid token merging
      expected_types[expected_nr] = type;
      if (type == TK_NUM || type == TK_REG) {
        strncpy(expected_strs[expected_nr], token_str, 31);
        expected_strs[expected_nr][31] = '\0';
      }
      expected_nr++;

      if (rand() % 10 == 0) break; // Randomly terminate the expression
    }
    if (expected_nr == 0) continue;
    // Perform the test
    bool success = make_token(buf);
    Assert(success, "make_token failed on string: \"%s\"", buf);
    Assert(nr_token == expected_nr, "nr_token mismatch: expected %d, got %d. string: \"%s\"", expected_nr, nr_token, buf);
    for (int j = 0; j < nr_token; j++) {
      Assert(tokens[j].type == expected_types[j], "token %d type mismatch: expected %d, got %d. string: \"%s\"", j, expected_types[j], tokens[j].type, buf);
      if (tokens[j].type == TK_NUM || tokens[j].type == TK_REG) {
        Assert(strcmp(tokens[j].str, expected_strs[j]) == 0, "token %d str mismatch: expected \"%s\", got \"%s\". string: \"%s\"", j, expected_strs[j], tokens[j].str, buf);
      }
    }
  }
  printf("Token testing passed!\n");
}

bool check_parentheses(int p, int q)
{
  if (tokens[p].type == TK_LPAREN && tokens[q].type == TK_RPAREN)
  {
    int balance = 0;
    for (int j = p; j <= q; j++)
    {
      if (tokens[j].type == TK_LPAREN)
      {
        balance++;
      }else if (tokens[j].type == TK_RPAREN)
      {
        balance--;
      }
      if ((balance == 0 &&  j < q) || balance < 0)
      {
        return false;
      }
    }
    return balance == 0;
  }
  return false;
}

static int get_operator_priority(int type) {
  switch (type) {
  case TK_AND: return 1;
  case TK_EQ: case TK_NOT_EQ: return 2;
  case '+': case '-': return 3;
  case '*': case '/': return 4;
  case TK_DEREF: case TK_NEGATIVE: return 5;
  default: return 0;
  }
}

static int find_main_operator(int p, int q)
{
  int balance = 0;
  int op = -1;
  int min_op = INT32_MAX;
  for (int i = q; i >= p; i--)
  {
    if (tokens[i].type == TK_RPAREN)
    {
      balance--;
    }else if (tokens[i].type == TK_LPAREN)
    {
      balance++;
    }
    if (balance != 0)
    {
      continue;
    }
    int priority = get_operator_priority(tokens[i].type);
    if (priority == 0) continue;

    if (priority < min_op)
    {
      min_op = priority;
      op = i;
    }
    else if (priority == min_op && (tokens[i].type == TK_DEREF || tokens[i].type == TK_NEGATIVE))
    {
      op = i;
    }
  }
  return op;
}


word_t eval(int p, int q, bool *success) {
  if (p > q) {
    printf("Error: Invalid expression (empty range: p=%d > q=%d)\n", p, q);
    *success = false;
    return 0;
  }
  else if (p == q) {
    if (tokens[p].type == TK_NUM) {
      return strtol(tokens[p].str, NULL, 0);
    }
    if (tokens[p].type == TK_REG) {
      return isa_reg_str2val(tokens[p].str, success);
    }
    *success = false;
    return 0;
  }

  if (check_parentheses(p, q) == true) {
    return eval(p + 1, q - 1, success);
  }

  int op = find_main_operator(p, q);
  if (op == -1) {
    *success = false;
    return 0;
  }

  if (tokens[op].type == TK_DEREF || tokens[op].type == TK_NEGATIVE) {
    word_t val = eval(op + 1, q, success);
    if (!*success) return 0;
    if (tokens[op].type == TK_NEGATIVE) return -val;
    if (tokens[op].type == TK_DEREF) return vaddr_read(val, 4);
  }

  word_t val1 = eval(p, op - 1, success);
  if (!*success) return 0;

  word_t val2 = eval(op + 1, q, success);
  if (!*success) return 0;

  switch (tokens[op].type) {
    case '+': return val1 + val2;
    case '-': return val1 - val2;
    case '*': return val1 * val2;
    case '/': 
      if (val2 == 0) {
        printf("Error: Division by zero\n");
        *success = false;
        return 0;
      }
      return val1 / val2;
    case TK_EQ: return val1 == val2;
    case TK_NOT_EQ: return val1 != val2;
    case TK_AND: return val1 && val2;
    default: *success = false; return 0;
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* Post-processing to distinguish unary operators */
  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type == '*' || tokens[i].type == '-') {
      if (i == 0 || tokens[i-1].type == TK_LPAREN || get_operator_priority(tokens[i-1].type) > 0) {
        tokens[i].type = (tokens[i].type == '*') ? TK_DEREF : TK_NEGATIVE;
      }
    }
  }

  *success = true;
  return eval(0, nr_token-1, success);
}