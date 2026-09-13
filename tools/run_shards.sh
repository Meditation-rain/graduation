#!/bin/bash
#
# 把算例集切成若干分片并行求解，再把结果按原顺序合并。
#
# 为什么能安全地并行：
#   求解器是「按算例串行」的，每个算例的种子由 (全局种子, 算例序号, 重启序号) 决定
#   （main.cpp 的 derive_seed）。因此只要每个算例仍拿到它在**完整算例集**中的全局序号，
#   分片并行跑的结果就与整批串行跑**逐字相同**——并行只是把墙钟缩短，不改变任何结果。
#   全局序号 = 分片内局部序号 + 该分片的起始偏移，通过 --idx-offset 传入。
#
# 用法:
#   tools/run_shards.sh <二进制> <算例目录> <输出目录> <标签> <分片数> [求解器额外参数...]
#
# 产物:
#   runs/<标签>/tabu_search_results.csv   合并后的完整结果（顺序与整批串行一致）
#   runs/<标签>/shard_*/                  各分片的工作目录与日志
#
set -euo pipefail

if [ $# -lt 5 ]; then
    echo "用法: $0 <二进制> <算例目录> <输出目录> <标签> <分片数> [求解器额外参数...]" >&2
    exit 1
fi

BIN=$1; INST=$2; OUT=$3; TAG=$4; NSHARD=$5; shift 5

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
INST="$(cd "$INST" && pwd)"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"

# 枚举算例：与 main.cpp 一致的规则（.txt 普通文件，按路径排序）
FILES=()
while IFS= read -r f; do
    FILES+=("$f")
done < <(cd "$INST" && ls -1 *.txt 2>/dev/null | LC_ALL=C sort)
TOTAL=${#FILES[@]}

if [ "$TOTAL" -eq 0 ]; then
    echo "[run_shards] 错误：在 $INST 下找不到 .txt 算例" >&2
    exit 1
fi

WORK="$ROOT/runs/$TAG"
rm -rf "$WORK"; mkdir -p "$WORK"

PER=$(( (TOTAL + NSHARD - 1) / NSHARD ))

# 注意：变量一律用 ${} 界定。bash 会把紧跟变量名的全角字符（如「（」「：」）
# 一并当成变量名的一部分，导致 unbound variable。
echo "[run_shards] 标签=${TAG}  算例=${TOTAL}  分片=${NSHARD}（每片约 ${PER} 个）  二进制=$(basename "${BIN}")"

s=0
start=0
while [ "$start" -lt "$TOTAL" ]; do
    sd="$WORK/shard_$s"
    mkdir -p "$sd"
    i=$start
    n=0
    while [ "$i" -lt "$TOTAL" ] && [ "$n" -lt "$PER" ]; do
        ln -sf "$INST/${FILES[$i]}" "$sd/${FILES[$i]}"
        i=$((i + 1)); n=$((n + 1))
    done
    # 后台跑一个分片；--idx-offset 把分片内局部序号还原成全局序号
    ( cd "$sd" && "$BIN" --inst "$sd" --out "$OUT" --idx-offset "$start" "$@" > run.log 2>&1 ) &
    echo "[run_shards] 分片 ${s}：序号 ${start}..$((start + n - 1)) 已启动"
    s=$((s + 1)); start=$((start + PER))
done

echo "[run_shards] 等待 $s 个分片完成..."
wait

# 合并：保留第一个分片的表头，其余只取数据行
merged="$WORK/tabu_search_results.csv"
: > "$merged"
sh=0
while [ "$sh" -lt "$s" ]; do
    f="$WORK/shard_$sh/tabu_search_results.csv"
    if [ ! -f "$f" ]; then
        echo "[run_shards] 警告：分片 $sh 没有产出结果" >&2
    elif [ "$sh" -eq 0 ]; then
        cat "$f" >> "$merged"
    else
        tail -n +2 "$f" >> "$merged"
    fi
    sh=$((sh + 1))
done

N=$(($(wc -l < "$merged") - 1))
echo "[run_shards] 完成 → ${merged}（${N} 行数据）"
[ "${N}" -eq "${TOTAL}" ] || echo "[run_shards] 警告：合并行数 ${N} 与算例数 ${TOTAL} 不一致" >&2
