#!/usr/bin/env python3
"""Exercise the HTTP transport against a disposable loopback server."""
import http.server
import os
import subprocess
import sys
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_POST(self):
        data = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        self.send_response(201)
        self.end_headers()
        if self.path == "/empty":
            self.wfile.write(self.headers.get("Content-Length", "missing").encode() + b":" +
                             (b"present" if self.headers.get("X-Empty") == "" else b"missing"))
        else:
            self.wfile.write(data + b":" + self.headers.get("X-Test", "").encode())

    def do_GET(self):
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/unexpected")
            self.end_headers()
        elif self.path == "/slow":
            time.sleep(0.08)
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"ok")
        elif self.path == "/truncated":
            self.send_response(200)
            self.send_header("Content-Length", "100")
            self.end_headers()
            self.wfile.write(b"partial")
            self.close_connection = True
        elif self.path == "/large":
            self.send_response(200)
            self.end_headers()
            try:
                for _ in range(80):
                    self.wfile.write(b"x" * 65536)
            except (BrokenPipeError, ConnectionResetError):
                pass
        else:
            self.send_error(500)


server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    env = dict(os.environ, LUCID_TEST_HTTP_PORT=str(server.server_port))
    subprocess.run([sys.argv[1]], env=env, check=True)
finally:
    server.shutdown()
    server.server_close()
    thread.join()
