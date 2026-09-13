import base64
import json
import logging
from typing import List
import requests
from PIL import Image
import io

from server.config import GEMINI_API_KEY
from server.ocr.base import BaseOCREngine, DetectedBlock

logger = logging.getLogger("switch-ocr.ocr.cloud")

class CloudOCREngine(BaseOCREngine):
    def __init__(self, api_key: str = GEMINI_API_KEY):
        self.api_key = api_key

    def is_available(self) -> bool:
        return bool(self.api_key and len(self.api_key.strip()) > 5)

    def detect_and_recognize(self, image_bytes: bytes) -> List[DetectedBlock]:
        if not self.is_available():
            raise RuntimeError("Cloud OCR engine requires a valid GEMINI_API_KEY.")

        # Read image size to un-normalize coordinates
        try:
            img = Image.open(io.BytesIO(image_bytes))
            img_w, img_h = img.size
        except Exception:
            img_w, img_h = 1280, 720

        b64_image = base64.b64encode(image_bytes).decode("utf-8")
        url = f"https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key={self.api_key}"

        prompt = (
            "Detect all Japanese text lines/bubbles in this image. "
            "For each line, return its bounding box and exact text as a JSON array of objects: "
            "[{\"box_2d\": [ymin, xmin, ymax, xmax], \"text\": \"...\"}] where coordinates are normalized 0-1000. "
            "Return only valid JSON."
        )

        payload = {
            "contents": [
                {
                    "parts": [
                        {"text": prompt},
                        {
                            "inline_data": {
                                "mime_type": "image/jpeg",
                                "data": b64_image
                            }
                        }
                    ]
                }
            ],
            "generationConfig": {
                "response_mime_type": "application/json"
            }
        }

        try:
            res = requests.post(url, json=payload, timeout=10)
            res.raise_for_status()
            data = res.json()
            raw_text = data["candidates"][0]["content"]["parts"][0]["text"]
            items = json.loads(raw_text)

            blocks: List[DetectedBlock] = []
            for item in items:
                b = item.get("box_2d", [0, 0, 0, 0])
                ymin, xmin, ymax, xmax = b[0], b[1], b[2], b[3]
                x = int((xmin / 1000.0) * img_w)
                y = int((ymin / 1000.0) * img_h)
                w = int(((xmax - xmin) / 1000.0) * img_w)
                h = int(((ymax - ymin) / 1000.0) * img_h)
                text = item.get("text", "").strip()
                if text:
                    blocks.append(DetectedBlock(
                        box=[x, y, w, h],
                        text=text,
                        confidence=0.99
                    ))
            return blocks
        except Exception as e:
            logger.error(f"Cloud OCR request failed: {e}")
            raise
