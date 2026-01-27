#include "include/ra_linear_scan.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <locale>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

using namespace ra_ls;

/**
 * @brief 统一的寄存器分配测试程序
 * @details 提供多个测试用例，详细的调试信息输出，便于分析寄存器分配过程
 */

// 测试用例结构
struct TestCase
{
    std::string name;
    std::string llvmIR;
    std::string description;
};

// 全局输出流指针，可以指向cout或文件流
std::ostream *g_output = &std::cout;

void printUsage(const std::string &programName)
{
    std::cout << "用法: " << programName << " [选项] [参数]\n"
              << "选项:\n"
              << "  -h, --help           显示此帮助信息\n"
              << "  -f, --file <path>    从文件读取LLVM IR进行测试\n"
              << "  -i, --input <ir>     直接提供LLVM IR字符串\n"
              << "  -o, --output <path>  将结果输出到文件\n"
              << "  --interactive        交互模式（默认）\n"
              << "\n"
              << "示例:\n"
              << "  " << programName << " -f input.ll            # 测试input.ll文件\n"
              << "  " << programName << " -i \"define i32 @main()...\" # 直接测试IR字符串\n"
              << std::endl;
}

std::string readFileContent(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        throw std::runtime_error("无法打开文件: " + filename);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void runSingleTest(const TestCase &testCase)
{
    *g_output << std::string(60, '=') << std::endl;
    *g_output << "测试用例: " << testCase.name << std::endl;
    *g_output << "描述: " << testCase.description << std::endl;
    *g_output << std::string(60, '=') << std::endl;

    *g_output << "\n输入的LLVM IR:" << std::endl;
    *g_output << testCase.llvmIR << std::endl;

    // 解析LLVM IR
    *g_output << "开始解析LLVM IR..." << std::endl;
    auto functionIR = parseFunctionFromLLVMIR(testCase.llvmIR, "");

    if (!functionIR)
    {
        *g_output << "错误：无法解析LLVM IR" << std::endl;
        return;
    }

    *g_output << "解析成功！" << std::endl;
    *g_output << "函数名: " << functionIR->name << std::endl;
    *g_output << "基本块数量: " << functionIR->blocks.size() << std::endl;

    // 创建寄存器信息和分配器
    auto regInfo = RegInfo();
    *g_output << "\n--- 寄存器信息 ---" << std::endl;
    *g_output << "总物理寄存器数: " << regInfo.physRegs.size() << std::endl;
    *g_output << "可分配寄存器数: " << regInfo.allocatableRegs.size() << std::endl;

    LinearScanAllocator allocator(regInfo);
    allocator.setDebugMode(true);
    allocator.setDebugOutput(g_output);

    // 执行寄存器分配
    *g_output << "\n--- 开始寄存器分配 ---" << std::endl;
    auto result = allocator.allocate(*functionIR);

    // 输出基本块分析结果
    *g_output << "\n--- 基本块分析结果 ---" << std::endl;
    *g_output << "最大虚拟寄存器ID: " << functionIR->maxVregId << std::endl;
    for (const auto &block : functionIR->blocks)
    {
        *g_output << "基本块 " << block->name << " (ID: " << block->id << "):" << std::endl;
        *g_output << "  指令数: " << block->insts.size() << std::endl;
        *g_output << "  后继块: ";
        for (const auto *succ : block->succ)
        {
            *g_output << succ->name << " ";
        }
        *g_output << std::endl;
        *g_output << "  前驱块: ";
        for (const auto *pred : block->pred)
        {
            *g_output << pred->name << " ";
        }
        *g_output << std::endl;
        *g_output << "  在本块定义的虚拟寄存器: ";
        for (int vreg : block->defSet)
        {
            *g_output << "%" << vreg << " ";
        }
        *g_output << std::endl;
        *g_output << "  在本块使用的虚拟寄存器: ";
        for (int vreg : block->useSet)
        {
            *g_output << "%" << vreg << " ";
        }
        *g_output << std::endl;
        *g_output << "  进入本块时活跃的虚拟寄存器: ";
        for (int vreg : block->liveIn)
        {
            *g_output << "%" << vreg << " ";
        }
        *g_output << std::endl;
        *g_output << "  离开本块时活跃的虚拟寄存器: ";
        for (int vreg : block->liveOut)
        {
            *g_output << "%" << vreg << " ";
        }
        *g_output << std::endl;
        *g_output << std::string(60, '=') << std::endl;
    }

    // 输出分配结果
    *g_output << "\n--- 分配结果统计 ---" << std::endl;
    *g_output << "总虚拟寄存器数: " << result.vregToPhys.size() << std::endl;
    *g_output << "溢出槽数量: " << result.vregToStack.size() << std::endl;

    int physRegCount = 0;
    int spillCount = 0;

    for (const auto &pair : result.vregToPhys)
    {
        if (pair.second >= 0)
        {
            physRegCount++;
        }
        else
        {
            spillCount++;
        }
    }

    *g_output << "分配到物理寄存器: " << physRegCount << std::endl;
    *g_output << "溢出到栈: " << spillCount << std::endl;

    *g_output << "\n--- 详细分配结果 ---" << std::endl;
    for (const auto &pair : result.vregToPhys)
    {
        int vreg = pair.first;
        int physReg = pair.second;

        if (physReg >= 0)
        {
            *g_output << "%" << vreg << " -> " << regInfo.getReg(physReg).name
                      << " (物理寄存器ID: " << physReg << ")" << std::endl;
        }
        else
        {
            auto it = result.vregToStack.find(vreg);
            int spillSlot = (it != result.vregToStack.end()) ? it->second : -1;
            *g_output << "%" << vreg << " -> 栈槽" << spillSlot << std::endl;
        }
    }
}

void runCustomTest()
{
    std::cout << "\n请输入自定义LLVM IR (输入一行END结束):" << std::endl;
    std::string customIR;
    std::string line;

    while (std::getline(std::cin, line))
    {
        if (line == "END")
            break;
        customIR += line + "\n";
    }

    if (customIR.empty())
    {
        std::cout << "未输入有效的IR代码" << std::endl;
        return;
    }

    TestCase customTest = {"自定义测试", customIR, "用户自定义的LLVM IR测试"};
    runSingleTest(customTest);
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // 设置Windows控制台支持UTF-8输出
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::ofstream outputFile;
    bool hasOutputFile = false;

    // 如果没有命令行参数，直接运行自定义测试模式
    if (argc == 1)
    {
        std::cout << "线性扫描寄存器分配器测试程序" << std::endl;
        std::cout << "请输入LLVM IR代码 (输入END结束):" << std::endl;
        runCustomTest();
        return 0;
    }

    // 解析命令行参数
    try
    {
        // 第一遍：处理设置选项
        for (int i = 1; i < argc; i++)
        {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help")
            {
                printUsage(argv[0]);
                return 0;
            }
            else if (arg == "-o" || arg == "--output")
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "错误: " << arg << " 需要一个文件路径参数" << std::endl;
                    return 1;
                }
                outputFile.open(argv[++i]);
                if (!outputFile.is_open())
                {
                    std::cerr << "错误: 无法打开输出文件 " << argv[i] << std::endl;
                    return 1;
                }
                g_output = &outputFile;
                hasOutputFile = true;
            }
        }

        // 第二遍：处理执行选项
        for (int i = 1; i < argc; i++)
        {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "-o" || arg == "--output")
            {
                // 跳过已处理的选项和它们的参数
                if (arg == "-o" || arg == "--output")
                {
                    i++; // 跳过文件路径参数
                }
                continue;
            }
            else if (arg == "-f" || arg == "--file")
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "错误: " << arg << " 需要一个文件路径参数" << std::endl;
                    return 1;
                }
                std::string filename = argv[++i];
                std::string ir = readFileContent(filename);
                TestCase fileTest = {"文件测试", ir, "从文件 " + filename + " 读取的LLVM IR测试"};
                runSingleTest(fileTest);
            }
            else if (arg == "-i" || arg == "--input")
            {
                if (i + 1 >= argc)
                {
                    std::cerr << "错误: " << arg << " 需要一个LLVM IR字符串参数" << std::endl;
                    return 1;
                }
                std::string ir = argv[++i];
                TestCase inputTest = {"命令行输入测试", ir, "从命令行输入的LLVM IR测试"};
                runSingleTest(inputTest);
            }
            else if (arg == "--interactive")
            {
                // 交互模式已经在上面处理了
                continue;
            }
            else
            {
                std::cerr << "错误: 未知选项 " << arg << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    if (hasOutputFile)
    {
        outputFile.close();
        std::cout << "结果已输出到文件" << std::endl;
    }

    return 0;
}
