"""Dev-time Modbus slave simulator for ModbusViewer (see PROGRESS.md prerequisites).

Not part of the application build - a standalone fixture for exercising TCP/RTU
reads and writes against real Modbus traffic without hardware.

Examples:
    python tools/pymodbus_simulator.py --mode tcp
    python tools/pymodbus_simulator.py --mode tcp --port 5020
    python tools/pymodbus_simulator.py --mode rtu --com-port COM10 --baudrate 115200
"""

import argparse
import threading
import time

from pymodbus.datastore import (
    ModbusSequentialDataBlock,
    ModbusServerContext,
    ModbusSlaveContext,
)
from pymodbus.server import StartSerialServer, StartTcpServer

REGISTER_COUNT = 100


def build_context() -> ModbusServerContext:
    def block() -> ModbusSequentialDataBlock:
        return ModbusSequentialDataBlock(0, [0] * REGISTER_COUNT)

    slave = ModbusSlaveContext(di=block(), co=block(), hr=block(), ir=block(), zero_mode=True)
    return ModbusServerContext(slaves=slave, single=True)


def increment_holding_register(context: ModbusServerContext, address: int, interval_seconds: float) -> None:
    """Makes one register change on its own, so a live poll loop has something to observe."""
    slave = context[0]
    while True:
        time.sleep(interval_seconds)
        current = slave.getValues(3, address, count=1)[0]
        next_value = (current + 1) % 0xFFFF
        slave.setValues(3, address, [next_value])
        print(f"holding register {address} -> {next_value}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=["tcp", "rtu"], default="tcp")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=502, help="502 may need elevation; try 5020 if it fails")
    parser.add_argument("--com-port", default="COM10")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--live-register", type=int, default=0, help="holding register address to auto-increment; -1 to disable")
    parser.add_argument("--live-interval", type=float, default=2.0)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    context = build_context()

    if args.live_register >= 0:
        thread = threading.Thread(
            target=increment_holding_register,
            args=(context, args.live_register, args.live_interval),
            daemon=True,
        )
        thread.start()

    if args.mode == "tcp":
        print(f"Modbus TCP simulator listening on {args.host}:{args.port}")
        StartTcpServer(context=context, address=(args.host, args.port))
    else:
        print(f"Modbus RTU simulator listening on {args.com_port} @ {args.baudrate} baud")
        StartSerialServer(
            context=context,
            port=args.com_port,
            baudrate=args.baudrate,
            bytesize=8,
            parity="E",
            stopbits=1,
        )


if __name__ == "__main__":
    main()
