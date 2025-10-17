# HIDra

Robust, low-latency remote keyboard input via **USB-HID in QEMU**, using **UDP edges** + **QUIC snapshots (msquic)** and **protobuf**.

## Quick Start
See **INSTALL.md** for setup. TL;DR:
1) Build daemon (C++) with msquic & protobuf-lite.  
2) Build QEMU with the USB-HID socket patch.  
3) Run `scripts/run_qemu.sh` and `scripts/run_daemon.sh`.  
4) Drive input with `client/client.py` (UDP) and `client/quic_snapshots.py` (QUIC).

