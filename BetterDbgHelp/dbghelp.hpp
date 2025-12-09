#pragma once
#include "util.hpp"
#include "sym_resolver.hpp"

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

		std::string search_path;
		{ // Need to set search_path because dbhelp.dll does not search next to exe for pdb, instead searching this processes working directory
			char exe_name[1024];
			DWORD size = sizeof(exe_name);
			if (!QueryFullProcessImageNameA(inspectee, 0, exe_name, &size)) {
				print_err_throw("QueryFullProcessImageNameA");
			}

			std::filesystem::path exe_path = std::string_view(exe_name, size);
			search_path = exe_path.has_parent_path() ? exe_path.parent_path().u8string() : ".";
		}

		DWORD opts = 0;
		opts |= SYMOPT_LOAD_LINES;         // line info
		//opts |= SYMOPT_UNDNAME;            // undecorate C++ names, tracy does not use this
		SymSetOptions(opts);

		// This means load symbol information for currently loaded modules
		// Tracy is using this, but then also calling SymLoadModuleEx later (since modules can be loaded later)
		// In my case I just want to measure symbol resolution performance and I assume the modules I'm interested in are already loaded
		BOOL fInvadeProcess = TRUE;
		if (!SymInitialize(inspectee, search_path.c_str(), fInvadeProcess)) {
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
	
	void measure_addr2sym (char* addr) {
		constexpr size_t MaxNameSize = 8192;
		char buf[sizeof(SYMBOL_INFO) + MaxNameSize] = {};
		auto* si = (SYMBOL_INFO*)buf;
		si->SizeOfStruct = sizeof(SYMBOL_INFO);
		si->MaxNameLen = MaxNameSize;

		DWORD Displacement = 0;

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

			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
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
		
		if (doInline) {
			for (DWORD i=0; i<inlineNum; i++) {
				{
					TimerMeasZone(tSymFromInlineContext);
					res1 = _SymFromInlineContext(inspectee, (DWORD64)addr, ctx, NULL, si);
				}
				
				if (res1) {
					TimerMeasZone(tSymGetLineFromInlineContext);

					IMAGEHLP_LINE64 line = {};
					line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
					res2 = _SymGetLineFromInlineContext(inspectee, (DWORD64)addr, ctx, 0, &Displacement, &line);
				}

				ctx++;
			}
		}
	}

	bool addr2sym (void* addr, SymResult* res) {
		*res = {};

		size_t strbuf_cur = 0;
		auto copy_to_strbuf = [&] (const char* str, size_t len) -> const char* {
			char* out = res->str_buf + strbuf_cur;
			size_t remain = SymResult::STRBUF_SIZE - strbuf_cur;
			size_t bytes_to_copy = remain >= len+1 ? len+1 : remain;
			if (bytes_to_copy <= 0) {
				return nullptr;
			}
			// copies whole or truncated
			memcpy(out, str, bytes_to_copy);
			strbuf_cur += bytes_to_copy;
			return out;
		};

		// SymFromAddr expects SYMBOL_INFO followed by string buffer memory
		char buf[sizeof(SYMBOL_INFO) + SymResult::STRBUF_SIZE] = {};

		auto* si = (SYMBOL_INFO*)buf;
		si->SizeOfStruct = sizeof(SYMBOL_INFO);
		si->MaxNameLen = SymResult::STRBUF_SIZE;

		DWORD Displacement = 0;

		if (!SymFromAddr(inspectee, (DWORD64)addr, nullptr, si)) {
			res->err = "SymFromAddr error";
			return false;
		}

		// need to copy into per-SymResult string buffer
		res->module_path = nullptr; // dbghelp.dll does not seem to return this, module_path is mainly for completeness sake, tracy actually determines this itself
		res->sym_name = copy_to_strbuf(si->Name, si->NameLen);
		res->src_filepath = nullptr;
		res->src_lineno = 0;
		
		{
			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			if (SymGetLineFromAddr64(inspectee, (DWORD64)addr, &Displacement, &line)) {
				res->src_filepath = copy_to_strbuf(line.FileName, strlen(line.FileName));
				res->src_lineno = line.LineNumber;
			}
		}

		BOOL doInline = FALSE;
		DWORD ctx = 0;
		DWORD inlineNum = 0;
		if (_SymAddrIncludeInlineTrace) {
			inlineNum = _SymAddrIncludeInlineTrace(inspectee, (DWORD64)addr);

			DWORD idx;
			if (inlineNum != 0) {
				doInline = _SymQueryInlineTrace(inspectee, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
			}
		}
		
		if (doInline) {
			res->num_inlines = (int)inlineNum;
			for (int i=res->num_inlines-1; i>=0; i--) {
				res->inlines[i] = {};

				if (_SymFromInlineContext(inspectee, (DWORD64)addr, ctx, NULL, si)) {
					res->inlines[i].fnname = copy_to_strbuf(si->Name, si->NameLen);
					
					IMAGEHLP_LINE64 line = {};
					line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
					if (_SymGetLineFromInlineContext(inspectee, (DWORD64)addr, ctx, 0, &Displacement, &line)) {
						res->inlines[i].filepath = copy_to_strbuf(line.FileName, strlen(line.FileName));
						res->inlines[i].lineno = line.LineNumber;
					}
				}

				ctx++;
			}
		}
		return res->valid();
	}

	void print_timings () {
		tDebughelp_init.print();

		tSymFromAddr.print();
		tSymGetLineFromAddr64.print();

		tSymAddrIncludeInlineTrace.print();
		tSymQueryInlineTrace.print();
		tSymFromInlineContext.print();
		tSymGetLineFromInlineContext.print();
	}
};

