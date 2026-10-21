"""Loopback-only JSON dashboard; no game control API."""
import argparse
import json
import time
import webbrowser
from functools import partial
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[2]
UI = Path(__file__).with_name("index.html")
MAX_BYTES = 2 * 1024 * 1024


def find_snapshot(explicit=None):
    if explicit:
        return Path(explicit).resolve()
    candidates = list((ROOT / "BaseProject_SE").glob("**/Profiles/monitor.json"))
    return max(candidates, key=lambda p: p.stat().st_mtime_ns, default=None)


def read_snapshot(path):
    if path is None:
        raise FileNotFoundError("게임 상태 대기 중입니다. 새 버전으로 빌드한 게임을 실행하세요.")
    with path.open("rb") as stream:
        raw = stream.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise ValueError("상태 파일 크기 제한 초과")
    value = json.loads(raw)
    if not isinstance(value, dict) or value.get("schema_version") != 1 or value.get("type") != "snapshot":
        raise ValueError("지원하지 않는 모니터 메시지입니다.")
    for key, kind in (("npcs", list), ("villages", list), ("counts", dict), ("game", dict)):
        if not isinstance(value.get(key), kind):
            raise ValueError(f"잘못된 메시지 필드: {key}")
    stamp = value.get("sent_at_ms")
    if not isinstance(stamp, (int, float)) or isinstance(stamp, bool):
        raise ValueError("메시지 생성 시각이 없습니다.")
    age = max(0, int(time.time() * 1000 - stamp))
    value["transport"] = {"age_ms": age, "stale": age > 5000, "source": str(path),
                          "received_at_ms": int(time.time() * 1000)}
    return value


class Handler(BaseHTTPRequestHandler):
    def __init__(self, *args, snapshot=None, **kwargs):
        self.snapshot = snapshot
        super().__init__(*args, **kwargs)

    def log_message(self, *_):
        pass

    def send(self, status, payload, content_type="application/json; charset=utf-8"):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'")
        self.end_headers()
        try:
            self.wfile.write(payload)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_GET(self):
        route = urlsplit(self.path).path
        if route in ("/", "/index.html"):
            self.send(200, UI.read_bytes(), "text/html; charset=utf-8")
        elif route == "/api/snapshot":
            try:
                data = read_snapshot(find_snapshot(self.snapshot))
                self.send(200, json.dumps(data, ensure_ascii=False, allow_nan=False).encode("utf-8"))
            except (OSError, ValueError, TypeError) as error:
                self.send(503, json.dumps({"error": str(error)}, ensure_ascii=False).encode("utf-8"))
        else:
            self.send(404, b'{"error":"Not found"}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--snapshot", type=Path, help="실행 파일 옆 Profiles/monitor.json")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--open", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("port must be 1..65535")
    try:
        server = ThreadingHTTPServer(("127.0.0.1", args.port), partial(Handler, snapshot=args.snapshot))
    except OSError as error:
        parser.exit(1, f"서버 시작 실패: {error}\n다른 --port 값을 사용하세요.\n")
    url = f"http://127.0.0.1:{server.server_port}/"
    print(f"NPC monitor: {url}\nCtrl+C: stop")
    if args.open:
        webbrowser.open_new(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
