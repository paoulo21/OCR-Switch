import io
import time
import json
import zipfile
import sqlite3
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from server.dict.yomitan_py import YomitanPyEngine

def build_sqlite_for_zip(zip_path: Path) -> Path:
    sqlite_path = zip_path.with_suffix(".sqlite3")
    if sqlite_path.exists():
        if sqlite_path.stat().st_mtime >= zip_path.stat().st_mtime:
            print(f"SQLite cache '{sqlite_path.name}' is up to date.")
            return sqlite_path
        else:
            print("Zip is newer than SQLite cache, rebuilding...")
            sqlite_path.unlink()
            
    print(f"Building SQLite cache for '{zip_path.name}'...")
    t0 = time.perf_counter()
    
    conn = sqlite3.connect(sqlite_path)
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
    
    pitches = {}
    rows_to_insert = []
    
    with zipfile.ZipFile(zip_path, "r") as z:
        namelist = z.namelist()
        if "index.json" not in namelist:
            return sqlite_path
        index_data = json.loads(z.read("index.json").decode("utf-8"))
        title = index_data.get("title", zip_path.stem)
        
        # 1. Pitch banks
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
                        
        # 2. Term banks
        count = 0
        for name in namelist:
            if name.startswith("term_bank_") and name.endswith(".json"):
                data = json.loads(z.read(name).decode("utf-8"))
                for row in data:
                    if len(row) >= 6:
                        expr = row[0]
                        reading = row[1] if row[1] else expr
                        pos = [row[2]] if row[2] else []
                        glossary = row[5] if isinstance(row[5], list) else [row[5]]
                        defs = YomitanPyEngine._parse_glossary(glossary)
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
    print(f"Built SQLite cache in {dt:.2f} s ({count} terms, {sqlite_path.stat().st_size / (1024*1024):.1f} MB).")
    return sqlite_path

if __name__ == "__main__":
    zip_p = Path("data/dictionaries/jitendex-yomitan.zip")
    if zip_p.exists():
        build_sqlite_for_zip(zip_p)
