# ToyC - C 语言子集编译器

## 📖 目录

- [项目简介](#项目简介)
- [核心技术与亮点](#核心技术与亮点)
- [支持的语言特性](#支持的语言特性)
- [快速开始](#快速开始)
- [使用方法](#使用方法)
- [批量测试](#批量测试)
- [输出示例](#输出示例)
- [项目结构](#项目结构)
- [技术文档](#技术文档)

---

## 项目简介

ToyC 是一个基于现代编译技术实现的 **C 语言子集编译器**，采用多阶段编译架构，支持将 C 代码编译为 **LLVM IR** 和 **RISC-V 汇编代码**。

### 编译流程

```
C 源代码 → 词法分析 → 语法分析 → AST → LLVM IR 生成 → 寄存器分配 → RISC-V 汇编
```

### 技术栈

- **语言**: C++20
- **目标架构**: RISC-V 32-bit (RV32I)
- **中间表示**: LLVM IR (SSA 形式)
- **寄存器分配**: 线性扫描算法
  - 原论文：https://web.cs.ucla.edu/~palsberg/course/cs132/linearscan.pdf
  - 关于本项目中实现的解释：[线性扫描算法核心思路.md](docs\线性扫描算法核心思路.md)

---

## 核心技术与亮点

### 1. 完整的编译器前端

#### 词法分析器 (Lexer)
- **手工实现**的高效词法分析器
- 支持单字符和多字符运算符识别
- 完整的关键字、标识符、数字字面量处理
- 支持注释和空白符过滤

#### 语法分析器 (Parser)
- **递归下降**解析算法
- 完整的表达式优先级处理
- 支持复杂控制流语句嵌套
- 作用域管理和符号表维护

#### 抽象语法树 (AST)
- 面向对象的节点设计
- 支持语法树可视化输出
- 类型检查和语义分析

### 2. LLVM IR 代码生成

- **SSA 形式**的中间代码生成
- 支持 phi 节点自动插入
- **控制流图 (CFG)** 构建
- 基本块管理和跳转优化
- 函数调用约定实现
- **短路求值**优化（逻辑运算符 `&&`, `||`）

### 3. 寄存器分配算法

实现了经典的 **线性扫描寄存器分配算法** (Linear Scan Register Allocation)：

#### 核心特性
- **活跃区间计算**: 精确分析变量生命周期
- **物理寄存器映射**: 符合 RISC-V 调用约定
  - `a0-a7`: 参数寄存器
  - `t0-t6`: 临时寄存器  
  - `s0-s11`: 保存寄存器
- **溢出处理**: 自动栈分配和加载/存储生成
- **参数处理**: 支持多参数函数调用（前 8 个通过寄存器，其余通过栈）

#### 算法优势
- **时间复杂度**: O(n log n)，远优于图着色算法
- **空间效率**: 高效利用寄存器，减少内存访问
- **工业级实现**: 被广泛应用于 JVM、V8 等生产环境

详细算法说明见：[docs/线性扫描算法核心思路.md](docs/线性扫描算法核心思路.md)

### 4. RISC-V 代码生成

#### 支持的指令集
- **算术运算**: `add`, `sub`, `mul`, `div`, `rem`
- **逻辑运算**: `and`, `or`, `xor`, `not`
- **比较运算**: `slt`, `seqz`, `snez`
- **控制流**: `beq`, `bne`, `blt`, `bge`, `j`, `jal`, `jalr`, `ret`
- **内存访问**: `lw`, `sw`（字对齐）

#### 调用约定
完全符合 **RISC-V ABI** 规范：
- 参数传递：`a0-a7` (前 8 个参数)
- 返回值：`a0`
- 返回地址：`ra`
- 栈指针：`sp`
- 帧指针：`s0`（可选）

### 5. 多种输出模式

支持灵活的编译输出：
- **AST**: 树形结构可视化
- **LLVM IR**: 标准 LLVM IR 格式
- **Assembly**: RISC-V 汇编代码
- **All**: 同时输出以上所有内容

---

## 支持的语言特性

- **标识符**：终结符 ID 识别符合 C 规范的标识符。其正则表达式为：`[_A-Za-z][_A-Za-z0-9]*`
- **整数**：终结符 NUMBER 识别十进制整数常量。其正则表达式为：`-?(0|[1-9][0-9]*)`
- **空白字符和注释**：
	- 忽略空白字符和注释。空白字符包括空格、制表符、换行符等；
	- 注释的规范与 C 语言一致：
		单行注释以 “//” 开始、到最近的换行符前结束，多行注释以 “/*” 开始、以最近的 “*/” 结束。

![image-20260127205519141](assets/image-20260127205519141.png)

![image-20260130132127243](assets/image-20260130132127243.png)

### 数据类型
- `int` - 32 位有符号整数
- `void` - 无返回值类型

### 运算符
| 类型 | 运算符 |
|------|--------|
| 算术 | `+`, `-`, `*`, `/`, `%` |
| 比较 | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| 逻辑 | `&&`, `||`, `!` |
| 一元 | `+`, `-`, `!` |
| 赋值 | `=` |

### 控制流
- `if` / `if-else` - 条件分支
- `while` - 循环
- `break` - 跳出循环
- `continue` - 继续下次迭代
- `return` - 函数返回

### 高级特性
- ✅ 函数定义和调用
- ✅ 递归函数支持
- ✅ 多参数传递
- ✅ 作用域和变量遮蔽
- ✅ 块级作用域 `{ ... }`
- ✅ 短路求值优化
- ✅ 运算符优先级和结合性

---

## 快速开始

### 环境要求

- **必需**:
  - g++ (支持 C++20)
  - make
  - bash

- **可选** (用于生成对比输出):
  - clang
  - riscv32-unknown-elf-gcc

### 安装依赖 (Ubuntu/WSL)

```bash
sudo apt update
sudo apt install build-essential clang
```

### 编译项目

```bash
# 克隆或下载项目后
cd C-SubsetCompilerUsingLLVM

# 构建编译器
make build

# 或直接运行测试（会自动构建）
make test
```

---

## 使用方法

### 命令行选项

```bash
./toyc <input_file> [options]
```

**选项说明**:
- `--mode <mode>`: 输出模式
  - `ast` - 抽象语法树
  - `ir` - LLVM IR
  - `asm` - RISC-V 汇编 (默认)
  - `all` - 所有输出
- `--output <file>`: 输出到文件（默认为 stdout）
- `--target <arch>`: 目标架构（默认 `riscv32`）

### 示例用法

#### 1. 编译到 RISC-V 汇编

```bash
./toyc examples/compiler_inputs/01_minimal.c --mode asm
```

#### 2. 生成 LLVM IR

```bash
./toyc examples/compiler_inputs/05_function_call.c --mode ir
```

#### 3. 查看 AST

```bash
./toyc examples/compiler_inputs/03_if_else.c --mode ast
```

#### 4. 输出到文件

```bash
./toyc examples/compiler_inputs/09_recursion.c --mode asm --output fib.s
```

#### 5. 从标准输入读取

```bash
cat test.c | ./toyc --mode asm
```

---

## 批量测试

### 特性说明

- **错误处理**: 编译失败的文件会被标记为 FAILED，但不会中断测试流程，其他文件会继续测试
- **智能验证**: 只验证成功编译的文件，失败的文件会被跳过并在统计中显示
- **详细输出**: 显示每个阶段的编译结果、错误信息和最终统计
- **阶段报告**: 编译失败时会报告具体在哪个阶段失败（解析、IR生成、汇编生成等）
- **无空文件**: 编译失败不会创建空的输出文件，保持文件系统整洁

### 编译错误报告

当编译失败时，编译器会报告失败的具体阶段：

```bash
$ ./toyc examples/spec/test_logic.c --mode asm
Error: Compilation failed at stage: generating RISC-V assembly
Details: stoi
```

支持的阶段包括：
- `reading input` - 读取源文件
- `parsing (lexical and syntax analysis)` - 词法和语法分析
- `generating LLVM IR` - LLVM IR 代码生成
- `generating RISC-V assembly` - RISC-V 汇编代码生成
- `writing output` - 写入输出文件

### 默认测试（测试 examples/compiler_inputs）

```bash
make test
```

**输出结构**:
```
test/
├── asm/                       # 汇编代码
│   ├── 01_minimal_toyc.s      # ToyC 生成
│   ├── 01_minimal_clang.s     # Clang 生成（对比）
│   └── ...
└── ir/                        # LLVM IR
    ├── 01_minimal_toyc.ll
    ├── 01_minimal_clang.ll
    └── ...
```

**注意**: 如果某个文件编译失败，会显示 "FAILED (compilation error)"，但测试会继续进行。

### 测试自定义目录

```bash
# 测试指定目录下的所有 .c 文件
make test TEST_SRC_DIR=path/to/your/test/folder

# 例如
make test TEST_SRC_DIR=examples/single_func
make test TEST_SRC_DIR=examples/multi_func
make test TEST_SRC_DIR=examples/spec
```

### 仅生成汇编或 IR

```bash
# 仅汇编
bash scripts/generate_asm.sh [source_dir]

# 仅 IR
bash scripts/generate_ir.sh [source_dir]

# 示例
bash scripts/generate_asm.sh examples/single_func
```

### 验证输出结果

验证功能会编译 ToyC 和 Clang 生成的汇编代码，并在 QEMU 中运行对比结果。

```bash
# 验证默认目录（examples/compiler_inputs）
make verify

# 验证自定义目录
make verify TEST_SRC_DIR=<your_directory>

# 验证单个文件（推荐用于调试特定文件）
make verify FILE=<filename.c>
make verify TEST_SRC_DIR=examples/single_func FILE=test_call.c

# 示例
make verify TEST_SRC_DIR=examples/single_func
make verify TEST_SRC_DIR=examples/multi_func
make verify FILE=01_minimal.c                          # 从默认目录
make verify FILE=examples/spec/test_fact.c             # 带路径的文件名
```

**验证输出示例**:
```
=========================================
  Verification Summary
=========================================
Total files:  10
Passed:       7 ✅
Failed:       1 ❌
Skipped:      2 ⚠

✅ All runnable tests passed (some files skipped due to compilation errors)
```

- **Passed**: 输出结果与 Clang 完全一致
- **Failed**: 退出码或输出不匹配
- **Skipped**: 编译失败，无法验证

### 调试模式验证

调试模式**逐步显示**编译的所有中间产物（AST、LLVM IR、汇编代码），并在最后进行验证：

```bash
# 必须指定文件名
make verify-debug FILE=<filename.c>

# 指定不同目录的文件
make verify-debug TEST_SRC_DIR=examples/single_func FILE=test_call.c

# 示例
make verify-debug FILE=01_minimal.c
make verify-debug FILE=09_recursion.c
make verify-debug FILE=examples/spec/test_fact.c       # 带路径的文件名
```

**调试模式输出流程**:
1. **Step 1**: 生成并显示 AST（抽象语法树）
2. **Step 2**: 生成并显示 LLVM IR
3. **Step 3**: 从 IR 生成并显示 RISC-V 汇编
4. **Step 4**: 准备验证文件（复制到 test/ 目录）
5. **Step 5**: 运行验证，与 Clang 输出对比

调试模式会在 `test/debug/` 目录下生成：
- `<filename>_ast.txt` - 抽象语法树
- `<filename>_ir.ll` - LLVM IR
- `<filename>_asm.s` - RISC-V 汇编代码

**使用场景**:
- 学习编译器各阶段的输出格式
- 调试特定文件的编译问题
- 查看编译过程的详细信息

### 清理输出

```bash
# 清理构建产物
make clean

# 清理测试输出
rm -rf test/asm test/ir test/debug
```

### 输出位置

无论测试哪个目录，输出都统一在 `test/` 目录下：

```
test/
├── asm/          # 汇编输出
│   ├── <file1>_toyc.s
│   ├── <file1>_clang.s
│   └── ...
├── ir/           # IR 输出
│   ├── <file1>_toyc.ll
│   ├── <file1>_clang.ll
│   └── ...
├── debug/        # 调试模式输出（仅 make verify-debug）
│   ├── <file>_ast.txt
│   ├── <file>_ir.ll
│   └── <file>_asm.s
└── verify_temp/  # 验证时的临时文件（可忽略）
```

---

## 测试用例说明

项目包含多个测试集，覆盖不同的语言特性和使用场景。

### 主测试集（examples/compiler_inputs/）

基础功能测试，包含 24 个测试用例：

| 文件                         | 测试功能             |
| ---------------------------- | -------------------- |
| `01_minimal.c`               | 最小程序（空 main）  |
| `02_assignment.c`            | 变量赋值             |
| `03_if_else.c`               | if-else 条件语句     |
| `04_while_break.c`           | while 循环和 break   |
| `05_function_call.c`         | 函数调用             |
| `06_continue.c`              | continue 语句        |
| `07_scope_shadow.c`          | 作用域和变量遮蔽     |
| `08_short_circuit.c`         | 逻辑短路求值         |
| `09_recursion.c`             | 递归函数（斐波那契） |
| `10_void_fn.c`               | void 函数            |
| `11_precedence.c`            | 运算符优先级         |
| `12_division_check.c`        | 除法和取模运算       |
| `13_scope_block.c`           | 块作用域             |
| `14_nested_if_while.c`       | 嵌套控制流           |
| `15_multiple_return_paths.c` | 多返回路径           |
| `16_test_multiple_funcs.c`   | 多函数               |
| `17_test_nested_loops.c`     | 嵌套循环             |
| `18_test_break_continue.c`   | break 和 continue    |
| `19_test_call.c`             | 函数调用             |
| `20_test_if.c`               | if 语句              |
| `21_test_mod_ops.c`          | 取模运算             |
| `22_test_nested_block.c`     | 嵌套块作用域         |
| `23_test_void.c`             | void 函数            |
| `24_test_while.c`            | while 循环           |

### 使用示例

```bash
# 测试主测试集
make verify

# 测试单函数集
make verify TEST_SRC_DIR=examples/single_func

# 测试规范集
make verify TEST_SRC_DIR=examples/spec

# 调试特定文件
make verify-debug FILE=examples/spec/test_fact.c
```

---

## 实用技巧

### 快速检查单个文件

```bash
# 只编译不验证
./toyc examples/compiler_inputs/01_minimal.c --mode asm

# 编译并验证
make verify FILE=01_minimal.c

# 查看详细的编译过程
make verify-debug FILE=01_minimal.c
```

### 对比不同目录的测试结果

```bash
# 测试并记录结果
make verify TEST_SRC_DIR=examples/compiler_inputs 2>&1 | tee result_inputs.txt
make verify TEST_SRC_DIR=examples/single_func 2>&1 | tee result_single.txt
make verify TEST_SRC_DIR=examples/spec 2>&1 | tee result_spec.txt
```

### 查找编译失败的文件

```bash
# 批量测试后查看哪些文件编译失败
make verify TEST_SRC_DIR=examples/single_func 2>&1 | grep "SKIPPED"
```

### 性能测试

```bash
# 测量编译时间
time ./toyc examples/compiler_inputs/09_recursion.c --mode asm > /dev/null

# 批量测试性能
time make test TEST_SRC_DIR=examples/compiler_inputs
```

---

## 常见问题

### Q: 编译失败但没有详细错误信息？

A: 使用 `make verify-debug` 查看详细的编译过程和失败阶段：
```bash
make verify-debug FILE=<your_file.c>
```

### Q: 如何添加自己的测试用例？

A: 将 `.c` 文件放入任意目录，然后使用 `TEST_SRC_DIR` 参数测试：
```bash
mkdir my_tests
# 将你的 .c 文件放入 my_tests/
make verify TEST_SRC_DIR=my_tests
```

### Q: 验证失败是什么原因？

A: 验证失败通常有以下原因：
- **退出码不匹配**: ToyC 生成的程序退出码与 Clang 不同
- **输出不一致**: 程序的标准输出与预期不符
- **汇编编译失败**: 生成的汇编代码有语法错误

使用 `make verify-debug` 查看详细信息，检查 `test/asm/` 下的汇编文件。

### Q: 如何清理所有测试文件？

A: 
```bash
make clean                    # 清理编译产物
rm -rf test/                  # 清理所有测试输出
```

---

## 项目结构

```
C-SubsetCompilerUsingLLVM/
├── Makefile                  # 构建配置
├── README.md                 # 本文档
├── toyc                      # 编译器可执行文件（编译后生成）
├── src/                      # 源代码
│   ├── main.cpp              # 主程序入口
│   ├── lexer.cpp             # 词法分析器
│   ├── parser.cpp            # 语法分析器
│   ├── ast.cpp               # AST 节点实现
│   ├── llvm_ir.cpp           # LLVM IR 生成
│   ├── riscv_gen.cpp         # RISC-V 汇编生成
│   ├── ra_linear_scan.cpp    # 寄存器分配
│   └── include/              # 头文件
├── scripts/                  # 测试脚本
│   ├── generate_asm.sh       # 批量生成汇编
│   ├── generate_ir.sh        # 批量生成 IR
│   ├── verify_output.sh      # 验证输出
│   ├── verify_debug.sh       # 调试模式验证
│   └── crt0.s                # 启动代码
├── examples/                 # 测试用例
│   ├── compiler_inputs/      # 主测试集
│   ├── single_func/          # 单函数测试
│   └── spec/                 # 规范测试
├── test/                     # 测试输出（自动生成）
│   ├── asm/                  # 汇编文件
│   ├── ir/                   # LLVM IR 文件
│   └── debug/                # 调试输出
├── docs/                     # 技术文档
└── build/                    # 编译中间文件
```
