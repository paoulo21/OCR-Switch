import asyncio
import base64
import logging
from contextlib import asynccontextmanager
from typing import List, Optional
import uvicorn
from fastapi import FastAPI, File, UploadFile, Request, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

import re

from server.config import SERVER_HOST, SERVER_PORT, SCREENSHOTS_DIR, ONLY_JAPANESE
from server.discovery import DiscoveryServer
from server.ocr.manager import get_ocr_engine
from server.dict.manager import get_dict_engine
from server.dict.base import TokenMatch
from server.anki.anki_client import AnkiExporter

# Unicode regex matching Japanese characters (Hiragana, Katakana, Kanji, Halfwidth Katakana)
JAPANESE_CHAR_REGEX = re.compile(r"[\u3040-\u30ff\u3400-\u4dbf\u4e00-\u9fff\uff66-\uff9f]")

def is_japanese_text(text: str) -> bool:
    """Return True if the text contains at least one Japanese character."""
    return bool(JAPANESE_CHAR_REGEX.search(text))

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")
logger = logging.getLogger("switch-ocr.server")

discovery_service = DiscoveryServer()
anki_exporter = AnkiExporter()

def get_local_lan_ips() -> List[str]:
    import socket
    ips = []
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        primary = s.getsockname()[0]
        s.close()
        if primary and primary != "127.0.0.1":
            ips.append(primary)
    except Exception:
        pass

    try:
        hostname = socket.gethostname()
        for info in socket.getaddrinfo(hostname, None, socket.AF_INET):
            ip = info[4][0]
            if ip and not ip.startswith("127.") and ip not in ips:
                ips.append(ip)
    except Exception:
        pass

    return ips or ["127.0.0.1"]

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    local_ips = get_local_lan_ips()
    primary_ip = local_ips[0]
    extra_ips = f" (autres: {', '.join(local_ips[1:])})" if len(local_ips) > 1 else ""
    logger.info("=" * 60)
    logger.info("  SWITCH OCR SERVER ACTIF")
    logger.info(f"  IP DE VOTRE APPAREIL      : {primary_ip}{extra_ips}")
    logger.info(f"  Port HTTP                 : {SERVER_PORT}")
    logger.info(f"  Port Decouverte UDP       : {discovery_service.port}")
    logger.info("=" * 60)
    get_ocr_engine()
    get_dict_engine()
    discovery_service.start()
    yield
    # Shutdown
    discovery_service.stop()
    logger.info("Switch OCR Server stopped.")

app = FastAPI(
    title="Switch OCR & Hoshidicts Server",
    description="Backend OCR and dictionary server for Nintendo Switch overlay",
    version="1.0.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class BoxResult(BaseModel):
    box: List[int]  # [x, y, w, h]
    text: str
    confidence: float
    tokens: List[TokenMatch]

class OCRResponse(BaseModel):
    status: str
    boxes: List[BoxResult]

class AnkiExportRequest(BaseModel):
    word: str
    reading: str = ""
    definitions: List[str] = []
    sentence: str = ""
    image_base64: Optional[str] = None

@app.get("/api/status")
async def get_status():
    ocr = get_ocr_engine()
    dict_eng = get_dict_engine()
    
    dict_names = []
    if hasattr(dict_eng, "loaded_dictionaries"):
        dict_names = dict_eng.loaded_dictionaries
    
    return {
        "status": "online",
        "ocr_engine": ocr.__class__.__name__,
        "dict_engine": dict_eng.__class__.__name__,
        "active_dictionaries": dict_names,
        "discovery_port": discovery_service.port,
        "http_port": SERVER_PORT,
    }

@app.post("/api/ocr", response_model=OCRResponse)
async def process_ocr(request: Request, file: Optional[UploadFile] = File(None)):
    # Read image bytes from either multipart upload or raw POST body
    if file is not None:
        image_bytes = await file.read()
    else:
        image_bytes = await request.body()

    if not image_bytes or len(image_bytes) < 32:
        raise HTTPException(status_code=400, detail="Empty or invalid image data received.")

    # Save incoming screenshot asynchronously in background so disk I/O does not block OCR
    def _save_screenshot(data: bytes):
        try:
            (SCREENSHOTS_DIR / "last_capture.jpg").write_bytes(data)
        except Exception as e:
            logger.debug(f"Could not save last_capture.jpg: {e}")

    asyncio.create_task(asyncio.to_thread(_save_screenshot, image_bytes))

    ocr_engine = get_ocr_engine()
    dict_engine = get_dict_engine()

    try:
        detected_blocks = ocr_engine.detect_and_recognize(image_bytes)
    except Exception as e:
        logger.error(f"OCR execution failed: {e}")
        raise HTTPException(status_code=500, detail=f"OCR execution error: {str(e)}")

    results: List[BoxResult] = []
    for block in detected_blocks:
        clean_text = block.text.strip()
        if not clean_text:
            continue

        # Filter non-Japanese text (e.g., overlay menu labels, English HUD)
        if ONLY_JAPANESE and not is_japanese_text(clean_text):
            logger.info(f"Skipping non-Japanese text: '{clean_text}'")
            continue

        # Query dictionary for each detected sentence/block
        tokens = dict_engine.lookup_sentence(clean_text)
        results.append(BoxResult(
            box=block.box,
            text=clean_text,
            confidence=block.confidence,
            tokens=tokens
        ))

    return OCRResponse(status="ok", boxes=results)

@app.post("/api/anki")
async def export_anki(req: AnkiExportRequest):
    img_bytes = None
    if req.image_base64:
        try:
            img_bytes = base64.b64decode(req.image_base64)
        except Exception as e:
            logger.warning(f"Could not decode Anki image base64: {e}")

    result = anki_exporter.export(
        word=req.word,
        reading=req.reading,
        definitions=req.definitions,
        sentence=req.sentence,
        image_bytes=img_bytes
    )
    return result

def run():
    uvicorn.run(app, host=SERVER_HOST, port=SERVER_PORT, log_level="info")

if __name__ == "__main__":
    run()
