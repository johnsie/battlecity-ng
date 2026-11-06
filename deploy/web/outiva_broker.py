#!/usr/bin/env python3
"""Outiva login broker for Battle City.

Takes a person's Outiva ID token (RS256, issued by accounts.outiva.com to the Battle City client), verifies it against Keycloak's published
keys, and answers with a short-lived, single-use login ticket for the game server (see server/OutivaTicket.h). The game server never sees an
Outiva token, and this service never sees a Battle City password: there are none.

    POST /outiva/ticket   {"id_token": "..."}   ->   200 {"ticket": "v1....", "callsign": "..."}   |   400/401/429

Listens on loopback only; Apache proxies /outiva/ticket to it. Standard library only (the RSA signature check is done here, so the
host needs no extra packages).

Environment:
    BC_BROKER_PORT            listen port (default 9684)
    BC_TICKET_SECRET_FILE     file holding the secret shared with the game server (at least 16 characters)
    BC_OUTIVA_ISSUER          default https://accounts.outiva.com/realms/outiva
    BC_OUTIVA_AUDIENCE        the Keycloak client the tokens are for (default battlecity-web, comma separated for more)
    BC_OUTIVA_JWKS_URL        where the signing keys are (default <issuer>/protocol/openid-connect/certs). On the Outiva host use Keycloak's
                              local address, http://127.0.0.1:8180/realms/outiva/protocol/openid-connect/certs: the host cannot look up its
                              own public name. Tokens are still checked against the public issuer.
"""
import base64
import hashlib
import hmac
import json
import os
import secrets
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TICKET_SECONDS = 60
SHA256_PREFIX = bytes.fromhex("3031300d060960864801650304020105000420")   # DigestInfo for SHA-256 (RFC 8017 section 9.2)
JWKS_SECONDS = 600


def b64url_decode(text):
    return base64.urlsafe_b64decode(text + "=" * (-len(text) % 4))


def b64url(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode()


def rsa_pkcs1_sha256_verify(n, e, message, signature):
    """RSASSA-PKCS1-v1_5 with SHA-256, checked by recomputing the padded block and comparing it exactly."""
    size = (n.bit_length() + 7) // 8
    if len(signature) != size:
        return False
    value = int.from_bytes(signature, "big")
    if value >= n:
        return False
    block = pow(value, e, n).to_bytes(size, "big")
    digest = hashlib.sha256(message).digest()
    suffix = b"\x00" + SHA256_PREFIX + digest
    expected = b"\x00\x01" + b"\xff" * (size - len(suffix) - 2) + suffix
    return hmac.compare_digest(block, expected)


class KeysUnavailable(Exception):
    """Keycloak's signing keys could not be fetched (the broker cannot tell good tokens from bad, so it answers 503, not 401)."""


class Keys:
    """Keycloak's signing keys, fetched when first needed and refreshed every few minutes (or at once when a key id is unknown)."""

    def __init__(self, url, fetch=None, clock=time.time):
        self.url = url
        self.fetch = fetch or self._download
        self.clock = clock
        self.lock = threading.Lock()
        self.keys = {}
        self.fetched = 0.0

    def _download(self):
        with urllib.request.urlopen(self.url, timeout=5) as response:
            return json.loads(response.read(200000))

    def get(self, kid):
        with self.lock:
            stale = self.clock() - self.fetched > JWKS_SECONDS
            if kid not in self.keys and (stale or self.clock() - self.fetched > 30):
                try:
                    document = self.fetch()
                except (OSError, ValueError) as error:   # includes URLError and a reply that is not JSON
                    self.fetched = self.clock() - JWKS_SECONDS + 10   # try again in ten seconds, not on every request
                    raise KeysUnavailable(str(error)) from error
                self.keys = {k["kid"]: (int.from_bytes(b64url_decode(k["n"]), "big"), int.from_bytes(b64url_decode(k["e"]), "big"))
                             for k in document.get("keys", []) if k.get("kty") == "RSA" and k.get("kid") and k.get("use") != "enc"}
                self.fetched = self.clock()
            return self.keys.get(kid)


class Broker:
    def __init__(self, secret, issuer, audiences, keys, clock=time.time):
        if len(secret) < 16:
            raise ValueError("the ticket secret must be at least 16 characters")
        self.secret = secret.encode()
        self.issuer = issuer
        self.audiences = set(audiences)
        self.keys = keys
        self.clock = clock

    def verify_id_token(self, token):
        """The token's claims if it is signed by Keycloak's key, for one of our clients, from our issuer and unexpired; otherwise None."""
        try:
            head, body, signature = token.split(".")
            header, claims = json.loads(b64url_decode(head)), json.loads(b64url_decode(body))
            if header.get("alg") != "RS256":
                return None
            key = self.keys.get(header.get("kid"))
            if key is None or not rsa_pkcs1_sha256_verify(key[0], key[1], f"{head}.{body}".encode(), b64url_decode(signature)):
                return None
            audience = claims.get("aud")
            audience = set(audience if isinstance(audience, list) else [audience])
            if claims.get("iss") != self.issuer or not (audience & self.audiences):
                return None
            if not isinstance(claims.get("exp"), (int, float)) or claims["exp"] <= self.clock():
                return None
            if not isinstance(claims.get("sub"), str) or not claims["sub"] or len(claims["sub"]) > 64:
                return None
            return claims
        except KeysUnavailable:
            raise
        except (ValueError, KeyError, TypeError, AttributeError):
            return None

    def ticket_for(self, claims):
        callsign = claims.get("callsign") if isinstance(claims.get("callsign"), str) else ""
        callsign = "".join(c for c in callsign if c.isalnum() and c.isascii() or c in " _-")[:16]
        expires = int(self.clock()) + TICKET_SECONDS
        payload = "|".join([claims["sub"], callsign, str(expires), secrets.token_urlsafe(9)])
        signed = "v1." + b64url(payload.encode())
        mac = hmac.new(self.secret, signed.encode(), hashlib.sha256).digest()
        return f"{signed}.{b64url(mac)}", callsign


class RateLimit:
    """At most `limit` requests per address per minute."""

    def __init__(self, limit=20, clock=time.time):
        self.limit, self.clock, self.hits, self.lock = limit, clock, {}, threading.Lock()

    def allow(self, address):
        now = self.clock()
        with self.lock:
            recent = [t for t in self.hits.get(address, []) if now - t < 60]
            if len(recent) >= self.limit:
                self.hits[address] = recent
                return False
            self.hits[address] = recent + [now]
            if len(self.hits) > 5000:
                self.hits = {a: t for a, t in self.hits.items() if t and now - t[-1] < 60}
            return True


def make_handler(broker, limiter):
    class Handler(BaseHTTPRequestHandler):
        def _send(self, status, body=None):
            data = json.dumps(body).encode() if body is not None else b""
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def do_POST(self):
            if self.path != "/outiva/ticket":
                return self._send(404)
            # behind Apache the real client is in X-Forwarded-For; the last entry is the one Apache added
            address = (self.headers.get("X-Forwarded-For") or self.client_address[0]).split(",")[-1].strip()
            if not limiter.allow(address):
                return self._send(429, {"error": "slow down"})
            try:
                length = int(self.headers.get("Content-Length") or 0)
                if length > 8192:
                    return self._send(413)
                token = json.loads(self.rfile.read(length)).get("id_token")
            except (ValueError, AttributeError):
                return self._send(400, {"error": "bad request"})
            try:
                claims = broker.verify_id_token(token) if isinstance(token, str) else None
            except KeysUnavailable as error:
                print("outiva-broker: cannot fetch Outiva's signing keys: %s" % error, flush=True)
                return self._send(503, {"error": "Outiva sign-in is not available right now"})
            if claims is None:
                return self._send(401, {"error": "that Outiva sign-in was not accepted"})
            ticket, callsign = broker.ticket_for(claims)
            return self._send(200, {"ticket": ticket, "callsign": callsign})

        def log_message(self, message, *args):
            print("outiva-broker: " + message % args, flush=True)

    return Handler


def main():
    issuer = os.environ.get("BC_OUTIVA_ISSUER", "https://accounts.outiva.com/realms/outiva")
    audiences = [a.strip() for a in os.environ.get("BC_OUTIVA_AUDIENCE", "battlecity-web").split(",") if a.strip()]
    jwks = os.environ.get("BC_OUTIVA_JWKS_URL", issuer + "/protocol/openid-connect/certs")
    with open(os.environ["BC_TICKET_SECRET_FILE"]) as handle:
        secret = handle.readline().strip()
    broker = Broker(secret, issuer, audiences, Keys(jwks))
    port = int(os.environ.get("BC_BROKER_PORT", "9684"))
    server = ThreadingHTTPServer(("127.0.0.1", port), make_handler(broker, RateLimit()))
    print(f"outiva-broker: listening on 127.0.0.1:{port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
