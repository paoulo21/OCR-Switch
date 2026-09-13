import base64
import logging
from contextlib import asynccontextmanager
from typing import List, Optional
import uvicorn
from fastapi import FastAPI, File, UploadFile, Request, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from server.config import SERVER_HOST, SERVER_PORT
from server.discovery import DiscoveryServer
from server.ocr.manager import get_ocr_engine
from server.dict.manager import get_dict_engine
from server.dict.base import TokenMatch
from server.anki.anki_client import AnkiExporter

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")
logger = logging.getLogger("switch-ocr.server")

discovery_service = DiscoveryServer()
anki_exporter = AnkiExporter()

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    logger.info("Initializing Switch OCR Server...")
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

    ocr_engine = get_ocr_engine()
    dict_engine = get_dict_engine()

    try:
        detected_blocks = ocr_engine.detect_and_recognize(image_bytes)
    except Exception as e:
        logger.error(f"OCR execution failed: {e}")
        raise HTTPException(status_code=500, detail=f"OCR execution error: {str(e)}")

    results: List[BoxResult] = []
    for block in detected_blocks:
        # Query dictionary for each detected sentence/block
        tokens = dict_engine.lookup_sentence(block.text)
        results.append(BoxResult(
            box=block.box,
            text=block.text,
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
