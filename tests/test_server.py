import io
import unittest
from PIL import Image
from fastapi.testclient import TestClient

from server.main import app
from server.dict.deinflector import deinflect
from server.dict.yomitan_py import YomitanPyEngine
from server.anki.anki_client import AnkiExporter
from server.ocr.mock import MockOCREngine

class TestServerComponents(unittest.TestCase):
    def setUp(self):
        self.client = TestClient(app)

    def test_deinflector(self):
        # Past tense
        candidates = deinflect("食べた")
        self.assertIn("食べる", candidates)

        # Te-form
        candidates_te = deinflect("食べて")
        self.assertIn("食べる", candidates_te)

        # Masu-form
        candidates_masu = deinflect("始めます")
        self.assertIn("始める", candidates_masu)

        # I-adjective past
        candidates_adj = deinflect("楽しかった")
        self.assertIn("楽しい", candidates_adj)

    def test_yomitan_py_engine(self):
        engine = YomitanPyEngine()
        
        # Exact lookup
        res = engine.query_term("日本語")
        self.assertTrue(len(res) > 0)
        self.assertEqual(res[0].reading, "にほんご")

        # Deinflected lookup
        res_deinflect = engine.query_term("勉強した")
        self.assertTrue(len(res_deinflect) > 0)

        # Sentence lookup
        tokens = engine.lookup_sentence("日本語を勉強する")
        words = [t.word for t in tokens]
        self.assertIn("日本語", words)
        self.assertTrue("勉強する" in words or "勉強" in words)

    def test_ocr_mock(self):
        engine = MockOCREngine()
        # Generate small dummy JPEG
        img = Image.new("RGB", (1280, 720), color=(30, 30, 30))
        buf = io.BytesIO()
        img.save(buf, format="JPEG")
        raw_bytes = buf.getvalue()

        blocks = engine.detect_and_recognize(raw_bytes)
        self.assertTrue(len(blocks) > 0)
        self.assertEqual(len(blocks[0].box), 4)

    def test_anki_export(self):
        exporter = AnkiExporter()
        res = exporter.export(
            word="日本語",
            reading="にほんご",
            definitions=["Japanese language"],
            sentence="日本語を勉強する",
            image_bytes=None
        )
        self.assertTrue(res["success"])
        self.assertTrue(res["saved_to_csv"])

    def test_api_status(self):
        response = self.client.get("/api/status")
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertEqual(data["status"], "online")
        self.assertIn("ocr_engine", data)
        self.assertIn("dict_engine", data)

    def test_api_ocr_endpoint(self):
        from tests.profile_scan import create_sample_game_screenshot
        raw_bytes = create_sample_game_screenshot()

        response = self.client.post(
            "/api/ocr",
            content=raw_bytes,
            headers={"Content-Type": "image/jpeg"}
        )
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertEqual(data["status"], "ok")
        self.assertTrue("boxes" in data)
        self.assertTrue(len(data["boxes"]) > 0)

        first_box = data["boxes"][0]
        self.assertIn("box", first_box)
        self.assertIn("text", first_box)
        self.assertIn("tokens", first_box)

    def test_api_anki_endpoint(self):
        payload = {
            "word": "冒険",
            "reading": "ぼうけん",
            "definitions": ["adventure", "venture"],
            "sentence": "冒険を始めよう！"
        }
        response = self.client.post("/api/anki", json=payload)
        self.assertEqual(response.status_code, 200)
        data = response.json()
        self.assertTrue(data["success"])
        self.assertEqual(data["word"], "冒険")

if __name__ == "__main__":
    unittest.main()
