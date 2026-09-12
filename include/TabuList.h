//
// Created by chenshiyu on 26-3-31.
//

#ifndef TABULIST_H
#define TABULIST_H

#include <vector>
#include <algorithm>

/**
 * 严格遵照 Shen et al. (2018) 定义的禁忌表：
 * 记录操作对 (v, u)，表示在禁忌期内，禁止将工序 u 再次放置在工序 v 的正后方。
 * 使用展平的 2D 矩阵实现，提供极致的 O(1) 读写性能。
 */
struct TabuList
{
    int total_nodes; // 析取图中的节点总数 (包含 dummy nodes)

    // 展平的二维矩阵: tabu_matrix[v * total_nodes + u] = 过期迭代次数
    std::vector<unsigned long long> tabu_matrix;

    /**
     * 构造函数
     * @param node_num 图中的工序节点总数 (Schedule.graph.node_num)
     */
    explicit TabuList(int node_num = 0) : total_nodes(node_num)
    {
        if (node_num > 0) {
            tabu_matrix.resize(node_num * node_num, 0);
        }
    }

    /**
     * 添加禁忌对 (v, u)
     * @param v 前置工序 ID
     * @param u 后置工序 ID
     * @param expiration_iter 解除禁忌的迭代次数
     */
    void add_pair(int v, int u, unsigned long long expiration_iter)
    {
        if (v >= 0 && v < total_nodes && u >= 0 && u < total_nodes)
        {
            tabu_matrix[v * total_nodes + u] = expiration_iter;
        }
    }

    /**
     * 检查操作对 (v, u) 是否处于禁忌状态
     * @param v 前置工序 ID
     * @param u 后置工序 ID
     * @param current_iter 当前迭代次数
     * @return 如果仍在禁忌期内返回 true
     */
    [[nodiscard]] bool is_tabu(int v, int u, unsigned long long current_iter) const
    {
        if (v >= 0 && v < total_nodes && u >= 0 && u < total_nodes)
        {
            return tabu_matrix[v * total_nodes + u] >= current_iter;
        }
        return false;
    }

    /**
     * 清空禁忌表 (用于 Multi-start 多起点重启时)
     */
    void clear()
    {
        std::fill(tabu_matrix.begin(), tabu_matrix.end(), 0);
    }
};


#endif //TABULIST_H
