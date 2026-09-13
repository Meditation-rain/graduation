//
// Created by qiming on 25-4-12.
//
#include "Graph.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include "Instance.h"
#include "Random.h"

std::deque<int> Graph::topological_sort(bool reverse) const
{
    std::vector<int> in_degree(node_num, 0);
    int first_node = reverse ? node_num - 1 : 0;

    // Calculate in-degree for each node
    for (int i = 0; i < node_num; ++i)
    {
        if (reverse)
        {
            in_degree[i] = (machine_successor[i] != -1) + (job_successor[i] != -1);
        }
        else
        {
            in_degree[i] = static_cast<int>(machine_predecessor[i] != -1) + static_cast<int>(job_predecessor[i] != -1);
        }
    }

    std::deque<int> result; // Topological order result
    std::deque<int> candidates; // Nodes with zero in-degree
    candidates.push_back(first_node);

    while (true)
    {
        if (candidates.empty())
        {
            throw std::runtime_error("Graph contains a cycle");
        }

        int curr = candidates.front();
        candidates.pop_front();
        result.push_back(curr);

        if (result.size() == node_num - 1)
        {
            if (reverse)
            {
                result.push_back(0);
            }
            else
            {
                result.push_back(node_num - 1);
            }
            break;
        }

        if (curr == first_node)
        {
            if (reverse)
            {
                for (const int node : last_job_operation)
                {
                    in_degree[node]--;
                    if (in_degree[node] == 0)
                    {
                        candidates.push_back(node);
                    }
                }
            }
            else
            {
                for (const int node : first_job_operation)
                {
                    in_degree[node]--;
                    if (in_degree[node] == 0)
                    {
                        candidates.push_back(node);
                    }
                }
            }
        }
        else
        {
            if (reverse)
            {
                if (job_predecessor[curr] != -1)
                {
                    in_degree[job_predecessor[curr]]--;
                    if (in_degree[job_predecessor[curr]] == 0)
                    {
                        candidates.push_back(job_predecessor[curr]);
                    }
                }

                if (machine_predecessor[curr] != -1)
                {
                    in_degree[machine_predecessor[curr]]--;
                    if (in_degree[machine_predecessor[curr]] == 0)
                    {
                        candidates.push_back(machine_predecessor[curr]);
                    }
                }
            }
            else
            {
                if (job_successor[curr] != -1)
                {
                    in_degree[job_successor[curr]]--;
                    if (in_degree[job_successor[curr]] == 0)
                    {
                        candidates.push_back(job_successor[curr]);
                    }
                }

                if (machine_successor[curr] != -1)
                {
                    in_degree[machine_successor[curr]]--;
                    if (in_degree[machine_successor[curr]] == 0)
                    {
                        candidates.push_back(machine_successor[curr]);
                    }
                }
            }
        }
    }
    return result;
}

void Graph::random_init(const Instance& instance, const OperationList& operation_list)
{
    const int job_num = instance.job_num;
    const int machine_num = instance.machine_num;
    this->node_num = static_cast<int>(operation_list.operations.size());

    this->job_successor.assign(this->node_num, -1);
    this->machine_successor.assign(this->node_num, -1);
    this->job_predecessor.assign(this->node_num, -1);
    this->machine_predecessor.assign(this->node_num, -1);

    this->first_job_operation.assign(job_num, -1);
    this->last_job_operation.assign(job_num, -1);
    this->first_machine_operation.assign(machine_num, -1);
    this->last_machine_operation.assign(machine_num, -1);

    this->machine_operation_count.assign(machine_num, 0);

    on_machine.resize(this->node_num, -1);

    // 建立工件边
    for (int job_id = 0, op_id = 1; job_id < job_num; ++job_id)
    {
        for (int op_index = 0; op_index < instance.jobs[job_id].size(); ++op_index)
        {
            if (op_index == 0)
            {
                first_job_operation[job_id] = op_id;
                job_predecessor[op_id] = 0;
                job_successor[op_id] = op_id + 1;
            }
            else if (op_index == instance.jobs[job_id].size() - 1)
            {
                last_job_operation[job_id] = op_id;
                job_predecessor[op_id] = op_id - 1;
                job_successor[op_id] = this->node_num - 1;
            }
            else
            {
                job_successor[op_id] = op_id + 1;
                job_predecessor[op_id] = op_id - 1;
            }
            ++op_id;
        }
    }

    // 建立机器边
    // 先分配机器
    // 当前可调度的操作队列，初始情况为所有工件的第一个工序
    std::vector<int> candidates = first_job_operation;

    // 统一随机源：由 rng 模块按全局种子惰性初始化，保证实验可复现
    auto& gen = rng::engine();

    while (!candidates.empty())
    {
        // 随机选择一个候选操作
        std::uniform_int_distribution<> dist_cand(0, candidates.size() - 1);
        const int index = dist_cand(gen);
        const int curr_op = candidates[index];
        // 为其分配一个机器
        const auto& curr_candidates = operation_list[curr_op].candidates;
        // === 修改：替换 RAND_INT(curr_candidates.size()) ===
        std::uniform_int_distribution<> dist_mach(0, curr_candidates.size() - 1);
        const int curr_machine = curr_candidates[dist_mach(gen)];        on_machine[curr_op] = curr_machine;
        machine_operation_count[curr_machine]++;
        // 如果当前操作是机器上的第一个操作，将其加入 first_machine_operation
        if (first_machine_operation[curr_machine] == -1)
        {
            first_machine_operation[curr_machine] = curr_op;
            last_machine_operation[curr_machine] = curr_op;
        }
        else
        {
            int pre_machine_op = last_machine_operation[curr_machine];
            last_machine_operation[curr_machine] = curr_op;
            machine_successor[pre_machine_op] = curr_op;
            machine_predecessor[curr_op] = pre_machine_op;
        }
        // 如果当前操作是工件的最后一个工序,将其从候选操作队列中移除
        if (job_successor[curr_op] == this->node_num - 1)
        {
            candidates[index] = candidates.back();
            candidates.pop_back();
        }
        else
        {
            candidates[index] = job_successor[curr_op];
        }
    }
}

void Graph::make_move(const NeighborhoodMove& move)
{
    // 【核心修复】：防止自我插入引发的自环破坏双向链表
    if (move.which == move.where) {
        return;
    }
    // ==========================================
    // 1. 换机器的动作处理 (CHANGE_MACHINE)
    // ==========================================
    if (move.method == Method::CHANGE_MACHINE_BACK || move.method == Method::CHANGE_MACHINE_FRONT)
    {
        int u_insert = -1; // 插入点的前一个工序
        int v_insert = -1; // 插入点的后一个工序

        const auto old_machine = on_machine[move.which];
        // 【核心修复】：使用 target_machine，防止目标机器为空时 move.where 为 -1 导致崩溃
        const auto new_machine = move.target_machine != -1 ? move.target_machine : on_machine[move.where];

        machine_operation_count[new_machine]++;
        machine_operation_count[old_machine]--;

        // 确定新机器上的插入位置
        if (move.method == Method::CHANGE_MACHINE_BACK)
        {
            u_insert = move.where;
            v_insert = (u_insert != -1) ? machine_successor[u_insert] : first_machine_operation[new_machine];
        }
        else // CHANGE_MACHINE_FRONT
        {
            v_insert = move.where;
            u_insert = (v_insert != -1) ? machine_predecessor[v_insert] : last_machine_operation[new_machine];
        }

        on_machine[move.which] = new_machine;

        // 【安全摘除】：从旧机器的双向链表中干净地摘除 move.which
        const auto old_mp = machine_predecessor[move.which];
        const auto old_ms = machine_successor[move.which];

        if (old_mp != -1) machine_successor[old_mp] = old_ms;
        else first_machine_operation[old_machine] = old_ms; // 如果是头部，更新头指针

        if (old_ms != -1) machine_predecessor[old_ms] = old_mp;
        else last_machine_operation[old_machine] = old_mp; // 如果是尾部，更新尾指针

        // 【安全插入】：将 move.which 插入新机器链表的正确位置
        if (u_insert != -1) machine_successor[u_insert] = move.which;
        else first_machine_operation[new_machine] = move.which;

        machine_predecessor[move.which] = u_insert;
        machine_successor[move.which] = v_insert;

        if (v_insert != -1) machine_predecessor[v_insert] = move.which;
        else last_machine_operation[new_machine] = move.which;

        return; // 换机器动作处理结束，直接返回
    }

    // ==========================================
    // 2. 同机器内的移动处理 (FRONT & BACK)
    // ==========================================
    const int u = move.which;
    const int m = on_machine[u]; // 在同机器移动下，机器 ID 不变

    // 获取当前被移动工序 u 的相邻节点
    const int ms_u = this->machine_successor[u];
    const int mp_u = this->machine_predecessor[u];

    // 【安全摘除】：将 u 从原来的位置干净地取出来，自动缝合两端
    if (mp_u != -1) this->machine_successor[mp_u] = ms_u;
    else this->first_machine_operation[m] = ms_u;

    if (ms_u != -1) this->machine_predecessor[ms_u] = mp_u;
    else this->last_machine_operation[m] = mp_u;

    // 【安全插入】：将 u 插入到目标位置
    if (move.method == Method::FRONT)
    {
        int w = move.where;
        // 注意：因为 u 已经被摘除，这里的 w 的前驱绝对不会是 u 了
        int mp_w = this->machine_predecessor[w];

        if (mp_w != -1) this->machine_successor[mp_w] = u;
        else this->first_machine_operation[m] = u;

        this->machine_predecessor[u] = mp_w;
        this->machine_successor[u] = w;
        this->machine_predecessor[w] = u;
    }
    else if (move.method == Method::BACK)
    {
        int v = move.where;
        // 同理，v 的后继已经没有 u 了
        int ms_v = this->machine_successor[v];

        if (ms_v != -1) this->machine_predecessor[ms_v] = u;
        else this->last_machine_operation[m] = u;

        this->machine_successor[u] = ms_v;
        this->machine_predecessor[u] = v;
        this->machine_successor[v] = u;
    }
}

// === 新增：在 Graph 类中添加此方法计算包含 SDST 的 Makespan ===
int Graph::calculate_makespan_with_sdst(const OperationList& op_list) const {
    // 1. 获取拓扑排序序列 (确保不会死锁，按依赖顺序计算)
    std::deque<int> topo_order = topological_sort(false);

    // 记录每个工序的完工时间 (Completion Time)
    std::vector<int> completion_time(node_num, 0);
    int makespan = 0;

    // 2. 按照拓扑顺序，逐个计算开工和完工时间
    for (int u : topo_order) {
        // 虚拟起始节点和虚拟终止节点不计算时间
        if (u == 0 || u == node_num - 1) continue;

        int job_pred = job_predecessor[u];
        int mach_pred = machine_predecessor[u];
        int machine_id = on_machine[u];

        // 决定条件A：同工件的前一个工序的完工时间
        int t_job = (job_pred == 0) ? 0 : completion_time[job_pred];

        // 决定条件B：同机器的前一个工序的完工时间 + 序列相关准备时间 (SDST)
        int t_mach = 0;
        if (mach_pred != -1) {
            // 这里调用了我们之前在 OperationList 中新增的 setup_time 函数！
            int setup_t = op_list.setup_time(mach_pred, u, machine_id);
            t_mach = completion_time[mach_pred] + setup_t;
        }

        // 开工时间必须是 满足工件约束 和 满足机器约束 的最大值
        int start_time = std::max(t_job, t_mach);

        // 完工时间 = 开工时间 + 机器上的实际加工时间
        int p_time = op_list.duration(u, machine_id);
        completion_time[u] = start_time + p_time;

        // 更新全局最大完工时间
        makespan = std::max(makespan, completion_time[u]);
    }

    return makespan;
}
// === 新增：贪心启发式初始化算法 ===
// void Graph::heuristic_init(const Instance& instance, const OperationList& operation_list)
// {
//     const int job_num = instance.job_num;
//     const int machine_num = instance.machine_num;
//     this->node_num = static_cast<int>(operation_list.operations.size());
//
//     this->job_successor.assign(this->node_num, -1);
//     this->machine_successor.assign(this->node_num, -1);
//     this->job_predecessor.assign(this->node_num, -1);
//     this->machine_predecessor.assign(this->node_num, -1);
//
//     this->first_job_operation.assign(job_num, -1);
//     this->last_job_operation.assign(job_num, -1);
//     this->first_machine_operation.assign(machine_num, -1);
//     this->last_machine_operation.assign(machine_num, -1);
//
//     this->machine_operation_count.assign(machine_num, 0);
//
//     on_machine.resize(this->node_num, -1);
//
//     // 用于贪心计算的可用时间跟踪数组
//     std::vector<int> machine_avail_time(machine_num, 0);
//     std::vector<int> node_avail_time(this->node_num, 0);
//
//     // 建立工件边
//     for (int job_id = 0, op_id = 1; job_id < job_num; ++job_id)
//     {
//         for (int op_index = 0; op_index < instance.jobs[job_id].size(); ++op_index)
//         {
//             if (op_index == 0)
//             {
//                 first_job_operation[job_id] = op_id;
//                 job_predecessor[op_id] = 0;
//                 job_successor[op_id] = op_id + 1;
//             }
//             else if (op_index == instance.jobs[job_id].size() - 1)
//             {
//                 last_job_operation[job_id] = op_id;
//                 job_predecessor[op_id] = op_id - 1;
//                 job_successor[op_id] = this->node_num - 1;
//             }
//             else
//             {
//                 job_successor[op_id] = op_id + 1;
//                 job_predecessor[op_id] = op_id - 1;
//             }
//             ++op_id;
//         }
//     }
//
//     // 建立机器边
//     std::vector<int> candidates = first_job_operation;
//
//     thread_local static std::random_device rd;
//     thread_local static std::mt19937 gen(rd());
//
//     while (!candidates.empty())
//     {
//         // 1. 在可调度工序中随机选择一个
//         std::uniform_int_distribution<> dist_cand(0, candidates.size() - 1);
//         const int index = dist_cand(gen);
//         const int curr_op = candidates[index];
//
//         // 2. 获取当前工序的前置工序完工时间
//         int prev_op = job_predecessor[curr_op];
//         int job_ready_time = (prev_op == 0) ? 0 : node_avail_time[prev_op];
//
//         // 3. 贪心遍历候选机器，选择能最早完工的机器
//         const auto& curr_candidates = operation_list[curr_op].candidates;
//         int best_machine = -1;
//         int min_end_time = INT_MAX;
//
//         // 【Bug修复】：curr_candidates 存储的是机器的ID (int)，时长需要通过 operation_list 查询
//         for (int m : curr_candidates)
//         {
//             // 获取工序在特定机器上的加工时间
//             int p = operation_list.duration(curr_op, m);
//
//             // 计算如果在机器 m 上加工的实际开工时间和完工时间
//             int start_time = std::max(job_ready_time, machine_avail_time[m]);
//             int end_time = start_time + p;
//
//             if (end_time < min_end_time)
//             {
//                 min_end_time = end_time;
//                 best_machine = m;
//             }
//         }
//
//         // 4. 执行分配
//         int curr_machine = best_machine;
//         on_machine[curr_op] = curr_machine;
//         machine_operation_count[curr_machine]++;
//
//         machine_avail_time[curr_machine] = min_end_time;
//         node_avail_time[curr_op] = min_end_time;
//
//         // 5. 维护双向链表拓扑指针
//         if (first_machine_operation[curr_machine] == -1)
//         {
//             first_machine_operation[curr_machine] = curr_op;
//             last_machine_operation[curr_machine] = curr_op;
//         }
//         else
//         {
//             int pre_machine_op = last_machine_operation[curr_machine];
//             last_machine_operation[curr_machine] = curr_op;
//             machine_successor[pre_machine_op] = curr_op;
//             machine_predecessor[curr_op] = pre_machine_op;
//         }
//
//         // 6. 推进工序进度
//         if (job_successor[curr_op] == this->node_num - 1)
//         {
//             candidates[index] = candidates.back();
//             candidates.pop_back();
//         }
//         else
//         {
//             candidates[index] = job_successor[curr_op];
//         }
//     }
// }
void Graph::heuristic_init(const Instance& instance, const OperationList& operation_list)
{
    const int job_num = instance.job_num;
    const int machine_num = instance.machine_num;
    this->node_num = static_cast<int>(operation_list.operations.size());

    this->job_successor.assign(this->node_num, -1);
    this->machine_successor.assign(this->node_num, -1);
    this->job_predecessor.assign(this->node_num, -1);
    this->machine_predecessor.assign(this->node_num, -1);

    this->first_job_operation.assign(job_num, -1);
    this->last_job_operation.assign(job_num, -1);
    this->first_machine_operation.assign(machine_num, -1);
    this->last_machine_operation.assign(machine_num, -1);

    this->machine_operation_count.assign(machine_num, 0);

    on_machine.resize(this->node_num, -1);

    std::vector<int> machine_avail_time(machine_num, 0);
    std::vector<int> node_avail_time(this->node_num, 0);
    std::vector<int> machine_last_job(machine_num, -1); // 每台机器上最后加工的工件（用于 SDST）

    // 建立工件边
    for (int job_id = 0, op_id = 1; job_id < job_num; ++job_id)
    {
        for (int op_index = 0; op_index < instance.jobs[job_id].size(); ++op_index)
        {
            if (op_index == 0) {
                first_job_operation[job_id] = op_id;
                job_predecessor[op_id] = 0;
                job_successor[op_id] = op_id + 1;
            } else if (op_index == instance.jobs[job_id].size() - 1) {
                last_job_operation[job_id] = op_id;
                job_predecessor[op_id] = op_id - 1;
                job_successor[op_id] = this->node_num - 1;
            } else {
                job_successor[op_id] = op_id + 1;
                job_predecessor[op_id] = op_id - 1;
            }
            ++op_id;
        }
    }

    // 建立机器边
    std::vector<int> candidates = first_job_operation;

    // 统一随机源：由 rng 模块按全局种子惰性初始化，保证实验可复现
    auto& gen = rng::engine();

    // =========================================================
    // 分派规则：MWKR (Most Work Remaining，剩余工作量最大优先)
    // 原实现在可调度工序里做均匀分布随机，等价于纯随机工序顺序，
    // 初始解质量很差（dpp01a 初始 5002 vs 最优 2505），全靠禁忌搜索兜底。
    // 这里改为按“该工序所属工件的剩余工作量”降序取前 K 个再随机，
    // 保留 GRASP 的随机性（多次重启仍能产生不同初始解），但起点大幅降低。
    // =========================================================
    std::vector<double> job_remaining_work(job_num, 0.0);
    for (int job_id = 0; job_id < job_num; ++job_id)
    {
        double work = 0.0;
        for (int op = first_job_operation[job_id];
             op != -1 && op != this->node_num - 1;
             op = job_successor[op])
        {
            const auto& cands = operation_list[op].candidates;
            double avg = 0.0;
            for (int m : cands) avg += operation_list.duration(op, m);
            if (!cands.empty()) avg /= static_cast<double>(cands.size());
            work += avg;
        }
        job_remaining_work[job_id] = work;
    }

    while (!candidates.empty())
    {
        // --- 工序选择：MWKR 前 K 个里随机（20% 概率完全随机，保持多样性）---
        int index = 0;
        const bool explore = std::uniform_int_distribution<>(0, 99)(gen) < 20;
        if (explore)
        {
            index = std::uniform_int_distribution<>(0, static_cast<int>(candidates.size()) - 1)(gen);
        }
        else
        {
            std::vector<int> order = candidates;
            std::sort(order.begin(), order.end(), [&](int a, int b)
            {
                const double wa = job_remaining_work[operation_list[a].job_id];
                const double wb = job_remaining_work[operation_list[b].job_id];
                if (wa != wb) return wa > wb;
                return a < b; // 保证排序稳定、结果可复现
            });
            const int k_limit = std::min<int>(static_cast<int>(order.size()), 3);
            const int pick = std::uniform_int_distribution<>(0, k_limit - 1)(gen);
            const int chosen = order[pick];
            index = static_cast<int>(std::find(candidates.begin(), candidates.end(), chosen) - candidates.begin());
        }
        const int curr_op = candidates[index];

        int prev_op = job_predecessor[curr_op];
        int job_ready_time = (prev_op == 0) ? 0 : node_avail_time[prev_op];

        // 调度走 curr_op 后，该工件剩余工作量减少
        {
            const auto& cands = operation_list[curr_op].candidates;
            double avg = 0.0;
            for (int m : cands) avg += operation_list.duration(curr_op, m);
            if (!cands.empty()) avg /= static_cast<double>(cands.size());
            job_remaining_work[operation_list[curr_op].job_id] -= avg;
        }

        const auto& curr_candidates = operation_list[curr_op].candidates;
        const int curr_job = operation_list[curr_op].job_id;

        // =========================================================
        // 半贪心半随机（GRASP），收集所有选项并排序
        // 机器选择的完工时间已计入 SDST 准备时间
        // =========================================================
        struct MachineChoice { int machine; int end_time; };
        std::vector<MachineChoice> choices;

        for (int m : curr_candidates)
        {
            int p = operation_list.duration(curr_op, m);
            int start_time = std::max(job_ready_time, machine_avail_time[m]);
            if (!instance.sdst_matrix.empty() && machine_last_job[m] != -1) {
                start_time += instance.get_setup_time(m, machine_last_job[m], curr_job);
            }
            choices.push_back({m, start_time + p});
        }

        // 按完工时间从小到大排序
        std::sort(choices.begin(), choices.end(), [](const MachineChoice& a, const MachineChoice& b) {
            return a.end_time < b.end_time;
        });

        // 在前 K=2 个最好的机器中随机挑一个（如果候选机器只有1个，就选第1个）
        int k_limit = std::min(static_cast<int>(choices.size()), 2);
        std::uniform_int_distribution<> dist_topk(0, k_limit - 1);
        int selected_idx = dist_topk(gen);

        int curr_machine = choices[selected_idx].machine;
        int min_end_time = choices[selected_idx].end_time;
        // =========================================================

        on_machine[curr_op] = curr_machine;
        machine_operation_count[curr_machine]++;
        machine_avail_time[curr_machine] = min_end_time;
        node_avail_time[curr_op] = min_end_time;
        machine_last_job[curr_machine] = curr_job;

        if (first_machine_operation[curr_machine] == -1) {
            first_machine_operation[curr_machine] = curr_op;
            last_machine_operation[curr_machine] = curr_op;
        } else {
            int pre_machine_op = last_machine_operation[curr_machine];
            last_machine_operation[curr_machine] = curr_op;
            machine_successor[pre_machine_op] = curr_op;
            machine_predecessor[curr_op] = pre_machine_op;
        }

        if (job_successor[curr_op] == this->node_num - 1) {
            candidates[index] = candidates.back();
            candidates.pop_back();
        } else {
            candidates[index] = job_successor[curr_op];
        }
    }
}