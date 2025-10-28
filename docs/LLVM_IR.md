# ToyC 编译器 LLVM IR 生成器

## 概述

本项目实现了一个ToyC语言的编译器，能够将ToyC源代码编译成LLVM IR中间表示。编译器采用多阶段架构：

1. **词法分析** - 将源代码转换为Token流
2. **语法分析** - 将Token流转换为抽象语法树(AST)
3. **LLVM IR生成** - 将AST转换为LLVM IR

## 功能特性

### 支持的ToyC语言特性

- **数据类型**: `int`, `void`
- **变量声明**: `int x = 5;`
- **算术运算**: `+`, `-`, `*`, `/`, `%`
- **比较运算**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **逻辑运算**: `&&`, `||`, `!`
- **一元运算**: `+`, `-`, `!`
- **控制流**: `if-else`, `while`, `break`, `continue`
- **函数**: 函数定义和调用
- **语句块**: `{ ... }`

### 输出模式

编译器支持三种输出模式：

1. **AST模式** (`ast`) - 显示抽象语法树
2. **IR模式** (`ir`) - 显示LLVM IR代码
3. **全部模式** (`all`) - 同时显示AST和LLVM IR

## 使用方法

### 编译

```bash
g++ -std=c++17 -o toyc src/*.cpp
```

### 运行

```bash
# 默认显示AST
./toyc source_file.tc

# 显示AST
./toyc source_file.tc ast

# 显示LLVM IR
./toyc source_file.tc ir

# 显示AST和LLVM IR
./toyc source_file.tc all
```

## 示例

### 输入文件 (test.c)
```c
int main() {
    int a = 5;
    int b = -a;
    int c = !0;
    return b + c;
}
```

### AST输出
```
=== Abstract Syntax Tree ===
Function int main()
  Block
    Decl(a)
      Number(5)
    Decl(b)
      Unary(-)
        Identifier(a)
    Decl(c)
      Unary(!)
        Number(0)
    Return
      Binary(+)
        Identifier(b)
        Identifier(c)
```

### LLVM IR输出
```llvm
=== LLVM IR ===
; ModuleID = 'toyc'
source_filename = "toyc"

define i32 @main() {
  %a_addr = alloca i32
  store i32 5, i32* %a_addr
  %t0 = load i32, i32* %a_addr
  %t1 = sub i32 0, %t0
  %b_addr = alloca i32
  store i32 %t1, i32* %b_addr
  %t2 = icmp eq i32 0, 0
  %c_addr = alloca i32
  store i32 %t2, i32* %c_addr
  %t3 = load i32, i32* %b_addr
  %t4 = load i32, i32* %c_addr
  %t5 = add i32 %t3, %t4
  ret i32 %t5
}
```

## 项目结构

```
src/
├── lexer.h/lexer.cpp      # 词法分析器
├── parser.h/parser.cpp     # 语法分析器
├── ast.h/ast.cpp          # 抽象语法树定义
├── llvm_ir.h/llvm_ir.cpp  # LLVM IR生成器
└── main.cpp               # 主程序

examples/
├── single_func/           # 单函数示例
└── multi_func/            # 多函数示例
```

## 技术细节

### LLVM IR生成策略

1. **变量管理**: 使用栈分配(`alloca`)和加载/存储指令
2. **表达式求值**: 生成临时变量存储中间结果
3. **控制流**: 使用标签和分支指令实现条件跳转
4. **函数调用**: 支持参数传递和返回值处理

### 优化特性

- 避免重复的返回语句
- 正确的变量作用域管理
- 支持嵌套的控制流结构
- 完整的表达式求值

## 测试

项目包含多个测试用例，覆盖了ToyC语言的各种特性：

- 基本算术运算
- 逻辑运算和比较运算
- 控制流语句
- 函数定义和调用
- 变量声明和赋值

## 扩展性

该编译器设计具有良好的扩展性，可以轻松添加：

- 新的数据类型
- 更多的运算符
- 数组和指针支持
- 优化pass
- 目标代码生成

## 许可证

本项目仅供学习和研究使用。 