#!/usr/bin/env python3
"""
为 FJSP 算例追加序列相关准备时间（SDST）矩阵，生成 FJSP-SDST 测试算例。

输出文件在原始 FJSP 数据之后追加 machine_num × job_num × job_num 个整数，
排列顺序与 C++ 端 Instance::operator>> 的读取顺序严格一致：

    外层 m（机器） → 中层 i（前序工件） → 内层 j（后序工件），即 s_{i->j}^{m}

准备时间取值：s ~ U[1, round(gamma × p̄)]，其中 p̄ 为该算例所有候选加工时间的
平均值；同一工件连续加工（i == j）时准备时间为 0。gamma 越大，准备时间相对加工
时间越显著，对调度的干扰也越强。

用法：
    python3 tools/generate_sdst_instances.py                     # 默认 medium，输出到 instance_SDST
    python3 tools/generate_sdst_instances.py --level high
    python3 tools/generate_sdst_instances.py --gamma 0.8 --out instance_SDST_h
"""
import argparse
import os
import random
import sys

LEVELS = {"low": 0.25, "medium": 0.5, "high": 1.0}


def parse_instance(path):
    """返回 (原文, job_num, machine_num, jobs, 加工时间列表, FJSP 段之后的剩余 token 数)。"""
    text = open(path, encoding="utf-8", errors="replace").read()
    tokens = text.split()
    it = iter(tokens)
    job_num = int(next(it))
    machine_num = int(next(it))
    next(it)  # max_candidate_num

    jobs = []
    durations = []
    for _ in range(job_num):
        n_op = int(next(it))
        ops = []
        for _ in range(n_op):
            k = int(next(it))
            cands = []
            for _ in range(k):
                m = int(next(it))
                d = int(next(it))
                cands.append((m, d))
                durations.append(d)
            ops.append(cands)
        jobs.append(ops)
    return text, job_num, machine_num, jobs, durations, len(list(it))


def build_sdst(machine_num, job_num, p_bar, gamma, rng):
    hi = max(1, int(round(gamma * p_bar)))
    matrix = []
    for _ in range(machine_num):
        per_machine = []
        for i in range(job_num):
            row = []
            for j in range(job_num):
                row.append(0 if i == j else rng.randint(1, hi))
            per_machine.append(row)
        matrix.append(per_machine)
    return matrix, hi


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=os.path.join(root, "instance"))
    ap.add_argument("--out", default=os.path.join(root, "instance_SDST"))
    ap.add_argument("--level", choices=sorted(LEVELS), default="medium")
    ap.add_argument("--gamma", type=float, default=None, help="直接指定强度因子，覆盖 --level")
    ap.add_argument("--seed", type=int, default=20240911)
    args = ap.parse_args()

    gamma = args.gamma if args.gamma is not None else LEVELS[args.level]
    rng = random.Random(args.seed)

    os.makedirs(args.out, exist_ok=True)
    files = sorted(f for f in os.listdir(args.src) if f.endswith(".txt"))
    if not files:
        print("源目录中没有 .txt 算例：", args.src)
        return 1

    print(f"强度: level={args.level}  gamma={gamma}  种子={args.seed}")
    print(f"输出目录: {args.out}")
    print("-" * 74)
    print(f"{'算例':<34}{'p̄':>7}{'setup范围':>10}{'setup均值':>10}   备注")

    total_setup = 0
    total_count = 0
    kept = 0
    for name in files:
        text, job_num, machine_num, _, durations, tail = parse_instance(os.path.join(args.src, name))
        p_bar = sum(durations) / len(durations) if durations else 1.0
        expected_sdst = machine_num * job_num * job_num

        # 原算例若已自带 SDST 段，则原样保留，避免追加出第二份矩阵造成歧义
        if tail >= expected_sdst:
            with open(os.path.join(args.out, name), "w", encoding="utf-8") as fh:
                fh.write(text.rstrip("\n") + "\n")
            kept += 1
            print(f"{name[:33]:<34}{p_bar:>7.1f}{'-':>10}{'-':>10}   已含SDST，原样保留")
            continue

        matrix, hi = build_sdst(machine_num, job_num, p_bar, gamma, rng)

        flat = [v for pm in matrix for row in pm for v in row]
        nonzero = [v for v in flat if v > 0]
        total_setup += sum(nonzero)
        total_count += len(nonzero)

        lines = [text.rstrip("\n"), ""]
        for pm in matrix:
            for row in pm:
                lines.append(" ".join(str(v) for v in row))
            lines.append("")
        with open(os.path.join(args.out, name), "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines).rstrip("\n") + "\n")

        avg_s = sum(nonzero) / len(nonzero) if nonzero else 0
        print(f"{name[:33]:<34}{p_bar:>7.1f}{f'1-{hi}':>10}{avg_s:>10.1f}")

    print("-" * 74)
    overall = total_setup / total_count if total_count else 0
    print(f"共处理 {len(files)} 个算例：新生成 {len(files) - kept} 个，"
          f"原样保留 {kept} 个（源文件已含 SDST）")
    print(f"新增准备时间的整体均值 {overall:.1f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
