# ToyC 编译器 Makefile
SHELL := /bin/bash

CXX := g++
CXXFLAGS := -std=c++20 -O2 -Wall
SRC_DIR := src
BUILD_DIR := build
TARGET := toyc
UNIFIED_TEST_TARGET := unified_test

# 主编译器源文件（排除unified_test.cpp）
MAIN_SRCS := $(filter-out $(SRC_DIR)/unified_test.cpp, $(wildcard $(SRC_DIR)/*.cpp))
MAIN_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(MAIN_SRCS))

# unified_test 需要的源文件
UNIFIED_TEST_SRCS := $(SRC_DIR)/unified_test.cpp $(SRC_DIR)/ra_linear_scan.cpp
UNIFIED_TEST_OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(UNIFIED_TEST_SRCS))

.PHONY: all clean test build unified_test verify

all: build

build: $(TARGET)

$(TARGET): $(MAIN_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

unified_test: $(UNIFIED_TEST_TARGET)

$(UNIFIED_TEST_TARGET): $(UNIFIED_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(UNIFIED_TEST_TARGET)
	rm -rf test

# 默认测试目录
TEST_SRC_DIR ?= examples/compiler_inputs

test: build
	@echo "Running tests on: $(TEST_SRC_DIR)"
	@if [ -f "scripts/generate_asm.sh" ]; then \
		bash scripts/generate_asm.sh "$(TEST_SRC_DIR)"; \
	else \
		echo "Error: scripts/generate_asm.sh not found"; \
		exit 1; \
	fi
	@if [ -f "scripts/generate_ir.sh" ]; then \
		bash scripts/generate_ir.sh "$(TEST_SRC_DIR)"; \
	else \
		echo "Error: scripts/generate_ir.sh not found"; \
		exit 1; \
	fi
	@echo "Tests completed successfully!"
	@echo "Output: test/asm/ and test/ir/"

verify: build
	@echo ""
	@if [ -z "$(FILE)" ]; then \
		echo "Running full test suite..."; \
		if [ -f "scripts/generate_asm.sh" ]; then \
			bash scripts/generate_asm.sh "$(TEST_SRC_DIR)"; \
		fi; \
		if [ -f "scripts/generate_ir.sh" ]; then \
			bash scripts/generate_ir.sh "$(TEST_SRC_DIR)"; \
		fi; \
	else \
		echo "Compiling single file: $(FILE)"; \
		mkdir -p test/asm test/ir; \
		FILE_PATH="$(TEST_SRC_DIR)/$(FILE)"; \
		if echo "$(FILE)" | grep -q "/"; then \
			FILE_PATH="$(FILE)"; \
		fi; \
		BASE_NAME=$$(basename "$$FILE_PATH" .c); \
		./toyc "$$FILE_PATH" --mode asm --output "test/asm/$${BASE_NAME}_toyc.s"; \
		clang --target=riscv32 -march=rv32im -mabi=ilp32 -S "$$FILE_PATH" -o "test/asm/$${BASE_NAME}_clang.s"; \
		./toyc "$$FILE_PATH" --mode ir --output "test/ir/$${BASE_NAME}_toyc.ll"; \
		clang -S -emit-llvm -O0 "$$FILE_PATH" -o "test/ir/$${BASE_NAME}_clang.ll"; \
	fi
	@echo ""
	@echo "Running output verification..."
	@if [ -f "scripts/verify_output.sh" ]; then \
		if [ -n "$(FILE)" ]; then \
			bash scripts/verify_output.sh "$(TEST_SRC_DIR)" "$(FILE)"; \
		else \
			bash scripts/verify_output.sh "$(TEST_SRC_DIR)"; \
		fi \
	else \
		echo "Error: scripts/verify_output.sh not found"; \
		exit 1; \
	fi

verify-debug: build
	@echo ""
	@echo "Running output verification in DEBUG mode..."
	@if [ -f "scripts/verify_debug.sh" ]; then \
		if [ -z "$(FILE)" ]; then \
			echo "Error: FILE parameter is required for verify-debug"; \
			echo "Usage: make verify-debug FILE=<filename.c>"; \
			exit 1; \
		fi; \
		bash scripts/verify_debug.sh "$(TEST_SRC_DIR)" "$(FILE)"; \
	else \
		echo "Error: scripts/verify_debug.sh not found"; \
		exit 1; \
	fi
