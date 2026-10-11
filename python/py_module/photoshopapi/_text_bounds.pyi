from typing import Sequence

class TextBoundsEngine:
    def __init__(self, font_data: bytes, face_index: int = 0) -> None: ...
    def measure(
        self, text: str, size: float, position: Sequence[float],
        leading: float = 1.2, mode: str = "metrics", aa: str = "smooth",
        baseline_direction: int = 1, stylistic_alternates: bool = False,
        old_style: bool = False, orientation: str = "vertical",
    ) -> list[int]: ...
    def optical_adjustment(self, left_glyph: int, right_glyph: int, size: float) -> float: ...
