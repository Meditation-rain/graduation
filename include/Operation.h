//
// Created by chenshiyu on 26-3-30.
//

#ifndef OPERATION_H
#define OPERATION_H

#include <unordered_map>
#include <vector>
#include "Instance.h"


/**
 * @struct Operation
 * @brief Represents an operation in the FJSP with its processing options
 */
struct Operation
{
    int id; ///< Operation identifier (for debugging)
    int job_id; ///< Job this operation belongs to (0-based)
    int index; ///< Index within the job (0-based)
    std::vector<int> candidates; ///< Machines that can process this operation
    std::vector<int> duration; ///< Vector of processing times indexed by machine_id


        Operation(int id, int job_id, int index) noexcept : id(id), job_id(job_id), index(index)
    {
    }

    /**
     * @brief Get processing time on a specific machine
     * @param machine_id Machine identifier
     * @return Processing time on the specified machine
     */
    [[nodiscard]] int operator[](int machine_id) const
    {
        if (machine_id < 0 || static_cast<size_t>(machine_id) >= duration.size())
            return -1;
        return duration[machine_id];    }

    /**
     * @brief Check if this operation can be processed on a specific machine
     * @param machine_id Machine identifier to check
     * @return True if the machine can process this operation
     */
    [[maybe_unused]] [[nodiscard]] bool canBeProcessedOn(int machine_id) const
    {

            return machine_id >= 0 &&
                   static_cast<size_t>(machine_id) < duration.size() &&
                   duration[machine_id] >= 0;    }
};

/**
 * @class OperationList
 * @brief Container for all operations in an FJSP instance
 *
 * Includes virtual start (index 0) and end (last index) nodes.
 */
struct OperationList
{
    std::vector<Operation> operations; ///< List of all operations including virtual nodes

    const Instance* instance_ptr = nullptr;
    /**
     * @brief Default constructor
     */
    OperationList() = default;

    /**
     * @brief Construct operation list from an FJSP instance
     * @param instance The FJSP problem instance
     */
   // explicit OperationList(const Instance& instance);

    // === 新增/修改部分开始：这里只留声明，不要加大括号和初始化列表 ===
    explicit OperationList(const Instance& instance);
    // === 新增/修改部分结束 ===
    /**
     * @brief Access an operation by id
     * @param operation_id Operation identifier
     * @return Reference to the operation
     */
    Operation& operator[](int operation_id) { return operations[operation_id]; }

    /**
     * @brief Access an operation by id (const version)
     * @param operation_id Operation identifier
     * @return Const reference to the operation
     */
    const Operation& operator[](int operation_id) const { return operations[operation_id]; }

    /**
     * @brief Get processing time for an operation on a specific machine
     * @param operation_id Operation identifier
     * @param machine_id Machine identifier
     * @return Processing time (0 for virtual nodes)
     */
    [[nodiscard]] int duration(int operation_id, int machine_id) const
    {
        if (operation_id <= 0 || operation_id >= operations.size() - 1)
            return 0; // Virtual start/end nodes have zero duration

        return operations[operation_id][machine_id];
    }

    /**
     * @brief Get total number of operations including virtual nodes
     * @return Operation count
     */
    [[maybe_unused]] [[nodiscard]] size_t size() const { return operations.size(); }

    /**
     * @brief Get total number of real operations (excluding virtual nodes)
     * @return Real operation count
     */
    [[maybe_unused]] [[nodiscard]] size_t realOperationCount() const { return operations.size() - 2; }

    /**
     * @brief Get iterator to beginning of operations list
     * @return Iterator to first operation
     */
    auto begin() { return operations.begin(); }

    /**
     * @brief Get const iterator to beginning of operations list
     * @return Const iterator to first operation
     */
    [[nodiscard]] auto begin() const { return operations.begin(); }

    /**
     * @brief Get iterator to end of operations list
     * @return Iterator to end
     */
    auto end() { return operations.end(); }

    /**
     * @brief Get const iterator to end of operations list
     * @return Const iterator to end
     */
    [[nodiscard]] auto end() const { return operations.end(); }
    [[nodiscard]] int setup_time(int prev_op_id, int next_op_id, int machine_id) const
    {
        if (prev_op_id <= 0 || next_op_id >= operations.size() - 1) return 0; // 虚拟起始节点不产生准备时间
        if (next_op_id <= 0 || prev_op_id >= operations.size() - 1) return 0; // 虚拟终止节点不产生准备时间

        int job_i = operations[prev_op_id].job_id;
        int job_j = operations[next_op_id].job_id;

        return instance_ptr->get_setup_time(machine_id, job_i, job_j);
    }
};

/**
 * @brief Implementation of OperationList constructor
 * @param instance The FJSP problem instance
 */

// === 新增/修改部分开始：这里是函数的具体实现，初始化列表加在这里 ===
/**
 * @brief Implementation of OperationList constructor
 * @param instance The FJSP problem instance
 */
inline OperationList::OperationList(const Instance& instance)
    : instance_ptr(&instance)  // <--- 初始化列表在这里
{
    const int total_operations = instance.op_num + 2; // +2 for virtual nodes
    operations.reserve(total_operations);

    // Add virtual start node
    operations.emplace_back(0, -1, -1);

    // Add real operations
    int id = 1;
    for (int job_id = 0; job_id < instance.job_num; ++job_id)
    {
        for (int operation_idx = 0; operation_idx < instance.jobs[job_id].size(); ++operation_idx)
        {
            Operation operation{id++, job_id, operation_idx};

            // Pre-allocate vector with invalid duration markers
            operation.duration.resize(instance.machine_num, -1);

            // Reserve space for candidates (small optimization)
            const auto& op_info = instance.jobs[job_id][operation_idx];
            operation.candidates.reserve(op_info.size());

            // Add machine candidates and durations
            for (const auto& [machine, proc_time] : op_info)
            {
                operation.candidates.push_back(machine);
                operation.duration[machine] = proc_time;
            }

            operations.push_back(std::move(operation));
        }
    }

    // Add virtual end node
    operations.emplace_back(id, -1, -1);
}
// === 新增/修改部分结束 ===

#endif //OPERATION_H
