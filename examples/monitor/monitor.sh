#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <amqp-uri> [--enable-firehose] [--enable-metadata] [--enable-log]" >&2
  exit 1
fi

URI="$1"; shift
BIN_DIR="$(cd "$(dirname "$0")" && pwd)"
EXE="$BIN_DIR/rmqmonitor"
exec "$EXE" "$URI" "$@"
