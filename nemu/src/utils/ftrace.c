#include <common.h>
#include <elf.h>

#ifdef CONFIG_FTRACE

typedef struct {
  char name[128];
  paddr_t addr;
  size_t size;
} SymbolEntry;

static SymbolEntry *symbols = NULL;
static int nr_sym = 0;
static int capacity = 0;

static void add_symbol(const char *name, paddr_t addr, size_t size) {
  if (nr_sym >= capacity) {
    capacity = (capacity == 0 ? 64 : capacity * 2);
    symbols = realloc(symbols, capacity * sizeof(SymbolEntry));
    assert(symbols);
  }
  strncpy(symbols[nr_sym].name, name, 127);
  symbols[nr_sym].name[127] = '\0';
  symbols[nr_sym].addr = addr;
  symbols[nr_sym].size = size;
  nr_sym++;
}

static const char* find_symbol(paddr_t addr) {
  for (int i = 0; i < nr_sym; i++) {
    if (addr >= symbols[i].addr && addr < symbols[i].addr + symbols[i].size) {
      return symbols[i].name;
    }
  }
  return "???";
}

void init_ftrace(const char *elf_file) {
  if (elf_file == NULL) return;

  FILE *fp = fopen(elf_file, "rb");
  if (fp == NULL) {
    Log("Can not open '%s'. FTRACE will not work.", elf_file);
    return;
  }

  // Determine architecture from config
  // We assume the ELF file matches the CONFIG_ISA bitness
#ifdef CONFIG_ISA64
  Elf64_Ehdr ehdr;
  assert(fread(&ehdr, sizeof(ehdr), 1, fp) == 1);
  // Verify magic
  if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
    Log("Invalid ELF file '%s'", elf_file);
    fclose(fp);
    return;
  }//e_ident是Header的第一个字段

  // Load Section Headers
  Elf64_Shdr *shdrs = malloc(sizeof(Elf64_Shdr) * ehdr.e_shnum);//shdrs是一个数组
  fseek(fp, ehdr.e_shoff, SEEK_SET);//fp指向shdrs入口
  assert(fread(shdrs, sizeof(Elf64_Shdr), ehdr.e_shnum, fp) == ehdr.e_shnum);//e_shnum代表有几个section Header

  // Find .symtab and .strtab
  // We look for SHT_SYMTAB
  char *strtab = NULL;
  Elf64_Sym *symtab = NULL;
  int sym_count = 0;

  for (int i = 0; i < ehdr.e_shnum; i++) {
    if (shdrs[i].sh_type == SHT_SYMTAB) {
      // Found symtab
      sym_count = shdrs[i].sh_size / sizeof(Elf64_Sym);
      symtab = malloc(shdrs[i].sh_size);
      fseek(fp, shdrs[i].sh_offset, SEEK_SET);
      assert(fread(symtab, shdrs[i].sh_size, 1, fp) == 1);

      // Load associated string table
      int strtab_idx = shdrs[i].sh_link;//sh_link 存储的是节区头部表（Section Header Table）中的下标（Index）
      long strtab_offset = shdrs[strtab_idx].sh_offset;
      long strtab_size = shdrs[strtab_idx].sh_size;
      strtab = malloc(strtab_size);
      fseek(fp, strtab_offset, SEEK_SET);
      assert(fread(strtab, strtab_size, 1, fp) == 1);
      break;
    }
  }

  if (symtab && strtab) {
    for (int i = 0; i < sym_count; i++) {
      if (ELF64_ST_TYPE(symtab[i].st_info) == STT_FUNC) {
        add_symbol(strtab + symtab[i].st_name, symtab[i].st_value, symtab[i].st_size);
      }
    }
  }

  free(shdrs);
  if (symtab) free(symtab);
  if (strtab) free(strtab);

#else // 32-bit
  Elf32_Ehdr ehdr;
  assert(fread(&ehdr, sizeof(ehdr), 1, fp) == 1);
  if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
    Log("Invalid ELF file '%s'", elf_file);
    fclose(fp);
    return;
  }

  Elf32_Shdr *shdrs = malloc(sizeof(Elf32_Shdr) * ehdr.e_shnum);
  fseek(fp, ehdr.e_shoff, SEEK_SET);
  assert(fread(shdrs, sizeof(Elf32_Shdr), ehdr.e_shnum, fp) == ehdr.e_shnum);

  char *strtab = NULL;
  Elf32_Sym *symtab = NULL;
  int sym_count = 0;

  for (int i = 0; i < ehdr.e_shnum; i++) {
    if (shdrs[i].sh_type == SHT_SYMTAB) {
      sym_count = shdrs[i].sh_size / sizeof(Elf32_Sym);
      symtab = malloc(shdrs[i].sh_size);
      fseek(fp, shdrs[i].sh_offset, SEEK_SET);
      assert(fread(symtab, shdrs[i].sh_size, 1, fp) == 1);

      int strtab_idx = shdrs[i].sh_link;
      long strtab_offset = shdrs[strtab_idx].sh_offset;
      long strtab_size = shdrs[strtab_idx].sh_size;
      strtab = malloc(strtab_size);
      fseek(fp, strtab_offset, SEEK_SET);
      assert(fread(strtab, strtab_size, 1, fp) == 1);
      break;
    }
  }

  if (symtab && strtab) {
    for (int i = 0; i < sym_count; i++) {
      if (ELF32_ST_TYPE(symtab[i].st_info) == STT_FUNC) {
        add_symbol(strtab + symtab[i].st_name, symtab[i].st_value, symtab[i].st_size);
      }
    }
  }

  free(shdrs);
  if (symtab) free(symtab);
  if (strtab) free(strtab);
#endif

  fclose(fp);
  Log("FTRACE: Loaded %d symbols from %s", nr_sym, elf_file);
}

void ftrace_write(paddr_t pc, paddr_t target, bool is_call) {
  static int depth = 0;
  
  if (is_call) {
    const char *func_name = find_symbol(target);
    log_write(FMT_PADDR ": %*scall [%s@" FMT_PADDR "]\n", pc, depth * 2, "", func_name, target);
    depth++;
  } else {
    // For return, 'target' is actually the destination PC (where we return to),
    // but the doc says "record current PC" to identify which function we are returning FROM.
    // The visual output typically shows "ret [func_name]" where func_name is the current function.
    if (depth > 0) depth--;
    const char *func_name = find_symbol(pc);
    log_write(FMT_PADDR ": %*sret  [%s]\n", pc, depth * 2, "", func_name);
  }
}

#else

void init_ftrace(const char *elf_file) {}
void ftrace_write(paddr_t pc, paddr_t target, bool is_call) {}

#endif
