# graduation

> 基于 Shen et al. (2018) 禁忌搜索（Tabu Search）的柔性作业车间调度（FJSP）求解器，
> 扩展支持**序列相关准备时间（Sequence-Dependent Setup Times, SDST）**。

## 项目简介

本仓库实现了一个 FJSP-SDST 调度求解器，采用析取图（disjunctive graph）建模，
核心算法为 Shen 等人 2018 年提出的禁忌搜索框架，并在此基础上做了若干工程改进。

- **非 SDST 算例**：与 FJSPLib 公开基准（Brandimarte / Dauzère-Pérès / Hurink）对照，评 gap 与达下界情况。
- **SDST 算例**：在 `instance_SDST/` 中生成带序列相关准备时间的变体，评可行性与相对非 SDST 基线的 Cmax 增量。

## 特性

- 析取图 + SDST 三处集成（初始解、下界、移动合法性）
- 初始解：GRASP + MWKR + top-K 机器随机
- 邻域：N5/N6 四类移动 + 关键块端点跳跃；top-K=8 完整解码精确评估
- 下界：LB1/LB2（含 SDST 感知）；移动分 Case I–IV 处理
- 搜索：动态禁忌长度 + 特赦 + 成环安全回退；混合扰动 + ILS 拉回
- 可复现：按算例序号派生随机种子，结果可重跑

## 目录结构

```
.
├── CMakeLists.txt            # 构建配置
├── main.cpp                  # 入口：参数解析、批量求解、结果输出
├── include/                  # 头文件（TabuSearch / Schedule / Graph / ...）
├── src/                      # 实现
├── tools/                    # 评测脚本（compare_benchmark / verify_schedule / make_sdst_report）
├── instance/                 # 非 SDST 算例（FJSPLib）
├── instance_SDST/            # 带序列相关准备时间的算例变体
├── benchmark_report.csv / benchmark_report_1M.csv      # 非 SDST 评测报告（300k / 1M 迭代）
├── sdst_report.csv / sdst_report_1M.csv               # SDST 评测报告
├── 技术方案与改进路线.md       # 现有方案 / 已回退方案 / 未来路线
└── git操作指南.md            # 本地 Git 操作说明
```

## 构建与运行

```bash
cmake -B build && cmake --build build -j8
# 批量求解（默认 1,000,000 次迭代 / top-K=8）
./build/MyProject --inst instance --out output
# 指定迭代次数、种子
./build/MyProject --inst instance --out output --iter 300000 --seed 20240911
```

## 评测结果（摘要）

| 指标 | 300k 迭代 | 1M 迭代 |
|---|---|---|
| 非 SDST 达下界（已证最优）/ 157 | 33 (21.0%) | 41 (26.1%) |
| 非 SDST gap_LB（均值） | 2.38% | 1.74% |
| SDST 可行性 | 157/157 全部可行 | 157/157 全部可行 |
| SDST Cmax vs 非 SDST 基线 | +13.38% | +13.02% |

> 详细分族统计见 `benchmark_report_1M.csv` 与 `sdst_report_1M.csv`。

## 参考文献

- Shen, X., et al. (2018). *A Tabu Search Algorithm for the Flexible Job Shop Scheduling Problem with Sequence-Dependent Setup Times*.

## License

本项目为毕业设计相关代码，使用请注明出处。
