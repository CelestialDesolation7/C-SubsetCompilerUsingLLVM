#include "include/ra_linear_scan.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <stack>
#include <cassert>
#include <climits>
#include <memory>

namespace ra_ls
{
#pragma region 指令类实现
    /**
     * @brief 解析指令中使用的虚拟寄存器
     * @return 虚拟寄存器ID列表
     * @details 使用正则表达式匹配%数字格式的虚拟寄存器
     */
    std::vector<int> Instruction::uses() const
    {
        std::vector<int> result;

        // 如果是分支指令，需要特殊处理，排除分支目标标号
        if (isTerminator())
        {
            // 如果不是条件分支，则直接返回
            if (branchCondUse() == -1)
            {
                return result;
            }
            // 如果是条件分支，则返回条件寄存器
            result.push_back(branchCondUse());
        }
        else
        {
            // 非分支指令，正常处理
            std::regex reg(RegexPatterns::VREG_USE);
            std::sregex_iterator iter(text.begin(), text.end(), reg);
            std::sregex_iterator end;

            // 如果有定义，第一个匹配就是定义，跳过它
            int defVreg = def();
            bool skipFirst = (defVreg != -1);

            for (; iter != end; ++iter)
            {
                if (skipFirst)
                {
                    skipFirst = false;
                    continue;
                }
                int vreg = std::stoi(iter->str(1));
                result.push_back(vreg);
            }
        }

        return result;
    }

    /**
     * @brief 解析指令定义的虚拟寄存器
     * @return 定义的虚拟寄存器ID，-1表示无定义
     */
    int Instruction::def() const
    {
        std::regex reg(RegexPatterns::VREG_DEF);
        std::smatch match;
        if (std::regex_search(text, match, reg))
        {
            return std::stoi(match.str(2));
        }
        return -1;
    }

    int Instruction::branchCondUse() const
    {
        std::regex reg(RegexPatterns::BRANCH_COND_USE);
        std::smatch match;
        if (std::regex_search(text, match, reg))
        {
            return std::stoi(match.str(1));
        }
        return -1;
    }
    /**
     * @brief 检查是否为终止指令
     * @return true表示是终止指令
     */
    bool Instruction::isTerminator() const
    {
        return text.find("br ") == 0 ||
               text.find("ret ") == 0;
    }

    /**
     * @brief 获取分支目标标签
     * @return 分支目标标签列表
     */
    std::vector<std::string> Instruction::branchTargets() const
    {
        std::vector<std::string> targets;

        // 条件分支: br i1 %cond, label %true, label %false
        std::regex condBr(RegexPatterns::BRANCH_COND_LABEL);
        std::smatch match;
        if (std::regex_search(text, match, condBr))
        {
            targets.push_back(match.str(1));
            targets.push_back(match.str(2));
            return targets;
        }

        // 无条件分支: br label %target
        std::regex uncondBr(RegexPatterns::BRANCH_UNCOND);
        if (std::regex_search(text, match, uncondBr))
        {
            targets.push_back(match.str(1));
            return targets;
        }

        return targets;
    }

    /**
     * @brief 检查是否为函数调用指令
     * @return true表示是函数调用
     */
    bool Instruction::isCall() const
    {
        return text.find("call ") != std::string::npos;
    }
#pragma endregion

#pragma region 基本块类实现
    /**
     * @brief 获取基本块第一条指令的位置
     * @return 第一条指令的位置(线性化编号)，-1表示空基本块
     */
    int BasicBlock::firstPos() const
    {
        if (insts.empty())
            return -1;
        return insts[0]->posDef();
    }

    /**
     * @brief 获取基本块最后一条指令的位置
     * @return 最后一条指令的位置(线性化编号)，-1表示空基本块
     */
    int BasicBlock::lastPos() const
    {
        if (insts.empty())
            return -1;
        return insts.back()->posUse();
    }

    /**
     * @brief 获取函数入口基本块
     * @return 入口基本块指针，nullptr表示无基本块
     */
    BasicBlock *FunctionIR::entryBlock() const
    {
        return blocks.empty() ? nullptr : blocks[0].get();
    }
#pragma endregion

#pragma region IR函数类实现
    /**
     * @brief 构建控制流图
     * @details 分析分支指令，建立基本块之间的前驱后继关系
     */
    void FunctionIR::buildControlFlowGraph()
    {
        // 清空现有的前驱后继关系
        for (auto &block : blocks)
        {
            block->succ.clear();
            block->pred.clear();
        }

        // 分析每个基本块的终止指令，建立后继关系
        for (auto &block : blocks)
        {
            if (block->insts.empty())
                continue;

            // 检查最后一条指令是否为终止指令
            auto &lastInst = block->insts.back();
            if (lastInst->isTerminator())
            {
                std::vector<std::string> targets = lastInst->branchTargets();

                for (const std::string &target : targets)
                {
                    auto it = nameToBlock.find(target);
                    if (it != nameToBlock.end())
                    {
                        BasicBlock *succBlock = it->second;
                        block->succ.push_back(succBlock);
                        succBlock->pred.push_back(block.get());
                    }
                }
            }
            else
            {
                // 如果不是终止指令，检查是否有fall-through到下一个基本块
                size_t currentIdx = 0;
                for (size_t i = 0; i < blocks.size(); ++i)
                {
                    if (blocks[i].get() == block.get())
                    {
                        currentIdx = i;
                        break;
                    }
                }

                if (currentIdx + 1 < blocks.size())
                {
                    BasicBlock *nextBlock = blocks[currentIdx + 1].get();
                    block->succ.push_back(nextBlock);
                    nextBlock->pred.push_back(block.get());
                }
            }
        }
    }
#pragma endregion

#pragma region 活跃区间类实现
    /**
     * @brief 添加活跃范围并自动合并重叠范围
     * @param start 范围起始位置
     * @param end 范围结束位置
     */
    void LiveInterval::addRange(int start, int end)
    {
        if (start > end)
            return;

        LiveRange newRange(start, end);

        // 如果ranges为空，直接添加
        if (ranges.empty())
        {
            ranges.push_back(newRange);
            return;
        }

        // 找到插入位置并合并
        auto it = ranges.begin();
        while (it != ranges.end() && it->start <= start)
        {
            ++it;
        }
        // 此时it指向第一个大于start的迭代器

        // 向前合并，即与插入目标位置前面的区间合并
        // mergeStart 最初指向第一个 Start 大于 newRange.start的迭代器
        auto mergeStart = it;
        while (mergeStart != ranges.begin())
        {
            // prev 最初指向第一个 Start 小于 newRange.start的迭代器
            auto prev = mergeStart - 1;
            // 如果重叠或者毗邻
            if (prev->overlaps(newRange) || prev->adjacent(newRange))
            {
                // 修改新建区间的左右界
                newRange.start = std::min(newRange.start, prev->start);
                newRange.end = std::max(newRange.end, prev->end);
                // 更新mergeStart为更前一个区间,该区间仍然可能与新建区间重叠，继续向前合并
                mergeStart = prev;
            }
            else
            {
                break;
            }
        }

        // 向后合并，即与插入目标位置后面的区间合并
        auto mergeEnd = it;
        while (mergeEnd != ranges.end())
        {
            if (mergeEnd->overlaps(newRange) || mergeEnd->adjacent(newRange))
            {
                newRange.start = std::min(newRange.start, mergeEnd->start);
                newRange.end = std::max(newRange.end, mergeEnd->end);
                ++mergeEnd;
            }
            else
            {
                break;
            }
        }

        // 删除被合并的范围并插入新范围
        ranges.erase(mergeStart, mergeEnd);
        ranges.insert(mergeStart, newRange);
    }

    /**
     * @brief 检查位置是否在活跃区间内
     * @param pos 位置
     * @return true表示位置在活跃区间内
     */
    bool LiveInterval::contains(int pos) const
    {
        for (const auto &range : ranges)
        {
            if (pos >= range.start && pos <= range.end)
                return true;
        }
        return false;
    }

    /**
     * @brief 获取活跃区间的起始位置
     * @return 第一个范围的起始位置，-1表示空区间
     */
    int LiveInterval::start() const
    {
        return ranges.empty() ? -1 : ranges[0].start;
    }

    /**
     * @brief 获取活跃区间的结束位置
     * @return 最后一个范围的结束位置，-1表示空区间
     */
    int LiveInterval::end() const
    {
        return ranges.empty() ? -1 : ranges.back().end;
    }

    /**
     * @brief 在指定位置分割活跃区间
     * @param pos 分割位置
     * @return 分割后的后半段活跃区间，nullptr表示无需分割
     */
    std::unique_ptr<LiveInterval> LiveInterval::splitAt(int pos)
    {
        // 查找包含pos的范围
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            if (pos >= ranges[i].start && pos <= ranges[i].end)
            {
                auto newInterval = std::unique_ptr<LiveInterval>(new LiveInterval(vreg));

                // 分割当前范围
                if (pos < ranges[i].end)
                {
                    newInterval->ranges.emplace_back(pos + 1, ranges[i].end);
                    ranges[i].end = pos;
                }

                // 移动后续所有范围到新区间
                for (size_t j = i + 1; j < ranges.size(); ++j)
                {
                    newInterval->ranges.push_back(ranges[j]);
                }
                ranges.erase(ranges.begin() + i + 1, ranges.end());

                return newInterval;
            }
        }
        return nullptr;
    }

#pragma endregion

#pragma region 寄存器信息类实现
    /**
     * @brief 构造RV32寄存器信息
     * @details 初始化RISC-V32架构的物理寄存器描述
     */
    RegInfo::RegInfo() : allocatableRegs(PhysRegComparator(&physRegs))
    {
        // RV32通用寄存器定义
        // x0 (zero) - 硬连线为0，保留
        physRegs.emplace_back(0, "zero", false, false, true, 999);

        // x1 (ra) - 返回地址，保留
        physRegs.emplace_back(1, "ra", false, false, true, 999);

        // x2 (sp) - 栈指针，保留
        physRegs.emplace_back(2, "sp", false, false, true, 999);

        // x3 (gp) - 全局指针，保留
        physRegs.emplace_back(3, "gp", false, false, true, 999);

        // x4 (tp) - 线程指针，保留
        physRegs.emplace_back(4, "tp", false, false, true, 999);

        // x5-x7 (t0-t2) - 临时寄存器，调用者保存
        // x5-x6 被溢出临时寄存器使用
        for (int i = 5; i <= 6; i++)
        {
            physRegs.emplace_back(i, "t" + std::to_string(i - 5), true, false, true, i - 5);
            allocatableRegs.insert(i);
        }
        for (int i = 7; i <= 7; i++)
        {
            physRegs.emplace_back(i, "t" + std::to_string(i - 5), true, false, true, i - 5);
            allocatableRegs.insert(i);
        }

        // x8 (s0/fp) - 保存寄存器/帧指针，被调用者保存，保留作为帧指针
        physRegs.emplace_back(8, "s0", false, true, true, 999);

        // x9 (s1) - 保存寄存器，被调用者保存
        physRegs.emplace_back(9, "s1", false, true, false, 20);
        allocatableRegs.insert(9);

        // x10-x17 (a0-a7) - 参数/返回值寄存器，调用者保存
        for (int i = 10; i <= 17; i++)
        {
            physRegs.emplace_back(i, "a" + std::to_string(i - 10), true, false, false, i - 10 + 3);
            allocatableRegs.insert(i);
        }

        // x18-x27 (s2-s11) - 保存寄存器，被调用者保存
        for (int i = 18; i <= 27; i++)
        {
            physRegs.emplace_back(i, "s" + std::to_string(i - 16), false, true, false, i - 18 + 21);
            allocatableRegs.insert(i);
        }

        // x28-x31 (t3-t6) - 临时寄存器，调用者保存
        for (int i = 28; i <= 31; i++)
        {
            physRegs.emplace_back(i, "t" + std::to_string(i - 25), true, false, false, i - 28 + 11);
            allocatableRegs.insert(i);
        }
    }

    /**
     * @brief 检查寄存器是否为保留寄存器
     * @param physId 物理寄存器ID
     * @return true表示是保留寄存器
     */
    bool RegInfo::isReserved(int physId) const
    {
        return physId >= 0 && physId < static_cast<int>(physRegs.size()) && physRegs[physId].reserved;
    }

    /**
     * @brief 检查寄存器是否为调用者保存
     * @param physId 物理寄存器ID
     * @return true表示是调用者保存寄存器
     */
    bool RegInfo::isCallerSaved(int physId) const
    {
        return physId >= 0 && physId < static_cast<int>(physRegs.size()) && physRegs[physId].callerSaved;
    }

    /**
     * @brief 检查寄存器是否为被调用者保存
     * @param physId 物理寄存器ID
     * @return true表示是被调用者保存寄存器
     */
    bool RegInfo::isCalleeSaved(int physId) const
    {
        return physId >= 0 && physId < static_cast<int>(physRegs.size()) && physRegs[physId].calleeSaved;
    }

#pragma endregion

#pragma region 活跃性分析类实现

    void LivenessAnalysis::run(FunctionIR &F)
    {
        computeUseDefSets(F);
        computeLivenessIteratively(F);
    }

    void LivenessAnalysis::computeUseDefSets(FunctionIR &F)
    {
        for (auto &block : F.blocks)
        {
            block->useSet.clear();
            block->defSet.clear();

            for (auto &inst : block->insts)
            {
                // 先处理 use
                for (int vreg : inst->uses())
                {
                    if (!block->defSet.count(vreg))
                        block->useSet.insert(vreg);
                    F.maxVregId = std::max(F.maxVregId, vreg);
                }

                // 再处理 def
                int defVreg = inst->def();
                if (defVreg != -1)
                {
                    block->defSet.insert(defVreg);
                    F.maxVregId = std::max(F.maxVregId, defVreg);
                }
            }
        }
    }

    void LivenessAnalysis::computeLivenessIteratively(FunctionIR &F)
    {
        // 构建 RPO，结果保存在 F 的成员变量中
        F.blocksInOrder = buildRPO(F.entryBlock());

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (int i = static_cast<int>(F.blocksInOrder.size()) - 1; i >= 0; --i)
            {
                BasicBlock *block = F.blocksInOrder[i];

                std::set<int> newLiveOut;
                for (BasicBlock *succ : block->succ)
                    newLiveOut.insert(succ->liveIn.begin(), succ->liveIn.end());

                std::set<int> newLiveIn = block->useSet;
                for (int vreg : newLiveOut)
                    if (!block->defSet.count(vreg))
                        newLiveIn.insert(vreg);

                if (newLiveIn != block->liveIn || newLiveOut != block->liveOut)
                {
                    changed = true;
                    block->liveIn.swap(newLiveIn);
                    block->liveOut.swap(newLiveOut);
                }
            }
        }
    }

    std::vector<BasicBlock *> LivenessAnalysis::buildRPO(BasicBlock *entry)
    {
        std::vector<BasicBlock *> order;
        if (!entry)
            return order;

        std::unordered_set<BasicBlock *> visited;

        std::stack<std::pair<BasicBlock *, bool>> stk;
        stk.push(std::make_pair(entry, false));

        while (!stk.empty())
        {
            auto top = stk.top();
            BasicBlock *bb = top.first;
            bool processed = top.second;
            stk.pop();

            if (processed)
            {
                order.push_back(bb);
                continue;
            }

            if (!visited.insert(bb).second)
                continue;

            // 先压入处理标记
            stk.push(std::make_pair(bb, true));

            // 再压入所有后继（逆序以保持原顺序）
            for (auto it = bb->succ.rbegin(); it != bb->succ.rend(); ++it)
            {
                stk.push(std::make_pair(*it, false));
            }
        }

        std::reverse(order.begin(), order.end()); // 反转得到 RPO
        return order;
    }

    /**
     * @brief 构造活跃区间构建器
     * @param F 函数IR引用
     * @param LA 活跃性分析引用
     * @param intervalSplittingEnabled 是否启用区间分割
     */
    LiveIntervalBuilder::LiveIntervalBuilder(FunctionIR &F, const LivenessAnalysis &LA, bool intervalSplittingEnabled)
        : F(F), LA(LA), intervalSplittingEnabled(intervalSplittingEnabled) {}

    /**
     * @brief 构建所有虚拟寄存器的活跃区间
     * @return 虚拟寄存器ID到活跃区间的映射
     */
    std::unordered_map<int, std::unique_ptr<LiveInterval>> LiveIntervalBuilder::build()
    {
        std::unordered_map<int, std::unique_ptr<LiveInterval>> intervals;

        // 为每个虚拟寄存器创建活跃区间
        for (int vreg = 0; vreg <= F.maxVregId; ++vreg)
        {
            auto interval = std::unique_ptr<LiveInterval>(new LiveInterval(vreg));
            if (intervalSplittingEnabled)
            {
                buildIntervalForVreg(vreg, interval);
            }
            else
            {
                buildSimplifiedIntervalForVreg(vreg, interval);
            }
            if (!interval->empty())
            {
                intervals[vreg] = std::move(interval);
            }
        }

        return intervals;
    }

    /**
     * @brief 为单个虚拟寄存器构建活跃区间
     * @param vreg 虚拟寄存器ID
     * @param interval 活跃区间对象
     */
    void LiveIntervalBuilder::buildIntervalForVreg(int vreg, std::unique_ptr<LiveInterval> &interval)
    {
        for (BasicBlock *block : F.blocksInOrder)
        {
            bool liveAtStart = (block->liveIn.find(vreg) != block->liveIn.end());
            bool liveAtEnd = (block->liveOut.find(vreg) != block->liveOut.end());

            if (!liveAtStart && !liveAtEnd)
            {
                // 检查是否在块内有def或use
                bool hasDefOrUse = false;
                for (auto &inst : block->insts)
                {
                    if (inst->def() == vreg)
                    {
                        hasDefOrUse = true;
                        break;
                    }
                    for (int use : inst->uses())
                    {
                        if (use == vreg)
                        {
                            hasDefOrUse = true;
                            break;
                        }
                    }
                }
                if (!hasDefOrUse)
                    continue;
            }

            int blockStart = block->firstPos();
            int blockEnd = block->lastPos();

            if (blockStart == -1 || blockEnd == -1)
                continue;

            // 如果在块入口活跃，从块开始扩展
            int rangeStart = liveAtStart ? blockStart : -1;
            int rangeEnd = liveAtEnd ? blockEnd : -1;

            // 扫描块内指令，精确计算活跃范围
            for (auto &inst : block->insts)
            {
                int defVreg = inst->def();
                if (defVreg == vreg)
                {
                    // 定义点，开始一个新的活跃范围
                    if (rangeStart == -1)
                        rangeStart = inst->posDef();

                    // 如果定义后还在块出口活跃，扩展到块末尾
                    if (liveAtEnd)
                        rangeEnd = blockEnd;
                    else
                        rangeEnd = inst->posDef(); // 仅在定义点活跃
                }

                for (int useVreg : inst->uses())
                {
                    if (useVreg == vreg)
                    {
                        // 使用点，扩展活跃范围
                        if (rangeStart == -1)
                            rangeStart = liveAtStart ? blockStart : inst->posUse();
                        rangeEnd = std::max(rangeEnd, inst->posUse());
                    }
                }
            }

            // 添加活跃范围
            if (rangeStart != -1 && rangeEnd != -1 && rangeStart <= rangeEnd)
            {
                interval->addRange(rangeStart, rangeEnd);
            }
        }
    }

    /**
     * @brief 为单个虚拟寄存器构建简化的活跃区间（不分割）
     * @param vreg 虚拟寄存器ID
     * @param interval 活跃区间对象
     */
    void LiveIntervalBuilder::buildSimplifiedIntervalForVreg(int vreg, std::unique_ptr<LiveInterval> &interval)
    {
        int minStart = INT_MAX;
        int maxEnd = INT_MIN;
        bool hasActivity = false;

        // 扫描所有基本块，找到该虚拟寄存器的最早定义/使用和最晚使用
        for (BasicBlock *block : F.blocksInOrder)
        {
            bool liveAtStart = (block->liveIn.find(vreg) != block->liveIn.end());
            bool liveAtEnd = (block->liveOut.find(vreg) != block->liveOut.end());

            if (!liveAtStart && !liveAtEnd)
            {
                // 检查是否在块内有def或use
                bool hasDefOrUse = false;
                for (auto &inst : block->insts)
                {
                    if (inst->def() == vreg)
                    {
                        hasDefOrUse = true;
                        break;
                    }
                    for (int use : inst->uses())
                    {
                        if (use == vreg)
                        {
                            hasDefOrUse = true;
                            break;
                        }
                    }
                }
                if (!hasDefOrUse)
                    continue;
            }

            int blockStart = block->firstPos();
            int blockEnd = block->lastPos();

            if (blockStart == -1 || blockEnd == -1)
                continue;

            // 如果在块入口活跃，从块开始计算
            if (liveAtStart)
            {
                minStart = std::min(minStart, blockStart);
                hasActivity = true;
            }

            // 如果在块出口活跃，延伸到块末尾
            if (liveAtEnd)
            {
                maxEnd = std::max(maxEnd, blockEnd);
                hasActivity = true;
            }

            // 扫描块内指令，找到实际的定义和使用位置
            for (auto &inst : block->insts)
            {
                int defVreg = inst->def();
                if (defVreg == vreg)
                {
                    minStart = std::min(minStart, inst->posDef());
                    maxEnd = std::max(maxEnd, inst->posDef());
                    hasActivity = true;
                }

                for (int useVreg : inst->uses())
                {
                    if (useVreg == vreg)
                    {
                        minStart = std::min(minStart, inst->posUse());
                        maxEnd = std::max(maxEnd, inst->posUse());
                        hasActivity = true;
                    }
                }
            }
        }

        // 如果有活动，添加一个覆盖整个活跃范围的区间
        if (hasActivity && minStart <= maxEnd)
        {
            interval->addRange(minStart, maxEnd);
        }
    }

#pragma endregion

#pragma region 线性扫描分配器实现

    /**
     * @brief 构造线性扫描分配器
     * @param regInfo 寄存器信息引用
     */
    LinearScanAllocator::LinearScanAllocator(const RegInfo &regInfo)
        : regInfo(regInfo), freePhysRegs(PhysRegComparator(&regInfo.physRegs))
    {
        // 初始化寄存器状态
        isPhysRegUsed.resize(regInfo.physRegs.size(), false);
        initializeFreeRegs();
    }

    /**
     * @brief 主分配接口
     * @param F 函数IR
     * @return 分配结果
     */
    AllocationResult LinearScanAllocator::allocate(FunctionIR &F)
    {
        // 重置状态
        active.clear();

        nextSpillSlot = 0;

        // 重新初始化寄存器状态
        initializeFreeRegs();

        // 1. 处理参数映射（在分配前处理）
        processParameters(F.parameters);

        // 2. 先进行活跃性分析（设置F.blocksInOrder）
        LivenessAnalysis liveness;
        liveness.run(F);

        // 3. 指令位置编号
        assignInstrPositions(F);

        // 4. 构建活跃区间
        LiveIntervalBuilder builder(F, liveness, intervalSplittingEnabled);
        auto intervals = builder.build();

        if (debugMode)
        {
            *debugOutput << "=== 活跃区间信息 ===" << std::endl;
            dumpIntervals(intervals);
        }

        // 5. 执行线性扫描分配
        result = runLinearScan(intervals);

        // 6. 计算指令位置的虚拟寄存器瞬时位置
        computeInstrVregLocations(F);

        return result;
    }

    /**
     * @brief 指令位置编号
     * @param F 函数IR
     */
    void LinearScanAllocator::assignInstrPositions(FunctionIR &F)
    {
        int pos = 0;
        for (BasicBlock *block : F.blocksInOrder)
        {
            for (auto &inst : block->insts)
            {
                inst->idx = pos++;
            }
        }

        if (debugMode)
        {
            *debugOutput << "=== 指令位置编号 ===" << std::endl;
            for (BasicBlock *block : F.blocksInOrder)
            {
                *debugOutput << "Block " << block->name << ":" << std::endl;
                for (auto &inst : block->insts)
                {
                    *debugOutput << "  " << inst->idx << ": " << inst->text << std::endl;
                }
            }
        }
    }

    /**
     * @brief 执行线性扫描分配
     * @param intervals 活跃区间映射
     * @return 分配结果
     */
    AllocationResult LinearScanAllocator::runLinearScan(
        const std::unordered_map<int, std::unique_ptr<LiveInterval>> &intervals)
    {
        // 将区间按起始位置排序
        std::vector<LiveInterval *> sortedIntervals;
        for (const auto &pair : intervals)
        {
            sortedIntervals.push_back(pair.second.get());
        }
        sortIntervalsByStart(sortedIntervals);

        if (debugMode)
        {
            *debugOutput << "=== 开始线性扫描分配 ===" << std::endl;
        }

        // 对每个区间执行分配
        for (LiveInterval *interval : sortedIntervals)
        {
            if (debugMode)
            {
                *debugOutput << "处理区间 %" << interval->vreg
                             << " [" << interval->start() << ", " << interval->end() << "]" << std::endl;
            }

            expireOldIntervals(interval->start());

            // 检查该虚拟寄存器是否已经被分配过物理寄存器
            if (allocatedVregs.find(interval->vreg) != allocatedVregs.end())
            {
                // 已经被分配过物理寄存器，直接溢出到栈
                spillAtInterval(*interval);
            }
            else if (freePhysRegs.empty())
            {
                // 没有空闲寄存器，需要溢出
                spillAtInterval(*interval);
            }
            else
            {
                // 分配物理寄存器
                allocatePhysicalReg(*interval);
                // 记录该虚拟寄存器已被分配过物理寄存器
                allocatedVregs.insert(interval->vreg);
            }
        }

        return result;
    }

    /**
     * @brief 释放已过期的活跃区间
     * @param curStart 当前区间起始位置
     */
    void LinearScanAllocator::expireOldIntervals(int curStart)
    {
        // active按结束位置排序，从前往后检查过期区间
        auto it = active.begin();
        while (it != active.end())
        {
            LiveInterval *interval = *it;
            if (interval->end() < curStart)
            {
                // 区间已过期，释放其物理寄存器
                if (interval->physReg != -1)
                {
                    freePhysReg(interval->physReg);
                    if (debugMode)
                    {
                        *debugOutput << "  释放寄存器 " << regInfo.getReg(interval->physReg).name
                                     << " (区间 %" << interval->vreg << ")" << std::endl;
                    }
                }
                it = active.erase(it);
            }
            else
            {
                // 由于active按结束位置排序，后续区间都不会过期
                break;
            }
        }
    }

    /**
     * @brief 为区间分配物理寄存器
     * @param interval 活跃区间
     */
    void LinearScanAllocator::allocatePhysicalReg(LiveInterval &interval)
    {
        // 分配优先级最高的空闲寄存器
        int physReg = allocatePhysReg();

        if (physReg == -1)
        {
            // 这不应该发生，因为调用前已检查
            spillAtInterval(interval);
            return;
        }

        interval.physReg = physReg;
        result.vregToPhys[interval.vreg] = physReg;

        // 插入到active列表中，保持按结束位置排序
        insertActiveInterval(&interval);

        if (debugMode)
        {
            *debugOutput << "  分配寄存器 " << regInfo.getReg(physReg).name
                         << " 给区间 %" << interval.vreg << std::endl;
        }
    }

    /**
     * @brief 在区间处执行溢出
     * @param interval 当前区间
     */
    void LinearScanAllocator::spillAtInterval(LiveInterval &interval)
    {
        if (active.empty())
        {
            // 直接溢出当前区间
            int spillSlot = allocateSpillSlot();
            interval.spillSlot = spillSlot;
            result.vregToPhys[interval.vreg] = -1;
            result.vregToStack[interval.vreg] = spillSlot;

            if (debugMode)
            {
                *debugOutput << "  溢出区间 %" << interval.vreg << " 到栈偏移 " << spillSlot << std::endl;
            }
            return;
        }

        // 选择溢出候选
        int spillCandidateIdx = chooseSpillCandidate(interval);
        LiveInterval *spillCandidate = active[spillCandidateIdx];

        if (spillCandidate->end() > interval.end())
        {
            // 溢出候选区间，将其寄存器分配给当前区间
            int physReg = spillCandidate->physReg;

            // 溢出候选区间
            int spillSlot = allocateSpillSlot();
            spillCandidate->spillSlot = spillSlot;
            spillCandidate->physReg = -1;
            result.vregToPhys[spillCandidate->vreg] = -1;
            result.vregToStack[spillCandidate->vreg] = spillSlot;

            // 从active中移除溢出的区间
            active.erase(active.begin() + spillCandidateIdx);

            // 将寄存器分配给当前区间
            interval.physReg = physReg;
            result.vregToPhys[interval.vreg] = physReg;
            insertActiveInterval(&interval);

            if (debugMode)
            {
                *debugOutput << "  溢出区间 %" << spillCandidate->vreg << " 到栈槽 " << spillSlot
                             << "，将寄存器 " << regInfo.getReg(physReg).name
                             << " 分配给区间 %" << interval.vreg << std::endl;
            }
        }
        else
        {
            // 溢出当前区间
            int spillSlot = allocateSpillSlot();
            interval.spillSlot = spillSlot;
            result.vregToPhys[interval.vreg] = -1;
            result.vregToStack[interval.vreg] = spillSlot;

            if (debugMode)
            {
                *debugOutput << "  溢出当前区间 %" << interval.vreg << " 到栈偏移 " << spillSlot << std::endl;
            }
        }
    }

    /**
     * @brief 选择溢出候选区间
     * @param current 当前区间
     * @return 溢出候选区间的索引
     */
    int LinearScanAllocator::chooseSpillCandidate(const LiveInterval & /* current */) const
    {
        // 选择结束位置最晚的区间作为溢出候选
        int maxEndPos = -1;
        int candidateIdx = 0;

        for (size_t i = 0; i < active.size(); ++i)
        {
            if (active[i]->end() > maxEndPos)
            {
                maxEndPos = active[i]->end();
                candidateIdx = static_cast<int>(i);
            }
        }

        return candidateIdx;
    }

    /**
     * @brief 分配新的溢出栈空间
     * @return 栈字节偏移（负数，相对于栈帧指针）
     */
    int LinearScanAllocator::allocateSpillSlot()
    {
        // 计算下一个可用的栈字节偏移
        // 栈向下增长，偏移为负数，每个变量4字节
        int nextOffset = -(static_cast<int>(result.vregToStack.size()) + 2) * 4;
        return nextOffset;
    }

    /**
     * @brief 将区间按起始位置排序
     * @param intervals 区间列表
     */
    // 比较函数对象，避免lambda表达式
    struct IntervalStartComparator
    {
        bool operator()(const LiveInterval *a, const LiveInterval *b) const
        {
            return a->start() < b->start();
        }
    };

    void LinearScanAllocator::sortIntervalsByStart(std::vector<LiveInterval *> &intervals)
    {
        std::sort(intervals.begin(), intervals.end(), IntervalStartComparator());
    }

    /**
     * @brief 将区间插入到active列表中，保持按结束位置排序
     * @param interval 活跃区间
     */
    // 比较函数对象，避免lambda表达式
    struct IntervalEndComparator
    {
        bool operator()(const LiveInterval *a, const LiveInterval *b) const
        {
            return a->end() < b->end();
        }
    };

    void LinearScanAllocator::insertActiveInterval(LiveInterval *interval)
    {
        auto it = std::lower_bound(active.begin(), active.end(), interval, IntervalEndComparator());
        active.insert(it, interval);
    }

    /**
     * @brief 从active列表中移除区间
     * @param interval 活跃区间
     */
    void LinearScanAllocator::removeFromActive(LiveInterval *interval)
    {
        auto it = std::find(active.begin(), active.end(), interval);
        if (it != active.end())
        {
            active.erase(it);
        }
    }

    /**
     * @brief 输出活跃区间信息（调试用）
     * @param intervals 活跃区间映射
     */
    void LinearScanAllocator::dumpIntervals(const std::unordered_map<int, std::unique_ptr<LiveInterval>> &intervals)
    {
        for (const auto &pair : intervals)
        {
            const LiveInterval *interval = pair.second.get();
            *debugOutput << "%" << interval->vreg << ": ";
            for (const auto &range : interval->ranges)
            {
                *debugOutput << "[" << range.start << "," << range.end << "] ";
            }
            *debugOutput << std::endl;
        }
    }

#pragma endregion

#pragma region LLVM IR解析接口实现
    /**
     * @brief 从LLVM IR构建函数IR
     * @param llvmIR LLVM IR字符串
     * @param funcName 函数名（默认查找第一个函数）
     * @return 构建的FunctionIR对象
     */
    std::unique_ptr<FunctionIR> parseFunctionFromLLVMIR(const std::string &llvmIR, const std::string &funcName)
    {
        auto functionIR = std::unique_ptr<FunctionIR>(new FunctionIR());
        std::istringstream iss(llvmIR);
        std::string line;

        BasicBlock *currentBlock = nullptr;
        bool inTargetFunction = false;
        std::string targetFuncName = funcName;

        // 正则表达式
        std::regex funcDefRegex(RegexPatterns::FUNC_DEF);
        std::regex labelRegex(RegexPatterns::LABEL);
        std::regex instRegex(RegexPatterns::INSTRUCTION);

        while (std::getline(iss, line))
        {
            // 去除前后空白
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            if (line.empty() || line[0] == ';')
                continue;

            std::smatch match;

            // 检查函数定义
            if (std::regex_search(line, match, funcDefRegex))
            {
                std::string foundFuncName = match.str(2);

                if (targetFuncName.empty() || foundFuncName == targetFuncName)
                {
                    functionIR->name = foundFuncName;
                    inTargetFunction = true;
                    // 不在这里创建基本块，等待遇到第一个标签时创建
                    currentBlock = nullptr;

                    // 解析函数参数
                    parseFunctionParameters(line, *functionIR);
                }
                continue;
            }

            if (!inTargetFunction)
                continue;

            // 检查函数结束
            if (line == "}")
            {
                inTargetFunction = false;
                break;
            }

            // 检查标签（新基本块）
            if (std::regex_match(line, match, labelRegex))
            {
                std::string labelName = match.str(1);

                auto newBlock = std::unique_ptr<BasicBlock>(new BasicBlock());
                newBlock->id = static_cast<int>(functionIR->blocks.size());
                newBlock->name = labelName;
                currentBlock = newBlock.get();
                functionIR->nameToBlock[labelName] = newBlock.get();
                functionIR->blocks.push_back(std::move(newBlock));
                continue;
            }

            // 处理普通指令
            if (std::regex_match(line, match, instRegex))
            {
                // 如果还没有当前基本块，创建一个默认的
                if (!currentBlock)
                {
                    auto defaultBlock = std::unique_ptr<BasicBlock>(new BasicBlock());
                    defaultBlock->id = static_cast<int>(functionIR->blocks.size());
                    defaultBlock->name = "entry";
                    currentBlock = defaultBlock.get();
                    functionIR->nameToBlock[defaultBlock->name] = defaultBlock.get();
                    functionIR->blocks.push_back(std::move(defaultBlock));
                }

                std::string instText = match.str(1);
                auto instruction = std::unique_ptr<Instruction>(new Instruction(instText));
                instruction->blockId = currentBlock->id;
                currentBlock->insts.push_back(std::move(instruction));
            }
        }

        // 如果没有找到目标函数
        if (!inTargetFunction && functionIR->blocks.empty())
        {
            return nullptr;
        }

        // 构建控制流图
        functionIR->buildControlFlowGraph();

        return functionIR;
    }

    /**
     * @brief 解析函数参数列表
     * @param funcDefLine 函数定义行
     * @param functionIR 函数IR对象
     */
    void parseFunctionParameters(const std::string &funcDefLine, FunctionIR &functionIR)
    {
        // 查找参数列表的开始和结束位置
        size_t startPos = funcDefLine.find('(');
        size_t endPos = funcDefLine.find(')', startPos);

        if (startPos == std::string::npos || endPos == std::string::npos)
            return;

        std::string paramStr = funcDefLine.substr(startPos + 1, endPos - startPos - 1);

        // 如果参数列表为空或只有void，直接返回
        if (paramStr.empty() || paramStr.find("void") != std::string::npos)
            return;

        // 解析参数列表
        std::regex paramRegex(RegexPatterns::VREG_USE);
        std::sregex_iterator iter(paramStr.begin(), paramStr.end(), paramRegex);
        std::sregex_iterator end;

        for (; iter != end; ++iter)
        {
            int paramVreg = std::stoi(iter->str(1));
            functionIR.parameters.push_back(paramVreg);
            functionIR.maxVregId = std::max(functionIR.maxVregId, paramVreg);
        }
    }

    /**
     * @brief 查询指令位置的虚拟寄存器位置
     * @param instrPos 指令位置
     * @param vreg 虚拟寄存器ID
     * @return 物理寄存器ID或栈槽索引，-1表示未找到
     */
    int LinearScanAllocator::getVregLocationAtInstr(int instrPos, int vreg) const
    {
        auto instrIt = result.instrToVregLocation.find(instrPos);
        if (instrIt != result.instrToVregLocation.end())
        {
            auto vregIt = instrIt->second.find(vreg);
            if (vregIt != instrIt->second.end())
            {
                return vregIt->second;
            }
        }
        return -1;
    }

    /**
     * @brief 查询虚拟寄存器定义时的位置
     * @param vreg 虚拟寄存器ID
     * @return 物理寄存器ID或栈槽索引，-1表示未找到
     */
    int LinearScanAllocator::getVregDefLocation(int vreg) const
    {
        auto physIt = result.vregToPhys.find(vreg);
        if (physIt != result.vregToPhys.end() && physIt->second != -1)
        {
            return physIt->second;
        }

        auto stackIt = result.vregToStack.find(vreg);
        if (stackIt != result.vregToStack.end())
        {
            return -(stackIt->second + 1); // 使用负数表示栈位置
        }

        return -1;
    }

    /**
     * @brief 处理函数参数映射
     * @param paramVregs 参数虚拟寄存器列表
     */
    void LinearScanAllocator::processParameters(const std::vector<int> &paramVregs)
    {
        int paramCount = paramVregs.size();
        int stackParamCount = paramCount - 8;

        // RV32调用约定：前8个参数通过a0-a7传递，其余通过栈传递
        for (size_t i = 0; i < std::min(8, static_cast<int>(paramCount)); ++i)
        {
            int vreg = paramVregs[i];

            // 使用a0-a7寄存器
            int argRegId = 10 + static_cast<int>(i); // a0是寄存器10
            result.paramVregToLocation[vreg] = argRegId;
            result.vregToPhys[vreg] = argRegId;
            result.usedPhysRegs.insert(argRegId);

            // 从空闲寄存器中移除，因为参数已经占用
            if (argRegId >= 0 && argRegId < static_cast<int>(isPhysRegUsed.size()))
            {
                isPhysRegUsed[argRegId] = true;
                freePhysRegs.erase(argRegId);
            }
        }
        if (paramCount > 8)
        {
            // 循环 stackParamCount 次，每次分配一个栈参数
            for (size_t i = stackParamCount, tmp = 1; i > 0; --i, ++tmp)
            {
                int vreg = paramVregs[i];
                int stackOffset = 4 * tmp;
                result.paramVregToLocation[vreg] = -1;  // 负数表示参数位于栈上，不使用物理寄存器
                result.vregToStack[vreg] = stackOffset; // 参数栈偏移为正数
            }
        }
    }

    /**
     * @brief 计算指令位置的虚拟寄存器瞬时位置
     * @param F 函数IR
     */
    void LinearScanAllocator::computeInstrVregLocations(FunctionIR &F)
    {
        // 遍历所有指令，记录每条指令执行时虚拟寄存器的瞬时位置
        for (BasicBlock *block : F.blocksInOrder)
        {
            for (auto &inst : block->insts)
            {
                int instrPos = inst->idx;
                std::unordered_map<int, int> vregLocs;

                // 检查所有虚拟寄存器在此指令位置的状态
                for (const auto &pair : result.vregToPhys)
                {
                    int vreg = pair.first;
                    int physReg = pair.second;

                    if (physReg != -1)
                    {
                        // 检查此虚拟寄存器在此指令位置是否活跃
                        // 这里简化处理，直接记录分配结果
                        vregLocs[vreg] = physReg;
                    }
                }

                // 记录溢出到栈的虚拟寄存器
                for (const auto &pair : result.vregToStack)
                {
                    int vreg = pair.first;
                    int stackSlot = pair.second;
                    vregLocs[vreg] = -(stackSlot + 1); // 负数表示栈位置
                }

                result.instrToVregLocation[instrPos] = vregLocs;
            }
        }
    }

    /**
     * @brief 获取函数使用的所有物理寄存器集合
     * @return 使用的物理寄存器集合
     */
    std::set<int> LinearScanAllocator::getUsedPhysRegs() const
    {
        std::set<int> usedRegs;
        for (const auto &pair : result.vregToPhys)
        {
            if (pair.second != -1)
            {
                usedRegs.insert(pair.second);
            }
        }
        return usedRegs;
    }

    /**
     * @brief 获取被调用者保护的寄存器集合
     * @return 被调用者保护的寄存器集合
     */
    std::set<int> LinearScanAllocator::getCalleeSavedRegs() const
    {
        std::set<int> calleeSaved;
        for (const auto &pair : result.vregToPhys)
        {
            if (pair.second != -1 && regInfo.isCalleeSaved(pair.second))
            {
                calleeSaved.insert(pair.second);
            }
        }
        return calleeSaved;
    }

    /**
     * @brief 初始化空闲寄存器集合
     */
    void LinearScanAllocator::initializeFreeRegs()
    {
        freePhysRegs.clear();
        std::fill(isPhysRegUsed.begin(), isPhysRegUsed.end(), false);

        for (int regId : regInfo.allocatableRegs)
        {
            if (regId >= 0 && regId < static_cast<int>(regInfo.physRegs.size()) &&
                !regInfo.physRegs[regId].reserved)
            {
                freePhysRegs.insert(regId);
                isPhysRegUsed[regId] = false;
            }
        }
    }

    /**
     * @brief 分配优先级最高的空闲寄存器
     * @return 物理寄存器ID，-1表示无可用寄存器
     */
    int LinearScanAllocator::allocatePhysReg()
    {
        if (freePhysRegs.empty())
            return -1;

        // 由于set按优先级排序，第一个就是优先级最高的
        int bestReg = *freePhysRegs.begin();
        freePhysRegs.erase(freePhysRegs.begin());

        if (bestReg >= 0 && bestReg < static_cast<int>(isPhysRegUsed.size()))
        {
            isPhysRegUsed[bestReg] = true;
        }

        return bestReg;
    }

    /**
     * @brief 释放寄存器
     * @param physId 物理寄存器ID
     */
    void LinearScanAllocator::freePhysReg(int physId)
    {
        if (physId >= 0 && physId < static_cast<int>(isPhysRegUsed.size()) &&
            !regInfo.isReserved(physId))
        {
            isPhysRegUsed[physId] = false;
            freePhysRegs.insert(physId);
        }
    }

    /**
     * @brief 检查寄存器是否空闲
     * @param physId 物理寄存器ID
     * @return true表示寄存器空闲
     */
    bool LinearScanAllocator::isRegFree(int physId) const
    {
        if (physId < 0 || physId >= static_cast<int>(isPhysRegUsed.size()))
            return false;
        return !isPhysRegUsed[physId];
    }

    /**
     * @brief 分配溢出临时寄存器（循环选择）
     * @return 溢出临时寄存器ID，-1表示无可用寄存器
     */
    int LinearScanAllocator::allocateSpillTempReg()
    {
        // 循环选择溢出临时寄存器
        int selectedReg = -1;

        if (spillTempCounter)
        {
            selectedReg = spillTempReg1;
        }
        else
        {
            selectedReg = spillTempReg2;
        }

        // 更新计数器
        spillTempCounter = !spillTempCounter;

        return selectedReg;
    }

    /**
     * @brief 检查是否是溢出临时寄存器
     * @param regId 寄存器ID
     * @return true表示是溢出临时寄存器
     */
    bool LinearScanAllocator::isSpillTempReg(int regId) const
    {
        return regId == spillTempReg1 || regId == spillTempReg2;
    }

#pragma endregion
}
