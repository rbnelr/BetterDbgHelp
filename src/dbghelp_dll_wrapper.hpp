#pragma once
#include "dbghelp_api.hpp"
#include "pdb/lookup.hpp"
#include "pdb/module_lookup.hpp"
#include "pdb/sym_result.hpp"

// Verification:
// Optionally verify all implemented dbghelp API calls against real dbghelp.dll
// Disabling is using compile-time macro for better optimization
// Currently real dbghelp.dll is still loaded and not-implemented calls are forwarded to it
//  actually calling a mix of implemented and non-implemented functions should in theory be supported but is completely untested
// Could consider not loading dbghelp at all and returning error in those cases
// Could consider adding additional runtime for verification, but there is no good reason to do so right now
#if NDEBUG
	//#define ENABLE_VERIFICATION 0
	#define ENABLE_VERIFICATION 1
#else
	#define ENABLE_VERIFICATION 1
#endif

struct DbgHelpWrapperSession {
	static constexpr int MAX_INLINES = 256;

	struct CachedResult {
		uint64_t address = 0;
		// store just address not LoadedModule* (as mod_cache did not keep stable ptrs, now it does)
		// avoiding indirection probably can't hurt
		uint64_t mod_base = 0;
		const char* mod_name = nullptr;

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
		bool has_mismatch = false;
	#endif

		REL_FORCEINLINE void clear () {
			address = 0;
			mod_base = 0;
			pdb = nullptr;
			sym = nullptr;
			sym_idx = (uint32_t)-1;
			num_inlines = -1;

		#if ENABLE_VERIFICATION
			dbh_inl_ctx = 0;
			has_mismatch = false;
		#endif
		}
	};
	
	ModuleCache mod_cache;
	CachedResult cached;
#if ENABLE_VERIFICATION
	MismatchCounts mismatches;
#endif
	
	DbgHelpWrapperSession (HANDLE hProcess): mod_cache{hProcess} {}
	~DbgHelpWrapperSession () {

	#if ENABLE_VERIFICATION
		mismatches.print();
	#endif
	}
	HANDLE hProcess () const {
		return mod_cache.hprocess;
	}
	
	// like dbghelp, copy null terminated string or truncate and _don't_ null terminate if max_len too short
	// return truncated length instead of more useful full length like snprintf would (as dbghelp does for some reason)
	static ULONG strcpy_trunc (char* dst, char const* src, ULONG max_len) {
		// I'm unsure which C str* function if any do this
		// strncpy supposedly fills the destination with zeros past the copied string, which we definitely don't want
		// as the destination buffer tends to be large
		// strlen + memcpy is not as fast as it could be
	#if 0
		ULONG len = 0;
		for (; len < max_len && src[len] != '\0'; len++) {
			dst[len] = src[len];
		}
		if (len < max_len)
			dst[len] = '\0';
		// return written length without null terminator
		return len;
	#elif 0
		ULONG len = (ULONG)std::min(strlen(src), (size_t)max_len);
		memcpy(dst, src, len);
		if (len < max_len)
			dst[len] = '\0';
		return len;
	#else
		ULONG written_len = VirtualMemoryVector::fast_strcpy_trunc(dst, max_len, src);
		assert(written_len <= max_len);
		if (written_len < max_len) {
			assert(dst[written_len] == '\0');
		}
		return written_len;
	#endif
	}

	// do base symbol lookup, store as cached
	__declspec(noinline) void get_and_cache_symbol (DWORD64 Address) {
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
		cached.mod_name = mod->ansi_filename.c_str();

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
			auto* exports = mod->exports.get();
			if (exports) {
				cached.export_table = exports;
				
				if (exports->query_index(query_rva, &cached.sym_idx)) {
					// sym_idx set if found
				}
			}
		}
	}

	// TODO: could make these two functions return pointer to CachedResult, which would enable caching multiple results
	// tracy does not require caching multiple,
	// but debuggers may keep track of multiple symbols and stacktraces and get lineinfo later on UI interaction
	// Though this should only be done if the callers behavior is understood
	// Another reason would be if multiple "sessions" are needed, we could have both ModuleCache and CachedResult be keyed based on hProcess + Address

	// get symbol from cache (run symbol lookup)
	REL_FORCEINLINE void get_symbol_from_cache (DWORD64 Address) {
		if (cached.address == Address) {
			// symbol already cached
			return;
		}

	#if ENABLE_VERIFICATION
		// Assume Sym* accesses are for same symbol are consecutive for purposes of mismatch counting
		// any newly cached symbol will count once for mismatches.total_mismatches via flag to allow accurate mismatch rate
		mismatches.total_checks++;
	#endif

		get_and_cache_symbol(Address);
	}
	// get inlinesites from cache (run trace_inlinesites_for_addr)
	REL_FORCEINLINE void get_inlinesites_from_cache (DWORD64 Address) {
		if (cached.address == Address && cached.num_inlines >= 0) {
			assert(cached.sym);
			// fastpath: inlinesites already cached
			return;
		}

		get_symbol_from_cache(Address);
		
		if (cached.sym) {
			// pdb existed and symbol was found

			cached.num_inlines = 0;
			uint64_t query_rva = Address - cached.mod_base;

			if (cached.sym->inline_depth > 0) {
				cached.num_inlines = cached.pdb->trace_inlinesites_for_addr(cached.sym, query_rva, cached.inlines, MAX_INLINES);
			}

			assert(cached.num_inlines >= 0);
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
	REL_FORCEINLINE static bool get_symbol_info (
		CachedResult& cached,
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		if (cached.sym) {
			// fastpath: pdb existed and symbol was found
			assert(cached.pdb);
			assert(cached.address == Address);

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
	REL_FORCEINLINE BOOL SymFromAddr (
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		ZoneScoped;

		get_symbol_from_cache(Address);

		return get_symbol_info(cached, Address, Displacement, Symbol);
	}
	
	REL_FORCEINLINE BOOL SymGetLineFromAddr64 (
		DWORD64          qwAddr,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		get_symbol_from_cache(qwAddr);
		
		if (cached.sym) {
			// fastpath: pdb existed and symbol was found
			assert(cached.pdb);
			assert(cached.address == qwAddr);
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

	static inline constexpr ULONG ARBITARY_INLINE_CTX = (ULONG)0xabcd0000u;

	REL_FORCEINLINE DWORD SymAddrIncludeInlineTrace (
		DWORD64 Address
	) {
		ZoneScoped;

		get_inlinesites_from_cache(Address);

		if (cached.num_inlines >= 0) {
			return (DWORD)cached.num_inlines;
		}
		return 0;
	}

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
	REL_FORCEINLINE BOOL SymQueryInlineTrace (
		DWORD64 StartAddress,
		DWORD   StartContext,
		DWORD64 StartRetAddress,
		DWORD64 CurAddress,
		LPDWORD CurContext,
		LPDWORD CurFrameIndex
	) {
		ZoneScoped;

		// Fail on (mysterious) unimplemented use cases for now (could also forward to real dbghelp for these, but should instead try to implement it for real if this ever happens)
		if (StartContext == 0 && CurAddress == StartAddress && CurAddress == StartRetAddress) {
			get_inlinesites_from_cache(CurAddress);

			if (cached.num_inlines >= 0) {
				*CurContext = (DWORD)ARBITARY_INLINE_CTX;
				*CurFrameIndex = (DWORD)0u;
				return true; true;
			}
		}
		else {
			SetLastError(ERROR_BAD_ARGUMENTS);
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
	REL_FORCEINLINE BOOL SymFromInlineContext (
		DWORD64      Address,
		ULONG        InlineContext,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		ZoneScoped;

		get_inlinesites_from_cache(Address);

		uint32_t idx = InlineContext - DbgHelpWrapperSession::ARBITARY_INLINE_CTX;
		if (cached.num_inlines < 0 || idx >= (uint32_t)cached.num_inlines) {
			// InlineContext out of range, return base symbol like dbghelp seems to be doing
			return get_symbol_info(cached, Address, Displacement, Symbol);
		}
		else {
			// dbghelp returns results of the stack top-down
			idx = (uint32_t)cached.num_inlines-1 - idx;

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
		return false;
	}
	
	REL_FORCEINLINE BOOL SymGetLineFromInlineContext (
		DWORD64          qwAddr,
		ULONG            InlineContext,
		DWORD64          qwModuleBaseAddress,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		ZoneScoped;

		get_inlinesites_from_cache(qwAddr);

		uint32_t idx = InlineContext - DbgHelpWrapperSession::ARBITARY_INLINE_CTX;
		if (cached.num_inlines < 0 || idx >= (uint32_t)cached.num_inlines) {
			// InlineContext out of range, return base symbol like dbghelp seems to be doing
			// TODO: this does unnecessary checks right now
			return SymGetLineFromAddr64(qwAddr, pdwDisplacement, Line64);
		}

		// dbghelp returns results of the stack top-down
		idx = (uint32_t)cached.num_inlines-1 - idx;

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

// Mismatches and diff logging
#if ENABLE_VERIFICATION
	// Count number of symbol with at least one mismatch to get accurate mismatch rate
	void count_checks (bool match) {
		if (!match && !cached.has_mismatch) {
			cached.has_mismatch = true;
			mismatches.total_mismatches++;
		}
	}

	void compare_SymFromAddr (
		HANDLE       hProcess,
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol,
		BOOL success
	) {
		SYMBOL_INFO_PACKAGE sym2 = {}; // null terminate even if MaxNameLen reached
		sym2.si.SizeOfStruct = sizeof(sym2.si);
		sym2.si.MaxNameLen = MAX_SYM_NAME;
		DWORD64 Displacement2 = 0;

		BOOL success2 = real_dbghelp.SymFromAddr(hProcess, Address, &Displacement2, &sym2.si);

		auto compare = [&] () {
			if (success != success2) {
				mismatches.other++;
				return false;
			}

			if (success) {
				// dbghelp.dll not returning module name, assume it's correct
				//if (nullable_strcmp(module_path, r.module_path) != 0) return false;

				ULONG len = std::min(Symbol->MaxNameLen, sym2.si.MaxNameLen);
				if (Symbol->NameLen != sym2.si.NameLen || !nullable_strcmp(Symbol->Name, sym2.si.Name)) {
					mismatches.symbol_mismatch++;
					return false;
				}
				
				bool match = true;

				//if (!( Symbol->Scope == sym2.si.Scope
				//	//&& Symbol->Size == sym2.si.Size
				//	&& Symbol->Flags == sym2.si.Flags
				//	&& Symbol->Value == sym2.si.Value
				//	//&& Symbol->Address == sym2.si.Address
				//	&& Symbol->Register == sym2.si.Register
				//	&& Symbol->Scope == sym2.si.Scope
				//	&& Symbol->Tag == sym2.si.Tag
				//	)) {
				//	mismatches.info_mimatch++;
				//
				//	if (Symbol->Size != sym2.si.Size)
				//		mismatches.info_size_mimatch++;
				//	if (Symbol->Flags != sym2.si.Flags)
				//		mismatches.info_flags_mimatch++;
				//	if (Symbol->Tag != sym2.si.Tag)
				//		mismatches.info_tag_mimatch++;
				//
				//	match = false;
				//}
				//
				//if (Displacement) {
				//	if (*Displacement != Displacement2) {
				//		mismatches.other++;
				//		match = false;
				//	}
				//}

				return match;
			}
			else {
				// If both throw error it counts as a match as comparing errors may be hard
			}

			return true;
		};

		bool match = compare();
		count_checks(match);

		if (!match) {
			print_diff_sym("SymFromAddr", Address,
				ResultSym{ success, Symbol, Displacement },
				ResultSym{ success2, &sym2.si, &Displacement2 });
		}
	}
	
	void compare_SymGetLineFromAddr64 (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64,
		BOOL success
	) {
		
		DWORD Displacement2 = 0;
		IMAGEHLP_LINE64 line2 = {};
		line2.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

		BOOL success2 = real_dbghelp.SymGetLineFromAddr64(hProcess, qwAddr, &Displacement2, &line2);

		auto compare = [&] () {
			if (success != success2) {
				if (success) {
					mismatches.DH_no_source++;
				} else {
					mismatches.source_mismatch++;
				}
				return false;
			}

			if (success) {
				if (!nullable_strcmp(Line64->FileName, line2.FileName)
					|| Line64->LineNumber != line2.LineNumber
					|| Line64->Address != line2.Address ) {
					mismatches.source_mismatch++;
					return false;
				}
			}

			// Displacement will always match if Line64.Address matches
			if (pdwDisplacement)
				assert(*pdwDisplacement == Displacement2);

			return true;
		};

		bool match = compare();
		count_checks(match);

		if (!match) {
			print_diff_line("SymGetLineFromAddr64", qwAddr,
				ResultLine{ success, Line64, pdwDisplacement },
				ResultLine{ success2, &line2, &Displacement2 });
		}
	}
	
	// NOTE: inline_mimatch right now will count any mismatch in the API calls for the inline stack instead of count it once like I do in SymResult

	void compare_SymAddrIncludeInlineTrace (
		HANDLE       hProcess,
		DWORD64      Address,
		DWORD num_inlines
	) {
		DWORD num_inlines2 = real_dbghelp.SymAddrIncludeInlineTrace(hProcess, Address);

		bool match = true;
		if (num_inlines != num_inlines2) {
			mismatches.inline_mimatch++;
			match = false;
		}
		count_checks(match);

		if (!match) {
			print_diff_SymAddrIncludeInlineTrace("SymAddrIncludeInlineTrace", Address, num_inlines, num_inlines2);
		}
	}

	void compare_SymQueryInlineTrace (
		HANDLE  hProcess,
		DWORD64 StartAddress,
		DWORD   StartContext,
		DWORD64 StartRetAddress,
		DWORD64 CurAddress,
		LPDWORD CurContext,
		LPDWORD CurFrameIndex,
		BOOL success
	) {
		DWORD Context2 = 0;
		DWORD FrameIndex2 = 0;
		BOOL success2 = real_dbghelp.SymQueryInlineTrace(hProcess, StartAddress, StartContext, StartRetAddress, CurAddress, &Context2, &FrameIndex2);

		assert(FrameIndex2 == 0);
		// HACK: to properly test custom inline results against dbghelp, need to remember dbghelp returned context and reconstruct incremented context later
		if (success && success2 && cached.address == CurAddress) {
			cached.dbh_inl_ctx = (ULONG)Context2;
		}

		bool match = true;
		if (success != success2 || *CurFrameIndex != FrameIndex2) {
			mismatches.inline_mimatch++;
			match = false;
		}
		count_checks(match);

		if (!match) {
			print_diff_SymQueryInlineTrace("SymQueryInlineTrace", CurAddress, success, success2);
		}
	}

	void compare_SymFromInlineContext (
		HANDLE       hProcess,
		DWORD64      Address,
		ULONG        InlineContext,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol,
		BOOL success
	) {
		if (cached.address == Address) {
			ULONG dbh_InlineContext = cached.dbh_inl_ctx;
			int inl_idx = (int)(InlineContext - ARBITARY_INLINE_CTX);
			dbh_InlineContext += InlineContext - ARBITARY_INLINE_CTX;

			SYMBOL_INFO_PACKAGE sym2 = {}; // null terminate even if MaxNameLen reached
			sym2.si.SizeOfStruct = sizeof(sym2.si);
			sym2.si.MaxNameLen = MAX_SYM_NAME;
			DWORD64 Displacement2 = 0;

			BOOL success2 = real_dbghelp.SymFromInlineContext(hProcess, Address, dbh_InlineContext, &Displacement2, &sym2.si);
			
			// if context out of range, base symbol is returned
			if (success2 && sym2.si.Tag == (ULONG)SymTagEnum::SymTagInlineSite) {
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

			auto compare = [&] () {
				if (success != success2) {
					return false;
				}

				if (success) {
					// dbghelp.dll not returning module name, assume it's correct
					//if (nullable_strcmp(module_path, r.module_path) != 0) return false;

					ULONG len = std::min(Symbol->MaxNameLen, sym2.si.MaxNameLen);
					if (Symbol->NameLen != sym2.si.NameLen || !nullable_strcmp(Symbol->Name, sym2.si.Name)) {
						return false;
					}
			
					bool match = true;

					//if (!( Symbol->Scope == sym2.si.Scope
					//	&& Symbol->Size == sym2.si.Size
					//	&& Symbol->Flags == sym2.si.Flags
					//	&& Symbol->Value == sym2.si.Value
					//	//&& Symbol->Address == sym2.si.Address
					//	&& Symbol->Register == sym2.si.Register
					//	&& Symbol->Scope == sym2.si.Scope
					//	&& Symbol->Tag == sym2.si.Tag
					//	)) {
					//	match = false;
					//}
					//
					//if (Displacement) {
					//	if (*Displacement != Displacement2) {
					//		match = false;
					//	}
					//}

					return match;
				}
				else {
					// If both throw error it counts as a match as comparing errors may be hard
				}

				return true;
			};

			bool match = compare();
			if (!match) {
				mismatches.inline_mimatch++;
			}

			count_checks(match);
			
			if (!match) {
				print_diff_sym("SymFromInlineContext", Address,
					ResultSym{ success, Symbol, Displacement },
					ResultSym{ success2, &sym2.si, &Displacement2 }, inl_idx);
			}
		}
	}

	void compare_SymGetLineFromInlineContext (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		ULONG            InlineContext,
		DWORD64          qwModuleBaseAddress,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64,
		BOOL success
	) {
		if (cached.address == qwAddr) {
			ULONG dbh_InlineContext = cached.dbh_inl_ctx;
			int inl_idx = (int)(InlineContext - ARBITARY_INLINE_CTX);
			dbh_InlineContext += InlineContext - ARBITARY_INLINE_CTX;
			
			DWORD Displacement2 = 0;
			IMAGEHLP_LINE64 line2 = {};
			line2.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

			BOOL success2 = real_dbghelp.SymGetLineFromInlineContext(hProcess, qwAddr, dbh_InlineContext, qwModuleBaseAddress, &Displacement2, &line2);
			
			auto compare = [&] () {
				if (success != success2) {
					return false;
				}

				if (success) {
					if (!nullable_strcmp(Line64->FileName, line2.FileName)
						|| Line64->LineNumber != line2.LineNumber
						|| Line64->Address != line2.Address ) {
						return false;
					}
				}

				// Displacement will always match if Line64.Address matches
				if (pdwDisplacement)
					assert(*pdwDisplacement == Displacement2);

				return true;
			};

			bool match = compare();
			if (!match) {
				mismatches.inline_mimatch++;
			}
			count_checks(match);

			if (!match) {
				print_diff_line("SymGetLineFromInlineContext", qwAddr,
					ResultLine{ success, Line64, pdwDisplacement },
					ResultLine{ success2, &line2, &Displacement2 }, inl_idx);
			}
		}
	}
	
	struct ResultSym {
		BOOL success;
		PSYMBOL_INFO Symbol;
		PDWORD64 Displacement;
	};
	struct ResultLine {
		BOOL success;
		PIMAGEHLP_LINE64 Line;
		PDWORD Displacement;
	};
	
	uint64_t last_mismatch_addr = 0;
	void print_diff_header (uint64_t addr) {
		// Print mismatch infos only once for each series of api calls with same address
		if (addr != last_mismatch_addr) {
			last_mismatch_addr = addr;
			// use module name we determined ourselves, this means the listed module name should represent the pdb we used, which seems useful
			// in cases of dbghelp finding a different module, possibly because of dynamic module unloading and re-loading, this may be confusing
			auto* mod_name = cached.address == addr && cached.mod_name ? cached.mod_name : "[unknown]";
			uint64_t rel_addr = cached.address == addr ? addr - cached.mod_base : addr;
			// use custom determined symbol name, in other mismatch logging I used dbghelp result in case we have a failure,
			// but as we log mismatches on each api call, and the user may not have called SymFromAddr yet, this is complicated, so avoid this for now
			const char* sym_name = cached.address == addr && cached.sym && cached.pdb && cached.sym->name >= 0 ?
				cached.pdb->stralloc[cached.sym->name] :
				"[unknown]";

			logf_quiet("[BetterDbgHelp] !! Mismatch [%s+%llx] (%s):\n", mod_name, rel_addr, sym_name);
		}
	}

	void print_diff_sym (const char* api, DWORD64 Address, const ResultSym cus, const ResultSym dh, int inl_ctx=-1) {
		print_diff_header(Address);
		
		if (inl_ctx < 0) logf_quiet("> %s", api);
		else             logf_quiet("> %s(%d)", api, inl_ctx);
		if (cus.success != dh.success) {
			logf_quiet(" custom: %s | dbghelp: %s\n",
			    cus.success ? "Success":"Fail",
			     dh.success ? "Success":"Fail");
			return;
		}
		logf_quiet("\n");

		if (cus.success) assert(cus.Symbol->Name && strlen(cus.Symbol->Name) == cus.Symbol->NameLen);
		if ( dh.success) assert( dh.Symbol->Name && strlen(dh.Symbol->Name) == dh.Symbol->NameLen);

		if (!nullable_strcmp(cus.Symbol->Name, dh.Symbol->Name)) {
			logf_quiet(" >  custom: \"%s\" !=\n"
			     " > dbghelp: \"%s\"\n", cus.Symbol->Name, dh.Symbol->Name);
		}

		//if (!sym_info_equal_diff(*cus.Symbol, *dh.Symbol)) {
		//	print_diff_SYMBOL_INFO_quiet(*cus.Symbol, *dh.Symbol);
		//}
		
		assert(dh.Displacement);
		if (cus.Displacement) {
			if (*dh.Displacement != *cus.Displacement) {
				logf_quiet(" > Displacement: custom: %llx != dbghelp: %llx\n", *dh.Displacement, *cus.Displacement);
			}
		}
	}
	void print_diff_line (const char* api, DWORD64 Address, const ResultLine cus, const ResultLine dh, int inl_ctx=-1) {
		print_diff_header(Address);
		
		if (inl_ctx < 0) logf_quiet("> %s\n", api);
		else             logf_quiet("> %s(%d)\n", api, inl_ctx);

		//if (cus.success != dh.success) {
		//	logf_quiet("custom: %s | dbghelp: %s\n",
		//		cus.success ? "Success":"Fail",
		//		 dh.success ? "Success":"Fail");
		//}

		if (cus.success) {
			logf_quiet(" >  custom: [%llx] \"%s:%d\" !=\n", cus.Line->Address, cus.Line->FileName, cus.Line->LineNumber);
		}
		else {
			logf_quiet(" >  custom: (No source info) !=\n");
		}
				
		if (dh.success) {
			logf_quiet(" > dbghelp: [%llx] \"%s:%d\"\n", dh.Line->Address, dh.Line->FileName, dh.Line->LineNumber);
		}
		else {
			logf_quiet(" > dbghelp: (No source info)\n");
		}
	}
	void print_diff_SymAddrIncludeInlineTrace (const char* api, DWORD64 Address, DWORD num_inlines, DWORD dh_num_inlines) {
		print_diff_header(Address);
		
		logf_quiet("> %s:  custom: %u | dbghelp: %u\n", api,
		    num_inlines, dh_num_inlines);
	}
	void print_diff_SymQueryInlineTrace (const char* api, DWORD64 Address, BOOL success, DWORD dh_success) {
		print_diff_header(Address);
		
		logf_quiet("> %s:  custom: %s | dbghelp: %s\n", api,
		       success ? "Success":"Fail",
		    dh_success ? "Success":"Fail");
	}
#endif
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
			
			auto var = get_env_var(L"_NT_SYMBOL_PATH");
			if (!var.empty()) {
				PDB_Locator::extra_search_paths.emplace_back(std::move(var));
			}
			var = get_env_var(L"_NT_ALTERNATE_SYMBOL_PATH");
			if (!var.empty()) {
				PDB_Locator::extra_search_paths.emplace_back(std::move(var));
			}
		}
	}

	bool check_sess (HANDLE hProcess) {
		return sess.has_value() && sess->hProcess() == hProcess;
	}
	void init (HANDLE hProcess) {
		// if not called SymSetSearchPath(W) yet, set default search path dbghelp would have according to dbghelp docs
		if (PDB_Locator::extra_search_paths.empty()) {
			set_search_pathW(nullptr);
		}
		sess.emplace(hProcess);
	}
	void cleanup (HANDLE hProcess) {
		if (sess->hProcess() == hProcess) {
			sess.reset();
		}
	}

	DbgHelpWrapper () {
		real_dbghelp.load_if_not_loaded_yet();
	}
	~DbgHelpWrapper () {
		real_dbghelp.unload();
	}
};
inline DbgHelpWrapper dbghelp_wrapper;

// The functions from dbghelp.dll we want to accelerate
// the rest are in dbghelp_dll_forward.hpp

// TODO: Handle wide string functions
// TODO: add testing/benchmarking code (& Make calling original dbghelp for implemented functions optional)

extern "C" {
	DWORD __stdcall hook_SymSetOptions (
		DWORD SymOptions
	) {
		ZoneScoped;

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
		ZoneScoped;

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
		ZoneScoped;

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
		ZoneScoped;

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
		ZoneScoped;

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
		ZoneScoped;

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
		BOOL success = false;
		if (dbghelp_wrapper.check_sess(hProcess)) {
			success = dbghelp_wrapper.sess->SymFromAddr(Address, Displacement, Symbol);

		#if ENABLE_VERIFICATION
			dbghelp_wrapper.sess->compare_SymFromAddr(hProcess, Address, Displacement, Symbol, success);
		#endif
		}
		return success;
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
		BOOL success = false;
		if (dbghelp_wrapper.check_sess(hProcess)) {
			success = dbghelp_wrapper.sess->SymGetLineFromAddr64(qwAddr, pdwDisplacement, Line64);

		#if ENABLE_VERIFICATION
			dbghelp_wrapper.sess->compare_SymGetLineFromAddr64(hProcess, qwAddr, pdwDisplacement, Line64, success);
		#endif
		}
		return success;
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
		if (dbghelp_wrapper.check_sess(hProcess)) {
			num_inlines = dbghelp_wrapper.sess->SymAddrIncludeInlineTrace(Address);

		#if ENABLE_VERIFICATION
			dbghelp_wrapper.sess->compare_SymAddrIncludeInlineTrace(hProcess, Address, num_inlines);
		#endif
		}
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
		BOOL success = false;
		if (dbghelp_wrapper.check_sess(hProcess)) {
			success = dbghelp_wrapper.sess->SymQueryInlineTrace(StartAddress, StartContext, StartRetAddress, CurAddress, CurContext, CurFrameIndex);

		#if ENABLE_VERIFICATION
			dbghelp_wrapper.sess->compare_SymQueryInlineTrace(hProcess, StartAddress, StartContext, StartRetAddress, CurAddress, CurContext, CurFrameIndex, success);
		#endif
		}
		return success;
	}
	
	BOOL __stdcall hook_SymFromInlineContext (
		HANDLE       hProcess,
		DWORD64      Address,
		ULONG        InlineContext,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		BOOL success = false;
		if (dbghelp_wrapper.check_sess(hProcess)) {
			success = dbghelp_wrapper.sess->SymFromInlineContext(Address, InlineContext, Displacement, Symbol);

		#if ENABLE_VERIFICATION
			dbghelp_wrapper.sess->compare_SymFromInlineContext(hProcess, Address, InlineContext, Displacement, Symbol, success);
		#endif
		}
		return success;
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
		BOOL success = false;
		if (dbghelp_wrapper.check_sess(hProcess)) {
			success = dbghelp_wrapper.sess->SymGetLineFromInlineContext(qwAddr, InlineContext, qwModuleBaseAddress, pdwDisplacement, Line64);

		#if ENABLE_VERIFICATION
			dbghelp_wrapper.sess->compare_SymGetLineFromInlineContext(hProcess, qwAddr, InlineContext, qwModuleBaseAddress, pdwDisplacement, Line64, success);
		#endif
		}
		return success;
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
