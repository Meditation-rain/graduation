//
// Created on 26-9-11.
//
// 全局可控随机数发生器。
//
// 论文复现实验要求结果可复现，因此这里用固定默认种子取代原先的
// std::random_device：不显式设置种子时，同一算例每次运行得到完全一致的结果。
//

#ifndef RANDOM_H
#define RANDOM_H

#include <cstdint>
#include <random>

namespace rng
{
    /**
     * @brief 设置全局随机种子。
     *
     * 必须在任何线程首次调用 engine() 之前调用；此后调用不会影响已经
     * 初始化过的线程引擎。
     */
    void set_seed(std::uint32_t seed) noexcept;

    /**
     * @brief 重置当前线程的随机流到指定种子。
     *
     * 批量求解多个算例时，必须在每个算例开始之前调用一次。否则算例之间会
     * 共用同一条随机流，导致某个算例的结果依赖于它前面算例消耗了多少随机数
     * (表现为: 增删算例或改动迭代次数后，后面算例的初始解和最终结果都变了)。
     */
    void reseed(std::uint32_t seed) noexcept;

    /**
     * @brief 获取当前生效的全局种子。
     */
    std::uint32_t seed() noexcept;

    /**
     * @brief 获取当前线程的梅森旋转引擎。
     *
     * 首次调用时按全局种子惰性初始化，之后一直复用同一个引擎状态。
     */
    std::mt19937& engine() noexcept;

    /**
     * @brief 返回 [lo, hi] 闭区间内的均匀随机整数。
     */
    int rand_int(int lo, int hi) noexcept;
}

#endif //RANDOM_H
