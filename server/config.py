import os
from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent
DATA_DIR = BASE_DIR / "data"
DICTIONARIES_DIR = DATA_DIR / "dictionaries"
SCREENSHOTS_DIR = DATA_DIR / "screenshots"
VOCAB_EXPORT_FILE = DATA_DIR / "vocab_export.csv"

# Make sure essential data directories exist
DATA_DIR.mkdir(exist_ok=True)
DICTIONARIES_DIR.mkdir(exist_ok=True)
SCREENSHOTS_DIR.mkdir(exist_ok=True)

# Server Network Configuration
SERVER_HOST = os.getenv("SERVER_HOST", "0.0.0.0")
SERVER_PORT = int(os.getenv("SERVER_PORT", "8766"))
DISCOVERY_PORT = int(os.getenv("DISCOVERY_PORT", "8764"))
ENABLE_DISCOVERY = os.getenv("ENABLE_DISCOVERY", "true").lower() in ("1", "true", "yes")

# OCR Engine Configuration ("auto", "rapidocr", "cloud", "mock")
OCR_ENGINE = os.getenv("OCR_ENGINE", "auto")
GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "")

# Dictionary Engine Configuration
HOSHIDICTS_BIN_PATH = os.getenv("HOSHIDICTS_BIN_PATH", "hoshidicts")

# AnkiConnect Configuration
ANKI_CONNECT_URL = os.getenv("ANKI_CONNECT_URL", "http://127.0.0.1:8765")
ANKI_DECK_NAME = os.getenv("ANKI_DECK_NAME", "Switch Japanese")
ANKI_MODEL_NAME = os.getenv("ANKI_MODEL_NAME", "Mining")
