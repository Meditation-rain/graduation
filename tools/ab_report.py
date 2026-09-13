#!/usr/bin/env python3
"""对多次实验做 A/B 汇总对比。

先对每个实验目录调用 compare_benchmark.py 生成各自的 benchmark_report.csv，
再把它们并排汇总成一张表，并给出判定所需的指标：

  - 达下界数（Cmax == LB，且 LB==UB 时为「已证最优」）
  - 平均 / 最大 gap_lb
  - 优于文献 UB 的算例数（破纪录数）
  - 逐例的质量变化（变好 / 变差 / 持平）

用法:
    python3 tools/ab_report.py <run_dir1> <run_dir2> [...]
    python3 tools/ab_report.py runs/p0_r1 runs/new_1M

第一个目录被视为**基线**，其余与它逐例对比。
"""
import csv
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COMPARE = os.path.join(ROOT, "tools", "compare_benchmark.py")


def short_name(path):
    # runs/p0_r1 -> p0_r1
    return os.path.basename(os.path.normpath(path))


def prepare(run_dir):
    """确保该目录下已有 benchmark_report.csv"""
    src = os.path.join(run_dir, "tabu_search_results.csv")
    if not os.path.isfile(src):
        sys.exit(f"[ab_report] 找不到 {src}")
    dst = os.path.join(run_dir, "benchmark_report.csv")
    if not os.path.isfile(dst):
        subprocess.run([sys.executable, COMPARE, src], check=True,
                       stdout=subprocess.DEVNULL)
    if not os.path.isfile(dst):
        sys.exit(f"[ab_report] compare_benchmark.py 未能生成 {dst}")
    with open(dst, encoding="utf-8-sig") as f:
        return {r["Instance"]: r for r in csv.DictReader(f)}


def main():
    dirs = sys.argv[1:]
    if len(dirs) < 1:
        sys.exit(__doc__)

    data = [(short_name(d), prepare(d)) for d in dirs]
    base_name, base = data[0]
    instances = sorted(base.keys(), key=lambda k: (base[k]["Family"], k))

    # ---- 逐例明细 ----
    names = [n for n, _ in data]
    header = f"{'算例':<14}{'族':<14}{'LB':>7}{'UB':>7}"
    for n in names:
        header += f"{n:>12}"
    if len(names) > 1:
        header += f"{'vs基线':>10}"
    print(header)
    print("-" * len(header))

    deltas = []  # (变好, 变差, 持平) 相对基线
    for inst in instances:
        row = base[inst]
        line = f"{inst:<14}{row['Family']:<14}{row['LB']:>7}{row['UB']:>7}"
        for _, d in data:
            line += f"{d[inst]['Best_Cmax']:>12}"
        if len(names) > 1:
            b = int(base[inst]["Best_Cmax"])
            cur = int(data[-1][1][inst]["Best_Cmax"])
            if cur < b:
                line += "✅ 更好".rjust(10)
                deltas.append(1)
            elif cur > b:
                line += "❌ 变差".rjust(10)
                deltas.append(-1)
            else:
                line += "—".rjust(10)
                deltas.append(0)
        print(line)

    # ---- 汇总 ----
    print()
    print(f"{'配置':<16}{'算例数':>7}{'达下界':>8}{'已证最优':>9}{'破纪录':>8}"
          f"{'平均gap':>10}{'最大gap':>10}{'超LB合计':>10}")
    print("-" * 78)
    for name, d in data:
        n = len(d)
        hit = sum(1 for i in d if d[i]["LB"] == d[i]["UB"]
                  and int(d[i]["Best_Cmax"]) == int(d[i]["LB"]))
        proved = sum(1 for i in d if d[i]["LB"] == d[i]["UB"])
        beat = sum(1 for i in d if int(d[i]["Best_Cmax"]) < int(d[i]["UB"]))
        gaps = [float(d[i]["Run1_Gap(%)"]) for i in d]
        over = sum(max(0, int(d[i]["Best_Cmax"]) - int(d[i]["LB"])) for i in d)
        print(f"{name:<16}{n:>7}{hit:>8}{proved:>9}{beat:>8}"
              f"{sum(gaps)/n:>9.2f}%{max(gaps):>9.2f}%{over:>10}")

    if deltas:
        good = deltas.count(1)
        bad = deltas.count(-1)
        print()
        print(f"相对基线 [{base_name}]：变好 {good} 例 / 变差 {bad} 例 / 持平 {deltas.count(0)} 例")
        if bad == 0 and good > 0:
            print("  → 判定：**胜出**（无回退且有增益）")
        elif bad > 0 and good > bad:
            print("  → 判定：增益为主，但有回退，需逐例复核")
        elif bad >= good and bad > 0:
            print("  → 判定：**回退**（变差不少于变好，建议关闭该改动）")


if __name__ == "__main__":
    main()
