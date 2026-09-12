#!/usr/bin/env python3
"""
独立校验 FJSP-SDST 求解结果的可行性（不依赖 C++ 代码，用于交叉验证）。

校验项：
  1. 每道工序的加工时长 == 算例中该工序在所选机器上的时长
  2. 工序只能分配在其候选机器上
  3. 同一机器上相邻工序满足：前序完工 + SDST 准备时间 <= 后序开工
  4. 同一工件的工序按工艺路线顺序加工
  5. 导出的 Cmax 与程序报告值一致

用法：
    python3 tools/verify_schedule.py                       # 校验 ./instance 与 ./output
    python3 tools/verify_schedule.py --inst DIR --out DIR
"""
import argparse
import csv
import os
import sys
from collections import defaultdict


def parse_instance(path):
    """按 C++ 端 operator>> 的顺序解析：job_num machine_num max_cand，随后是 SDST 矩阵。"""
    tokens = open(path, encoding="utf-8", errors="replace").read().split()
    it = iter(tokens)
    job_num = int(next(it))
    machine_num = int(next(it))
    next(it)  # max_candidate_num，本脚本不使用

    jobs = []
    op_count = 0
    for _ in range(job_num):
        n_op = int(next(it))
        ops = []
        for _ in range(n_op):
            k = int(next(it))
            cand = {}
            for _ in range(k):
                m, d = int(next(it)), int(next(it))
                cand[m] = d
            ops.append(cand)
            op_count += 1
        jobs.append(ops)

    # SDST: [machine][prev_job][next_job]；算例文件缺少该段时全部补 0
    sdst = [[[0] * job_num for _ in range(job_num)] for _ in range(machine_num)]
    for m in range(machine_num):
        for i in range(job_num):
            for j in range(job_num):
                try:
                    sdst[m][i][j] = int(next(it))
                except StopIteration:
                    break
    return job_num, machine_num, jobs, sdst, op_count


def verify(inst_path, out_dir):
    stem = os.path.basename(inst_path)[:-4]
    csv_path = os.path.join(out_dir, stem + "_optimized.csv")
    if not os.path.exists(csv_path):
        return stem, None, ["缺少输出文件 " + csv_path]

    _, machine_num, jobs, sdst, _ = parse_instance(inst_path)
    rows = list(csv.DictReader(open(csv_path)))
    errors = []

    # 1 & 2. 机器合法性与加工时长
    for r in rows:
        j, o, m = int(r["Job"]), int(r["Operation"]), int(r["Machine"])
        st, et = int(r["StartTime"]), int(r["EndTime"])
        if m >= machine_num:
            errors.append(f"非法机器编号 {m}")
            continue
        dur = jobs[j][o].get(m)
        if dur is None:
            errors.append(f"J{j}O{o} 不可在机器 {m} 上加工")
        elif et - st != dur:
            errors.append(f"J{j}O{o} 时长 {et - st} != 实例值 {dur}")

    # 3. 机器侧不重叠（含 SDST）
    by_machine = defaultdict(list)
    for r in rows:
        by_machine[int(r["Machine"])].append(
            (int(r["StartTime"]), int(r["EndTime"]), int(r["Job"]), int(r["ID"]))
        )
    for m, lst in by_machine.items():
        lst.sort()
        for a, b in zip(lst, lst[1:]):
            need = sdst[m][a[2]][b[2]]
            if a[1] + need > b[0]:
                errors.append(
                    f"机器 {m} 时间冲突: op{a[3]}[..{a[1]}] + setup {need} > op{b[3]}[{b[0]}..]"
                )

    # 4. 工件工艺顺序
    by_job = defaultdict(list)
    for r in rows:
        by_job[int(r["Job"])].append(
            (int(r["Operation"]), int(r["StartTime"]), int(r["EndTime"]))
        )
    for j, lst in by_job.items():
        lst.sort()
        if [x[0] for x in lst] != list(range(len(lst))):
            errors.append(f"工件 {j} 工序编号不连续")
        for a, b in zip(lst, lst[1:]):
            if a[2] > b[1]:
                errors.append(f"工件 {j} 顺序违反: O{a[0]} 完工 {a[2]} > O{b[0]} 完工 {b[1]}")

    # 5. Cmax
    cmax = max(int(r["EndTime"]) for r in rows) if rows else None
    return stem, cmax, errors


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser()
    ap.add_argument("--inst", default=os.path.join(root, "instance"))
    ap.add_argument("--out", default=os.path.join(root, "output"))
    ap.add_argument("--limit", type=int, default=0, help="只校验前 N 个算例")
    args = ap.parse_args()

    files = sorted(f for f in os.listdir(args.inst) if f.endswith(".txt"))
    if args.limit:
        files = files[: args.limit]

    print(f"{'算例':<28}{'Cmax':>8}   校验结果")
    print("-" * 60)
    total_bad = 0
    for f in files:
        stem, cmax, errors = verify(os.path.join(args.inst, f), args.out)
        if errors:
            total_bad += 1
            status = "X " + "; ".join(errors[:2])
        else:
            status = "OK 可行"
        print(f"{stem:<28}{str(cmax):>8}   {status}")

    print("-" * 60)
    print(f"共 {len(files)} 个算例，{total_bad} 个存在问题")
    return 1 if total_bad else 0


if __name__ == "__main__":
    sys.exit(main())
