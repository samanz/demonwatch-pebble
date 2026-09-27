#!/usr/bin/env python3
"""
build_pbw.py: Packages pDOOM into a valid Pebble .pbw distribution bundle.
Generates:
  1. app_resources.pbpack (containing e1m1.pbl with valid STM32 CRC-32)
  2. pebble-app.bin (with official 120-byte PebbleAppHeader and entrypoint)
  3. manifest.json (v2 schema with CRCs and metadata)
  4. pdoom.pbw (standard Pebble app bundle targeting emery / Pebble Time 2)
"""

import os
import sys
import json
import time
import uuid
import struct
import zipfile
import subprocess

CRC_POLY = 0x04C11DB7

def stm32_crc(data: bytes, crc: int = 0xFFFFFFFF) -> int:
    """Computes STM32 hardware CRC-32 (standard for PebbleOS firmware/resources)."""
    # Pad data to multiple of 4 bytes
    remainder = len(data) % 4
    if remainder != 0:
        data = data + b'\x00' * (4 - remainder)

    for i in range(0, len(data), 4):
        word = struct.unpack_from('<I', data, i)[0]
        crc ^= word
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ CRC_POLY) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc

def make_pbpack(resource_files, timestamp: int) -> bytes:
    """Builds a Pebble app_resources.pbpack file from list of files."""
    num_files = len(resource_files)
    table_bytes = bytearray()
    content_bytes = bytearray()

    cur_offset = 0
    for idx, (res_id, file_path) in enumerate(resource_files, start=1):
        with open(file_path, "rb") as f:
            content = f.read()
        res_crc = stm32_crc(content)
        res_len = len(content)
        # Entry: res_id, offset, len, crc
        table_bytes += struct.pack('<IIII', res_id, cur_offset, res_len, res_crc)
        content_bytes += content
        cur_offset += res_len

    data_payload = bytes(table_bytes + content_bytes)
    pack_crc = stm32_crc(data_payload)
    header = struct.pack('<III', num_files, pack_crc, timestamp)

    return header + data_payload

def make_pebble_app_bin(raw_bin_path: str, appinfo: dict, entry_offset: int = 0x000c) -> bytes:
    """
    Wraps raw ARM binary with 120-byte PebbleAppHeader:
    STRUCT_DEFINITION:
      '8s'   header sentinel: b'PBLAPP\x00\x00'
      '2B'   struct version: (5, 0)
      '2B'   sdk version: (5, 79)
      '2B'   app version: (major, minor)
      'H'    size: uint16 total binary size
      'I'    offset: uint32 entry point
      'I'    crc: uint32 CRC
      '32s'  app name (null-padded)
      '32s'  company name (null-padded)
      'I'    icon resource id
      'I'    symbol table address
      'I'    flags
      'I'    relocation list start
      'I'    num relocation entries
      '16s'  uuid
    """
    with open(raw_bin_path, "rb") as f:
        raw_code = f.read()

    HEADER_SIZE = 120
    total_size = HEADER_SIZE + len(raw_code)

    # Parse version label (e.g. "1.0.0" -> major 1, minor 0)
    ver_parts = appinfo.get("versionLabel", "1.0").split(".")
    ver_major = int(ver_parts[0]) if len(ver_parts) > 0 else 1
    ver_minor = int(ver_parts[1]) if len(ver_parts) > 1 else 0

    app_uuid = uuid.UUID(appinfo["uuid"]).bytes
    name_bytes = appinfo.get("shortName", "pDOOM").encode("utf-8")[:32].ljust(32, b'\x00')
    company_bytes = appinfo.get("companyName", "pDOOM Project").encode("utf-8")[:32].ljust(32, b'\x00')

    entry_point = HEADER_SIZE + entry_offset
    code_crc = stm32_crc(raw_code)

    header = struct.pack(
        '<8sBBBBBBHII32s32sIIIII16s',
        b'PBLAPP\x00\x00',
        5, 0,           # struct version 5.0
        5, 79,          # sdk version 5.79 (Emery / 4.x compatible)
        ver_major, ver_minor,
        total_size & 0xFFFF,
        entry_point,
        code_crc,
        name_bytes,
        company_bytes,
        0,              # icon resource id
        0,              # sym table addr
        0,              # flags
        0,              # reloc list start
        0,              # num reloc entries
        app_uuid
    )

    return header + raw_code

def build_pbw(project_dir: str, output_pbw: str):
    print("=" * 60)
    print("Building pDOOM Pebble Watchapp Package (.pbw)")
    print("=" * 60)

    # 1. Read appinfo.json
    appinfo_path = os.path.join(project_dir, "appinfo.json")
    with open(appinfo_path, "r") as f:
        appinfo = json.load(f)

    # 2. Extract raw ARM binary from pdoom.elf if not already done
    elf_path = os.path.join(project_dir, "pdoom.elf")
    raw_bin_path = os.path.join(project_dir, "pdoom.bin")
    if not os.path.exists(raw_bin_path) or os.path.getmtime(elf_path) > os.path.getmtime(raw_bin_path):
        print("Extracting raw ARM binary with arm-none-eabi-objcopy...")
        subprocess.check_call(["arm-none-eabi-objcopy", "-O", "binary", elf_path, raw_bin_path])

    # 3. Build pebble-app.bin with PebbleAppHeader
    print("Generating pebble-app.bin with PebbleAppHeader...")
    pebble_app_bin = make_pebble_app_bin(raw_bin_path, appinfo, entry_offset=0x000c)
    bin_crc = stm32_crc(pebble_app_bin)
    print(f"  pebble-app.bin size: {len(pebble_app_bin):,} bytes ({len(pebble_app_bin)/1024:.2f} KiB)")
    print(f"  pebble-app.bin CRC:  0x{bin_crc:08X}")

    # 4. Build app_resources.pbpack
    pbl_path = os.path.join(project_dir, "resources", "e1m1.pbl")
    if not os.path.exists(pbl_path):
        raise FileNotFoundError(f"Missing resource bundle: {pbl_path}")

    now = int(time.time())
    print("Generating app_resources.pbpack...")
    pbpack_data = make_pbpack([(1, pbl_path)], now)
    pbpack_crc = stm32_crc(pbpack_data)
    print(f"  app_resources.pbpack size: {len(pbpack_data):,} bytes ({len(pbpack_data)/1024:.2f} KiB)")
    print(f"  app_resources.pbpack CRC:  0x{pbpack_crc:08X}")

    # 5. Build manifest.json
    manifest = {
        "manifestVersion": 2,
        "generatedBy": "pDOOM-Builder",
        "generatedAt": now,
        "application": {
            "timestamp": now,
            "sdk_version": {"major": 5, "minor": 79},
            "crc": bin_crc,
            "name": "pebble-app.bin",
            "size": len(pebble_app_bin)
        },
        "debug": {},
        "type": "application",
        "resources": {
            "timestamp": now,
            "crc": pbpack_crc,
            "name": "app_resources.pbpack",
            "size": len(pbpack_data)
        }
    }
    manifest_bytes = json.dumps(manifest, indent=2).encode("utf-8")

    # 6. Bundle everything into ZIP (.pbw)
    out_dir = os.path.dirname(os.path.abspath(output_pbw))
    os.makedirs(out_dir, exist_ok=True)

    with zipfile.ZipFile(output_pbw, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        # Root files
        zf.writestr("appinfo.json", json.dumps(appinfo, indent=2))
        zf.writestr("manifest.json", manifest_bytes)
        zf.writestr("pebble-app.bin", pebble_app_bin)
        zf.writestr("app_resources.pbpack", pbpack_data)

        # Platform-specific folders (emery and basalt)
        for platform in ("emery", "basalt"):
            zf.writestr(f"{platform}/manifest.json", manifest_bytes)
            zf.writestr(f"{platform}/pebble-app.bin", pebble_app_bin)
            zf.writestr(f"{platform}/app_resources.pbpack", pbpack_data)

    pbw_size = os.path.getsize(output_pbw)
    print("=" * 60)
    print(f"SUCCESS: Created {output_pbw}")
    print(f"PBW Archive Size:    {pbw_size:,} bytes ({pbw_size/1024:.2f} KiB)")
    print("============================================================")

if __name__ == "__main__":
    proj = sys.argv[1] if len(sys.argv) > 1 else "."
    out = sys.argv[2] if len(sys.argv) > 2 else "pdoom.pbw"
    build_pbw(proj, out)
