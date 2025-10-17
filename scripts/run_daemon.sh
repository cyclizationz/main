#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")"/.. && pwd)"
sudo rm -f /run/hid.kbd
"$DIR/daemon/build/daemon" \
  --udp-edges 0.0.0.0:4444 \
  --hid-socket /run/hid.kbd \
  --watchdog-ms 200 --deadline-ms 100
