#include "util.hpp"
#include "dbghelp_api.hpp"

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH: {
			real_debughelp.load();
		} break;
		case DLL_PROCESS_DETACH: {
			real_debughelp.unload();
		} break;
	}
	return TRUE;
}
