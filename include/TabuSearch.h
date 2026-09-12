//
// Created by chenshiyu on 26-3-31.
//

#ifndef TABUSEARCH_H
#define TABUSEARCH_H
#include <atomic>
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
        tabu_list(2000), // 如果你的 Instance 里没有 total_op_num，请替换为你图中实际的 node_num
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
