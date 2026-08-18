#!/usr/bin/env python3
import pathlib
import shutil
import struct
import sys

SECTOR_SIZE = 512
METADATA_LBA = 67584
RECORD_LBA = METADATA_LBA + 2
RECORD_SECTORS = 32
BITMAP_LBA = RECORD_LBA + RECORD_SECTORS
BITMAP_SECTORS = 48
DATA_LBA = BITMAP_LBA + BITMAP_SECTORS
JOURNAL_LBA = 262110
PERSIST_DATA_BLOCKS = 193932
PERSIST_MAX_FILES = 128
RECORD_SIZE = 128
PARENT_ROOT = 0xFFFF
PARENT_SYSTEM = 0xFFFE
TYPE_REGULAR = 1
TYPE_DIRECTORY = 2
LEGACY_RECORD_SIZE = 56
LEGACY_RECORD_OFFSET = 16
LEGACY_FILE_SECTORS = 64


def write_at(handle, lba, data):
    if len(data) % SECTOR_SIZE != 0:
        raise ValueError("sector-aligned data required")
    handle.seek(lba * SECTOR_SIZE)
    handle.write(data)


def read_superblock(revision):
    block = bytearray(SECTOR_SIZE)
    block[0:8] = b"MYPFS00" + bytes([ord(revision)])
    struct.pack_into("<I", block, 8, PERSIST_MAX_FILES)
    struct.pack_into("<I", block, 12, PERSIST_DATA_BLOCKS)
    return block


def mypfs003_record(used, object_type, parent, name, size=0, start_block=0, block_count=0):
    record = bytearray(RECORD_SIZE)
    encoded_name = name.encode("ascii")
    if len(encoded_name) >= 64:
        raise ValueError("name too long")
    record[0] = used
    record[1] = object_type
    record[2] = len(encoded_name)
    struct.pack_into("<H", record, 4, parent)
    struct.pack_into("<Q", record, 8, size)
    struct.pack_into("<I", record, 16, start_block)
    struct.pack_into("<I", record, 20, block_count)
    record[24:24 + len(encoded_name)] = encoded_name
    return record


def blank_mypfs004_regions(handle):
    zero = bytes(SECTOR_SIZE)
    for lba in range(METADATA_LBA, METADATA_LBA + 2):
        write_at(handle, lba, zero)
    for lba in range(RECORD_LBA, RECORD_LBA + RECORD_SECTORS):
        write_at(handle, lba, zero)
    for lba in range(BITMAP_LBA, BITMAP_LBA + BITMAP_SECTORS):
        write_at(handle, lba, zero)
    write_at(handle, JOURNAL_LBA, zero)


def build_mypfs003(source, destination):
    shutil.copyfile(source, destination)
    payload = b"MYPFS003 migration regression payload\n"
    with destination.open("r+b") as handle:
        blank_mypfs004_regions(handle)
        write_at(handle, METADATA_LBA, read_superblock("3"))
        write_at(handle, METADATA_LBA + 1, read_superblock("3"))
        records = bytearray(RECORD_SECTORS * SECTOR_SIZE)
        records[0:RECORD_SIZE] = mypfs003_record(1, TYPE_DIRECTORY, PARENT_ROOT, "")
        records[RECORD_SIZE:2 * RECORD_SIZE] = mypfs003_record(1, TYPE_DIRECTORY, PARENT_SYSTEM, "data")
        records[2 * RECORD_SIZE:3 * RECORD_SIZE] = mypfs003_record(
            1, TYPE_REGULAR, 1, "migrate003.txt", len(payload), 7, 128
        )
        write_at(handle, RECORD_LBA, records)
        bitmap = bytearray(BITMAP_SECTORS * SECTOR_SIZE)
        for block in range(7, 7 + 128):
            bitmap[block // 8] |= 1 << (block % 8)
        write_at(handle, BITMAP_LBA, bitmap)
        data_sector = bytearray(SECTOR_SIZE)
        data_sector[0:len(payload)] = payload
        write_at(handle, DATA_LBA + 7, data_sector)


def build_mypfs002(source, destination):
    shutil.copyfile(source, destination)
    payload = b"MYPFS002 legacy migration regression payload\n"
    with destination.open("r+b") as handle:
        blank_mypfs004_regions(handle)
        metadata = read_superblock("2")
        metadata[LEGACY_RECORD_OFFSET] = 1
        struct.pack_into("<Q", metadata, LEGACY_RECORD_OFFSET + 1, len(payload))
        metadata[LEGACY_RECORD_OFFSET + 9:LEGACY_RECORD_OFFSET + 18] = b"disk/note"
        write_at(handle, METADATA_LBA, metadata)
        data_sector = bytearray(SECTOR_SIZE)
        data_sector[0:len(payload)] = payload
        write_at(handle, METADATA_LBA + 1, data_sector)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: create_mypfs_migration_fixtures.py <base-myos.img>")
    source = pathlib.Path(sys.argv[1]).resolve()
    if not source.is_file():
        raise SystemExit("base image not found")
    fixture_dir = pathlib.Path(__file__).resolve().parent
    fixture003 = fixture_dir / "mypfs003-migration.img"
    fixture002 = fixture_dir / "mypfs002-migration.img"
    build_mypfs003(source, fixture003)
    build_mypfs002(source, fixture002)
    print(f"created {fixture003}")
    print(f"created {fixture002}")


if __name__ == "__main__":
    main()
