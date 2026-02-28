cmake_minimum_required(VERSION 3.16)
project(toyc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 头文件目录
include_directories(${CMAKE_SOURCE_DIR}/src/include)

# 核心库源文件（不含 main）
set(LIB_SOURCES
    src/lexer.cpp
    src/ast.cpp
    src/parser.cpp
    src/ir.cpp
    src/ir_builder.cpp
    src/ir_parser.cpp
    src/reg_alloc.cpp
    src/riscv_codegen.cpp
)

# 静态库
add_library(toyc_lib STATIC ${LIB_SOURCES})

# 主可执行文件
add_executable(toyc src/main.cpp)
target_link_libraries(toyc PRIVATE toyc_lib)

# 测试可执行文件
add_executable(toyc_test src/unified_test.cpp)
target_link_libraries(toyc_test PRIVATE toyc_lib)

# 寄存器分配调试工具
add_executable(ra_debug src/ra_debug.cpp)
target_link_libraries(ra_debug PRIVATE toyc_lib)

# 安装
install(TARGETS toyc DESTINATION bin)

# 自定义目标：运行内置单元测试
add_custom_target(test_all
    COMMAND toyc_test ${CMAKE_SOURCE_DIR}/examples/compiler_inputs
    DEPENDS toyc_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running 36 unified tests..."
)

# 自定义目标：批量生成汇编（需在 WSL / Linux 运行）
add_custom_target(generate_asm
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/generate_asm.sh
            ${CMAKE_SOURCE_DIR}/examples/compiler_inputs
    DEPENDS toyc
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating ToyC + Clang assembly..."
)

# 自定义目标：端到端验证（需在 WSL / Linux 运行，依赖 clang + qemu）
add_custom_target(verify
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/verify_output.sh
            ${CMAKE_SOURCE_DIR}/examples/compiler_inputs
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Verifying outputs via QEMU..."
)
