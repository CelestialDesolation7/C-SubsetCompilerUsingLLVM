#!/usr/bin/env bash

# 结果验证脚本：编译运行 ToyC 和 Clang 生成的汇编，对比输出结果

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

# 检查 RISC-V 工具链（支持 32 位和 64 位）
RISCV_GCC=""
RISCV_ARCH=""
RISCV_ABI=""

if command -v riscv32-unknown-elf-gcc >/dev/null 2>&1; then
  RISCV_GCC="riscv32-unknown-elf-gcc"
  RISCV_ARCH="rv32im"
  RISCV_ABI="ilp32"
elif command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  RISCV_GCC="riscv64-unknown-elf-gcc"
  RISCV_ARCH="rv32im"
  RISCV_ABI="ilp32"
  echo "Note: Using riscv64-unknown-elf-gcc for RV32 compilation"
else
  echo "Error: RISC-V GCC not found"
  echo "Please install RISC-V toolchain to verify outputs"
  echo ""
  echo "Installation guide:"
  echo "  Ubuntu/Debian: sudo apt install gcc-riscv64-unknown-elf"
  echo "  Or build from: https://github.com/riscv/riscv-gnu-toolchain"
  exit 1
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
if ! $RISCV_GCC -march=$RISCV_ARCH -mabi=$RISCV_ABI -c "$CRT0_FILE" -o "$CRT0_OBJ" 2>/dev/null; then
  echo "Error: Failed to compile startup file"
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
echo "RISC-V GCC: $RISCV_GCC"
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
  
  # 编译 ToyC 生成的汇编（使用 -nostdlib 和自定义启动代码，添加 -lgcc 支持软件除法）
  toyc_exe="$TEMP_DIR/${base}_toyc"
  if ! $RISCV_GCC -march=$RISCV_ARCH -mabi=$RISCV_ABI -nostdlib \
       "$CRT0_OBJ" "$toyc_asm" -o "$toyc_exe" -lgcc 2>/dev/null; then
    echo "  ❌ ToyC assembly compilation failed"
    FAILED=$((FAILED + 1))
    continue
  fi
  
  # 处理 Clang 汇编（移除 GNU 汇编器不支持的伪指令）
  clang_asm_filtered="$TEMP_DIR/${base}_clang_filtered.s"
  grep -v '\.addrsig\|\.ident\|\.attribute' "$clang_asm" > "$clang_asm_filtered"
  
  # 编译 Clang 生成的汇编（使用 -nostdlib 和自定义启动代码，添加 -lgcc 支持软件除法）
  clang_exe="$TEMP_DIR/${base}_clang"
  if ! $RISCV_GCC -march=$RISCV_ARCH -mabi=$RISCV_ABI -nostdlib \
       "$CRT0_OBJ" "$clang_asm_filtered" -o "$clang_exe" -lgcc 2>/dev/null; then
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
