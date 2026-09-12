#!/usr/bin/env python3
"""把求解结果与 FJSPLib 的已知下界/上界对比。

用法:
    python3 tools/compare_benchmark.py <tabu_search_results.csv> [更多 csv...]

输出:
    - 每个算例的 Cmax / LB / UB，以及相对下界的偏差 gap_lb = (Cmax - LB)/LB
    - 相对最好已知上界的偏差 gap_ub = (Cmax - UB)/UB
    - 按族(brandimarte / dauzere / hurink-*)汇总: 命中最优数、平均/最大偏差
    - 汇总行写入 <第一个csv同目录>/benchmark_report.csv
"""
import csv
import os
import re
import sys

from collections import defaultdict

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
BENCH_FILE = os.path.join(TOOLS_DIR, "benchmarks.csv")


def load_benchmarks(path=BENCH_FILE):
    """返回 {instance_key: (lb, ub)}"""
    ref = {}
    with open(path, encoding="utf-8-sig") as f:   # utf-8-sig 自动吃掉 BOM
        lines = [ln for ln in f if not ln.lstrip().startswith("#")]
        for row in csv.DictReader(lines):
            if not row.get("instance"):
                continue
            ref[row["instance"].strip()] = (int(row["lb"]), int(row["ub"]))
    return ref


def to_key(filename):
    """fjsp.brandimarte.Mk01.m6j10c3.txt -> mk01
       fjsp.dauzere.01a.m5j10c3.txt     -> dpp01a
       fjsp.hurink.vdata-la38.m15j15c12 -> la38_vdata
    """
    name = os.path.basename(filename)
    name = re.sub(r"\.(txt|csv)$", "", name)

    m = re.match(r"fjsp\.brandimarte\.(Mk\d+)", name, re.I)
    if m:
        return m.group(1).lower()

    m = re.match(r"fjsp\.dauzere\.(\d+a)", name, re.I)
    if m:
        return "dpp" + m.group(1).lower()

    m = re.match(r"fjsp\.hurink\.(edata|rdata|vdata)-(\w+)", name, re.I)
    if m:
        base = m.group(2).lower()
        return f"{base}_{m.group(1).lower()}"

    return name.lower()


def family_of(key):
    if key.startswith("mk"):
        return "brandimarte"
    if key.startswith("dpp"):
        return "dauzere"
    m = re.match(r"\w+_(edata|rdata|vdata)$", key)
    if m:
        return "hurink-" + m.group(1)
    return "other"


def load_results(path):
    """返回 (list of dict, 参数说明)"""
    rows = []
    with open(path, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            try:
                r["_cmax"] = int(r["Optimized_Cmax"])
                r["_init"] = int(r["Initial_Cmax"])
            except (KeyError, ValueError):
                continue
            rows.append(r)
    return rows


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1

    ref = load_benchmarks()
    paths = argv[1:]
    runs = []
    for p in paths:
        rs = load_results(p)
        # 结果文件常同名(tabu_search_results.csv)，标签带上两级父目录以便区分
        d1 = os.path.dirname(os.path.abspath(p))
        d2 = os.path.dirname(d1)
        parts = [os.path.basename(d2), os.path.basename(d1)] if d2 != d1 else [os.path.basename(d1)]
        label = "/".join(x for x in parts if x) + "/" + os.path.basename(p)
        runs.append({"path": p, "label": label, "rows": rs,
                     "map": {to_key(r["Instance"]): r for r in rs}})

    base = runs[0]
    keys = [to_key(r["Instance"]) for r in base["rows"]]
    matched_keys = [k for k in keys if k in ref]

    for i, run in enumerate(runs):
        print(f"#{i + 1} = {run['path']}  （{len(run['rows'])} 个算例）")
    print(f"基准表 {len(ref)} 条；匹配到基准的算例: {len(matched_keys)} / {len(keys)}"
          + (f"（{len(keys) - len(matched_keys)} 个无基准值，已跳过）"
             if len(matched_keys) < len(keys) else ""))
    print()

    def state_of(cmax, lb, ub):
        if cmax < lb:
            return "!! 优于下界(需复核)"
        if cmax == lb:
            return "★ 达下界" + ("(已证最优)" if lb == ub else "(超 LB，未闭合)")
        if cmax <= ub:
            return "≤ 最好已知解" + ("(已证最优)" if cmax == ub and lb == ub else "")
        return f"差 {cmax - ub}"

    # ---------------- 明细：每个 run 两列(Cmax / gap%) ----------------
    head = f"{'算例':<16}{'族':<14}{'LB':>8}{'UB':>8}"
    for i in range(len(runs)):
        head += f"{'#' + str(i + 1) + ' Cmax':>11}{'gap%':>8}"
    head += "  状态(最后一个 run)" if len(runs) == 1 else ""
    print(head)
    print("-" * min(len(head), 200))

    # stats[fam][i] -> 第 i 个 run 在该族的统计
    stats = defaultdict(lambda: [{"n": 0, "hit": 0, "gap": [], "gapu": [],
                                  "beat": 0, "t": 0.0} for _ in runs])
    report_rows = []

    for key in matched_keys:
        lb, ub = ref[key]
        fam = family_of(key)
        line = f"{key:<16}{fam:<14}{lb:>8}{ub:>8}"
        row = {"Instance": key, "Family": fam, "LB": lb, "UB": ub,
               "Initial_Cmax": base["map"][key]["_init"]}
        best_cmax, last_state = None, ""
        for i, run in enumerate(runs):
            r = run["map"].get(key)
            if r is None:
                line += f"{'-':>11}{'-':>8}"
                row[f"Run{i + 1}_Cmax"] = ""
                row[f"Run{i + 1}_Gap(%)"] = ""
                continue
            cmax = r["_cmax"]
            gap = 100.0 * (cmax - lb) / lb
            line += f"{cmax:>11}{gap:>8.2f}"
            row[f"Run{i + 1}_Cmax"] = cmax
            row[f"Run{i + 1}_Gap(%)"] = round(gap, 3)
            s = stats[fam][i]
            s["n"] += 1
            s["gap"].append(gap)
            s["gapu"].append(100.0 * (cmax - ub) / ub)
            s["t"] += float(r.get("Time(s)", 0))
            if cmax <= lb:
                s["hit"] += 1
            if cmax < ub:
                s["beat"] += 1
            last_state = state_of(cmax, lb, ub)
            if best_cmax is None or cmax < best_cmax:
                best_cmax = cmax
        row["Best_Cmax"] = best_cmax
        row["State"] = last_state.lstrip("* ").strip()
        report_rows.append(row)
        print(line + (f"  {last_state}" if len(runs) == 1 else ""))

    # ---------------- 汇总：每个 run 一张表 ----------------
    summaries = []
    for i, run in enumerate(runs):
        print()
        print(f"--- 汇总 #{i + 1}：{run['label']} ---")
        print(f"{'族':<14}{'数量':>5}{'达下界':>8}{'占比':>8}{'优于UB':>8}"
              f"{'gap_LB均值%':>12}{'gap_LB最大%':>12}{'gap_UB均值%':>13}{'总耗时s':>10}")
        print("-" * 96)
        fams = sorted(stats)
        allgap, allgapu, alln, allhit, allbeat, allt = [], [], 0, 0, 0, 0.0
        for fam in fams:
            s = stats[fam][i]
            if s["n"] == 0:
                continue
            alln += s["n"]; allhit += s["hit"]; allbeat += s["beat"]
            allgap += s["gap"]; allgapu += s["gapu"]; allt += s["t"]
            print(f"{fam:<14}{s['n']:>5}{s['hit']:>8}{100.0 * s['hit'] / s['n']:>7.1f}%"
                  f"{s['beat']:>8}{sum(s['gap']) / s['n']:>12.2f}"
                  f"{max(s['gap']):>12.2f}{sum(s['gapu']) / s['n']:>13.2f}{s['t']:>10.1f}")
        print("-" * 96)
        print(f"{'合计':<14}{alln:>5}{allhit:>8}{100.0 * allhit / alln:>7.1f}%"
              f"{allbeat:>8}{sum(allgap) / alln:>12.2f}{max(allgap):>12.2f}"
              f"{sum(allgapu) / alln:>13.2f}{allt:>10.1f}")
        # gap_UB 相对已知最好解，对 LB<UB 的未闭合算例是更公平的评价口径
        summaries.append((run["label"], sum(allgap) / alln,
                          sum(allgapu) / alln, allhit, alln))

    # ---------------- 多 run 总览对比 ----------------
    if len(summaries) > 1:
        print()
        print("=== 多次运行总览 (达下界数 / gap_LB / gap_UB) ===")
        for name, gap, gapu, hit, n in summaries:
            print(f"  {name:<40} 达下界 {hit:>3}/{n}"
                  f"   gap_LB={gap:>6.2f}%   gap_UB={gapu:>+6.2f}%")
        best = min(summaries, key=lambda x: x[1])
        worst = max(summaries, key=lambda x: x[1])
        print(f"  最优: {best[0]}  （相对最差 {worst[0]} 降低 "
              f"{worst[1] - best[1]:.2f} 个百分点，"
              f"相对降幅 {100.0 * (worst[1] - best[1]) / worst[1]:.1f}%）")
        print("  注: gap_LB 相对下界，LB<UB 的算例即使做到已知最好解也仍为正；")
        print("      gap_UB 相对目前已知最好解，是更贴近实际水平的口径。")

    # ---------------- 写出报告 ----------------
    out_path = os.path.join(os.path.dirname(os.path.abspath(paths[0])),
                            "benchmark_report.csv")
    if report_rows:
        with open(out_path, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=list(report_rows[0].keys()))
            w.writeheader()
            w.writerows(report_rows)
        print(f"\n明细已写入: {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
