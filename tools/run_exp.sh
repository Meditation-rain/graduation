#!/bin/bash
#
# 运行一次评测实验，并把结果隔离到独立目录。
#
# 为什么需要它：main.cpp 把 tabu_search_results.csv 写在「当前工作目录」
# （相对路径，见 main.cpp:109），而不是 --out 目录。因此多次实验若在同一目录下跑，
# 结果会互相覆盖。本脚本为每次实验单独建一个工作目录并 cd 进去，
# 使各自的 CSV 天然隔离。
#
# 用法:
#   tools/run_exp.sh <二进制> <算例目录> <输出目录> <标签> [传给求解器的额外参数...]
#
# 产物:
#   runs/<标签>/run.log                      完整 stdout
#   runs/<标签>/tabu_search_results.csv       结果 CSV
#   <输出目录>/                               调度表
#
set -euo pipefail

if [ $# -lt 4 ]; then
    echo "用法: $0 <二进制> <算例目录> <输出目录> <标签> [额外参数...]" >&2
    exit 1
fi

BIN=$1; INST=$2; OUT=$3; TAG=$4; shift 4

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
INST="$(cd "$INST" && pwd)"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

WORK="$ROOT/runs/$TAG"
rm -rf "$WORK"
mkdir -p "$WORK"

echo "[run_exp] 标签=$TAG  二进制=$(basename "$BIN")  算例=$(basename "$INST")"
echo "[run_exp] 工作目录=$WORK"

cd "$WORK"
"$BIN" --inst "$INST" --out "$OUT" "$@" > run.log 2>&1

if [ ! -f tabu_search_results.csv ]; then
    echo "[run_exp] 错误：未生成 tabu_search_results.csv" >&2
    exit 1
fi

echo "[run_exp] 完成 → $WORK/tabu_search_results.csv"
