import logging
from typing import List
from server.ocr.base import BaseOCREngine, DetectedBlock

logger = logging.getLogger("switch-ocr.ocr.rapid")

class RapidOCREngine(BaseOCREngine):
    def __init__(self):
        try:
            from rapidocr_onnxruntime import RapidOCR
            self.engine = RapidOCR()
            logger.info("RapidOCR (ONNX Runtime) engine successfully initialized.")
        except Exception as e:
            logger.warning(f"Failed to load RapidOCR: {e}. Make sure 'rapidocr-onnxruntime' is installed.")
            self.engine = None

    def is_available(self) -> bool:
        return self.engine is not None

    def detect_and_recognize(self, image_bytes: bytes) -> List[DetectedBlock]:
        if not self.is_available():
            raise RuntimeError("RapidOCR engine is not available.")

        result, _ = self.engine(image_bytes)
        blocks: List[DetectedBlock] = []
        if not result:
            return blocks

        for item in result:
            # item format: [box, text, score]
            # box: [[x1, y1], [x2, y2], [x3, y3], [x4, y4]]
            pts = item[0]
            text = item[1]
            score = float(item[2])

            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            min_x = int(min(xs))
            min_y = int(min(ys))
            w = int(max(xs) - min_x)
            h = int(max(ys) - min_y)

            blocks.append(DetectedBlock(
                box=[min_x, min_y, w, h],
                text=text.strip(),
                confidence=score
            ))

        return blocks
