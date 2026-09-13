//
// Created by chenshiyu on 26-3-30.
//

#ifndef GRAPH_H
#define GRAPH_H


#include <deque>
#include <vector>
#include "Instance.h"
//#include "NeighborhoodMove1.h"
#include "NeighborhoodMove.h"
#include "Operation.h"

/**
 * Disjunctive graph representation for flexable job shop scheduling.
 * Total number of nodes = job_num * operation_num + 2 (including virtual start and end nodes)
 */
struct Graph
{
    // 必须给默认值：默认构造的 Graph（如 `Graph g;`）若未赋值就被读取，
    // 这里会是未初始化的垃圾值，属于未定义行为。
    int node_num{};
    std::vector<int> job_successor; // Successor in job sequence
    std::vector<int> machine_successor; // Successor in machine sequence

    /**
     * Backward edges in the disjunctive graph.
     * For all nodes except tail virtual nodes:
     */
    std::vector<int> job_predecessor; // Predecessor in job sequence
    std::vector<int> machine_predecessor; // Predecessor in machine sequence

    /**
     * Special edge handling for virtual nodes' outgoing edges:
     */
    std::vector<int> first_job_operation; // First operation ID for each job
    std::vector<int> last_job_operation; // Last operation ID for each job
    std::vector<int> first_machine_operation; // First operation ID for each machine
    std::vector<int> last_machine_operation; // Last operation ID for each machine

    std::vector<int> on_machine; // 操作在哪个机器上
    std::vector<int> machine_operation_count; // 每个机器上当前有几个操作
    // 【P9】原 on_machine_pos_vec 已删除：它在 make_move 里被 10 处写入、
    // 每次换机器还要沿链表做 O(链长) 的整体位移，但全项目没有任何读取点。

    /**
     * Performs topological sort on the graph
     * @param reverse If true, performs reverse topological sort
     * @return Deque containing node IDs in topologically sorted order
     */
    [[nodiscard]] std::deque<int> topological_sort(bool reverse = false) const;

    /**
     * Generates a random initial solution graph from problem instance
     * @param instance The scheduling problem instance
     * @return Graph structure representing a feasible solution
     */
    void random_init(const Instance& instance, const OperationList& operation_list);

    void heuristic_init(const Instance &instance, const OperationList &operation_list);

    int calculate_makespan_with_sdst(const OperationList &op_list) const;

    void make_move(const NeighborhoodMove& move);


};



#endif //GRAPH_H
