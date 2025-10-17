#!/usr/bin/env bash
set -euo pipefail
QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64}"
IMG="${IMG:-$HOME/vm/guest.img}"
"$QEMU_BIN" \
  -m 4096 -enable-kvm -cpu host \
  -drive file="$IMG",if=virtio \
  -chardev socket,id=hidkbd,path=/run/hid.kbd,server,nowait \
  -device usb-kbd,chardev=hidkbd \
  -netdev user,id=n0 -device virtio-net-pci,netdev=n0
