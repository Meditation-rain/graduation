#include "Instance.h"
#include <iostream>
#include <fstream>
#include <string>

std::ostream &operator<<(std::ostream &os, const Instance &instance) {
    os << instance.job_num << " " << instance.machine_num << " " << instance.max_candidate_num << std::endl;
    for (auto &job: instance.jobs) {
        os << job.size() << "\t";
        for (auto &operation: job) {
            os << operation.size() << " ";
            for (auto &candidate: operation) {
                os << candidate.machine << " " << candidate.duration << " ";
            }
        }
        os << "\t";
        os << std::endl;
    }
    // === 新增/修改部分开始 ===
    // 序列化输出 SDST 矩阵
    if (!instance.sdst_matrix.empty()) {
        for (int m = 0; m < instance.machine_num; ++m) {
            for (int i = 0; i < instance.job_num; ++i) {
                for (int j = 0; j < instance.job_num; ++j) {
                    os << instance.sdst_matrix[m][i][j] << " ";
                }
                os << std::endl;
            }
            os << std::endl;
        }
    }
    // === 新增/修改部分结束 ===
    return os;
}

std::istream &operator>>(std::istream &is, Instance &instance) {
    is >> instance.job_num >> instance.machine_num >> instance.max_candidate_num;
    instance.jobs.resize(instance.job_num);
    for (auto &job: instance.jobs) {
        int operation_num;      // 任务有几道工序
        is >> operation_num;
        job.resize(operation_num);
        for (auto &operation: job) {
            instance.op_num++;
            int candidate_num;  // 操作有几个候选机器
            is >> candidate_num;
            operation.resize(candidate_num);
            for (auto &candidate: operation) {
                is >> candidate.machine >> candidate.duration;
            }
        }
    }
    // === 新增/修改部分开始 ===
    // 解析 SDST 矩阵 (假设算例文件中追加了这些数据)
    // 维度初始化: machine_num x job_num x job_num
    instance.sdst_matrix.assign(instance.machine_num,
                                std::vector<std::vector<int>>(instance.job_num,
                                std::vector<int>(instance.job_num, 0)));

    for (int m = 0; m < instance.machine_num; ++m) {
        for (int i = 0; i < instance.job_num; ++i) {
            for (int j = 0; j < instance.job_num; ++j) {
                // 如果文件末尾有数据，则读取；如果没有，则默认为0 (兼容无 SDST 的标准算例)
                if (!(is >> instance.sdst_matrix[m][i][j])) {
                    instance.sdst_matrix[m][i][j] = 0;
                }
            }
        }
    }
    // 【P5】三层嵌套的 sdst_matrix 在热路径上每次查询要 3 次依赖式解引用，
    // 这里在解析完成后摊平成一维，供 update_time / LB 估计使用。
    // sdst_matrix 此后不再被修改，摊平结果始终有效。
    instance.sdst_flat.resize(static_cast<std::size_t>(instance.machine_num) *
                              instance.job_num * instance.job_num);
    {
        std::size_t k = 0;
        for (int m = 0; m < instance.machine_num; ++m) {
            for (int i = 0; i < instance.job_num; ++i) {
                for (int j = 0; j < instance.job_num; ++j) {
                    instance.sdst_flat[k++] = instance.sdst_matrix[m][i][j];
                }
            }
        }
    }

    // === 新增/修改部分结束 ===
    return is;
}

void load_instance(const char *file_name, Instance &instance) {
    std::ifstream file(file_name);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open instance: " + std::string(file_name));
    }
    file >> instance;
    file.close();

    // Save the source filename for later use (exporting, logging, etc.)
    instance.source_filename = std::string(file_name);
}

Instance::Instance(const char *filename) {
    load_instance(filename, *this);
}
