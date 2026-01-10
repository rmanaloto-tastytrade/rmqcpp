#!/usr/bin/env python3
"""
Fetch /api/overview before and after running the CLI, then diff the JSON.

Usage:
  scripts/compare_overview.py --host HOST --user USER --pass PASS \
      --logdir LOGDIR --binary ./cmake-out/.../rmq_order_status_monitor_cli \
      [--timeout 60] -- [binary args...]
"""
import argparse
import base64
import datetime
import difflib
import json
import subprocess
import sys
from pathlib import Path
from urllib.request import Request, urlopen


def timestamp() -> str:
    return datetime.datetime.now().strftime("%Y%m%d.%H%M%S")


def fetch_overview(host: str, user: str, password: str, dest: Path) -> dict:
    auth = base64.b64encode(f"{user}:{password}".encode()).decode()
    req = Request(
        f"http://{host}:15672/api/overview",
        headers={"Accept": "application/json", "Authorization": f"Basic {auth}"},
    )
    with urlopen(req, timeout=30) as resp:
        data = resp.read()
    dest.write_bytes(data)
    return json.loads(data)


def write_sorted(obj: dict, dest: Path) -> None:
    dest.write_text(json.dumps(obj, sort_keys=True, indent=2))


def write_diff(a: str, b: str, dest: Path) -> None:
    diff = difflib.unified_diff(
        a.splitlines(), b.splitlines(), fromfile="overview-before", tofile="overview-after", lineterm=""
    )
    dest.write_text("\n".join(diff))


def run_binary(cmd: list[str], timeout: int) -> None:
    try:
        subprocess.run(cmd, check=False, timeout=timeout)
    except subprocess.TimeoutExpired:
        print(f"[warn] binary timeout after {timeout}s; continuing", file=sys.stderr)


def main() -> int:
    ap = argparse.ArgumentParser(description="Compare RabbitMQ /api/overview before/after a CLI run.")
    ap.add_argument("--host", required=True)
    ap.add_argument("--user", required=True)
    ap.add_argument("--pass", dest="password", required=True)
    ap.add_argument("--logdir", required=True)
    ap.add_argument("--binary", required=True)
    ap.add_argument("--timeout", type=int, default=1200, help="Seconds to allow the binary to run (default: 1200 = 20 minutes)")
    ap.add_argument("binary_args", nargs=argparse.REMAINDER, help="Arguments passed to the binary (use '--' before args)")
    args = ap.parse_args()

    logdir = Path(args.logdir)
    logdir.mkdir(parents=True, exist_ok=True)

    ts = timestamp()
    before_path = logdir / f"overview-before-{ts}.json"
    after_path = logdir / f"overview-after-{ts}.json"
    before_sorted = logdir / f"overview-before-{ts}.json.sorted"
    after_sorted = logdir / f"overview-after-{ts}.json.sorted"
    diff_path = logdir / f"overview-diff-{ts}.txt"

    before_obj = fetch_overview(args.host, args.user, args.password, before_path)

    # Drop a leading "--" if present (common when forwarding through argparse).
    forwarded = list(args.binary_args)
    if forwarded and forwarded[0] == "--":
        forwarded = forwarded[1:]

    cmd = [args.binary] + forwarded
    run_binary(cmd, args.timeout)
    after_obj = fetch_overview(args.host, args.user, args.password, after_path)

    write_sorted(before_obj, before_sorted)
    write_sorted(after_obj, after_sorted)
    write_diff(before_sorted.read_text(), after_sorted.read_text(), diff_path)

    print("Wrote:")
    print(f"  {before_path}")
    print(f"  {after_path}")
    print(f"Diff: {diff_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
