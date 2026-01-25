#include <proc.h>
#include <elf.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

#if defined(__ISA_AM_NATIVE__)
# define EXPECT_TYPE EM_X86_64  // 宿主机通常是 x86_64
#elif defined(__ISA_X86__)
# define EXPECT_TYPE EM_386     // 32位 x86 架构
#elif defined(__ISA_MIPS32__)
# define EXPECT_TYPE EM_MIPS    // MIPS 架构
#elif defined(__ISA_RISCV32__) || defined(__ISA_RISCV64__)
# define EXPECT_TYPE EM_RISCV   // RISC-V 架构
#else
# error Unsupported ISA
#endif

static uintptr_t loader(PCB *pcb, const char *filename) {
  Elf_Ehdr ehdr;
  ramdisk_read(&ehdr, 0, sizeof(Elf_Ehdr));
  assert(memcmp(ehdr.e_ident, ELFMAG, SELFMAG) == 0);
  if (ehdr.e_machine != EXPECT_TYPE) {
    Log("ISA mismatch! Expected: %d, ELF: %d", EXPECT_TYPE, ehdr.e_machine);
    panic("Cannot load this ELF: ISA type error.");
  }
  Elf_Phdr* phdr = malloc(ehdr.e_phnum * sizeof(Elf_Phdr));
  ramdisk_read(phdr, ehdr.e_phoff, ehdr.e_phnum * sizeof(Elf_Phdr));
  for(int i = 0; i < ehdr.e_phnum; i++)
  {
    if(phdr[i].p_type == PT_LOAD) {
      ramdisk_read((void*)phdr[i].p_vaddr, phdr[i].p_offset, phdr[i].p_filesz);
      if (phdr[i].p_memsz > phdr[i].p_filesz) {
        memset((void*)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
      }
    }
  }
    return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", entry);
  ((void(*)())entry) ();
}

