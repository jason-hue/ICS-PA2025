# NJU ICS PA 2025 (x86)

This repository contains my implementation of the **Integrated Computer Systems (ICS) Programming Assignments** at Nanjing University, 2025.

The project involves building a complete computer system from scratch, including a hardware emulator (NEMU), a hardware abstraction layer (Abstract-Machine), a simple operating system (Nanos-lite), and various user applications (Navy-apps).

## 🚀 Project Overview

- **Architecture:** x86
- **Goal:** To understand the inner workings of a computer system by implementing it step-by-step.
- **Syllabus:** [NJU ICS PA 2025 Guide](https://nju-projectn.github.io/ics-pa-gitbook/ics2025/)

## 🛠 Progress & Milestones

### [PA1] NEMU: Infrastructure
- [x] **SDB (Simple Debugger):** Implemented a debugger with command-line interface.
- [x] **Expression Evaluation:** Supported complex arithmetic expressions with variable and memory access.
- [x] **Watchpoints:** Implemented a watchpoint system to monitor memory/register changes.

### [PA2] NEMU: Instruction Cycle & I/O
- [x] **Instruction Set:** Implemented essential x86 instructions (EFLAGS, CALL, RET, PUSH, POP, etc.).
- [x] **String Library (klib):** Implemented standard C library functions (`sprintf`, `printf`, `malloc`, etc.) for AM.
- [x] **I/O Devices:** Integrated support for Serial, Timer (RTC), Keyboard, and VGA (Frame Buffer).

### [PA3] Nanos-lite: Exceptions & VFS
- [x] **CTE (Context Extension):** Implemented exception and interrupt handling mechanisms.
- [x] **Nanos-lite Kernel:** A simplified OS kernel capable of loading and running programs.
- [x] **VFS (Virtual File System):** Implemented a file system abstraction layer.
- [x] **Device Abstraction:** Integrated hardware devices (Keyboard, VGA, etc.) into the VFS as files.

### [PA4] Paging & Multiprogramming (Pending)
- [ ] **VME (Virtual Memory Extension):** Implementation of x86 paging mechanism.
- [ ] **Multiprogramming:** Support for multiple processes and context switching.

## 📂 Project Structure

| Component | Description |
|-----------|-------------|
| `nemu/` | NJU Emulator (Hardware Emulator) |
| `abstract-machine/` | Hardware Abstraction Layer |
| `nanos-lite/` | Simple Operating System Kernel |
| `navy-apps/` | User Applications & Libraries |

## 🛠 Getting Started

### Prerequisites
Ensure you have the necessary development tools installed (gcc, make, readline, SDL2, etc.).

### Initialization
```bash
bash init.sh nemu
bash init.sh abstract-machine
bash init.sh nanos-lite
bash init.sh navy-apps
```

### Running NEMU
```bash
cd nemu
make menuconfig # Select x86 ISA
make run
```

## 📜 License
This project follows the licensing terms of the NJU ICS PA course.
