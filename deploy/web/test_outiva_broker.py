#!/usr/bin/env python3
"""Tests for outiva_broker.py. Signs test tokens with a throwaway RSA key made by the openssl command line tool."""
import base64
import hashlib
import hmac
import json
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

import outiva_broker as ob

ISSUER = "https://accounts.outiva.com/realms/outiva"


def make_key(directory):
    pem = Path(directory, "key.pem")
    subprocess.run(["openssl", "genrsa", "-out", str(pem), "2048"], check=True, capture_output=True)
    text = subprocess.run(["openssl", "rsa", "-in", str(pem), "-noout", "-modulus"], check=True, capture_output=True, text=True).stdout
    n = int(text.strip().split("=")[1], 16)
    return pem, n


def sign(pem, head, body):
    out = subprocess.run(["openssl", "dgst", "-sha256", "-sign", str(pem)], input=f"{head}.{body}".encode(), check=True, capture_output=True).stdout
    return ob.b64url(out)


class BrokerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        cls.pem, cls.n = make_key(cls.dir.name)
        cls.jwk = {"keys": [{"kid": "k1", "kty": "RSA", "n": ob.b64url(cls.n.to_bytes(256, "big")), "e": ob.b64url((65537).to_bytes(3, "big"))}]}

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def token(self, **claims):
        base = {"iss": ISSUER, "aud": "battlecity-web", "sub": "user-1", "exp": time.time() + 300, "callsign": "Maverick"}
        base.update(claims)
        head = ob.b64url(json.dumps({"alg": "RS256", "kid": "k1"}).encode())
        body = ob.b64url(json.dumps(base).encode())
        return f"{head}.{body}.{sign(self.pem, head, body)}"

    def broker(self, **kw):
        return ob.Broker("a-long-enough-secret-value", ISSUER, ["battlecity-web"], ob.Keys("unused", fetch=lambda: self.jwk), **kw)

    def test_a_good_token_is_accepted(self):
        claims = self.broker().verify_id_token(self.token())
        self.assertEqual(claims["sub"], "user-1")

    def test_bad_tokens_are_refused(self):
        b = self.broker()
        self.assertIsNone(b.verify_id_token(self.token(exp=time.time() - 1)), "expired")
        self.assertIsNone(b.verify_id_token(self.token(iss="https://evil.example")), "wrong issuer")
        self.assertIsNone(b.verify_id_token(self.token(aud="another-client")), "wrong audience")
        self.assertIsNone(b.verify_id_token(self.token(sub="")), "no subject")
        good = self.token()
        head, body, sig = good.split(".")
        forged = ob.b64url(json.dumps({"iss": ISSUER, "aud": "battlecity-web", "sub": "someone-else", "exp": time.time() + 300}).encode())
        self.assertIsNone(b.verify_id_token(f"{head}.{forged}.{sig}"), "signature does not cover a different body")
        none = ob.b64url(json.dumps({"alg": "none"}).encode())
        self.assertIsNone(b.verify_id_token(f"{none}.{body}."), "alg none")
        self.assertIsNone(b.verify_id_token("a.b"), "not a token")
        self.assertIsNone(b.verify_id_token(""), "empty")

    def test_a_token_signed_by_another_key_is_refused(self):
        other_pem, _ = make_key(self.dir.name)
        head = ob.b64url(json.dumps({"alg": "RS256", "kid": "k1"}).encode())
        body = ob.b64url(json.dumps({"iss": ISSUER, "aud": "battlecity-web", "sub": "u", "exp": time.time() + 300}).encode())
        self.assertIsNone(self.broker().verify_id_token(f"{head}.{body}.{sign(other_pem, head, body)}"))

    def test_the_ticket_matches_what_the_game_server_checks(self):
        b = self.broker(clock=lambda: 1000)
        ticket, callsign = b.ticket_for({"sub": "user-1", "callsign": "Mav<script>erick"})
        self.assertEqual(callsign, "Mavscripterick")
        self.assertTrue(ticket.startswith("v1."))
        _, payload, mac = ticket.split(".")
        expected = hmac.new(b"a-long-enough-secret-value", f"v1.{payload}".encode(), hashlib.sha256).digest()
        self.assertEqual(ob.b64url(expected), mac)
        sub, cs, expires, tid = ob.b64url_decode(payload).decode().split("|")
        self.assertEqual((sub, cs, expires), ("user-1", "Mavscripterick", "1060"))
        self.assertGreater(len(tid), 6)
        self.assertNotEqual(b.ticket_for({"sub": "user-1"})[0], b.ticket_for({"sub": "user-1"})[0], "every ticket has its own id")

    def test_a_short_secret_is_refused(self):
        with self.assertRaises(ValueError):
            ob.Broker("short", ISSUER, ["battlecity-web"], None)

    def test_rate_limit(self):
        now = [0]
        limit = ob.RateLimit(limit=3, clock=lambda: now[0])
        self.assertEqual([limit.allow("a") for _ in range(4)], [True, True, True, False])
        self.assertTrue(limit.allow("b"))
        now[0] = 61
        self.assertTrue(limit.allow("a"))

    def test_unreachable_keys_are_reported_not_treated_as_a_bad_token(self):
        def down():
            raise OSError("Name or service not known")
        b = ob.Broker("a-long-enough-secret-value", ISSUER, ["battlecity-web"], ob.Keys("unused", fetch=down))
        with self.assertRaises(ob.KeysUnavailable):
            b.verify_id_token(self.token())

    def test_the_encryption_only_key_is_ignored(self):
        jwk = {"keys": [dict(self.jwk["keys"][0], kid="enc1", use="enc"), dict(self.jwk["keys"][0], use="sig")]}
        keys = ob.Keys("unused", fetch=lambda: jwk)
        self.assertIsNone(keys.get("enc1"))
        self.assertIsNotNone(keys.get("k1"))

    def test_unknown_key_ids_trigger_a_refetch_but_not_a_flood(self):
        calls = []
        clock = [1000.0]
        keys = ob.Keys("unused", fetch=lambda: (calls.append(1), self.jwk)[1], clock=lambda: clock[0])
        self.assertIsNotNone(keys.get("k1"))
        self.assertIsNone(keys.get("unknown"))
        self.assertIsNone(keys.get("unknown"))
        self.assertEqual(len(calls), 1, "an unknown key id does not refetch more than every 30 seconds")
        clock[0] += 31
        keys.get("unknown")
        self.assertEqual(len(calls), 2)


if __name__ == "__main__":
    unittest.main()
