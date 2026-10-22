import json
import http.client
import socket
import sys
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import outiva_status


class FakeUpstream(BaseHTTPRequestHandler):
    payload = {"status": "available", "players": 7}
    delay = 0
    calls = 0

    def do_GET(self):
        type(self).calls += 1
        time.sleep(self.delay)
        body = json.dumps(self.payload).encode()
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except BrokenPipeError:
            pass

    def log_message(self, *_args):
        pass


class AdapterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.upstream = ThreadingHTTPServer(("127.0.0.1", 0), FakeUpstream)
        cls.thread = threading.Thread(target=cls.upstream.serve_forever, daemon=True)
        cls.thread.start()
        outiva_status.StatusHandler.source = type(
            "FixedSource", (), {"current": lambda self: (200, {"status": "available", "players": 2})}
        )()
        cls.public = ThreadingHTTPServer(("127.0.0.1", 0), outiva_status.StatusHandler)
        cls.public_thread = threading.Thread(target=cls.public.serve_forever, daemon=True)
        cls.public_thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.upstream.shutdown()
        cls.upstream.server_close()
        cls.public.shutdown()
        cls.public.server_close()

    def source(self, timeout=0.2):
        return outiva_status.StatusSource(port=self.upstream.server_port, timeout=timeout, ttl=0)

    def test_server_up(self):
        FakeUpstream.delay = 0
        FakeUpstream.payload = {"status": "available", "players": 7}
        self.assertEqual(self.source().current(), (200, {"status": "available", "players": 7}))

    def test_server_down_is_degraded(self):
        sock = socket.socket()
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
        sock.close()
        source = outiva_status.StatusSource(port=port, timeout=0.1, ttl=0)
        self.assertEqual(source.current(), (200, {"status": "degraded", "players": 0}))

    def test_timeout_is_unknown(self):
        FakeUpstream.delay = 0.2
        self.assertEqual(self.source(timeout=0.02).current(), (503, {"status": "unknown"}))

    def test_upstream_fields_are_not_exposed(self):
        FakeUpstream.delay = 0
        FakeUpstream.payload = {
            "status": "available", "players": 3, "names": ["private"], "city": "private"
        }
        status, payload = self.source().current()
        self.assertEqual(status, 200)
        self.assertEqual(payload, {"status": "available", "players": 3})

    def test_invalid_count_is_unknown(self):
        FakeUpstream.payload = {"status": "available", "players": "3", "name": "private"}
        self.assertEqual(self.source().current(), (503, {"status": "unknown"}))

    def test_queries_are_cached(self):
        FakeUpstream.delay = 0
        FakeUpstream.payload = {"status": "available", "players": 4}
        FakeUpstream.calls = 0
        source = outiva_status.StatusSource(port=self.upstream.server_port, timeout=0.2, ttl=15)
        self.assertEqual(source.current(), (200, {"status": "available", "players": 4}))
        self.assertEqual(source.current(), (200, {"status": "available", "players": 4}))
        self.assertEqual(FakeUpstream.calls, 1)

    def test_public_contract_headers_and_body(self):
        connection = http.client.HTTPConnection("127.0.0.1", self.public.server_port, timeout=0.2)
        connection.request("GET", "/outiva-status.json", headers={"Origin": "https://outiva.com"})
        response = connection.getresponse()
        self.assertEqual(response.status, 200)
        self.assertEqual(response.getheader("Content-Type"), "application/json")
        self.assertEqual(response.getheader("Cache-Control"), "public, max-age=15")
        self.assertEqual(response.getheader("Access-Control-Allow-Origin"), "https://outiva.com")
        self.assertEqual(response.read(), b'{"status":"available","players":2}')
        connection.close()


if __name__ == "__main__":
    unittest.main()
