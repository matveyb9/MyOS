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
    if len(sys.argv) != 4:
        raise SystemExit("usage: mkcpio.py OUTPUT NAME INPUT")

    output = pathlib.Path(sys.argv[1])
    name = sys.argv[2].lstrip("/")
    data = pathlib.Path(sys.argv[3]).read_bytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as stream:
        write_entry(stream, name, data, 0o100755, 1)
        write_entry(stream, "TRAILER!!!", b"", 0, 2)


if __name__ == "__main__":
    main()
