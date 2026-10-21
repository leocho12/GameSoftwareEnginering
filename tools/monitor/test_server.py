"""Monitor transport tests; does not build or launch the game."""
import json
import tempfile
import threading
import time
import unittest
from functools import partial
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import urlopen
import server


class MonitorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="monitor-test-", dir=Path(__file__).resolve().parent)
        self.path = Path(self.tmp.name) / "monitor.json"
        self.payload = {
            "schema_version": 1, "type": "snapshot", "sent_at_ms": time.time() * 1000,
            "npcs": [{"id": "town:0", "name": '윤서 "상인"\n테스트'}],
            "villages": [{"id": "a"}, {"id": "b"}], "counts": {}, "game": {},
        }
        self.write()

    def tearDown(self):
        assert self.path.parent.resolve().parent == Path(__file__).resolve().parent
        self.tmp.cleanup()

    def write(self):
        self.path.write_text(json.dumps(self.payload, ensure_ascii=False), encoding="utf-8")

    def test_fresh_unicode_and_multiple_villages(self):
        value = server.read_snapshot(self.path)
        self.assertFalse(value["transport"]["stale"])
        self.assertEqual(value["npcs"], self.payload["npcs"])
        self.assertEqual(len(value["villages"]), 2)

    def test_village_social_fields_are_transmitted_without_rewriting(self):
        village = {
            "id": "frontier", "population": 22,
            "atmosphere": {"label": "不安 / 불안", "reason": "실종 소식"},
            "tendencies": {"rescue_priority": 0.7, "autonomy": 0.6},
            "rumors": [{"text": "서로 다른 소식", "confidence": 0.45, "holders": 1,
                        "sources": ["미라"]}],
        }
        self.payload["villages"] = [village]
        self.write()
        self.assertEqual(server.read_snapshot(self.path)["villages"], [village])

    def test_old_snapshot_is_marked_stale(self):
        self.payload["sent_at_ms"] -= 15000
        self.write()
        self.assertTrue(server.read_snapshot(self.path)["transport"]["stale"])

    def test_invalid_and_missing(self):
        with self.assertRaises(FileNotFoundError):
            server.read_snapshot(None)
        for malformed in ('{"broken":', '[]', '{"schema_version":2}'):
            self.path.write_text(malformed)
            with self.assertRaises(ValueError):
                server.read_snapshot(self.path)

    def test_http_json_errors_and_fixed_routes(self):
        host = ThreadingHTTPServer(("127.0.0.1", 0), partial(server.Handler, snapshot=self.path))
        thread = threading.Thread(target=host.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{host.server_port}"
        try:
            with urlopen(base + "/api/snapshot", timeout=3) as response:
                self.assertIn("application/json", response.headers["Content-Type"])
                self.assertEqual(json.load(response)["npcs"], self.payload["npcs"])
            with urlopen(base + "/", timeout=3) as response:
                self.assertIn("월드 모니터", response.read().decode("utf-8-sig"))
            with self.assertRaises(HTTPError) as error:
                urlopen(base + "/../server.py", timeout=3)
            self.assertEqual(error.exception.code, 404)
            self.path.write_text("incomplete")
            with self.assertRaises(HTTPError) as error:
                urlopen(base + "/api/snapshot", timeout=3)
            self.assertEqual(error.exception.code, 503)
        finally:
            host.shutdown()
            host.server_close()
            thread.join(timeout=3)


if __name__ == "__main__":
    unittest.main()
