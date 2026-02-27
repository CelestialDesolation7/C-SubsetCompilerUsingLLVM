# ─── CMake 包装器 Makefile ──────────────────────────────────────
#
# 常用指令:
#   make              编译项目（Windows MinGW / WSL 均可）
#   make test         运行 36 个内置单元测试
#   make generate-asm 批量生成 ToyC + Clang 汇编（需 WSL 中的 clang）
#   make generate-ir  批量生成 ToyC + Clang LLVM IR
#   make verify       端到端验证：生成汇编 → Clang 链接 → QEMU 运行 → 对比结果
#   make debug FILE=01_minimal.c   单文件调试：输出 AST / IR / ASM 及验证
#   make clean        清理构建与测试产物
#
# 前置条件:
#   - CMake ≥ 3.16,  g++ ≥ 13 (C++20)
#   - WSL 中安装 clang (RISC-V 后端), qemu-user
#     sudo apt install clang qemu-user
# ─────────────────────────────────────────────────────────────────

BUILD_DIR := build
SRC_DIR   := examples/compiler_inputs

.PHONY: all build test generate-asm generate-ir verify debug clean rebuild help

all: build

# ---------- 编译 ----------
build:
	@cmake -S . -B $(BUILD_DIR) -G Ninja 2>/dev/null \
	 || cmake -S . -B $(BUILD_DIR) -G "MinGW Makefiles" 2>/dev/null \
	 || cmake -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR) --parallel

# ---------- 内置测试（不需要 WSL） ----------
test: build
	@./$(BUILD_DIR)/toyc_test $(SRC_DIR)

# ---------- 批量汇编生成（在 WSL 中运行） ----------
generate-asm: build
	@bash scripts/generate_asm.sh $(SRC_DIR)

# ---------- 批量 IR 生成（在 WSL 中运行） ----------
generate-ir: build
	@bash scripts/generate_ir.sh $(SRC_DIR)

# ---------- 端到端验证（在 WSL 中运行，需 clang + qemu） ----------
verify: generate-asm
	@bash scripts/verify_output.sh $(SRC_DIR)

# ---------- 单文件调试（在 WSL 中运行） ----------
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
	@rm -rf $(BUILD_DIR) test/

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
	@echo "  make generate-asm Generate ToyC + Clang assembly for all test cases"
	@echo "  make generate-ir  Generate ToyC + Clang LLVM IR for all test cases"
	@echo "  make verify       Full end-to-end verification (asm → link → QEMU → diff)"
	@echo "  make debug FILE=xx.c  Single-file debug (AST / IR / ASM + verify)"
	@echo "  make clean        Remove build/ and test/ directories"
	@echo "  make rebuild      Clean then build"
	@echo "  make help         This message"
