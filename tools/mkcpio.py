#!/usr/bin/env python3
import pathlib
import sys


def pad(stream, alignment):
    padding = (-stream.tell()) % alignment
    if padding:
        stream.write(b"\0" * padding)


def write_entry(stream, name, data, mode, inode):
    name_bytes = name.encode("ascii") + b"\0"
    fields = (
        inode,
        mode,
        0,
        0,
        1,
        0,
        len(data),
        0,
        0,
        0,
        0,
        len(name_bytes),
        0,
    )
    header = "070701" + "".join(f"{field:08x}" for field in fields)
    stream.write(header.encode("ascii"))
    stream.write(name_bytes)
    pad(stream, 4)
    stream.write(data)
    pad(stream, 4)


def main():
    arguments = sys.argv[2:]
    if len(arguments) < 2 or len(arguments) % 2 != 0:
        raise SystemExit("usage: mkcpio.py OUTPUT NAME INPUT [NAME INPUT ...]")

    output = pathlib.Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as stream:
        for inode, index in enumerate(range(0, len(arguments), 2), start=1):
            name = arguments[index].lstrip("/")
            data = pathlib.Path(arguments[index + 1]).read_bytes()
            write_entry(stream, name, data, 0o100755, inode)
        write_entry(stream, "TRAILER!!!", b"", 0, len(arguments) // 2 + 1)


if __name__ == "__main__":
    main()
