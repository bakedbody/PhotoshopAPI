import re

import photoshopapi as psapi
import pytest


def make_text(name="text", content="Test123"):
    text = psapi.TextLayer_8bit(name, content, font="ArialMT", font_size=32,
                              position_x=30, position_y=60)
    text.convert_to_point_text()
    return text


def txt2(path):
    data = path.read_bytes()
    at = data.index(b"8BIMTxt2")
    size = int.from_bytes(data[at + 8:at + 12], "big")
    return data[at + 12:at + 12 + size]


@pytest.mark.parametrize("mode,value", [("manual", 0), ("metrics", 1), ("optical", 2)])
@pytest.mark.parametrize("extension", [".psd", ".psb"])
@pytest.mark.parametrize("vertical", [False, True])
def test_business_properties_and_roundtrip(tmp_path, mode, value, extension, vertical):
    doc = psapi.LayeredFile_8bit(psapi.enum.ColorMode.rgb, 400, 200)
    layer = make_text()
    if vertical:
        layer.set_orientation(psapi.enum.WritingDirection.Vertical)
    layer.style_all().set_dligatures(True)
    layer.set_advanced_text_style(stylistic_alternates=True, old_style=True, auto_kern=mode)
    doc.add_layer(layer)
    source = tmp_path / ("source" + extension)
    doc.write(str(source))
    payload = txt2(source)
    assert re.search(rb"/19\s+true\s", payload)
    if vertical:
        assert re.search(rb"/1\s+2\s", payload)  # native frame orientation
    for key, expected in [(28, b"true"), (30, b"2"), (11, str(value).encode())]:
        assert re.search(rb"/" + str(key).encode() + rb"\s+" + expected + rb"(?=\s)", payload)
    assert b"oldTextAge" in source.read_bytes()
    assert b"TextIndexlong\x00\x00\x00\x00" in source.read_bytes()

    reread = psapi.LayeredFile_8bit.read(str(source))
    output = tmp_path / ("roundtrip" + extension)
    reread.write(str(output))
    assert txt2(output) == payload  # no requested changes => byte-exact Txt2


def test_no_options_keeps_legacy_output(tmp_path):
    doc = psapi.LayeredFile_8bit(psapi.enum.ColorMode.rgb, 400, 200)
    layer = make_text()
    layer.set_advanced_text_style()
    doc.add_layer(layer)
    path = tmp_path / "legacy.psd"
    doc.write(str(path))
    assert b"8BIMTxt2" not in path.read_bytes()
    assert b"oldTextAge" not in path.read_bytes()


@pytest.mark.parametrize("direction,value", [("Vertical", 1), ("CrossStream", 2)])
def test_native_txt2_preserves_baseline_direction(tmp_path, direction, value):
    layer = make_text()
    layer.set_orientation(psapi.enum.WritingDirection.Vertical)
    direction = getattr(psapi.enum.BaselineDirection, direction)
    layer.set_style_run_baseline_direction(0, direction)
    layer.set_advanced_text_style(auto_kern="metrics")
    doc = psapi.LayeredFile_8bit(psapi.enum.ColorMode.rgb, 400, 200)
    doc.add_layer(layer)
    path = tmp_path / "baseline.psd"
    doc.write(str(path))
    # Character-run feature /35, not merely the document's default style.
    match = re.search(rb"/1\s+32\.0\s.*?/35\s+(\d+)", txt2(path), re.S)
    assert match and int(match[1]) == value


def test_invalid_mode_and_unverified_frame_are_explicit(tmp_path):
    layer = make_text()
    with pytest.raises(ValueError, match="auto_kern"):
        layer.set_advanced_text_style(auto_kern="invalid")
    layer.convert_to_box_text(200, 100)
    layer.set_advanced_text_style(old_style=True)
    doc = psapi.LayeredFile_8bit(psapi.enum.ColorMode.rgb, 400, 200)
    doc.add_layer(layer)
    with pytest.raises(ValueError, match="requires point text"):
        doc.write(str(tmp_path / "unsupported.psd"))


def test_multiple_layers_distinct_indices_and_partial_update(tmp_path):
    doc = psapi.LayeredFile_8bit(psapi.enum.ColorMode.rgb, 400, 200)
    for i in range(3):
        layer = make_text(str(i), "Text " + str(i))
        if i < 2:
            layer.set_advanced_text_style(old_style=bool(i), auto_kern="optical")
        doc.add_layer(layer)
    path = tmp_path / "multiple.psd"
    doc.write(str(path))
    raw = path.read_bytes()
    for index in [0, 1, -1]:
        assert b"TextIndexlong" + index.to_bytes(4, "big", signed=True) in raw
    reopened = psapi.LayeredFile_8bit.read(str(path))
    reopened["0"].set_advanced_text_style(fractions=True)
    edited = tmp_path / "edited.psd"
    reopened.write(str(edited))
    assert re.search(rb"/23\s+true\s", txt2(edited))
    assert re.search(rb"/11\s+2\s", txt2(edited))


@pytest.mark.parametrize("prop,key", [
    ("fractions",23), ("ordinals",24), ("swash",25), ("titling",26),
    ("ornaments",29), ("slashed_zero",84), ("contextual_ligatures",20),
])
def test_boolean_properties(tmp_path, prop, key):
    doc = psapi.LayeredFile_8bit(psapi.enum.ColorMode.rgb, 400, 200)
    layer = make_text()
    layer.set_advanced_text_style(**{prop: True})
    doc.add_layer(layer)
    path = tmp_path / "property.psd"
    doc.write(str(path))
    assert re.search(rb"/" + str(key).encode() + rb"\s+true\s", txt2(path))
