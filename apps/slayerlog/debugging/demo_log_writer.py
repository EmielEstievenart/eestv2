from __future__ import annotations

import os
import signal
import sys
import time
from datetime import datetime
from pathlib import Path

INITIAL_LINE_COUNT = 20


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: demo_log_writer.py <output-file>", file=sys.stderr)
        return 1

    output_path = Path(sys.argv[1]).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    pid_path = output_path.with_suffix(output_path.suffix + ".pid")

    running = True

    def stop_handler(signum: int, frame: object) -> None:
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop_handler)
    signal.signal(signal.SIGTERM, stop_handler)

    print("SLAYERLOG_WRITER_STARTING", flush=True)
    pid_path.write_text(str(os.getpid()), encoding="utf-8")

    with output_path.open("w", encoding="utf-8", newline="") as handle:
        handle.write("slayerlog demo writer started\n")
        handle.write(f"monitoring file: {output_path}\n")

        count = 1
        for _ in range(INITIAL_LINE_COUNT):
            line = f"[{datetime.now().isoformat(timespec='seconds')}] demo line {count}"
            handle.write(line + "\n")
            count += 1

        handle.flush()

        print("SLAYERLOG_WRITER_READY", flush=True)

        while running:
            line = f"[{datetime.now().isoformat(timespec='seconds')}] demo line {count}"
            handle.write(line + "\n")
            handle.flush()
            print(line, flush=True)
            count += 1
            time.sleep(1)

    try:
        pid_path.unlink()
    except FileNotFoundError:
        pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
