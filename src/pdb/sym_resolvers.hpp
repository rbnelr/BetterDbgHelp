#pragma once
#include "util.hpp"
#include "sym_result.hpp"
#include "module_lookup.hpp"
#include "dbghelp_api.hpp"

class SymResolver {
	HANDLE inspectee;

	ModuleCache mod_cache;
	
	TimerMeasurement tfind_symbol_for_addr = TimerMeasurement("find_symbol_for_addr");
	TimerMeasurement tfind_source_loc_for_addr= TimerMeasurement("find_source_loc_for_addr");
	TimerMeasurement ttrace_inlinesites = TimerMeasurement("trace_inlinesites");
	TimerMeasurement tCombinedAddr2sym = TimerMeasurement("CombinedAddr2sym");

public:
	SymResolver (HANDLE inspectee): inspectee{inspectee} {}
	
	void measure_addr2sym (char* ptr) {
		SymResult res;
		measure_addr2sym(ptr, &res);
		res.dont_optimize_away();
	}
	
	bool addr2sym (void* ptr, SymResult* res) {
		res->clear();
		uintptr_t addr = (uintptr_t)ptr;

		auto* mod = mod_cache.find_module_for_addr(inspectee, addr);
		if (!mod) {
			res->err = "Module not found";
			return false;
		}

		uintptr_t mod_raddr = addr - mod->base_addr;
		res->module_path = mod->ansi_path.c_str();

		if (!mod->pdb) {
			auto* exports = mod->load_export_table();

			if (exports) {
				auto* mangled_name = exports->query(mod_raddr);
				if (mangled_name) {
					res->sym_name = mangled_name;
					return true;
				}
			}
			res->err = "Module pdb not found and not found in export table";
			return false;
		}

		auto* sym = mod->pdb->find_symbol_for_addr(mod_raddr);
		if (!sym) {
			res->err = "Symbol not found";
			return false;
		}
		
		res->sym_name = mod->pdb->stralloc[sym->name];
		
		res->info.TypeIndex = 0; // sym->si_type_index;
		res->info.Reserved[0] = 0;
		res->info.Reserved[1] = 0;
		res->info.Index = 0; //sym->si_index;
		res->info.Size = sym->size;
		res->info.ModBase = mod->base_addr;
		res->info.Flags = sym->si_flags;
		res->info.Value = 0; //sym->si_value;
		res->info.Address = sym->base_addr != 0 ? sym->base_addr + mod->base_addr : 0; // HACK: __ImageBase does not return an Address unlike seemingly everything else in dbghelp
		res->info.Register = 0; //sym->si_register;
		res->info.Scope = 0; //sym->si_scope;
		res->info.Tag = (ULONG)sym->si_tag;
		//res->info.NameLen = strlen(res->sym_name);

		SourceLoc src_loc = {};
		if (mod->pdb->find_source_loc_for_addr(sym, mod_raddr, &src_loc)) {
			res->src_filepath = src_loc.filepath;
			res->src_lineno = src_loc.lineno;
		}
		
		if (sym->inline_depth > 0) {
			res->num_inlines = mod->pdb->trace_inlinesites_for_addr(sym, mod_raddr, res->inlines, SymResult::MAX_INLINES);
		}

		return res->valid();
	}

	// In cases where multiple symbols exist at one address, we will sometimes pick a different one than dbghelp
	// This function was meant to test if the symbol dbghelp returned existed in the pdb at all (to differentiate bugs from simply picking the wrong symbol which I can't do anything about afaik)
	// But originally this required extra logic, and now that I optimized and built custom data structures these extra symbols don't exist anymore
	// This could be re-implemented at some point
	// Though instead of implementing this, I'd rather implement a method where all symbols at an address can be iterated instead and let the caller implement the check itself
	bool has_symbol_for_addr (void* ptr, SymResult const& dbghelp_res) {
		/*
		uintptr_t addr = (uintptr_t)ptr;
		
		auto* mod = mod_cache.find_module_for_addr(inspectee, addr);
		if (!mod) {
			return false;
		}
		if (!mod->pdb) {
			return false;
		}

		uintptr_t mod_raddr = addr - mod->base_addr;
		
		return mod->pdb->has_symbol_for_addr(mod_raddr, dbghelp_res.sym_name);
		*/
		return false;
	}
	
	__declspec(noinline)
	void measure_pdb_parse (void* ptr) {
		auto* mod = mod_cache.find_module_for_addr(inspectee, (uintptr_t)ptr);
		mod_cache.clear(); // clear cache so this can be called repeatedly
	}

	//__declspec(noinline)
	bool _measure_addr2sym (uintptr_t addr, LoadedModule* mod, SymResult* res) {
		//ZoneScoped;

		uintptr_t mod_raddr = addr - mod->base_addr;
		res->module_path = mod->ansi_path.c_str();

		if (!mod->pdb) {
			auto* exports = mod->load_export_table();

			if (exports) {
				auto* mangled_name = exports->query(mod_raddr);
				if (mangled_name) {
					res->sym_name = mangled_name;
					return true;
				}
			}
			res->err = "Module pdb not found and not found in export table";
			return false;
		}
		
		Symbol* sym;
		{
			//TimerMeasZone(tfind_symbol_for_addr);
			sym = mod->pdb->find_symbol_for_addr(mod_raddr);
			if (!sym) {
				res->err = "Symbol not found";
				return false;
			}
		}
		
		res->sym_name = mod->pdb->stralloc[sym->name];
		res->src_filepath = nullptr;
		res->src_lineno = 0;

		SourceLoc src_loc = {};
		{
			//TimerMeasZone(tfind_source_loc_for_addr);
			if (mod->pdb->find_source_loc_for_addr(sym, mod_raddr, &src_loc)) {
				res->src_filepath = src_loc.filepath;
				res->src_lineno = src_loc.lineno;
			}
		}

		if (sym->inline_depth > 0) {
			//TimerMeasZone(ttrace_inlinesites);
			res->num_inlines = mod->pdb->trace_inlinesites_for_addr(sym, mod_raddr, res->inlines, SymResult::MAX_INLINES);
		}

		return res->valid();
	}
	__declspec(noinline) bool measure_addr2sym (void* ptr, SymResult* res) {
		auto _tCombinedAddr2sym = kiss::TimerMeasureZone::started(&tCombinedAddr2sym);
		ZoneScoped;
		
		res->clear();
		uintptr_t addr = (uintptr_t)ptr;

		auto* mod = mod_cache.find_module_for_addr(inspectee, addr);
		if (!mod) {
			res->err = "Module not found";
			return false;
		}

		return _measure_addr2sym(addr, mod, res);
	}
	
	void print_pdb_stats (void* ptr) {
		auto* mod = mod_cache.find_module_for_addr(inspectee, (uintptr_t)ptr);
		if (mod->pdb) mod->pdb->print_stats();
	}

	void print_timings () {
		mod_cache.ttry_get_and_cache_module.print();
		mod_cache.tload_pdb.print();
		tfind_symbol_for_addr.print();
		tfind_source_loc_for_addr.print();
		ttrace_inlinesites.print();
		tCombinedAddr2sym.print();
	}
};

class SymResolverDebughelp {
	HANDLE inspectee;
public:
	
	TimerMeasurement tDebughelp_init = TimerMeasurement("Debughelp_init");
	TimerMeasurement tSymFromAddr = TimerMeasurement("SymFromAddr");
	TimerMeasurement tSymGetLineFromAddr64 = TimerMeasurement("SymGetLineFromAddr64");
	TimerMeasurement tSymAddrIncludeInlineTrace = TimerMeasurement("SymAddrIncludeInlineTrace");
	TimerMeasurement tSymQueryInlineTrace = TimerMeasurement("SymQueryInlineTrace");
	TimerMeasurement tSymFromInlineContext = TimerMeasurement("SymFromInlineContext");
	TimerMeasurement tSymGetLineFromInlineContext = TimerMeasurement("SymGetLineFromInlineContext");
	TimerMeasurement tCombinedAddr2sym = TimerMeasurement("CombinedAddr2sym");

	SymResolverDebughelp (HANDLE inspectee): inspectee{inspectee} {
		TimerMeasZone(tDebughelp_init);

		real_dbghelp.load_if_not_loaded_yet();
		
		std::string search_path;
		{ // Need to set search_path because dbhelp.dll does not search next to exe for pdb, instead searching this processes working directory
			char exe_name[1024];
			DWORD size = sizeof(exe_name);
			if (!QueryFullProcessImageNameA(inspectee, 0, exe_name, &size)) {
				print_err_throw("QueryFullProcessImageNameA");
			}

			std::filesystem::path exe_path = std::string_view(exe_name, size);
			search_path = exe_path.has_parent_path() ? exe_path.parent_path().string() : ".";
		}

		DWORD opts = 0;
		opts |= SYMOPT_LOAD_LINES;         // line info
		//opts |= SYMOPT_UNDNAME;            // undecorate C++ names, tracy does not use this
		real_dbghelp.SymSetOptions(opts);

		// This means load symbol information for currently loaded modules
		// Tracy is using this, but then also calling SymLoadModuleEx later (since modules can be loaded later)
		// In my case I just want to measure symbol resolution performance and I assume the modules I'm interested in are already loaded
		BOOL fInvadeProcess = TRUE;
		if (!real_dbghelp.SymInitialize(inspectee, search_path.c_str(), fInvadeProcess)) {
			print_err_throw("SymInitialize");
		}

	}
	~SymResolverDebughelp () {
		real_dbghelp.SymCleanup(inspectee);
	}

	bool addr2sym (void* addr, SymResult* res) {
		res->clear();
		
		SYMBOL_INFO_PACKAGE buf;
		buf.si = {};
		buf.si.SizeOfStruct = sizeof(buf.si);
		buf.si.MaxNameLen = MAX_SYM_NAME;

		DWORD Displacement = 0;

		if (!real_dbghelp.SymFromAddr(inspectee, (DWORD64)addr, nullptr, &buf.si)) {
			res->err = "SymFromAddr error";
			return false;
		}

		memcpy(&res->info, &buf.si, SymResult::INFO_RELEVANT_SIZE);

		constexpr uint32_t SEEN_FLAGS =
			SYMFLAG_FUNC_NO_RETURN
		;
		assert((res->info.Flags & ~SEEN_FLAGS) == 0);
		assert(res->info.Value == 0);
		assert(res->info.Register == 0);
		assert(res->info.Scope == 0);
		assert(
			   res->info.Tag == 5 // SymTagFunction
			|| res->info.Tag == 7 // SymTagData
			|| res->info.Tag == 10 // SymTagPublicSymbol
			|| res->info.Tag == 27 // SymTagThunk
		);

		// need to copy into per-SymResult string buffer
		res->module_path = nullptr; // dbghelp.dll does not seem to return this, module_path is mainly for completeness sake, tracy actually determines this itself
		res->sym_name = res->str_alloc.push(buf.si.Name, buf.si.NameLen);
		res->src_filepath = nullptr;
		res->src_lineno = 0;
		
		{
			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			if (real_dbghelp.SymGetLineFromAddr64(inspectee, (DWORD64)addr, &Displacement, &line)) {
				res->src_filepath = res->str_alloc.push(line.FileName, strlen(line.FileName));
				res->src_lineno = line.LineNumber;
			}
		}

		BOOL doInline = FALSE;
		DWORD ctx = 0;
		DWORD inlineNum = 0;
		if (real_dbghelp.SymAddrIncludeInlineTrace) {
			inlineNum = real_dbghelp.SymAddrIncludeInlineTrace(inspectee, (DWORD64)addr);

			DWORD idx;
			if (inlineNum != 0) {
				doInline = real_dbghelp.SymQueryInlineTrace(inspectee, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
			}
		}
		
		if (doInline) {
			res->num_inlines = (int)inlineNum;
			for (int i=res->num_inlines-1; i>=0; i--) {
				res->inlines[i] = {};

				if (real_dbghelp.SymFromInlineContext(inspectee, (DWORD64)addr, ctx, NULL, &buf.si)) {
					res->inlines[i].fnname = res->str_alloc.push(buf.si.Name, buf.si.NameLen);
					
					IMAGEHLP_LINE64 line = {};
					line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
					if (real_dbghelp.SymGetLineFromInlineContext(inspectee, (DWORD64)addr, ctx, 0, &Displacement, &line)) {
						res->inlines[i].filepath = res->str_alloc.push(line.FileName, strlen(line.FileName));
						res->inlines[i].lineno = line.LineNumber;
					}
				}

				ctx++;
			}
		}
		return res->valid();
	}
	
	void measure_addr2sym (char* addr) {
		ZoneScopedC(0xAC563E);

		SYMBOL_INFO_PACKAGE buf;
		buf.si = {};
		buf.si.SizeOfStruct = sizeof(buf.si);
		buf.si.MaxNameLen = MAX_SYM_NAME;

		//TimerMeasZone(tCombinedAddr2sym);

		DWORD Displacement = 0;

		BOOL res1;
		{
			//TimerMeasZone(tSymFromAddr);
			res1 = real_dbghelp.SymFromAddr(inspectee, (DWORD64)addr, nullptr, &buf.si);
		}
		if (!res1) {
			return;
		}
		
		BOOL res2;
		{
			//TimerMeasZone(tSymGetLineFromAddr64);

			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			res2 = real_dbghelp.SymGetLineFromAddr64(inspectee, (DWORD64)addr, &Displacement, &line);
		}
		
		{
			//ZoneScopedNC("inlining", 0xAC563E);
		
			BOOL doInline = FALSE;
			DWORD ctx = 0;
			DWORD inlineNum = 0;
			if (real_dbghelp.SymAddrIncludeInlineTrace) {
				{
					//TimerMeasZone(tSymAddrIncludeInlineTrace);
					inlineNum = real_dbghelp.SymAddrIncludeInlineTrace(inspectee, (DWORD64)addr);
				}

				DWORD idx;
				if (inlineNum != 0) {
					//TimerMeasZone(tSymQueryInlineTrace);
					doInline = real_dbghelp.SymQueryInlineTrace(inspectee, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
				}
			}
		
			if (doInline) {
				for (DWORD i=0; i<inlineNum; i++) {
					{
						//TimerMeasZone(tSymFromInlineContext);
						res1 = real_dbghelp.SymFromInlineContext(inspectee, (DWORD64)addr, ctx, NULL, &buf.si);
					}
				
					if (res1) {
						//TimerMeasZone(tSymGetLineFromInlineContext);

						IMAGEHLP_LINE64 line = {};
						line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
						res2 = real_dbghelp.SymGetLineFromInlineContext(inspectee, (DWORD64)addr, ctx, 0, &Displacement, &line);
					}

					ctx++;
				}
			}
		}
	}

	void print_timings () {
		tDebughelp_init.print();

		tSymFromAddr.print();
		tSymGetLineFromAddr64.print();

		tSymAddrIncludeInlineTrace.print();
		tSymQueryInlineTrace.print();
		tSymFromInlineContext.print();
		tSymGetLineFromInlineContext.print();
		tCombinedAddr2sym.print();
	}
};

void MismatchCounts::symbol_mismatch_or_mangled (SymResult const& res, SymResult const& dbghelp_res, MismatchCounts::Data const& d) {
	//if (res.sym_name[0] == '?') {
	//	// Should be obsolete now as I am processing these names
	//	auto* end = strchr(res.sym_name+1, '@');
	//	if (end) {
	//		std::string_view a = dbghelp_res.sym_name;
	//		std::string_view b = std::string_view(res.sym_name+1, end-(res.sym_name+1));
	//		if (a == b) {
	//			symbol_name_mangled++;
	//			return;
	//		}
	//	}
	//}
	
	if (d.resolver->has_symbol_for_addr(d.addr, dbghelp_res))
		symbol_mismatch_overlap++;
	else
		symbol_mismatch++;
}
