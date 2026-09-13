from typing import List
from server.ocr.base import BaseOCREngine, DetectedBlock

class MockOCREngine(BaseOCREngine):
    """Fallback OCR engine for unit testing and offline development."""
    def detect_and_recognize(self, image_bytes: bytes) -> List[DetectedBlock]:
        return [
            DetectedBlock(
                box=[200, 520, 480, 80],
                text="冒険を始めよう！",
                confidence=0.98
            ),
            DetectedBlock(
                box=[200, 610, 520, 60],
                text="日本語を勉強するのは楽しいです。",
                confidence=0.95
            )
        ]
