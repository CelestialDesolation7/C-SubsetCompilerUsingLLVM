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
mkdir -p "$DEBUG_DIR"

echo "========================================="
echo "  ToyC Compiler Debug Mode"
echo "========================================="
echo "Source file: $c_file"
echo "Output directory: $DEBUG_DIR"
echo ""

# 输出文件路径
AST_OUT="$DEBUG_DIR/${base}_ast.txt"
IR_OUT="$DEBUG_DIR/${base}_ir.ll"
ASM_OUT="$DEBUG_DIR/${base}_asm.s"
ALL_OUT="$DEBUG_DIR/${base}_all.txt"

# 使用 --mode all 生成所有中间产物
echo "─────────────────────────────────────────"
echo "Step 1: Generating all intermediate outputs..."
if ./toyc "$c_file" --mode all --output "$ALL_OUT"; then
  echo "✅ All outputs generated successfully"
  echo "   Output file: $ALL_OUT"
else
  echo "❌ Compilation failed"
  exit 1
fi

echo ""
echo "─────────────────────────────────────────"
echo "Step 2: Extracting individual stages..."

# 分离AST、IR和Assembly到单独的文件
awk '
  BEGIN { stage="none" }
  /^=== Abstract Syntax Tree ===$/ { stage="ast"; next }
  /^; ModuleID =/ { stage="ir" }
  /^        \.text$/ || /^        \.globl/ { stage="asm" }
  stage == "ast" && /^$/ && NR > 2 { next }
  stage == "ast" { print > "'"$AST_OUT"'" }
  stage == "ir" { print > "'"$IR_OUT"'" }
  stage == "asm" { print > "'"$ASM_OUT"'" }
' "$ALL_OUT"

# 检查各个文件是否生成成功
if [[ -f "$AST_OUT" ]]; then
  echo "✅ AST extracted: $AST_OUT"
else
  echo "⚠ AST not found (might be empty or not generated)"
fi

if [[ -f "$IR_OUT" ]] && [[ -s "$IR_OUT" ]]; then
  echo "✅ LLVM IR extracted: $IR_OUT"
else
  echo "⚠ LLVM IR not found or empty"
fi

if [[ -f "$ASM_OUT" ]] && [[ -s "$ASM_OUT" ]]; then
  echo "✅ Assembly extracted: $ASM_OUT"
else
  echo "⚠ Assembly not found or empty"
fi

echo ""
echo "─────────────────────────────────────────"
echo "Step 3: Displaying outputs..."
echo ""

# 显示完整输出
cat "$ALL_OUT"

echo ""
echo "========================================="
echo "  Debug Output Summary"
echo "========================================="
echo "All outputs combined: $ALL_OUT"
if [[ -f "$AST_OUT" ]]; then
  echo "AST only:            $AST_OUT"
fi
if [[ -f "$IR_OUT" ]] && [[ -s "$IR_OUT" ]]; then
  echo "LLVM IR only:        $IR_OUT"
fi
if [[ -f "$ASM_OUT" ]] && [[ -s "$ASM_OUT" ]]; then
  echo "Assembly only:       $ASM_OUT"
fi
echo ""

# 为验证准备汇编文件
echo "─────────────────────────────────────────"
echo "Step 4: Preparing for verification..."
ASM_DIR="test/asm"
IR_DIR="test/ir"
mkdir -p "$ASM_DIR" "$IR_DIR"

# 直接使用编译器生成汇编和IR文件到测试目录
echo "Generating ToyC outputs..."
if ./toyc "$c_file" --mode asm --output "$ASM_DIR/${base}_toyc.s" 2>/dev/null; then
  echo "✅ ToyC assembly: $ASM_DIR/${base}_toyc.s"
else
  echo "⚠ ToyC assembly generation failed"
fi

if ./toyc "$c_file" --mode ir --output "$IR_DIR/${base}_toyc.ll" 2>/dev/null; then
  echo "✅ ToyC IR: $IR_DIR/${base}_toyc.ll"
else
  echo "⚠ ToyC IR generation failed"
fi

# 使用Clang生成对比汇编和IR
echo "Generating Clang outputs for comparison..."
if clang --target=riscv32 -march=rv32im -mabi=ilp32 -S "$c_file" -o "$ASM_DIR/${base}_clang.s" 2>/dev/null; then
  echo "✅ Clang assembly: $ASM_DIR/${base}_clang.s"
else
  echo "⚠ Clang assembly generation failed"
fi

if clang -S -emit-llvm -O0 "$c_file" -o "$IR_DIR/${base}_clang.ll" 2>/dev/null; then
  echo "✅ Clang IR: $IR_DIR/${base}_clang.ll"
else
  echo "⚠ Clang IR generation failed"
fi

echo ""
echo "─────────────────────────────────────────"
echo "Step 5: Running verification..."
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
