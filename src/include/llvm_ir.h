#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>

/**
 * @brief LLVM IR 生成器类
 *
 * 本类负责将 AST 转换为 LLVM IR 代码。
 *
 * ## 核心机制说明
 *
 * ### 1. 虚拟寄存器编号 (varCount)
 * - LLVM IR 使用数字编号的虚拟寄存器，如 %1, %2, %3 等
 * - varCount 是全局递增计数器，确保每个虚拟寄存器编号唯一
 * - newTemp() 函数返回完整的虚拟寄存器名（带%前缀），如 "%1"
 * - 函数入口时，varCount 初始化为参数数量（因为参数占用 %0, %1, ... 编号）
 *
 * ### 2. 作用域栈 (scopeStack)
 * - 类型：vector<map<string, string>>
 * - 用途：管理变量名到虚拟寄存器的映射，支持嵌套作用域
 * - 存储格式：源码变量名 -> 完整虚拟寄存器名（带%前缀）
 * - 示例：{"a": "%3", "b": "%4"} 表示变量 a 的 alloca 地址在 %3，b 在 %4
 * - 查找时从内层向外层搜索，实现变量遮蔽
 *
 * ### 3. 已加载值缓存 (loadedValues)
 * - 类型：map<string, string>
 * - 用途：缓存已经通过 load 指令加载到寄存器的变量值，避免重复 load
 * - 存储格式：源码变量名 -> 存储该变量值的完整虚拟寄存器名（带%前缀）
 * - 示例：{"a": "%5"} 表示变量 a 的当前值已加载到 %5
 * - 当变量被赋值时，需要清除对应的缓存条目
 * - 进入新的控制流基本块时，通常需要清除整个缓存
 *
 * ### 4. 标签计数器 (labelCount)
 * - 用于生成唯一的基本块标签
 * - newLabel(base) 返回 "base_N" 形式的标签名
 * - 每处理完一个控制结构后递增
 */
class LLVMIRGenerator
{
public:
    // 构造函数
    LLVMIRGenerator();

    // 为单个函数生成LLVM IR
    std::string generateFunction(const std::shared_ptr<FuncDef> &funcDef);

    // 生成完整的模块IR（包含所有函数）
    std::string generateModule(const std::vector<std::shared_ptr<FuncDef>> &funcs);

private:
    /**
     * @brief 虚拟寄存器编号计数器
     *
     * 每次调用 newTemp() 时递增，用于生成唯一的虚拟寄存器编号。
     * 函数入口时初始化为参数数量（参数占用 %0, %1, ... 编号）。
     */
    int varCount;

    /**
     * @brief 基本块标签计数器
     *
     * 用于生成唯一的基本块标签，如 then_0, else_0, while_cond_1 等。
     */
    int labelCount;

    std::string currentFunction; // 当前处理的函数名

    /**
     * @brief 作用域栈
     *
     * 每个元素是一个 map，表示一个作用域层级。
     * map 的 key 是源码中的变量名，value 是该变量 alloca 指令的结果寄存器（带%前缀）。
     * 例如：{"x": "%3"} 表示变量 x 通过 "%3 = alloca i32" 分配。
     */
    std::vector<std::map<std::string, std::string>> scopeStack;

    /**
     * @brief 已加载变量值的缓存
     *
     * key 是源码中的变量名，value 是通过 load 指令加载后存放值的寄存器（带%前缀）。
     * 例如：{"x": "%5"} 表示 "%5 = load i32, ptr %3" 已执行，%5 存放 x 的当前值。
     * 注意：变量被赋值后需清除对应缓存；进入新基本块时通常清除全部缓存。
     */
    std::map<std::string, std::string> loadedValues;

    int stackOffset;                         // 当前栈偏移（预留，暂未使用）
    std::vector<std::string> breakLabels;    // break 目标标签栈（用于循环）
    std::vector<std::string> continueLabels; // continue 目标标签栈（用于循环）
    std::string currentInstructions;         // 当前累积的指令序列
    bool hasReturn;                          // 标记当前函数是否已有返回语句
    bool isMainFunction;                     // 标记是否为 main 函数
    std::vector<std::string> blockLabels;    // 基本块标签栈，用于生成 preds 注释
    std::vector<std::string> allocas;        // 函数入口处的 alloca 指令列表
    std::vector<std::string> initStores;     // 函数入口处的初始化 store 指令列表

    // 作用域管理方法
    void enterScope();                                                   // 进入新作用域
    void exitScope();                                                    // 退出当前作用域
    void addVariable(const std::string &name, const std::string &varId); // 在当前作用域添加变量（varId 带%前缀）
    std::string findVariable(const std::string &name);                   // 查找变量，返回其 alloca 寄存器（带%前缀）

    /**
     * @brief 生成新的虚拟寄存器名
     *
     * @return 带%前缀的完整虚拟寄存器名，如 "%1", "%2" 等
     */
    std::string newTemp();

    // 生成新的标签名
    std::string newLabel(const std::string &base);

    /**
     * @brief 获取变量的 alloca 地址寄存器
     *
     * @param name 源码中的变量名
     * @return 该变量 alloca 的结果寄存器（带%前缀），如 "%3"；未找到返回空字符串
     */
    std::string getVariableOffset(const std::string &name);

    // 分配新的栈偏移（预留接口）
    int allocateStack();

    /**
     * @brief 添加指令到当前指令序列
     *
     * @param instruction 要添加的指令文本
     * @param isLabel 是否为标签行（标签行不缩进）
     */
    void addInstruction(const std::string &instruction, bool isLabel = false);

    // 生成表达式IR，返回临时变量名
    std::string generateExpr(const ASTPtr &expr);

    // 生成语句IR
    void generateStmt(const ASTPtr &stmt);

    // 生成语句块IR
    void generateBlock(const std::shared_ptr<BlockStmt> &block);

    // 生成函数参数IR
    std::string generateParams(const std::vector<Param> &params);

    // 生成函数调用IR
    std::string generateCall(const std::shared_ptr<CallExpr> &call);

    // 生成条件分支IR
    void generateIf(const std::shared_ptr<IfStmt> &ifStmt);

    // 生成循环IR
    void generateWhile(const std::shared_ptr<WhileStmt> &whileStmt);

    // 生成返回语句IR
    void generateReturn(const std::shared_ptr<ReturnStmt> &returnStmt);

    // 生成break语句IR
    void generateBreak();

    // 生成continue语句IR
    void generateContinue();

    // 生成赋值语句IR
    void generateAssign(const std::shared_ptr<AssignStmt> &assign);

    // 生成声明语句IR
    void generateDecl(const std::shared_ptr<DeclStmt> &decl);

    // 生成二元运算IR
    std::string generateBinaryOp(const std::string &op, const ASTPtr &lhs, const ASTPtr &rhs);

    // 生成一元运算IR
    std::string generateUnaryOp(const std::string &op, const ASTPtr &expr);

    // 生成比较运算IR
    std::string generateComparison(const std::string &op, const ASTPtr &lhs, const ASTPtr &rhs);

    // 生成逻辑运算IR
    std::string generateLogicalOp(const std::string &op, const ASTPtr &lhs, const ASTPtr &rhs);
};

// 生成LLVM IR的辅助函数
std::string generateLLVMIR(const std::vector<std::shared_ptr<FuncDef>> &funcs);