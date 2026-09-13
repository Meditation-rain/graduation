#!/bin/bash
#
# 通用并行池：按「逐算例指定全局序号」的方式跑一批**非连续**算例。
#
# 与 run_shards.sh 的区别：
#   run_shards.sh 只做连续切分（整片共用一个 --idx-offset），适合跑全集；
#   本脚本用于「只跑某个子集」的场景（例如只跑未达下界的算例），
#   此时子集在全局目录里不连续，必须给每个算例单独指定 --idx-offset，
#   才能让它拿到与整批串行跑**完全相同**的种子（可复现、可与旧结果逐例对齐）。
#
# 用法:
#   tools/run_pool.sh <二进制> <算例目录> <输出目录> <标签> <任务文件> <并行数> [求解器额外参数...]
#
# 任务文件格式（每行一个算例）:
#   <文件名>|<全局序号>
#
# 产物:
#   runs/<标签>/tabu_search_results.csv   合并结果（按全局序号的原始顺序排列）
#   runs/<标签>/jobs/<文件名>/            各算例的工作目录与日志
#
set -uo pipefail

if [ $# -lt 6 ]; then
    echo "用法: $0 <二进制> <算例目录> <输出目录> <标签> <任务文件> <并行数> [额外参数...]" >&2
    exit 1
fi

BIN=$1; INST=$2; OUT=$3; TAG=$4; JOBS=$5; PAR=$6; shift 6

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
INST="$(cd "$INST" && pwd)"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"

WORK="$ROOT/runs/$TAG"
rm -rf "$WORK"; mkdir -p "$WORK/parts" "$WORK/jobs"

export POOL_BIN="$BIN" POOL_INST="$INST" POOL_OUT="$OUT" POOL_WORK="$WORK" POOL_EXTRA="$*"

cat > "$WORK/one.sh" <<'EOS'
#!/bin/bash
set -uo pipefail
file="$1"; off="$2"
d="$POOL_WORK/jobs/$file"
rm -rf "$d"; mkdir -p "$d"
ln -sf "$POOL_INST/$file" "$d/$file"
( cd "$d" && "$POOL_BIN" --inst . --out "$POOL_OUT" --idx-offset "$off" $POOL_EXTRA > run.log 2>&1 )
if [ -f "$d/tabu_search_results.csv" ]; then
    cp "$d/tabu_search_results.csv" "$POOL_WORK/parts/$file.csv"
    echo "[ok] $file"
else
    echo "[fail] $file" >&2
fi
EOS
chmod +x "$WORK/one.sh"

TOTAL=$(grep -c . "$JOBS")
echo "[run_pool] 标签=${TAG}  任务=${TOTAL}  并行=${PAR}  二进制=$(basename "$BIN")"

# xargs 按任务文件顺序（调用方已按预估耗时降序排列）依次派发，
# 等价于 LPT 贪心调度，长任务先占坑，分片负载自然均衡。
awk -F'|' 'NF>=2 {print $1, $2}' "$JOBS" | xargs -P "$PAR" -n 2 bash "$WORK/one.sh"

# 合并：按全局序号升序重组，还原整批串行的原始行序
python3 - "$JOBS" "$WORK" <<'PY'
import csv, os, sys
jobs, work = sys.argv[1], sys.argv[2]
order = []
for ln in open(jobs, encoding="utf-8"):
    ln = ln.strip()
    if not ln: continue
    f, off = ln.split("|")[:2]
    order.append((int(off), f))
order.sort()
merged = os.path.join(work, "tabu_search_results.csv")
rows = []; header = None
for _, f in order:
    p = os.path.join(work, "parts", f + ".csv")
    if not os.path.exists(p): continue
    with open(p, encoding="utf-8") as fh:
        rs = list(csv.reader(fh))
    if not rs: continue
    if header is None: header = rs[0]
    rows += rs[1:]
with open(merged, "w", encoding="utf-8", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(header)
    w.writerows(rows)
print(f"[run_pool] 完成 → {merged}（{len(rows)} 行数据）")
if len(rows) != len(order):
    print(f"[run_pool] 警告：合并 {len(rows)} 行 ≠ 任务 {len(order)}", file=sys.stderr)
PY
