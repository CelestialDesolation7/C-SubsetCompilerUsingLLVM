#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <map>
#include <set>
#include <list>
#include <algorithm>
#include <iostream>
#include <functional>

namespace ra_ls
{
    // ----------------------------- 正则表达式常量 -----------------------------
    namespace RegexPatterns
    {
        // 虚拟寄存器使用模式：%数字
        const std::string VREG_USE = R"(%(\d+))";

        // 虚拟寄存器定义模式：%数字 =
        const std::string VREG_DEF = R"((%(\d+))\s*=)";

        // 条件分支模式：br i1 %cond, label %true, label %false
        // 捕获跳转标签名,不捕获条件寄存器
        const std::string BRANCH_COND_LABEL = R"(br\s+i1\s+[^,]+,\s+label\s+%([^,]+),\s+label\s+%([^,]+))";

        // 条件分支模式：br i1 %cond, label %true, label %false
        // 捕获条件寄存器,不捕获跳转标签名
        const std::string BRANCH_COND_USE = R"(br\s+i1\s+%([^,]+),\s+label\s+[^,]+,\s+label\s+[^,]+)";

        // 无条件分支模式：br label %target
        const std::string BRANCH_UNCOND = R"(br\s+label\s+%([^,]+))";

        // 函数定义模式：define [dso_local] type @name(
        const std::string FUNC_DEF = R"(define\s+(?:dso_local\s+)?(\w+)\s+@(\w+)\s*\()";

        // 标签模式：支持字母标签和数字标签，允许前导空格
        const std::string LABEL = R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*|\d+):\s*(.*))";

        // 指令模式：匹配任何非空行
        const std::string INSTRUCTION = R"(^\s*(.+))";
    }

    // ----------------------------- 前向声明 -----------------------------
    class BasicBlock;
    class FunctionIR;
    class LiveInterval;
    class LinearScanAllocator;
    class AllocationResult;

#pragma region 指令类
    class Instruction
    {
    public:
        std::string text; // 指令原文（用于解析 def/uses/terminator/targets）
        int idx = -1;     // 全局顺序编号（线性化后设置）
        int blockId = -1; // 所属基本块ID

        Instruction() = default;
        explicit Instruction(const std::string &t) : text(t) {}

        // 位置编号：使用双位方案，def=idx*2, use=idx*2+1
        int posDef() const { return idx * 2; }
        int posUse() const { return idx * 2 + 1; }

        // 解析指令中的虚拟寄存器使用和定义
        std::vector<int> uses() const; // 返回使用的 vreg id 列表（%42 -> 42）
        int def() const;               // 返回定义的 vreg id；若无定义返回 -1
        int branchCondUse() const;     // 返回条件分支使用的 vreg id；若无条件分支返回 -1

        // 控制流分析
        bool isTerminator() const;                      // 是否为终止指令
        std::vector<std::string> branchTargets() const; // 分支目标标签（仅 terminator）
        bool isCall() const;                            // 是否为函数调用
    };
#pragma endregion

#pragma region 基本块类
    class BasicBlock
    {
    public:
        int id = -1;                                     // 基本块ID
        std::string name;                                // 基本块名称（标签）
        std::vector<std::unique_ptr<Instruction>> insts; // 指令列表
        std::vector<BasicBlock *> succ;                  // 后继基本块
        std::vector<BasicBlock *> pred;                  // 前驱基本块

        // 位置信息
        int firstPos() const;
        int lastPos() const;

        // 数据流分析结果
        std::set<int> defSet;  // 在本块定义的虚拟寄存器
        std::set<int> useSet;  // 在本块使用但未定义的虚拟寄存器
        std::set<int> liveIn;  // 进入本块时活跃的虚拟寄存器
        std::set<int> liveOut; // 离开本块时活跃的虚拟寄存器
    };
#pragma endregion

#pragma region 单个LLVM IR函数的所有信息
    class FunctionIR
    {
    public:
        std::string name;                                          // 函数名
        std::vector<std::unique_ptr<BasicBlock>> blocks;           // 基本块列表
        std::unordered_map<std::string, BasicBlock *> nameToBlock; // 名称到基本块的映射
        std::vector<BasicBlock *> blocksInOrder;                   // 线性化后的基本块顺序（RPO）
        std::vector<int> parameters;                               // 参数虚拟寄存器列表

        BasicBlock *entryBlock() const;
        int maxVregId = -1; // 最大虚拟寄存器ID

        void buildControlFlowGraph();
    };
#pragma endregion

#pragma region 活跃区间类
    struct LiveRange
    {
        int start; // 起始位置（包含）
        int end;   // 结束位置（包含）

        LiveRange(int s, int e) : start(s), end(e) {}

        bool operator<(const LiveRange &other) const
        {
            return start < other.start;
        }

        bool overlaps(const LiveRange &other) const
        {
            return !(end < other.start || other.end < start);
        }

        bool adjacent(const LiveRange &other) const
        {
            return end + 1 == other.start || other.end + 1 == start;
        }
    };

    class LiveInterval
    {
    public:
        int vreg = -1;                 // 虚拟寄存器 ID
        std::vector<LiveRange> ranges; // 有序且不重叠的活跃范围列表
        int spillSlot = -1;            // 溢出槽索引（-1表示未溢出）
        int physReg = -1;              // 分配的物理寄存器ID（-1表示未分配）

        LiveInterval() = default;
        explicit LiveInterval(int v) : vreg(v) {}

        // 活跃区间操作
        void addRange(int start, int end); // 添加活跃范围（自动合并）
        bool contains(int pos) const;      // 检查位置是否在活跃区间内
        int start() const;                 // 第一个范围的起始位置
        int end() const;                   // 最后一个范围的结束位置
        bool empty() const { return ranges.empty(); }

        // 区间分割（用于溢出优化）
        // 当前未被使用，因为溢出优化未被实现
        std::unique_ptr<LiveInterval> splitAt(int pos);

        // 排序比较（按起始位置）
        bool operator<(const LiveInterval &other) const
        {
            return start() < other.start();
        }
    };
#pragma endregion

#pragma region 物理寄存器信息类
    /**
     * @brief 物理寄存器描述
     */
    struct PhysReg
    {
        int id;                   // 逻辑编号（连续，从 0 开始）
        std::string name;         // 寄存器名称（如 "t0", "a0"）
        bool callerSaved = false; // 调用者保存
        bool calleeSaved = false; // 被调用者保存
        bool reserved = false;    // 保留寄存器（如 sp, s0, zero）
        int priority = 0;         // 分配优先级（数值越小优先级越高）

        PhysReg() = default;
        PhysReg(int i, const std::string &n, bool caller = false, bool callee = false,
                bool res = false, int prio = 0)
            : id(i), name(n), callerSaved(caller), calleeSaved(callee), reserved(res), priority(prio) {}
    };

    // 自定义比较器，按优先级排序寄存器
    struct PhysRegComparator
    {
        const std::vector<PhysReg> *physRegs;

        PhysRegComparator(const std::vector<PhysReg> *regs) : physRegs(regs) {}

        bool operator()(int a, int b) const
        {
            if (a < 0 || a >= static_cast<int>(physRegs->size()) ||
                b < 0 || b >= static_cast<int>(physRegs->size()))
            {
                return a < b; // 回退到ID比较
            }

            int priorityA = (*physRegs)[a].priority;
            int priorityB = (*physRegs)[b].priority;

            if (priorityA != priorityB)
            {
                return priorityA < priorityB; // 优先级低的排在前面
            }
            return a < b; // 优先级相同时按ID排序
        }
    };

    /**
     * @brief 寄存器信息类
     * @details 描述目标架构的物理寄存器信息，不参与运行时状态管理
     */
    class RegInfo
    {
    public:
        std::vector<PhysReg> physRegs;                    // 所有物理寄存器
        std::set<int, PhysRegComparator> allocatableRegs; // 可分配的寄存器ID列表（按优先级排序）

        RegInfo(); // 构造函数，初始化 RV32 寄存器信息

        // 查询接口
        bool isReserved(int physId) const;
        bool isCallerSaved(int physId) const;
        bool isCalleeSaved(int physId) const;
        const PhysReg &getReg(int physId) const { return physRegs[physId]; }
        std::string getRegName(int physId) const
        {
            return (physId >= 0 && physId < static_cast<int>(physRegs.size())) ? physRegs[physId].name : "invalid";
        }
    };
#pragma endregion

#pragma region 活跃性分析类
    /**
     * @brief 活跃性分析类
     * @details 执行数据流分析，计算每个基本块的 liveIn 和 liveOut 集合
     */
    class LivenessAnalysis
    {
    public:
        void run(FunctionIR &F);
        static std::vector<BasicBlock *> buildRPO(BasicBlock *entry); // 构建逆后序遍历顺序

    private:
        void computeUseDefSets(FunctionIR &F);          // 计算 USE 和 DEF 集合
        void computeLivenessIteratively(FunctionIR &F); // 迭代计算活跃性
    };

    /**
     * @brief 活跃区间构建器
     * @details 根据活跃性分析结果构建虚拟寄存器的活跃区间
     */
    class LiveIntervalBuilder
    {
    public:
        LiveIntervalBuilder(FunctionIR &F, const LivenessAnalysis &LA, bool intervalSplittingEnabled = false);

        // 构建所有虚拟寄存器的活跃区间
        std::unordered_map<int, std::unique_ptr<LiveInterval>> build();

    private:
        FunctionIR &F;
        const LivenessAnalysis &LA;
        bool intervalSplittingEnabled;

        void buildIntervalForVreg(int vreg, std::unique_ptr<LiveInterval> &interval);
        void buildSimplifiedIntervalForVreg(int vreg, std::unique_ptr<LiveInterval> &interval);
    };
#pragma endregion

#pragma region 线性扫描分配器类
    class AllocationResult
    {
    public:
        std::unordered_map<int, int> vregToPhys;  // vreg -> 物理寄存器ID（-1表示溢出）
        std::unordered_map<int, int> vregToStack; // vreg -> 栈字节偏移（负数，相对于栈帧指针）
        // 指令位置到虚拟寄存器瞬时位置的映射
        std::unordered_map<int, std::unordered_map<int, int>> instrToVregLocation; // instr_pos -> (vreg -> phys_reg_id or stack_slot)
        // 参数列表到物理存储位置的映射
        std::unordered_map<int, int> paramVregToLocation; // param_vreg -> location (reg_id or stack_offset)
        // 函数使用的物理寄存器集合
        std::set<int> usedPhysRegs;
        // 被调用者保护的寄存器集合
        std::set<int> calleeSavedRegs;
        AllocationResult() = default;
    };

    /**
     * @brief 线性扫描寄存器分配器主类
     * @details 实现基于线性扫描算法的寄存器分配，包括溢出处理
     */
    class LinearScanAllocator
    {
    public:
        explicit LinearScanAllocator(const RegInfo &regInfo);

        // 主分配接口
        AllocationResult allocate(FunctionIR &F);

        // 配置接口
        void enableIntervalSplitting(bool enable) { intervalSplittingEnabled = enable; }
        void setSpillTempRegs(int spillTemp1, int spillTemp2)
        {
            spillTempReg1 = spillTemp1;
            spillTempReg2 = spillTemp2;
        }

        // 查询接口：按指令行数和Vreg name查找虚拟寄存器使用时所处位置
        int getVregLocationAtInstr(int instrPos, int vreg) const;
        // 查询接口：按Vreg name查找虚拟寄存器定义时分配位置
        int getVregDefLocation(int vreg) const;

        // 参数处理接口
        void processParameters(const std::vector<int> &paramVregs);

        // 获取分配结果的只读访问
        const AllocationResult &getAllocationResult() const { return result; }

        // 获取函数使用的所有物理寄存器集合
        std::set<int> getUsedPhysRegs() const;
        // 获取被调用者保护的寄存器集合
        std::set<int> getCalleeSavedRegs() const;

        // 调试接口
        void setDebugMode(bool enable) { debugMode = enable; }
        void setDebugOutput(std::ostream *output) { debugOutput = output; }
        void dumpIntervals(const std::unordered_map<int, std::unique_ptr<LiveInterval>> &intervals);

        // 溢出临时寄存器管理（公有接口）
        int allocateSpillTempReg();           // 分配溢出临时寄存器（循环选择）
        bool isSpillTempReg(int regId) const; // 检查是否是溢出临时寄存器

    private:
        const RegInfo &regInfo; // 寄存器信息（只读）
        bool debugMode = false;
        std::ostream *debugOutput = &std::cout;

        // 寄存器状态管理
        std::vector<bool> isPhysRegUsed;               // 记录寄存器索引对应寄存器是否被使用
        std::set<int, PhysRegComparator> freePhysRegs; // 当前空闲寄存器池，按优先级排序

        // 配置选项
        bool intervalSplittingEnabled = false; // 是否启用区间分割
        int spillTempReg1 = 5;                 // 溢出临时寄存器1 (t0)
        int spillTempReg2 = 6;                 // 溢出临时寄存器2 (t1)

        // 溢出管理
        std::set<int> spilledVregs;    // 已经被溢出的虚拟寄存器集合
        std::set<int> allocatedVregs;  // 已经被分配过物理寄存器的虚拟寄存器集合
        bool spillTempCounter = false; // 溢出寄存器循环计数器

        // 分配阶段
        void assignInstrPositions(FunctionIR &F); // 指令位置编号
        AllocationResult runLinearScan(
            const std::unordered_map<int, std::unique_ptr<LiveInterval>> &intervals);

        // 线性扫描核心算法
        void expireOldIntervals(int curStart);
        void allocatePhysicalReg(LiveInterval &interval);
        void spillAtInterval(LiveInterval &interval);
        int chooseSpillCandidate(const LiveInterval &current) const;
        int allocateSpillSlot();

        // 工作数据结构
        std::vector<LiveInterval *> active; // 当前活跃区间（按结束位置排序）
        AllocationResult result;            // 分配结果
        int nextSpillSlot = 0;              // 下一个可用溢出槽

        // 辅助函数
        void sortIntervalsByStart(std::vector<LiveInterval *> &intervals);
        void insertActiveInterval(LiveInterval *interval);
        void removeFromActive(LiveInterval *interval);
        void computeInstrVregLocations(FunctionIR &F);

        // 寄存器状态管理
        void initializeFreeRegs();        // 初始化空闲寄存器集合
        int allocatePhysReg();            // 分配优先级最高的空闲寄存器
        void freePhysReg(int physId);     // 释放寄存器
        bool isRegFree(int physId) const; // 检查寄存器是否空闲
    };

#pragma endregion

    /**
     * @brief 从 LLVM IR 构建函数 IR
     * @param llvmIR LLVM IR 字符串
     * @param funcName 函数名（默认查找第一个函数）
     * @return 构建的 FunctionIR 对象
     */
    std::unique_ptr<FunctionIR> parseFunctionFromLLVMIR(const std::string &llvmIR,
                                                        const std::string &funcName = "");

    /**
     * @brief 解析函数参数列表
     * @param funcDefLine 函数定义行
     * @param functionIR 函数IR对象
     */
    void parseFunctionParameters(const std::string &funcDefLine, FunctionIR &functionIR);

}
