#!/usr/bin/env python3
"""
Fetch /api/overview via HTTP basic auth and diff it against the most recent
http_overview-*.json emitted by rmq_order_status_monitor_cli.

Intended to be run via the custom CMake target `order_status_monitor_cli_diff`.
The target passes the build logs directory; credentials come from the config
JSON (or can be overridden via CLI flags).
"""

from __future__ import annotations

import argparse
import base64
import datetime as dt
import glob
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Dict, Optional
import difflib


def ts_stamp() -> str:
    now = dt.datetime.now(dt.timezone.utc)
    return now.strftime("%Y%m%d.%H%M%S.") + f"{now.microsecond * 1000:09d}"


def load_config(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as fp:
        return json.load(fp)


def pick(value: Optional[Any], fallback: Optional[Any]) -> Optional[Any]:
    return value if value not in (None, "") else fallback


def fetch_overview(url: str, user: str, password: str) -> Dict[str, Any]:
    req = urllib.request.Request(url)
    auth = base64.b64encode(f"{user}:{password}".encode("utf-8")).decode("ascii")
    req.add_header("Authorization", f"Basic {auth}")
    req.add_header("Accept", "application/json")
    with urllib.request.urlopen(req, timeout=30) as resp:
        charset = resp.headers.get_content_charset() or "utf-8"
        data = resp.read().decode(charset)
    return json.loads(data)


def find_latest_cli_overview(logs_dir: Path) -> Optional[Path]:
    candidates = sorted(logs_dir.glob("http_overview-*.json"))
    return candidates[-1] if candidates else None


def write_json(path: Path, payload: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fp:
        json.dump(payload, fp, indent=2, sort_keys=True)


def diff_json(a: Dict[str, Any], b: Dict[str, Any]) -> str:
    a_str = json.dumps(a, indent=2, sort_keys=True)
    b_str = json.dumps(b, indent=2, sort_keys=True)
    lines = list(
        difflib.unified_diff(
            a_str.splitlines(),
            b_str.splitlines(),
            fromfile="cli_overview",
            tofile="curl_overview",
        )
    )
    return "\n".join(lines)


def main() -> int:
    default_config = Path(__file__).resolve().parent.parent / "config.local.json"

    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, default=default_config)
    parser.add_argument("--logs-dir", type=Path, default=None)
    parser.add_argument("--host", dest="host", default=None)
    parser.add_argument("--port", dest="port", type=int, default=None)
    parser.add_argument("--user", dest="user", default=None)
    parser.add_argument("--password", dest="password", default=None)
    parser.add_argument(
        "--scheme", default="http", help="http or https (default: http)"
    )
    args = parser.parse_args()

    if not args.config.exists():
        print(f"[diff] config {args.config} missing; skipping", file=sys.stderr)
        return 0

    cfg = load_config(args.config)

    host = pick(args.host, cfg.get("host"))
    port = pick(args.port, cfg.get("http_admin_port", 15672))
    user = pick(args.user, cfg.get("http_admin_user"))
    password = pick(args.password, cfg.get("http_admin_password"))
    enable_http = cfg.get("enable_http_admin", False)

    logs_dir = args.logs_dir or Path.cwd() / "logs"

    if not enable_http and args.host is None:
        print("[diff] enable_http_admin=false and no host override; skipping", file=sys.stderr)
        return 0
    if not host or not user or password is None:
        print("[diff] missing host/user/password; skipping", file=sys.stderr)
        return 0

    latest_cli = find_latest_cli_overview(logs_dir)
    if not latest_cli:
        print(f"[diff] no cli http_overview-*.json found in {logs_dir}", file=sys.stderr)
        return 0

    url = f"{args.scheme}://{host}:{port}/api/overview"
    stamp = ts_stamp()
    curl_path = logs_dir / f"curl_overview-{stamp}.json"
    diff_path = logs_dir / f"overview_diff-{stamp}.txt"

    try:
        live = fetch_overview(url, str(user), str(password))
    except urllib.error.HTTPError as exc:
        print(f"[diff] HTTP error {exc.code}: {exc.reason}", file=sys.stderr)
        return 1
    except Exception as exc:  # noqa: BLE001
        print(f"[diff] failed to fetch overview: {exc}", file=sys.stderr)
        return 1

    write_json(curl_path, live)

    with latest_cli.open("r", encoding="utf-8") as fp:
        cli_json = json.load(fp)

    diff_text = diff_json(cli_json, live)
    diff_path.parent.mkdir(parents=True, exist_ok=True)
    with diff_path.open("w", encoding="utf-8") as fp:
        fp.write(diff_text + "\n")

    print(f"[diff] cli overview: {latest_cli}")
    print(f"[diff] curl overview: {curl_path}")
    print(f"[diff] wrote unified diff: {diff_path} (length {len(diff_text.splitlines())} lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
