#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include "include/parser.h"
#include "include/llvm_ir.h"
#include "include/riscv_gen.h"

using namespace std;

int main(int argc, char **argv)
{
    string inputFile = "";
    string outputMode = "asm";
    string outputFile = "";
    string targetArch = "riscv32";
    bool use_stdin = false;
    bool optimize = false;

    // 特殊情况：无参数或只有优化参数，使用标准输入
    if (argc == 1 || (argc == 2 && string(argv[1]) == "-opt"))
    {
        use_stdin = true;
    }

    // 否则解析参数
    for (int i = 1; i < argc; i++)
    {
        string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc)
        {
            outputMode = argv[++i];
        }
        else if (arg == "--output" && i + 1 < argc)
        {
            outputFile = argv[++i];
        }
        else if (arg == "--target" && i + 1 < argc)
        {
            targetArch = argv[++i];
        }
        else if (arg == "-opt")
        {
            optimize = true;
        }
        else if (
            (arg.size() >= 3 && arg.compare(arg.size() - 3, 3, ".tc") == 0) ||
            (arg.size() >= 2 && arg.compare(arg.size() - 2, 2, ".c") == 0) ||
            (arg.size() >= 3 && arg.compare(arg.size() - 3, 3, ".ll") == 0))
        {
            inputFile = arg;
        }
        else
        {
            cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }

    // 如果没有输入文件也不是 stdin，则输出帮助
    if (inputFile.empty() && !use_stdin)
    {
        cerr << "Usage: toyc <input_file> [options]\n";
        cerr << "\nInput file types:\n";
        cerr << "  .c/.tc    - ToyC source file\n";
        cerr << "  .ll       - LLVM IR file\n";
        cerr << "\nOptions:\n";
        cerr << "  --mode <mode>     - Output mode (default: ast)\n";
        cerr << "    ast             - Print Abstract Syntax Tree\n";
        cerr << "    ir              - Print LLVM IR\n";
        cerr << "    asm             - Print RISC-V assembly\n";
        cerr << "    all             - Print AST, IR, and assembly\n";
        cerr << "  --output <file>   - Output to file instead of stdout\n";
        cerr << "  --target <arch>   - Target architecture (default: riscv32)\n";
        cerr << "\nExamples:\n";
        cerr << "  toyc test.c --mode ir                    # Compile C to IR\n";
        cerr << "  toyc test.ll --mode asm                  # IR to assembly\n";
        cerr << "  toyc test.c --mode asm --output test.s   # C to assembly file\n";
        cerr << "  cat test.c | toyc --mode asm             # From stdin\n";
        return 1;
    }

    // 加载输入
    stringstream buf;
    string inputExt;
    bool isLLVMIR = false;
    vector<shared_ptr<FuncDef>> funcs;
    string llvmIR;

    if (use_stdin)
    {
        string line;
        while (getline(cin, line))
        {
            buf << line << '\n';
        }
        inputExt = ".tc"; // 默认认为 stdin 是 ToyC 源码
    }
    else
    {
        inputExt = inputFile.substr(inputFile.find_last_of('.'));
        ifstream in(inputFile);
        if (!in)
        {
            cerr << "Cannot open file: " << inputFile << "\n";
            return 1;
        }
        buf << in.rdbuf();
    }

    // 分支逻辑处理
    isLLVMIR = (inputExt == ".ll");
    if (isLLVMIR)
    {
        llvmIR = buf.str(); // 直接读取 IR
    }
    else
    {
        Parser parser(buf.str());
        funcs = parser.parseCompUnit();
        llvmIR = generateLLVMIR(funcs);
    }

    // 输出目标流
    ostream *out = &cout;
    ofstream fileOut;
    if (!outputFile.empty())
    {
        fileOut.open(outputFile);
        if (!fileOut)
        {
            cerr << "Cannot open output file: " << outputFile << "\n";
            return 1;
        }
        out = &fileOut;
    }

    // 输出各阶段
    if (outputMode == "ast" || outputMode == "all")
    {
        if (!isLLVMIR)
        {
            *out << "=== Abstract Syntax Tree ===\n";
            for (auto &f : funcs)
            {
                f->print(0);
                *out << "\n";
            }
            *out << "\n";
        }
        else
        {
            *out << "AST not available for LLVM IR input\n\n";
        }
    }

    if (outputMode == "ir" || outputMode == "all")
    {
        *out << llvmIR;
    }

    if (outputMode == "asm" || outputMode == "all")
    {
        string assembly = generateRISCVAssembly(llvmIR);
        *out << assembly;
    }

    return 0;
}
