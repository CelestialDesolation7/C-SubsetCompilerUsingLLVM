#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <set>
#include <stack>
#include <list>
#include <algorithm>
#include "ra_linear_scan.h"

// RISC-V32代码生成器类
class RISCVGenerator
{
public:
    // 构造函数
    RISCVGenerator();

    // 正则表达式常量
    static const std::string REGEX_VAR_DEF;      // 变量定义：%1 =
    static const std::string REGEX_VAR_USE;      // 变量使用：%数字
    static const std::string REGEX_ALLOCA;       // alloca指令
    static const std::string REGEX_STORE;        // store指令
    static const std::string REGEX_LOAD;         // load指令
    static const std::string REGEX_CALL;         // 函数调用
    static const std::string REGEX_ARITHMETIC;   // 算术运算
    static const std::string REGEX_ICMP;         // 比较运算
    static const std::string REGEX_BR_COND;      // 条件分支
    static const std::string REGEX_BR_UNCOND;    // 无条件分支
    static const std::string REGEX_RET;          // 返回指令
    static const std::string REGEX_LABEL;        // 标签
    static const std::string REGEX_FUNCTION_DEF; // 函数定义
    static const std::string REGEX_I32_IMM;      // i32立即数
    static const std::string REGEX_I32_REG;      // i32寄存器

    // 生成完整的汇编文件
    std::string generateModule(const std::string &llvmIR);

private:
    int tempCount;                           // 临时变量计数器
    int labelCount;                          // 标签计数器
    std::string currentFunction;             // 当前处理的函数名
    std::map<std::string, int> variables;    // 变量名到栈偏移的映射
    int stackOffset;                         // 当前栈偏移
    int totalStackSize;                      // 总栈帧大小
    std::vector<std::string> breakLabels;    // break标签栈
    std::vector<std::string> continueLabels; // continue标签栈
    std::string currentInstructions;         // 当前指令序列
    bool hasReturn;                          // 标记函数是否已有返回语句
    bool isMainFunction;                     // 标记是否为main函数
    int functionCount;                       // 函数计数器

    struct CmpInfo
    {
        std::string op, lhsReg, rhsReg;
    };
    std::unordered_map<std::string, CmpInfo> cmpMap;

    // 新的线性扫描寄存器分配器
    const ra_ls::RegInfo regInfo = ra_ls::RegInfo();
    ; // 寄存器信息对象
    std::unique_ptr<ra_ls::LinearScanAllocator> registerAllocator;
    std::unique_ptr<ra_ls::FunctionIR> currentFunctionIR;

    // 函数名到分配器实例的映射（AllocationResult由分配器管理）
    std::map<std::string, std::unique_ptr<ra_ls::LinearScanAllocator>> functionAllocators;

    // 新增成员变量
    int instructionCount = 0;
    int preciseInstructionCount = 0;

    // 生成新的临时变量名
    std::string newTemp();

    // 生成新的标签名
    std::string newLabel(const std::string &base);

    // 生成新的函数名
    std::string newFunctionName(const std::string &base);

    // 获取变量的栈偏移
    int getVariableOffset(const std::string &name);

    // 分配新的栈偏移
    int allocateStack();

    // 计算总栈帧大小
    void calculateStackFrame();

    // 添加指令到当前序列
    void addInstruction(const std::string &instruction);

    // 解析LLVM IR指令
    void parseLLVMInstruction(const std::string &line);

    // 生成函数定义
    void generateFunctionDef(const std::string &funcName, const std::string &retType);

    // 生成函数结束
    void generateFunctionEnd();

    // 生成alloca指令
    void generateAlloca(const std::string &var, const std::string &type);

    // 生成store指令
    void generateStore(const std::string &value, const std::string &ptr);

    // 生成load指令
    std::string generateLoad(const std::string &ptr);

    // 生成分支指令
    void generateBranch(const std::string &cond, const std::string &trueLabel, const std::string &falseLabel);

    // 生成无条件跳转
    void generateJump(const std::string &label);

    // 生成标签
    void generateLabel(const std::string &label);

    // 生成函数调用
    void generateCall(const std::string &funcName, const std::vector<std::string> &args);

    // 生成返回指令
    void generateReturn(const std::string &value);

    // 解析LLVM IR中的立即数
    int parseImmediate(const std::string &imm);

    // 解析LLVM IR中的寄存器
    std::string parseRegister(const std::string &reg);

    // 重置函数状态
    void resetFunctionState();

    // 新的寄存器分配接口
    void initializeRegisterAllocator();
    void performRegisterAllocation(const std::string &llvmIR);
    void precomputeAllFunctionAllocations(const std::string &llvmIR);

    // 统一的虚拟寄存器处理接口
    std::string parseRegUse(const std::string &vreg);
    std::string parseRegDef(const std::string &vreg);
    std::string parseOperand(const std::string &operand); // 通用操作数解析

    // 辅助方法
    void initRegisterPool();
    void updateStackFrameAllocation();
};

// 生成RISC-V汇编的辅助函数
std::string generateRISCVAssembly(const std::string &llvmIR);