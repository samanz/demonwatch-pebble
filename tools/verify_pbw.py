"""Validate the actual Emery bundle, including its loaded-image limit."""
import struct
import sys
import zipfile
from pathlib import Path
path = Path(sys.argv[1] if len(sys.argv) > 1 else "build/pdoom.pbw")
with zipfile.ZipFile(path) as z:
    assert z.testzip() is None, "ZIP CRC failure"
    app = z.read("emery/pebble-app.bin")
    resources = z.read("emery/app_resources.pbpack")
    virtual_size = struct.unpack_from("<H", app, 0x80)[0]
    assert 0 < virtual_size <= 65535
    assert len(app) <= 131072, "Emery binary too large"
    assert len(resources) <= 262144, "Emery resources too large"
    print(f"PASS: {path}; binary {len(app):,} B, loaded image {virtual_size:,} B, resources {len(resources):,} B; CRC OK")
