from abc import ABC, abstractmethod
from typing import List
from pydantic import BaseModel

class DetectedBlock(BaseModel):
    box: List[int]  # [x, y, width, height]
    text: str
    confidence: float = 1.0

class BaseOCREngine(ABC):
    @abstractmethod
    def detect_and_recognize(self, image_bytes: bytes) -> List[DetectedBlock]:
        """Process raw image bytes and return detected text blocks with bounding boxes."""
        pass
