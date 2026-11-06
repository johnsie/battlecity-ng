#include "../OutivaTicket.h"

#include <cassert>
#include <cstdio>

using namespace OutivaTicket;

static std::string Hex(const uint8_t *data, size_t size) {
	static const char digits[] = "0123456789abcdef";
	std::string out;
	for (size_t i = 0; i < size; i++) { out += digits[data[i] >> 4]; out += digits[data[i] & 15]; }
	return out;
}

int main() {
	// SHA-256 and HMAC-SHA256 against published test vectors
	uint8_t digest[32];
	Sha256 hash;
	hash.Update((const uint8_t *)"abc", 3);
	hash.Final(digest);
	assert(Hex(digest, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	HmacSha256("Jefe", "what do ya want for nothing?", digest);
	assert(Hex(digest, 32) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
	HmacSha256(std::string(131, '\xaa'), "Test Using Larger Than Block-Size Key - Hash Key First", digest);
	assert(Hex(digest, 32) == "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");

	// tickets
	const std::string secret = "0123456789abcdef0123456789abcdef";
	Claims in; in.sub = "5f0c-sub"; in.callsign = "Maverick"; in.expires = 1000 + 60; in.id = "t1";
	std::string ticket = Make(secret, in);
	Claims out;
	assert(Verify(secret, ticket, 1000, out) == Result::Ok);
	assert(out.sub == "5f0c-sub" && out.callsign == "Maverick" && out.expires == 1060 && out.id == "t1");
	assert(Verify("a different secret!!", ticket, 1000, out) == Result::BadSignature);
	assert(Verify(secret, ticket, 1060, out) == Result::Expired);
	assert(Verify(secret, ticket, 900, out) == Result::TooFarAhead);
	assert(Verify(secret, "v1.abc", 1000, out) == Result::Malformed);
	assert(Verify(secret, "", 1000, out) == Result::Malformed);
	assert(Verify("short", ticket, 1000, out) == Result::Malformed);
	std::string tampered = ticket;
	tampered[5] = tampered[5] == 'A' ? 'B' : 'A';
	assert(Verify(secret, tampered, 1000, out) == Result::BadSignature);
	Claims empty = in; empty.callsign = "";
	assert(Verify(secret, Make(secret, empty), 1000, out) == Result::Ok && out.callsign.empty());

	// a ticket is used once
	UsedTickets used;
	assert(used.Use("t1", 1060, 1000));
	assert(!used.Use("t1", 1060, 1001));
	assert(used.Use("t1", 1200, 1100));   // the old record expired, so the id is free again (the ticket itself has long expired)

	// names
	assert(CleanName("  Mav  erick!! ") == "Mav erick");
	assert(CleanName("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == "ABCDEFGHIJKLMNO");
	assert(CharacterName("Maverick") == "Maverick");
	assert(CharacterName("Guest-Bob") == "P-Guest-Bob");
	assert(CharacterName("guest-bob") == "P-guest-bob");
	assert(CharacterName("!!") == "Player");
	assert(CharacterName("ab") == "Player");
	assert(GuestName("Bob", "1234") == "Guest-Bob");
	assert(GuestName("", "1234") == "Guest-1234");
	assert(GuestName("Guest-Zed", "1234") == "Guest-Zed");
	assert(GuestName("A very long name indeed", "1234").size() <= NAME_MAX_LENGTH);
	assert(StartsWithGuest("GUEST-x") && !StartsWithGuest("Guesty"));
	std::printf("outiva ticket: ok\n");
	return 0;
}
