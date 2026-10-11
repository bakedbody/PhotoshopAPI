import struct

import numpy as np
import photoshopapi as psapi
import pytest


LOCKS = ("is_transparency_locked", "is_pixels_locked", "is_position_locked", "is_locked")


def image(bits, name):
    dtype = {8: np.uint8, 16: np.uint16, 32: np.float32}[bits]
    return getattr(psapi, f"ImageLayer_{bits}bit")(
        np.ones((4, 8, 8), dtype=dtype), name, width=8, height=8
    )


@pytest.mark.parametrize("bits,extension", [(8, "psd"), (16, "psb"), (32, "psd")])
def test_independent_locks_roundtrip(tmp_path, bits, extension):
    file_type = getattr(psapi, f"LayeredFile_{bits}bit")
    doc = file_type(psapi.enum.ColorMode.rgb, 8, 8)
    group = getattr(psapi, f"GroupLayer_{bits}bit")("group")
    # Preserve raw group metadata too. Photoshop's UI partial-lock command
    # instead changes the children; it ignores the group's own partial bits.
    group.is_position_locked = True
    doc.add_layer(group)
    for mask in range(16):
        layer = image(bits, f"lock_{mask}")
        for index, key in enumerate(LOCKS):
            setattr(layer, key, bool(mask & (1 << index)))
        group.add_layer(doc, layer)
    path = tmp_path / f"locks.{extension}"
    doc.write(str(path))
    restored = file_type.read(str(path))
    assert restored["group"].is_position_locked
    for mask in range(16):
        layer = restored["group"][f"lock_{mask}"]
        assert [getattr(layer, key) for key in LOCKS] == [bool(mask & (1 << i)) for i in range(4)]
    # Clearing one bit must not clear other partial locks or Lock All.
    layer = restored["group"]["lock_15"]
    layer.is_pixels_locked = False
    assert [getattr(layer, key) for key in LOCKS] == [True, False, True, True]
    layer.is_locked = False
    restored.write(str(path))
    layer = file_type.read(str(path))["group"]["lock_15"]
    assert [getattr(layer, key) for key in LOCKS] == [True, False, True, False]


def test_unknown_lspf_bits_survive_edits(tmp_path):
    doc = psapi.LayeredFile_8bit(psapi.enum.ColorMode.rgb, 8, 8)
    doc.add_layer(image(8, "layer"))
    path = tmp_path / "unknown.psd"
    doc.write(str(path))
    data = bytearray(path.read_bytes())
    # This controlled fixture contains exactly one layer and one lspf block.
    signature = b"8BIMlspf\0\0\0\4"
    assert data.count(signature) == 1
    offset = data.index(signature) + len(signature)
    struct.pack_into(">I", data, offset, 0x00010004)
    path.write_bytes(data)
    restored = psapi.LayeredFile_8bit.read(str(path))
    restored["layer"].is_position_locked = False
    restored["layer"].is_transparency_locked = True
    restored.write(str(path))
    data = path.read_bytes()
    offset = data.index(signature) + len(signature)
    assert struct.unpack_from(">I", data, offset)[0] == 0x00010001
