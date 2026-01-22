#include "util.hpp"
#include "dbghelp_api.hpp"
#include "dbghelp_dll_forward.hpp"
#include "dbghelp_dll_wrapper.hpp"

// from util/logger.hpp; used everywhere instead of printf
Logger g_logger("BetterDbgHelp-output.txt");

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH: {
			real_dbghelp.load_if_not_loaded_yet();
		} break;
		case DLL_PROCESS_DETACH: {
			// Don't unload real dll for now, we probably need to refcount since these DLL_PROCESS_ATTACH can be called multiple times (once for each LoadLibrary on the user)
			// instead we might want to move this to a global variable and use ctor/dtor after all?
			//real_dbghelp.unload();
		} break;
	}
	return TRUE;
}
