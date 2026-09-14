import json
import logging
import sqlite3
import time
import zipfile
from functools import lru_cache
from pathlib import Path
from typing import Dict, List, Optional, Tuple

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


class EntriesProxy(dict):
    """Transparent proxy that lets self.entries[term] or 'term in self.entries' work seamlessly."""
    def __init__(self, engine):
        super().__init__()
        self._engine = engine

    def __contains__(self, term: object) -> bool:
        if not isinstance(term, str):
            return False
        return bool(self._engine.query_term(term))

    def __getitem__(self, term: str) -> List[DictDefinition]:
        res = self._engine.query_term(term)
        if res:
            return res
        raise KeyError(term)

    def get(self, term: str, default=None):
        res = self._engine.query_term(term)
        return res if res else default

    def __len__(self) -> int:
        total = len(DEFAULT_VOCAB)
        for conn in self._engine.sqlite_conns:
            try:
                cur = conn.cursor()
                cur.execute("SELECT COUNT(*) FROM entries")
                row = cur.fetchone()
                if row:
                    total += row[0]
            except Exception:
                pass
        return total


class YomitanPyEngine(BaseDictEngine):
    def __init__(self, dict_dir: Path = DICTIONARIES_DIR):
        self.dict_dir = dict_dir
        self.sqlite_conns: List[sqlite3.Connection] = []
        self.loaded_filepaths: set = set()
        self.loaded_dictionaries: List[str] = []
        self.builtin_entries: Dict[str, List[DictDefinition]] = {k: list(v) for k, v in DEFAULT_VOCAB.items()}
        self.entries = EntriesProxy(self)
        
        self.load_dictionaries()

    def load_dictionaries(self):
        if not self.dict_dir.exists():
            return

        t0 = time.perf_counter()

        # 1. Process zip files (build SQLite cache if needed, then load via SQLite)
        for zip_path in self.dict_dir.glob("*.zip"):
            try:
                sqlite_path = self._build_sqlite_for_zip(zip_path)
                if sqlite_path and sqlite_path.exists():
                    self._load_sqlite(sqlite_path)
            except Exception as e:
                logger.error(f"Failed to load dictionary zip '{zip_path.name}': {e}")

        # 2. Process standalone .sqlite3 files (if not already opened from a .zip)
        for sq_path in self.dict_dir.glob("*.sqlite3"):
            if str(sq_path.resolve()) not in self.loaded_filepaths:
                try:
                    self._load_sqlite(sq_path)
                except Exception as e:
                    logger.error(f"Failed to load SQLite dictionary '{sq_path.name}': {e}")

        # 3. Process uncompressed directory dictionaries
        for path in self.dict_dir.iterdir():
            if path.is_dir() and (path / "index.json").exists():
                try:
                    self._load_from_folder(path)
                except Exception as e:
                    logger.error(f"Failed to load dictionary folder '{path.name}': {e}")

        elapsed = time.perf_counter() - t0
        logger.info(
            f"Yomitan engine ready in {elapsed*1000:.1f} ms. "
            f"{len(self.loaded_dictionaries)} dictionary(ies) active: {', '.join(self.loaded_dictionaries)}."
        )

    def _load_sqlite(self, sqlite_path: Path):
        resolved_path = str(sqlite_path.resolve())
        if resolved_path in self.loaded_filepaths:
            return

        conn = sqlite3.connect(str(sqlite_path), check_same_thread=False)
        conn.execute("PRAGMA synchronous = OFF")
        conn.execute("PRAGMA journal_mode = OFF")
        conn.execute("PRAGMA mmap_size = 268435456")  # 256 MB memory-mapped I/O
        conn.execute("PRAGMA cache_size = -64000")     # 64 MB page cache
        conn.execute("PRAGMA temp_store = MEMORY")
        self.sqlite_conns.append(conn)
        self.loaded_filepaths.add(resolved_path)

        # Retrieve dictionary title
        try:
            cur = conn.cursor()
            cur.execute("SELECT DISTINCT dict_name FROM entries LIMIT 1")
            row = cur.fetchone()
            title = row[0] if row and row[0] else sqlite_path.stem
        except Exception:
            title = sqlite_path.stem

        if title not in self.loaded_dictionaries:
            self.loaded_dictionaries.append(title)
        logger.info(f"Loaded SQLite dictionary '{title}' ({sqlite_path.stat().st_size / (1024*1024):.1f} MB).")

    def _build_sqlite_for_zip(self, zip_path: Path) -> Path:
        sqlite_path = zip_path.with_suffix(".sqlite3")
        if sqlite_path.exists():
            if sqlite_path.stat().st_mtime >= zip_path.stat().st_mtime:
                return sqlite_path
            else:
                logger.info(f"Zip '{zip_path.name}' is newer than SQLite cache. Rebuilding...")
                try:
                    sqlite_path.unlink()
                except Exception:
                    pass

        logger.info(f"Building fast SQLite cache for '{zip_path.name}'...")
        t0 = time.perf_counter()

        conn = sqlite3.connect(str(sqlite_path))
        conn.execute("PRAGMA synchronous = OFF")
        conn.execute("PRAGMA journal_mode = MEMORY")
        conn.execute("""
        CREATE TABLE IF NOT EXISTS entries (
            term TEXT NOT NULL,
            reading TEXT,
            definitions TEXT NOT NULL,
            pitch TEXT,
            dict_name TEXT
        )
        """)

        pitches: Dict[str, List[str]] = {}
        rows_to_insert = []

        with zipfile.ZipFile(zip_path, "r") as z:
            namelist = z.namelist()
            if "index.json" not in namelist:
                return sqlite_path
            index_data = json.loads(z.read("index.json").decode("utf-8"))
            title = index_data.get("title", zip_path.stem)

            # Load pitch banks if available
            for name in namelist:
                if name.startswith("pitch_bank_") and name.endswith(".json"):
                    data = json.loads(z.read(name).decode("utf-8"))
                    for row in data:
                        if len(row) >= 3:
                            t, r, p_data = row[0], row[1], row[2]
                            pitch_val = []
                            if isinstance(p_data, dict):
                                pitch_val = [str(p.get("pitch", "")) for p in p_data.get("pitches", [])]
                            pitches[f"{t}:{r}"] = pitch_val

            # Load term banks
            count = 0
            for name in namelist:
                if name.startswith("term_bank_") and name.endswith(".json"):
                    data = json.loads(z.read(name).decode("utf-8"))
                    for row in data:
                        if len(row) >= 6:
                            expr = row[0]
                            reading = row[1] if row[1] else expr
                            glossary = row[5] if isinstance(row[5], list) else [row[5]]
                            defs = self._parse_glossary(glossary)
                            p = pitches.get(f"{expr}:{reading}", [])

                            rows_to_insert.append((
                                expr,
                                reading,
                                json.dumps(defs, ensure_ascii=False),
                                json.dumps(p, ensure_ascii=False),
                                title
                            ))
                            count += 1

        conn.executemany("INSERT INTO entries VALUES (?, ?, ?, ?, ?)", rows_to_insert)
        conn.execute("CREATE INDEX IF NOT EXISTS idx_entries_term ON entries(term)")
        conn.commit()
        conn.close()

        dt = time.perf_counter() - t0
        logger.info(f"Built SQLite cache in {dt:.2f} s ({count} terms, {sqlite_path.stat().st_size / (1024*1024):.1f} MB).")
        return sqlite_path

    @staticmethod
    def _parse_glossary(raw_glossary) -> List[str]:
        """Extract definition strings from Yomitan glossary items, supporting structured content."""
        definitions = []

        def extract_text(node) -> str:
            if isinstance(node, str):
                return node.strip()
            if isinstance(node, (int, float)):
                return str(node)
            if isinstance(node, list):
                return " ".join(filter(None, [extract_text(x) for x in node]))
            if isinstance(node, dict):
                if node.get("tag") == "rt":  # omit ruby furigana annotations
                    return ""
                data = node.get("data")
                if isinstance(data, dict) and data.get("content") in ("extra-info", "attribution", "xref"):
                    return ""
                return extract_text(node.get("content", ""))
            return ""

        def find_glossary_items(node):
            if isinstance(node, dict):
                data = node.get("data")
                if isinstance(data, dict) and data.get("content") == "glossary":
                    c = node.get("content", [])
                    if isinstance(c, list):
                        for li in c:
                            t = extract_text(li)
                            if t:
                                yield t
                    else:
                        t = extract_text(c)
                        if t:
                            yield t
                    return
                if "content" in node:
                    yield from find_glossary_items(node["content"])
            elif isinstance(node, list):
                for item in node:
                    yield from find_glossary_items(item)

        for g in raw_glossary:
            if isinstance(g, (str, int, float)):
                definitions.append(str(g))
            elif isinstance(g, dict):
                found = list(find_glossary_items(g.get("content", [])))
                if found:
                    definitions.extend(found)
                else:
                    fallback = extract_text(g.get("content", []))
                    if fallback:
                        definitions.append(fallback)

        return definitions

    def _load_from_folder(self, folder_path: Path):
        # Fallback for uncompressed folders
        index_file = folder_path / "index.json"
        with open(index_file, "r", encoding="utf-8") as f:
            index_data = json.load(f)
        title = index_data.get("title", folder_path.name)
        if title not in self.loaded_dictionaries:
            self.loaded_dictionaries.append(title)

    @lru_cache(maxsize=32768)
    def _query_term_cached(self, term: str) -> Tuple[DictDefinition, ...]:
        # 1. Built-in entries exact match
        if term in self.builtin_entries:
            return tuple(self.builtin_entries[term])

        # 2. SQLite exact match across loaded databases
        for conn in self.sqlite_conns:
            cur = conn.cursor()
            cur.execute("SELECT term, reading, definitions, pitch, dict_name FROM entries WHERE term = ?", (term,))
            rows = cur.fetchall()
            if rows:
                return tuple(
                    DictDefinition(
                        word=r[0],
                        reading=r[1] or r[0],
                        definitions=json.loads(r[2]),
                        pitch=json.loads(r[3]),
                        dict_name=r[4]
                    )
                    for r in rows
                )

        # 3. Deinflection candidates
        for candidate in deinflect(term):
            if candidate in self.builtin_entries:
                return tuple(self.builtin_entries[candidate])
            for conn in self.sqlite_conns:
                cur = conn.cursor()
                cur.execute("SELECT term, reading, definitions, pitch, dict_name FROM entries WHERE term = ?", (candidate,))
                rows = cur.fetchall()
                if rows:
                    return tuple(
                        DictDefinition(
                            word=r[0],
                            reading=r[1] or r[0],
                            definitions=json.loads(r[2]),
                            pitch=json.loads(r[3]),
                            dict_name=r[4]
                        )
                        for r in rows
                    )

        return ()

    def query_term(self, term: str) -> List[DictDefinition]:
        return list(self._query_term_cached(term))

    def lookup_sentence(self, sentence: str, max_scan_length: int = 16) -> List[TokenMatch]:
        tokens: List[TokenMatch] = []
        n = len(sentence)
        i = 0

        while i < n:
            matched = False
            max_len = min(max_scan_length, n - i)
            for length in range(max_len, 0, -1):
                sub = sentence[i : i + length]
                defs = self.query_term(sub)
                if defs:
                    first = defs[0]
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
                    i += length
                    matched = True
                    break

            if not matched:
                i += 1

        return tokens
