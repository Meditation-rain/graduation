//
// Created by chenshiyu on 26-3-30.
//

#ifndef INSTANCE_H
#define INSTANCE_H

#include <vector>
#include <iomanip>
#include <string>

// 工序的候选机器
struct Candidate {
    int machine;    // 机器编号
    int duration;   // 持续时长
};

struct Instance {
    int job_num{};
    int machine_num{};
    int max_candidate_num{};
    std::vector<std::vector<std::vector<Candidate>>> jobs;
    int op_num{};

    // === 新增/修改部分开始 ===
    // 存储序列相关准备时间矩阵 s_{ii'k}
    // 维度: [machine_id][prev_job_id][next_job_id]
    std::vector<std::vector<std::vector<int>>> sdst_matrix;

    // 保存该 Instance 的源文件名（用于生成导出文件名等）
    std::string source_filename;

    Instance() = default;
    explicit Instance(const char *filename);

    /**
     * @brief 获取在机器 k 上，先后加工 job_i 和 job_j 之间的准备时间
     */
    [[nodiscard]] int get_setup_time(int machine_id, int prev_job, int next_job) const {
        // 同一个工件的不同工序在同一台机器上连续加工，准备时间为 0
        if (prev_job == next_job) return 0;

        // 虚拟节点(job_id = -1)不产生准备时间
        if (prev_job == -1 || next_job == -1) return 0;

        return sdst_matrix[machine_id][prev_job][next_job];
    }
    // === 新增/修改部分结束 ===
};

void load_instance(const char *filename, Instance &instance);

std::istream &operator>>(std::istream &is, Instance &instance);

std::ostream &operator<<(std::ostream &os, const Instance &instance);


#endif //INSTANCE_H
