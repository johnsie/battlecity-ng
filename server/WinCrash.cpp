/*
 * Battle City server - Windows crash reporting.
 * When the process dies from an access violation etc., print the exception and a stack trace (with function
 * names and line numbers when the .pdb is next to the executable) so the cause is in the log, not just in
 * the Windows Event Log. Does nothing on other platforms.
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI BCCrashHandler(EXCEPTION_POINTERS *info) {
	fprintf(stderr, "\n=== CRASH: exception 0x%08lX at %p ===\n", info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress);
	if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
		fprintf(stderr, "access violation: %s address %p\n", info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading", (void *)info->ExceptionRecord->ExceptionInformation[1]);
	}
	HANDLE process = GetCurrentProcess();
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	SymInitialize(process, NULL, TRUE);

	CONTEXT context = *info->ContextRecord;
	STACKFRAME64 frame = {};
	frame.AddrPC.Offset = context.Rip;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrStack.Offset = context.Rsp;
	frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

	for (int i = 0; i < 32; i++) {
		if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
		if (frame.AddrPC.Offset == 0) break;
		char buffer[sizeof(SYMBOL_INFO) + 256];
		SYMBOL_INFO *symbol = (SYMBOL_INFO *)buffer;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = 255;
		DWORD64 displacement = 0;
		DWORD lineDisplacement = 0;
		IMAGEHLP_LINE64 line = {};
		line.SizeOfStruct = sizeof(line);
		const char *name = SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol) ? symbol->Name : "?";
		if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line)) {
			fprintf(stderr, "  #%d %s  (%s:%lu)\n", i, name, line.FileName, line.LineNumber);
		} else {
			fprintf(stderr, "  #%d %s  [0x%llX]\n", i, name, (unsigned long long)frame.AddrPC.Offset);
		}
	}
	fflush(stderr);
	return EXCEPTION_EXECUTE_HANDLER;
}

void InstallCrashHandler() {
	SetUnhandledExceptionFilter(BCCrashHandler);
}
#else
void InstallCrashHandler() {}
#endif
