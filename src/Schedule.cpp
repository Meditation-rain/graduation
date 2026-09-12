//
// Created by chenshiyu on 26-3-31.
//

#include "Schedule.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <queue>
#include <ranges>
#include <unordered_set>
#include "NeighborhoodMove.h"

void Schedule::update_time()
{
    if (instance == nullptr) {
        std::cout << "警告: Schedule 中的 instance 指针为空！" << std::endl;
    }
    // 正向拓扑排序计算r值
    int n = graph.node_num;
    time_info.resize(n);
    auto forward_queue = graph.topological_sort(false); // Forward topological order
    makespan = 0;
    for (int i = 1; i < n - 1; i++)
    {
        int curr_node = forward_queue[i];
        int start_time = 0;
        int prev_op_id = graph.job_predecessor[curr_node];
        int prev_machine_id = graph.machine_predecessor[curr_node];
        // === 1. 单独计算并保存 r_u^J (作业路径长度) ===
        time_info[curr_node].r_job = 0;
        if (prev_op_id != -1)
        {
            time_info[curr_node].r_job = time_info[prev_op_id].end_time;
        }

        // === 2. 单独计算并保存 r_u^M (机器路径长度，包含 Setup) ===
        time_info[curr_node].r_machine = 0;
        if (prev_machine_id != -1)
        {
            int machine_ready_time = time_info[prev_machine_id].end_time;
            if (instance != nullptr)
            {
                int m = graph.on_machine[curr_node];
                int curr_job = (*operation_list)[curr_node].job_id;
                int prev_job = (*operation_list)[prev_machine_id].job_id;
                machine_ready_time += instance->sdst_matrix[m][prev_job][curr_job];
            }
            time_info[curr_node].r_machine = machine_ready_time;
        }

        // === 3. 综合的 start_time 就是这两者的最大值 ===
        start_time = std::max(time_info[curr_node].r_job, time_info[curr_node].r_machine);

        // Update node time information
        time_info[curr_node].operator_id = curr_node;
        time_info[curr_node].forward_path_length = start_time;
        const int end_time = start_time + (*operation_list)[curr_node][graph.on_machine[curr_node]];

        // Update makespan if needed
        if (end_time > makespan)
        {
            makespan = end_time;
        }
        time_info[curr_node].end_time = end_time;
    }

    // 逆向拓扑排序计算q值
    // auto backward_queue = graph.topological_sort(true); // Reverse topological order
    for (int i = n - 2; i > 0; --i)
    {
        int op = forward_queue[i];
        int js = graph.job_successor[op];
        int ms = graph.machine_successor[op];

        // === 1. 单独计算并保存 q_u^J (作业尾部时间) ===
        time_info[op].q_job = 0;
        if (js != n - 1)
        {
            time_info[op].q_job = time_info[js].backward_path_length + (*operation_list)[js][graph.on_machine[js]];
        }

        // === 2. 单独计算并保存 q_u^M (机器尾部时间，包含 Setup) ===
        time_info[op].q_machine = 0;
        if (ms != -1)
        {
            int next_machine_q = time_info[ms].backward_path_length + (*operation_list)[ms][graph.on_machine[ms]];

            // 逆向计算也必须加上 Setup，否则 IsCritical 判断会出错
            if (instance != nullptr)
            {
                int m = graph.on_machine[ms];
                int curr_job = (*operation_list)[op].job_id;
                int next_job = (*operation_list)[ms].job_id;
                next_machine_q += instance->sdst_matrix[m][curr_job][next_job];
            }
            time_info[op].q_machine = next_machine_q;
        }

        // === 3. 综合的 q 值就是两者的最大值 ===
        time_info[op].backward_path_length = std::max(time_info[op].q_job, time_info[op].q_machine);
    }

}

void Schedule::export_schedule(const char* filename) const
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + std::string(filename));
    }
    // Write CSV header
    file << "ID,Job,Operation,Machine,StartTime,EndTime,IsCritical" << std::endl;
    // Write operation data
    for (int i = 1; i < graph.node_num - 1; i++)
    {
        file << i << "," << (*operation_list)[i].job_id << "," << (*operation_list)[i].index << ","
             << graph.on_machine[i] << "," << time_info[i].forward_path_length << "," << time_info[i].end_time << ","
             << is_critical_operation(i) << std::endl;
    }

    file.close();
    std::clog << "Schedule exported to " << filename << std::endl;
}

// bool Schedule::is_legal_move(const NeighborhoodMove& move) const
// {
//     int u = move.which;
//     int v = -1; // 目标位置的前一个工序
//     int w = -1; // 目标位置的后一个工序
//
//     // 1. 根据移动类型，确定 v 和 w 的身份
//     if (move.method == Method::BACK || move.method == Method::CHANGE_MACHINE_BACK)
//     {
//         v = move.where;
//         w = graph.machine_successor[v]; // u 插在 v 后面，所以 w 是 v 原来的机器后继
//     }
//     else if (move.method == Method::FRONT || move.method == Method::CHANGE_MACHINE_FRONT)
//     {
//         w = move.where;
//         v = graph.machine_predecessor[w]; // u 插在 w 前面，所以 v 是 w 原来的机器前驱
//     }
//
//     // 获取 u 的作业前驱和后继
//     int u_prev_job = graph.job_predecessor[u];
//     int u_next_job = graph.job_successor[u];
//
//     // --- 校验条件 1: max{r_x, r_{v-1}} < r_{u+1} + p_{u+1} ---
//     if (v != -1 && u_next_job != -1) // 如果 v 存在且 u 不是作业的最后一道工序
//     {
//         int x = graph.machine_predecessor[v];
//         int v_prev_job = graph.job_predecessor[v];
//
//         int r_x = (x != -1) ? time_info[x].forward_path_length : 0;
//         int r_v_prev_job = (v_prev_job != -1) ? time_info[v_prev_job].forward_path_length : 0;
//
//         int max_r = std::max(r_x, r_v_prev_job);
//         // time_info 里的 end_time 已经等价于 r + p
//         if (max_r >= time_info[u_next_job].end_time) {
//             return false;
//         }
//     }
//
//     // --- 校验条件 2: min{r_y + p_y, r_{w+1} + p_{w+1}} > r_{u-1} ---
//     if (w != -1 && u_prev_job != -1) // 如果 w 存在且 u 不是作业的第一道工序
//     {
//         int y = graph.machine_successor[w];
//         int w_next_job = graph.job_successor[w];
//
//         int r_y_plus_p_y = (y != -1) ? time_info[y].end_time : INT_MAX;
//         int r_w_next_plus_p_w_next = (w_next_job != -1) ? time_info[w_next_job].end_time : INT_MAX;
//
//         int min_r_plus_p = std::min(r_y_plus_p_y, r_w_next_plus_p_w_next);
//         int r_u_prev_job = time_info[u_prev_job].forward_path_length;
//
//         if (min_r_plus_p <= r_u_prev_job) {
//             return false; // 违反条件 2，会产生环
//         }
//     }
//
//     // 2. 额外常识性校验：不能把工序插到自己的作业前驱或后继旁边 (同作业工序天然不能在同机器交叉)
//     if (v == u_next_job || w == u_prev_job || v == u_prev_job || w == u_next_job) {
//         return false;
//     }
//
//     return true; // 命题 4.2 验证通过，证明动作是安全的
// }

// bool Schedule::is_legal_move(const NeighborhoodMove& mv) const
// {
//     int u = mv.which;
//     int v = -1;
//     int w = -1;
//
//     if (mv.method == Method::BACK || mv.method == Method::CHANGE_MACHINE_BACK) {
//         v = mv.where;
//         w = (v != -1) ? graph.machine_successor[v] : -1;
//     } else if (mv.method == Method::FRONT || mv.method == Method::CHANGE_MACHINE_FRONT) {
//         w = mv.where;
//         v = (w != -1) ? graph.machine_predecessor[w] : -1;
//     }
//
//     int u_prev_job = graph.job_predecessor[u];
//     int u_next_job = graph.job_successor[u];
//     int n = graph.node_num; // 获取总节点数
//
//     // 【逻辑修复】：加上 > 0 和 < n-1 的限制，彻底排除两端的虚拟节点
//     if (v != -1 && u_next_job > 0 && u_next_job < n - 1) {
//         if (time_info[v].forward_path_length >= time_info[u_next_job].forward_path_length) {
//             return false;
//         }
//     }
//
//     if (w != -1 && u_prev_job > 0 && u_prev_job < n - 1) {
//         if (time_info[u_prev_job].forward_path_length >= time_info[w].forward_path_length) {
//             return false;
//         }
//     }
//
//     // 物理防呆检查
//     if (v == u_next_job || w == u_prev_job || v == u_prev_job || w == u_next_job) {
//         return false;
//     }
//
//     return true;
// }

bool Schedule::is_legal_move(const NeighborhoodMove& mv) const
{
    int u = mv.which;
    int v = -1;
    int w = -1;

    // 精确解析不同动作下，真正的物理前驱 v 和后继 w
    if (mv.method == Method::BACK) {
        v = mv.where;
        w = (v != -1) ? graph.machine_successor[v] : graph.first_machine_operation[graph.on_machine[u]];
    } else if (mv.method == Method::FRONT) {
        w = mv.where;
        v = (w != -1) ? graph.machine_predecessor[w] : graph.last_machine_operation[graph.on_machine[u]];
    } else if (mv.method == Method::CHANGE_MACHINE_BACK) {
        int new_m = mv.target_machine;
        v = mv.where;
        w = (v != -1) ? graph.machine_successor[v] : graph.first_machine_operation[new_m];
    } else if (mv.method == Method::CHANGE_MACHINE_FRONT) {
        int new_m = mv.target_machine;
        w = mv.where;
        v = (w != -1) ? graph.machine_predecessor[w] : graph.last_machine_operation[new_m];
    }

    int u_prev_job = graph.job_predecessor[u];
    int u_next_job = graph.job_successor[u];
    int n = graph.node_num;

    // --- 时间约束校验 ---
    if (v != -1 && u_next_job > 0 && u_next_job < n - 1) {
        if (time_info[v].forward_path_length >= time_info[u_next_job].forward_path_length) return false;
    }

    if (w != -1 && u_prev_job > 0 && u_prev_job < n - 1) {
        if (time_info[u_prev_job].forward_path_length >= time_info[w].forward_path_length) return false;
    }

    // --- 物理防呆校验 ---
    if (v == u_next_job || w == u_prev_job || v == u_prev_job || w == u_next_job) return false;

    // 【核心新增】：防止自我重叠，杜绝双向链表自己指向自己产生的死循环
    if (v == u || w == u) return false;

    return true;
}

void Schedule::make_move(const NeighborhoodMove& move)
{
    graph.make_move(move);
    update_time();
}

// 把这段实现加到 Schedule.cpp 的末尾
MoveCase Schedule::get_move_case(int u) const
{
    // 如果不是关键工序，直接返回
    if (!is_critical_operation(u)) {
        return MoveCase::NOT_CRITICAL;
    }

    // 获取工序的加工时间 (p_u)
    int p_u = (*operation_list)[u][graph.on_machine[u]];
    const auto& info = time_info[u];

    // 严格对照 Shen et al. (2018) 论文中的 4 种情况公式：
    if (makespan == info.r_job + p_u + info.q_machine) {
        return MoveCase::CASE_I;
    }
    else if (makespan == info.r_machine + p_u + info.q_job) {
        return MoveCase::CASE_II;
    }
    else if (makespan == info.r_machine + p_u + info.q_machine) {
        return MoveCase::CASE_III;
    }
    else if (makespan == info.r_job + p_u + info.q_job) {
        return MoveCase::CASE_IV;
    }

    return MoveCase::NOT_CRITICAL; // 防御性返回
}

void Schedule::output() const
{
    for (int i = 0; i < graph.machine_operation_count.size(); i++)
    {
        int machine_op_num = graph.machine_operation_count[i];
        std::cout << machine_op_num << "\t";
        for (auto op = graph.first_machine_operation[i]; op != -1; op = graph.machine_successor[op])
        {
            const auto& op_info = operation_list->operations[op];
            std::cout << op_info.job_id << " " << op_info.index << "\t";
        }
        std::cout << std::endl;
    }
}

std::vector<CriticalBlock> Schedule::get_critical_blocks() const
{
    std::vector<CriticalBlock> blocks;

    // 遍历每台机器，沿机器链找出连续的关键工序段
    for (int m = 0; m < static_cast<int>(graph.first_machine_operation.size()); ++m)
    {
        CriticalBlock current_block;
        current_block.machine_id = m;

        for (int op = graph.first_machine_operation[m]; op != -1; op = graph.machine_successor[op])
        {
            if (is_critical_operation(op))
            {
                current_block.operations.push_back(op);
            }
            else
            {
                // 非关键工序打断当前块
                if (!current_block.operations.empty())
                {
                    blocks.push_back(std::move(current_block));
                    current_block.operations.clear();
                    current_block.machine_id = m;
                }
            }
        }

        // 收尾：机器链末尾可能还有未提交的块
        if (!current_block.operations.empty())
        {
            blocks.push_back(std::move(current_block));
        }
    }

    return blocks;
}


