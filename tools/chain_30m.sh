#!/bin/bash
# Relay: wait for SDST 10M, then launch 30M targeted boost for gap<=15.
set -e
ROOT=/Users/chenshiyu/Desktop/maser-0911
MERGED="$ROOT/runs/full_10M_sdst/tabu_search_results.csv"

echo "[chain] $(date +%H:%M:%S) waiting for SDST 10M (flag: $MERGED)"
for i in $(seq 1 400); do
    if [ -f "$MERGED" ]; then
        echo "[chain] $(date +%H:%M:%S) SDST done (waited $i min)"
        break
    fi
    sleep 60
    if [ $((i % 5)) -eq 0 ]; then
        echo "[chain] $(date +%H:%M:%S) heartbeat: waiting SDST ($i min)"
    fi
done
[ -f "$MERGED" ] || echo "[chain] WARN: timeout, launching 30M anyway"

cd "$ROOT"
echo "[chain] $(date +%H:%M:%S) launching 30M boost (59 cases, 9 cores)"
tools/run_pool.sh build/MyProject instance out_full_30M_target full_30M_target \
    tools/jobs_gap15.txt 9 --iter 30000000 --time 3000 --restarts 1 --seed 20240911
echo "CHAIN_DONE"
