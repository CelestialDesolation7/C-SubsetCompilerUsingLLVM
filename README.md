# ToyC - C 语言子集编译器

## 📖 目录

- [项目简介](#项目简介)
- [核心技术与亮点](#核心技术与亮点)
- [支持的语言特性](#支持的语言特性)
- [快速开始](#快速开始)
- [使用方法](#使用方法)
- [测试与验证](#测试与验证)
- [测试用例说明](#测试用例说明)
- [输出示例](#输出示例)
- [项目结构](#项目结构)
- [技术文档](#技术文档)

---

## 项目简介

ToyC 是一个基于现代编译技术实现的 **C 语言子集编译器**，采用多阶段编译架构，支持将 C 代码编译为 **LLVM IR** 和 **RISC-V 汇编代码**。

### 编译流程

```
C 源代码 → 词法分析 → 语法分析 → AST → IRBuilder → ir::Module → 寄存器分配 → RISC-V 汇编
```

### 技术栈

- **语言**: C++20
- **构建系统**: CMake (≥ 3.16) + Unix Makefiles (WSL)
- **目标架构**: RISC-V 32-bit (RV32I)
- **中间表示**: 结构化 LLVM IR (SSA 形式)，具有完整的 Opcode 枚举和类型化指令模型
- **寄存器分配**: 线性扫描算法
  （原论文：https://web.cs.ucla.edu/~palsberg/course/cs132/linearscan.pdf）
  （关于本项目中实现的解释：[线性扫描算法核心思路.md](docs/线性扫描算法核心思路.md)）

---

## 核心技术与亮点

### 1. 完整的编译器前端

#### 词法分析器 (Lexer)
- **手工实现**的高效词法分析器
- 支持单字符和多字符运算符识别
- 完整的关键字、标识符、数字字面量处理
- 支持单行 `//` 和多行 `/* */` 注释

#### 语法分析器 (Parser)
- **递归下降**解析算法
- 完整的表达式优先级处理
- 支持复杂控制流语句嵌套
- 作用域管理和符号表维护

#### 抽象语法树 (AST)
- 面向对象的节点设计
- 支持语法树可视化输出
- 类型检查和语义分析

### 2. 结构化 IR 模型

采用类似 LLVM 官方项目的 **结构化指令模型**，通过 `Opcode` 枚举和类型化 `Instruction` 类实现：

- **`ir::Opcode` 枚举**: 明确定义所有指令类型 (`Alloca`, `Load`, `Store`, `Add`, `Sub`, `Mul`, `SDiv`, `SRem`, `ICmp`, `Br`, `CondBr`, `Ret`, `RetVoid`, `Call`)
- **`ir::Operand` 类**: 类型安全的操作数 (`VReg`, `Imm`, `Label`, `BoolLit`)
- **`ir::Instruction` 工厂方法**: 如 `makeAlloca()`, `makeBinOp()`, `makeICmp()` 等
- **基于 Opcode 的查询接口**: `defReg()`, `useRegs()`, `isTerminator()`, `branchTargets()` — **无需正则表达式或字符串匹配**
- **`ir::BasicBlock` / `ir::Function` / `ir::Module`**: 完整的 CFG 构建和管理

#### IRBuilder
- AST → 结构化 IR 的直接转换
- 作用域栈管理
- **短路求值**优化（逻辑运算符 `&&`, `||`）
- 函数调用约定实现

#### IRParser
- 文本 LLVM IR → 结构化 `ir::Module` 的解析器
- 支持从 `.ll` 文件导入

### 3. 寄存器分配算法

实现了经典的 **线性扫描寄存器分配算法** (Linear Scan Register Allocation)，直接操作结构化 IR：

#### 核心特性
- **活跃区间计算**: 通过 `ir::Instruction::defReg()` / `useRegs()` 精确分析变量生命周期，无需正则匹配
- **物理寄存器映射**: 符合 RISC-V 调用约定
  - `a0-a7`: 参数寄存器（最高分配优先级）
  - `t2-t6`: 临时寄存器
  - `s1-s11`: 保存寄存器
  - `t0/t1`: 溢出专用临时寄存器
- **溢出处理**: 自动栈分配和加载/存储生成
- **参数处理**: 支持多参数函数调用（前 8 个通过寄存器，其余通过栈）

#### 算法优势
- **时间复杂度**: O(n log n)，远优于图着色算法
- **空间效率**: 高效利用寄存器，减少内存访问
- **工业级实现**: 被广泛应用于 JVM、V8 等生产环境

详细算法说明见：[docs/线性扫描算法核心思路.md](docs/线性扫描算法核心思路.md)

### 4. RISC-V 代码生成

基于 **Opcode 分派**的代码生成器，通过 `switch(inst.opcode)` 实现指令级分发：

#### 支持的指令集
- **算术运算**: `add`, `addi`, `sub`, `mul`, `div`, `rem`
- **比较运算**: `slt`, `seqz`, `snez`
- **控制流**: `beq`, `bne`, `blt`, `bge`, `bgt`, `ble`, `j`, `ret`
- **内存访问**: `lw`, `sw`, `lb`, `sb`

#### 优化技术
- **比较-分支融合**: `icmp + condBr` 合并为单条 RISC-V 分支指令
- **立即数优化**: `add %x, imm` → `addi`
- **占位符栈帧**: 延迟计算栈大小，支持多返回路径

#### 调用约定
完全符合 **RISC-V ABI** 规范：
- 参数传递：`a0-a7` (前 8 个参数)
- 返回值：`a0`
- 返回地址：`ra`
- 栈指针：`sp`
- 帧指针：`s0`

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
		单行注释以 "//" 开始、到最近的换行符前结束，多行注释以 "/*" 开始、以最近的 "*/" 结束。

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
| 逻辑 | `&&`, `\|\|`, `!` |
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
- ✅ 多参数传递（含超过 8 个参数的栈传递）
- ✅ 作用域和变量遮蔽
- ✅ 块级作用域 `{ ... }`
- ✅ 短路求值优化
- ✅ 运算符优先级和结合性
- ✅ 多行/单行注释

---

## 快速开始

### 环境要求

- Windows 11 + **WSL 2**（Ubuntu 推荐）
- CMake ≥ 3.16 + g++ ≥ 13（C++20）
- clang（含 RISC-V 后端）
- qemu-user（RISC-V 用户态模拟器）
- riscv64-unknown-elf-gcc（可选，提供 libgcc 软件除法支持）

### 安装依赖

```bash
# 在 WSL (Ubuntu) 中执行
sudo apt update
sudo apt install build-essential cmake clang qemu-user
# 可选：安装 RISC-V 工具链以获取 libgcc
sudo apt install gcc-riscv64-unknown-elf
```

### 编译项目

```bash
# 从 Windows PowerShell 通过 WSL 编译（推荐）
wsl make

# 或在 WSL 终端中直接执行
make
```

编译完成后，可执行文件位于 `build/toyc` 和 `build/toyc_test`。

---

## 使用方法

### 命令行接口

```
用法: toyc <input.[c|tc|ll]> [options]

选项:
  --ast         输出抽象语法树
  --ir          输出 LLVM IR
  --asm         输出 RISC-V 汇编（默认）
  --all         输出 AST + IR + 汇编
  -o <file>     将汇编写入指定文件
```

### 使用示例

```bash
# 1. 编译到 RISC-V 汇编（默认模式）
./build/toyc examples/compiler_inputs/01_minimal.c

# 2. 生成 LLVM IR
./build/toyc examples/compiler_inputs/05_function_call.c --ir

# 3. 查看 AST
./build/toyc examples/compiler_inputs/03_if_else.c --ast

# 4. 同时查看 AST + IR + 汇编
./build/toyc examples/compiler_inputs/09_recursion.c --all

# 5. 将汇编输出到文件
./build/toyc examples/compiler_inputs/09_recursion.c -o output.s

# 6. 从 .ll 文件生成汇编（支持 IR 输入）
./build/toyc test/ir/01_minimal_toyc.ll --asm

# 以上命令均在 WSL 中执行，也可从 PowerShell 调用：
# wsl ./build/toyc examples/compiler_inputs/01_minimal.c --ir
```

### Makefile 便捷目标

```bash
# 编译单个文件到汇编（stdout）
make 01_minimal.s

# 编译单个文件到 LLVM IR（stdout）
make 01_minimal.ll
```

---

## 测试与验证

ToyC 提供四层测试体系——从快速的内置单元测试到完整的 QEMU 端到端验证。
所有命令均通过 WSL 执行（从 PowerShell 调用时加 `wsl` 前缀）。

### 1. 内置单元测试

对所有 36 个测试用例执行完整编译流水线（词法 → 语法 → AST → IR → 寄存器分配 → RISC-V 汇编），验证各阶段无异常：

```bash
make test
```

输出示例：
```
Testing: 01_minimal.c ... OK
Testing: 02_assignment.c ... OK
...
Testing: 36_test_while.c ... OK

=== Results: 36/36 passed ===
```

### 2. 批量生成汇编 / IR

批量对所有测试用例同时调用 ToyC 和 Clang，生成汇编或 IR 以供人工对比分析。

```bash
# 批量生成 RISC-V 汇编（ToyC + Clang）
make generate-asm
# 输出到 test/asm/：<base>_toyc.s 和 <base>_clang.s

# 批量生成 LLVM IR（ToyC + Clang）
make generate-ir
# 输出到 test/ir/：<base>_toyc.ll 和 <base>_clang.ll
```

### 3. 端到端验证（需 clang + qemu）

对每个测试用例，分别将 ToyC 和 Clang 生成的汇编**链接为 RISC-V ELF** 并在 **QEMU** 中执行，对比两者的退出码和输出，确保 ToyC 生成的代码行为与 Clang 完全一致。

```bash
# 一键验证（自动生成汇编 + 验证，汇编生成过程静默）
make verify
```

验证流程：
```
ToyC ASM → Clang 汇编器 → ELF → QEMU 执行 → 退出码 A
Clang ASM → Clang 汇编器 → ELF → QEMU 执行 → 退出码 B
比较 A == B → ✅ PASS / ❌ FAIL
```

> **说明**: 链接使用自定义启动代码 `scripts/crt0.s`（调用 `main` 后执行 `ecall` 退出），无需标准 C 库。

### 4. 单文件调试模式

对单个文件输出所有中间产物（AST → IR → ASM），同时生成 Clang 参考输出，并自动进行端到端验证：

```bash
make debug FILE=01_minimal.c
```

调试模式流程（6 步）：
1. 生成 AST 并打印
2. 生成 LLVM IR 并保存
3. 生成 RISC-V 汇编
4. 生成 Clang 参考 IR 和汇编
5. 复制产物到 `test/asm/` 和 `test/ir/` 目录
6. 调用 `verify_output.sh` 执行端到端验证

### 5. 从 Windows PowerShell 调用

在 Windows 环境开发时，所有 make 指令通过 `wsl` 前缀调用：

```powershell
wsl make              # 编译
wsl make test         # 运行单元测试
wsl make verify       # 端到端验证
wsl make debug FILE=01_minimal.c
```

### 清理

```bash
make clean      # 清理 build/ 和 test/ 目录
make rebuild    # 清理后重新编译
```

---

## 测试用例说明

项目包含 **36 个测试用例**，覆盖从基础语法到复杂控制流的各种场景：

### 基础语法（01–15）

| 文件                         | 测试功能             | 预期退出码 |
| ---------------------------- | -------------------- | ---------- |
| `01_minimal.c`               | 最小程序（空 main）  | 0          |
| `02_assignment.c`            | 变量赋值运算         | 3          |
| `03_if_else.c`               | if-else 条件分支     | 4          |
| `04_while_break.c`           | while 循环 + break   | 5          |
| `05_function_call.c`         | 基本函数定义与调用   | 7          |
| `06_continue.c`              | continue 跳过迭代    | 8          |
| `07_scope_shadow.c`          | 块作用域变量遮蔽     | 1          |
| `08_short_circuit.c`         | 逻辑短路求值 `&&` `\|\|` | 211    |
| `09_recursion.c`             | 递归函数（阶乘）     | 120        |
| `10_void_fn.c`               | void 函数定义与调用  | 0          |
| `11_precedence.c`            | 运算符优先级         | 14         |
| `12_division_check.c`        | 整数除法             | 2          |
| `13_scope_block.c`           | 块内局部变量作用域   | 8          |
| `14_nested_if_while.c`       | 嵌套 if + while      | 6          |
| `15_multiple_return_paths.c` | 多路径 return        | 62         |

### 进阶功能（16–20）

| 文件                         | 测试功能             |
| ---------------------------- | -------------------- |
| `16_complex_syntax.c`        | 复杂语法含注释解析   |
| `17_complex_expressions.c`   | 阶乘 + 斐波那契 + 嵌套调用 |
| `18_many_variables.c`        | 大量局部变量（寄存器溢出） |
| `19_many_arguments.c`        | 8/16 参数函数（栈传递） |
| `20_comprehensive.c`         | 综合测试：递归 + 循环 + 多函数 |

### 回归测试（21–36）

| 文件                           | 测试功能                   |
| ------------------------------ | -------------------------- |
| `21_test_break_continue.c`     | break + continue 组合      |
| `22_test_call.c`               | 函数调用返回值             |
| `23_test_complex.c`            | 复合表达式 + 块作用域      |
| `24_test_fact.c`               | 递归阶乘                   |
| `25_test_fib.c`                | 递归斐波那契               |
| `26_test_if.c`                 | if-else 分支               |
| `27_test_logic.c`              | 逻辑表达式 `&& \|\| !`    |
| `28_test_logical_multiple.c`   | 多条件逻辑组合             |
| `29_test_mix_expr.c`           | 混合表达式 + 一元运算      |
| `30_test_modulo.c`             | 取模运算                   |
| `31_test_multiple_funcs.c`     | 多函数定义与调用           |
| `32_test_nested_block.c`       | 嵌套块作用域               |
| `33_test_nested_loops.c`       | 嵌套 while 循环            |
| `34_test_unary.c`              | 一元运算符 `-` 和 `!`     |
| `35_test_void.c`               | void 函数                  |
| `36_test_while.c`              | while + break 循环控制     |

---

## 输出示例

以 `01_minimal.c` 为例：

```c
// 01_minimal.c
int main() {
    return 0;
}
```

### AST 输出 (`--ast`)

```
=== AST ===
FuncDef: int main()
  Block:
    Return:
      Number: 0
```

### LLVM IR 输出 (`--ir`)

```
=== LLVM IR ===
define i32 @main() {
entry:
  ret i32 0
}
```

### RISC-V 汇编输出 (`--asm`)

```
=== RISC-V Assembly ===
  .text
  .globl main
main:
  addi sp, sp, -16
  sw ra, 12(sp)
  sw s0, 8(sp)
  addi s0, sp, 16
  li a0, 0
  lw ra, 12(sp)
  lw s0, 8(sp)
  addi sp, sp, 16
  ret
```

---

## 项目结构

```
C-SubsetCompilerUsingLLVM/
├── CMakeLists.txt                  # CMake 构建配置
├── Makefile                        # 包装器：build / test / verify / debug / clean
├── README.md                       # 本文档
│
├── src/                            # 源代码
│   ├── include/                    # 头文件（工作目录，开发时直接修改此处）
│   │   ├── token.h                 #   Token 类型枚举
│   │   ├── lexer.h                 #   词法分析器
│   │   ├── ast.h                   #   AST 节点定义
│   │   ├── parser.h                #   语法分析器
│   │   ├── ir.h                    #   结构化 IR 模型（Opcode/Operand/Instruction/BB/Function/Module）
│   │   ├── ir_builder.h            #   IRBuilder（AST → ir::Module）
│   │   ├── ir_parser.h             #   IRParser（LLVM IR 文本 → ir::Module）
│   │   ├── reg_alloc.h             #   线性扫描寄存器分配器
│   │   └── riscv_codegen.h         #   RISC-V 代码生成器
│   ├── main.cpp                    # 主程序入口（CLI 处理）
│   ├── lexer.cpp                   # 词法分析实现
│   ├── ast.cpp                     # AST 打印实现
│   ├── parser.cpp                  # 语法分析实现
│   ├── ir.cpp                      # IR 模型实现（工厂方法、查询接口、序列化）
│   ├── ir_builder.cpp              # IRBuilder 实现（AST → IR 转换）
│   ├── ir_parser.cpp               # IRParser 实现（.ll 文本 → IR 结构）
│   ├── reg_alloc.cpp               # 寄存器分配器实现
│   ├── riscv_codegen.cpp           # RISC-V 代码生成实现
│   └── unified_test.cpp            # 统一测试程序
│
├── include/toyc/                   # 公共头文件（同步自 src/include/）
│
├── scripts/                        # 构建和测试脚本（在 WSL 中运行）
│   ├── crt0.s                      #   RISC-V 启动代码（_start → main → ecall 退出）
│   ├── generate_asm.sh             #   批量生成 ToyC + Clang 汇编
│   ├── generate_ir.sh              #   批量生成 ToyC + Clang LLVM IR
│   ├── verify_output.sh            #   端到端验证（Clang 链接 → QEMU 执行 → 退出码对比）
│   ├── verify_debug.sh             #   单文件调试模式（输出全部中间产物 + 验证）
│   └── test_instr.sh               #   指令测试命令参考
│
├── examples/                       # 测试用例
│   └── compiler_inputs/            #   36 个 .c 测试文件
│       ├── 01_minimal.c            #   最小程序
│       ├── ...                     #   ...
│       └── 36_test_while.c         #   while 循环
│
├── docs/                           # 技术文档（6 篇）
│   ├── 编译流程详解.md
│   ├── 从调用链理解的寄存器分配流程.md
│   ├── 从调用链理解的目标代码生成.md
│   ├── 汇编到验证完整流程.md
│   ├── 寄存器分配与代码生成详解.md
│   └── 线性扫描算法核心思路.md
│
├── assets/                         # 文档图片资源
└── build/                          # 构建产物（自动生成）
    ├── toyc                        #   编译器主程序 (ELF)
    └── toyc_test                   #   统一测试程序 (ELF)
```

---

## 技术文档

详细的技术说明位于 `docs/` 目录：

| 文档 | 说明 |
|------|------|
| [编译流程详解](docs/编译流程详解.md) | 整体编译流水线：词法分析 → 语法分析 → IR 生成 → 寄存器分配 → 代码生成 |
| [从调用链理解的寄存器分配流程](docs/从调用链理解的寄存器分配流程.md) | 活跃性分析 + 线性扫描寄存器分配的完整调用链追踪 |
| [从调用链理解的目标代码生成](docs/从调用链理解的目标代码生成.md) | RISC-V 汇编生成的完整调用链追踪 |
| [汇编到验证完整流程](docs/汇编到验证完整流程.md) | 从汇编输出到 QEMU 验证的端到端流程 |
| [寄存器分配与代码生成详解](docs/寄存器分配与代码生成详解.md) | 寄存器分配与代码生成的算法细节 |
| [线性扫描算法核心思路](docs/线性扫描算法核心思路.md) | 线性扫描寄存器分配核心算法解释 |
