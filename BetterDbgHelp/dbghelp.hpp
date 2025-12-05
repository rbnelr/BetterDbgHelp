#pragma once
#include "util.hpp"

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

extern "C" {
	typedef DWORD (__stdcall *t_SymAddrIncludeInlineTrace)( HANDLE hProcess, DWORD64 Address );
	typedef BOOL (__stdcall *t_SymQueryInlineTrace)( HANDLE hProcess, DWORD64 StartAddress, DWORD StartContext, DWORD64 StartRetAddress, DWORD64 CurAddress, LPDWORD CurContext, LPDWORD CurFrameIndex );
	typedef BOOL (__stdcall *t_SymFromInlineContext)( HANDLE hProcess, DWORD64 Address, ULONG InlineContext, PDWORD64 Displacement, PSYMBOL_INFO Symbol );
	typedef BOOL (__stdcall *t_SymGetLineFromInlineContext)( HANDLE hProcess, DWORD64 qwAddr, ULONG InlineContext, DWORD64 qwModuleBaseAddress, PDWORD pdwDisplacement, PIMAGEHLP_LINE64 Line64 );
}

class Debughelp {
	HANDLE inspectee;
public:
	
	TimerMeasurement tDebughelp_init = TimerMeasurement("Debughelp_init");
	TimerMeasurement twarmup = TimerMeasurement("warmup");
	TimerMeasurement tSymFromAddr = TimerMeasurement("SymFromAddr");
	TimerMeasurement tSymGetLineFromAddr64 = TimerMeasurement("SymGetLineFromAddr64");
	TimerMeasurement tSymAddrIncludeInlineTrace = TimerMeasurement("SymAddrIncludeInlineTrace");
	TimerMeasurement tSymQueryInlineTrace = TimerMeasurement("SymQueryInlineTrace");
	TimerMeasurement tSymFromInlineContext = TimerMeasurement("SymFromInlineContext");
	TimerMeasurement tSymGetLineFromInlineContext = TimerMeasurement("SymGetLineFromInlineContext");

	// from tracy's code
	t_SymAddrIncludeInlineTrace _SymAddrIncludeInlineTrace = 0;
	t_SymQueryInlineTrace _SymQueryInlineTrace = 0;
	t_SymFromInlineContext _SymFromInlineContext = 0;
	t_SymGetLineFromInlineContext _SymGetLineFromInlineContext = 0;

	Debughelp (HANDLE inspectee): inspectee{inspectee} {
		TimerMeasZone(tDebughelp_init);

		DWORD opts = 0;
		opts |= SYMOPT_LOAD_LINES;         // line info
		//opts |= SYMOPT_UNDNAME;            // undecorate C++ names, tracy does not use this
		SymSetOptions(opts);

		// This means load symbol information for currently loaded modules
		// Tracy is using this, but then also calling SymLoadModuleEx later (since modules can be loaded later)
		// In my case I just want to measure symbol resolution performance and I assume the modules I'm interested in are already loaded
		BOOL fInvadeProcess = TRUE;
		if (!SymInitialize(inspectee, NULL, fInvadeProcess)) {
			print_err_throw("SymInitialize");
		}

		//SymFromAddr
		//SymGetLineFromAddr64
		_SymAddrIncludeInlineTrace = (t_SymAddrIncludeInlineTrace)GetProcAddress(GetModuleHandleA("dbghelp.dll"), "SymAddrIncludeInlineTrace");
		_SymQueryInlineTrace = (t_SymQueryInlineTrace)GetProcAddress(GetModuleHandleA("dbghelp.dll"), "SymQueryInlineTrace");
		_SymFromInlineContext = (t_SymFromInlineContext)GetProcAddress(GetModuleHandleA("dbghelp.dll"), "SymFromInlineContext");
		_SymGetLineFromInlineContext = (t_SymGetLineFromInlineContext)GetProcAddress(GetModuleHandleA("dbghelp.dll"), "SymGetLineFromInlineContext");

	}
	~Debughelp () {
		SymCleanup(inspectee);
	}
	
	void show_addr2sym (char* addr) {
		constexpr size_t MaxNameSize = 8192;
		char buf[sizeof(SYMBOL_INFO) + MaxNameSize] = {};
		auto* si = (SYMBOL_INFO*)buf;
		si->SizeOfStruct = sizeof(SYMBOL_INFO);
		si->MaxNameLen = MaxNameSize;

		IMAGEHLP_LINE64 line = {};
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		BOOL res1 = SymFromAddr(inspectee, (DWORD64)addr, NULL, si);
		if (!res1) {
			printf("[%16p]: Not found\n", addr);
			print_err("SymFromAddr");
			return;
		}
		// start_of_symbol + Displacement == addr
		
		DWORD Displacement = 0;
		BOOL res2 = SymGetLineFromAddr64(inspectee, (DWORD64)addr, &Displacement, &line);

		//printf("[%16p]: sz: %4d flags: %4d D%4d %-15s", addr, si->Size, si->Flags, Displacement, si->Name);
		printf("[%16p]: %-15s", addr, si->Name);
		
		if (!res2) { // tracy has another check here: line.LineNumber >= 0xF00000 but this is undocumented
			printf("\n");
			//print_err("SymGetLineFromAddr64"); // Not having a line number is normal for certain addresses
			return;
		}
		// address_of_first_instruction_of_line + Displacement2 == addr
		assert((char*)(line.Address + Displacement) == addr);

		printf(" @ %s:%d\n", line.FileName, line.LineNumber);
		
		BOOL doInline = FALSE;
		DWORD ctx = 0;
		DWORD inlineNum = 0;
		if (_SymAddrIncludeInlineTrace) {
			inlineNum = _SymAddrIncludeInlineTrace(inspectee, (DWORD64)addr);

			DWORD idx;
			if (inlineNum != 0)
				doInline = _SymQueryInlineTrace(inspectee, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
		}

		//printf("> Inline %d %d %d\n", inlineNum, doInline, ctx);
		
		for (DWORD i=0; i<inlineNum; i++) {
			res1 = _SymFromInlineContext(inspectee, (DWORD64)addr, ctx, NULL, si);
			if (!res1) {
				print_err("SymFromInlineContext");
				//printf("> Not found\n");
				continue;
			}

			res2 = _SymGetLineFromInlineContext(inspectee, (DWORD64)addr, ctx, 0, &Displacement, &line);

			printf("> %-15s", si->Name);

			if (!res2) {
				printf("\n");
				//print_err("SymGetLineFromInlineContext");
				continue;
			}

			printf(" @ %s:%d\n", line.FileName, line.LineNumber);

			ctx++;
		}
	}
	
	void warmup_addr2sym (char* addr) {
		TimerMeasZone(twarmup);
		// simply measure_addr2sym without timers

		constexpr size_t MaxNameSize = 8192;
		char buf[sizeof(SYMBOL_INFO) + MaxNameSize] = {};
		auto* si = (SYMBOL_INFO*)buf;
		si->SizeOfStruct = sizeof(SYMBOL_INFO);
		si->MaxNameLen = MaxNameSize;

		DWORD Displacement = 0;

		IMAGEHLP_LINE64 line = {};
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		BOOL res1;
		{
			res1 = SymFromAddr(inspectee, (DWORD64)addr, nullptr, si);
		}
		if (!res1) {
			return;
		}
		
		BOOL res2;
		{
			res2 = SymGetLineFromAddr64(inspectee, (DWORD64)addr, &Displacement, &line);
		}

		
		BOOL doInline = FALSE;
		DWORD ctx = 0;
		DWORD inlineNum = 0;
		if (_SymAddrIncludeInlineTrace) {
			{
				inlineNum = _SymAddrIncludeInlineTrace(inspectee, (DWORD64)addr);
			}

			DWORD idx;
			if (inlineNum != 0) {
				doInline = _SymQueryInlineTrace(inspectee, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
			}
		}
		
		for (DWORD i=0; i<inlineNum; i++) {
			{
				res1 = _SymFromInlineContext(inspectee, (DWORD64)addr, ctx, NULL, si);
			}
			if (!res1) {
				continue;
			}
			
			{
				res2 = _SymGetLineFromInlineContext(inspectee, (DWORD64)addr, ctx, 0, &Displacement, &line);
			}
			if (!res2) {
				continue;
			}

			ctx++;
		}
	}
	void measure_addr2sym (char* addr) {
		constexpr size_t MaxNameSize = 8192;
		char buf[sizeof(SYMBOL_INFO) + MaxNameSize] = {};
		auto* si = (SYMBOL_INFO*)buf;
		si->SizeOfStruct = sizeof(SYMBOL_INFO);
		si->MaxNameLen = MaxNameSize;

		DWORD Displacement = 0;

		IMAGEHLP_LINE64 line = {};
		line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		BOOL res1;
		{
			TimerMeasZone(tSymFromAddr);
			res1 = SymFromAddr(inspectee, (DWORD64)addr, nullptr, si);
		}
		if (!res1) {
			return;
		}
		
		BOOL res2;
		{
			TimerMeasZone(tSymGetLineFromAddr64);
			res2 = SymGetLineFromAddr64(inspectee, (DWORD64)addr, &Displacement, &line);
		}

		
		BOOL doInline = FALSE;
		DWORD ctx = 0;
		DWORD inlineNum = 0;
		if (_SymAddrIncludeInlineTrace) {
			{
				TimerMeasZone(tSymAddrIncludeInlineTrace);
				inlineNum = _SymAddrIncludeInlineTrace(inspectee, (DWORD64)addr);
			}

			DWORD idx;
			if (inlineNum != 0) {
				TimerMeasZone(tSymQueryInlineTrace);
				doInline = _SymQueryInlineTrace(inspectee, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
			}
		}
		
		for (DWORD i=0; i<inlineNum; i++) {
			{
				TimerMeasZone(tSymFromInlineContext);
				res1 = _SymFromInlineContext(inspectee, (DWORD64)addr, ctx, NULL, si);
			}
			if (!res1) {
				continue;
			}
			
			{
				TimerMeasZone(tSymGetLineFromInlineContext);
				res2 = _SymGetLineFromInlineContext(inspectee, (DWORD64)addr, ctx, 0, &Displacement, &line);
			}
			if (!res2) {
				continue;
			}

			ctx++;
		}
	}

	void print_timings () {
		tDebughelp_init.print();
		twarmup.print();

		tSymFromAddr.print();
		tSymGetLineFromAddr64.print();

		tSymAddrIncludeInlineTrace.print();
		tSymQueryInlineTrace.print();
		tSymFromInlineContext.print();
		tSymGetLineFromInlineContext.print();
	}
};

