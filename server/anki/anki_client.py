import base64
import csv
import logging
import time
from pathlib import Path
from typing import List, Optional
import requests

from server.config import (
    ANKI_CONNECT_URL,
    ANKI_DECK_NAME,
    ANKI_MODEL_NAME,
    VOCAB_EXPORT_FILE,
    SCREENSHOTS_DIR,
)

logger = logging.getLogger("switch-ocr.anki")

class AnkiExporter:
    def __init__(
        self,
        url: str = ANKI_CONNECT_URL,
        deck: str = ANKI_DECK_NAME,
        model: str = ANKI_MODEL_NAME,
        csv_file: Path = VOCAB_EXPORT_FILE,
        screenshots_dir: Path = SCREENSHOTS_DIR,
    ):
        self.url = url
        self.deck = deck
        self.model = model
        self.csv_file = csv_file
        self.screenshots_dir = screenshots_dir

    def export(
        self,
        word: str,
        reading: str,
        definitions: List[str],
        sentence: str = "",
        image_bytes: Optional[bytes] = None,
    ) -> dict:
        timestamp = int(time.time())
        img_filename = ""

        # 1. Save screenshot locally if provided
        if image_bytes:
            img_filename = f"vocab_{timestamp}_{word}.jpg"
            img_path = self.screenshots_dir / img_filename
            try:
                with open(img_path, "wb") as f:
                    f.write(image_bytes)
            except Exception as e:
                logger.error(f"Failed to save screenshot: {e}")

        # 2. Append to local CSV (UTF-8 with BOM for universal spreadsheet / Anki import)
        csv_written = self._append_to_csv(word, reading, definitions, sentence, img_filename, timestamp)

        # 3. Try AnkiConnect
        anki_status = self._send_to_ankiconnect(word, reading, definitions, sentence, image_bytes, img_filename)

        return {
            "success": True,
            "word": word,
            "saved_to_csv": csv_written,
            "ankiconnect_synced": anki_status.get("synced", False),
            "ankiconnect_message": anki_status.get("message", ""),
            "screenshot": img_filename,
        }

    def _append_to_csv(
        self,
        word: str,
        reading: str,
        definitions: List[str],
        sentence: str,
        image_file: str,
        timestamp: int,
    ) -> bool:
        file_exists = self.csv_file.exists()
        try:
            with open(self.csv_file, "a", encoding="utf-8-sig", newline="") as f:
                writer = csv.writer(f)
                if not file_exists:
                    writer.writerow(["Timestamp", "Word", "Reading", "Definitions", "Sentence", "Image"])
                defs_joined = " | ".join(definitions)
                writer.writerow([timestamp, word, reading, defs_joined, sentence, image_file])
            return True
        except Exception as e:
            logger.error(f"CSV append failed: {e}")
            return False

    def _send_to_ankiconnect(
        self,
        word: str,
        reading: str,
        definitions: List[str],
        sentence: str,
        image_bytes: Optional[bytes],
        img_filename: str,
    ) -> dict:
        formatted_defs = "<br>".join([f"• {d}" for d in definitions])
        payload = {
            "action": "addNote",
            "version": 6,
            "params": {
                "note": {
                    "deckName": self.deck,
                    "modelName": self.model,
                    "fields": {
                        "Word": word,
                        "Expression": word,
                        "Reading": reading,
                        "Glossary": formatted_defs,
                        "Meaning": formatted_defs,
                        "Sentence": sentence,
                    },
                    "options": {
                        "allowDuplicate": False,
                        "duplicateScope": "deck",
                    },
                    "tags": ["switch-ocr"],
                }
            }
        }

        if image_bytes and img_filename:
            b64_data = base64.b64encode(image_bytes).decode("utf-8")
            payload["params"]["note"]["picture"] = [
                {
                    "data": b64_data,
                    "filename": img_filename,
                    "fields": ["Picture", "Image"],
                }
            ]

        try:
            res = requests.post(self.url, json=payload, timeout=2)
            data = res.json()
            if data.get("error"):
                logger.info(f"AnkiConnect notice: {data['error']}")
                return {"synced": False, "message": str(data["error"])}
            return {"synced": True, "note_id": data.get("result"), "message": "Card created"}
        except Exception as e:
            logger.debug(f"AnkiConnect unavailable: {e}")
            return {"synced": False, "message": "AnkiConnect unreachable (saved to CSV instead)"}
