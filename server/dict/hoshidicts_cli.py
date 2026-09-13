import logging
import shutil
import subprocess
from pathlib import Path
from typing import List

from server.config import HOSHIDICTS_BIN_PATH, DICTIONARIES_DIR
from server.dict.base import BaseDictEngine, DictDefinition, TokenMatch

logger = logging.getLogger("switch-ocr.dict.hoshidicts_cli")

class HoshidictsCLIEngine(BaseDictEngine):
    def __init__(self, bin_path: str = HOSHIDICTS_BIN_PATH, dict_dir: Path = DICTIONARIES_DIR):
        self.bin_path = bin_path
        self.dict_dir = dict_dir
        self.available = self._check_availability()
        if self.available:
            logger.info(f"Hoshidicts C++ CLI detected at '{self.bin_path}'.")

    def _check_availability(self) -> bool:
        path = shutil.which(self.bin_path) or self.bin_path
        if Path(path).is_file() or shutil.which(path):
            try:
                res = subprocess.run([path], capture_output=True, text=True, timeout=2)
                return "Usage:" in res.stdout or "Usage:" in res.stderr
            except Exception:
                return False
        return False

    def is_available(self) -> bool:
        return self.available

    def lookup_sentence(self, sentence: str) -> List[TokenMatch]:
        if not self.is_available():
            raise RuntimeError("Hoshidicts CLI is not available.")

        # Find first imported dictionary folder
        imported_dirs = [d for d in self.dict_dir.iterdir() if d.is_dir() and not d.name.startswith(".")]
        if not imported_dirs:
            return []

        dict_path = str(imported_dirs[0])
        cmd = [self.bin_path, "lookup", dict_path, sentence]

        try:
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=3, encoding="utf-8")
            tokens: List[TokenMatch] = []
            
            # Parse Hoshidicts CLI standard output
            for line in res.stdout.strip().splitlines():
                parts = line.split("\t")
                if len(parts) >= 2:
                    word = parts[0].strip()
                    reading = parts[1].strip() if len(parts) > 1 else word
                    glossary = parts[2].split(";") if len(parts) > 2 else []
                    
                    # Estimate char offset in sentence
                    idx = sentence.find(word)
                    char_range = [idx, idx + len(word)] if idx != -1 else [0, len(word)]

                    tokens.append(TokenMatch(
                        word=word,
                        reading=reading,
                        definitions=glossary,
                        pitch=[],
                        char_range=char_range
                    ))
            return tokens
        except Exception as e:
            logger.error(f"Hoshidicts CLI lookup failed: {e}")
            return []

    def query_term(self, term: str) -> List[DictDefinition]:
        if not self.is_available():
            return []
        imported_dirs = [d for d in self.dict_dir.iterdir() if d.is_dir()]
        if not imported_dirs:
            return []
        dict_path = str(imported_dirs[0])
        cmd = [self.bin_path, "query", dict_path, term]
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=3, encoding="utf-8")
            defs: List[DictDefinition] = []
            for line in res.stdout.strip().splitlines():
                parts = line.split("\t")
                if len(parts) >= 2:
                    defs.append(DictDefinition(
                        word=parts[0].strip(),
                        reading=parts[1].strip() if len(parts) > 1 else parts[0].strip(),
                        definitions=parts[2].split(";") if len(parts) > 2 else [],
                        pitch=[],
                        dict_name="Hoshidicts"
                    ))
            return defs
        except Exception as e:
            logger.error(f"Hoshidicts CLI query failed: {e}")
            return []
