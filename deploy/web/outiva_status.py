#!/usr/bin/env python3
"""Cached, privacy-preserving public adapter for Battle City's native status."""

import http.client
import json
import os
import socket
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PUBLIC_ORIGIN = "https://outiva.com"


class StatusSource:
    def __init__(self, host="127.0.0.1", port=9682, timeout=1.0, ttl=15.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.ttl = ttl
        self._lock = threading.Lock()
        self._cached = None
        self._cached_at = 0.0

    def current(self):
        now = time.monotonic()
        with self._lock:
            if self._cached is not None and now - self._cached_at < self.ttl:
                return self._cached
            result = self._query()
            self._cached = result
            self._cached_at = now
            return result

    def _query(self):
        connection = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        try:
            connection.request("GET", "/health")
            response = connection.getresponse()
            raw = response.read(1024)
            if response.status != 200 or len(raw) >= 1024:
                return 503, {"status": "unknown"}
            payload = json.loads(raw.decode("utf-8"))
            # Build a fresh object from a strict allow-list. Never pass upstream fields through.
            players = payload.get("players")
            if payload.get("status") != "available" or type(players) is not int or not 0 <= players <= 10000:
                return 503, {"status": "unknown"}
            return 200, {"status": "available", "players": players}
        except ConnectionRefusedError:
            return 200, {"status": "degraded", "players": 0}
        except (http.client.HTTPException, socket.timeout, TimeoutError, OSError, ValueError, UnicodeError):
            return 503, {"status": "unknown"}
        finally:
            connection.close()


class StatusHandler(BaseHTTPRequestHandler):
    source = None

    def do_GET(self):
        if self.path != "/outiva-status.json":
            self.send_error(404)
            return
        status, payload = self.source.current()
        body = json.dumps(payload, separators=(",", ":")).encode("ascii")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "public, max-age=15")
        self.send_header("Access-Control-Allow-Origin", PUBLIC_ORIGIN)
        self.send_header("Vary", "Origin")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, message, *args):
        print("outiva-status: " + message % args, flush=True)


def main():
    listen_port = int(os.environ.get("BC_OUTIVA_STATUS_PORT", "9683"))
    upstream_port = int(os.environ.get("BC_STATUS_PORT", "9682"))
    timeout = float(os.environ.get("BC_STATUS_TIMEOUT", "1"))
    ttl = float(os.environ.get("BC_STATUS_CACHE_SECONDS", "15"))
    StatusHandler.source = StatusSource(port=upstream_port, timeout=timeout, ttl=ttl)
    server = ThreadingHTTPServer(("127.0.0.1", listen_port), StatusHandler)
    server.serve_forever()


if __name__ == "__main__":
    main()
