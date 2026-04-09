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
NANOS_LITE_DIR="nanos-lite"
NAVY_APPS_DIR="navy-apps"
FCEUX_AM_DIR="fceux-am"
MGBA_AM_DIR="mgba"

# ---------------------------------------------------------
# 0. 环境检查与配置解析
# ---------------------------------------------------------

# 解析参数：支持 -j 开启多核编译
MAKE_FLAGS=""
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -j) MAKE_FLAGS="-j$(nproc)"; shift ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
done

if [ -n "$MAKE_FLAGS" ]; then
    echo ">>> Multi-core compilation enabled: $MAKE_FLAGS"
fi

# Check if am-kernels directory exists
AM_KERNELS_DIR="$START_DIR/am-kernels"
if [ ! -d "$AM_KERNELS_DIR" ] || [ -z "$(ls -A "$AM_KERNELS_DIR" 2>/dev/null)" ]; then
    echo -e "\033[1;37;41m Error: am-kernels directory not found or empty. Please run 'bash init.sh am-kernels' first. \033[0m"
    exit 1
fi

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
bear -- make $MAKE_FLAGS
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
bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH -C klib archive
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
bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH -C am archive
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
bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH
if [ -f "compile_commands.json" ]; then
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
bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH
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
bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH
if [ -f "compile_commands.json" ]; then
    mv compile_commands.json compile_commands_alu_tests.json
    JSON_LIST+=("$START_DIR/$ALU_TESTS_DIR/compile_commands_alu_tests.json")
else
    echo "Warning: Failed to generate compile_commands.json in alu-tests"
fi

# ---------------------------------------------------------
# 4.3. 生成 Navy-apps 的编译数据库
# ---------------------------------------------------------
echo ">>> 4.3. Generating compile_commands.json for Navy-apps..."
cd "$START_DIR/$NAVY_APPS_DIR"
# 设置 NAVY_HOME 环境变量
export NAVY_HOME="$START_DIR/$NAVY_APPS_DIR"
make clean-all
# 编译 fsimg 包含默认的 apps 和 tests (触发 libs 的编译)
bear -- make $MAKE_FLAGS ISA=$ISA fsimg
if [ -f "compile_commands.json" ]; then
    mv compile_commands.json compile_commands_navy_apps.json
    JSON_LIST+=("$START_DIR/$NAVY_APPS_DIR/compile_commands_navy_apps.json")
else
    echo "Warning: Failed to generate compile_commands.json in navy-apps"
fi

# ---------------------------------------------------------
# 4.3.1. 生成 libminiSDL 的编译数据库
# ---------------------------------------------------------
echo ">>> 4.3.1. Generating compile_commands.json for libminiSDL..."
cd "$START_DIR/$NAVY_APPS_DIR/libs/libminiSDL"
make clean
bear -- make $MAKE_FLAGS ISA=$ISA archive
if [ -f "compile_commands.json" ]; then
    mv compile_commands.json compile_commands_libminisdl.json
    JSON_LIST+=("$START_DIR/$NAVY_APPS_DIR/libs/libminiSDL/compile_commands_libminisdl.json")
else
    echo "Warning: Failed to generate compile_commands.json for libminiSDL"
fi

# ---------------------------------------------------------
# 4.4. 生成 Nanos-lite 的编译数据库
# ---------------------------------------------------------
echo ">>> 4.4. Generating compile_commands.json for Nanos-lite..."
cd "$START_DIR/$NANOS_LITE_DIR"
make clean
make ARCH=$TARGET_ARCH update
bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH
if [ -f "compile_commands.json" ]; then
    mv compile_commands.json compile_commands_nanos_lite.json
    JSON_LIST+=("$START_DIR/$NANOS_LITE_DIR/compile_commands_nanos_lite.json")
else
    echo "Warning: Failed to generate compile_commands.json in nanos-lite"
fi
# ---------------------------------------------------------
# 4.5. 生成 AM Kernels 的编译数据库
# ---------------------------------------------------------
echo ">>> 4.5. Generating compile_commands.json for AM Kernels..."
KERNELS_DIR="$START_DIR/am-kernels/kernels"
for d in $(ls -d $KERNELS_DIR/*/); do
    KERNEL_NAME=$(basename $d)
    echo "Processing kernel: $KERNEL_NAME"
    # 检查是否为标准 AM 项目 (包含 include $(AM_HOME)/Makefile)
    if [ -f "$d/Makefile" ] && grep -q "include \$(AM_HOME)/Makefile" "$d/Makefile"; then
        cd "$d"
        make clean
        bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH
        if [ -f "compile_commands.json" ]; then
            mv compile_commands.json "compile_commands_kernel_${KERNEL_NAME}.json"
            JSON_LIST+=("$d/compile_commands_kernel_${KERNEL_NAME}.json")
        else
            echo "Warning: Failed to generate compile_commands.json for kernel $KERNEL_NAME"
        fi
    else
        echo "Skipping non-standard AM project: $KERNEL_NAME"
    fi
done

# ---------------------------------------------------------
# 4.6. 生成 AM Benchmarks 的编译数据库
# ---------------------------------------------------------
echo ">>> 4.6. Generating compile_commands.json for AM Benchmarks..."
BENCHMARKS_DIR="$START_DIR/am-kernels/benchmarks"
for d in $(ls -d $BENCHMARKS_DIR/*/); do
    BENCH_NAME=$(basename $d)
    echo "Processing benchmark: $BENCH_NAME"
    cd "$d"
    make clean
    bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH
    if [ -f "compile_commands.json" ]; then
        mv compile_commands.json "compile_commands_bench_${BENCH_NAME}.json"
        JSON_LIST+=("$d/compile_commands_bench_${BENCH_NAME}.json")
    else
        echo "Warning: Failed to generate compile_commands.json for benchmark $BENCH_NAME"
    fi
done

# ---------------------------------------------------------
# 4.7. 生成 fceux-am 的编译数据库
# ---------------------------------------------------------
echo ">>> 4.7. Generating compile_commands.json for fceux-am..."
if [ -d "$START_DIR/$FCEUX_AM_DIR" ]; then
    cd "$START_DIR/$FCEUX_AM_DIR"
    make clean
    bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH
    if [ -f "compile_commands.json" ]; then
        mv compile_commands.json compile_commands_fceux_am.json
        JSON_LIST+=("$START_DIR/$FCEUX_AM_DIR/compile_commands_fceux_am.json")
    else
        echo "Warning: Failed to generate compile_commands.json in fceux-am"
    fi
else
    echo "Warning: fceux-am directory not found, skipping"
fi

# ---------------------------------------------------------
# 4.8. 生成 mgba-am 的编译数据库
# ---------------------------------------------------------
echo ">>> 4.8. Generating compile_commands.json for mgba-am..."
if [ -d "$START_DIR/$MGBA_AM_DIR" ]; then
    cd "$START_DIR/$MGBA_AM_DIR"
    make clean
    bear -- make $MAKE_FLAGS ARCH=$TARGET_ARCH
    if [ -f "compile_commands.json" ]; then
        mv compile_commands.json compile_commands_mgba_am.json
        JSON_LIST+=("$START_DIR/$MGBA_AM_DIR/compile_commands_mgba_am.json")
    else
        echo "Warning: Failed to generate compile_commands.json in mgba-am"
    fi
else
    echo "Warning: mgba-am directory not found, skipping"
fi



# ---------------------------------------------------------
# 5. 合并数据库
# ---------------------------------------------------------
echo ">>> Merging JSON files..."
cd "$START_DIR"
FINAL_JSON="$START_DIR/compile_commands.json"

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
for f in "${JSON_LIST[@]}"; do
    if [[ "$f" != "$FINAL_JSON" ]]; then
        rm -f "$f"
    fi
done
