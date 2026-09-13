//
// Created by chenshiyu on 26-3-31.
//

#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <climits>
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

    /// 只做正向松弛求 makespan，**不计算**反向尾量（q_job / q_machine / backward_path_length）。
    ///
    /// 供 top-K 探针使用：探针每代评估 8 个候选，但只读 makespan，
    /// 反向尾量是给「下一轮识别关键路径 / 计算 LB」用的，探针阶段用不上。
    ///
    /// 实现上是「Kahn 拓扑排序 + 最长路松弛」的**融合版**：
    /// 析取图中每个节点最多两个前驱（工件前驱 + 机器前驱），当入度归零时
    /// 两个前驱必然都已松弛完，因此可以在出队瞬间就地算出 end_time，
    /// 不需要先排好序再遍历一遍。顺带消除了原 topological_sort 的
    /// 3 次堆分配（in_degree vector + 2 个 deque）。
    ///
    /// @param abort_bound 一旦运行中的 makespan 严格大于该值，立即放弃并返回 INT_MAX。
    ///                    探针只需要知道「是否比已知候选更好」，因此可以提前中止。
    ///                    注意只在「严格大于」时中止，等于时照常算完，以维持平局的取舍顺序。
    /// @return makespan；若提前中止或图中有环，返回 INT_MAX。
    ///
    /// 注意：调用后 time_info 的 q_* 与 backward_path_length 仍是旧值，调用方不可依赖。
    int eval_makespan_only(int abort_bound = INT_MAX);

    /// 探针专用：在图上执行移动，并用「只算正向」的方式求 makespan。
    /// 不再抛异常——成环时返回 INT_MAX（等价于「这个移动不可行 / 不更优」）。
    int apply_and_eval_makespan(const NeighborhoodMove& move, int abort_bound = INT_MAX);

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

    // 【P2】正向评估的复用缓冲区。
    // 原实现每次调用 topological_sort 都新建 1 个 vector + 2 个 deque，
    // 每代 8 次探针就是 24 次堆分配。这里改成成员复用，稳态下零分配。
    std::vector<int> fwd_indegree_; // 各节点剩余未处理的前驱数
    std::vector<int> fwd_queue_;    // Kahn 队列（用 head 指针模拟出队，避免 pop_front）

    bool is_critical_operation(int operation_id) const;

    //void make_move(const NeighborhoodMove& move);
};

inline bool Schedule::is_critical_operation(int operation_id) const
{
    return (time_info[operation_id].end_time + time_info[operation_id].backward_path_length == makespan);
}


#endif
