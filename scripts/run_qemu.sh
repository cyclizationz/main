#!/usr/bin/env bash
set -euo pipefail

QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64}"
IMG="${IMG:-$HOME/vm/guest.img}"
# Use a user-writable path for the Unix socket (fallback to /tmp)
HID_SOCK="${HID_SOCK:-${XDG_RUNTIME_DIR:-/tmp}/hidra.kbd}"

# QEMU will LISTEN on this socket; the daemon will CONNECT.
rm -f "$HID_SOCK"

exec "$QEMU_BIN"   -m 4096 -enable-kvm -cpu host   -drive file="$IMG",if=virtio   -chardev socket,id=hidkbd,path="$HID_SOCK",server=on,wait=off   -device usb-kbd,chardev=hidkbd   -netdev user,id=n0 -device virtio-net-pci,netdev=n0
