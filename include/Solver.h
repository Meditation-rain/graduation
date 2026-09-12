//
// Created by chenshiyu on 26-3-30.
//

#ifndef SOLVER_H
#define SOLVER_H
#include <string>
#include <utility>

#include "Schedule.h"


struct Instance;

/// 单个算例的求解结果
struct SolveResult
{
    int initial_makespan = 0;            ///< 启发式初始解的 makespan
    int best_makespan = 0;               ///< 禁忌搜索后的最优 makespan
    unsigned long long iterations = 0;   ///< 实际执行的迭代次数
    bool timed_out = false;              ///< 是否因触及时间上限而提前终止
    double search_seconds = 0.0;         ///< 禁忌搜索阶段实际耗时（秒）
    Schedule best_schedule{};            ///< 最优调度（供外层多次重启时比较与导出）
};

class Solver {
public:
    /**
     * @param output_dir 最优调度表（*_optimized.csv）的导出目录
     */
    explicit Solver(std::string output_dir = "../output") : output_dir_(std::move(output_dir)) {}

    /**
     * @param instance           待求解算例
     * @param max_iterations     迭代次数上限（主要终止条件，决定结果是否可复现）
     * @param time_limit_seconds 时间上限（秒），兜底保护
     * @param initial_attempts   初始解构造次数，取其中 makespan 最小的作为搜索起点
     */
    SolveResult Solve(const Instance& instance,
                      unsigned long long max_iterations = 300000ULL,
                      double time_limit_seconds = 60.0,
                      int initial_attempts = 5);

    /// 把最优调度导出为 CSV（与求解解耦，便于多次重启后只导出最终胜出的那一份）
    bool ExportSchedule(const Schedule& schedule, const Instance& instance) const;

private:
    std::string output_dir_;
};


#endif //SOLVER_H
