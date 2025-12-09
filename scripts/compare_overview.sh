#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   scripts/compare_overview.sh <host> <user> <pass> <logdir> <binary> [args...]
# Example:
#   scripts/compare_overview.sh staging-rabbit-haproxy-vip-01.ar2.tastytrade.systems guest guest ./logs \
#     ./cmake-out/macos-arm64-vcpkg-llvm-26/examples/order_status_monitor_cli/rmq_order_status_monitor_cli \
#     --config ./examples/order_status_monitor_cli/config.local.json
#
# The script:
#   1) curls /api/overview to <logdir>/overview-before.json
#   2) runs the CLI with provided args
#   3) curls /api/overview to <logdir>/overview-after.json
#   4) diffs the two overviews (jq-normalized) into <logdir>/overview-diff.txt

if [ "$#" -lt 5 ]; then
  echo "Usage: $0 <host> <user> <pass> <logdir> <binary> [args...]" >&2
  exit 1
fi

host="$1"; user="$2"; pass="$3"; logdir="$4"; shift 4
binary="$1"; shift
args=("$@")

mkdir -p "$logdir"
ts() { date +"%Y%m%d.%H%M%S"; }

before="$logdir/overview-before-$(ts).json"
after="$logdir/overview-after-$(ts).json"
diffout="$logdir/overview-diff-$(ts).txt"

curl -sS -u "$user:$pass" -H 'Accept: application/json' "http://$host:15672/api/overview" -o "$before"

timeout_cmd=""
if command -v gtimeout >/dev/null 2>&1; then
  timeout_cmd="gtimeout"
elif command -v timeout >/dev/null 2>&1; then
  timeout_cmd="timeout"
fi
run_seconds="${TIMEOUT_SECS:-15}"

if [ -n "$timeout_cmd" ]; then
  "$timeout_cmd" "$run_seconds" "$binary" "${args[@]}" || true
else
  # Fallback: background and kill after run_seconds
  "$binary" "${args[@]}" &
  pid=$!
  sleep "$run_seconds"
  kill "$pid" 2>/dev/null || true
fi

curl -sS -u "$user:$pass" -H 'Accept: application/json' "http://$host:15672/api/overview" -o "$after"

if command -v jq >/dev/null 2>&1; then
  jq -S . "$before" > "$before.sorted" || true
  jq -S . "$after"  > "$after.sorted"  || true
  diff -u "$before.sorted" "$after.sorted" > "$diffout" || true
else
  diff -u "$before" "$after" > "$diffout" || true
fi

echo "Wrote:"
echo "  $before"
echo "  $after"
echo "Diff (sorted JSON if jq present): $diffout"
