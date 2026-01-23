#!/bin/bash
set -e # 遇到错误立即停止

# 保存脚本启动时的根目录
START_DIR=$(pwd)

# 定义路径 (使用相对路径)
NEMU_DIR="nemu"
AM_DIR="abstract-machine"
TESTS_DIR="am-kernels/tests/cpu-tests"
AM_TESTS_DIR="am-kernels/tests/am-tests"
ALU_TESTS_DIR="am-kernels/tests/alu-tests"

# ---------------------------------------------------------
# 0. 环境检查与配置解析
# ---------------------------------------------------------

# 检查 .config 是否存在
CONFIG_FILE="$START_DIR/$NEMU_DIR/.config"
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: .config not found in $NEMU_DIR."
    echo "Please go to $NEMU_DIR and run 'make menuconfig' to configure NEMU first."
    exit 1
fi

# 从 .config 中提取 ISA 架构
# 逻辑：查找 CONFIG_ISA="xxx" 或 CONFIG_ISA=xxx，提取等号后的内容并去除引号
ISA=$(grep "^CONFIG_ISA=" "$CONFIG_FILE" | cut -d'=' -f2 | sed 's/"//g')

if [ -z "$ISA" ]; then
    echo "Error: Could not parse CONFIG_ISA from .config"
    exit 1
fi

# 构造目标架构 (例如 x86-nemu)
TARGET_ARCH="${ISA}-nemu"
echo ">>> Detected ISA: $ISA"
echo ">>> Target Architecture: $TARGET_ARCH"

# 输出文件列表
JSON_LIST=()

# ---------------------------------------------------------
# 1. 生成 NEMU 的编译数据库
# ---------------------------------------------------------
echo ">>> 1. Generating compile_commands.json for NEMU..."
cd "$START_DIR/$NEMU_DIR"
make clean
bear -- make
if [ -f "compile_commands.json" ]; then
    mv compile_commands.json compile_commands_nemu.json
    JSON_LIST+=("$START_DIR/$NEMU_DIR/compile_commands_nemu.json")
else
    echo "Error: Failed to generate compile_commands.json in nemu"
    exit 1
fi

# ---------------------------------------------------------
# 2. 生成 Abstract Machine (Klib) 的编译数据库
# ---------------------------------------------------------
echo ">>> 2. Generating compile_commands.json for Abstract Machine (klib)..."
cd "$START_DIR/$AM_DIR"
# 清理 klib
make -C klib clean
# 生成
bear -- make ARCH=$TARGET_ARCH -C klib archive
if [ -f "compile_commands.json" ]; then
    mv compile_commands.json compile_commands_am_klib.json
    JSON_LIST+=("$START_DIR/$AM_DIR/compile_commands_am_klib.json")
else
    echo "Warning: No compile_commands.json generated for klib"
fi

# ---------------------------------------------------------
# 3. 生成 Abstract Machine (AM Core) 的编译数据库
# ---------------------------------------------------------
echo ">>> 3. Generating compile_commands.json for Abstract Machine (am)..."
cd "$START_DIR/$AM_DIR"
# 清理 am
make -C am clean
# 生成
bear -- make ARCH=$TARGET_ARCH -C am archive
if [ -f "compile_commands.json" ]; then
    mv compile_commands.json compile_commands_am_core.json
    JSON_LIST+=("$START_DIR/$AM_DIR/compile_commands_am_core.json")
else
    echo "Error: Failed to generate compile_commands.json in am"
    exit 1
fi

# ---------------------------------------------------------
# 4. 生成 CPU Tests 的编译数据库
# ---------------------------------------------------------
echo ">>> 4. Generating compile_commands.json for CPU Tests..."
cd "$START_DIR/$TESTS_DIR"
make clean
bear -- make ARCH=$TARGET_ARCH
if [ -f "compile_commands.json" ]; then
    # 这里的 compile_commands.json 就是最终要被覆盖/使用的目标位置
    # 我们先把它加入列表读取内容，最后脚本会重新生成合并版覆盖它
    JSON_LIST+=("$START_DIR/$TESTS_DIR/compile_commands.json")
else
    echo "Error: Failed to generate compile_commands.json in cpu-tests"
    exit 1
fi

# ---------------------------------------------------------
# 4.1. 生成 AM Tests 的编译数据库
# ---------------------------------------------------------
echo ">>> 4.1. Generating compile_commands.json for AM Tests..."
cd "$START_DIR/$AM_TESTS_DIR"
make clean
bear -- make ARCH=$TARGET_ARCH
if [ -f "compile_commands.json" ]; then
    mv compile_commands.json compile_commands_am_tests.json
    JSON_LIST+=("$START_DIR/$AM_TESTS_DIR/compile_commands_am_tests.json")
else
    echo "Warning: Failed to generate compile_commands.json in am-tests"
fi

# ---------------------------------------------------------
# 4.2. 生成 ALU Tests 的编译数据库
# ---------------------------------------------------------
echo ">>> 4.2. Generating compile_commands.json for ALU Tests..."
cd "$START_DIR/$ALU_TESTS_DIR"
make clean
bear -- make ARCH=$TARGET_ARCH
if [ -f "compile_commands.json" ]; then
    mv compile_commands.json compile_commands_alu_tests.json
    JSON_LIST+=("$START_DIR/$ALU_TESTS_DIR/compile_commands_alu_tests.json")
else
    echo "Warning: Failed to generate compile_commands.json in alu-tests"
fi

# ---------------------------------------------------------
# 5. 合并数据库
# ---------------------------------------------------------
echo ">>> Merging JSON files..."
cd "$START_DIR"
FINAL_JSON="$START_DIR/$TESTS_DIR/compile_commands.json"

# 使用 python 进行合并，确保 JSON 格式正确
python3 -c "
import json
import os
import sys

files = sys.argv[1:]
merged_data = []

for f in files:
    if os.path.exists(f):
        try:
            with open(f, 'r') as infile:
                data = json.load(infile)
                if isinstance(data, list):
                    merged_data.extend(data)
                else:
                    print(f'Warning: {f} does not contain a list')
        except Exception as e:
            print(f'Error reading {f}: {e}')
    else:
        print(f'Warning: {f} not found')

# 写入最终文件
with open('$FINAL_JSON', 'w') as outfile:
    json.dump(merged_data, outfile, indent=2)
" "${JSON_LIST[@]}"

echo ">>> Done! Merged compile_commands.json created at: $FINAL_JSON"

# ---------------------------------------------------------
# 6. 清理临时文件
# ---------------------------------------------------------
echo ">>> cleaning temporary files..."
rm -f "$START_DIR/$NEMU_DIR/compile_commands_nemu.json"
rm -f "$START_DIR/$AM_DIR/compile_commands_am_klib.json"
rm -f "$START_DIR/$AM_DIR/compile_commands_am_core.json"
rm -f "$START_DIR/$AM_TESTS_DIR/compile_commands_am_tests.json"
rm -f "$START_DIR/$ALU_TESTS_DIR/compile_commands_alu_tests.json"
# 注意：不删除 $TESTS_DIR/compile_commands.json，因为那是最终输出结果
