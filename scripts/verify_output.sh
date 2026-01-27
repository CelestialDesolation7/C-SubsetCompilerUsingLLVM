#!/usr/bin/env bash

# 结果验证脚本：编译运行 ToyC 和 Clang 生成的汇编，对比输出结果
# 使用 Clang 作为汇编器和链接器

set -eu
if [[ -n "${BASH_VERSION:-}" ]]; then
    set -o pipefail
fi

# 支持通过参数指定源目录，默认为 examples/compiler_inputs
SRC_DIR="${1:-examples/compiler_inputs}"
ASM_DIR="test/asm"

# 检查源目录是否存在
if [[ ! -d "$SRC_DIR" ]]; then
  echo "Error: Source directory '$SRC_DIR' does not exist"
  exit 1
fi

# 检查汇编输出目录
if [[ ! -d "$ASM_DIR" ]]; then
  echo "Error: Assembly directory '$ASM_DIR' not found"
  echo "Please run 'make test' first to generate assembly files"
  exit 1
fi

# Clang 目标三元组和架构参数
CLANG_TARGET="riscv32-unknown-elf"
CLANG_ARCH="rv32im"
CLANG_ABI="ilp32"

# 检查 Clang 是否存在
if ! command -v clang >/dev/null 2>&1; then
  echo "Error: Clang not found"
  echo "Please install Clang to verify outputs"
  echo ""
  echo "Installation guide:"
  echo "  Ubuntu/Debian: sudo apt install clang"
  exit 1
fi

# 检查 Clang 是否支持 RISC-V 目标
if ! clang --target=$CLANG_TARGET -march=$CLANG_ARCH -mabi=$CLANG_ABI -c -x assembler /dev/null -o /dev/null 2>/dev/null; then
  echo "Error: Clang does not support RISC-V target"
  echo "Your Clang may not have RISC-V backend enabled"
  echo ""
  echo "Try installing a full LLVM toolchain or use a version with RISC-V support"
  exit 1
fi

echo "Note: Using Clang with --target=$CLANG_TARGET"

# 查找 libgcc（提供软件除法等内置函数）
# 使用 riscv64-unknown-elf-gcc 获取 rv32im/ilp32 的 libgcc 路径
LIBGCC_PATH=""
if command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  LIBGCC_PATH=$(riscv64-unknown-elf-gcc -march=$CLANG_ARCH -mabi=$CLANG_ABI -print-libgcc-file-name 2>/dev/null || true)
  if [[ -n "$LIBGCC_PATH" ]] && [[ -f "$LIBGCC_PATH" ]]; then
    echo "Note: Using libgcc from $LIBGCC_PATH"
  else
    LIBGCC_PATH=""
  fi
elif command -v riscv32-unknown-elf-gcc >/dev/null 2>&1; then
  LIBGCC_PATH=$(riscv32-unknown-elf-gcc -march=$CLANG_ARCH -mabi=$CLANG_ABI -print-libgcc-file-name 2>/dev/null || true)
  if [[ -n "$LIBGCC_PATH" ]] && [[ -f "$LIBGCC_PATH" ]]; then
    echo "Note: Using libgcc from $LIBGCC_PATH"
  else
    LIBGCC_PATH=""
  fi
fi

if [[ -z "$LIBGCC_PATH" ]]; then
  echo "Warning: libgcc not found, some tests with division may fail"
fi

# 检查 QEMU RISC-V 用户模式
QEMU_CMD=""
if command -v qemu-riscv32 >/dev/null 2>&1; then
  QEMU_CMD="qemu-riscv32"
elif command -v qemu-riscv32-static >/dev/null 2>&1; then
  QEMU_CMD="qemu-riscv32-static"
elif command -v qemu-riscv64 >/dev/null 2>&1; then
  QEMU_CMD="qemu-riscv64"
  echo "Note: Using qemu-riscv64 for RV32 emulation"
elif command -v qemu-riscv64-static >/dev/null 2>&1; then
  QEMU_CMD="qemu-riscv64-static"
  echo "Note: Using qemu-riscv64-static for RV32 emulation"
else
  echo "Error: QEMU RISC-V user mode emulator not found"
  echo "Please install qemu-user or qemu-user-static"
  echo ""
  echo "Installation:"
  echo "  Ubuntu/Debian: sudo apt install qemu-user"
  exit 1
fi

# 创建临时目录用于编译
TEMP_DIR="test/verify_temp"
mkdir -p "$TEMP_DIR"

# 获取脚本所在目录（用于找到 crt0.s）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CRT0_FILE="$SCRIPT_DIR/crt0.s"

# 检查启动文件是否存在
if [[ ! -f "$CRT0_FILE" ]]; then
  echo "Error: Startup file '$CRT0_FILE' not found"
  exit 1
fi

# 编译启动文件
CRT0_OBJ="$TEMP_DIR/crt0.o"
if ! clang --target=$CLANG_TARGET -march=$CLANG_ARCH -mabi=$CLANG_ABI \
     -c "$CRT0_FILE" -o "$CRT0_OBJ" 2>/dev/null; then
  echo "Error: Failed to compile startup file with Clang"
  exit 1
fi

# 统计变量
TOTAL=0
PASSED=0
FAILED=0

echo "========================================="
echo "  ToyC Compiler Output Verification"
echo "========================================="
echo "Source directory: $SRC_DIR"
echo "Assembly directory: $ASM_DIR"
echo "Compiler: clang --target=$CLANG_TARGET"
echo "QEMU command: $QEMU_CMD"
echo ""

# 遍历所有测试文件
for c_file in "$SRC_DIR"/*.c; do
  # 跳过不存在的情况
  if [[ ! -f "$c_file" ]]; then
    echo "No .c files found in $SRC_DIR"
    exit 0
  fi
  
  base="$(basename "$c_file" .c)"
  toyc_asm="$ASM_DIR/${base}_toyc.s"
  clang_asm="$ASM_DIR/${base}_clang.s"
  
  # 检查汇编文件是否存在
  if [[ ! -f "$toyc_asm" ]]; then
    echo "⚠ Skipping $base: ToyC assembly not found"
    continue
  fi
  
  if [[ ! -f "$clang_asm" ]]; then
    echo "⚠ Skipping $base: Clang assembly not found"
    continue
  fi
  
  TOTAL=$((TOTAL + 1))
  echo "─────────────────────────────────────────"
  echo "Testing: $base"
  
  # 构建链接参数（如果有 libgcc 则添加）
  LINK_LIBS=""
  if [[ -n "$LIBGCC_PATH" ]]; then
    LINK_LIBS="$LIBGCC_PATH"
  fi
  
  # 编译 ToyC 生成的汇编（使用 Clang + -nostdlib + 自定义启动代码）
  toyc_exe="$TEMP_DIR/${base}_toyc"
  if ! clang --target=$CLANG_TARGET -march=$CLANG_ARCH -mabi=$CLANG_ABI \
       -nostdlib "$CRT0_OBJ" "$toyc_asm" $LINK_LIBS -o "$toyc_exe" 2>/dev/null; then
    echo "  ❌ ToyC assembly compilation failed"
    FAILED=$((FAILED + 1))
    continue
  fi
  
  # 编译 Clang 生成的汇编（使用 Clang + -nostdlib + 自定义启动代码）
  # Clang 自己生成的汇编可以直接被 Clang 汇编器处理，无需过滤
  clang_exe="$TEMP_DIR/${base}_clang"
  if ! clang --target=$CLANG_TARGET -march=$CLANG_ARCH -mabi=$CLANG_ABI \
       -nostdlib "$CRT0_OBJ" "$clang_asm" $LINK_LIBS -o "$clang_exe" 2>/dev/null; then
    echo "  ❌ Clang assembly compilation failed"
    FAILED=$((FAILED + 1))
    continue
  fi
  
  # 运行 ToyC 生成的可执行文件
  toyc_output=""
  toyc_exitcode=0
  if toyc_output=$($QEMU_CMD "$toyc_exe" 2>&1); then
    toyc_exitcode=$?
  else
    toyc_exitcode=$?
  fi
  
  # 运行 Clang 生成的可执行文件
  clang_output=""
  clang_exitcode=0
  if clang_output=$($QEMU_CMD "$clang_exe" 2>&1); then
    clang_exitcode=$?
  else
    clang_exitcode=$?
  fi
  
  # 对比结果
  echo "  Clang  exit code: $clang_exitcode"
  echo "  ToyC   exit code: $toyc_exitcode"
  
  if [[ -n "$clang_output" ]]; then
    echo "  Clang  output: $clang_output"
  fi
  if [[ -n "$toyc_output" ]]; then
    echo "  ToyC   output: $toyc_output"
  fi
  
  # 判断是否一致
  if [[ "$toyc_exitcode" -eq "$clang_exitcode" ]] && [[ "$toyc_output" == "$clang_output" ]]; then
    echo "  ✅ Result: CORRECT"
    PASSED=$((PASSED + 1))
  else
    echo "  ❌ Result: INCORRECT"
    if [[ "$toyc_exitcode" -ne "$clang_exitcode" ]]; then
      echo "     Exit code mismatch: expected $clang_exitcode, got $toyc_exitcode"
    fi
    if [[ "$toyc_output" != "$clang_output" ]]; then
      echo "     Output mismatch"
      echo "     Expected: '$clang_output'"
      echo "     Got:      '$toyc_output'"
    fi
    FAILED=$((FAILED + 1))
  fi
done

echo "========================================="
echo "  Verification Summary"
echo "========================================="
echo "Total tests:  $TOTAL"
echo "Passed:       $PASSED ✅"
echo "Failed:       $FAILED ❌"
echo ""

if [[ $FAILED -eq 0 ]] && [[ $TOTAL -gt 0 ]]; then
  echo "🎉 All tests passed!"
  exit 0
elif [[ $TOTAL -eq 0 ]]; then
  echo "⚠ No tests were run"
  exit 1
else
  echo "⚠ Some tests failed"
  exit 1
fi
