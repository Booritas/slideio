#!/usr/bin/env python3
# src/tests/main/http_fixture/test_http_server.py
# Local HTTP server with byte-range and fault-injection support for HttpStream tests.
import argparse
import os
import re
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer

RANGE_RE = re.compile(r"bytes=(\d*)-(\d*)")

class Handler(BaseHTTPRequestHandler):
    server_version = "SlideIOTestHTTP/1.0"
    fail_count = 0          # how many initial GET requests to fail with 503
    served = 0              # count of successful file GETs (200 or 206)

    def log_message(self, fmt, *args):
        pass  # quiet

    def _resolve(self):
        # URL path -> filesystem path under root_dir
        rel = self.path.split("?", 1)[0].lstrip("/")
        return os.path.normpath(os.path.join(self.server.root_dir, rel))

    def do_HEAD(self):
        full = self._resolve()
        if not os.path.isfile(full):
            self.send_response(404); self.end_headers(); return
        # If client passes nohead=1, return 200 with no Content-Length and no
        # Accept-Ranges. This forces the client's Content-Range fallback path.
        if "nohead=1" in self.path:
            self.send_response(200); self.end_headers(); return
        size = os.path.getsize(full)
        self.send_response(200)
        self.send_header("Content-Length", str(size))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()

    def do_GET(self):
        # Stats control endpoint: report the number of successful file GETs.
        if self.path.startswith("/__control__/stats"):
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            payload = f"served={Handler.served}\n".encode()
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload); return
        # Fault injection: while fail_count > 0, fail this GET with 503.
        if Handler.fail_count > 0:
            Handler.fail_count -= 1
            self.send_response(503); self.end_headers(); return
        full = self._resolve()
        if not os.path.isfile(full):
            self.send_response(404); self.end_headers(); return
        size = os.path.getsize(full)
        rng = self.headers.get("Range")
        if rng:
            m = RANGE_RE.match(rng)
            if not m:
                self.send_response(416); self.end_headers(); return
            start = int(m.group(1)) if m.group(1) else 0
            end = int(m.group(2)) if m.group(2) else size - 1
            if start > end or end >= size:
                self.send_response(416); self.end_headers(); return
            length = end - start + 1
            self.send_response(206)
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
            self.send_header("Content-Length", str(length))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            with open(full, "rb") as f:
                f.seek(start); self.wfile.write(f.read(length))
        else:
            self.send_response(200)
            self.send_header("Content-Length", str(size))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            with open(full, "rb") as f:
                self.wfile.write(f.read())
        Handler.served += 1

    def do_POST(self):
        # Control endpoint: /__control__/fail-next/N sets fail_count = N
        if self.path.startswith("/__control__/fail-next/"):
            try:
                Handler.fail_count = int(self.path.rsplit("/", 1)[-1])
                self.send_response(204); self.end_headers(); return
            except ValueError:
                pass
        self.send_response(404); self.end_headers()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--port", type=int, default=0)
    args = ap.parse_args()
    server = HTTPServer(("127.0.0.1", args.port), Handler)
    server.root_dir = args.root
    # Print the actual port on a single line so the test process can read it.
    print(f"PORT={server.server_address[1]}", flush=True)
    server.serve_forever()

if __name__ == "__main__":
    main()
