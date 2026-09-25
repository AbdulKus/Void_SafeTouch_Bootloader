#!/usr/bin/env python3
"""Сборка и загрузка транзакционных образов Void Bootloader."""

import argparse
import binascii
import pathlib
import struct
import sys
import time

VID = 0x1209
PID = 0xB007
PAGE = 128
MAGIC_BOOT = 0x31425256
MAGIC_APP = 0x31504156
CMD_INFO, CMD_BEGIN, CMD_DATA, CMD_END = 0x01, 0x10, 0x11, 0x12
CMD_ABORT, CMD_BOOT, CMD_RECOVERY = 0x13, 0x20, 0x21


def make_header(magic, body, entry, version):
    crc = binascii.crc32(body) & 0xFFFFFFFF
    words = [magic, 1, len(body), crc, entry, version] + [0xFFFFFFFF] * 26
    return struct.pack("<32I", *words)


def read_image(path):
    blob = pathlib.Path(path).read_bytes()
    if len(blob) < PAGE:
        raise ValueError("образ короче 128-байтного заголовка")
    fields = struct.unpack("<6I", blob[:24])
    magic, fmt, length, crc, entry, version = fields
    body = blob[PAGE:]
    if magic not in (MAGIC_BOOT, MAGIC_APP) or fmt != 1:
        raise ValueError("файл не является образом Void")
    if length != len(body):
        raise ValueError("длина в заголовке не совпадает с размером файла")
    if (binascii.crc32(body) & 0xFFFFFFFF) != crc:
        raise ValueError("неверная CRC32 образа")
    return magic, body, crc, entry, version


def cmd_pack(args):
    body = pathlib.Path(args.input).read_bytes()
    magic = MAGIC_BOOT if args.target == "boot" else MAGIC_APP
    header = make_header(magic, body, args.entry, args.version)
    pathlib.Path(args.output).write_bytes(header + body)
    print(f"{args.output}: тело {len(body)} байт, CRC32={binascii.crc32(body) & 0xffffffff:08x}")


class Device:
    def __init__(self):
        try:
            import hid
        except ImportError as exc:
            raise SystemExit("Установите зависимость: py -m pip install hidapi") from exc
        self.dev = hid.device()
        self.dev.open(VID, PID)
        self.dev.set_nonblocking(False)

    def command(self, command, payload=b"", timeout=5000):
        report = bytearray(64)
        report[0] = command
        report[1:1 + len(payload)] = payload
        if self.dev.write(bytes([0]) + report) < 0:
            raise IOError("ошибка записи HID")
        reply = bytes(self.dev.read(64, timeout))
        if len(reply) != 64 or reply[0] != command:
            raise IOError("ответ отсутствует или не соответствует команде")
        return reply


def wait_device(seconds=10):
    deadline = time.time() + seconds
    last = None
    while time.time() < deadline:
        try:
            return Device()
        except OSError as exc:
            last = exc
            time.sleep(0.25)
    raise OSError(f"Void Bootloader не появился в системе: {last}")


def require_ok(reply, operation):
    if reply[1] != 0:
        raise IOError(f"операция {operation} завершилась кодом {reply[1]}")


def cmd_info(_args):
    dev = wait_device()
    reply = dev.command(CMD_INFO)
    if reply[1:5] != b"VOID":
        raise IOError("неожиданный ответ INFO")
    print(f"Void Bootloader: протокол {reply[5]}.{reply[6]}, stage {reply[7]}, запись={reply[9]}")


def cmd_upload(args):
    magic, body, crc, _entry, version = read_image(args.image)
    target = 1 if magic == MAGIC_BOOT else 2
    dev = wait_device()
    info = dev.command(CMD_INFO)
    stage = info[7]
    if target == 1 and stage == 1:
        require_ok(dev.command(CMD_RECOVERY), "переход в recovery")
        time.sleep(0.05)
        stage = dev.command(CMD_INFO)[7]
    if (target == 1 and stage != 0) or (target == 2 and stage != 1):
        raise IOError(f"stage {stage} не может записывать цель {target}")

    begin = bytes([target, 0, 0]) + struct.pack("<III", len(body), crc, version)
    require_ok(dev.command(CMD_BEGIN, begin), "начало обновления")
    try:
        offset = 0
        while offset < len(body):
            chunk = body[offset:offset + 56]
            payload = bytes([len(chunk), 0, 0]) + struct.pack("<I", offset) + chunk
            reply = dev.command(CMD_DATA, payload)
            require_ok(reply, f"запись данных по смещению {offset}")
            accepted = struct.unpack_from("<I", reply, 4)[0]
            if accepted != offset + len(chunk):
                raise IOError(f"принято {accepted} байт вместо {offset + len(chunk)}")
            offset = accepted
            print(f"\r{offset}/{len(body)} bytes", end="", flush=True)
        require_ok(dev.command(CMD_END), "проверка и фиксация")
    except BaseException:
        try:
            dev.command(CMD_ABORT)
        except Exception:
            pass
        raise
    print("\nОбновление проверено и зафиксировано.")
    if args.boot:
        require_ok(dev.command(CMD_BOOT), "запуск образа")


def cmd_action(args):
    dev = wait_device()
    command = CMD_BOOT if args.action == "boot" else CMD_RECOVERY
    require_ok(dev.command(command), args.action)


def integer(text):
    return int(text, 0)


def main():
    parser = argparse.ArgumentParser(prog="voidtool")
    sub = parser.add_subparsers(required=True)
    pack = sub.add_parser("pack")
    pack.add_argument("--target", choices=("boot", "app"), required=True)
    pack.add_argument("--input", required=True)
    pack.add_argument("--output", required=True)
    pack.add_argument("--entry", type=integer, required=True)
    pack.add_argument("--version", type=integer, required=True)
    pack.set_defaults(func=cmd_pack)
    info = sub.add_parser("info")
    info.set_defaults(func=cmd_info)
    upload = sub.add_parser("upload")
    upload.add_argument("image")
    upload.add_argument("--boot", action="store_true")
    upload.set_defaults(func=cmd_upload)
    action = sub.add_parser("action")
    action.add_argument("action", choices=("boot", "recovery"))
    action.set_defaults(func=cmd_action)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as exc:
        print(f"ошибка: {exc}", file=sys.stderr)
        raise SystemExit(1)
