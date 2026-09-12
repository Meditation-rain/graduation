//
// Created on 26-9-11.
//

#include "Random.h"

#include <atomic>

namespace
{
    // 默认固定种子（与 std::mt19937 的默认种子一致），保证不显式设置时也可复现
    std::atomic<std::uint32_t> g_seed{5489U};

    // 线程编号发号器：让每个线程拿到互不相同的序列。
    // 注意：多线程下线程抵达顺序不确定，严格可复现需保证单线程运行（当前求解器即为此模式）。
    std::atomic<std::uint32_t> g_thread_index{0};

    std::mt19937 make_engine(std::uint32_t base_seed, std::uint32_t thread_idx)
    {
        std::seed_seq seq{base_seed, thread_idx, 0x9E3779B9U};
        return std::mt19937(seq);
    }
}

namespace rng
{
    void set_seed(const std::uint32_t seed) noexcept
    {
        g_seed.store(seed, std::memory_order_relaxed);
    }

    std::uint32_t seed() noexcept
    {
        return g_seed.load(std::memory_order_relaxed);
    }

    std::mt19937& engine() noexcept
    {
        thread_local std::mt19937 eng = []
        {
            const std::uint32_t base_seed = g_seed.load(std::memory_order_relaxed);
            const std::uint32_t thread_idx = g_thread_index.fetch_add(1, std::memory_order_relaxed);
            return make_engine(base_seed, thread_idx);
        }();
        return eng;
    }

    void reseed(const std::uint32_t seed) noexcept
    {
        g_seed.store(seed, std::memory_order_relaxed);
        // 把当前线程已经初始化好的引擎直接重建，从而切断与之前算例的随机流关联
        engine() = make_engine(seed, 0U);
    }

    int rand_int(const int lo, const int hi) noexcept
    {
        if (hi <= lo) return lo;
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(engine());
    }
}
