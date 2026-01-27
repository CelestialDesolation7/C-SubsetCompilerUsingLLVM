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

verify: test
	@echo ""
	@echo "Running output verification..."
	@if [ -f "scripts/verify_output.sh" ]; then \
		bash scripts/verify_output.sh "$(TEST_SRC_DIR)"; \
	else \
		echo "Error: scripts/verify_output.sh not found"; \
		exit 1; \
	fi
