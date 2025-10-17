# HIDra

Robust, low-latency remote keyboard input via **USB-HID in QEMU**, using **UDP edges** + **QUIC snapshots (msquic)** and **protobuf**.

## File Tree
```
HIDra/
├── README.md
├── INSTALL.md
├── docs/
│   └── proposal.md
├── daemon/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── hid_report.h
│   │   └── msquic_server.h
│   ├── proto/
│   │   └── remoter.proto
│   └── src/
│       ├── main.cpp
│       ├── msquic_server.cpp
│       ├── state_machine.cpp        # (optional later)
│       ├── udp_edges.cpp            # (optional later)
│       └── hid_output.cpp           # (optional later)
├── client/
│   ├── client.py                    # UDP edges (quick sanity)
│   ├── quic_snapshots.py            # QUIC snapshots via aioquic
│   └── requirements.txt
├── qemu/
│   └── patches/
│       └── 0001-usb-hid-socket-backend.patch
└── scripts/
    ├── netem.sh
    ├── gen_proto.sh
    ├── run_daemon.sh
    └── run_qemu.sh
```

## Quick Start
See **INSTALL.md** for setup. TL;DR:
1) Build daemon (C++) with msquic & protobuf-lite.  
2) Build QEMU with the USB-HID socket patch.  
3) Run `scripts/run_qemu.sh` and `scripts/run_daemon.sh`.  
4) Drive input with `client/client.py` (UDP) and `client/quic_snapshots.py` (QUIC).

