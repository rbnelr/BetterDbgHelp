#include "util.hpp"
#include "dbghelp_api.hpp"
#include "dbghelp_dll_forward.hpp"
#include "dbghelp_dll_wrapper.hpp"

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH: {
			dbghelp_wrapper.init();
		} break;
		case DLL_PROCESS_DETACH: {
			dbghelp_wrapper.cleanup();
		} break;
	}
	return TRUE;
}
