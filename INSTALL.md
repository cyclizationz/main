# INSTALL.md — QuicKeys Setup (Ubuntu 22.04+)

## System Dependencies
```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config git \
  libssl-dev libprotobuf-dev protobuf-compiler libboost-all-dev \
  python3-pip libglib2.0-dev libpixman-1-dev libusb-1.0-0-dev \
  libcap-ng-dev libepoxy-dev zlib1g-dev
```

## Build msquic (QUIC with TLS 1.3 via OpenSSL)
```bash
git clone https://github.com/microsoft/msquic.git
cd msquic
git submodule update --init --recursive
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQUIC_TLS=openssl -DQUIC_BUILD_SHARED=OFF
cmake --build build
sudo cmake --install build
```

## Generate protobuf stubs
```bash
cd HIDra/daemon/proto
protoc --cpp_out=../include --python_out=../../client remoter.proto
```

## Build daemon
```bash
cd HIDra/daemon
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ln -sf build/daemon ../bin_daemon   # convenience link
```

## Build QEMU with USB-HID socket backend
```bash
git clone https://gitlab.com/qemu-project/qemu.git
cd qemu
git checkout v8.2.0  # example; adjust as needed
git apply HIDra/qemu/patches/0001-usb-hid-socket-backend.patch
mkdir build && cd build
../configure --target-list=x86_64-softmmu --enable-kvm --enable-usb-redir
make -j$(nproc)
ln -sf $(pwd)/qemu-system-x86_64 ~/bin/qemu-hidra
```

## Python client (UDP edges + QUIC snapshots)
```bash
cd HIDra/client
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
```

## Run
- Start daemon (creates /run/hid.kbd and emits HID reports):
```bash
HIDra/scripts/run_daemon.sh
```
- Start QEMU:
```bash
HIDra/scripts/run_qemu.sh
```
- Send edges (UDP) and snapshots (QUIC):
```bash
python client.py --target 127.0.0.1 --udp-port 4444 --script "apt ENTER"
python quic_snapshots.py --host 127.0.0.1 --port 4445 --hz 120
```

See `scripts/netem.sh` for loss/jitter emulation.
