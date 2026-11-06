// Where the client finds the Outiva login ticket (see server/OutivaTicket.h).
//
// In the browser the page keeps a fresh one-time ticket in window.bcTicket (see web/shell.html), renewed every 45 seconds, so a ticket asked for
// when the player clicks is never stale. The desktop client reads BC_OUTIVA_TICKET / BC_OUTIVA_CALLSIGN from the environment.
#ifndef BC_OUTIVA_LOGIN_H
#define BC_OUTIVA_LOGIN_H

#include <cstdlib>
#include <string>

#ifdef __EMSCRIPTEN__
// Defined in CLogin.cpp with EM_JS (plain JavaScript functions). emscripten_run_script* would need eval, which the site's Content Security Policy forbids.
extern "C" int bc_ticket_load(int which);   // copies window.bcTicket (0) or window.bcTicketCallsign (1) aside as UTF-8 and returns its length
extern "C" int bc_ticket_byte(int index);
extern "C" void bc_ticket_used();

inline std::string BCTicketText(int which) {
	int length = bc_ticket_load(which);
	std::string out;
	for (int i = 0; i < length && i < 400; i++) out += (char)bc_ticket_byte(i);
	return out;
}
#endif

inline std::string OutivaTicket() {
#ifdef __EMSCRIPTEN__
	return BCTicketText(0);
#else
	const char *ticket = std::getenv("BC_OUTIVA_TICKET");
	return ticket ? ticket : "";
#endif
}

inline std::string OutivaCallsign() {
#ifdef __EMSCRIPTEN__
	return BCTicketText(1);
#else
	const char *callsign = std::getenv("BC_OUTIVA_CALLSIGN");
	return callsign ? callsign : "";
#endif
}

/** A ticket works once: tell the page, so it fetches the next one. */
inline void OutivaTicketUsed() {
#ifdef __EMSCRIPTEN__
	bc_ticket_used();
#endif
}

#endif
