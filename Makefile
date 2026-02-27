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

SRC_DIR := examples/compiler_inputs

# ── OS 检测：Linux/WSL → build-linux/   Windows/MinGW → build/ ──
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifneq ($(findstring Linux,$(UNAME_S)),)
  BUILD_DIR := build-linux
else
  BUILD_DIR := build
endif

.PHONY: all build test generate-asm generate-ir verify debug clean rebuild help check-toyc

all: build

# ---------- 编译（自动适配平台） ----------
build:
ifneq ($(findstring Linux,$(UNAME_S)),)
	@cmake -S . -B $(BUILD_DIR)
else
	@cmake -S . -B $(BUILD_DIR) -G Ninja 2>/dev/null \
	 || cmake -S . -B $(BUILD_DIR) -G "MinGW Makefiles" 2>/dev/null \
	 || cmake -S . -B $(BUILD_DIR)
endif
	@cmake --build $(BUILD_DIR) --parallel

# ---------- 检查 ToyC 是否已编译（不重新编译） ----------
check-toyc:
	@FOUND=""; \
	for f in build-linux/toyc build/toyc build/toyc.exe ./toyc ./toyc.exe; do \
	    [ -f "$$f" ] && { FOUND="$$f"; break; }; \
	done; \
	if [ -z "$$FOUND" ]; then \
	    echo "Error: ToyC not found (searched build/, build-linux/)"; \
	    echo "Please build first: make"; \
	    exit 1; \
	fi; \
	echo "Found ToyC: $$FOUND"

# ---------- 内置测试 ----------
test: build
	@./$(BUILD_DIR)/toyc_test $(SRC_DIR)

# ---------- 批量汇编生成（WSL 中运行，不重新编译） ----------
generate-asm: check-toyc
	@bash scripts/generate_asm.sh $(SRC_DIR)

# ---------- 批量 IR 生成（WSL 中运行，不重新编译） ----------
generate-ir: check-toyc
	@bash scripts/generate_ir.sh $(SRC_DIR)

# ---------- 端到端验证（在 WSL 中运行，需 clang + qemu） ----------
verify: generate-asm
	@bash scripts/verify_output.sh $(SRC_DIR)

# ---------- 单文件调试（在 WSL 中运行） ----------
# 用法: make debug FILE=01_minimal.c
debug: check-toyc
	@if [ -z "$(FILE)" ]; then \
	    echo "Usage: make debug FILE=<filename.c>"; \
	    echo "Example: make debug FILE=01_minimal.c"; \
	    exit 1; \
	fi
	@bash scripts/verify_debug.sh $(SRC_DIR) $(FILE)

# ---------- 清理 ----------
clean:
	@rm -rf build build-linux test/

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
	@echo "  make              Build (auto-detects Windows/Linux/WSL)"
	@echo "  make test         Run 36 built-in unit tests"
	@echo "  make generate-asm Generate ToyC + Clang assembly (WSL)"
	@echo "  make generate-ir  Generate ToyC + Clang LLVM IR (WSL)"
	@echo "  make verify       Full end-to-end verification (WSL)"
	@echo "  make debug FILE=xx.c  Single-file debug (WSL)"
	@echo "  make clean        Remove build/, build-linux/ and test/"
	@echo "  make rebuild      Clean then build"
	@echo "  make help         This message"
