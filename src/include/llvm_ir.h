#pragma once
#include "ast.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>

// LLVM IR生成器类
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
    int tempCount;                                              // 临时变量计数器
    int labelCount;                                             // 标签计数器
    std::string currentFunction;                                // 当前处理的函数名
    std::vector<std::map<std::string, std::string>> scopeStack; // 作用域栈，每个作用域包含变量名到变量编号的映射
    std::map<std::string, std::string> loadedValues;            // 已加载的变量值缓存
    int stackOffset;                                            // 当前栈偏移
    std::vector<std::string> breakLabels;                       // break标签栈
    std::vector<std::string> continueLabels;                    // continue标签栈
    std::string currentInstructions;                            // 当前指令序列
    bool hasReturn;                                             // 标记函数是否已有返回语句
    int varCount;                                               // 变量计数器（用于生成数字标签）
    bool isMainFunction;                                        // 标记是否为main函数
    std::vector<std::string> blockLabels;                       // 栈：记录当前的基本块标签
    std::vector<std::string> allocas;
    std::vector<std::string> initStores;

    // 作用域管理方法
    void enterScope();                                                   // 进入新作用域
    void exitScope();                                                    // 退出当前作用域
    void addVariable(const std::string &name, const std::string &varId); // 在当前作用域添加变量
    std::string findVariable(const std::string &name);                   // 从当前作用域开始查找变量

    // 生成新的临时变量名
    std::string newTemp();

    // 生成新的标签名
    std::string newLabel(const std::string &base);

    // 生成新的变量名（数字格式）
    std::string newVar();

    // 获取变量的编号
    std::string getVariableOffset(const std::string &name);

    // 分配新的栈偏移
    int allocateStack();

    // 添加指令到当前序列
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