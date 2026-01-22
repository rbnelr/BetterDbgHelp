#pragma once
#include "dbghelp_api.hpp"
#include "pdb/pdb.hpp"
#include "pdb/module_lookup.hpp"

// Verification:
// Optionally verify all implemented dbghelp API calls against real dbghelp.dll
// Disabling is using compile-time macro for better optimization
// Currently real dbghelp.dll is still loaded and not-implemented calls are forwarded to it
//  actually calling a mix of implemented and non-implemented functions should in theory be supported but is completely untested
// Could consider not loading dbghelp at all and returning error in those cases
// Could consider adding additional runtime for verification, but there is no good reason to do so right now
#if NDEBUG
	#define ENABLE_VERIFICATION 0
#else
	#define ENABLE_VERIFICATION 1
#endif

struct DbgHelpWrapperSession {
	ModuleCache mod_cache;

	static constexpr int MAX_INLINES = 256;

	struct CachedResult {
		uint64_t address = 0;
		// store just address not LoadedModule* as mod_cache does not keep stable ptrs
		uint64_t mod_base = 0;

		// unsafe ptr to unique_ptr<FastPdbLookup> (but currently pdbs should stay loaded until DbgHelpWrapperSession dtor)
		FastPdbLookup* pdb = nullptr;
		// unsafe ptr
		ExportTableQuery* export_table = nullptr;
		// unsafe but stable ptr if FastPdbLookup stays loaded, could also store symbol index here
		Symbol* sym = nullptr;
		uint32_t sym_idx = (uint32_t)-1;

		// -1: not yet cached
		// >=0: inlines cached
		int num_inlines = -1;
		SourceLocAndFn inlines[MAX_INLINES];

	#if ENABLE_VERIFICATION
		ULONG dbh_inl_ctx = 0;
	#endif

		void clear () {
			address = 0;
			mod_base = 0;
			pdb = nullptr;
			sym = nullptr;
			sym_idx = (uint32_t)-1;
			num_inlines = -1;

		#if ENABLE_VERIFICATION
			dbh_inl_ctx = 0;
		#endif
		}
	};

	CachedResult cached;
	
	DbgHelpWrapperSession (HANDLE hProcess): mod_cache{hProcess} {}
	~DbgHelpWrapperSession () {}
	
	// like dbghelp, copy null terminated string or truncate and _don't_ null terminate if max_len too short
	// return truncated length instead of more useful full length like snprintf would (as dbghelp does for some reason)
	static ULONG strcpy_trunc (char* dst, char const* src, ULONG max_len) {
		// strcpy_s has no way of returning actually copied length, so I am forced to do a separate strlen
		// Interestingly, this truncated copy + return truncated length operation could probably simd accelerated easily
		// TODO: do so if this shows up at all in the profiler
		ULONG len = (ULONG)strlen(src);
		strcpy_s(dst, max_len, src);
		return std::min(max_len, len);
	}

	// do base symbol lookup, store as cached
	void get_and_cache_symbol (DWORD64 Address) {
		cached.clear();

		auto* mod = mod_cache.find_module_for_addr(Address);
		if (!mod) {
			return;
		}

		// Intentionally only cache if module was found
		// as dlls in target process can be loaded after program startup
		// This way failures are not cached and instead further calls would be slower but return updated module loading
		cached.address = Address;
		cached.mod_base = mod->base_addr;

		uint64_t query_rva = Address - mod->base_addr;
		
		if (mod->pdb) {
			cached.pdb = mod->pdb.get();

			auto* sym = mod->pdb->find_symbol_for_addr(query_rva, &cached.sym_idx);
			if (sym) {
				cached.sym = sym;
			}
		}
		else {
			// No pdb found, lookup export table instead
			auto* exports = mod->load_export_table();
			if (exports) {
				cached.export_table = exports;
				
				if (exports->query_index(query_rva, &cached.sym_idx)) {
					// sym_idx set if found
				}
			}
		}
	}
	// get symbol as cached (run symbol lookup)
	void get_symbol_lazily (DWORD64 Address) {
		return get_and_cache_symbol(Address);
	}
	// get inlinesites as cached (run trace_inlinesites_for_addr)
	void get_inlinesites_lazily (DWORD64 Address) {
		get_symbol_lazily(Address);
		
		if (cached.sym) {
			// common case: pdb existed and symbol was found
			uint64_t query_rva = Address - cached.mod_base;

			if (cached.sym->inline_depth > 0) {
				cached.num_inlines = (uint32_t)cached.pdb->trace_inlinesites_for_addr(cached.sym, query_rva, cached.inlines, MAX_INLINES);
			}
		}
	}

	static bool get_symbol_info_from_export_table (
		CachedResult& cached,
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		assert(cached.address == Address);
		assert(cached.sym == nullptr); // sym_idx is not index in export table if normal symbol was found
		assert(cached.pdb == nullptr);
		if (cached.export_table == nullptr || cached.sym_idx == (uint32_t)-1)
			return false;

		uint64_t sym_rva;
		auto* mangled_name = cached.export_table->get(cached.sym_idx, &sym_rva);
		assert(mangled_name);

		Symbol->TypeIndex = 0;
		Symbol->Reserved[0] = 0;
		Symbol->Reserved[1] = 0;
		Symbol->Index = 0;
		Symbol->Size = 0;
		Symbol->ModBase = cached.mod_base;
		Symbol->Flags = 0;
		Symbol->Value = 0;
		Symbol->Address = cached.mod_base + sym_rva;
		Symbol->Register = 0;
		Symbol->Scope = 0;
		Symbol->Tag = (ULONG)SymTagEnum::SymTagPublicSymbol;

		if (Displacement)
			*Displacement = Address - Symbol->Address;
		
		Symbol->NameLen = strcpy_trunc(Symbol->Name, mangled_name, Symbol->MaxNameLen);
		return true;
	}
	static bool get_symbol_info (
		CachedResult& cached,
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		if (cached.sym) {
			// common case: pdb existed and symbol was found

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
			Symbol->Size = cached.sym->size;
			Symbol->ModBase = cached.mod_base;
			// Don't bother returning flags, i've only observed very few of them appear and
			// at least SYMFLAG_EXPORT seems to be determined based on PE export table instead of pdb, which is overcomplicated imho
			// a full 1-1 match of dbghelp behavior is probably only possibly by caching dbghelp results, which is not my goal
			Symbol->Flags = 0;
			Symbol->Value = 0;
			// HACK: __ImageBase does not return an Address unlike seemingly everything else in dbghelp, super pointless but this matches dbghelp more closely
			Symbol->Address = cached.sym->base_addr != 0 ? cached.mod_base + cached.sym->base_addr : 0;
			Symbol->Register = 0;
			Symbol->Scope = 0;
			Symbol->Tag = (ULONG)cached.sym->si_tag;

			if (Displacement) {
				*Displacement = Address - (cached.mod_base + cached.sym->base_addr);
			}
			const char* name = cached.pdb->stralloc[cached.sym->name];
			Symbol->NameLen = strcpy_trunc(Symbol->Name, name, Symbol->MaxNameLen);
			return true;
		}
		else {
			// slowpath

			if (cached.address != Address) {
				// not loaded module at address in target process
				return false;
			}

			if (cached.pdb) {
				// pdb was found, but not symbol -> symbol not found
				return false;
			}

			// No pdb found, lookup export table instead
			return get_symbol_info_from_export_table(cached, Address, Displacement, Symbol);
		}
		return false;
	}

	// Search for symbol by Address, fill SYMBOL_INFO and optional Displacement
	// Displacement: Address offset from symbol base address
	// NOTE: Symbol choice on ambiguous symbols and exact contents of SYMBOL_INFO cannot currently be replicated exactly but should be "correct" according to pdb
	BOOL SymFromAddr (
		HANDLE       hProcess,
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		get_symbol_lazily(Address);
		
		return get_symbol_info(cached, Address, Displacement, Symbol);
	}
	
	BOOL SymGetLineFromAddr64 (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		get_symbol_lazily(qwAddr);
		
		if (cached.sym) {
			// common case: pdb existed and symbol was found
			uint64_t query_rva = qwAddr - cached.mod_base;
			
			SourceLoc src_loc = {};
			if (cached.pdb->find_source_loc_for_addr(cached.sym, query_rva, &src_loc)) {
			
				// "This member is reserved for use by the operating system"
				// we just set it to 0
				Line64->Key = nullptr;
				Line64->LineNumber = src_loc.lineno;
				// For some reason dbghelp returns a non-const char* pointer here, maybe because the string is a copy anyway
				// but the string is invalidated on the next dbghelp call, so I'm not sure anybody would write into this
				// for us this means we would technically be forced to do a copy here, but it's unlikely anybody would write into this pointer
				// TODO: consider actually doing a copy after all
				Line64->FileName = (char*)src_loc.filepath;
				// "The address of the first instruction in the line"
				Line64->Address = cached.mod_base + cached.sym->base_addr + src_loc.line_start_offset;
			
				if (pdwDisplacement) {
					*pdwDisplacement = (DWORD)(qwAddr - Line64->Address);
				}
				return true;
			}
		}
		return false;
	}

	// Undocumented, but according to tracy code this returns the depth of the inline stack at the instruction at Address
	// Which requires a full pdb INLINESITE walk and full decoding of the "compressed binary annotations" of each site, which already decodes the lineinfo alongside
	DWORD SymAddrIncludeInlineTrace (
		HANDLE  hProcess,
		DWORD64 Address
	) {
		get_inlinesites_lazily(Address);

		if (cached.num_inlines >= 0) {
			return (DWORD)cached.num_inlines;
		}
		return 0;
	}
	
	static inline constexpr ULONG ARBITARY_CTX = (ULONG)0xabcd0000u;

	// This function is a bit of a mystery to me, like SymAddrIncludeInlineTrace the exact meaning of all the parameters is undocumented
	// tracy passes the same address for all the Address parameters, I have no interest in reverse engineering how it behaves if different addresses are passed for now
	// Seems to me like this could be relevant for stepping with debuggers
	// returned are CurFrameIndex, which seems to be always 0 in the tracy usecase
	// CurContext may be some kind of opaque ptr, the upper bits look like they might contain a pointer
	// but as the number itself is incremented to query the symbol info for each inlinesite, its unclear how exactly this behaves
	// or why the Address passed into SymFromInlineContext && SymGetLineFromInlineContext is not simply used as the key to find the presumably cached information
	// So for now, my implementation only allows StartContext == 0 && StartAddress == StartRetAddress == CurAddress
	// CurContext:
	//   Need to return a number the user can increment to send into SymFromInlineContext && SymGetLineFromInlineContext
	//   Set the upper bits to something arbitrary like dbghelp does to try detect invalid calls to those functions
	//   for the tracy use case we can instead simply use the Address passed into those as a key to find our cached results
	// CurFrameIndex:
	//   Return 0 always as explained above
	BOOL SymQueryInlineTrace (
		HANDLE  hProcess,
		DWORD64 StartAddress,
		DWORD   StartContext,
		DWORD64 StartRetAddress,
		DWORD64 CurAddress,
		LPDWORD CurContext,
		LPDWORD CurFrameIndex
	) {
		// Fail on (mysterious) unimplemented use cases for now (could also forward to real dbghelp for these, but should instead try to implement it for real if this ever happens)
		if (!(StartContext == 0 && CurAddress == StartAddress && CurAddress == StartRetAddress)) {
			SetLastError(ERROR_BAD_ARGUMENTS);
			return false;
		}
		
		get_inlinesites_lazily(CurAddress);

		if (cached.num_inlines >= 0) {
			*CurContext = (DWORD)ARBITARY_CTX;
			*CurFrameIndex = (DWORD)0u;
			return true;
		}
		return false;
	}

	// Returns the SYMBOL_INFO for the inlinesite based on the incremented context
	// For normal symbols most of it is set to 0 except:
	// ModBase, Address, Tag
	// Tag is SymTagEnum::SymTagInlineSite
	// Address mostly is the call Address, but sometimes isn't, and I have not yet figured out what is is
	//  it is not the function symbol base address that the inlinesite is
	//  afaik i't can't be anything like a base address for the inlinesite as the pdb does not encode it
	//  I have not yet tried to investigate further and just return the input Address for now
	BOOL SymFromInlineContext (
		HANDLE       hProcess,
		DWORD64      Address,
		ULONG        InlineContext,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		get_inlinesites_lazily(Address);

		uint32_t idx = InlineContext - ARBITARY_CTX;
		if (cached.num_inlines < 0 || idx >= (uint32_t)cached.num_inlines) {
			// InlineContext out of range, return base symbol like dbghelp seems to be doing
			return SymFromAddr(hProcess, Address, Displacement, Symbol);
		}
		// dbghelp returns results of the stack top-down
		idx = cached.num_inlines-1 - idx;

		auto& inl = cached.inlines[idx];
		
		Symbol->TypeIndex = 0;
		Symbol->Reserved[0] = 0;
		Symbol->Reserved[1] = 0;
		Symbol->Index = 0;
		Symbol->Size = 0;
		Symbol->ModBase = cached.mod_base;
		Symbol->Flags = 0;
		Symbol->Value = 0;
		Symbol->Address = Address; // TODO: Is this always simply the input or is it possibly the start of the line address?
		Symbol->Register = 0;
		Symbol->Scope = 0;
		Symbol->Tag = (ULONG)SymTagEnum::SymTagInlineSite;

		if (Displacement) {
			*Displacement = (DWORD64)(Address - Symbol->Address);
		}

		const char* name = inl.fnname;
		Symbol->NameLen = strcpy_trunc(Symbol->Name, name, Symbol->MaxNameLen);
		return true;
	}
	
	BOOL SymGetLineFromInlineContext (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		ULONG            InlineContext,
		DWORD64          qwModuleBaseAddress,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		get_inlinesites_lazily(qwAddr);

		uint32_t idx = InlineContext - ARBITARY_CTX;
		if (cached.num_inlines < 0 || idx >= (uint32_t)cached.num_inlines) {
			// InlineContext out of range, return base symbol like dbghelp seems to be doing
			return SymGetLineFromAddr64(hProcess, qwAddr, pdwDisplacement, Line64);
		}

		// dbghelp returns results of the stack top-down
		idx = cached.num_inlines-1 - idx;

		auto& inl = cached.inlines[idx];
		
		// "This member is reserved for use by the operating system"
		// we just set it to 0
		Line64->Key = nullptr;
		Line64->LineNumber = inl.lineno;
		// For some reason dbghelp returns a non-const char* pointer here, maybe because the string is a copy anyway
		// but the string is invalidated on the next dbghelp call, so I'm not sure anybody would write into this
		// for us this means we would technically be forced to do a copy here, but it's unlikely anybody would write into this pointer
		// TODO: consider actually doing a copy after all
		Line64->FileName = (char*)inl.filepath;
		// "The address of the first instruction in the line"
		Line64->Address = cached.mod_base + cached.sym->base_addr + inl.line_start_offset;
		
		if (pdwDisplacement) {
			*pdwDisplacement = (DWORD)(qwAddr - Line64->Address);
		}
		return true;
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
				if (!end)
					end = cur + wcslen(cur); // wcschr doesn't return pointers to \0
				//auto* end = cur;
				//while (*end && *end != L';')
				//	end++;

				if (end > cur) {
					PDB_Locator::extra_search_paths.emplace_back(cur, end);
				}
				cur = end;

				if (*cur == L';')
					cur++;
			}
		}
		// Pass null to set default like SymInitialize docs say
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
		// if not called SymSetSearchPath(W) yet, set default search path dbghelp would have according to dbghelp docs
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

// TODO: Handle wide string functions
// TODO: add testing/benchmarking code (& Make calling original dbghelp for implemented functions optional)

extern "C" {
	DWORD __stdcall hook_SymSetOptions (
		DWORD SymOptions
	) {
		// I have no idea if I should respect any SymOptions
		// Tracy only passes SYMOPT_LOAD_LINES (I assume not passing it makes lineinfo and inlinee line info fail?)
		// SYMOPT_UNDNAME might be another common option, but it's out of scope for me right now
		// for now this is a no-op for us

	#if ENABLE_VERIFICATION
		real_dbghelp.SymSetOptions(SymOptions);
	#endif
		return true;
	}
	
	BOOL __stdcall hook_SymSetSearchPath (
		HANDLE hProcess,
		PCSTR  SearchPath
	) {
		dbghelp_wrapper.set_search_path(SearchPath);
		
	#if ENABLE_VERIFICATION
		real_dbghelp.SymSetSearchPath(hProcess, SearchPath);
	#endif
		return true;
	}
	BOOL __stdcall hook_SymSetSearchPathW (
		HANDLE hProcess,
		PCWSTR SearchPath
	) {
		dbghelp_wrapper.set_search_pathW(SearchPath);
		
	#if ENABLE_VERIFICATION
		real_dbghelp.SymSetSearchPathW(hProcess, SearchPath);
	#endif
		return true;
	}

	BOOL __stdcall hook_SymInitialize (
		HANDLE hProcess,
		PCSTR  UserSearchPath,
		BOOL   fInvadeProcess
	) {
		dbghelp_wrapper.set_search_path(UserSearchPath);
		dbghelp_wrapper.init(hProcess);
		
	#if ENABLE_VERIFICATION
		real_dbghelp.SymInitialize(hProcess, UserSearchPath, fInvadeProcess);
	#endif
		return true;
	}
	BOOL __stdcall hook_SymInitializeW (
		HANDLE hProcess,
		PCWSTR UserSearchPath,
		BOOL   fInvadeProcess
	) {
		dbghelp_wrapper.set_search_pathW(UserSearchPath);
		dbghelp_wrapper.init(hProcess);
		
	#if ENABLE_VERIFICATION
		real_dbghelp.SymInitializeW(hProcess, UserSearchPath, fInvadeProcess);
	#endif
		return true;
	}
	
	BOOL __stdcall hook_SymCleanup (
		HANDLE hProcess
	) {
		dbghelp_wrapper.cleanup(hProcess);
		
	#if ENABLE_VERIFICATION
		real_dbghelp.SymCleanup(hProcess);
	#endif
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
		
	#if ENABLE_VERIFICATION
		SYMBOL_INFO_PACKAGE sym2;
		sym2.si = {};
		sym2.si.SizeOfStruct = sizeof(sym2.si);
		sym2.si.MaxNameLen = MAX_SYM_NAME;
		DWORD64 Displacement2 = 0;

		BOOL res2 = real_dbghelp.SymFromAddr(hProcess, Address, &Displacement2, &sym2.si);
		
		//if (res)
		//	logf("%s\n", Symbol->Name);
		//if (res2)
		//	logf("%s\n", sym2.si.Name);
	#endif
		return res;
	}
	BOOL __stdcall hook_SymFromAddrW (
		HANDLE        hProcess,
		DWORD64       Address,
		PDWORD64      Displacement,
		PSYMBOL_INFOW Symbol
	) {
		// TODO:
		return false;
		//return real_dbghelp.SymFromAddrW(hProcess, Address, Displacement, Symbol);
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
		
	#if ENABLE_VERIFICATION
		DWORD Displacement2 = 0;
		IMAGEHLP_LINE64 line2 = {};
		line2.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		BOOL res2 = real_dbghelp.SymGetLineFromAddr64(hProcess, qwAddr, &Displacement2, &line2);
		
		//if (res)
		//	logf("> %s:%d +%d\n", Line64->FileName, Line64->LineNumber, *pdwDisplacement);
		//if (res2)
		//	logf("> %s:%d +%d\n", line2.FileName, line2.LineNumber, Displacement2);
	#endif
		return res;
	}
	BOOL __stdcall hook_SymGetLineFromAddrW64 (
		HANDLE            hProcess,
		DWORD64           dwAddr,
		PDWORD            pdwDisplacement,
		PIMAGEHLP_LINEW64 Line
	) {
		// TODO:
		return false;
		//return real_dbghelp.SymGetLineFromAddrW64(hProcess, dwAddr, pdwDisplacement, Line);
	}
	
	DWORD __stdcall hook_SymAddrIncludeInlineTrace (
		HANDLE  hProcess,
		DWORD64 Address
	) {
		DWORD num_inlines = 0;
		if (dbghelp_wrapper.sess.has_value())
			num_inlines = dbghelp_wrapper.sess->SymAddrIncludeInlineTrace(hProcess, Address);
		
	#if ENABLE_VERIFICATION
		DWORD num_inlines2 = real_dbghelp.SymAddrIncludeInlineTrace(hProcess, Address);
	#endif
		return num_inlines;
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
		BOOL res = 0;
		if (dbghelp_wrapper.sess.has_value())
			res = dbghelp_wrapper.sess->SymQueryInlineTrace(hProcess, StartAddress, StartContext, StartRetAddress, CurAddress, CurContext, CurFrameIndex);
		
	#if ENABLE_VERIFICATION
		DWORD Context2 = 0;
		DWORD FrameIndex2 = 0;
		BOOL res2 = real_dbghelp.SymQueryInlineTrace(hProcess, StartAddress, StartContext, StartRetAddress, CurAddress, &Context2, &FrameIndex2);

		assert(FrameIndex2 == 0);
		// HACK: to properly test custom inline results against dbghelp, need to remember dbghelp returned context and reconstruct incremented context later
		if (res && res2 && dbghelp_wrapper.sess->cached.address == CurAddress) {
			dbghelp_wrapper.sess->cached.dbh_inl_ctx = (ULONG)Context2;
		}
	#endif
		return res;
	}
	
	BOOL __stdcall hook_SymFromInlineContext (
		HANDLE       hProcess,
		DWORD64      Address,
		ULONG        InlineContext,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		DWORD64 displacement;
		BOOL res = 0;
		if (dbghelp_wrapper.sess.has_value())
			res = dbghelp_wrapper.sess->SymFromInlineContext(hProcess, Address, InlineContext, &displacement, Symbol);
		if (Displacement) {
			*Displacement = displacement;
		}
		
	#if ENABLE_VERIFICATION
		if (dbghelp_wrapper.sess->cached.address == Address) {
			ULONG dbh_InlineContext = dbghelp_wrapper.sess->cached.dbh_inl_ctx;
			dbh_InlineContext += InlineContext - DbgHelpWrapperSession::ARBITARY_CTX;

			SYMBOL_INFO_PACKAGE sym2;
			sym2.si = {};
			sym2.si.SizeOfStruct = sizeof(sym2.si);
			sym2.si.MaxNameLen = MAX_SYM_NAME;
			DWORD64 Displacement2 = 0;

			BOOL res2 = real_dbghelp.SymFromInlineContext(hProcess, Address, dbh_InlineContext, &Displacement2, &sym2.si);
			
			// if context out of range, base symbol is returned
			if (res2 && sym2.si.Tag == (ULONG)SymTagEnum::SymTagInlineSite) {
				assert(res);
				assert(sym2.si.Size == 0);
				assert(sym2.si.ModBase == Symbol->ModBase);
				assert(sym2.si.Flags == 0);
				assert(sym2.si.Value == 0);
				//assert(sym2.si.Address == Address);
				assert(sym2.si.Register == 0);
				assert(sym2.si.Scope == 0);
				assert(sym2.si.Tag == (ULONG)SymTagEnum::SymTagInlineSite);
				//assert(Displacement2 == displacement);
			}

			//auto& cached = dbghelp_wrapper.sess->cached;
			//if (res)
			//	logf("%s sym+%d\n", Symbol->Name, Symbol->Address - (cached.sym->base_addr + cached.mod_base));
			//if (res2)
			//	logf("%s sym+%d\n", sym2.si.Name, sym2.si.Address - (cached.sym->base_addr + cached.mod_base));
			//printf("");
		}
	#endif
		return res;
	}
	BOOL __stdcall hook_SymFromInlineContextW (
		HANDLE        hProcess,
		DWORD64       Address,
		ULONG         InlineContext,
		PDWORD64      Displacement,
		PSYMBOL_INFOW Symbol
	) {
		// TODO:
		return false;
		//return real_dbghelp.SymFromInlineContextW(hProcess, Address, InlineContext, Displacement, Symbol);
	}
	
	
	BOOL __stdcall hook_SymGetLineFromInlineContext (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		ULONG            InlineContext,
		DWORD64          qwModuleBaseAddress,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		DWORD displacement;
		BOOL res = 0;
		if (dbghelp_wrapper.sess.has_value())
			res = dbghelp_wrapper.sess->SymGetLineFromInlineContext(hProcess, qwAddr, InlineContext, qwModuleBaseAddress, &displacement, Line64);
		if (pdwDisplacement) {
			*pdwDisplacement = displacement;
		}
		
	#if ENABLE_VERIFICATION
		if (dbghelp_wrapper.sess->cached.address == qwAddr) {
			ULONG dbh_InlineContext = dbghelp_wrapper.sess->cached.dbh_inl_ctx;
			dbh_InlineContext += InlineContext - DbgHelpWrapperSession::ARBITARY_CTX;
			
			DWORD Displacement2 = 0;
			IMAGEHLP_LINE64 line2 = {};
			line2.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

			BOOL res2 = real_dbghelp.SymGetLineFromInlineContext(hProcess, qwAddr, dbh_InlineContext, qwModuleBaseAddress, &Displacement2, &line2);
			
			if (res2) {
				assert(res);
				//assert(strcmp(line2.FileName, Line64->FileName) == 0);
				//assert(line2.LineNumber == Line64->LineNumber);
				//assert(line2.Address == Line64->Address);
				//assert(Displacement2 == displacement);
			}

			//if (res)
			//	logf("> %s:%d +%d\n", Line64->FileName, Line64->LineNumber, *pdwDisplacement);
			//if (res2)
			//	logf("> %s:%d +%d\n", line2.FileName, line2.LineNumber, Displacement2);
			//printf("");
		}
	#endif
		return res;
	}
	BOOL __stdcall hook_SymGetLineFromInlineContextW (
		HANDLE            hProcess,
		DWORD64           dwAddr,
		ULONG             InlineContext,
		DWORD64           qwModuleBaseAddress,
		PDWORD            pdwDisplacement,
		PIMAGEHLP_LINEW64 Line
	) {
		// TODO:
		return false;
		//return real_dbghelp.SymGetLineFromInlineContextW(hProcess, dwAddr, InlineContext, qwModuleBaseAddress, pdwDisplacement, Line);
	}
}
