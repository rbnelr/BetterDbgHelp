#pragma once
#include "dbghelp_api.hpp"

// The functions from dbghelp.dll we want to accelerate
// the rest are in dbghelp_dll_forward.hpp

struct DbgHelpWrapper {
	void init () {
		real_dbghelp.load_if_not_loaded_yet();
	}
	void cleanup () {
		real_dbghelp.unload();
	}
};
DbgHelpWrapper dbghelp_wrapper;

extern "C" {
	__declspec(dllexport) DWORD __stdcall hook_SymSetOptions (
		DWORD SymOptions
	) {
		// Don't do anything
		return real_dbghelp.SymSetOptions(SymOptions);
	}
	
	__declspec(dllexport) BOOL __stdcall hook_SymInitialize (
		HANDLE hProcess,
		PCSTR  UserSearchPath,
		BOOL   fInvadeProcess
	) {
		dbghelp_wrapper.init();
		return real_dbghelp.SymInitialize(hProcess, UserSearchPath, fInvadeProcess);
	}
	
	__declspec(dllexport) BOOL __stdcall hook_SymInitializeW (
		HANDLE hProcess,
		PCWSTR UserSearchPath,
		BOOL   fInvadeProcess
	) {
		dbghelp_wrapper.init();
		return real_dbghelp.SymInitializeW(hProcess, UserSearchPath, fInvadeProcess);
	}
	
	__declspec(dllexport) BOOL __stdcall hook_SymCleanup (
		HANDLE hProcess
	) {
		dbghelp_wrapper.cleanup();
		return real_dbghelp.SymCleanup(hProcess);
	}
	
	__declspec(dllexport) BOOL __stdcall hook_SymFromAddr (
		HANDLE       hProcess,
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		// TODO:
		printf("SymFromAddr(%8llx)\n", Address);
		return real_dbghelp.SymFromAddr(hProcess, Address, Displacement, Symbol);
	}
	__declspec(dllexport) BOOL __stdcall hook_SymFromAddrW (
		HANDLE        hProcess,
		DWORD64       Address,
		PDWORD64      Displacement,
		PSYMBOL_INFOW Symbol
	) {
		return real_dbghelp.SymFromAddrW(hProcess, Address, Displacement, Symbol);
	}
	
	__declspec(dllexport) BOOL __stdcall hook_SymGetLineFromAddr64 (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		return real_dbghelp.SymGetLineFromAddr64(hProcess, qwAddr, pdwDisplacement, Line64);
	}
	__declspec(dllexport) BOOL __stdcall hook_SymGetLineFromAddrW64 (
		HANDLE            hProcess,
		DWORD64           dwAddr,
		PDWORD            pdwDisplacement,
		PIMAGEHLP_LINEW64 Line
	) {
		return real_dbghelp.SymGetLineFromAddrW64(hProcess, dwAddr, pdwDisplacement, Line);
	}
	
	__declspec(dllexport) DWORD __stdcall hook_SymAddrIncludeInlineTrace (
		HANDLE  hProcess,
		DWORD64 Address
	) {
		return real_dbghelp.SymAddrIncludeInlineTrace(hProcess, Address);
	}
	
	__declspec(dllexport) BOOL __stdcall hook_SymQueryInlineTrace (
		HANDLE  hProcess,
		DWORD64 StartAddress,
		DWORD   StartContext,
		DWORD64 StartRetAddress,
		DWORD64 CurAddress,
		LPDWORD CurContext,
		LPDWORD CurFrameIndex
	) {
		return real_dbghelp.SymQueryInlineTrace(hProcess, StartAddress, StartContext, StartRetAddress, CurAddress, CurContext, CurFrameIndex);
	}
	
	
	__declspec(dllexport) BOOL __stdcall hook_SymFromInlineContext (
		HANDLE       hProcess,
		DWORD64      Address,
		ULONG        InlineContext,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		return real_dbghelp.SymFromInlineContext(hProcess, Address, InlineContext, Displacement, Symbol);
	}
	__declspec(dllexport) BOOL __stdcall hook_SymFromInlineContextW (
		HANDLE        hProcess,
		DWORD64       Address,
		ULONG         InlineContext,
		PDWORD64      Displacement,
		PSYMBOL_INFOW Symbol
	) {
		return real_dbghelp.SymFromInlineContextW(hProcess, Address, InlineContext, Displacement, Symbol);
	}
	
	
	__declspec(dllexport) BOOL __stdcall hook_SymGetLineFromInlineContext (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		ULONG            InlineContext,
		DWORD64          qwModuleBaseAddress,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		return real_dbghelp.SymGetLineFromInlineContext(hProcess, qwAddr, InlineContext, qwModuleBaseAddress, pdwDisplacement, Line64);
	}
	__declspec(dllexport) BOOL __stdcall hook_SymGetLineFromInlineContextW (
		HANDLE            hProcess,
		DWORD64           dwAddr,
		ULONG             InlineContext,
		DWORD64           qwModuleBaseAddress,
		PDWORD            pdwDisplacement,
		PIMAGEHLP_LINEW64 Line
	) {
		return real_dbghelp.SymGetLineFromInlineContextW(hProcess, dwAddr, InlineContext, qwModuleBaseAddress, pdwDisplacement, Line);
	}
}
