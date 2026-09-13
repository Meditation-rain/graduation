//
// Created by chenshiyu on 26-3-31.
//

#ifndef TABUSEARCH_H
#define TABUSEARCH_H
#include <atomic>
#include <chrono>
#include <random>

#include "Instance.h"
#include "Random.h"
#include "Schedule.h"
#include "TabuList.h"


class TabuSearch
{
public:
    [[nodiscard]] int get_makespan() const { return best_schedule.makespan; }
    [[maybe_unused]] [[nodiscard]] unsigned long long get_iteration() const { return iteration; }

    /// 上一轮 search() 是否因为到达时间上限而终止
    [[nodiscard]] bool timed_out() const { return timed_out_; }
    /// 上一轮 search() 实际耗时（秒）
    [[nodiscard]] double elapsed_seconds() const { return elapsed_seconds_; }


    /**
     * @param instance            算例（用于推导禁忌长度 L）
     * @param max_iterations      迭代次数上限，是主要的终止条件。
     *                            以迭代次数终止才能保证同一算例多次运行结果完全一致。
     * @param time_limit_seconds  时间上限（秒），仅作为兜底保护，防止超大算例耗时失控。
     *                            一旦触发，本次搜索将不可复现（CSV 中 TimedOut 会记为 1）。
     */
    explicit TabuSearch(const Instance& instance,
                        unsigned long long max_iterations = 300000ULL,
                        double time_limit_seconds = 60.0) :
        // 【核心修改】：把 machine_num 换成图中所有的节点总数 + 1
        // (这里的 +1 是为了给原本前驱为 -1 的头部工序留一个特殊索引位置)
        // 【P7】按算例实际节点数定容。原先硬编码 2000 → 2000×2000×8B ≈ 32MB，
        // 而最大算例的 node_num 仅约 300，浪费 44 倍；且每次扰动的 clear()
        // 都要 memset 整整 32MB。TabuList 的读写都有边界检查，容量缩小后
        // 对合法节点 ID 的行为完全一致，因此不影响搜索轨迹。
        // 【P7】按算例实际规模定容。原先硬编码 2000 → 2000×2000×8B ≈ 32MB，
        // 而最大算例的 node_num 仅约 300，浪费 44 倍；且每次扰动的 clear()
        // 都要 memset 整整 32MB。内存降到约 0.7MB，对缓存也更友好。
        //
        // ⚠️ 容量必须是 node_num + 1，不能只用 node_num：
        // make_move / is_tabu 用「id == node_num」表示「虚拟起点」这一路
        // （见 TabuSearch.cpp 中 v == -1 时的映射），所以最大合法下标是 node_num。
        // 若只按 node_num 定容，这类「移到机器最前面」的禁忌会被边界检查
        // 静默丢弃，搜索轨迹随之改变——这一点在实测中已确认。
        tabu_list(instance.op_num + 3), // node_num + 1 == (op_num + 2) + 1
        L(10 + instance.job_num / instance.machine_num),
        L_max(instance.job_num <= 2 * instance.machine_num ? static_cast<int>(L * 1.4) : static_cast<int>(L * 1.5)),
        time_limit_seconds_(time_limit_seconds),
        max_iterations_(max_iterations)
    {
    }
    // 注意：如果以时间上限作为主终止条件，墙钟抖动会导致迭代次数不一致、结果无法复现，
    // 因此这里用迭代次数作为主终止条件，时间上限只做兜底。

    void search(const Schedule& schedule, const std::atomic<bool>& stop_flag);

    NeighborhoodMove find_move();
    void make_move(const NeighborhoodMove& move);

    Schedule best_schedule{};


private:
    unsigned long long iteration{};
    Schedule current_schedule{};
    TabuList tabu_list;
    int L;
    int L_max;

    /// 与 find_move 中 all_moves 同下标的 LB 估计值；被禁忌者记为 INT_MAX，
    /// 供「LB top-K 完整解码」阶段排序使用。
    mutable std::vector<int> lb_values_;

    /// 【P6】top-K 排序用的下标缓冲，复用以避免每代一次堆分配。
    /// 注意：这里仍用 std::sort 全排序而非 partial_sort —— partial_sort 对
    /// LB 值相同的候选项会给出不同的排列顺序，会改变搜索轨迹。
    /// 只在确认「改成 partial_sort 后质量不下降」时才可替换（届时需另做 A/B）。
    std::vector<int> topk_order_;

    // ==================================================================
    // 【性能】可复用缓冲区：消除每代上百次堆分配
    //
    // 原实现每一代都要构造 1 个 Schedule 回滚备份 + 最多 K(8) 个 Schedule 探针，
    // 每个 Schedule 含 Graph 的 11 个 vector 与 time_info，合计约 100 次 new/delete。
    // 30 万代就是约 3000 万次堆分配 —— 这正是堆碎片化、单代耗时随运行时间
    // 暴涨（实测最高 76 倍）的根因，也让「时间上限在哪一代截断」变得不可复现。
    //
    // 改为复用成员后，vector 的拷贝赋值会直接复用已有容量，稳态下零分配。
    // ==================================================================

    /// 回滚缓冲：只快照图结构即可。
    /// 理由：成环异常由 update_time() 中的 topological_sort 抛出，而它在写入
    /// 任何时间信息之前执行，因此 time_info 与 makespan 在异常时仍保持移动前
    /// 的原值，无需备份。
    Graph backup_graph_;

    /// 扰动算子 try_apply_move 的回滚缓冲（与 backup_graph_ 分开，避免嵌套调用互相覆盖）
    Graph undo_graph_;

    /// top-K 精确评估的探针调度。只覆盖 graph，time_info 由 update_time() 全量重算。
    Schedule probe_schedule_;

    /// 【C1】LAHC 历史 cost 窗口：长度为 cfg::lahc_H，循环覆盖。
    /// 每代把"本代开始时的解 cost"写入 history[iteration % H]，
    /// 接受准则参考该槽位在 H 代之前记录的历史解。仅在 cfg::lahc 开启时使用。
    std::vector<int> lahc_history_;

    double time_limit_seconds_;
    unsigned long long max_iterations_;
    bool timed_out_{false};
    double elapsed_seconds_{0.0};

    void change_machine_evaluate_and_push(const Schedule& schedule, const NeighborhoodMove& move,
                                          std::vector<NeighborhoodMove>& all_moves,
                                          std::vector<NeighborhoodMove>& best_moves, int& min_makespan) const;

    std::vector<int> critical_path;
    std::vector<std::vector<int>> critical_blocks;

    [[nodiscard]] bool is_tabu(const NeighborhoodMove& move, int makespan) const;
    void update_critical_block();
    void update_all_critical_block();

    // ==================================================================
    // 【P8】find_move 内部的期限（deadline）检查
    //
    // 原先只在 search() 每轮循环顶部查一次时钟，而真正耗时的部分在 find_move()
    // 内部（邻域枚举 + top-K 精确解码），单次调用在病态算例上可达数百秒且无法打断。
    // 实测 la22_vdata 在设定 60s 上限的情况下跑了 **926 秒**（过冲 15 倍），
    // 既浪费预算，也让结果依赖机器负载而不可复现。
    //
    // deadline_tp_ 由 search() 设为「起始时刻 + 时间上限」；find_move() 在关键块
    // 循环与候选机器循环里周期性地查它，超时则放弃本轮、置 timed_out_ 并返回空移动，
    // search() 据此跳出。默认值为 time_point::max()，避免未设置时误判为超时。
    // ==================================================================
    using Clock = std::chrono::steady_clock;
    Clock::time_point deadline_tp_ = Clock::time_point::max();

    /// 是否已超过本轮的时间期限
    [[nodiscard]] bool past_deadline() const
    {
        return Clock::now() >= deadline_tp_;
    }

    /**
     * @brief 混合扰动：换机器 + 同机器重插 + 同机器交换。
     *
     * 原实现只做换机器，且只在目标机器头/尾两个位置尝试，
     * 对柔性小的算例（edata 每道工序仅 2 台候选、大量工序只有 1 台）几乎空转，
     * 导致数百次扰动后解仍停在同一个吸引域。这里补上改变工序顺序的算子。
     *
     * @param intensity 强度倍率，随连续无效扰动次数自适应放大
     */
    void apply_perturbation(double intensity = 1.0);

    /// 执行一次合法移动；若移动会产生环则回滚并返回 false
    bool try_apply_move(const NeighborhoodMove& move);
    /// 把工序 u 移动到 machine 上去除 u 后序列的第 pos 个位置
    bool relocate_to_position(int u, int machine, int pos);
    /// 扰动算子 1：随机换一台机器并随机选插入位置
    bool perturb_change_machine(int u);
    /// 扰动算子 2：同机器内随机改插到另一个位置
    bool perturb_reinsert(int u);
    /// 扰动算子 3：同机器内与随机另一道工序交换位置
    bool perturb_swap(int u);

    void same_machine_evaluate_and_push(const Schedule& schedule, const NeighborhoodMove& move,
                                        std::vector<NeighborhoodMove>& all_moves,
                                        std::vector<NeighborhoodMove>& best_moves, int& min_makespan) const;
};


// 线程安全的随机整数助手，统一走 rng 模块，保证全局种子可控、结果可复现
inline int get_random_int(int min, int max) {
    return rng::rand_int(min, max);
}

#endif //TABUSEARCH_H
