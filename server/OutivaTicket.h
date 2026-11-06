// Outiva login tickets and the names they produce. Header-only and free of server types, so it can be tested on its own.
//
// A ticket is made by the Outiva login broker (deploy/web/outiva_broker.py) after it has verified a person's Outiva sign-in, and is
// handed to the game server in a cmTicketLogin packet. The game server never sees an Outiva token or a password. Format:
//
//     v1.<base64url(sub|callsign|expires|id)>.<base64url(HMAC-SHA256(secret, the first two parts joined by a dot))>
//
// `expires` is Unix seconds, `id` is random per ticket and a ticket is accepted once.
#ifndef OUTIVA_TICKET_H
#define OUTIVA_TICKET_H

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace OutivaTicket {

// ---------------------------------------------------------------------------------------------------------------- SHA-256 / HMAC
class Sha256 {
public:
	Sha256() { Reset(); }
	void Reset() {
		static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
		std::memcpy(state, init, sizeof(state));
		length = 0;
		used = 0;
	}
	void Update(const uint8_t *data, size_t size) {
		length += size;
		while (size > 0) {
			size_t take = 64 - used < size ? 64 - used : size;
			std::memcpy(block + used, data, take);
			used += take; data += take; size -= take;
			if (used == 64) { Compress(); used = 0; }
		}
	}
	void Final(uint8_t out[32]) {
		uint64_t bits = length * 8;
		uint8_t pad = 0x80;
		Update(&pad, 1);
		uint8_t zero = 0;
		while (used != 56) Update(&zero, 1);
		uint8_t tail[8];
		for (int i = 0; i < 8; i++) tail[i] = (uint8_t)(bits >> (56 - 8 * i));
		Update(tail, 8);
		for (int i = 0; i < 8; i++) {
			out[i * 4] = (uint8_t)(state[i] >> 24); out[i * 4 + 1] = (uint8_t)(state[i] >> 16);
			out[i * 4 + 2] = (uint8_t)(state[i] >> 8); out[i * 4 + 3] = (uint8_t)state[i];
		}
	}
private:
	uint32_t state[8];
	uint8_t block[64];
	size_t used;
	uint64_t length;
	static uint32_t Rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
	void Compress() {
		static const uint32_t k[64] = {
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
			0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
			0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
			0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
			0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
			0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
		uint32_t w[64];
		for (int i = 0; i < 16; i++) w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) | ((uint32_t)block[i * 4 + 2] << 8) | block[i * 4 + 3];
		for (int i = 16; i < 64; i++) {
			uint32_t s0 = Rotr(w[i - 15], 7) ^ Rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
			uint32_t s1 = Rotr(w[i - 2], 17) ^ Rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
			w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}
		uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
		for (int i = 0; i < 64; i++) {
			uint32_t t1 = h + (Rotr(e, 6) ^ Rotr(e, 11) ^ Rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
			uint32_t t2 = (Rotr(a, 2) ^ Rotr(a, 13) ^ Rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
			h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
		}
		state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e; state[5] += f; state[6] += g; state[7] += h;
	}
};

inline void HmacSha256(const std::string &key, const std::string &message, uint8_t out[32]) {
	uint8_t pad[64];
	uint8_t keyBlock[64];
	std::memset(keyBlock, 0, sizeof(keyBlock));
	if (key.size() > 64) {
		Sha256 hash;
		hash.Update((const uint8_t *)key.data(), key.size());
		hash.Final(keyBlock);
	} else {
		std::memcpy(keyBlock, key.data(), key.size());
	}
	for (int i = 0; i < 64; i++) pad[i] = keyBlock[i] ^ 0x36;
	uint8_t inner[32];
	Sha256 first;
	first.Update(pad, 64);
	first.Update((const uint8_t *)message.data(), message.size());
	first.Final(inner);
	for (int i = 0; i < 64; i++) pad[i] = keyBlock[i] ^ 0x5c;
	Sha256 second;
	second.Update(pad, 64);
	second.Update(inner, 32);
	second.Final(out);
}

// ---------------------------------------------------------------------------------------------------------------- base64url
inline std::string Base64UrlEncode(const uint8_t *data, size_t size) {
	static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::string out;
	for (size_t i = 0; i < size; i += 3) {
		uint32_t chunk = (uint32_t)data[i] << 16;
		if (i + 1 < size) chunk |= (uint32_t)data[i + 1] << 8;
		if (i + 2 < size) chunk |= data[i + 2];
		out += alphabet[(chunk >> 18) & 63];
		out += alphabet[(chunk >> 12) & 63];
		if (i + 1 < size) out += alphabet[(chunk >> 6) & 63];
		if (i + 2 < size) out += alphabet[chunk & 63];
	}
	return out;
}

inline bool Base64UrlDecode(const std::string &text, std::string &out) {
	out.clear();
	uint32_t chunk = 0;
	int bits = 0;
	for (char c : text) {
		int v;
		if (c >= 'A' && c <= 'Z') v = c - 'A';
		else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
		else if (c >= '0' && c <= '9') v = c - '0' + 52;
		else if (c == '-') v = 62;
		else if (c == '_') v = 63;
		else return false;
		chunk = (chunk << 6) | (uint32_t)v;
		bits += 6;
		if (bits >= 8) { bits -= 8; out += (char)((chunk >> bits) & 0xff); }
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------- tickets
struct Claims {
	std::string sub;       // the person's immutable Outiva id
	std::string callsign;  // the callsign chosen on their Outiva account (may be empty)
	int64_t expires = 0;   // Unix seconds
	std::string id;        // random per ticket
};

enum class Result { Ok, Malformed, BadSignature, Expired, TooFarAhead };

/** Tickets live for at most this long; a ticket claiming to outlive that is refused, so a leaked secret-less forgery cannot be long-lived either. */
const int64_t MAX_LIFETIME_SECONDS = 120;

inline bool ConstantTimeEqual(const uint8_t *a, const uint8_t *b, size_t size) {
	uint8_t difference = 0;
	for (size_t i = 0; i < size; i++) difference |= (uint8_t)(a[i] ^ b[i]);
	return difference == 0;
}

inline Result Verify(const std::string &secret, const std::string &ticket, int64_t now, Claims &claims) {
	if (secret.size() < 16 || ticket.size() > 400 || ticket.compare(0, 3, "v1.") != 0) return Result::Malformed;
	size_t dot = ticket.find('.', 3);
	if (dot == std::string::npos || ticket.find('.', dot + 1) != std::string::npos) return Result::Malformed;
	std::string signed_part = ticket.substr(0, dot);
	std::string payload64 = ticket.substr(3, dot - 3);
	std::string signature;
	if (!Base64UrlDecode(ticket.substr(dot + 1), signature) || signature.size() != 32) return Result::Malformed;
	uint8_t expected[32];
	HmacSha256(secret, signed_part, expected);
	if (!ConstantTimeEqual(expected, (const uint8_t *)signature.data(), 32)) return Result::BadSignature;

	std::string payload;
	if (!Base64UrlDecode(payload64, payload)) return Result::Malformed;
	std::vector<std::string> parts;
	size_t start = 0;
	for (;;) {
		size_t bar = payload.find('|', start);
		if (bar == std::string::npos) { parts.push_back(payload.substr(start)); break; }
		parts.push_back(payload.substr(start, bar - start));
		start = bar + 1;
	}
	if (parts.size() != 4 || parts[0].empty() || parts[0].size() > 64 || parts[1].size() > 40 || parts[3].empty() || parts[3].size() > 40) return Result::Malformed;
	for (char c : parts[2]) if (c < '0' || c > '9') return Result::Malformed;
	if (parts[2].empty() || parts[2].size() > 12) return Result::Malformed;
	claims.sub = parts[0];
	claims.callsign = parts[1];
	claims.expires = std::stoll(parts[2]);
	claims.id = parts[3];
	if (claims.expires <= now) return Result::Expired;
	if (claims.expires > now + MAX_LIFETIME_SECONDS) return Result::TooFarAhead;
	return Result::Ok;
}

/** Make a ticket (used by tests; in production the broker makes them). */
inline std::string Make(const std::string &secret, const Claims &claims) {
	std::string payload = claims.sub + "|" + claims.callsign + "|" + std::to_string(claims.expires) + "|" + claims.id;
	std::string signedPart = "v1." + Base64UrlEncode((const uint8_t *)payload.data(), payload.size());
	uint8_t mac[32];
	HmacSha256(secret, signedPart, mac);
	return signedPart + "." + Base64UrlEncode(mac, 32);
}

/** Remembers the tickets already used until they would have expired anyway, so each logs in once. */
class UsedTickets {
public:
	/** False if this ticket id was already used. */
	bool Use(const std::string &id, int64_t expires, int64_t now) {
		for (auto it = used.begin(); it != used.end();) it = it->second <= now ? used.erase(it) : std::next(it);
		if (used.count(id)) return false;
		used[id] = expires;
		return true;
	}
private:
	std::map<std::string, int64_t> used;
};

// ---------------------------------------------------------------------------------------------------------------- names
const size_t NAME_MAX_LENGTH = 15;   // the account name column and every protocol struct hold 15 characters
const char GUEST_PREFIX[] = "Guest-";

inline bool IsNameChar(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-';
}

/** Keep the allowed characters, collapse spaces, trim and cut to `limit`. */
inline std::string CleanName(const std::string &text, size_t limit = NAME_MAX_LENGTH) {
	std::string out;
	for (char c : text) {
		if (!IsNameChar(c)) continue;
		if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
		out += c;
		if (out.size() >= limit) break;
	}
	while (!out.empty() && out.back() == ' ') out.pop_back();
	return out;
}

inline bool StartsWithGuest(const std::string &name) {
	if (name.size() < sizeof(GUEST_PREFIX) - 1) return false;
	for (size_t i = 0; i < sizeof(GUEST_PREFIX) - 1; i++) {
		char a = name[i], b = GUEST_PREFIX[i];
		if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
		if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
		if (a != b) return false;
	}
	return true;
}

/** The character name for an Outiva callsign. It can never look like a guest, and is never shorter than 3 characters. */
inline std::string CharacterName(const std::string &callsign) {
	std::string name = CleanName(callsign);
	if (StartsWithGuest(name)) name = CleanName("P-" + name);
	if (name.size() < 3) name = "Player";
	return name;
}

/** A guest's name: always starts with "Guest-", so it cannot be mistaken for an Outiva character. */
inline std::string GuestName(const std::string &requested, const std::string &fallbackDigits) {
	std::string rest = CleanName(requested, NAME_MAX_LENGTH - (sizeof(GUEST_PREFIX) - 1));
	if (StartsWithGuest(rest)) rest = rest.substr(sizeof(GUEST_PREFIX) - 1);
	if (rest.empty()) rest = fallbackDigits;
	return std::string(GUEST_PREFIX) + rest;
}

} // namespace OutivaTicket

#endif
