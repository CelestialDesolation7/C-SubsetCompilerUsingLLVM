#!/usr/bin/env bash

# Use pipefail if available, but don't fail if it's not supported
set -eu
if [[ -n "${BASH_VERSION:-}" ]]; then
    set -o pipefail
fi

SRC_DIR="examples/compiler_inputs"
OUT_DIR="$SRC_DIR/asm"

# 创建输出目录（如果已存在则忽略）
mkdir -p "$OUT_DIR"

# 处理每个 .c 文件
for c in "$SRC_DIR"/*.c; do
  # Skip if no .c files found
  if [[ ! -f "$c" ]]; then
    echo "No .c files found in $SRC_DIR"
    exit 0
  fi
  
  base="$(basename "$c" .c)"
  c_file="$SRC_DIR/$base.c"

  echo ">>> 处理 $base …"

  # 1. ToyC 生成 asm
  if [[ -f "./toyc" ]]; then
    ./toyc "$c" --mode asm --output "$OUT_DIR/${base}_toyc.s"
    echo "  ToyC → asm/${base}_toyc.s"
  elif [[ -f "./toyc.exe" ]]; then
    ./toyc.exe "$c" --mode asm --output "$OUT_DIR/${base}_toyc.s"
    echo "  ToyC → asm/${base}_toyc.s"
  else
    echo "  ToyC → skipped (toyc not found)"
  fi

  # 2. Clang 生成 asm（需要对应的 .c 文件存在）
  if [[ -f "$c_file" ]] && command -v clang >/dev/null 2>&1; then
    clang -S \
      --target=riscv32-unknown-elf \
      -march=rv32i -mabi=ilp32 \
      "$c_file" \
      -o "$OUT_DIR/${base}_clang.s"
    echo "  Clang → asm/${base}_clang.s"
  else
    echo "  跳过 Clang（未找到 $base.c 或 clang 未安装）"
  fi

  # 3. riscv32-unknown-elf-gcc 生成 asm
  if [[ -f "$c_file" ]] && command -v riscv32-unknown-elf-gcc >/dev/null 2>&1; then
    riscv32-unknown-elf-gcc -march=rv32i -mabi=ilp32 -Wall -S \
      "$c_file" \
      -o "$OUT_DIR/${base}_riscv.s"
    echo "  riscv-gcc → asm/${base}_riscv.s"
  else
    echo "  跳过 riscv-gcc（未找到 $base.c 或 riscv32-unknown-elf-gcc 未安装）"
  fi

  echo
done

echo "所有汇编已生成到 $OUT_DIR"
