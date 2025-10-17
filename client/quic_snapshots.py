import argparse, asyncio, time, struct
from aioquic.asyncio import connect
from aioquic.quic.configuration import QuicConfiguration

def build_snapshot(mods: int, pressed_bits: bytes, version=1, seq=0, t_ms=None) -> bytes:
    # Minimal toy format for now (replace with protobuf in real build)
    if t_ms is None:
        t_ms = int(time.monotonic() * 1000)
    hdr = struct.pack("!IIQ", version, seq, t_ms)
    body = struct.pack("!I", mods) + pressed_bits
    return hdr + body

async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=4445)
    ap.add_argument("--hz", type=int, default=120)
    args = ap.parse_args()

    cfg = QuicConfiguration(is_client=True, alpn_protocols=["hidra-snp"])
    async with connect(args.host, args.port, configuration=cfg) as client:
        stream_id = client._quic.get_next_available_stream_id(is_unidirectional=False)
        writer = client.create_stream(stream_id)
        interval = 1.0/args.hz
        seq = 0
        while True:
            # Example: mods=0, pressed_bits all zero (no keys pressed)
            snap = build_snapshot(0, b"\x00"*32, seq=seq)
            writer.write(snap)
            await writer.drain()
            await asyncio.sleep(interval)
            seq += 1

if __name__ == "__main__":
    asyncio.run(main())
