// Where the client finds the Outiva login ticket (see server/OutivaTicket.h).
//
// In the browser the page keeps a fresh one-time ticket in window.bcTicket (see web/shell.html), renewed every 45 seconds, so a ticket asked for
// when the player clicks is never stale. The desktop client reads BC_OUTIVA_TICKET / BC_OUTIVA_CALLSIGN from the environment.
#ifndef BC_OUTIVA_LOGIN_H
#define BC_OUTIVA_LOGIN_H

#include <cstdlib>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

inline std::string OutivaTicket() {
#ifdef __EMSCRIPTEN__
	const char *ticket = emscripten_run_script_string("window.bcTicket || ''");
	return ticket ? ticket : "";
#else
	const char *ticket = std::getenv("BC_OUTIVA_TICKET");
	return ticket ? ticket : "";
#endif
}

inline std::string OutivaCallsign() {
#ifdef __EMSCRIPTEN__
	const char *callsign = emscripten_run_script_string("window.bcTicketCallsign || ''");
	return callsign ? callsign : "";
#else
	const char *callsign = std::getenv("BC_OUTIVA_CALLSIGN");
	return callsign ? callsign : "";
#endif
}

/** A ticket works once: tell the page, so it fetches the next one. */
inline void OutivaTicketUsed() {
#ifdef __EMSCRIPTEN__
	emscripten_run_script("if (window.bcTicketUsed) window.bcTicketUsed();");
#endif
}

#endif
