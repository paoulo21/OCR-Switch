import logging
from typing import List
import numpy as np
import cv2

from server.ocr.base import BaseOCREngine, DetectedBlock

logger = logging.getLogger("switch-ocr.ocr.meiki")
logging.getLogger("httpx").setLevel(logging.WARNING)
logging.getLogger("huggingface_hub").setLevel(logging.ERROR)

class MeikiOCREngine(BaseOCREngine):
    def __init__(self):
        try:
            import warnings
            warnings.filterwarnings("ignore", message=".*HF_TOKEN.*")
            import onnxruntime as ort
            from meikiocr import MeikiOCR

            available = ort.get_available_providers()
            provider = "CUDAExecutionProvider" if "CUDAExecutionProvider" in available else "CPUExecutionProvider"
            logger.info(f"Initializing Meiki OCR with provider: {provider}")
            self.engine = MeikiOCR(provider=provider)

            logger.info("Meiki OCR engine successfully initialized.")
        except Exception as e:
            logger.warning(f"Failed to load Meiki OCR: {e}")
            self.engine = None

    def is_available(self) -> bool:
        return self.engine is not None

    def detect_and_recognize(self, image_bytes: bytes) -> List[DetectedBlock]:
        if not self.is_available():
            raise RuntimeError("Meiki OCR engine is not available.")

        # Decode raw image bytes to OpenCV BGR format
        nparr = np.frombuffer(image_bytes, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        if img is None:
            logger.warning("Failed to decode image for Meiki OCR.")
            return []

        # Run detection and recognition
        results = self.engine.run_ocr(img)
        blocks: List[DetectedBlock] = []

        for item in results:
            text = item.get("text", "").strip()
            chars = item.get("chars", [])
            if not text or not chars:
                continue

            # Compute bounding box enclosing all recognized characters in this line
            x1 = min(c["bbox"][0] for c in chars)
            y1 = min(c["bbox"][1] for c in chars)
            x2 = max(c["bbox"][2] for c in chars)
            y2 = max(c["bbox"][3] for c in chars)
            w = max(0, x2 - x1)
            h = max(0, y2 - y1)

            conf = sum(c.get("conf", 1.0) for c in chars) / len(chars)

            blocks.append(DetectedBlock(
                box=[int(x1), int(y1), int(w), int(h)],
                text=text,
                confidence=float(conf)
            ))

        return blocks
