#include "include/llvm_ir.h"
#include <sstream>
#include <algorithm>

LLVMIRGenerator::LLVMIRGenerator()
    : tempCount(0), labelCount(0), stackOffset(0), currentInstructions(""), hasReturn(false), varCount(0), isMainFunction(false)
{
    // 初始化全局作用域
    enterScope();
}

// 作用域管理方法实现
void LLVMIRGenerator::enterScope()
{
    scopeStack.push_back(std::map<std::string, std::string>());
}

void LLVMIRGenerator::exitScope()
{
    if (!scopeStack.empty())
    {
        scopeStack.pop_back();
    }
}

void LLVMIRGenerator::addVariable(const std::string &name, const std::string &varId)
{
    if (!scopeStack.empty())
    {
        scopeStack.back()[name] = varId;
    }
}

std::string LLVMIRGenerator::findVariable(const std::string &name)
{
    // 从当前作用域开始，向外层查找变量
    for (int i = scopeStack.size() - 1; i >= 0; --i)
    {
        auto it = scopeStack[i].find(name);
        if (it != scopeStack[i].end())
        {
            return it->second;
        }
    }
    return ""; // 变量未找到
}

std::string LLVMIRGenerator::newTemp()
{
    return "%" + std::to_string(++varCount);
}

std::string LLVMIRGenerator::newLabel(const std::string &base)
{
    return base + "_" + std::to_string(labelCount);
}

std::string LLVMIRGenerator::newVar()
{
    return std::to_string(++varCount);
}

std::string LLVMIRGenerator::getVariableOffset(const std::string &name)
{
    return findVariable(name);
}

int LLVMIRGenerator::allocateStack()
{
    return stackOffset += 4; // 每个int变量占用4字节
}

void LLVMIRGenerator::addInstruction(const std::string &instruction, bool isLabel)
{
    if (isLabel)
    {
        currentInstructions += instruction + "\n";
    }
    else
    {
        currentInstructions += "  " + instruction + "\n";
    }
}

std::string LLVMIRGenerator::generateParams(const std::vector<Param> &params)
{
    std::string result;
    for (size_t i = 0; i < params.size(); ++i)
    {
        if (i > 0)
            result += ", ";
        result += "i32 noundef %" + params[i].name;
    }
    return result;
}

std::string LLVMIRGenerator::generateFunction(const std::shared_ptr<FuncDef> &funcDef)
{
    // 重置状态
    tempCount = 0;
    labelCount = 0;
    varCount = funcDef->params.size();
    scopeStack.clear();
    enterScope(); // 创建函数作用域
    loadedValues.clear();
    stackOffset = 0;
    breakLabels.clear();
    continueLabels.clear();
    currentFunction = funcDef->name;
    currentInstructions = "";
    hasReturn = false;
    isMainFunction = (funcDef->name == "main");
    allocas.clear();
    initStores.clear();

    std::string ir;

    // 先把原始参数名存下来
    std::vector<std::string> origNames;
    for (auto &p : funcDef->params)
        origNames.push_back(p.name);
    // 把 AST 里原本的参数名（如 "a","b"）统一改成 "0","1",…
    for (size_t i = 0; i < funcDef->params.size(); ++i)
    {
        funcDef->params[i].name = std::to_string(i);
    }

    // 函数声明
    std::string retType = (funcDef->retType == "void") ? "void" : "i32";
    // std::string funcAttrs = isMainFunction ? "dso_local" : "";
    ir += std::string("define ") + "dso_local" + " " + retType + " @" + funcDef->name + "(" + generateParams(funcDef->params) + ") #0 {\n";

    blockLabels.push_back("0");

    // 1) main 的返回值变量
    if (isMainFunction)
    {
        std::string retVar = newVar();
        addVariable(funcDef->name + "_ret", retVar);
        allocas.push_back("%" + retVar + " = alloca i32, align 4");
        initStores.push_back("store i32 0, ptr %" + retVar + ", align 4");
    }

    // 2) 参数的 alloca + store
    for (size_t i = 0; i < funcDef->params.size(); ++i)
    {
        std::string irName = funcDef->params[i].name;
        std::string orig = origNames[i];
        std::string slot = newVar();

        allocas.push_back("%" + slot + " = alloca i32, align 4");
        initStores.push_back("store i32 %" + irName + ", ptr %" + slot + ", align 4");

        // 建立 IR 名称 -> alloca 插槽 的映射
        addVariable(irName, slot);
        addVariable(orig, slot);
    }

    // 3) 预分配所有局部变量的 alloca
    // 注意：现在变量分配在运行时进行，所以这里不需要预分配

    // 一次性输出所有 alloca
    for (auto &line : allocas)
    {
        addInstruction(line);
    }
    // 再一次性输出所有初始化的 store
    for (auto &line : initStores)
    {
        addInstruction(line);
    }

    // 生成函数体
    generateBlock(funcDef->body);

    // 添加生成的指令
    ir += currentInstructions;

    blockLabels.pop_back(); // 清理 entry

    // 如果函数没有显式返回语句，添加默认返回
    if (!hasReturn)
    {
        if (funcDef->retType == "int")
            ir += "  ret i32 0\n";
        else if (funcDef->retType == "void")
            ir += "  ret void\n";
    }
    ir += "}\n\n";
    return ir;
}

std::string LLVMIRGenerator::generateModule(const std::vector<std::shared_ptr<FuncDef>> &funcs)
{
    std::string ir = "; ModuleID = 'toyc'\n";
    ir += "source_filename = \"toyc\"\n";
    ir += "target triple = \"riscv32-unknown-elf\"\n\n\n";

    for (const auto &func : funcs)
    {
        ir += generateFunction(func);
    }

    // 添加函数属性
    if (isMainFunction)
    {
        // ir += "\nattributes #0 = { noinline nounwind optnone uwtable \"min-legal-vector-width\"=\"0\" \"no-trapping-math\"=\"true\" \"stack-protector-buffer-size\"=\"8\" \"target-cpu\"=\"x86-64\" \"target-features\"=\"+cx8,+fxsr,+mmx,+sse,+sse2,+x87\" \"tune-cpu\"=\"generic\" }\n\n";
    }

    return ir;
}

std::string LLVMIRGenerator::generateExpr(const ASTPtr &expr)
{
    if (auto numExpr = std::dynamic_pointer_cast<NumberExpr>(expr))
    {
        return std::to_string(numExpr->value);
    }

    if (auto idExpr = std::dynamic_pointer_cast<IdentifierExpr>(expr))
    {
        std::string varName = getVariableOffset(idExpr->name);
        if (!varName.empty())
        {
            // 检查是否已经加载过这个变量
            auto it = loadedValues.find(idExpr->name);
            if (it != loadedValues.end())
            {
                return it->second; // 返回已加载的值
            }

            // 首次加载，生成load指令并缓存
            std::string temp = newTemp();
            addInstruction(temp + " = load i32, ptr %" + varName + ", align 4");
            loadedValues[idExpr->name] = temp;
            return temp;
        }
        else
        {
            // 参数变量
            return "%" + idExpr->name;
        }
    }

    if (auto binExpr = std::dynamic_pointer_cast<BinaryExpr>(expr))
    {
        return generateBinaryOp(binExpr->op, binExpr->lhs, binExpr->rhs);
    }

    if (auto unaryExpr = std::dynamic_pointer_cast<UnaryExpr>(expr))
    {
        return generateUnaryOp(unaryExpr->op, unaryExpr->expr);
    }

    if (auto callExpr = std::dynamic_pointer_cast<CallExpr>(expr))
    {
        return generateCall(callExpr);
    }

    return "0"; // 默认返回值
}

std::string LLVMIRGenerator::generateBinaryOp(const std::string &op, const ASTPtr &lhs, const ASTPtr &rhs)
{
    std::string lhsTemp = generateExpr(lhs);
    std::string rhsTemp = generateExpr(rhs);
    std::string result = newTemp();

    if (op == "&&" || op == "||")
    {
        // 逻辑运算需要特殊处理
        return generateLogicalOp(op, lhs, rhs);
    }
    else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=")
    {
        // 比较运算
        return generateComparison(op, lhs, rhs);
    }
    else
    {
        // 算术运算
        std::string llvmOp;
        if (op == "+")
            llvmOp = "add";
        else if (op == "-")
            llvmOp = "sub";
        else if (op == "*")
            llvmOp = "mul";
        else if (op == "/")
            llvmOp = "sdiv";
        else if (op == "%")
            llvmOp = "srem";

        addInstruction(result + " = " + llvmOp + " nsw i32 " + lhsTemp + ", " + rhsTemp);
        return result;
    }
}

std::string LLVMIRGenerator::generateUnaryOp(const std::string &op, const ASTPtr &expr)
{
    std::string exprTemp = generateExpr(expr);

    // 如果是 - 并且紧跟一个 NumberExpr，就直接返回负数的字面量
    if (op == "-")
    {
        if (auto numExpr = std::dynamic_pointer_cast<NumberExpr>(expr))
        {
            // 直接把 -2 当作常量 "-2"，不输出任何指令
            return "-" + std::to_string(numExpr->value);
        }
        // 否则才正常生成 sub nsw i32 0, %tmp
        std::string inner = generateExpr(expr);
        std::string result = newTemp();
        addInstruction(result + " = sub nsw i32 0, " + inner);
        return result;
    }
    else if (op == "!")
    {
        std::string inner = generateExpr(expr);
        std::string result = newTemp();
        addInstruction(result + " = icmp eq i32 " + inner + ", 0");
        return result;
    }
    else if (op == "+")
    {
        return exprTemp; // 正号不需要操作
    }

    return exprTemp;
}

std::string LLVMIRGenerator::generateComparison(const std::string &op, const ASTPtr &lhs, const ASTPtr &rhs)
{
    std::string lhsTemp = generateExpr(lhs);
    std::string rhsTemp = generateExpr(rhs);
    std::string result = newTemp();

    std::string llvmOp;
    if (op == "==")
        llvmOp = "eq";
    else if (op == "!=")
        llvmOp = "ne";
    else if (op == "<")
        llvmOp = "slt";
    else if (op == ">")
        llvmOp = "sgt";
    else if (op == "<=")
        llvmOp = "sle";
    else if (op == ">=")
        llvmOp = "sge";

    addInstruction(result + " = icmp " + llvmOp + " i32 " + lhsTemp + ", " + rhsTemp);
    return result;
}

std::string LLVMIRGenerator::generateLogicalOp(const std::string &op, const ASTPtr &lhs, const ASTPtr &rhs)
{
    // 为短路逻辑创建结果变量
    std::string resultVar = newVar();
    addInstruction(resultVar + " = alloca i1, align 1");

    if (op == "&&")
    {
        // 短路与逻辑：lhs && rhs
        // 如果lhs为false，直接返回false；否则计算rhs

        std::string lhsTemp = generateExpr(lhs);

        // 创建基本块
        std::string rhsBlock = newLabel("land_rhs");
        std::string falseBlock = newLabel("land_false");
        std::string endBlock = newLabel("land_end");

        // 条件跳转：如果lhs为true，跳转到rhs块；否则跳转到false块
        addInstruction("br i1 " + lhsTemp + ", label %" + rhsBlock + ", label %" + falseBlock);

        // false块：存储false到结果变量
        addInstruction("", true); // 空行分隔
        addInstruction(falseBlock + ":", true);
        addInstruction("store i1 false, ptr " + resultVar + ", align 1");
        addInstruction("br label %" + endBlock);

        // rhs块：计算rhs并存储结果
        addInstruction("", true); // 空行分隔
        addInstruction(rhsBlock + ":", true);
        std::string rhsTemp = generateExpr(rhs);
        addInstruction("store i1 " + rhsTemp + ", ptr " + resultVar + ", align 1");
        addInstruction("br label %" + endBlock);

        // end块：加载并返回结果
        addInstruction("", true); // 空行分隔
        addInstruction(endBlock + ":", true);
    }
    else if (op == "||")
    {
        // 短路或逻辑：lhs || rhs
        // 如果lhs为true，直接返回true；否则计算rhs

        std::string lhsTemp = generateExpr(lhs);

        // 创建基本块
        std::string trueBlock = newLabel("lor_true");
        std::string rhsBlock = newLabel("lor_rhs");
        std::string endBlock = newLabel("lor_end");

        // 条件跳转：如果lhs为true，跳转到true块；否则跳转到rhs块
        addInstruction("br i1 " + lhsTemp + ", label %" + trueBlock + ", label %" + rhsBlock);

        // true块：存储true到结果变量
        addInstruction("", true); // 空行分隔
        addInstruction(trueBlock + ":", true);
        addInstruction("store i1 true, ptr " + resultVar + ", align 1");
        addInstruction("br label %" + endBlock);

        // rhs块：计算rhs并存储结果
        addInstruction("", true); // 空行分隔
        addInstruction(rhsBlock + ":", true);
        std::string rhsTemp = generateExpr(rhs);
        addInstruction("store i1 " + rhsTemp + ", ptr " + resultVar + ", align 1");
        addInstruction("br label %" + endBlock);

        // end块：加载并返回结果
        addInstruction("", true); // 空行分隔
        addInstruction(endBlock + ":", true);
    }

    // 加载结果
    std::string result = newTemp();
    addInstruction(result + " = load i1, ptr " + resultVar + ", align 1");

    return result;
}

std::string LLVMIRGenerator::generateCall(const std::shared_ptr<CallExpr> &call)
{
    std::string result = newTemp();
    std::string args;

    for (size_t i = 0; i < call->args.size(); ++i)
    {
        if (i > 0)
            args += ", ";
        args += "i32 noundef " + generateExpr(call->args[i]);
    }

    addInstruction(result + " = call i32 @" + call->callee + "(" + args + ")");
    return result;
}

void LLVMIRGenerator::generateStmt(const ASTPtr &stmt)
{
    if (auto assignStmt = std::dynamic_pointer_cast<AssignStmt>(stmt))
    {
        generateAssign(assignStmt);
    }
    else if (auto declStmt = std::dynamic_pointer_cast<DeclStmt>(stmt))
    {
        generateDecl(declStmt);
    }
    else if (auto ifStmt = std::dynamic_pointer_cast<IfStmt>(stmt))
    {
        generateIf(ifStmt);
    }
    else if (auto whileStmt = std::dynamic_pointer_cast<WhileStmt>(stmt))
    {
        generateWhile(whileStmt);
    }
    else if (auto returnStmt = std::dynamic_pointer_cast<ReturnStmt>(stmt))
    {
        generateReturn(returnStmt);
    }
    else if (auto breakStmt = std::dynamic_pointer_cast<BreakStmt>(stmt))
    {
        generateBreak();
    }
    else if (auto continueStmt = std::dynamic_pointer_cast<ContinueStmt>(stmt))
    {
        generateContinue();
    }
    else if (auto blockStmt = std::dynamic_pointer_cast<BlockStmt>(stmt))
    {
        generateBlock(blockStmt);
    }
}

void LLVMIRGenerator::generateBlock(const std::shared_ptr<BlockStmt> &block)
{
    // 进入新的作用域
    enterScope();

    for (const auto &stmt : block->stmts)
    {
        generateStmt(stmt);
    }

    // 退出当前作用域
    exitScope();
}

void LLVMIRGenerator::generateAssign(const std::shared_ptr<AssignStmt> &assign)
{
    std::string value = generateExpr(assign->expr);
    std::string varName = getVariableOffset(assign->name);

    if (!varName.empty())
    {
        addInstruction("store i32 " + value + ", ptr %" + varName + ", align 4");
        // 清除该变量的缓存，因为值已经改变
        loadedValues.erase(assign->name);
    }
    else
    {
        // 参数赋值
        addInstruction("store i32 " + value + ", ptr %" + assign->name + "_addr, align 4");
    }
}

void LLVMIRGenerator::generateDecl(const std::shared_ptr<DeclStmt> &decl)
{
    std::string val = generateExpr(decl->expr);
    std::string varName = newVar();

    // 为变量分配空间
    addInstruction("%" + varName + " = alloca i32, align 4");

    // 将变量添加到当前作用域
    addVariable(decl->name, varName);

    // 存储初始值
    addInstruction("store i32 " + val + ", ptr %" + varName + ", align 4");
    loadedValues.erase(decl->name);
}

void LLVMIRGenerator::generateIf(const std::shared_ptr<IfStmt> &ifStmt)
{
    loadedValues.clear();
    std::string cond = generateExpr(ifStmt->cond);
    std::string thenLabel = newLabel("then");
    std::string elseLabel = newLabel("else");
    std::string endLabel = newLabel("endif");
    labelCount++;

    addInstruction("br i1 " + cond +
                   ", label %" + thenLabel +
                   ", label %" + elseLabel);
    addInstruction("");

    // Then block
    addInstruction(thenLabel + ":                                                ; preds = %" + blockLabels.back(), true);
    loadedValues.clear();
    generateStmt(ifStmt->thenStmt);
    addInstruction("br label %" + endLabel);
    addInstruction("");

    // Else block
    addInstruction(elseLabel + ":                                                ; preds = %" + blockLabels.back(), true);
    loadedValues.clear();
    generateStmt(ifStmt->elseStmt);
    addInstruction("br label %" + endLabel);
    addInstruction("");

    // End block
    addInstruction(endLabel + ":                                               ; preds = %" + thenLabel + ", %" + elseLabel, true);
}

void LLVMIRGenerator::generateWhile(const std::shared_ptr<WhileStmt> &whileStmt)
{
    std::string condLabel = newLabel("while_cond");
    std::string bodyLabel = newLabel("while_body");
    std::string endLabel = newLabel("while_end");
    labelCount++;

    // 保存break和continue标签
    breakLabels.push_back(endLabel);
    continueLabels.push_back(condLabel);

    addInstruction("br label %" + condLabel);
    addInstruction("");

    // Condition
    // preds = %0 (entry) and %while_body_X (从 loop 迭代回来)
    addInstruction(condLabel + ":                                                ; preds = %" + blockLabels.back() + ", %" + bodyLabel,
                   true);
    std::string cond = generateExpr(whileStmt->cond);
    addInstruction("br i1 " + cond +
                   ", label %" + bodyLabel +
                   ", label %" + endLabel);
    addInstruction("");

    // Body
    // preds = %" + condLabel
    addInstruction(bodyLabel +
                       ":                                                ; preds = %" + condLabel,
                   true);
    blockLabels.push_back(bodyLabel);
    generateStmt(whileStmt->body);
    addInstruction("br label %" + condLabel);
    addInstruction("");

    blockLabels.pop_back();

    // End
    // preds = %" + condLabel + ", %" + bodyLabel
    addInstruction(endLabel +
                       ":                                                ; preds = %" + condLabel + ", %" + bodyLabel,
                   true);

    // 恢复break和continue标签
    breakLabels.pop_back();
    continueLabels.pop_back();
}

void LLVMIRGenerator::generateReturn(const std::shared_ptr<ReturnStmt> &returnStmt)
{
    if (returnStmt->expr)
    {
        std::string value = generateExpr(returnStmt->expr);
        addInstruction("ret i32 " + value);
    }
    else
    {
        addInstruction("ret void");
    }
    hasReturn = true;
}

void LLVMIRGenerator::generateBreak()
{
    if (!breakLabels.empty())
    {
        addInstruction("br label %" + breakLabels.back());
    }
}

void LLVMIRGenerator::generateContinue()
{
    if (!continueLabels.empty())
    {
        addInstruction("br label %" + continueLabels.back());
    }
}

std::string generateLLVMIR(const std::vector<std::shared_ptr<FuncDef>> &funcs)
{
    LLVMIRGenerator generator;
    return generator.generateModule(funcs);
}