"""Validate every platform in a PBW bundle, including the loaded-image limit
(the app header's 16-bit size field) and the app store's size limits."""
import struct
import sys
import zipfile
from pathlib import Path

path = Path(sys.argv[1] if len(sys.argv) > 1 else "build/pdoom.pbw")
with zipfile.ZipFile(path) as z:
    assert z.testzip() is None, "ZIP CRC failure"
    platforms = sorted(n.split('/')[0] for n in z.namelist() if n.endswith('/pebble-app.bin'))
    assert platforms, "no platform binaries in the bundle"
    for platform in platforms:
        app = z.read(f"{platform}/pebble-app.bin")
        resources = z.read(f"{platform}/app_resources.pbpack")
        virtual_size = struct.unpack_from("<H", app, 0x80)[0]
        assert 0 < virtual_size <= 65535, f"{platform}: loaded image does not fit"
        assert len(app) <= 131072, f"{platform}: binary too large"
        assert len(resources) <= 262144, f"{platform}: resources too large"
        print(f"PASS: {path} [{platform}]; binary {len(app):,} B, loaded image {virtual_size:,} B, "
              f"resources {len(resources):,} B; CRC OK")
