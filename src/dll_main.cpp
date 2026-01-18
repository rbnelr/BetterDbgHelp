#include "util.hpp"
#include "dbghelp_api.hpp"
#include "dbghelp_dll_forward.hpp"
#include "dbghelp_dll_wrapper.hpp"

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH: {
			real_dbghelp.load_if_not_loaded_yet();
		} break;
		case DLL_PROCESS_DETACH: {
			real_dbghelp.unload();
		} break;
	}
	return TRUE;
}
