import argparse, asyncio, random, socket

async def send_udp(host, port, events, loss=0.0, jitter_ms=0):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    for (delay_ms, cmd, key) in events:
        await asyncio.sleep(delay_ms/1000.0)
        if loss > 0 and random.random() < loss:
            continue
        if jitter_ms:
            await asyncio.sleep(jitter_ms/1000.0)
        msg = f"{cmd} {key}".encode()
        sock.sendto(msg, (host, port))

def script_from_text(txt: str):
    events = []
    for ch in txt:
        key = "ENTER" if ch == "\n" else ch
        events.append((10, "down", key))
        events.append((20, "up", key))
    return events

async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", required=True)
    ap.add_argument("--udp-port", type=int, default=4444)
    ap.add_argument("--loss", type=float, default=0.0)
    ap.add_argument("--jitter", type=int, default=0)
    ap.add_argument("--script", default="apt ENTER")
    args = ap.parse_args()

    events = script_from_text(args.script)
    await send_udp(args.target, args.udp_port, events, loss=args.loss, jitter_ms=args.jitter)

if __name__ == "__main__":
    asyncio.run(main())
