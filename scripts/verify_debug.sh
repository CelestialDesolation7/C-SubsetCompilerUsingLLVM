#!/usr/bin/env bash

# Debug模式的结果验证脚本：输出编译的所有中间产物（AST, IR, Assembly）
# 用于调试和学习编译器各阶段的输出

set -eu
if [[ -n "${BASH_VERSION:-}" ]]; then
    set -o pipefail
fi

# 参数检查
if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <source_dir> <file_name>"
  echo "Example: $0 examples/compiler_inputs 01_minimal.c"
  exit 1
fi

SRC_DIR="$1"
FILE_NAME="$2"

# 智能处理文件路径：
# 如果 FILE_NAME 包含路径分隔符，说明是相对路径，直接使用
# 否则，与 SRC_DIR 拼接
if [[ "$FILE_NAME" == *"/"* ]] || [[ "$FILE_NAME" == *"\\"* ]]; then
  c_file="$FILE_NAME"
else
  c_file="$SRC_DIR/$FILE_NAME"
fi

# 检查源文件是否存在
if [[ ! -f "$c_file" ]]; then
  echo "Error: Source file '$c_file' does not exist"
  exit 1
fi

# 检查编译器是否存在
if [[ ! -f "./toyc" ]]; then
  echo "Error: ToyC compiler './toyc' not found"
  echo "Please run 'make build' first"
  exit 1
fi

base="$(basename "$c_file" .c)"
DEBUG_DIR="test/debug"

# 清理 debug 目录中该文件的旧输出
echo "Cleaning previous debug outputs for ${base}..."
rm -f "$DEBUG_DIR/${base}"_* 2>/dev/null || true

mkdir -p "$DEBUG_DIR"

echo "========================================="
echo "  ToyC Compiler Debug Mode"
echo "========================================="
echo "Source file: $c_file"
echo "Output directory: $DEBUG_DIR"
echo ""

# 输出文件路径 - ToyC
AST_OUT="$DEBUG_DIR/${base}_toyc_ast.txt"
IR_OUT="$DEBUG_DIR/${base}_toyc_ir.ll"
ASM_OUT="$DEBUG_DIR/${base}_toyc_asm.s"

# 输出文件路径 - Clang
CLANG_IR_OUT="$DEBUG_DIR/${base}_clang_ir.ll"
CLANG_ASM_OUT="$DEBUG_DIR/${base}_clang_asm.s"

# Step 1: 生成 AST
echo "─────────────────────────────────────────"
echo "Step 1: Generating Abstract Syntax Tree..."
echo ""
if ./toyc "$c_file" --mode ast --output "$AST_OUT" 2>/dev/null; then
  echo "✅ AST generated successfully"
  echo "   Saved to: $AST_OUT"
  echo ""
  echo "AST Output:"
  cat "$AST_OUT"
  echo ""
else
  echo "❌ AST generation failed"
  exit 1
fi

# Step 2: 生成 LLVM IR
echo "─────────────────────────────────────────"
echo "Step 2: Generating LLVM IR..."
echo ""
if ./toyc "$c_file" --mode ir --output "$IR_OUT" 2>/dev/null; then
  echo "✅ LLVM IR generated successfully"
  echo "   Saved to: $IR_OUT"
  echo ""
  echo "LLVM IR Output:"
  cat "$IR_OUT"
  echo ""
else
  echo "❌ LLVM IR generation failed"
  exit 1
fi

# Step 3: 生成 RISC-V Assembly (从 IR)
echo "─────────────────────────────────────────"
echo "Step 3: Generating RISC-V Assembly from IR..."
echo ""
if ./toyc "$IR_OUT" --mode asm --output "$ASM_OUT" 2>/dev/null; then
  echo "✅ Assembly generated successfully"
  echo "   Saved to: $ASM_OUT"
  echo ""
  echo "Assembly Output:"
  cat "$ASM_OUT"
  echo ""
else
  echo "❌ Assembly generation failed"
  exit 1
fi

echo "========================================="
echo "  Debug Output Summary"
echo "========================================="

# Step 4: 生成 Clang 输出用于对比
echo "─────────────────────────────────────────"
echo "Step 4: Generating Clang outputs for comparison..."
echo ""

# 生成 Clang IR
if clang -S -emit-llvm -O0 "$c_file" -o "$CLANG_IR_OUT" 2>/dev/null; then
  echo "✅ Clang IR generated successfully"
  echo "   Saved to: $CLANG_IR_OUT"
else
  echo "⚠ Clang IR generation failed"
fi

# 生成 Clang 汇编
if clang --target=riscv32 -march=rv32im -mabi=ilp32 -S "$c_file" -o "$CLANG_ASM_OUT" 2>/dev/null; then
  echo "✅ Clang assembly generated successfully"
  echo "   Saved to: $CLANG_ASM_OUT"
else
  echo "⚠ Clang assembly generation failed"
fi

echo ""
echo "========================================="
echo "  Debug Files Generated"
echo "========================================="
echo "ToyC Outputs:"
echo "  AST:       $AST_OUT"
echo "  LLVM IR:   $IR_OUT"
echo "  Assembly:  $ASM_OUT"
echo ""
echo "Clang Outputs (for comparison):"
echo "  LLVM IR:   $CLANG_IR_OUT"
echo "  Assembly:  $CLANG_ASM_OUT"
echo ""

# 为验证准备汇编文件
echo "─────────────────────────────────────────"
echo "Step 5: Preparing for verification..."
ASM_DIR="test/asm"
IR_DIR="test/ir"
mkdir -p "$ASM_DIR" "$IR_DIR"

# 复制生成的文件到测试目录
echo "Copying outputs to test directories..."
if [[ -f "$ASM_OUT" ]] && [[ -s "$ASM_OUT" ]]; then
  cp "$ASM_OUT" "$ASM_DIR/${base}_toyc.s"
  echo "✅ ToyC assembly: $ASM_DIR/${base}_toyc.s"
else
  echo "⚠ ToyC assembly not available"
fi

if [[ -f "$IR_OUT" ]] && [[ -s "$IR_OUT" ]]; then
  cp "$IR_OUT" "$IR_DIR/${base}_toyc.ll"
  echo "✅ ToyC IR: $IR_DIR/${base}_toyc.ll"
else
  echo "⚠ ToyC IR not available"
fi

# 复制 Clang 输出到测试目录
if [[ -f "$CLANG_ASM_OUT" ]] && [[ -s "$CLANG_ASM_OUT" ]]; then
  cp "$CLANG_ASM_OUT" "$ASM_DIR/${base}_clang.s"
  echo "✅ Clang assembly: $ASM_DIR/${base}_clang.s"
else
  echo "⚠ Clang assembly not available"
fi

if [[ -f "$CLANG_IR_OUT" ]] && [[ -s "$CLANG_IR_OUT" ]]; then
  cp "$CLANG_IR_OUT" "$IR_DIR/${base}_clang.ll"
  echo "✅ Clang IR: $IR_DIR/${base}_clang.ll"
else
  echo "⚠ Clang IR not available"
fi

echo ""
echo "─────────────────────────────────────────"
echo "Step 6: Running verification..."
echo ""

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERIFY_SCRIPT="$SCRIPT_DIR/verify_output.sh"

# 调用verify_output.sh进行验证
if [[ -f "$VERIFY_SCRIPT" ]]; then
  # 从c_file路径提取目录和文件名
  if [[ "$FILE_NAME" == *"/"* ]] || [[ "$FILE_NAME" == *"\\"* ]]; then
    # 如果FILE_NAME包含路径，使用整个路径
    bash "$VERIFY_SCRIPT" "$SRC_DIR" "$FILE_NAME"
  else
    # 否则使用basename
    bash "$VERIFY_SCRIPT" "$SRC_DIR" "$base.c"
  fi
else
  echo "⚠ Verification script not found: $VERIFY_SCRIPT"
  echo "✅ Debug outputs generated successfully!"
fi
