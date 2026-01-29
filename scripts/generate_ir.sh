#!/usr/bin/env bash

# Use pipefail if available, but don't fail if it's not supported
set -eu
if [[ -n "${BASH_VERSION:-}" ]]; then
    set -o pipefail
fi

# 支持通过参数指定源目录，默认为 examples/compiler_inputs
SRC_DIR="${1:-examples/compiler_inputs}"
OUT_DIR="test/ir"

# 检查源目录是否存在
if [[ ! -d "$SRC_DIR" ]]; then
  echo "Error: Source directory '$SRC_DIR' does not exist"
  exit 1
fi

# 1. 创建输出目录
mkdir -p "$OUT_DIR"

# 2. 对每个 .c 文件执行 toyc --mode ir
for c in "$SRC_DIR"/*.c; do
  # Skip if no .c files found
  if [[ ! -f "$c" ]]; then
    echo "No .c files found in $SRC_DIR"
    exit 0
  fi
  
  base="$(basename "$c" .c)"
  echo "─── Processing $base ───"

  # 2.1 ToyC 生成 IR（.ll）
  if [[ -f "./toyc" ]]; then
    if ./toyc "$c" --mode ir --output "$OUT_DIR/${base}_toyc.ll" 2>/dev/null; then
      echo "ToyC IR →  $OUT_DIR/${base}_toyc.ll"
    else
      echo "ToyC IR → FAILED (compilation error)"
    fi
  elif [[ -f "./toyc.exe" ]]; then
    if ./toyc.exe "$c" --mode ir --output "$OUT_DIR/${base}_toyc.ll" 2>/dev/null; then
      echo "ToyC IR →  $OUT_DIR/${base}_toyc.ll"
    else
      echo "ToyC IR → FAILED (compilation error)"
    fi
  else
    echo "ToyC IR → skipped (toyc not found)"
  fi

  # 2.2 Clang 为同名 .c 生成 LLVM IR（.ll）
  cfile="$SRC_DIR/$base.c"
  if [[ -f "$cfile" ]] && command -v clang >/dev/null 2>&1; then
    clang -S -emit-llvm -O0 "$cfile" \
      -o "$OUT_DIR/${base}_clang.ll"
    echo "Clang IR → $OUT_DIR/${base}_clang.ll"
  else
    echo "Clang IR → skipped (no $base.c or clang not installed)"
  fi

  echo
done

echo "All IR files written into $OUT_DIR"
