#include "include/riscv_gen.h"
#include <sstream>
#include <algorithm>
#include <regex>
#include <climits>
#include <iterator>

#pragma region 常量定义
// 正则表达式常量定义 - 用于解析LLVM IR指令
const std::string RISCVGenerator::REGEX_VAR_DEF = R"((%\d+)\s*=)";
const std::string RISCVGenerator::REGEX_VAR_USE = R"(%\d+)";
const std::string RISCVGenerator::REGEX_ALLOCA = R"((%\d+)\s*=\s*alloca\s+(\w+))";
const std::string RISCVGenerator::REGEX_STORE = R"(store\s+(\w+)\s+([^,]+),\s+ptr\s+([^,]+))";
const std::string RISCVGenerator::REGEX_LOAD = R"((%\d+)\s*=\s*load\s+(\w+),\s+ptr\s+([^,]+))";
const std::string RISCVGenerator::REGEX_CALL = R"((%\d+)\s*=\s*call\s+(\w+)\s+@(\w+)\s*\(([^)]*)\))";
const std::string RISCVGenerator::REGEX_ARITHMETIC = R"((%\d+)\s*=\s*(\w+)\s+nsw\s+(\w+)\s+([^,]+),\s+([^,]+))";
const std::string RISCVGenerator::REGEX_ICMP = R"((%\d+)\s*=\s*icmp\s+(\w+)\s+\w+\s+([^,]+),\s+([^,]+))";
const std::string RISCVGenerator::REGEX_BR_COND = R"(br\s+i1\s+([^,]+),\s+label\s+%([^,]+),\s+label\s+%([^,]+))";
const std::string RISCVGenerator::REGEX_BR_UNCOND = R"(br\s+label\s+%([^,]+))";
const std::string RISCVGenerator::REGEX_RET = R"(ret\s+(\w+)\s+([^,]+))";
const std::string RISCVGenerator::REGEX_LABEL = R"(([^:]+):)";
const std::string RISCVGenerator::REGEX_FUNCTION_DEF = R"(define\s+(?:dso_local\s+)?(\w+)\s+@(\w+)\s*\()";
const std::string RISCVGenerator::REGEX_I32_IMM = R"(i32\s+(\d+))";
const std::string RISCVGenerator::REGEX_I32_REG = R"(i32\s+(%\d+))";
#pragma endregion

#pragma region 简单工具函数
RISCVGenerator::RISCVGenerator()
    : tempCount(0), labelCount(0), stackOffset(0), totalStackSize(0),
      currentInstructions(""), hasReturn(false), isMainFunction(false), functionCount(0),
      instructionCount(0), preciseInstructionCount(0)
{
    initRegisterPool();
    initializeRegisterAllocator();
}

/**
 * @brief 初始化寄存器池
 * @details 按照固定优先级顺序初始化可用寄存器列表：
 * @note 寄存器池使用两个向量管理：freeRegisters存储寄存器名，isRegisterUsed标记使用状态
 */
void RISCVGenerator::initRegisterPool()
{
    // 这个函数现在主要用于初始化regInfo
    // 实际的寄存器管理由LinearScanAllocator负责
    // regInfo现在是只读的，不需要初始化
}

/**
 * @brief 生成新的临时变量名
 * @return std::string 新的临时变量名
 * @details 生成形如"t0", "t1", "t2"的临时变量名，计数器自动递增
 */
std::string RISCVGenerator::newTemp()
{
    return "t" + std::to_string(tempCount++);
}

/**
 * @brief 生成新的标签名
 * @param base 基础标签名
 * @return std::string 新的标签名
 */
std::string RISCVGenerator::newLabel(const std::string &base)
{
    return ".L" + base + "_" + std::to_string(labelCount++);
}

/**
 * @brief 生成新的函数名
 * @param base 基础函数名
 * @return std::string 新的函数名
 */
std::string RISCVGenerator::newFunctionName(const std::string &base)
{
    if (base == "main")
        return "main";
    return base;
}

/**
 * @brief 分配栈空间
 * @return int 分配的栈偏移量
 * @details 为局部变量分配栈空间，每次分配4字节（RISC-V32中int的大小）
 */
int RISCVGenerator::allocateStack()
{
    stackOffset -= 4; // RISC-V32中int是4字节
    return stackOffset;
}

/**
 * @brief 计算函数栈帧大小
 * @details 计算函数执行所需的栈空间，包括：
 * @note 该函数在函数结束时调用，用于生成正确的栈帧分配指令
 */
void RISCVGenerator::calculateStackFrame()
{
    int varSpace = variables.size() * 4;
    int saveSpace = 8;
    totalStackSize = varSpace + saveSpace;
    if (totalStackSize % 16 != 0)
        totalStackSize = ((totalStackSize / 16) + 1) * 16;
    updateStackFrameAllocation();
}

/**
 * @brief 更新栈帧分配和回收指令
 * @details 替换指令序列中的占位符为实际的栈帧操作
 * @note 该函数会重建整个指令序列
 */
void RISCVGenerator::updateStackFrameAllocation()
{
    // 安全地更新栈帧分配指令
    std::vector<std::string> lines;
    std::istringstream iss(currentInstructions);
    std::string line;

    while (std::getline(iss, line))
    {
        if (line.find("__STACK_FRAME_ALLOCATION_PLACEHOLDER__") != std::string::npos)
        {
            // 替换栈帧分配占位符
            lines.push_back("	addi	sp, sp, -" + std::to_string(totalStackSize));
            lines.push_back("	sw	ra, " + std::to_string(totalStackSize - 4) + "(sp)                      # 4-byte Folded Spill");
            lines.push_back("	sw	s0, " + std::to_string(totalStackSize - 8) + "(sp)                       # 4-byte Folded Spill");
            lines.push_back("	addi	s0, sp, " + std::to_string(totalStackSize));
        }
        else if (line.find("__STACK_FRAME_DEALLOCATION_PLACEHOLDER__") != std::string::npos)
        {
            // 替换栈帧回收占位符
            lines.push_back("	lw	ra, " + std::to_string(totalStackSize - 4) + "(sp)                      # 4-byte Folded Reload");
            lines.push_back("	lw	s0, " + std::to_string(totalStackSize - 8) + "(sp)                       # 4-byte Folded Reload");
            lines.push_back("	addi	sp, sp, " + std::to_string(totalStackSize));
        }
        else
        {
            lines.push_back(line);
        }
    }

    // 重建指令序列
    currentInstructions = "";
    for (const auto &l : lines)
        currentInstructions += l + "\n";
}

/**
 * @brief 向当前指令序列添加一条指令
 * @param instruction 要添加的RISC-V汇编指令
 * @details 将指令添加到currentInstructions缓冲区，用于构建函数的指令序列
 */
void RISCVGenerator::addInstruction(const std::string &instruction)
{
    currentInstructions += instruction + "\n";
}

/**
 * @brief 重置函数状态，准备处理新函数
 * @details 清空所有与当前函数相关的状态
 */
void RISCVGenerator::resetFunctionState()
{
    tempCount = 0;
    labelCount = 0;
    variables.clear();
    stackOffset = 0;
    totalStackSize = 0;
    breakLabels.clear();
    continueLabels.clear();
    currentInstructions = "";
    hasReturn = false;
    cmpMap.clear();
    // 重置指令计数器
    preciseInstructionCount = 0;
    // 重新初始化寄存器分配相关状态
    initRegisterPool();
    // 重新初始化新的寄存器分配器
    if (registerAllocator)
    {
        initializeRegisterAllocator();
    }
}

/**
 * @brief 生成函数定义
 * @details 生成函数的开始部分例行的汇编
 * @note 该函数在解析到函数定义时调用
 */
void RISCVGenerator::generateFunctionDef(const std::string &funcName, const std::string &retType)
{
    std::string funcLabel = isMainFunction ? "main" : newFunctionName(funcName);
    currentFunction = funcLabel;
    addInstruction(funcLabel + ":                                   # @" + funcName);
    addInstruction("# %bb.0:");
    addInstruction("	__STACK_FRAME_ALLOCATION_PLACEHOLDER__");

    // 处理被调用者保护的寄存器
    auto it = functionAllocators.find(currentFunction);
    if (it != functionAllocators.end())
    {
        // 获取需要保护的被调用者保存寄存器
        std::set<int> calleeSavedToProtect = it->second->getCalleeSavedRegs();

        // 生成保护指令
        int offset = 0;
        for (int regId : calleeSavedToProtect)
        {
            const auto &reg = regInfo.getReg(regId);
            addInstruction("	sw	" + reg.name + ", " + std::to_string(offset) + "(sp)");
            offset += 4;
        }
    }
}

/**
 * @brief 生成函数结束部分
 * @note 该函数在解析到函数结束时调用
 */
void RISCVGenerator::generateFunctionEnd()
{
    if (!hasReturn)
    {
        if (isMainFunction)
        {
            addInstruction("	li	a0, 0"); // main函数默认返回0
        }
    }

    // 恢复被调用者保护的寄存器
    auto it = functionAllocators.find(currentFunction);
    if (it != functionAllocators.end())
    {
        // 获取需要恢复的被调用者保存寄存器
        std::set<int> calleeSavedToRestore = it->second->getCalleeSavedRegs();

        // 恢复被调用者保存寄存器
        int offset = 0;
        for (int regId : calleeSavedToRestore)
        {
            const auto &reg = regInfo.getReg(regId);
            addInstruction("	lw	" + reg.name + ", " + std::to_string(offset) + "(sp)");
            offset += 4;
        }
    }

    addInstruction("                                        # -- End function");
}
#pragma endregion

#pragma region 代码生成器主函数
/**
 * @brief 生成RISC-V汇编代码的主入口函数
 * @param llvmIR 输入的LLVM IR代码字符串
 * @return std::string 生成的RISC-V汇编代码
 * @details 该函数是代码生成器的主要入口点，执行以下步骤：
 *          - 构建控制流图(CFG)
 *          - 执行数据流分析（USE/DEF/IN/OUT集合）
 *          - 构建活跃区间
 *          - 解析LLVM IR并生成对应的RISC-V汇编
 *          - 处理函数定义、标签、指令等
 * @note 线性扫描寄存器分配采用按需分配策略
 */
std::string RISCVGenerator::generateModule(const std::string &llvmIR)
{
    std::string assembly = "";

    // 提前计算所有函数的寄存器分配结果
    precomputeAllFunctionAllocations(llvmIR);

    assembly += "	.globl	main                            # -- Begin function main\n";

    std::istringstream iss(llvmIR);
    std::string line;

    while (std::getline(iss, line))
    {
        // 跳过注释和空行
        if (line.empty() || line[0] == ';' || line[0] == '!' || line.find("===") != std::string::npos)
        {
            continue;
        }

        // 解析函数定义
        if (line.find("define") != std::string::npos)
        {
            // 解析函数名和返回类型
            std::regex funcRegex(REGEX_FUNCTION_DEF);
            std::smatch match;
            if (std::regex_search(line, match, funcRegex))
            {
                std::string retType = match[1].str();
                std::string funcName = match[2].str();

                // 重置状态
                resetFunctionState();
                isMainFunction = (funcName == "main");

                generateFunctionDef(funcName, retType);
            }
        }
        // 解析标签
        else if (line.find(":") != std::string::npos)
        {
            std::regex labelRegex(REGEX_LABEL);
            std::smatch match;
            if (std::regex_search(line, match, labelRegex))
            {
                std::string label = match[1].str();
                // 清理标签名（去除前导空格）
                label.erase(0, label.find_first_not_of(" \t"));
                generateLabel(label);
            }
        }
        // 解析指令
        else if (!line.empty() && line[0] == ' ')
        {
            parseLLVMInstruction(line);
        }
        // 函数结束
        else if (line == "}")
        {
            generateFunctionEnd();
            calculateStackFrame();
            assembly += currentInstructions;
            functionCount++;
        }
    }

    return assembly;
}
#pragma endregion

#pragma region 所有指令各自对应的解析函数
/**
 * @brief 解析LLVM IR指令并生成对应的RISC-V汇编
 * @param line 单行LLVM IR指令
 * @details 该函数是LLVM IR到RISC-V汇编转换的核心，支持以下指令类型：
 *          - alloca: 局部变量分配
 *          - store: 内存存储
 *          - load: 内存加载
 *          - call: 函数调用
 *          - 算术运算: add, sub, mul, sdiv等
 *          - 比较运算: icmp
 *          - 分支指令: br
 *          - 返回指令: ret
 * @note 该函数会更新指令计数器，用于寄存器分配
 */
void RISCVGenerator::parseLLVMInstruction(const std::string &line)
{
    // 更新指令计数器 - 使用与数据流分析一致的计数
    preciseInstructionCount++;

    // 解析alloca指令
    if (line.find("alloca") != std::string::npos)
    {
        std::regex allocaRegex(REGEX_ALLOCA);
        std::smatch match;
        if (std::regex_search(line, match, allocaRegex))
        {
            std::string var = match[1].str();
            std::string type = match[2].str();
            generateAlloca(var, type);
        }
    }
    // 解析store指令
    else if (line.find("store") != std::string::npos)
    {
        std::regex storeRegex(REGEX_STORE);
        std::smatch match;
        if (std::regex_search(line, match, storeRegex))
        {
            std::string type = match[1].str();
            std::string value = match[2].str();
            std::string ptr = match[3].str();
            generateStore(value, ptr);
        }
    }
    // 解析load指令
    else if (line.find("load") != std::string::npos)
    {
        std::regex loadRegex(REGEX_LOAD);
        std::smatch match;
        if (std::regex_search(line, match, loadRegex))
        {
            std::string result = match[1].str();
            std::string type = match[2].str();
            std::string ptr = match[3].str();

            std::string resultReg = parseRegDef(result);
            int offset = getVariableOffset(ptr);
            if (offset != -1)
            {
                addInstruction("	lw	" + resultReg + ", " + std::to_string(offset) + "(s0)");
            }
        }
    }
    // 解析函数调用
    else if (line.find("call") != std::string::npos)
    {
        std::regex callRegex(REGEX_CALL);
        std::smatch match;
        if (std::regex_search(line, match, callRegex))
        {
            std::string result = match[1].str();
            std::string retType = match[2].str();
            std::string funcName = match[3].str();
            std::string argsStr = match[4].str();

            // 解析参数
            std::vector<std::string> args;
            if (!argsStr.empty())
            {
                std::istringstream argsStream(argsStr);
                std::string arg;
                while (std::getline(argsStream, arg, ','))
                {
                    // 去两端空白
                    arg.erase(0, arg.find_first_not_of(" \t"));
                    arg.erase(arg.find_last_not_of(" \t") + 1);

                    if (arg.empty())
                        continue;

                    // 只保留最后一个空格之后的部分（去掉 "i32", "noundef" 等）
                    auto pos = arg.find_last_of(' ');
                    if (pos != std::string::npos)
                    {
                        arg = arg.substr(pos + 1);
                    }
                    // 现在 arg 应该是 "3" 或 "%reg" 之类的
                    args.push_back(arg);
                }
            }

            generateCall(funcName, args);
            if (result != "%void")
            {
                std::string destReg = parseRegDef(result);
                addInstruction("	mv	" + destReg + ", a0");
            }
        }
    }
    // 解析算术运算
    else if (line.find("add") != std::string::npos ||
             line.find("sub") != std::string::npos ||
             line.find("mul") != std::string::npos ||
             line.find("sdiv") != std::string::npos ||
             line.find("srem") != std::string::npos)
    {
        std::regex arithRegex(REGEX_ARITHMETIC);
        std::smatch match;
        if (std::regex_search(line, match, arithRegex))
        {
            std::string result = match[1].str();
            std::string op = match[2].str();
            std::string type = match[3].str();
            std::string lhs = match[4].str();
            std::string rhs = match[5].str();

            std::string resultReg = parseRegDef(result);
            std::string lhsReg = parseOperand(lhs);
            // 如果是 add/sub 且 RHS 是立即数，就用 addi
            bool rhsIsImm = (rhs.find_first_not_of("0123456789-") == std::string::npos);
            if (op == "add" && rhsIsImm)
            {
                int imm = parseImmediate(rhs);
                addInstruction("	addi\t" + resultReg + ", " + lhsReg + ", " + std::to_string(imm));
            }
            else if (op == "sub" && rhsIsImm)
            {
                int imm = parseImmediate(rhs);
                // sub lhs, imm  等价于 addi lhs, -imm
                addInstruction("	addi\t" + resultReg + ", " + lhsReg + ", " + std::to_string(-imm));
            }
            else
            {
                // 普通二元运算，RHS 可能是寄存器或临时常量
                std::string rhsReg = parseOperand(rhs);
                if (op == "add")
                    addInstruction("	add	" + resultReg + ", " + lhsReg + ", " + rhsReg);
                else if (op == "sub")
                    addInstruction("	sub	" + resultReg + ", " + lhsReg + ", " + rhsReg);
                else if (op == "mul")
                    addInstruction("	mul	" + resultReg + ", " + lhsReg + ", " + rhsReg);
                else if (op == "sdiv")
                    addInstruction("	div	" + resultReg + ", " + lhsReg + ", " + rhsReg);
                else if (op == "srem")
                    addInstruction("	rem	" + resultReg + ", " + lhsReg + ", " + rhsReg);
            }
        }
    }
    // 解析比较运算
    else if (line.find("icmp") != std::string::npos)
    {
        // 匹配 "%5 = icmp sgt i32 %3, 2" 之类
        std::regex cmpRegex(REGEX_ICMP);
        std::smatch match;
        if (std::regex_search(line, match, cmpRegex))
        {
            std::string result = match[1].str(); // 比较结果 %5
            std::string op = match[2].str();     // 比较类型 sgt,slt,eq...
            std::string lhs = match[3].str();    // 左操作数
            std::string rhs = match[4].str();    // 右操作数

            std::string resultReg = parseRegDef(result);
            std::string lhsReg = parseOperand(lhs);
            std::string rhsReg = parseOperand(rhs);
            // 存入 map，留到后面 br i1 时使用
            cmpMap[result] = {op, lhsReg, rhsReg};
        }
    }
    // 解析带条件的分支
    else if (line.find("br i1") != std::string::npos)
    {
        // 匹配 "br i1 %5, label %then_0, label %else_0"
        std::regex brRegex(REGEX_BR_COND);
        std::smatch match;
        if (std::regex_search(line, match, brRegex))
        {
            std::string cond = match[1].str(); // %5
            std::string trueLabel = match[2].str();
            std::string falseLabel = match[3].str();

            auto it = cmpMap.find(cond);
            if (it != cmpMap.end())
            {
                // 根据不同的 icmp 类型选用 RISC-V 分支助记符
                auto &ci = it->second;
                if (ci.op == "slt")
                    addInstruction("	blt " + ci.lhsReg + ", " + ci.rhsReg + ", .LBB0_" + trueLabel);
                else if (ci.op == "sgt")
                    addInstruction("	bgt " + ci.lhsReg + ", " + ci.rhsReg + ", .LBB0_" + trueLabel);
                else if (ci.op == "eq")
                    addInstruction("	beq " + ci.lhsReg + ", " + ci.rhsReg + ", .LBB0_" + trueLabel);
                else if (ci.op == "ne")
                    addInstruction("	bne " + ci.lhsReg + ", " + ci.rhsReg + ", .LBB0_" + trueLabel);
                else if (ci.op == "sle")
                    addInstruction("	ble " + ci.lhsReg + ", " + ci.rhsReg + ", .LBB0_" + trueLabel);
                else if (ci.op == "sge")
                    addInstruction("	bge " + ci.lhsReg + ", " + ci.rhsReg + ", .LBB0_" + trueLabel);
                // 跳到 false 分支
                addInstruction("	j .LBB0_" + falseLabel);
                cmpMap.erase(it);
            }
            else
            {
                // 回退到默认的 bnez + j
                generateBranch(cond, trueLabel, falseLabel);
            }
        }
    }
    // 解析无条件跳转
    else if (line.find("br label") != std::string::npos)
    {
        std::regex jumpRegex(REGEX_BR_UNCOND);
        std::smatch match;
        if (std::regex_search(line, match, jumpRegex))
        {
            std::string label = match[1].str();
            generateJump(label);
        }
    }
    // 解析返回指令
    else if (line.find("ret") != std::string::npos)
    {
        std::regex retRegex(REGEX_RET);
        std::smatch match;
        if (std::regex_search(line, match, retRegex))
        {
            std::string type = match[1].str();
            std::string value = match[2].str();
            generateReturn(value);
        }
        else
        {
            generateReturn("0"); // void return
        }
    }
}

/**
 * @brief 生成alloca指令对应的汇编代码
 * @param std::string 变量名
 * @param std::string 变量类型
 * @details 为局部变量分配栈空间：
 *          - 调用allocateStack()分配栈空间
 *          - 将变量名和栈偏移量记录到variables映射中
 */
void RISCVGenerator::generateAlloca(const std::string &var, const std::string &type)
{
    int offset = allocateStack();
    variables[var] = offset;
}

/**
 * @brief 获取变量的栈偏移量
 * @param ptr 指向变量的指针
 * @return int 变量在栈中的偏移量，如果未找到则返回-1
 * @details 从LLVM IR指针中提取变量名，查找其在栈中的位置：
 *          - 使用正则表达式提取变量编号
 *          - 在variables映射中查找对应的栈偏移量
 * @note 变量从-12开始，每个变量4字节
 */
int RISCVGenerator::getVariableOffset(const std::string &ptr)
{
    // 从ptr中提取变量名
    std::regex varRegex(R"(%(\d+))");
    std::smatch match;
    if (std::regex_search(ptr, match, varRegex))
    {
        std::string varName = "%" + match[1].str();
        auto it = variables.find(varName);
        if (it != variables.end())
        {
            // 使用相对于s0的偏移
            // s0指向栈帧顶部，局部变量在s0下方的安全区域
            // 需要避开保存ra和s0的8字节空间
            return it->second - 8; // 变量偏移减去保存寄存器的空间
        }
    }
    return -1;
}

/**
 * @brief 生成store指令对应的汇编代码
 * @param value 要存储的值
 * @param ptr 目标指针
 * @details 生成内存存储指令：
 *          - 解析value和ptr参数
 *          - 生成sw指令将值存储到指定内存位置
 *          - 处理临时寄存器的释放
 * @note 该函数在解析store指令时调用
 */
void RISCVGenerator::generateStore(const std::string &value, const std::string &ptr)
{
    int offset = getVariableOffset(ptr);
    if (offset == -1)
        return;

    // value 本身就是 "%0", "%1", "%3" 这类 IR 变量，或者是立即数/复杂表达式
    std::string valueReg = parseOperand(value);
    addInstruction("	sw	" + valueReg + ", " + std::to_string(offset) + "(s0)");
}

/**
 * @brief 生成load指令对应的汇编代码
 * @param ptr 源指针
 * @return std::string 加载值的寄存器名
 * @details 生成内存加载指令：
 *          - 获取变量的栈偏移量
 *          - 分配临时寄存器存储加载的值
 *          - 生成lw指令从内存加载值到寄存器
 * @note 该函数在解析load指令时调用

 */
std::string RISCVGenerator::generateLoad(const std::string &ptr)
{
    int offset = getVariableOffset(ptr);
    if (offset != -1)
    {
        // 为临时结果使用溢出临时寄存器
        auto allocator = functionAllocators[currentFunction].get();
        int tempReg = allocator->allocateSpillTempReg();
        if (tempReg != -1)
        {
            std::string tempRegName = regInfo.getReg(tempReg).name;
            addInstruction("	lw	" + tempRegName + ", " + std::to_string(offset) + "(s0)");
            return tempRegName;
        }
        return "t0"; // 回退，但这应该不会发生
    }
    return parseOperand(ptr);
}

/**
 * @brief 生成比较运算指令对应的汇编代码
 * @param op 比较操作符（eq, ne, slt, sle, sgt, sge）
 * @param std::string 左操作数
 * @param std::string 右操作数
 * @return std::string 比较结果的寄存器名
 * @details 生成RISC-V比较运算指令：
 *          - 解析左右操作数到寄存器
 *          - 分配结果寄存器
 *          - 根据比较操作符生成对应的RISC-V指令序列
 * @note 该函数在解析icmp指令时调用
 */

/**
 * @brief 生成条件分支指令对应的汇编代码
 * @param std::string 条件表达式
 * @param std::string 条件为真时的跳转标签
 * @param std::string 条件为假时的跳转标签
 * @details 生成条件分支指令：
 *          - 解析条件表达式到寄存器
 *          - 生成bnez指令进行条件跳转
 *          - 生成无条件跳转到false分支
 * @note 该函数在解析条件分支指令时调用

 */
void RISCVGenerator::generateBranch(const std::string &cond, const std::string &trueLabel, const std::string &falseLabel)
{
    std::string condReg = parseOperand(cond);
    addInstruction("	bnez	" + condReg + ", .LBB0_" + trueLabel);
    addInstruction("	j	.LBB0_" + falseLabel);
}

/**
 * @brief 生成无条件跳转指令对应的汇编代码
 * @param std::string 跳转目标标签
 * @details 生成无条件跳转指令j
 * @note 该函数在解析无条件分支指令时调用

 */
void RISCVGenerator::generateJump(const std::string &label)
{
    addInstruction("	j	.LBB0_" + label);
}

/**
 * @brief 生成标签指令对应的汇编代码
 * @param std::string 标签名
 * @details 生成汇编标签，用于标记代码位置
 * @note 该函数在解析标签指令时调用

 */
void RISCVGenerator::generateLabel(const std::string &label)
{
    addInstruction(".LBB0_" + label + ":");
}

/**
 * @brief 生成函数调用指令对应的汇编代码
 * @param std::string 函数名
 * @param std::vector<std::string> 参数列表
 * @details 生成函数调用指令：
 *          - 处理函数参数传递
 *          - 生成call指令
 *          - 处理返回值（如果有）
 * @note 该函数在解析call指令时调用

 */
void RISCVGenerator::generateCall(const std::string &funcName, const std::vector<std::string> &args)
{
    // 保存调用者保护的寄存器（t和a寄存器）
    std::set<std::string> callerSavedRegs;

    // 计算需要保护的调用者保存寄存器
    std::set<int> currentUsedRegs;
    std::set<int> calledFuncUsedRegs;
    std::set<int> callerSavedRegIds;

    // 收集当前函数使用的寄存器
    auto currentIt = functionAllocators.find(currentFunction);
    if (currentIt != functionAllocators.end())
    {
        currentUsedRegs = currentIt->second->getUsedPhysRegs();
    }

    // 收集被调用函数使用的寄存器
    auto calledIt = functionAllocators.find(funcName);
    if (calledIt != functionAllocators.end())
    {
        calledFuncUsedRegs = calledIt->second->getUsedPhysRegs();
    }

    // 计算调用者保存寄存器集合
    for (const auto &physReg : regInfo.physRegs)
    {
        if (physReg.callerSaved)
        {
            callerSavedRegIds.insert(physReg.id);
        }
    }

    // 计算三个集合的交集：当前使用的 ∩ 调用者保存 ∩ 被调用函数使用
    for (int regId : currentUsedRegs)
    {
        if (callerSavedRegIds.count(regId) &&
            (calledFuncUsedRegs.empty() || calledFuncUsedRegs.count(regId)))
        {
            callerSavedRegs.insert(regInfo.getReg(regId).name);
        }
    }

    // 保存调用者保护的寄存器
    int offset = 0;
    for (const auto &reg : callerSavedRegs)
    {
        addInstruction("	sw	" + reg + ", " + std::to_string(offset) + "(sp)");
        offset += 4;
    }

    // 传递参数
    for (size_t i = 0; i < args.size() && i < 8; ++i)
    {
        // 先生成把 args[i] 载入某个寄存器（可能是 li，也可能是已有的 %n 寄存器）
        std::string argReg = parseOperand(args[i]);
        std::string dest = "a" + std::to_string(i);

        // 如果它已经就在 a{i} 上，就不用再写 mv/ li 了
        if (argReg != dest)
        {
            // 统一用 mv，而不是 li，避免出现 "li a0, a0"
            addInstruction("	mv	" + dest + ", " + argReg);
        }
    }

    // 调用函数
    addInstruction("	call	" + funcName);

    // 恢复调用者保护的寄存器
    offset = 0;
    for (const auto &reg : callerSavedRegs)
    {
        addInstruction("	lw	" + reg + ", " + std::to_string(offset) + "(sp)");
        offset += 4;
    }
}

/**
 * @brief 生成返回指令
 * @param value 返回值
 * @details 生成函数的返回指令：
 *          - 如果返回值不是0，将其移动到a0寄存器
 *          - 标记函数已有返回语句
 * @note 该函数在解析到ret指令时调用

 */
void RISCVGenerator::generateReturn(const std::string &value)
{
    if (value != "0")
    {
        std::string valueReg = parseOperand(value);
        addInstruction("	mv	a0, " + valueReg);
        addInstruction("__STACK_FRAME_DEALLOCATION_PLACEHOLDER__");
        addInstruction("	ret");
    }
    hasReturn = true;
}

/**
 * @brief 解析立即数
 * @param imm 立即数字符串
 * @return int 解析出的整数值
 * @details 将字符串转换为整数，如果转换失败则返回0
 * @note 该函数用于处理LLVM IR中的立即数常量

 */
int RISCVGenerator::parseImmediate(const std::string &imm)
{
    try
    {
        return std::stoi(imm);
    }
    catch (...)
    {
        return -999;
    }
}

/**
 * @brief 解析寄存器或立即数
 * @param std::string 寄存器或立即数的字符串表示
 * @return std::string 对应的RISC-V寄存器名或临时寄存器
 * @details 处理多种格式的寄存器表示：
 *          - LLVM寄存器（%数字）：使用线性扫描分配
 *          - 纯数字：作为立即数处理
 *          - i32格式：提取其中的立即数或寄存器
 * @note 该函数是寄存器分配的重要接口

 */

#pragma endregion

/**
 * @brief 初始化新的寄存器分配器
 */
void RISCVGenerator::initializeRegisterAllocator()
{
    registerAllocator = std::unique_ptr<ra_ls::LinearScanAllocator>(new ra_ls::LinearScanAllocator(regInfo));
}

/**
 * @brief 执行寄存器分配
 * @param llvmIR LLVM IR代码
 */
void RISCVGenerator::performRegisterAllocation(const std::string &llvmIR)
{
    // 解析LLVM IR构建函数IR
    currentFunctionIR = ra_ls::parseFunctionFromLLVMIR(llvmIR, currentFunction);

    if (currentFunctionIR && registerAllocator)
    {
        // 执行寄存器分配，结果由分配器内部管理
        registerAllocator->allocate(*currentFunctionIR);
    }
}

/**
 * @brief 提前计算所有函数的寄存器分配结果
 * @param llvmIR LLVM IR代码
 */
void RISCVGenerator::precomputeAllFunctionAllocations(const std::string &llvmIR)
{
    std::istringstream iss(llvmIR);
    std::string line;

    while (std::getline(iss, line))
    {
        // 查找函数定义
        if (line.find("define") != std::string::npos)
        {
            std::regex funcRegex(REGEX_FUNCTION_DEF);
            std::smatch match;
            if (std::regex_search(line, match, funcRegex))
            {
                std::string funcName = match[2].str();

                // 为每个函数创建独立的分配器实例
                auto allocator = std::make_unique<ra_ls::LinearScanAllocator>(regInfo);
                auto functionIR = ra_ls::parseFunctionFromLLVMIR(llvmIR, funcName);

                if (functionIR)
                {
                    // 执行寄存器分配，结果由分配器内部管理
                    // 分配器会自动处理参数映射和瞬时位置计算
                    allocator->allocate(*functionIR);

                    // 存储分配器实例，这样可以随时查询分配结果
                    functionAllocators[funcName] = std::move(allocator);
                }
            }
        }
    }
}

/**
 * @brief 统一的虚拟寄存器使用处理接口
 * @param vreg 虚拟寄存器名称
 * @return 对应的物理寄存器名或临时寄存器
 */
std::string RISCVGenerator::parseRegUse(const std::string &vreg)
{
    if (vreg[0] != '%')
    {
        throw std::runtime_error("parseRegUse: not a virtual register: " + vreg);
    }

    // 提取虚拟寄存器编号
    int vregId = std::stoi(vreg.substr(1));

    // 从当前函数的分配器中查找分配结果
    auto it = functionAllocators.find(currentFunction);
    if (it != functionAllocators.end())
    {
        const auto &result = it->second->getAllocationResult();

        // 首先检查是否是参数寄存器
        auto paramIt = result.paramVregToLocation.find(vregId);
        if (paramIt != result.paramVregToLocation.end())
        {
            if (paramIt->second >= 0)
            {
                // 参数在物理寄存器中
                return regInfo.getReg(paramIt->second).name;
            }
            else
            {
                // 参数在栈中
                int tempReg = it->second->allocateSpillTempReg();
                if (tempReg != -1)
                {
                    std::string tempRegName = regInfo.getReg(tempReg).name;
                    int stackOffset = paramIt->second; // 直接使用参数栈偏移
                    addInstruction("	lw	" + tempRegName + ", " + std::to_string(stackOffset) + "(sp)");
                    return tempRegName;
                }
            }
        }

        // 检查是否分配到物理寄存器
        auto physIt = result.vregToPhys.find(vregId);
        if (physIt != result.vregToPhys.end() && physIt->second != -1)
        {
            return regInfo.getReg(physIt->second).name;
        }

        // 检查是否溢出到栈
        auto stackIt = result.vregToStack.find(vregId);
        if (stackIt != result.vregToStack.end())
        {
            // 需要从栈加载到溢出临时寄存器
            auto allocator = functionAllocators[currentFunction].get();
            int tempReg = allocator->allocateSpillTempReg();
            if (tempReg != -1)
            {
                std::string tempRegName = regInfo.getReg(tempReg).name;
                int stackOffset = stackIt->second; // 直接使用字节偏移
                addInstruction("	lw	" + tempRegName + ", " + std::to_string(stackOffset) + "(sp)");
                return tempRegName;
            }
        }
    }

    // 回退到原来的逻辑
    throw std::runtime_error("parseRegUse: not a virtual register");
}

/**
 * @brief 解析操作数，可以是虚拟寄存器、立即数或其他格式
 * @param operand 操作数字符串
 * @return 对应的寄存器名
 */
std::string RISCVGenerator::parseOperand(const std::string &operand)
{
    if (operand[0] == '%')
    {
        // 虚拟寄存器，直接调用parseRegUse
        return parseRegUse(operand);
    }
    else if (operand.find_first_not_of("0123456789-") == std::string::npos)
    {
        // 立即数 - 使用溢出临时寄存器
        int imm = parseImmediate(operand);
        auto allocator = functionAllocators[currentFunction].get();
        int tempReg = allocator->allocateSpillTempReg();
        if (tempReg != -1)
        {
            std::string temp = regInfo.getReg(tempReg).name;
            addInstruction("	li	" + temp + ", " + std::to_string(imm));
            return temp;
        }
        return "t0"; // 回退
    }
    else if (operand.find("i32") != std::string::npos)
    {
        // 处理 "i32 1" 这样的格式
        std::regex immRegex(R"(i32\s+(-?\d+))");
        std::smatch match;
        if (std::regex_search(operand, match, immRegex))
        {
            int imm = std::stoi(match[1].str());
            auto allocator = functionAllocators[currentFunction].get();
            int tempReg = allocator->allocateSpillTempReg();
            if (tempReg != -1)
            {
                std::string temp = regInfo.getReg(tempReg).name;
                addInstruction("	li	" + temp + ", " + std::to_string(imm));
                return temp;
            }
            return "t0"; // 回退
        }
        // 处理 "i32 %4" 这样的格式
        std::regex regRegex(R"(i32\s+(%\d+))");
        if (std::regex_search(operand, match, regRegex))
        {
            std::string llvmReg = match[1].str();
            return parseRegUse(llvmReg);
        }
    }
    else if (operand.find("ptr") != std::string::npos)
    {
        // 处理 "ptr %1" 这样的格式
        std::regex ptrRegex(R"(ptr\s+(%\d+))");
        std::smatch match;
        if (std::regex_search(operand, match, ptrRegex))
        {
            std::string llvmReg = match[1].str();
            return parseRegUse(llvmReg);
        }
    }

    // 如果都不匹配，返回原字符串（可能是寄存器名）
    return operand;
}

/**
 * @brief 统一的虚拟寄存器定义处理接口
 * @param vreg 虚拟寄存器名称
 * @return 对应的物理寄存器名
 */
std::string RISCVGenerator::parseRegDef(const std::string &vreg)
{
    if (vreg[0] != '%')
    {
        throw std::runtime_error("parseRegDef: not a virtual register: " + vreg);
    }

    // 提取虚拟寄存器编号
    int vregId = std::stoi(vreg.substr(1));

    // 从当前函数的分配器中查找分配结果
    auto it = functionAllocators.find(currentFunction);
    if (it != functionAllocators.end())
    {
        const auto &result = it->second->getAllocationResult();

        // 检查是否分配到物理寄存器
        auto physIt = result.vregToPhys.find(vregId);
        if (physIt != result.vregToPhys.end() && physIt->second != -1)
        {
            return regInfo.getReg(physIt->second).name;
        }

        // 检查是否需要溢出到栈
        auto stackIt = result.vregToStack.find(vregId);
        if (stackIt != result.vregToStack.end())
        {
            // 需要溢出临时寄存器存储结果，然后存储到栈
            auto allocator = functionAllocators[currentFunction].get();
            int tempReg = allocator->allocateSpillTempReg();
            if (tempReg != -1)
            {
                std::string tempRegName = regInfo.getReg(tempReg).name;
                // 注意：这里返回临时寄存器，调用者需要在使用后将结果存储到栈
                return tempRegName;
            }
        }
    }

    // 回退到原来的逻辑
    throw std::runtime_error("parseRegDef: not a virtual register");
}

#pragma region 对外接口
/**
 * @brief 生成RISC-V汇编代码的全局函数
 * @param llvmIR 输入的LLVM IR代码字符串
 * @return std::string 生成的RISC-V汇编代码
 * @details 该函数是代码生成器的全局入口点：
 *          - 创建RISCVGenerator实例
 *          - 调用generateModule生成汇编代码
 *          - 返回完整的RISC-V汇编字符串
 * @note 该函数是外部调用的主要接口
 */
std::string generateRISCVAssembly(const std::string &llvmIR)
{
    RISCVGenerator generator;
    return generator.generateModule(llvmIR);
}
#pragma endregion