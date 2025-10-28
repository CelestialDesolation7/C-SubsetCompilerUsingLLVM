#pragma once
#include <vector>
#include <memory>
#include <iostream>
using namespace std;

//--------------------------------------------------------
// 抽象语法树（AST）所有节点的抽象基类
//--------------------------------------------------------
struct ASTNode
{
    virtual ~ASTNode() = default;             // 虚析构，确保派生类正确析构
    virtual void print(int indent) const = 0; // 纯虚函数：打印节点，indent 表示缩进级别
};

// 智能指针别名：指向 ASTNode 的共享所有权指针
using ASTPtr = shared_ptr<ASTNode>;

//--------------------------------------------------------
// 表达式基类（继承自 ASTNode）
//--------------------------------------------------------
struct Expr : ASTNode {};

// 数字字面量表达式节点
struct NumberExpr : Expr
{
    int value;                             // 存储数字值
    NumberExpr(int v) : value(v) {}        // 构造时初始化数字
    void print(int indent) const override; // 按缩进打印数字
};

// 标识符表达式节点
struct IdentifierExpr : Expr
{
    string name;                                // 变量名或函数名
    IdentifierExpr(string n) : name(move(n)) {} // 构造时移动字符串
    void print(int indent) const override;      // 按缩进打印标识符
};

// 二元运算表达式节点
struct BinaryExpr : Expr
{
    string op;       // 运算符
    ASTPtr lhs, rhs; // 左右子表达式
    BinaryExpr(string o, ASTPtr l, ASTPtr r)
        : op(move(o)), lhs(move(l)), rhs(move(r)) {}
    void print(int indent) const override; // 按缩进打印运算及子节点
};

// 一元运算表达式节点
struct UnaryExpr : Expr
{
    string op;   // 运算符
    ASTPtr expr; // 作用对象表达式
    UnaryExpr(string o, ASTPtr e)
        : op(move(o)), expr(move(e)) {}
    void print(int indent) const override; // 按缩进打印运算符及子表达式
};

// 函数调用表达式节点
struct CallExpr : Expr
{
    string callee;                          // 被调用函数名称
    vector<ASTPtr> args;                    // 参数列表
    CallExpr(string c) : callee(move(c)) {} // 构造时初始化函数名
    void print(int indent) const override;  // 按缩进打印调用信息及参数
};

//--------------------------------------------------------
// 语句基类（继承自 ASTNode）
//--------------------------------------------------------
struct Stmt : ASTNode {};

// 赋值语句节点
struct AssignStmt : Stmt
{
    string name; // 被赋值变量名
    ASTPtr expr; // 右值表达式
    AssignStmt(string n, ASTPtr e)
        : name(move(n)), expr(move(e)) {}
    void print(int indent) const override; // 打印赋值语句
};

// 声明语句节点
struct DeclStmt : Stmt
{
    string name; // 声明变量名
    ASTPtr expr; // 初始化表达式
    DeclStmt(string n, ASTPtr e)
        : name(move(n)), expr(move(e)) {}
    void print(int indent) const override; // 打印声明语句
};

// if 语句节点，包括可选的 else 分支
struct IfStmt : Stmt
{
    ASTPtr cond;     // 条件表达式
    ASTPtr thenStmt; // then 分支语句
    ASTPtr elseStmt; // else 分支语句（可空）
    IfStmt(ASTPtr c, ASTPtr t, ASTPtr e)
        : cond(move(c)), thenStmt(move(t)), elseStmt(move(e)) {}
    void print(int indent) const override; // 打印 if/else 结构
};

// while 循环语句节点
struct WhileStmt : Stmt
{
    ASTPtr cond; // 循环条件
    ASTPtr body; // 循环体语句
    WhileStmt(ASTPtr c, ASTPtr b)
        : cond(move(c)), body(move(b)) {}
    void print(int indent) const override; // 打印 while 结构
};

// break 语句节点
struct BreakStmt : Stmt
{
    void print(int indent) const override; // 按缩进打印 "break"
};

// continue 语句节点
struct ContinueStmt : Stmt
{
    void print(int indent) const override; // 按缩进打印 "continue"
};

// 返回语句节点
struct ReturnStmt : Stmt
{
    ASTPtr expr; // 返回值表达式
    ReturnStmt(ASTPtr e) : expr(move(e)) {}
    void print(int indent) const override; // 按缩进打印 return 及表达式
};

// 语句块节点，用于表示大括号中的多条语句
struct BlockStmt : Stmt
{
    vector<ASTPtr> stmts;                  // 内部语句列表
    void print(int indent) const override; // 打印块及内部所有语句
};

// 函数参数结构，仅包含参数名
struct Param
{
    string name; // 参数名称
};

// 函数定义节点，包含返回类型、函数名、参数列表和函数体
struct FuncDef : ASTNode
{
    string retType;                        // 返回类型（"int" 或 "void"）
    string name;                           // 函数名称
    vector<Param> params;                  // 参数列表
    shared_ptr<BlockStmt> body;            // 函数体
    void print(int indent) const override; // 打印函数签名及函数体
};
