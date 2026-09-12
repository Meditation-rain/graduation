#!/usr/bin/env python3
"""汇总 SDST 求解结果：可行性(交叉校验) + Cmax + 相对非SDST基线的增量。

依赖:
    - tools/verify_schedule.py 的 verify()（独立重解码校验可行性并回算 Cmax）
    - runs/fulleval/nonsdst/tabu_search_results.csv  （非SDST 基线 Cmax）
    - runs/fulleval/sdst/tabu_search_results.csv      （SDST 求解器输出）
    - instance_SDST/ 与 output_SDST/

产出:
    - sdst_report.csv  （每算例一行，含可行性、Cmax、相对非SDST增量、耗时）
    - 控制台打印整体汇总（可行数、平均 Cmax 增量）
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import verify_schedule as vs  # noqa: E402

SKIP = {"simple.txt", "test_sdst.txt"}


def load_cmax(path):
    d = {}
    with open(path, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            try:
                d[r["Instance"]] = int(r["Optimized_Cmax"])
            except (KeyError, ValueError):
                pass
    return d


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--inst-dir", default=os.path.join(ROOT, "instance_SDST"))
    ap.add_argument("--out-dir", default=os.path.join(ROOT, "output_SDST"))
    ap.add_argument("--nonsdst-csv", default=os.path.join(ROOT, "runs", "fulleval", "nonsdst", "tabu_search_results.csv"))
    ap.add_argument("--sdst-csv", default=os.path.join(ROOT, "runs", "fulleval", "sdst", "tabu_search_results.csv"))
    ap.add_argument("--report", default=os.path.join(ROOT, "sdst_report.csv"))
    ap.add_argument("--config-note", default="300k iters / top-K=8 / single critical path (Shen2018 tabu)")
    args = ap.parse_args()

    INST_SBST = args.inst_dir
    OUT_SBST = args.out_dir
    NONSDST_CSV = args.nonsdst_csv
    SDST_CSV = args.sdst_csv
    OUT_REPORT = args.report
    CONFIG_NOTE = args.config_note

    base = load_cmax(NONSDST_CSV)
    sdst_rows = {}
    with open(SDST_CSV, encoding="utf-8") as f:
        for r in csv.DictReader(f):
            sdst_rows[r["Instance"]] = r

    files = sorted(f for f in os.listdir(INST_SBST) if f.endswith(".txt") and f not in SKIP)
    report = []
    for f in files:
        stem, cmax, errors = vs.verify(os.path.join(INST_SBST, f), OUT_SBST)
        sr = sdst_rows.get(f)
        solver_cmax = int(sr["Optimized_Cmax"]) if sr else None
        feasible = len(errors) == 0
        base_cmax = base.get(f)
        if cmax is not None and base_cmax:
            overhead = 100.0 * (cmax - base_cmax) / base_cmax
        else:
            overhead = None
        try:
            improvement = float(sr["Improvement(%)"]) if sr else None
        except (TypeError, ValueError):
            improvement = None
        try:
            t = float(sr["Time(s)"]) if sr else None
        except (TypeError, ValueError):
            t = None
        report.append({
            "Instance": f,
            "Config": CONFIG_NOTE,
            "Solver_Cmax": solver_cmax if solver_cmax is not None else "",
            "Verified_Cmax": cmax if cmax is not None else "",
            "Feasible": "OK" if feasible else "X",
            "Overhead_vs_nonSDST(%)": round(overhead, 2) if overhead is not None else "",
            "Improvement(%)": round(improvement, 2) if improvement is not None else "",
            "Time(s)": round(t, 2) if t is not None else "",
            "Errors": "; ".join(errors[:3]) if errors else "",
        })

    with open(OUT_REPORT, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(report[0].keys()))
        w.writeheader()
        w.writerows(report)

    n = len(report)
    feas = sum(1 for r in report if r["Feasible"] == "OK")
    oh = [r["Overhead_vs_nonSDST(%)"] for r in report if r["Overhead_vs_nonSDST(%)"] != ""]
    mism = [r["Instance"] for r in report
            if r["Solver_Cmax"] != "" and r["Verified_Cmax"] != ""
            and r["Solver_Cmax"] != r["Verified_Cmax"]]
    print(f"[SDST] 算例: {n}, 可行: {feas}, 不可行: {n - feas}")
    if oh:
        print(f"[SDST] 平均 Cmax 增量(vs 非SDST): {sum(oh) / len(oh):.2f}%")
    if mism:
        print(f"[SDST] 警告: {len(mism)} 个算例 Solver_Cmax 与 Verified_Cmax 不一致: {mism[:5]}")
    else:
        print("[SDST] Solver_Cmax 与 Verified_Cmax 全部一致 ✓")
    print(f"[SDST] 已写出: {OUT_REPORT}")


if __name__ == "__main__":
    main()
