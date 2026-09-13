#include <cstdint>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono> // 用于统计求解时间
#include <iomanip>
#include <algorithm>
#include <string>
#include <vector>

#include "Instance.h"
#include "Random.h"
#include "Solver.h"
#include "Config.h"

namespace
{
    constexpr unsigned long long kDefaultIterations = 1000000ULL; // 迭代上限：主要终止条件
    constexpr unsigned kDefaultSeed = 20240911U;                 // 固定种子，保证实验可复现
    constexpr double kDefaultTimeLimit = 60.0;                   // 时间上限（秒）：兜底保护
    constexpr int kDefaultRestarts = 1;                          // 每个算例独立重启次数
    constexpr int kDefaultInitAttempts = 5;                      // 初始解构造次数
    const std::string kDefaultInstanceDir = "../instance";
    const std::string kDefaultOutputDir = "../output";

    void print_usage(const char* program)
    {
        std::cout << "用法: " << program << " [选项]\n"
                  << "  --iter N   迭代次数上限，默认 " << kDefaultIterations
                  << "（主要终止条件，决定结果是否可复现）\n"
                  << "  --seed S   随机种子，默认 " << kDefaultSeed << "\n"
                  << "  --time T   时间上限(秒)，默认 " << kDefaultTimeLimit
                  << "（仅兜底，触发后结果不可复现）\n"
                  << "  --restarts N  每个算例独立重启并取最优的次数，默认 " << kDefaultRestarts << "\n"
                  << "  --init N   初始解构造次数（取最优），默认 " << kDefaultInitAttempts << "\n"
                  << "  --inst DIR 算例目录，默认 " << kDefaultInstanceDir << "\n"
                  << "  --out  DIR 调度表输出目录，默认 " << kDefaultOutputDir << "\n"
                  << "\n实验开关（用于 A/B 与回退）：\n"
                  << "  --no-fast-probe   关闭 P1 探针正向-only 评估，改回完整 update_time()\n"
                  << "  --no-adaptive-topk  关闭 P11 动态 top-K，固定为 8\n"
                  << "  --topk-max N      P11 打平时 K 的上限，默认 32\n"
                  << "  --idx-offset N    算例序号偏移；分片并行跑时用于还原全局序号，保证种子不变。默认 0\n"
                  << "示例: " << program << " --inst ../instance_SDST --out ../output_SDST\n";
    }

    /// 由 (全局种子, 算例序号, 重启序号) 推导出互不相同的运行种子
    std::uint32_t derive_seed(unsigned base, std::size_t idx, int restart)
    {
        const auto mixed = static_cast<std::uint64_t>(base)
                         ^ static_cast<std::uint64_t>(idx) * 2654435761ULL
                         ^ static_cast<std::uint64_t>(restart) * 2246822519ULL;
        return static_cast<std::uint32_t>(mixed ^ (mixed >> 32));
    }
}

int main(int argc, char* argv[]) {
    unsigned long long max_iterations = kDefaultIterations;
    unsigned seed = kDefaultSeed;
    double time_limit = kDefaultTimeLimit;
    int restarts = kDefaultRestarts;
    int init_attempts = kDefaultInitAttempts;
    std::string instance_dir = kDefaultInstanceDir;
    std::string output_dir = kDefaultOutputDir;
    // 算例序号偏移：把算例集切成若干分片并行跑时，用它在每个分片内还原算例的
    // 「全局序号」，从而拿到与串行跑完全相同的种子（见 main.cpp 中 derive_seed 的调用）。
    // 默认 0，即序号就是本目录内的排序位置，行为与原先一致。
    int idx_offset = 0;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto next_value = [&](const char* name) -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument(std::string(name) + " 缺少取值");
                return argv[++i];
            };

            if (arg == "--iter") {
                max_iterations = std::stoull(next_value("--iter"));
            } else if (arg == "--seed") {
                seed = static_cast<unsigned>(std::stoul(next_value("--seed")));
            } else if (arg == "--time") {
                time_limit = std::stod(next_value("--time"));
            } else if (arg == "--restarts") {
                restarts = std::stoi(next_value("--restarts"));
            } else if (arg == "--init") {
                init_attempts = std::stoi(next_value("--init"));
            } else if (arg == "--inst") {
                instance_dir = next_value("--inst");
            } else if (arg == "--out") {
                output_dir = next_value("--out");
            } else if (arg == "--no-fast-probe") {
                cfg::fast_probe = false; // P1 回退：探针改回完整 update_time()
            } else if (arg == "--no-adaptive-topk") {
                cfg::adaptive_topk = false; // P11 回退：top-K 固定为 8
            } else if (arg == "--topk-max") {
                cfg::topk_max = std::stoi(next_value("--topk-max")); // P11 打平时 K 的上限
            } else if (arg == "--idx-offset") {
                idx_offset = std::stoi(next_value("--idx-offset")); // 分片并行时还原全局算例序号
            } else if (arg == "--lahc") {
                cfg::lahc = true; // C1：开启 LAHC 滑动窗口接受准则
            } else if (arg == "--lahc-H") {
                cfg::lahc_H = std::stoi(next_value("--lahc-H")); // C1：窗口长度 H
            } else if (arg == "--adaptive-perturb") {
                cfg::adaptive_perturb = true; // C3：开启 ILS 自适应扰动强度
            } else if (arg == "--help" || arg == "-h") {
                print_usage(argv[0]);
                return 0;
            } else {
                std::cerr << "未知参数: " << arg << std::endl;
                print_usage(argv[0]);
                return 1;
            }
        }

        if (max_iterations == 0 || time_limit <= 0.0 || restarts < 1 || init_attempts < 1) {
            std::cerr << "迭代次数、时间上限必须为正数，重启次数与初始解构造次数至少为 1" << std::endl;
            return 1;
        }
    } catch (const std::exception&) {
        print_usage(argv[0]);
        return 1;
    }

    // 固定随机种子：保证同一算例、同一参数下结果完全可复现
    rng::set_seed(seed);

    try {
        const std::string output_csv = "tabu_search_results.csv";

        if (!std::filesystem::exists(instance_dir)) {
            std::cerr << "找不到算例目录: " << instance_dir << "（请在 build 目录下运行）" << std::endl;
            return 1;
        }
        std::filesystem::create_directories(output_dir);

        // 按文件名排序后逐个求解：std::filesystem::directory_iterator 的遍历顺序与文件系统有关，
        // 排序后才能保证多次运行顺序一致、结果可复现。
        std::vector<std::filesystem::path> instance_files;
        for (const auto &entry : std::filesystem::directory_iterator(instance_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                instance_files.push_back(entry.path());
            }
        }
        std::sort(instance_files.begin(), instance_files.end());

        std::ofstream out(output_csv);
        if (!out.is_open()) {
            std::cerr << "无法创建输出文件: " << output_csv << std::endl;
            return 1;
        }

        // 写入表头
        out << "Instance,Initial_Cmax,Optimized_Cmax,Improvement(%),Time(s),Iterations,TimedOut,Seed,Run_Seed,Restarts\n";

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "============================================================" << std::endl;
        std::cout << "   Shen et al. (2018) FJSP-SDST Algorithm Reproduction      " << std::endl;
        std::cout << "============================================================" << std::endl;
        std::cout << "迭代上限: " << max_iterations << "    时间上限: " << time_limit
                  << " s/算例    随机种子: " << seed << std::endl;
        std::cout << "重启次数: " << restarts << "    初始解构造次数: " << init_attempts << std::endl;
        std::cout << "实验开关: " << cfg::summary() << std::endl;
        std::cout << "待求解算例数: " << instance_files.size() << std::endl;

        for (std::size_t idx = 0; idx < instance_files.size(); ++idx) {
            const auto &path = instance_files[idx];
            const std::string filename = path.string();
            const std::string short_name = path.filename().string();

            std::cout << "\n[正在求解] " << short_name << "..." << std::endl;

            // 全局序号 = 本目录内的排序位置 + 分片偏移。
            // 这样把算例集切成多片并行跑时，每个算例仍拿到与整批串行跑完全相同的种子。
            const std::size_t global_idx = idx + static_cast<std::size_t>(idx_offset);

            try {
                // 0. 为当前算例重新播种。
                //    若不重置，算例会共用同一条随机流，其初始解取决于前面算例消耗了多少
                //    随机数 —— 增删算例或改动迭代次数都会让它变化，实验无法逐例复现。
                const std::uint32_t instance_seed = derive_seed(seed, global_idx, 0);

                // 1. 加载算例
                Instance instance(filename.c_str());
                Solver solver(output_dir);

                // 2. 计时开始
                auto start = std::chrono::high_resolution_clock::now();

                // 3. 多次独立重启求解，取 makespan 最小的一次。
                //    每次重启都重新播种，保证 restart k 的结果与 k 的取值无关。
                SolveResult result;
                for (int r = 0; r < restarts; ++r) {
                    rng::reseed(derive_seed(seed, global_idx, r));
                    if (restarts > 1) {
                        std::cout << "  [重启 " << (r + 1) << "/" << restarts << "]" << std::endl;
                    }
                    SolveResult one = solver.Solve(instance, max_iterations, time_limit, init_attempts);
                    if (result.best_makespan <= 0 || one.best_makespan < result.best_makespan) {
                        result = one;
                    }
                }
                solver.ExportSchedule(result.best_schedule, instance);

                const int initial_cmax = result.initial_makespan;
                const int final_cmax = result.best_makespan;

                // 4. 计时结束
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> diff = end - start;

                // 计算改进百分比
                double improvement = 0.0;
                if (initial_cmax > 0) {
                    improvement = static_cast<double>(initial_cmax - final_cmax) / initial_cmax * 100.0;
                }

                // 打印结果到控制台
                std::cout << "  - 初始 Makespan: " << initial_cmax << std::endl;
                std::cout << "  - 最终 Makespan: " << final_cmax << std::endl;
                std::cout << "  - 改进幅度: " << improvement << "%" << std::endl;
                std::cout << "  - 耗时: " << diff.count() << " s" << std::endl;

                // 5. 写入 CSV
                out << short_name << ","
                    << initial_cmax << ","
                    << final_cmax << ","
                    << improvement << ","
                    << diff.count() << ","
                    << result.iterations << ","
                    << (result.timed_out ? 1 : 0) << ","
                    << seed << ","
                    << instance_seed << ","
                    << restarts << "\n";

            } catch (const std::exception &e) {
                std::cerr << "  [错误] " << short_name << ": " << e.what() << std::endl;
                out << short_name << ",ERROR,ERROR,ERROR,0,0,0," << seed << ",0," << restarts << "\n";
            }
        }

        out.close();
        std::cout << "\n============================================================" << std::endl;
        std::cout << "所有测试完成！详细结果见: " << output_csv << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "全局致命错误：" << e.what() << std::endl;
        return 1;
    }
    return 0;
}
