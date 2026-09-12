//
// Created by chenshiyu on 26-3-31.
//

#ifndef NEIGHBORHOODMOVE_H
#define NEIGHBORHOODMOVE_H

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

/**
 * @enum Method
 * @brief Enumerates different types of neighborhood moves for FJSP optimization.
 *
 * 对应 Shen et al. (2018) 论文中的 4 种 Move Types:
 * - BACK: Move type 1 (在同一机器上，将 u 插入到 v 之后)
 * - FRONT: Move type 2 (在同一机器上，将 u 插入到 w 之前)
 * - CHANGE_MACHINE_BACK: Move type 3 (换到机器 k'，并将 u 插入到 v 之后) [cite: 378]
 * - CHANGE_MACHINE_FRONT: Move type 4 (换到机器 k'，并将 u 插入到 w 之前) [cite: 380]
 */
enum class Method
{
    BACK,
    FRONT,
    CHANGE_MACHINE_BACK,
    CHANGE_MACHINE_FRONT
};

struct NeighborhoodMove
{
    Method method; ///< 移动类型
    int which;     ///< 要移动的工序 (对应论文中的 u)
    int where;     ///< 目标位置的参照工序 (如果是 BACK，则代表 v；如果是 FRONT，则代表 w)
    int target_machine; ///< 目标机器 ID (对应论文中的 k')，对于 BACK/FRONT 动作，该值可以保持为原来的机器

    // [新增] 默认构造函数：将 which 设为 0，标记为空动作
    constexpr NeighborhoodMove() noexcept :
        method(Method::BACK), which(0), where(-1), target_machine(-1)
    {
    }

    // 更新了构造函数以包含 target_machine
    constexpr NeighborhoodMove(Method method, int which, int where, int target_machine = -1) noexcept :
        method(method), which(which), where(where), target_machine(target_machine)
    {
    }

    friend std::ostream& operator<<(std::ostream& os, const NeighborhoodMove& move)
    {
        os << "method: " << move.str()
           << " which: " << move.which
           << " where: " << move.where
           << " target_machine: " << move.target_machine;
        return os;
    }

    std::string str() const
    {
        static constexpr std::array<std::string_view, 4> methodToString = {
            "BACK", "FRONT", "CHANGE_MACHINE_BACK", "CHANGE_MACHINE_FRONT"
        };

        const auto index = static_cast<size_t>(method);
        if (index >= methodToString.size())
        {
            throw std::invalid_argument("Unknown method enum value");
        }
        return std::string(methodToString[index]);
    }
};


#endif //NEIGHBORHOODMOVE_H
