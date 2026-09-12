//
// Created by chenshiyu on 26-3-31.
//

#include "TabuSearch.h"

#include <algorithm>
#include <chrono>
#include <set>

NeighborhoodMove TabuSearch::find_move()
{
    std::vector<NeighborhoodMove> all_moves;
    std::vector<NeighborhoodMove> best_moves;
    int min_makespan = INT_MAX;

    // 1. 提取当前的关键块（保留随机单路径 —— 实测比"每轮看全部路径"探索性更好）
    update_critical_block();
    lb_values_.clear();

    // 2. 遍历每一个关键块，以及块内的每一个工序
    for (const auto& block : critical_blocks)
    {
        const int block_size = static_cast<int>(block.size());

        for (int idx = 0; idx < block_size; ++idx)
        {
            const int u = block[idx];

            // 【核心】判断该工序属于 Case I, II, III 还是 IV
            MoveCase op_case = current_schedule.get_move_case(u);

            // Case IV 不生成任何动作
            if (op_case == MoveCase::CASE_IV || op_case == MoveCase::NOT_CRITICAL) {
                continue;
            }

            int current_machine = current_schedule.graph.on_machine[u];
            int v = current_schedule.graph.machine_successor[u];   // 同机器后继
            int w = current_schedule.graph.machine_predecessor[u]; // 同机器前驱

            // ========================================================
            // 【P0-1】块端点跳跃：除了紧邻的 v / w，再允许一步跳到块的两端。
            //
            // 原先 BACK 的参照恒为 machine_successor[u]、FRONT 恒为 machine_predecessor[u]，
            // 于是块长 k 时只能做 k-1 个「相邻交换」，而标准 N5 允许把块内工序直接
            // 移到块尾之后 / 块首之前。块长 3 时前者只有 2 个动作，后者有 4 个。
            //
            // 方向约束（务必遵守）：BACK 的参照必须在 u 之后、FRONT 的参照必须在 u 之前，
            // 否则 Graph::make_move 内部的链表遍历会走出界（表现为卡死而非崩溃）。
            // block 是按机器链顺序排列的，故用下标判断方向是安全的。
            // ========================================================
            int v_far = -1; // 块尾（在 u 之后）
            int w_far = -1; // 块首（在 u 之前）
            if (block_size > 1) {
                if (idx + 1 < block_size) v_far = block.back();
                if (idx > 0)              w_far = block.front();
            }

            // ========================================================
// Case I 和 Case III：生成向后插入动作 (Type 1 和 Type 3)
// ========================================================
if (op_case == MoveCase::CASE_I || op_case == MoveCase::CASE_III)
{
    if (v != -1) {
        NeighborhoodMove move_back(Method::BACK, u, v, current_machine);
        if (current_schedule.is_legal_move(move_back)) {
            same_machine_evaluate_and_push(current_schedule, move_back, all_moves, best_moves, min_makespan);
        }
    }

    // 跳跃到块尾之后（与紧邻移动重复时跳过）
    if (v_far != -1 && v_far != v) {
        NeighborhoodMove move_back_far(Method::BACK, u, v_far, current_machine);
        if (current_schedule.is_legal_move(move_back_far)) {
            same_machine_evaluate_and_push(current_schedule, move_back_far, all_moves, best_moves, min_makespan);
        }
    }

    // 动作 Type 3：换机器向后移动 (CHANGE_MACHINE_BACK)
    for (int k_prime : (*current_schedule.operation_list)[u].candidates) {
        if (k_prime != current_machine) {
            // 尝试插入到新机器的最前面 (where = -1)
            NeighborhoodMove move_change_back_first(Method::CHANGE_MACHINE_BACK, u, -1, k_prime);
            if (current_schedule.is_legal_move(move_change_back_first)) {
                change_machine_evaluate_and_push(current_schedule, move_change_back_first, all_moves, best_moves, min_makespan);
            }
            // 尝试插入到新机器的每一个工序之后
            for (int op = current_schedule.graph.first_machine_operation[k_prime]; op != -1; op = current_schedule.graph.machine_successor[op]) {
                NeighborhoodMove move_change_back(Method::CHANGE_MACHINE_BACK, u, op, k_prime);
                if (current_schedule.is_legal_move(move_change_back)) {
                    change_machine_evaluate_and_push(current_schedule, move_change_back, all_moves, best_moves, min_makespan);
                }
            }
        }
    }
}

// ========================================================
// Case II 和 Case III：生成向前插入动作 (Type 2 和 Type 4)
// ========================================================
if (op_case == MoveCase::CASE_II || op_case == MoveCase::CASE_III)
{
    if (w != -1) {
        NeighborhoodMove move_front(Method::FRONT, u, w, current_machine);
        if (current_schedule.is_legal_move(move_front)) {
            same_machine_evaluate_and_push(current_schedule, move_front, all_moves, best_moves, min_makespan);
        }
    }

    // 跳跃到块首之前（与紧邻移动重复时跳过）
    if (w_far != -1 && w_far != w) {
        NeighborhoodMove move_front_far(Method::FRONT, u, w_far, current_machine);
        if (current_schedule.is_legal_move(move_front_far)) {
            same_machine_evaluate_and_push(current_schedule, move_front_far, all_moves, best_moves, min_makespan);
        }
    }

    // 动作 Type 4：换机器向前移动 (CHANGE_MACHINE_FRONT)
    for (int k_prime : (*current_schedule.operation_list)[u].candidates) {
        if (k_prime != current_machine) {
            // 尝试插入到新机器的最后面 (where = -1)
            NeighborhoodMove move_change_front_last(Method::CHANGE_MACHINE_FRONT, u, -1, k_prime);
            if (current_schedule.is_legal_move(move_change_front_last)) {
                change_machine_evaluate_and_push(current_schedule, move_change_front_last, all_moves, best_moves, min_makespan);
            }
            // 尝试插入到新机器的每一个工序之前
            for (int op = current_schedule.graph.last_machine_operation[k_prime]; op != -1; op = current_schedule.graph.machine_predecessor[op]) {
                NeighborhoodMove move_change_front(Method::CHANGE_MACHINE_FRONT, u, op, k_prime);
                if (current_schedule.is_legal_move(move_change_front)) {
                    change_machine_evaluate_and_push(current_schedule, move_change_front, all_moves, best_moves, min_makespan);
                }
            }
        }
    }
}
        }
    }

    // ========================================================
    // 动作选择逻辑 (与你原来的一致)
    // ========================================================
    if (all_moves.empty()) {
    std::cerr << "Warning: No moves found for " << current_schedule.get_makespan() << std::endl;
    return NeighborhoodMove(); // 返回一个默认构造的动作 (which 为 0)
}

    if (best_moves.empty()) {
        // 全是禁忌动作，随机选一个打破僵局
        int random_idx = get_random_int(0, static_cast<int>(all_moves.size()) - 1);
        return all_moves[random_idx];
    }

    // ========================================================
    // 【精确评估】LB top-K 完整解码
    //
    // LB1/LB2 只是下界估计，用它排序选出的「最优」移动未必真的最优。
    // 实测：LB 选中的移动只有 15–22% 恰好是完整解码后的真实最优，
    // 与真实最优的平均差距约 50（dpp01a 的 Cmax 约 2800，即 ~2%），
    // 但真实最优 100% 落在 LB 的前 3 名内。
    //
    // 因此这里对 LB 最小的 K 个候选做一次完整解码（update_time），
    // 按真实 makespan 择优。代价是每轮多 K 次解码，换取显著更好的选择。
    // ========================================================
    if (!lb_values_.empty() && lb_values_.size() == all_moves.size()) {
        constexpr int kPreciseTopK = 8;
        std::vector<int> order(lb_values_.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
        std::sort(order.begin(), order.end(),
                  [this](int a, int b) { return lb_values_[a] < lb_values_[b]; });

        int best_real = INT_MAX;
        int best_idx = -1;
        const int limit = std::min<int>(kPreciseTopK, static_cast<int>(order.size()));
        for (int i = 0; i < limit; ++i) {
            if (lb_values_[order[i]] == INT_MAX) break; // 后面的都是禁忌动作
            Schedule probe = current_schedule;
            try {
                probe.make_move(all_moves[order[i]]);
                probe.update_time();
            } catch (...) {
                continue; // 产生环等异常，跳过
            }
            const int real = probe.get_makespan();
            if (real < best_real) {
                best_real = real;
                best_idx = order[i];
            }
        }
        if (best_idx >= 0) return all_moves[best_idx];
    }

    if (min_makespan > current_schedule.get_makespan()) {
        // 小概率 (3%) 接受次优解以增加扰动
        if (get_random_int(0, 99) < 3) {
            int random_idx = get_random_int(0, static_cast<int>(all_moves.size()) - 1);
            return all_moves[random_idx];
        }
    }

    // 从最优邻域动作中随机返回一个动作
    int best_idx = get_random_int(0, static_cast<int>(best_moves.size()) - 1);
    return best_moves[best_idx];
}


void TabuSearch::update_critical_block()
{
    const auto& graph = current_schedule.graph;
    const auto& time_info = current_schedule.time_info;
    const auto& operation_list = *(current_schedule.operation_list);
    const auto* instance = current_schedule.instance; // 获取 instance 以读取 SDST

    std::vector<int> start_candidates;
    // 1. 从每个机器的第一个工序开始寻找，找到关键路径的开头
    for (auto start : graph.first_machine_operation)
    {
        if (start != -1 && current_schedule.is_critical_operation(start) && time_info[start].forward_path_length == 0)
        {
            start_candidates.push_back(start);
        }
    }

    do
    {
        critical_path.clear();
        if (start_candidates.empty()) break; // 防御性检查

        // 从随机一个start开始寻找 (替换了原来的 RAND_INT 宏)
        int start_index = get_random_int(0, static_cast<int>(start_candidates.size()) - 1);
        critical_path.push_back(start_candidates[start_index]);
        start_candidates[start_index] = start_candidates.back();
        start_candidates.pop_back();

        // 2. 顺藤摸瓜寻找完整关键路径
        while (time_info[critical_path.back()].end_time != current_schedule.get_makespan())
        {
            const auto op = critical_path.back();
            const auto ms_op = graph.machine_successor[op];

            // === [核心修改 1] 获取机器上的 Setup Time ===
            int setup_time = 0;
            if (ms_op != -1 && instance != nullptr) {
                int m = graph.on_machine[op];
                int prev_job = operation_list[op].job_id;
                int next_job = operation_list[ms_op].job_id;
                setup_time = instance->sdst_matrix[m][prev_job][next_job];
            }
            // ============================================

            // 贪心在同机器上寻找 (必须加上 setup_time 判断)
            if (ms_op != -1 && current_schedule.is_critical_operation(ms_op) &&
                time_info[ms_op].forward_path_length == time_info[op].end_time + setup_time)
            {
                critical_path.push_back(ms_op);
                continue;
            }
            const auto js_op = graph.job_successor[op];
            if (js_op != -1) {
                critical_path.push_back(js_op);
            } else {
                break; // 防御性跳出
            }
        }

        // 3. 开始将关键路径切分为关键块
        //
        // 【P0-2】原先只保留 size > 1 的块，关键路径上「独占一台机器」的工序会被整个丢弃。
        // 实测关键路径长 11–22，而块长 ≥2 的块只有 1–3 个 —— 也就是说绝大多数关键工序
        // 根本不参与移动，邻域只有个位数。这里改为保留长度为 1 的单例块，
        // 由 get_move_case 的 Case I/II/III 剪枝决定它究竟能不能移动。
        critical_blocks.clear();
        auto prev_machine = -1;
        std::vector<int> critical_block;
        for (auto op : critical_path)
        {
            if (graph.on_machine[op] != prev_machine)
            {
                if (!critical_block.empty())
                {
                    critical_blocks.push_back(critical_block);
                    critical_block.clear();
                }
                critical_block.push_back(op);
                prev_machine = graph.on_machine[op];
            }
            else
            {
                critical_block.push_back(op);
            }
        }
        if (!critical_block.empty())
        {
            critical_blocks.push_back(critical_block);
        }
    }
    while (critical_blocks.empty() && !start_candidates.empty());

    // 4. 兜底逻辑：如果前面的贪心追踪失效，遍历机器寻找
    if (critical_blocks.empty())
    {
        for (const auto start_machine_op : graph.first_machine_operation)
        {
            int curr_machine_op = start_machine_op;
            std::vector<int> critical_block;
            while (curr_machine_op != -1)
            {
                if (current_schedule.is_critical_operation(curr_machine_op))
                {
                    if (critical_block.empty())
                    {
                        critical_block.push_back(curr_machine_op);
                    }
                    else
                    {
                        int prev_critical_op = critical_block.back();

                        // === [核心修改 2] 获取兜底逻辑的 Setup Time ===
                        int setup_time = 0;
                        if (instance != nullptr) {
                            int m = graph.on_machine[prev_critical_op];
                            int prev_job = operation_list[prev_critical_op].job_id;
                            int next_job = operation_list[curr_machine_op].job_id;
                            setup_time = instance->sdst_matrix[m][prev_job][next_job];
                        }
                        // ============================================

                        // 判断块内连续性 (加上 setup_time)
                        if (time_info[prev_critical_op].end_time + setup_time ==
                            time_info[curr_machine_op].forward_path_length)
                        {
                            critical_block.push_back(curr_machine_op);
                        }
                        else
                        {
                            if (critical_block.size() > 1)
                            {
                                critical_blocks.push_back(critical_block);
                            }
                            critical_block.clear();
                            critical_block.push_back(curr_machine_op);
                        }
                    }
                }
                else
                {
                    if (critical_block.size() > 1)
                    {
                        critical_blocks.push_back(critical_block);
                        critical_block.clear();
                    }
                }
                curr_machine_op = graph.machine_successor[curr_machine_op];
            }
            if (critical_block.size() > 1)
            {
                critical_blocks.push_back(critical_block);
            }
        }
    }
}

void TabuSearch::make_move(const NeighborhoodMove& move)
{
    // 1. 先让底层图真正执行拓扑指针的修改
    current_schedule.make_move(move);

    // 2. 提取移动后的机器前驱关系
    int u = move.which;
    // 因为图已经更新了，我们直接查它现在的机器前驱是谁
    int v = current_schedule.graph.machine_predecessor[u];

    // 如果 u 被插到了某台机器的最前面，它的前驱就是 -1。
    // 为了能在矩阵中存储，我们用总节点数 (node_num) 来代表“虚拟起点”
    if (v == -1) {
        v = current_schedule.graph.node_num;
    }

    // 3. 计算动态禁忌长度 T
    const unsigned long long tabu_time = iteration + get_random_int(L, L_max);

    // 4. 将有向边 (v, u) 关进禁忌表
    tabu_list.add_pair(v, u, tabu_time);
}
bool TabuSearch::is_tabu(const NeighborhoodMove& mv, const int makespan) const
{
    // 特赦准则
    if (makespan < best_schedule.get_makespan()) {
        return false;
    }

    int u = mv.which;
    int v = -1;

    if (mv.method == Method::BACK || mv.method == Method::CHANGE_MACHINE_BACK) {
        v = mv.where;
    } else if (mv.method == Method::FRONT || mv.method == Method::CHANGE_MACHINE_FRONT) {
        // 【防越界修复】：严格拦截 where 为 -1 的情况
        v = (mv.where != -1) ? current_schedule.graph.machine_predecessor[mv.where] : -1;
    }

    if (v == -1) {
        v = current_schedule.graph.node_num; // 映射为虚拟节点
    }

    return tabu_list.is_tabu(v, u, iteration);
}
void TabuSearch::same_machine_evaluate_and_push(const Schedule& schedule, const NeighborhoodMove& move,
                                                std::vector<NeighborhoodMove>& all_moves,
                                                std::vector<NeighborhoodMove>& best_moves, int& min_makespan) const
{
    if (!schedule.is_legal_move(move)) return;
    all_moves.push_back(move);

    int u = move.which;
    int m = schedule.graph.on_machine[u];
    int p_u = (*schedule.operation_list)[u][m];

    const auto& info_u = schedule.time_info[u];
    const auto* instance = schedule.instance;
    int job_u = (*schedule.operation_list)[u].job_id;

    // 1. 论文推导：精确定位新位置的机器前驱 (PM) 和机器后继 (SM)
    int PM = -1;
    int SM = -1;

    if (move.method == Method::BACK) {
        PM = move.where;
        SM = (PM != -1) ? schedule.graph.machine_successor[PM] : schedule.graph.first_machine_operation[m];
        if (SM == u) SM = schedule.graph.machine_successor[u]; // 跳过自身
    } else { // FRONT
        SM = move.where;
        PM = (SM != -1) ? schedule.graph.machine_predecessor[SM] : schedule.graph.last_machine_operation[m];
        if (PM == u) PM = schedule.graph.machine_predecessor[u]; // 跳过自身
    }

    // 2. 论文公式：计算包含 SDST 的新机器头部 r'_u
    int r_prime_machine = 0;
    if (PM != -1) {
        r_prime_machine = schedule.time_info[PM].end_time;
        if (instance != nullptr) {
            int job_pm = (*schedule.operation_list)[PM].job_id;
            r_prime_machine += instance->sdst_matrix[m][job_pm][job_u]; // 加上新产生的 Setup Time
        }
    }

    // 3. 论文公式：计算包含 SDST 的新机器尾部 q'_u
    int q_prime_machine = 0;
    if (SM != -1) {
        int p_sm = (*schedule.operation_list)[SM][m];
        q_prime_machine = p_sm + schedule.time_info[SM].q_machine;
        if (instance != nullptr) {
            int job_sm = (*schedule.operation_list)[SM].job_id;
            q_prime_machine += instance->sdst_matrix[m][job_u][job_sm]; // 加上新产生的 Setup Time
        }
    }

    // 4. 命题 4.3 最终公式: LB1 = max(r_u^J + p_u + q_u^J, r'_u + p_u + q'_u)
    int lb1 = std::max(info_u.r_job + p_u + info_u.q_job,
                       r_prime_machine + p_u + q_prime_machine);

    if (!is_tabu(move, lb1)) {
        lb_values_.push_back(lb1);
        if (lb1 < min_makespan) {
            min_makespan = lb1;
            best_moves.clear();
            best_moves.push_back(move);
        } else if (lb1 == min_makespan) {
            best_moves.push_back(move);
        }
    } else {
        lb_values_.push_back(INT_MAX); // 被禁忌，不参与 top-K 精确评估
    }
}
void TabuSearch::change_machine_evaluate_and_push(const Schedule& schedule, const NeighborhoodMove& move,
                                                  std::vector<NeighborhoodMove>& all_moves,
                                                  std::vector<NeighborhoodMove>& best_moves, int& min_makespan) const
{
    if (!schedule.is_legal_move(move)) return;
    all_moves.push_back(move);

    int u = move.which;
    int target_m = move.target_machine;
    int p_u_new = (*schedule.operation_list)[u][target_m];

    const auto& info_u = schedule.time_info[u];
    const auto* instance = schedule.instance;
    int job_u = (*schedule.operation_list)[u].job_id;

    // 1. 跨机器定位新的 PM 和 SM
    int PM = -1;
    int SM = -1;

    if (move.method == Method::CHANGE_MACHINE_BACK) {
        PM = move.where;
        SM = (PM != -1) ? schedule.graph.machine_successor[PM] : schedule.graph.first_machine_operation[target_m];
    } else { // CHANGE_MACHINE_FRONT
        SM = move.where;
        PM = (SM != -1) ? schedule.graph.machine_predecessor[SM] : schedule.graph.last_machine_operation[target_m];
    }

    // 2. 计算目标机器上的新头部 r'_u
    int r_prime_machine = 0;
    if (PM != -1) {
        r_prime_machine = schedule.time_info[PM].end_time;
        if (instance != nullptr) {
            int job_pm = (*schedule.operation_list)[PM].job_id;
            r_prime_machine += instance->sdst_matrix[target_m][job_pm][job_u];
        }
    }

    // 3. 计算目标机器上的新尾部 q'_u
    int q_prime_machine = 0;
    if (SM != -1) {
        int p_sm = (*schedule.operation_list)[SM][target_m];
        q_prime_machine = p_sm + schedule.time_info[SM].q_machine;
        if (instance != nullptr) {
            int job_sm = (*schedule.operation_list)[SM].job_id;
            q_prime_machine += instance->sdst_matrix[target_m][job_u][job_sm];
        }
    }

    // 4. 命题 4.4 最终公式: LB2 = max(r_u^J + p_u_new + q_u^J, r'_u + p_u_new + q'_u)
    int lb2 = std::max(info_u.r_job + p_u_new + info_u.q_job,
                       r_prime_machine + p_u_new + q_prime_machine);

    if (!is_tabu(move, lb2)) {
        lb_values_.push_back(lb2);
        if (lb2 < min_makespan) {
            min_makespan = lb2;
            best_moves.clear();
            best_moves.push_back(move);
        } else if (lb2 == min_makespan) {
            best_moves.push_back(move);
        }
    } else {
        lb_values_.push_back(INT_MAX); // 被禁忌，不参与 top-K 精确评估
    }
}
void TabuSearch::update_all_critical_block()
{
    const auto& graph = current_schedule.graph;
    const auto& time_info = current_schedule.time_info;
    const auto& operation_list = *(current_schedule.operation_list);
    const auto* instance = current_schedule.instance;

    critical_blocks.clear();
    std::set<std::vector<int>> seen; // 跨多条关键路径去重（相同块只生成一次）

    // 收集所有关键路径起点（首机器上的关键工序，且向前路径长度为 0）
    std::vector<int> starts;
    for (int s : graph.first_machine_operation) {
        if (s != -1 && current_schedule.is_critical_operation(s) && time_info[s].forward_path_length == 0)
            starts.push_back(s);
    }

    // 把一条完整关键路径切成“同机器连续块”。
    // 【P0-2】保留 size>=1 的单例块：由 find_move 里 get_move_case 的剪枝决定它能否移动，
    // 否则关键路径上“独占一台机器”的工序会被整个丢弃，邻域只剩个位数。
    auto extract_blocks = [&](const std::vector<int>& path) {
        std::vector<int> block;
        int prev_m = -1;
        for (int op : path) {
            int m = graph.on_machine[op];
            if (m != prev_m) {
                if (!block.empty()) seen.insert(block);
                block = {op};
                prev_m = m;
            } else {
                block.push_back(op);
            }
        }
        if (!block.empty()) seen.insert(block);
    };

    // 对每一个起点，用与 update_critical_block 一致、已验证不会卡死的追踪逻辑走完整条关键路径。
    // 原 BFS 写法在“机器后继”分支里原地延长当前路径却不再展开其后续节点，
    // 会导致关键路径只追踪到第一跳就被丢弃；这里改为：每个起点独立追踪到终点。
    for (int start : starts) {
        std::vector<int> path;
        path.push_back(start);
        while (time_info[path.back()].end_time != current_schedule.get_makespan()) {
            const int op = path.back();
            const int ms_op = graph.machine_successor[op];

            int setup = 0;
            if (ms_op != -1 && instance != nullptr) {
                int m = graph.on_machine[op];
                setup = instance->sdst_matrix[m][operation_list[op].job_id][operation_list[ms_op].job_id];
            }

            // 贪心在同机器上继续关键路径（必须加上 setup_time 判断）
            if (ms_op != -1 && current_schedule.is_critical_operation(ms_op) &&
                time_info[ms_op].forward_path_length == time_info[op].end_time + setup) {
                path.push_back(ms_op);
                continue;
            }
            const int js_op = graph.job_successor[op];
            if (js_op != -1) {
                path.push_back(js_op);
            } else {
                break; // 防御性跳出
            }
        }
        extract_blocks(path);
    }

    for (const auto& b : seen) critical_blocks.push_back(b);

    // 兜底：若上面的追踪一条有效块都没产出，退化为逐机器扫描（逻辑同原 update_critical_block 兜底）
    if (critical_blocks.empty()) {
        for (int start_machine_op : graph.first_machine_operation) {
            int curr = start_machine_op;
            std::vector<int> block;
            while (curr != -1) {
                if (current_schedule.is_critical_operation(curr)) {
                    if (block.empty()) {
                        block.push_back(curr);
                    } else {
                        int prev_op = block.back();
                        int setup = 0;
                        if (instance != nullptr) {
                            int m = graph.on_machine[prev_op];
                            setup = instance->sdst_matrix[m][operation_list[prev_op].job_id][operation_list[curr].job_id];
                        }
                        if (time_info[prev_op].end_time + setup == time_info[curr].forward_path_length) {
                            block.push_back(curr);
                        } else {
                            if (block.size() > 1) seen.insert(block);
                            block.clear();
                            block.push_back(curr);
                        }
                    }
                }
                curr = graph.machine_successor[curr];
            }
            if (block.size() > 1) seen.insert(block);
        }
        for (const auto& b : seen) critical_blocks.push_back(b);
    }
}

// void TabuSearch::apply_perturbation() {
//     std::vector<int> candidates;
//     for (int u = 1; u < current_schedule.graph.node_num - 1; ++u) {
//         if (current_schedule.is_critical_operation(u)) {
//             candidates.push_back(u);
//         }
//     }
//
//     if (candidates.empty()) return;
//
//     std::clog << "[扰动] 邻域枯竭，强制多样化解结构..." << std::endl;
//
//     int success_count = 0;
//     for (int i = 0; i < 20 && success_count < 5; ++i) {
//         int u = candidates[rand() % candidates.size()];
//         const auto& m_list = (*current_schedule.operation_list)[u].candidates;
//
//         if (m_list.size() > 1) {
//             int old_m = current_schedule.graph.on_machine[u];
//             int new_m = m_list[rand() % m_list.size()];
//
//             if (new_m == old_m) continue;
//
//             int v = current_schedule.graph.first_machine_operation[new_m];
//             NeighborhoodMove p_move(Method::CHANGE_MACHINE_FRONT, u, v, new_m);
//
//             if (current_schedule.is_legal_move(p_move)) {
//                 current_schedule.make_move(p_move);
//                 current_schedule.update_time(); // 依然保留，刷新时间
//                 success_count++;
//             } else {
//                 int last_v = current_schedule.graph.last_machine_operation[new_m];
//                 NeighborhoodMove p_move_back(Method::CHANGE_MACHINE_BACK, u, last_v, new_m);
//
//                 if (current_schedule.is_legal_move(p_move_back)) {
//                     current_schedule.make_move(p_move_back);
//                     current_schedule.update_time(); // 依然保留，刷新时间
//                     success_count++;
//                 }
//             }
//         }
//     }
// }

namespace
{
    /// 沿机器链从头到尾收集某台机器上的工序序列
    /// （步数上限是防御性的：链表一旦被破坏，无限遍历会让程序彻底卡死而不是报错）
    std::vector<int> collect_machine_sequence(const Graph& g, int machine)
    {
        std::vector<int> seq;
        int op = g.first_machine_operation[machine];
        for (int step = 0; op != -1 && step <= g.node_num; ++step) {
            seq.push_back(op);
            op = g.machine_successor[op];
        }
        return seq;
    }
}

bool TabuSearch::try_apply_move(const NeighborhoodMove& move)
{
    if (move.which <= 0 || move.where == move.which) return false;
    if (!current_schedule.is_legal_move(move)) return false;

    // 只备份图结构：时间信息可由 update_time() 重新算出，无需整体复制
    const Graph backup = current_schedule.graph;
    try {
        current_schedule.make_move(move); // 内部执行 graph.make_move + update_time（含成环检测）
    }
    catch (const std::runtime_error&) {
        current_schedule.graph = backup;
        current_schedule.update_time();
        return false;
    }
    return true;
}

bool TabuSearch::relocate_to_position(int u, int machine, int pos)
{
    const Graph& g = current_schedule.graph;
    const bool same_machine = (g.on_machine[u] == machine);

    std::vector<int> seq = collect_machine_sequence(g, machine);

    int self_index = -1;
    if (same_machine) {
        const auto it = std::find(seq.begin(), seq.end(), u);
        if (it == seq.end()) return false;
        self_index = static_cast<int>(it - seq.begin());
    }

    seq.erase(std::remove(seq.begin(), seq.end(), u), seq.end());
    if (seq.empty()) return false;
    pos = std::clamp(pos, 0, static_cast<int>(seq.size()));

    // 换机器必须用 CHANGE_MACHINE_*：Graph::make_move 对 FRONT/BACK 走的是同机器分支，
    // 会按 on_machine[u] 定位链表；参照工序在别的机器上时会直接破坏链表结构。
    if (!same_machine) {
        const bool at_tail = (pos >= static_cast<int>(seq.size()));
        const int ref = at_tail ? seq.back() : seq[pos];
        const Method method = at_tail ? Method::CHANGE_MACHINE_BACK : Method::CHANGE_MACHINE_FRONT;
        return try_apply_move(NeighborhoodMove(method, u, ref, machine));
    }

    // 同机器时方向有硬约束：Graph::make_move 里 FRONT 会从 where 沿后继正向遍历到 u，
    // BACK 会从 where 沿前驱反向遍历到 u。方向给反了会一路走到链表外（越界/死循环），
    // 因此必须先判断目标位置在 u 的前面还是后面，再选对应的动作与参照工序。
    if (pos == self_index) return false;                 // 原地不动，跳过

    if (pos < self_index) {
        // 目标在 u 之前：插到 seq[pos] 前面（该工序原本就在 u 之前）
        return try_apply_move(NeighborhoodMove(Method::FRONT, u, seq[pos], machine));
    }
    // 目标在 u 之后：插到 seq[pos-1] 后面（该工序原本就在 u 之后）
    return try_apply_move(NeighborhoodMove(Method::BACK, u, seq[pos - 1], machine));
}

bool TabuSearch::perturb_change_machine(int u)
{
    const auto& m_list = (*current_schedule.operation_list)[u].candidates;
    if (m_list.size() <= 1) return false;

    const int old_m = current_schedule.graph.on_machine[u];
    const int new_m = m_list[get_random_int(0, static_cast<int>(m_list.size()) - 1)];
    if (new_m == old_m) return false;

    // 不再局限于头/尾两个位置，随机挑一个插入点
    const std::vector<int> seq = collect_machine_sequence(current_schedule.graph, new_m);
    return relocate_to_position(u, new_m, get_random_int(0, static_cast<int>(seq.size())));
}

bool TabuSearch::perturb_reinsert(int u)
{
    const int m = current_schedule.graph.on_machine[u];
    const std::vector<int> seq = collect_machine_sequence(current_schedule.graph, m);
    if (seq.size() <= 1) return false;

    return relocate_to_position(u, m, get_random_int(0, static_cast<int>(seq.size()) - 1));
}

bool TabuSearch::perturb_swap(int u)
{
    const int m = current_schedule.graph.on_machine[u];
    const std::vector<int> seq = collect_machine_sequence(current_schedule.graph, m);
    if (seq.size() <= 1) return false;

    const auto it = std::find(seq.begin(), seq.end(), u);
    if (it == seq.end()) return false;
    const int i = static_cast<int>(it - seq.begin());

    int j = get_random_int(0, static_cast<int>(seq.size()) - 1);
    if (j == i) return false;
    const int w = seq[j];

    // 第一步：把 u 插到（去掉 u 后）序列的第 j 个位置 —— 恰好落在 w 的前面或后面
    if (!relocate_to_position(u, m, j)) return false;
    // 第二步：把 w 插到第 i 个位置，即 u 原来的位置 —— 完成一次交换
    static_cast<void>(relocate_to_position(w, m, i));
    return true;
}

void TabuSearch::apply_perturbation(const double intensity) {
    std::vector<int> candidates;
    // 收集所有关键工序作为扰动的候选池
    for (int u = 1; u < current_schedule.graph.node_num - 1; ++u) {
        if (current_schedule.is_critical_operation(u)) {
            candidates.push_back(u);
        }
    }

    if (candidates.empty()) return;

    const int node_num = std::max(1, current_schedule.graph.node_num);

    // 目标破坏数量：总节点数的 15%，可按 intensity（连续无效扰动次数）放大
    const int base_target = std::max(5, static_cast<int>(node_num * 0.15));
    const int disrupt_target = std::min(node_num - 2,
                                        std::max(5, static_cast<int>(base_target * intensity)));
    const int max_attempts = disrupt_target * 8; // 设置最大尝试次数防止死循环
    int success_count = 0;

    std::clog << "[扰动] 混合扰动(换机器/重插/交换)，目标 " << disrupt_target << " 个工序..." << std::endl;

    for (int i = 0; i < max_attempts && success_count < disrupt_target; ++i) {
        // 候选池：80% 取关键工序，20% 放宽到全体工序，避免只在关键路径上打转
        const int u = (get_random_int(0, 99) < 20)
                          ? get_random_int(1, node_num - 2)
                          : candidates[get_random_int(0, static_cast<int>(candidates.size()) - 1)];

        // 算子配比：35% 换机器 / 45% 同机器重插 / 20% 同机器交换
        const int roll = get_random_int(0, 99);
        bool ok = false;
        if (roll < 35) {
            ok = perturb_change_machine(u);
        } else if (roll < 80) {
            ok = perturb_reinsert(u);
        } else {
            ok = perturb_swap(u);
        }

        // 换机器对只有单台候选的工序无效，退化为重插，保证柔性小的算例也能被扰动到
        if (!ok && roll < 35) ok = perturb_reinsert(u);

        if (ok) success_count++;
    }
}

void TabuSearch::search(const Schedule& schedule, const std::atomic<bool>& stop_flag) {
    current_schedule = schedule;
    best_schedule = schedule;
    iteration = 0;
    int non_improve_iter = 0;
    int continuous_no_move = 0;
    int perturb_since_improve = 0;   // 连续多少次扰动仍未刷新历史最优
    static constexpr int kMaxPerturbNoImprove = 12; // 超过则拉回历史最优解重新出发

    // 非改进迭代上限：达到后触发一次扰动。按算例规模缩放，小算例保留下限，
    // 避免扰动过于频繁反而破坏搜索。
    const int node_num = std::max(1, current_schedule.graph.node_num);
    const int max_non_improve = std::max(5000, node_num * 30);

    tabu_list.clear();
    timed_out_ = false;
    elapsed_seconds_ = 0.0;

    const auto start_time = std::chrono::steady_clock::now();
    unsigned long long check_counter = 0;

    while (iteration < max_iterations_) {
        // 每 64 轮检查一次终止条件，避免高频读取系统时钟拖慢搜索
        if ((++check_counter & 63ULL) == 0)
        {
            if (stop_flag.load(std::memory_order_relaxed)) break;

            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed >= time_limit_seconds_)
            {
                timed_out_ = true;
                break;
            }
        }

        auto move = find_move();

        if (move.which <= 0) {
            continuous_no_move++;
            apply_perturbation(1.0 + 0.15 * perturb_since_improve);
            tabu_list.clear();
            perturb_since_improve++;
            if (perturb_since_improve >= kMaxPerturbNoImprove) {
                current_schedule = best_schedule; // 多次扰动无果，回到历史最优再出发
                perturb_since_improve = 0;
            }

            if (continuous_no_move > 30) {
                std::cerr << "Warning: 邻域已彻底锁死，提前跳过该算例测试。" << std::endl;
                break;
            }
            continue;
        }

        continuous_no_move = 0;

        // ==========================================
        // 【核心修改 1】：保存当前绝对安全的无环状态
        // ==========================================
        Schedule backup_schedule = current_schedule;

        try {
            // 尝试执行物理移动并更新时间 (内部会触发拓扑排序检测环)
            make_move(move);
        }
        catch (const std::runtime_error& e) {
            // ==========================================
            // 【核心修改 2】：检测到环，触发安全回退 (Rollback)
            // ==========================================
            // std::clog << "[拦截] 动作导致拓扑环，执行回退..." << std::endl;

            current_schedule = backup_schedule; // 瞬间恢复所有图结构和时间指针

            // 严厉惩罚这个导致死锁的动作，将其关入“永久禁忌黑名单”
            int v = -1;
            if (move.method == Method::BACK || move.method == Method::CHANGE_MACHINE_BACK) {
                v = move.where;
            } else {
                v = (move.where != -1) ? current_schedule.graph.machine_predecessor[move.where] : -1;
            }
            if (v == -1) v = current_schedule.graph.node_num;

            // 禁忌长度原为 999999（近乎永久），会把大量“此刻成环、换个上下文就合法”的
            // 动作永久封死，导致邻域越搜越小。改为一个普通禁忌周期即可。
            tabu_list.add_pair(v, move.which, iteration + static_cast<unsigned long long>(std::max(1, L)));

            continue; // 跳过后续的 best_schedule 更新，重新寻找合法动作
        }

        iteration++;
        non_improve_iter++;

        if (current_schedule.get_makespan() < best_schedule.get_makespan()) {
            best_schedule = current_schedule;
            non_improve_iter = 0;
            perturb_since_improve = 0;
        }

        if (non_improve_iter >= max_non_improve) {
            // 扰动强度随连续无效扰动次数放大，帮助跳出深吸引域
            apply_perturbation(1.0 + 0.15 * perturb_since_improve);
            tabu_list.clear();
            non_improve_iter = 0;
            perturb_since_improve++;

            if (perturb_since_improve >= kMaxPerturbNoImprove) {
                current_schedule = best_schedule; // 拉回历史最优解，避免越扰动越差
                perturb_since_improve = 0;
            }
        }
    }

    elapsed_seconds_ = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start_time).count();
}