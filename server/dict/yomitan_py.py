import json
import logging
import zipfile
from pathlib import Path
from typing import Dict, List, Optional

from server.config import DICTIONARIES_DIR
from server.dict.base import BaseDictEngine, DictDefinition, TokenMatch
from server.dict.deinflector import deinflect

logger = logging.getLogger("switch-ocr.dict.yomitan")

# Built-in essential fallback vocabulary for immediate testing
DEFAULT_VOCAB: Dict[str, List[DictDefinition]] = {
    "日本語": [
        DictDefinition(word="日本語", reading="にほんご", definitions=["Japanese language"], pitch=["0"], pos=["n"])
    ],
    "勉強": [
        DictDefinition(word="勉強", reading="べんきょう", definitions=["study", "diligence"], pitch=["0"], pos=["n", "vs"])
    ],
    "勉強する": [
        DictDefinition(word="勉強する", reading="べんきょうする", definitions=["to study"], pitch=["0"], pos=["vs"])
    ],
    "冒険": [
        DictDefinition(word="冒険", reading="ぼうけん", definitions=["adventure", "venture"], pitch=["0"], pos=["n", "vs"])
    ],
    "始める": [
        DictDefinition(word="始める", reading="はじめる", definitions=["to start", "to begin"], pitch=["0"], pos=["v1"])
    ],
    "始めよう": [
        DictDefinition(word="始める", reading="はじめる", definitions=["let's begin", "let's start (volitional form)"], pitch=["0"], pos=["v1"])
    ],
    "楽しい": [
        DictDefinition(word="楽しい", reading="たのしい", definitions=["fun", "enjoyable", "pleasant"], pitch=["3"], pos=["adj-i"])
    ],
    "ゲーム": [
        DictDefinition(word="ゲーム", reading="げーむ", definitions=["game"], pitch=["1"], pos=["n"])
    ],
    "設定": [
        DictDefinition(word="設定", reading="せってい", definitions=["settings", "configuration", "setup"], pitch=["0"], pos=["n", "vs"])
    ],
    "セーブ": [
        DictDefinition(word="セーブ", reading="せーぶ", definitions=["save (game)"], pitch=["1"], pos=["n", "vs"])
    ],
    "ロード": [
        DictDefinition(word="ロード", reading="ろーど", definitions=["load (game)"], pitch=["1"], pos=["n", "vs"])
    ],
}

class YomitanPyEngine(BaseDictEngine):
    def __init__(self, dict_dir: Path = DICTIONARIES_DIR):
        self.dict_dir = dict_dir
        # term -> list of DictDefinition
        self.entries: Dict[str, List[DictDefinition]] = {}
        # (term, reading) -> list of pitch strings
        self.pitches: Dict[str, List[str]] = {}
        self.loaded_dictionaries: List[str] = []
        
        # Load built-in fallback entries
        for term, defs in DEFAULT_VOCAB.items():
            self.entries[term] = list(defs)

        self.load_dictionaries()

    def load_dictionaries(self):
        if not self.dict_dir.exists():
            return

        # Check for zip archives and directories
        for path in self.dict_dir.glob("*.zip"):
            try:
                self._load_from_zip(path)
            except Exception as e:
                logger.error(f"Failed to load dictionary zip '{path.name}': {e}")

        for path in self.dict_dir.iterdir():
            if path.is_dir() and (path / "index.json").exists():
                try:
                    self._load_from_folder(path)
                except Exception as e:
                    logger.error(f"Failed to load dictionary folder '{path.name}': {e}")

        logger.info(f"Yomitan engine ready. {len(self.loaded_dictionaries)} dictionary(ies) active, {len(self.entries)} unique terms.")

    def _load_from_zip(self, zip_path: Path):
        with zipfile.ZipFile(zip_path, "r") as z:
            namelist = z.namelist()
            if "index.json" not in namelist:
                return
            index_data = json.loads(z.read("index.json").decode("utf-8"))
            title = index_data.get("title", zip_path.stem)

            # Load pitch banks if available
            for name in namelist:
                if name.startswith("pitch_bank_") and name.endswith(".json"):
                    data = json.loads(z.read(name).decode("utf-8"))
                    for row in data:
                        # [term, reading, pitch_obj]
                        if len(row) >= 3:
                            t, r, p_data = row[0], row[1], row[2]
                            pitch_val = []
                            if isinstance(p_data, dict):
                                pitch_val = [str(p.get("pitch", "")) for p in p_data.get("pitches", [])]
                            self.pitches[f"{t}:{r}"] = pitch_val

            # Load term banks
            count = 0
            for name in namelist:
                if name.startswith("term_bank_") and name.endswith(".json"):
                    data = json.loads(z.read(name).decode("utf-8"))
                    for row in data:
                        # [expression, reading, def_tags, rules, score, [glossary...], seq, term_tags]
                        if len(row) >= 6:
                            expr = row[0]
                            reading = row[1] if row[1] else expr
                            pos = [row[2]] if row[2] else []
                            glossary = row[5] if isinstance(row[5], list) else [str(row[5])]
                            
                            p = self.pitches.get(f"{expr}:{reading}", [])
                            
                            def_item = DictDefinition(
                                word=expr,
                                reading=reading,
                                definitions=[str(g) for g in glossary if isinstance(g, (str, int, float))],
                                pitch=p,
                                pos=pos,
                                dict_name=title
                            )
                            if expr not in self.entries:
                                self.entries[expr] = []
                            self.entries[expr].append(def_item)
                            count += 1

            self.loaded_dictionaries.append(title)
            logger.info(f"Loaded dictionary '{title}' from zip ({count} terms).")

    def _load_from_folder(self, folder_path: Path):
        index_file = folder_path / "index.json"
        with open(index_file, "r", encoding="utf-8") as f:
            index_data = json.load(f)
        title = index_data.get("title", folder_path.name)

        count = 0
        for bank_file in folder_path.glob("term_bank_*.json"):
            with open(bank_file, "r", encoding="utf-8") as f:
                data = json.load(f)
                for row in data:
                    if len(row) >= 6:
                        expr = row[0]
                        reading = row[1] if row[1] else expr
                        pos = [row[2]] if row[2] else []
                        glossary = row[5] if isinstance(row[5], list) else [str(row[5])]
                        def_item = DictDefinition(
                            word=expr,
                            reading=reading,
                            definitions=[str(g) for g in glossary if isinstance(g, (str, int, float))],
                            pitch=[],
                            pos=pos,
                            dict_name=title
                        )
                        if expr not in self.entries:
                            self.entries[expr] = []
                        self.entries[expr].append(def_item)
                        count += 1

        self.loaded_dictionaries.append(title)
        logger.info(f"Loaded dictionary '{title}' from folder ({count} terms).")

    def query_term(self, term: str) -> List[DictDefinition]:
        # Exact lookup
        if term in self.entries:
            return self.entries[term]
        # Deinflection lookup
        for candidate in deinflect(term):
            if candidate in self.entries:
                return self.entries[candidate]
        return []

    def lookup_sentence(self, sentence: str, max_scan_length: int = 16) -> List[TokenMatch]:
        tokens: List[TokenMatch] = []
        n = len(sentence)
        i = 0

        while i < n:
            matched = False
            # Check substrings from longest to shortest
            max_len = min(max_scan_length, n - i)
            for length in range(max_len, 0, -1):
                sub = sentence[i : i + length]
                defs = self.query_term(sub)
                if defs:
                    # Pick best entry
                    first = defs[0]
                    # Gather definitions across matched entries
                    all_defs = []
                    for d in defs[:3]:
                        all_defs.extend(d.definitions[:2])
                    
                    tokens.append(TokenMatch(
                        word=first.word,
                        reading=first.reading or first.word,
                        definitions=all_defs,
                        pitch=first.pitch,
                        char_range=[i, i + length]
                    ))
                    i += length  # Jump past matched word
                    matched = True
                    break
            
            if not matched:
                i += 1  # Advance by one character (punctuation, particle, or unindexed char)

        return tokens
