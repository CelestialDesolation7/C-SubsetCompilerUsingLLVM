#!/usr/bin/env bash
# setup_spike_rv32.sh — 在 macOS 上构建 rv32im 版本的 RISC-V proxy kernel (pk)
#
# spike (RISC-V ISA 模拟器) 需要 pk 来处理系统调用。
# Homebrew 安装的 pk 仅为 rv64 版本，需额外编译 rv32 版本。
#
# 前置条件:
#   brew tap riscv-software-src/riscv
#   brew install riscv-isa-sim riscv-gnu-toolchain
#
# 用法:  bash scripts/setup_spike_rv32.sh
# 产物:  build/pk_rv32  (rv32im proxy kernel)

set -eu
[[ -n "${BASH_VERSION:-}" ]] && set -o pipefail

# ========== 检查前置工具 ==========
echo "Checking prerequisites..."

if ! command -v spike >/dev/null 2>&1; then
  echo "Error: spike not found."
  echo "Install with: brew tap riscv-software-src/riscv && brew install riscv-isa-sim"
  exit 1
fi

if ! command -v riscv64-unknown-elf-gcc >/dev/null 2>&1; then
  echo "Error: riscv64-unknown-elf-gcc not found."
  echo "Install with: brew tap riscv-software-src/riscv && brew install riscv-gnu-toolchain"
  exit 1
fi

# ========== 配置路径 ==========
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
PK_OUTPUT="$BUILD_DIR/pk_rv32"

# 如果已存在，跳过构建
if [[ -f "$PK_OUTPUT" ]]; then
  echo "rv32 pk already exists at: $PK_OUTPUT"
  file "$PK_OUTPUT"
  echo "To rebuild, delete it first: rm $PK_OUTPUT"
  exit 0
fi

mkdir -p "$BUILD_DIR"

# ========== 下载 riscv-pk 源码 ==========
TEMP_DIR=$(mktemp -d)
trap "rm -rf '$TEMP_DIR'" EXIT

echo "Cloning riscv-pk source..."
git clone --depth=1 https://github.com/riscv-software-src/riscv-pk.git "$TEMP_DIR/riscv-pk" 2>&1 | grep -v "^$"

# ========== 构建 rv32im pk ==========
echo "Building rv32im proxy kernel..."
PK_BUILD="$TEMP_DIR/riscv-pk/build-rv32"
mkdir -p "$PK_BUILD"
cd "$PK_BUILD"

../configure \
  --host=riscv64-unknown-elf \
  --with-arch=rv32im_zicsr_zifencei \
  --with-abi=ilp32 \
  > /dev/null 2>&1

make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)" > /dev/null 2>&1

# ========== 安装到项目 build/ ==========
cp "$PK_BUILD/pk" "$PK_OUTPUT"

echo ""
echo "========================================="
echo "  rv32im proxy kernel built successfully!"
echo "========================================="
echo "Output: $PK_OUTPUT"
file "$PK_OUTPUT"
echo ""
echo "You can now run: make verify"
