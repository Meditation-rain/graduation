//
// Created by chenshiyu on 26-3-31.
//

#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <memory>
#include "Graph.h"
#include "Instance.h"
#include "NeighborhoodMove.h"


struct OperationTimeInfo
{
    int operator_id; // Unique identifier for the operation, only use in export function
    int forward_path_length; // Longest path from start to this operation (R[i])
    int backward_path_length; // Longest path from this operation to finish (Q[i])
    int end_time; // Earliest possible completion time (R[i] + processing time)

    int r_job;     // r_u^J
    int r_machine; // r_u^M
    int q_job;     // q_u^J
    int q_machine; // q_u^M
};

enum class MoveCase {
    CASE_I,       // 对应论文 Case I
    CASE_II,      // 对应论文 Case II
    CASE_III,     // 对应论文 Case III (内部工序)
    CASE_IV,      // 对应论文 Case IV
    NOT_CRITICAL  // 非关键工序
};

// 关键块结构体
struct CriticalBlock {
    int machine_id;
    std::vector<int> operations; // 存入关键工序的 ID

    // 首工序（块内第一个）
    int first() const { return operations.front(); }
    // 尾工序（块内最后一个）
    int last() const { return operations.back(); }
    // 是否为内部工序（既不是首也不是尾）
    bool is_internal(int op_id) const {
        return operations.size() > 2 &&
               op_id != operations.front() &&
               op_id != operations.back();
    }
};

class Schedule
{
    friend class TabuSearch; // Allow TabuSearch access to private members
public:
    explicit Schedule(const Graph& graph, const std::shared_ptr<OperationList>& operation_list, const Instance* inst = nullptr) :
            graph(graph), operation_list(operation_list), instance(inst)
    {
        time_info.resize(graph.node_num);
    }    Schedule() = default;

    // 【修改 3】另一个构造函数：同样增加对 instance 的绑定
    Schedule(const Instance& instance, const std::shared_ptr<OperationList>& operation_list) :
        operation_list(operation_list), instance(&instance) // 绑定 instance 地址
    {
        graph.random_init(instance, *operation_list);
        time_info.resize(graph.node_num);
        update_time();
    }


    void update_time();

    void export_schedule(const char* filename) const;

    bool is_legal_move(const NeighborhoodMove &move) const;

    void make_move(const NeighborhoodMove &move);

    //friend int same_machine_evaluate(const Schedule& schedule, const NeighborhoodMove& move);
    // friend int change_machine_evaluate(const Schedule& schedule, const NeighborhoodMove& move,
    //                                    const std::vector<int>& intersection);

     int get_makespan() const { return makespan; }
     //bool is_legal_move(const NeighborhoodMove& move) const;
    MoveCase get_move_case(int u) const;

    void output() const;

    std::vector<CriticalBlock> get_critical_blocks() const;

private:
    int makespan{};
    Graph graph;
    std::shared_ptr<OperationList> operation_list;
    std::vector<OperationTimeInfo> time_info;

    // 【修改 4】新增私有成员：存储 instance 指针
    const Instance* instance = nullptr;

    bool is_critical_operation(int operation_id) const;

    //void make_move(const NeighborhoodMove& move);
};

inline bool Schedule::is_critical_operation(int operation_id) const
{
    return (time_info[operation_id].end_time + time_info[operation_id].backward_path_length == makespan);
}


#endif
