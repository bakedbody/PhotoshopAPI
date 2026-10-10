import struct

import numpy as np
import photoshopapi as psapi
import pytest


def _sections(data):
    version = struct.unpack_from(">H", data, 4)[0]
    offset = 26
    for _ in range(2):
        offset += 4 + struct.unpack_from(">I", data, offset)[0]
    size = 4 if version == 1 else 8
    length = int.from_bytes(data[offset:offset + size], "big")
    return offset + size + size, offset + size + length


def _source(tmp_path, extension, channels, merged=False):
    path = tmp_path / ("source" + extension)
    doc = psapi.LayeredFile_8bit(psapi.enum.ColorMode.rgb, 5, 7)
    pixels = np.full((4, 7, 5), 255, dtype=np.uint8)
    doc.add_layer(psapi.ImageLayer_8bit(pixels, "RGBA", width=5, height=7))
    doc.write(str(path))
    data = bytearray(path.read_bytes())
    count_offset, tail_offset = _sections(data)
    struct.pack_into(">H", data, 12, channels)
    if merged:
        count = struct.unpack_from(">h", data, count_offset)[0]
        struct.pack_into(">h", data, count_offset, -abs(count))
    # Known RAW planes isolate preservation from the layer pixel data.
    tail = b"\0\0" + bytes(range(channels * 5 * 7))
    path.write_bytes(data[:tail_offset] + tail)
    return path, tail


@pytest.mark.parametrize("extension", [".psd", ".psb"])
@pytest.mark.parametrize("channels,merged", [(3, False), (4, False), (4, True)])
def test_composite_roundtrip_preserves_channels_and_transparency(tmp_path, extension, channels, merged):
    source, tail = _source(tmp_path, extension, channels, merged)
    doc = psapi.LayeredFile_8bit.read(str(source))
    assert doc.num_channels == channels
    output = tmp_path / ("roundtrip" + extension)
    doc.write(str(output))
    data = output.read_bytes()
    count_offset, tail_offset = _sections(data)
    assert struct.unpack_from(">H", data, 12)[0] == channels
    assert data[tail_offset:] == tail
    assert (struct.unpack_from(">h", data, count_offset)[0] < 0) == merged


@pytest.mark.parametrize("extension", [".psd", ".psb"])
def test_composite_fallback_matches_header_after_resize(tmp_path, extension):
    source, _ = _source(tmp_path, extension, 4, merged=True)
    doc = psapi.LayeredFile_8bit.read(str(source))
    doc.width = 6
    output = tmp_path / ("resized" + extension)
    doc.write(str(output))
    data = output.read_bytes()
    count_offset, tail_offset = _sections(data)
    assert struct.unpack_from(">H", data, 12)[0] == 3
    assert struct.unpack_from(">h", data, count_offset)[0] > 0
    tail = data[tail_offset:]
    assert tail[:2] == b"\0\1"  # RLE placeholder
    size = 2 if extension == ".psd" else 4
    count = 7 * 3
    lengths = [int.from_bytes(tail[2 + i * size:2 + (i + 1) * size], "big") for i in range(count)]
    assert len(tail) == 2 + size * count + sum(lengths)
