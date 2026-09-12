//
// Created by chenshiyu on 26-3-30.
//

#include "Solver.h"

#include <algorithm>
#include <atomic>
#include <climits>
#include <filesystem>
#include <iostream>

#include "Graph.h"
#include "Schedule.h"
#include "TabuSearch.h" // 必须包含禁忌搜索头文件

SolveResult Solver::Solve(const Instance &instance,
                          const unsigned long long max_iterations,
                          const double time_limit_seconds,
                          const int initial_attempts) {
    SolveResult result;

    // 1. 初始化工序列表
    auto op_list = std::make_shared<OperationList>(instance);

    // 2. 多次构造初始解，取 makespan 最小的那个作为搜索起点。
    //    构造过程是 GRASP（分派规则 + 随机），单次质量波动较大，多试几次成本很低。
    Graph best_graph;
    int best_initial = INT_MAX;
    const int attempts = std::max(1, initial_attempts);

    for (int attempt = 0; attempt < attempts; ++attempt) {
        Graph graph;
        graph.heuristic_init(instance, *op_list);
        Schedule candidate(graph, op_list, &instance);
        candidate.update_time();
        if (candidate.get_makespan() < best_initial) {
            best_initial = candidate.get_makespan();
            best_graph = graph;
        }
    }

    // 3. 用胜出的初始解构造调度
    Schedule initial_schedule(best_graph, op_list, &instance);
    initial_schedule.update_time();

    const int initial_makespan = initial_schedule.get_makespan();
    std::cout << "初始解生成的 Makespan: " << initial_makespan
              << "（构造 " << attempts << " 次取优）" << std::endl;

    // 4. --- 核心步骤：启动 Shen et al. (2018) 禁忌搜索 ---
    TabuSearch ts(instance, max_iterations, time_limit_seconds);
    std::atomic<bool> stop_flag(false);

    std::cout << "开始禁忌搜索优化 (迭代上限 " << max_iterations
              << "，时间上限 " << time_limit_seconds << " s)..." << std::endl;
    ts.search(initial_schedule, stop_flag);

    // 5. 获取优化后的最佳结果
    Schedule best_schedule = ts.best_schedule;
    const int optimized_makespan = best_schedule.get_makespan();

    result.iterations = ts.get_iteration();
    result.timed_out = ts.timed_out();
    result.search_seconds = ts.elapsed_seconds();

    std::cout << "优化后的最终 Makespan: " << optimized_makespan << std::endl;
    std::cout << "改进量: " << initial_makespan - optimized_makespan << std::endl;
    std::cout << "搜索迭代: " << ts.get_iteration()
              << " 次，耗时 " << result.search_seconds << " s"
              << (result.timed_out ? "（达到时间上限）" : "（提前结束）") << std::endl;

    result.initial_makespan = initial_makespan;
    result.best_makespan = optimized_makespan;
    result.best_schedule = best_schedule;
    return result;
}

bool Solver::ExportSchedule(const Schedule& schedule, const Instance& instance) const
{
    try {
        std::string base = !instance.source_filename.empty() ?
                           std::filesystem::path(instance.source_filename).stem().string() : "result";
        const std::string csv_file = output_dir_ + "/" + base + "_optimized.csv";
        schedule.export_schedule(csv_file.c_str());
        return true;
    } catch (const std::exception &e) {
        std::cerr << "导出失败: " << e.what() << std::endl;
        return false;
    }
}
