import io
import time
import json
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from server.config import ONLY_JAPANESE
from server.ocr.meiki import MeikiOCREngine
from server.dict.yomitan_py import YomitanPyEngine
from server.dict.base import TokenMatch
from server.main import is_japanese_text, BoxResult, OCRResponse

def create_sample_game_screenshot() -> bytes:
    # 1280x720 720p Switch game screen
    img = Image.new("RGB", (1280, 720), color=(15, 20, 30))
    d = ImageDraw.Draw(img)
    
    # Dialogue box
    d.rectangle([100, 520, 1180, 680], fill=(5, 10, 20), outline=(220, 220, 240), width=3)
    
    # Try loading a Japanese font
    font = None
    for fpath in [r"C:\WINDOWS\Fonts\meiryo.ttc", r"C:\WINDOWS\Fonts\msgothic.ttc"]:
        try:
            font = ImageFont.truetype(fpath, 32)
            break
        except Exception:
            pass
            
    # Dialogue text
    d.text((140, 545), "勇者よ、世界を救ってください！", font=font, fill=(255, 255, 255))
    d.text((140, 595), "魔王の軍勢が城へ向かっています。", font=font, fill=(240, 240, 250))
    
    # Some HUD text
    d.text((50, 40), "冒険の記録", font=font, fill=(255, 220, 100))
    
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=85)
    return buf.getvalue()

def run_profile():
    raw_jpeg = create_sample_game_screenshot()
    print(f"Sample screenshot generated: {len(raw_jpeg) / 1024:.1f} KB")
    
    print("\n--- Initializing Engines ---")
    t0 = time.perf_counter()
    ocr = MeikiOCREngine()
    t_ocr_init = time.perf_counter() - t0
    print(f"OCR Engine init: {t_ocr_init*1000:.1f} ms")
    
    t0 = time.perf_counter()
    dict_eng = YomitanPyEngine()
    t_dict_init = time.perf_counter() - t0
    print(f"Dict Engine init: {t_dict_init*1000:.1f} ms")
    
    # Warm up OCR and Dict
    print("\n--- Warming up ---")
    ocr.detect_and_recognize(raw_jpeg)
    dict_eng.lookup_sentence("勇者よ世界を救う")
    
    print("\n--- Detailed Profiling (5 iterations) ---")
    results = []
    
    for i in range(5):
        t_start = time.perf_counter()
        
        # Step 1: Decode image
        t0 = time.perf_counter()
        nparr = np.frombuffer(raw_jpeg, np.uint8)
        cv_img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        t_decode = time.perf_counter() - t0
        
        # Step 2: OCR Detection & Recognition
        t0 = time.perf_counter()
        blocks = ocr.detect_and_recognize(raw_jpeg)
        t_ocr = time.perf_counter() - t0
        
        # Step 3: Filter non-Japanese
        t0 = time.perf_counter()
        filtered = [b for b in blocks if is_japanese_text(b.text.strip())]
        t_filter = time.perf_counter() - t0
        
        # Step 4: Dictionary lookup
        t0 = time.perf_counter()
        box_results = []
        for b in filtered:
            tokens = dict_eng.lookup_sentence(b.text.strip())
            box_results.append(BoxResult(
                box=b.box,
                text=b.text.strip(),
                confidence=b.confidence,
                tokens=tokens
            ))
        t_dict = time.perf_counter() - t0
        
        # Step 5: Serialization
        t0 = time.perf_counter()
        resp = OCRResponse(status="ok", boxes=box_results)
        json_str = resp.model_dump_json() if hasattr(resp, "model_dump_json") else resp.json()
        t_serialize = time.perf_counter() - t0
        
        t_total = time.perf_counter() - t_start
        
        results.append({
            "decode_ms": t_decode * 1000,
            "ocr_ms": t_ocr * 1000,
            "filter_ms": t_filter * 1000,
            "dict_ms": t_dict * 1000,
            "serialize_ms": t_serialize * 1000,
            "total_ms": t_total * 1000,
            "detected_count": len(filtered),
            "tokens_count": sum(len(b.tokens) for b in box_results)
        })
        print(f"Run {i+1}: Total = {t_total*1000:.1f}ms (Decode: {t_decode*1000:.1f}ms, OCR: {t_ocr*1000:.1f}ms, Dict: {t_dict*1000:.1f}ms, Serialize: {t_serialize*1000:.1f}ms)")
        print(f"       Found {len(filtered)} phrases, {sum(len(b.tokens) for b in box_results)} total dictionary words.")
        for b in box_results:
            print(f"       - '{b.text}' -> {[t.word for t in b.tokens]}")
    
    avg_total = sum(r["total_ms"] for r in results) / len(results)
    avg_ocr = sum(r["ocr_ms"] for r in results) / len(results)
    avg_dict = sum(r["dict_ms"] for r in results) / len(results)
    print(f"\n=== BASELINE AVERAGE ===")
    print(f"Total: {avg_total:.1f} ms | OCR: {avg_ocr:.1f} ms | Dict: {avg_dict:.1f} ms")

if __name__ == "__main__":
    run_profile()
