#pragma once
#include "dbghelp_api.hpp"
#include "pdb/pdb.hpp"
#include "pdb/module_lookup.hpp"

#define MODE_VERIFY 1

struct DbgHelpWrapperSession {
	ModuleCache mod_cache;

	DbgHelpWrapperSession (HANDLE hProcess): mod_cache{hProcess} {}
	~DbgHelpWrapperSession () {}

	// TODO: cache mod_cache.find_module_for_addr + mod->pdb->find_symbol_for_addr on Address
	// by simply storing last Address + mod + sym_idx

	BOOL SymFromAddr (
		HANDLE       hProcess,
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		auto* mod = mod_cache.find_module_for_addr(Address);
		if (!mod) {
			return false;
		}
		uint64_t rva = Address - mod->base_addr;
		
		if (!mod->pdb) {
			auto* exports = mod->load_export_table();
		
			if (exports) {
				uint64_t sym_rva;
				uint32_t sym_idx;
				auto* mangled_name = exports->query(rva, &sym_rva, &sym_idx);
				if (mangled_name) {
					Symbol->TypeIndex = 0;
					Symbol->Reserved[0] = 0;
					Symbol->Reserved[1] = 0;
					Symbol->Index = 0;
					Symbol->Size = 0;
					Symbol->ModBase = mod->base_addr;
					Symbol->Flags = 0;
					Symbol->Value = 0;
					Symbol->Address = mod->base_addr + sym_rva;
					Symbol->Register = 0;
					Symbol->Scope = 0;
					Symbol->Tag = (ULONG)SymTagEnum::SymTagPublicSymbol;

					if (Displacement)
						*Displacement = Address - Symbol->Address;
		
					Symbol->NameLen = strcpy_trunc(Symbol->Name, mangled_name, Symbol->MaxNameLen);
					return true;
				}
			}
			//res->err = "Module pdb not found and not found in export table";
			return false;
		}
		
		uint32_t sym_idx;
		auto* sym = mod->pdb->find_symbol_for_addr(rva, &sym_idx);
		if (!sym) {
			//res->err = "Symbol not found";
			return false;
		}
		
		Symbol->TypeIndex = 0;
		Symbol->Reserved[0] = 0;
		Symbol->Reserved[1] = 0;
		// Not the same index as dbghelp (as the indices are unstable across runs and only used for passing into other api functions)
		// we could implement other parts of the api using our own index scheme
		// actually, should return 0 here so that if dbghelp calls that I do not replace are called with this index, it can fail instead of being confused
		//Symbol->Index = sym_idx;
		Symbol->Index = 0;
		// Size for module symbols makes sense, but for global data symbols it has to be "estimated" based on the typeinfo, which I cannot easily replicate
		// for global function symbols it's even more weird and dbghelp seemingly sometimes computes it based on types extracted from name mangling
		// Since can't reasonably match it I resort to jus returning 0 in those cases
		Symbol->Size = sym->size;
		Symbol->ModBase = mod->base_addr;
		// Don't bother returning flags, i've only observed very few of them appear and
		// at least SYMFLAG_EXPORT seems to be determined based on PE export table instead of pdb, which is overcomplicated imho
		// a full 1-1 match of dbghelp behavior is probably only possibly by caching dbghelp results, which is not my goal
		Symbol->Flags = 0;
		Symbol->Value = 0;
		// HACK: __ImageBase does not return an Address unlike seemingly everything else in dbghelp, super pointless but this matches dbghelp more closely
		Symbol->Address = sym->base_addr != 0 ? mod->base_addr + sym->base_addr : 0;
		Symbol->Register = 0;
		Symbol->Scope = 0;
		Symbol->Tag = (ULONG)sym->si_tag;

		if (Displacement)
			*Displacement = Address - (mod->base_addr + sym->base_addr);

		const char* name = mod->pdb->stralloc[sym->name];
		Symbol->NameLen = strcpy_trunc(Symbol->Name, name, Symbol->MaxNameLen);
		return true;
	}
	
	BOOL SymGetLineFromAddr64 (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		auto* mod = mod_cache.find_module_for_addr(qwAddr);
		if (!mod || !mod->pdb) {
			return false;
		}
		uint64_t rva = qwAddr - mod->base_addr;
		
		uint32_t sym_idx;
		auto* sym = mod->pdb->find_symbol_for_addr(rva, &sym_idx);
		if (!sym) {
			//res->err = "Symbol not found";
			return false;
		}

		SourceLoc src_loc = {};
		if (mod->pdb->find_source_loc_for_addr(sym, rva, &src_loc)) {

			Line64->Key = nullptr;
			Line64->LineNumber = src_loc.lineno;
			// For some reason dbghelp returns a non-const char* pointer here, maybe because the string is a copy anyway
			// but the string is invalidated on the next dbghelp call, so I'm not sure anybody would write into this
			// for us this means we would technically be forced to do a copy here, but it's unlikely anybody would write into this pointer
			// TODO: consider actually doing a copy after all
			Line64->FileName = (char*)src_loc.filepath;
			Line64->Address = mod->base_addr + sym->base_addr + src_loc.line_start_offset;
			
			if (pdwDisplacement)
				*pdwDisplacement = (uint32_t)(qwAddr - Line64->Address);

			return true;
		}

		return false;
	}
	
	// like dbghelp, copy null terminated string or truncate and don't null terminate if max_len too short
	// return truncated length (for some reason)
	static ULONG strcpy_trunc (char* dst, char const* src, ULONG max_len) {
		ULONG len = (ULONG)strlen(src);
		strcpy_s(dst, max_len, src);
		return std::min(max_len, len);
	}
};
struct DbgHelpWrapper {
	// TODO: only support single session for now, could use a hashmap HANDLE hProcess -> ModuleCache here to support that
	// for now std::optional ties Session ctor/dtor to init/cleanup
	std::optional<DbgHelpWrapperSession> sess;

	// This is not needed for tracy at all
	// in theory this lets me find pdbs at the user search path, but there was no testing or research done...
	void set_search_path (PCSTR UserSearchPath=nullptr) {
		if (UserSearchPath) {
			set_search_pathW(str2wstr(UserSearchPath).c_str());
		}
		else {
			set_search_pathW(nullptr);
		}
	}
	void set_search_pathW (PCWSTR UserSearchPath=nullptr) {
		PDB_Locator::extra_search_paths = {};

		if (UserSearchPath) {
			auto* cur = UserSearchPath;
			while (*cur != L'\0') {
				auto* end = wcschr(cur, L';');
				if (end > cur) {
					PDB_Locator::extra_search_paths.emplace_back(cur, end);
				}
				cur = end;

				if (*cur == L';')
					cur++;
			}
		}
		else {
			//search_paths.emplace_back(std::filesystem::current_path());
			PDB_Locator::extra_search_paths.emplace_back(".\\");
			
			auto var = GetEnvVar(L"_NT_SYMBOL_PATH");
			if (!var.empty()) {
				PDB_Locator::extra_search_paths.emplace_back(std::move(var));
			}
			var = GetEnvVar(L"_NT_ALTERNATE_SYMBOL_PATH");
			if (!var.empty()) {
				PDB_Locator::extra_search_paths.emplace_back(std::move(var));
			}
		}
	}

	void init (HANDLE hProcess) {
		if (PDB_Locator::extra_search_paths.empty()) {
			set_search_pathW(nullptr);
		}
		sess.emplace(hProcess);
	}
	void cleanup (HANDLE hProcess) {
		sess.reset();
	}
};
DbgHelpWrapper dbghelp_wrapper;

// The functions from dbghelp.dll we want to accelerate
// the rest are in dbghelp_dll_forward.hpp

extern "C" {
	DWORD __stdcall hook_SymSetOptions (
		DWORD SymOptions
	) {
		// I have no idea if I should respect any SymOptions
		// Tracy only passes SYMOPT_LOAD_LINES (I assume not passing it makes lineinfo and inlinee line info fail?)
		// SYMOPT_UNDNAME might be another common option, but it's out of scope for me right now
		// for now do nothing here

		real_dbghelp.SymSetOptions(SymOptions);
		
		return true;
	}
	
	BOOL __stdcall hook_SymSetSearchPath (
		HANDLE hProcess,
		PCSTR  SearchPath
	) {
		dbghelp_wrapper.set_search_path(SearchPath);

		real_dbghelp.SymSetSearchPath(hProcess, SearchPath);

		return true;
	}
	BOOL __stdcall hook_SymSetSearchPathW (
		HANDLE hProcess,
		PCWSTR SearchPath
	) {
		dbghelp_wrapper.set_search_pathW(SearchPath);

		real_dbghelp.SymSetSearchPathW(hProcess, SearchPath);

		return true;
	}

	BOOL __stdcall hook_SymInitialize (
		HANDLE hProcess,
		PCSTR  UserSearchPath,
		BOOL   fInvadeProcess
	) {
		dbghelp_wrapper.set_search_path(UserSearchPath);
		dbghelp_wrapper.init(hProcess);

		real_dbghelp.SymInitialize(hProcess, UserSearchPath, fInvadeProcess);

		return true;
	}
	BOOL __stdcall hook_SymInitializeW (
		HANDLE hProcess,
		PCWSTR UserSearchPath,
		BOOL   fInvadeProcess
	) {
		dbghelp_wrapper.set_search_pathW(UserSearchPath);
		dbghelp_wrapper.init(hProcess);

		real_dbghelp.SymInitializeW(hProcess, UserSearchPath, fInvadeProcess);
		
		return true;
	}
	
	BOOL __stdcall hook_SymCleanup (
		HANDLE hProcess
	) {
		dbghelp_wrapper.cleanup(hProcess);

		real_dbghelp.SymCleanup(hProcess);

		return true;
	}
	
	BOOL __stdcall hook_SymFromAddr (
		HANDLE       hProcess,
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		BOOL res = false;
		if (dbghelp_wrapper.sess.has_value())
			res = dbghelp_wrapper.sess->SymFromAddr(hProcess, Address, Displacement, Symbol);
		
		SYMBOL_INFO_PACKAGE sym2;
		sym2.si = {};
		sym2.si.SizeOfStruct = sizeof(sym2.si);
		sym2.si.MaxNameLen = MAX_SYM_NAME;

		BOOL res2 = real_dbghelp.SymFromAddr(hProcess, Address, Displacement, &sym2.si);
		
		if (res)
			logf("%s\n", Symbol->Name);
		if (res2)
			logf("%s\n", sym2.si.Name);

		return res;
	}
	BOOL __stdcall hook_SymFromAddrW (
		HANDLE        hProcess,
		DWORD64       Address,
		PDWORD64      Displacement,
		PSYMBOL_INFOW Symbol
	) {
		return real_dbghelp.SymFromAddrW(hProcess, Address, Displacement, Symbol);
	}
	
	BOOL __stdcall hook_SymGetLineFromAddr64 (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		BOOL res = false;
		if (dbghelp_wrapper.sess.has_value())
			res = dbghelp_wrapper.sess->SymGetLineFromAddr64(hProcess, qwAddr, pdwDisplacement, Line64);
		
		DWORD Displacement2 = 0;
		IMAGEHLP_LINE64 line2 = {};
		line2.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		BOOL res2 = real_dbghelp.SymGetLineFromAddr64(hProcess, qwAddr, &Displacement2, &line2);
		
		if (res)
			logf("> %s:%d +%d\n", Line64->FileName, Line64->LineNumber, *pdwDisplacement);
		if (res2)
			logf("> %s:%d +%d\n", line2.FileName, line2.LineNumber, Displacement2);

		return res;
	}
	BOOL __stdcall hook_SymGetLineFromAddrW64 (
		HANDLE            hProcess,
		DWORD64           dwAddr,
		PDWORD            pdwDisplacement,
		PIMAGEHLP_LINEW64 Line
	) {
		return real_dbghelp.SymGetLineFromAddrW64(hProcess, dwAddr, pdwDisplacement, Line);
	}
	
	DWORD __stdcall hook_SymAddrIncludeInlineTrace (
		HANDLE  hProcess,
		DWORD64 Address
	) {
		return real_dbghelp.SymAddrIncludeInlineTrace(hProcess, Address);
	}
	
	BOOL __stdcall hook_SymQueryInlineTrace (
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
	
	
	BOOL __stdcall hook_SymFromInlineContext (
		HANDLE       hProcess,
		DWORD64      Address,
		ULONG        InlineContext,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		return real_dbghelp.SymFromInlineContext(hProcess, Address, InlineContext, Displacement, Symbol);
	}
	BOOL __stdcall hook_SymFromInlineContextW (
		HANDLE        hProcess,
		DWORD64       Address,
		ULONG         InlineContext,
		PDWORD64      Displacement,
		PSYMBOL_INFOW Symbol
	) {
		return real_dbghelp.SymFromInlineContextW(hProcess, Address, InlineContext, Displacement, Symbol);
	}
	
	
	BOOL __stdcall hook_SymGetLineFromInlineContext (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		ULONG            InlineContext,
		DWORD64          qwModuleBaseAddress,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		return real_dbghelp.SymGetLineFromInlineContext(hProcess, qwAddr, InlineContext, qwModuleBaseAddress, pdwDisplacement, Line64);
	}
	BOOL __stdcall hook_SymGetLineFromInlineContextW (
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
