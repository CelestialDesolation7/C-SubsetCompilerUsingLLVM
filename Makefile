# ─── CMake 包装器 Makefile ──────────────────────────────────────
#
# 使用方式（从 Windows PowerShell 通过 WSL 调用）:
#   wsl make              编译项目
#   wsl make test         运行 36 个内置单元测试
#   wsl make generate-asm 批量生成 ToyC + Clang 汇编
#   wsl make generate-ir  批量生成 ToyC + Clang LLVM IR
#   wsl make verify       端到端验证：生成汇编 → 链接 → QEMU 运行 → 对比结果
#   wsl make debug FILE=01_minimal.c   单文件调试
#   wsl make clean        清理构建与测试产物
#
# 前置条件:
#   - WSL (Ubuntu) + CMake ≥ 3.16 + g++ ≥ 13 (C++20)
#   - clang (RISC-V 后端) + qemu-user
#     sudo apt install build-essential cmake clang qemu-user
# ─────────────────────────────────────────────────────────────────

BUILD_DIR := build
SRC_DIR   := examples/compiler_inputs

.PHONY: all build test generate-asm generate-ir verify debug clean rebuild help

all: build

# ---------- 编译 ----------
build:
	@cmake -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR) --parallel

# ---------- 内置测试 ----------
test: build
	@bash scripts/generate_asm.sh $(SRC_DIR) > /dev/null
	@bash scripts/generate_ir.sh $(SRC_DIR) > /dev/null
	@./$(BUILD_DIR)/toyc_test $(SRC_DIR)

# ---------- 批量汇编生成 ----------
generate-asm: build
	@bash scripts/generate_asm.sh $(SRC_DIR) > /dev/null

# ---------- 批量 IR 生成 ----------
generate-ir: build
	@bash scripts/generate_ir.sh $(SRC_DIR) > /dev/null

# ---------- 端到端验证（汇编生成静默，仅显示验证结果） ----------
verify: build
	@bash scripts/generate_asm.sh $(SRC_DIR) > /dev/null
	@bash scripts/generate_ir.sh $(SRC_DIR) > /dev/null
	@bash scripts/verify_output.sh $(SRC_DIR)

# ---------- 单文件调试 ----------
# 用法: make debug FILE=01_minimal.c
debug: build
	@if [ -z "$(FILE)" ]; then \
	    echo "Usage: make debug FILE=<filename.c>"; \
	    echo "Example: make debug FILE=01_minimal.c"; \
	    exit 1; \
	fi
	@bash scripts/verify_debug.sh $(SRC_DIR) $(FILE)

# ---------- 清理 ----------
clean:
	@rm -rf $(BUILD_DIR) build-linux test/

rebuild: clean build

# ---------- 便捷: 单文件编译 ----------
%.s: $(SRC_DIR)/%.c build
	@./$(BUILD_DIR)/toyc $< --asm

%.ll: $(SRC_DIR)/%.c build
	@./$(BUILD_DIR)/toyc $< --ir

# ---------- 帮助 ----------
help:
	@echo "ToyC Compiler - Makefile Targets"
	@echo "================================"
	@echo "  make              Build the project"
	@echo "  make test         Run 36 built-in unit tests"
	@echo "  make generate-asm Generate ToyC + Clang assembly"
	@echo "  make generate-ir  Generate ToyC + Clang LLVM IR"
	@echo "  make verify       End-to-end verify (asm → link → QEMU → diff)"
	@echo "  make debug FILE=xx.c  Single-file debug (AST/IR/ASM + verify)"
	@echo "  make clean        Remove build/ and test/"
	@echo "  make rebuild      Clean then build"
	@echo "  make help         This message"
