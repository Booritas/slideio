#!/usr/bin/env python3
# src/tests/main/http_fixture/test_http_server.py
# Local HTTP server with byte-range and fault-injection support for HttpStream tests.
import argparse
import os
import re
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

RANGE_RE = re.compile(r"bytes=(\d*)-(\d*)")

class Handler(BaseHTTPRequestHandler):
    server_version = "SlideIOTestHTTP/1.0"
    # HTTP/1.1 so the server keeps connections alive (Connection: keep-alive by
    # default). This lets the client reuse a single TCP connection across many
    # ranged GETs -- the behavior the connection-reuse tests verify. The server
    # is a ThreadingHTTPServer (see main), so an idle keep-alive connection
    # blocked in its own thread does not stall other connections (e.g. the
    # control channel).
    protocol_version = "HTTP/1.1"

    lock = threading.Lock()
    fail_count = 0          # how many initial GET requests to fail with 503
    served = 0              # count of successful file GETs (200 or 206)
    next_conn_id = 0        # monotonic id assigned to each new connection
    file_conn_ids = set()   # ids of connections that served at least one file GET

    def log_message(self, fmt, *args):
        pass  # quiet

    def send_empty(self, code):
        # Under HTTP/1.1 keep-alive every response must delimit its body. A
        # bodyless response with no Content-Length leaves the client waiting for
        # bytes that never come (the connection is not closed), so send an
        # explicit zero-length body.
        self.send_response(code)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def setup(self):
        # setup() runs once per connection (one handler instance per TCP
        # connection under ThreadingHTTPServer). Assign a stable id so file
        # GETs can be attributed to the connection that carried them.
        super().setup()
        with Handler.lock:
            Handler.next_conn_id += 1
            self.conn_id = Handler.next_conn_id

    def _resolve(self):
        # URL path -> filesystem path under root_dir
        rel = self.path.split("?", 1)[0].lstrip("/")
        return os.path.normpath(os.path.join(self.server.root_dir, rel))

    def do_HEAD(self):
        full = self._resolve()
        if not os.path.isfile(full):
            self.send_empty(404); return
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
        # Stats control endpoint: report file-GET count and distinct connection
        # count. Served on its own connection; never counted as a file GET.
        if self.path.startswith("/__control__/stats"):
            with Handler.lock:
                served = Handler.served
                conns = len(Handler.file_conn_ids)
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            payload = f"served={served}\nconnections={conns}\n".encode()
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload); return
        # Fault injection: while fail_count > 0, fail this GET with 503.
        with Handler.lock:
            if Handler.fail_count > 0:
                Handler.fail_count -= 1
                fail = True
            else:
                fail = False
        if fail:
            self.send_empty(503); return
        full = self._resolve()
        if not os.path.isfile(full):
            self.send_empty(404); return
        size = os.path.getsize(full)
        rng = self.headers.get("Range")
        # Fault injection: ignore_range=1 makes the server disregard the Range
        # header and return 200 with the full body, simulating a non-conforming
        # server. The client must reject this for any ranged request at offset>0.
        if "ignore_range=1" in self.path:
            rng = None
        if rng:
            m = RANGE_RE.match(rng)
            if not m:
                self.send_empty(416); return
            start = int(m.group(1)) if m.group(1) else 0
            end = int(m.group(2)) if m.group(2) else size - 1
            # S3 semantics: a range whose end is past EOF is clamped to the last
            # byte (still a 206), and only a start at/after EOF is unsatisfiable
            # (416). The folded size probe requests bytes=0-(blockSize-1) without
            # knowing the size yet, so it relies on this clamping for small files.
            if end >= size:
                end = size - 1
            if start > end or start >= size:
                self.send_empty(416); return
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
        with Handler.lock:
            Handler.served += 1
            Handler.file_conn_ids.add(self.conn_id)

    def do_POST(self):
        # Control endpoint: /__control__/fail-next/N sets fail_count = N
        if self.path.startswith("/__control__/fail-next/"):
            try:
                n = int(self.path.rsplit("/", 1)[-1])
            except ValueError:
                self.send_empty(404); return
            with Handler.lock:
                Handler.fail_count = n
            self.send_empty(204); return
        self.send_empty(404)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--port", type=int, default=0)
    args = ap.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    server.root_dir = args.root
    # Print the actual port on a single line so the test process can read it.
    print(f"PORT={server.server_address[1]}", flush=True)
    server.serve_forever()

if __name__ == "__main__":
    main()
