# QuicKeys: Hypervisor-Based HID for Robust, Low-Latency Remote Input (QEMU + msquic)

(Former working title: HiDra)

See also: docs/ for diagrams and report plan.

## Abstract
QuicKeys combines low-latency UDP edge events with reliable QUIC snapshots, reconciled in a C++ host daemon that emits standard USB-HID reports into a QEMU guest. This heals missed releases within one snapshot interval while preserving input feel and avoiding guest hooks—ideal for terminal/text validation, with gaming as future work.
