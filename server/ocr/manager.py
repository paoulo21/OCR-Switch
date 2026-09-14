import logging
from server.config import OCR_ENGINE
from server.ocr.base import BaseOCREngine
from server.ocr.rapid import RapidOCREngine
from server.ocr.meiki import MeikiOCREngine
from server.ocr.cloud import CloudOCREngine
from server.ocr.mock import MockOCREngine

logger = logging.getLogger("switch-ocr.ocr.manager")

_active_engine: BaseOCREngine = None

def get_ocr_engine() -> BaseOCREngine:
    global _active_engine
    if _active_engine is not None:
        return _active_engine

    mode = OCR_ENGINE.lower()
    if mode in ("meiki", "meikiocr"):
        engine = MeikiOCREngine()
        if engine.is_available():
            _active_engine = engine
            return _active_engine
        logger.warning("Meiki OCR was requested but is not available. Falling back to auto mode.")

    if mode == "rapidocr":
        engine = RapidOCREngine()
        if engine.is_available():
            _active_engine = engine
            return _active_engine
        logger.warning("RapidOCR was requested but is not available. Falling back to auto mode.")

    if mode == "cloud":
        engine = CloudOCREngine()
        if engine.is_available():
            _active_engine = engine
            return _active_engine
        logger.warning("Cloud OCR was requested but API key is missing. Falling back to auto mode.")

    # Auto mode: try MeikiOCR (game-optimized) -> RapidOCR (general) -> Cloud -> Mock
    meiki = MeikiOCREngine()
    if meiki.is_available():
        logger.info("Auto-selected Meiki OCR (local video game model) as OCR engine.")
        _active_engine = meiki
        return _active_engine

    rapid = RapidOCREngine()
    if rapid.is_available():
        logger.info("Auto-selected RapidOCR (ONNX Runtime) as OCR engine.")
        _active_engine = rapid
        return _active_engine

    cloud = CloudOCREngine()
    if cloud.is_available():
        logger.info("Auto-selected Cloud OCR (Gemini Flash) as OCR engine.")
        _active_engine = cloud
        return _active_engine

    logger.warning("No local or cloud OCR engine is available. Using MockOCREngine for demonstration.")
    _active_engine = MockOCREngine()
    return _active_engine
