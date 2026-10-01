#!/usr/bin/env python3
"""Print the text-encoded left_ptr.hc fixture used by src/tests/main/cursor.cpp.

Only needed to regenerate the fixture, not to run hyprtester.
"""

import argparse
import io
from pathlib import Path
import re
import struct
import zipfile
import zlib


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def png(size, color):
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress((b"\0" + bytes(color) * size) * size))
        + chunk(b"IEND", b"")
    )


archive = io.BytesIO()
with zipfile.ZipFile(archive, "w") as cursor:
    for name, data in {
        "meta.hl": (
            b"hotspot_x = 0.25\nhotspot_y = 0.5\nresize_algorithm = none\n"
            b"define_size = 24, 24.png\ndefine_size = 32, 32.png\ndefine_override = default\n"
        ),
        "24.png": png(24, (255, 0, 0, 255)),
        "32.png": png(32, (0, 255, 0, 255)),
    }.items():
        cursor.writestr(zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0)), data)

encoded = archive.getvalue().hex()
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--check", action="store_true", help="verify the embedded archive matches the generator")
if parser.parse_args().check:
    source = (Path(__file__).parent / "../src/tests/main/cursor.cpp").read_text()
    initializer = source.split("CURSOR_ARCHIVE =", 1)[1].split(";", 1)[0]
    embedded = "".join(re.findall(r'"([0-9a-f]+)"', initializer))
    assert embedded == encoded, "CURSOR_ARCHIVE differs from the generated fixture"
    with zipfile.ZipFile(io.BytesIO(bytes.fromhex(embedded))) as cursor:
        assert cursor.testzip() is None, "Invalid ZIP checksum"
    print("Embedded cursor fixture matches the generator; ZIP checksums passed.")
else:
    for offset in range(0, len(encoded), 120):
        print(f'    "{encoded[offset:offset + 120]}"')
