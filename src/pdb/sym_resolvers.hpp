#pragma once
#include "util.hpp"
#include "sym_result.hpp"
#include "module_lookup.hpp"
#include "dbghelp_api.hpp"

class SymResolverBase {
public:
	virtual ~SymResolverBase () {}
	
	virtual bool addr2sym (void* ptr, SymResult* res) = 0;
	virtual void measure_addr2sym (char* ptr) {}
	virtual void measure_pdb_parse (void* ptr) {}
	
	virtual bool has_symbol_for_addr (void* ptr, SymResult const& dbghelp_res) {
		return false;
	}
	
	virtual void print_pdb_stats (void* ptr) {}
	virtual void print_timings () {}
};

class SymResolver : public SymResolverBase {
friend class SymResolverDebughelp;
	ModuleCache mod_cache;
	
	TimerMeasurement tfind_symbol_for_addr = TimerMeasurement("find_symbol_for_addr");
	TimerMeasurement tfind_source_loc_for_addr= TimerMeasurement("find_source_loc_for_addr");
	TimerMeasurement ttrace_inlinesites = TimerMeasurement("trace_inlinesites");
	TimerMeasurement tCombinedAddr2sym = TimerMeasurement("CombinedAddr2sym");

public:

	SymResolver (HANDLE hprocess): mod_cache{hprocess} {}
	virtual ~SymResolver () {}
	
	virtual bool addr2sym (void* ptr, SymResult* res) override {
		res->clear();
		uint64_t addr = (uint64_t)ptr;

		auto* mod = mod_cache.find_module_for_addr(addr);
		if (!mod) {
			res->err = "Module not found";
			return false;
		}

		uint64_t rva = addr - mod->base_addr;
		res->module_path = mod->ansi_path.c_str();

		if (!mod->pdb) {
			auto* exports = mod->exports.get();
			if (exports) {
				uint64_t sym_rva;
				uint32_t sym_idx;
				auto* mangled_name = exports->query(rva, &sym_rva, &sym_idx);
				if (mangled_name) {
					res->sym_name = mangled_name;

					res->info.TypeIndex = 0;
					res->info.Reserved[0] = 0;
					res->info.Reserved[1] = 0;
					//res->info.Index = sym_idx;
					res->info.Index = 0;
					res->info.Size = 0;
					res->info.ModBase = mod->base_addr;
					res->info.Flags = 0;
					res->info.Value = 0;
					res->info.Address = mod->base_addr + rva;
					res->info.Register = 0;
					res->info.Scope = 0;
					res->info.Tag = (ULONG)SymTagEnum::SymTagPublicSymbol;

					return true;
				}
			}
			res->err = "Module pdb not found and not found in export table";
			return false;
		}

		uint32_t sym_idx;
		auto* sym = mod->pdb->find_symbol_for_addr(rva, &sym_idx);
		if (!sym) {
			res->err = "Symbol not found";
			return false;
		}
		
		res->sym_name = mod->pdb->stralloc[sym->name];
		
		res->info.TypeIndex = 0;
		res->info.Reserved[0] = 0;
		res->info.Reserved[1] = 0;
		// Not the same index as dbghelp (as the indices are unstable across runs and only used for passing into other api functions)
		// we could implement other parts of the api using our own index scheme
		// actually, should return 0 here so that if dbghelp calls that I do not replace are called with this index, it can fail instead of being confused
		//res->info.Index = sym_idx;
		res->info.Index = 0;
		// Size for module symbols makes sense, but for global data symbols it has to be "estimated" based on the typeinfo, which I cannot easily replicate
		// for global function symbols it's even more weird and dbghelp seemingly sometimes computes it based on types extracted from name mangling
		// Since can't reasonably match it I resort to jus returning 0 in those cases
		res->info.Size = sym->size;
		res->info.ModBase = mod->base_addr;
		// Don't bother returning flags, i've only observed very few of them appear and
		// at least SYMFLAG_EXPORT seems to be determined based on PE export table instead of pdb, which is overcomplicated imho
		// a full 1-1 match of dbghelp behavior is probably only possibly by caching dbghelp results, which is not my goal
		res->info.Flags = 0; //sym->si_flags;
		res->info.Value = 0;
		// HACK: __ImageBase does not return an Address unlike seemingly everything else in dbghelp, super pointless but this matches dbghelp more closely
		res->info.Address = sym->base_addr != 0 ? sym->base_addr + mod->base_addr : 0;
		res->info.Register = 0;
		res->info.Scope = 0;
		res->info.Tag = (ULONG)sym->si_tag;
		//res->info.NameLen = strlen(res->sym_name);

		SourceLoc src_loc = {};
		if (mod->pdb->find_source_loc_for_addr(sym, rva, &src_loc)) {
			res->src_filepath = src_loc.filepath;
			res->src_lineno = src_loc.lineno;
		}
		
		if (sym->inline_depth > 0) {
			res->num_inlines = mod->pdb->trace_inlinesites_for_addr(sym, rva, res->inlines, SymResult::MAX_INLINES);
		}

		return res->valid();
	}
	
	virtual void measure_addr2sym (char* ptr) override {
		SymResult res;
		measure_addr2sym(ptr, &res);
		res.dont_optimize_away();
	}

	// In cases where multiple symbols exist at one address, we will sometimes pick a different one than dbghelp
	// This function was meant to test if the symbol dbghelp returned existed in the pdb at all (to differentiate bugs from simply picking the wrong symbol which I can't do anything about afaik)
	// But originally this required extra logic, and now that I optimized and built custom data structures these extra symbols don't exist anymore
	// This could be re-implemented at some point
	// Though instead of implementing this, I'd rather implement a method where all symbols at an address can be iterated instead and let the caller implement the check itself
	virtual bool has_symbol_for_addr (void* ptr, SymResult const& dbghelp_res) override {
		/*
		uint64_t addr = (uint64_t)ptr;
		
		auto* mod = mod_cache.find_module_for_addr(inspectee, addr);
		if (!mod) {
			return false;
		}
		if (!mod->pdb) {
			return false;
		}

		uint64_t rva = addr - mod->base_addr;
		
		return mod->pdb->has_symbol_for_addr(rva, dbghelp_res.sym_name);
		*/
		return false;
	}
	
	__declspec(noinline)
	virtual void measure_pdb_parse (void* ptr) override {
		auto* mod = mod_cache.find_module_for_addr((uint64_t)ptr);
		mod_cache.clear(); // clear cache so this can be called repeatedly
	}

	//__declspec(noinline)
	bool _measure_addr2sym (uint64_t addr, LoadedModule* mod, SymResult* res) {
		//ZoneScoped;
		
		uint64_t rva = addr - mod->base_addr;
		res->module_path = mod->ansi_path.c_str();

		if (!mod->pdb) {
			auto* exports = mod->exports.get();
			if (exports) {
				uint64_t sym_rva;
				uint32_t sym_idx;
				auto* mangled_name = exports->query(rva, &sym_rva, &sym_idx);
				if (mangled_name) {
					res->sym_name = mangled_name;

					res->info.TypeIndex = 0;
					res->info.Reserved[0] = 0;
					res->info.Reserved[1] = 0;
					//res->info.Index = sym_idx;
					res->info.Index = 0;
					res->info.Size = 0;
					res->info.ModBase = mod->base_addr;
					res->info.Flags = 0;
					res->info.Value = 0;
					res->info.Address = mod->base_addr + rva;
					res->info.Register = 0;
					res->info.Scope = 0;
					res->info.Tag = (ULONG)SymTagEnum::SymTagPublicSymbol;

					return true;
				}
			}
			res->err = "Module pdb not found and not found in export table";
			return false;
		}
		
		uint32_t sym_idx;
		Symbol* sym;
		{
			TimerMeasZone(tfind_symbol_for_addr);
			sym = mod->pdb->find_symbol_for_addr(rva, &sym_idx);
			if (!sym) {
				res->err = "Symbol not found";
				return false;
			}
		}
		
		res->sym_name = mod->pdb->stralloc[sym->name];
		
		res->info.TypeIndex = 0;
		res->info.Reserved[0] = 0;
		res->info.Reserved[1] = 0;
		res->info.Index = 0;
		res->info.Size = sym->size;
		res->info.ModBase = mod->base_addr;
		res->info.Flags = 0;
		res->info.Value = 0;
		res->info.Address = sym->base_addr != 0 ? sym->base_addr + mod->base_addr : 0;
		res->info.Register = 0;
		res->info.Scope = 0;
		res->info.Tag = (ULONG)sym->si_tag;
		//res->info.NameLen = strlen(res->sym_name);

		SourceLoc src_loc = {};
		{
			TimerMeasZone(tfind_source_loc_for_addr);
			if (mod->pdb->find_source_loc_for_addr(sym, rva, &src_loc)) {
				res->src_filepath = src_loc.filepath;
				res->src_lineno = src_loc.lineno;
			}
		}

		if (sym->inline_depth > 0) {
			TimerMeasZone(ttrace_inlinesites);
			res->num_inlines = mod->pdb->trace_inlinesites_for_addr(sym, rva, res->inlines, SymResult::MAX_INLINES);
		}

		return res->valid();
	}
	__declspec(noinline) bool measure_addr2sym (void* ptr, SymResult* res) {
		auto _tCombinedAddr2sym = kiss::TimerMeasureZone::started(&tCombinedAddr2sym);
		ZoneScoped;
		
		res->clear();
		uint64_t addr = (uint64_t)ptr;

		auto* mod = mod_cache.find_module_for_addr(addr);
		if (!mod) {
			res->err = "Module not found";
			return false;
		}

		return _measure_addr2sym(addr, mod, res);
	}
	
	virtual void print_pdb_stats (void* ptr) override {
		auto* mod = mod_cache.find_module_for_addr((uint64_t)ptr);
		if (mod->pdb) mod->pdb->print_stats();
	}

	virtual void print_timings () override {
		mod_cache.ttry_get_and_cache_module.print();
		mod_cache.tload_pdb.print();
		tfind_symbol_for_addr.print();
		tfind_source_loc_for_addr.print();
		ttrace_inlinesites.print();
		tCombinedAddr2sym.print();
	}
};

class SymResolverDebughelp {
	HANDLE hprocess;
	
	TimerMeasurement tDebughelp_init = TimerMeasurement("Debughelp_init");
	TimerMeasurement tSymFromAddr = TimerMeasurement("SymFromAddr");
	TimerMeasurement tSymGetLineFromAddr64 = TimerMeasurement("SymGetLineFromAddr64");
	TimerMeasurement tSymAddrIncludeInlineTrace = TimerMeasurement("SymAddrIncludeInlineTrace");
	TimerMeasurement tSymQueryInlineTrace = TimerMeasurement("SymQueryInlineTrace");
	TimerMeasurement tSymFromInlineContext = TimerMeasurement("SymFromInlineContext");
	TimerMeasurement tSymGetLineFromInlineContext = TimerMeasurement("SymGetLineFromInlineContext");
	TimerMeasurement tCombinedAddr2sym = TimerMeasurement("CombinedAddr2sym");
	TimerMeasurement tGetInlines = TimerMeasurement("GetInlines");
public:

	SymResolverDebughelp (HANDLE hprocess): hprocess{hprocess} {
		TimerMeasZone(tDebughelp_init);

		real_dbghelp.load_if_not_loaded_yet();
		
		std::string search_path;
		{ // Need to set search_path because dbhelp.dll does not search next to exe for pdb, instead searching this processes working directory
			char exe_name[1024];
			DWORD size = sizeof(exe_name);
			if (!QueryFullProcessImageNameA(hprocess, 0, exe_name, &size)) {
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
		if (!real_dbghelp.SymInitialize(hprocess, search_path.c_str(), fInvadeProcess)) {
			print_err_throw("SymInitialize");
		}

		CacheProcessDrivers();
	}
	~SymResolverDebughelp () {
		real_dbghelp.SymCleanup(hprocess);
	}

	// Temp: stolen from tracy, this is needed to get dbghelp to return kernel symbols
	void CacheProcessDrivers() {
		DWORD needed;
		LPVOID dev[4096];
		if (EnumDeviceDrivers(dev, sizeof(dev), &needed) != 0) {
			char windir[MAX_PATH];
			if (!GetWindowsDirectoryA(windir, sizeof(windir))) memcpy(windir, "c:\\windows", 11);
			const auto windirlen = strlen(windir);

			const auto sz = needed / sizeof(LPVOID);
			for (size_t i = 0; i < sz; i++) {
				char fn[MAX_PATH];
				const auto len = GetDeviceDriverBaseNameA(dev[i], fn, sizeof(fn));
				if (len != 0) {

					const auto len = GetDeviceDriverFileNameA(dev[i], fn, sizeof(fn));
					if (len != 0) {
						char full[MAX_PATH];
						char* path = fn;

						if (memcmp(fn, "\\SystemRoot\\", 12) == 0) {
							memcpy(full, windir, windirlen);
							strcpy(full + windirlen, fn + 11);
							path = full;
						}

						DWORD64 baseOfDll = (DWORD64)dev[i];
						DWORD bllSize = 0;
						real_dbghelp.SymLoadModuleEx(hprocess, nullptr, path, nullptr, baseOfDll, bllSize, nullptr, 0);
					}
				}
			}
		}
	}

	bool addr2sym (void* addr, SymResult* res) {
		res->clear();
		
		SYMBOL_INFO_PACKAGE buf;
		buf.si = {};
		buf.si.SizeOfStruct = sizeof(buf.si);
		buf.si.MaxNameLen = MAX_SYM_NAME;

		DWORD Displacement = 0;

		if (!real_dbghelp.SymFromAddr(hprocess, (DWORD64)addr, nullptr, &buf.si)) {
			res->err = "SymFromAddr error";
			return false;
		}

		memcpy(&res->info, &buf.si, SymResult::INFO_RELEVANT_SIZE);

		constexpr uint32_t SEEN_FLAGS =
			SYMFLAG_FUNC_NO_RETURN |
			SYMFLAG_EXPORT |
			SYMFLAG_PUBLIC_CODE;
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
			if (real_dbghelp.SymGetLineFromAddr64(hprocess, (DWORD64)addr, &Displacement, &line)) {
				res->src_filepath = res->str_alloc.push(line.FileName, strlen(line.FileName));
				res->src_lineno = line.LineNumber;
			}
		}

		BOOL doInline = FALSE;
		DWORD ctx = 0;
		DWORD inlineNum = 0;
		if (real_dbghelp.SymAddrIncludeInlineTrace) {
			inlineNum = real_dbghelp.SymAddrIncludeInlineTrace(hprocess, (DWORD64)addr);

			DWORD idx;
			if (inlineNum != 0) {
				// returned idx seems to be 0, not sure what it would be used for either
				doInline = real_dbghelp.SymQueryInlineTrace(hprocess, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
				//logf("real_dbghelp.SymQueryInlineTrace: doInline=%d, ctx:%x, idx:%d\n", doInline, ctx, idx);
			}
		}
		
		if (doInline) {
			res->num_inlines = (int)inlineNum;
			//res->num_inlines = (int)inlineNum + 2; // reverse engineering: see what SymFromInlineContext return if you keep incrementing ctx
			for (int i=res->num_inlines-1; i>=0; i--) {
				res->inlines[i] = {};

				if (real_dbghelp.SymFromInlineContext(hprocess, (DWORD64)addr, ctx, NULL, &buf.si)) {
					res->inlines[i].fnname = res->str_alloc.push(buf.si.Name, buf.si.NameLen);
					
					IMAGEHLP_LINE64 line = {};
					line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
					if (real_dbghelp.SymGetLineFromInlineContext(hprocess, (DWORD64)addr, ctx, 0, &Displacement, &line)) {
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

		TimerMeasZone(tCombinedAddr2sym);

		DWORD Displacement = 0;

		BOOL res1;
		{
			TimerMeasZone(tSymFromAddr);
			res1 = real_dbghelp.SymFromAddr(hprocess, (DWORD64)addr, nullptr, &buf.si);
		}
		if (!res1) {
			return;
		}
		
		BOOL res2;
		{
			TimerMeasZone(tSymGetLineFromAddr64);

			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			res2 = real_dbghelp.SymGetLineFromAddr64(hprocess, (DWORD64)addr, &Displacement, &line);
		}
		
		{
			//ZoneScopedNC("inlining", 0xAC563E);
		
			BOOL doInline = FALSE;
			DWORD ctx = 0;
			DWORD inlineNum = 0;
			if (real_dbghelp.SymAddrIncludeInlineTrace) {
				{
					TimerMeasZone(tSymAddrIncludeInlineTrace);
					inlineNum = real_dbghelp.SymAddrIncludeInlineTrace(hprocess, (DWORD64)addr);
				}

				DWORD idx;
				if (inlineNum != 0) {
					TimerMeasZone(tSymQueryInlineTrace);
					doInline = real_dbghelp.SymQueryInlineTrace(hprocess, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
				}
			}
		
			if (doInline) {
				TimerMeasZone(tGetInlines);
				for (DWORD i=0; i<inlineNum; i++) {
					{
						TimerMeasZone(tSymFromInlineContext);
						res1 = real_dbghelp.SymFromInlineContext(hprocess, (DWORD64)addr, ctx, NULL, &buf.si);
					}
				
					if (res1) {
						TimerMeasZone(tSymGetLineFromInlineContext);

						IMAGEHLP_LINE64 line = {};
						line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
						res2 = real_dbghelp.SymGetLineFromInlineContext(hprocess, (DWORD64)addr, ctx, 0, &Displacement, &line);
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
		tGetInlines.print();
	}
	void compare_timings (SymResolverBase* resolver) {
		SymResolver* res = dynamic_cast<SymResolver*>(resolver);
		if (res) {
			float dbh = tCombinedAddr2sym.avg_sec();

			float total = res->tCombinedAddr2sym.avg_sec();
			float no_load = res->tCombinedAddr2sym.count > 0 ?
				(res->tCombinedAddr2sym.total_sec - res->mod_cache.tload_pdb.total_sec) / (float)res->tCombinedAddr2sym.count : 0.0f;

			//logf("|Speedup: %.2fx  excluding pdb read: %.2fx\n", dbh/total, dbh/no_load);

			logf("%6.2f us | %6.2f us | %6.2f us | %.0fx |\n",
				dbh * 1000000.0f,
				total * 1000000.0f,
				no_load * 1000000.0f,
				dbh/no_load
			);
		}
	}
};

// This is a bit awkward, I designed my test framework with SymResult and SymResolver and SymResolverDebughelp
// But now I want to test my dbghelp dll wrapper code which obviously has the same API as dbgehlp
// So I'm just gonna make a copy SymResolverDebughelp for the moment and have it call my dbghelp instead
class SymResolverBetterDebughelp : public SymResolverBase {
	HANDLE hprocess;
public:
	// set TESTING_DLL_PATH in VS build settings to pick correct Debug/Release/Profiling one
	static inline constexpr const char* path = TESTING_DLL_PATH;

	//HMODULE dll = NULL;
	inline static HMODULE dll = NULL;
	t_SymInitialize               _SymInitialize               = nullptr;
	t_SymSetOptions               _SymSetOptions               = nullptr;
	t_SymCleanup                  _SymCleanup                  = nullptr;
	t_SymFromAddr                 _SymFromAddr                 = nullptr;
	t_SymGetLineFromAddr64        _SymGetLineFromAddr64        = nullptr;
	t_SymAddrIncludeInlineTrace   _SymAddrIncludeInlineTrace   = nullptr;
	t_SymQueryInlineTrace         _SymQueryInlineTrace         = nullptr;
	t_SymFromInlineContext        _SymFromInlineContext        = nullptr;
	t_SymGetLineFromInlineContext _SymGetLineFromInlineContext = nullptr;

	TimerMeasurement tDebughelp_init = TimerMeasurement("Debughelp_init");
	TimerMeasurement tSymFromAddr = TimerMeasurement("SymFromAddr");
	TimerMeasurement tSymGetLineFromAddr64 = TimerMeasurement("SymGetLineFromAddr64");
	TimerMeasurement tSymAddrIncludeInlineTrace = TimerMeasurement("SymAddrIncludeInlineTrace");
	TimerMeasurement tSymQueryInlineTrace = TimerMeasurement("SymQueryInlineTrace");
	TimerMeasurement tSymFromInlineContext = TimerMeasurement("SymFromInlineContext");
	TimerMeasurement tSymGetLineFromInlineContext = TimerMeasurement("SymGetLineFromInlineContext");
	TimerMeasurement tCombinedAddr2sym = TimerMeasurement("CombinedAddr2sym");
	TimerMeasurement tGetInlines = TimerMeasurement("GetInlines");

	SymResolverBetterDebughelp (HANDLE hprocess): hprocess{hprocess} {
		TimerMeasZone(tDebughelp_init);
		
		//dll = LoadLibraryA(path);
		// Load dll lazily for now and keep it loaded
		// maybe loading and unloading is causing problems when trying to profile with tracy?

		// It seems like in my test code this was fine, but somehow with tracy this was causing random crashes in tracy?
		// (even after editing tracy code so it does not accidentally call our dbghelp.dll)
		// NOTE: unload calls FreeLibrary, and the OS refcounts dlls, so this should be actually unloading the real dbghelp.dll for us
		// weirdly, this is our dll, but my tracy code was in the exe, I have no idea how this even affects each other unless dbghelp does something weird
		if (!dll)
			dll = LoadLibraryA(path);

		_SymInitialize               = (t_SymInitialize              )GetProcAddress(dll, "SymInitialize");
		_SymSetOptions               = (t_SymSetOptions              )GetProcAddress(dll, "SymSetOptions");
		_SymCleanup                  = (t_SymCleanup                 )GetProcAddress(dll, "SymCleanup");
		_SymFromAddr                 = (t_SymFromAddr                )GetProcAddress(dll, "SymFromAddr");
		_SymGetLineFromAddr64        = (t_SymGetLineFromAddr64       )GetProcAddress(dll, "SymGetLineFromAddr64");
		_SymAddrIncludeInlineTrace   = (t_SymAddrIncludeInlineTrace  )GetProcAddress(dll, "SymAddrIncludeInlineTrace");
		_SymQueryInlineTrace         = (t_SymQueryInlineTrace        )GetProcAddress(dll, "SymQueryInlineTrace");
		_SymFromInlineContext        = (t_SymFromInlineContext       )GetProcAddress(dll, "SymFromInlineContext");
		_SymGetLineFromInlineContext = (t_SymGetLineFromInlineContext)GetProcAddress(dll, "SymGetLineFromInlineContext");
		
		std::string search_path;
		{ // Need to set search_path because dbhelp.dll does not search next to exe for pdb, instead searching this processes working directory
			char exe_name[1024];
			DWORD size = sizeof(exe_name);
			if (!QueryFullProcessImageNameA(hprocess, 0, exe_name, &size)) {
				print_err_throw("QueryFullProcessImageNameA");
			}

			std::filesystem::path exe_path = std::string_view(exe_name, size);
			search_path = exe_path.has_parent_path() ? exe_path.parent_path().string() : ".";
		}

		DWORD opts = 0;
		opts |= SYMOPT_LOAD_LINES;         // line info
		//opts |= SYMOPT_UNDNAME;            // undecorate C++ names, tracy does not use this
		_SymSetOptions(opts);

		// This means load symbol information for currently loaded modules
		// Tracy is using this, but then also calling SymLoadModuleEx later (since modules can be loaded later)
		// In my case I just want to measure symbol resolution performance and I assume the modules I'm interested in are already loaded
		BOOL fInvadeProcess = TRUE;
		if (!_SymInitialize(hprocess, search_path.c_str(), fInvadeProcess)) {
			print_err_throw("SymInitialize");
		}

	}
	virtual ~SymResolverBetterDebughelp () {
		_SymCleanup(hprocess);
		
		//FreeLibrary(dll);
	}

	virtual bool addr2sym (void* addr, SymResult* res) override {
		res->clear();
		
		SYMBOL_INFO_PACKAGE buf;
		buf.si = {};
		buf.si.SizeOfStruct = sizeof(buf.si);
		buf.si.MaxNameLen = MAX_SYM_NAME;

		DWORD Displacement = 0;

		if (!_SymFromAddr(hprocess, (DWORD64)addr, nullptr, &buf.si)) {
			res->err = "SymFromAddr error";
			return false;
		}

		memcpy(&res->info, &buf.si, SymResult::INFO_RELEVANT_SIZE);

		constexpr uint32_t SEEN_FLAGS =
			SYMFLAG_FUNC_NO_RETURN |
			SYMFLAG_EXPORT |
			SYMFLAG_PUBLIC_CODE;
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
			if (_SymGetLineFromAddr64(hprocess, (DWORD64)addr, &Displacement, &line)) {
				res->src_filepath = res->str_alloc.push(line.FileName, strlen(line.FileName));
				res->src_lineno = line.LineNumber;
			}
		}

		BOOL doInline = FALSE;
		DWORD ctx = 0;
		DWORD inlineNum = 0;
		if (_SymAddrIncludeInlineTrace) {
			inlineNum = _SymAddrIncludeInlineTrace(hprocess, (DWORD64)addr);

			DWORD idx;
			if (inlineNum != 0) {
				doInline = _SymQueryInlineTrace(hprocess, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
			}
		}
		
		if (doInline) {
			res->num_inlines = (int)inlineNum;
			//res->num_inlines = (int)inlineNum + 2; // reverse engineering: see what SymFromInlineContext return if you keep incrementing ctx
			for (int i=res->num_inlines-1; i>=0; i--) {
				res->inlines[i] = {};

				if (_SymFromInlineContext(hprocess, (DWORD64)addr, ctx, NULL, &buf.si)) {
					res->inlines[i].fnname = res->str_alloc.push(buf.si.Name, buf.si.NameLen);
					
					IMAGEHLP_LINE64 line = {};
					line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
					if (_SymGetLineFromInlineContext(hprocess, (DWORD64)addr, ctx, 0, &Displacement, &line)) {
						res->inlines[i].filepath = res->str_alloc.push(line.FileName, strlen(line.FileName));
						res->inlines[i].lineno = line.LineNumber;
					}
				}

				ctx++;
			}
		}
		return res->valid();
	}
	
	virtual void measure_addr2sym (char* addr) override {
		ZoneScoped;

		SYMBOL_INFO_PACKAGE buf;
		buf.si = {};
		buf.si.SizeOfStruct = sizeof(buf.si);
		buf.si.MaxNameLen = MAX_SYM_NAME;

		TimerMeasZone(tCombinedAddr2sym);

		DWORD Displacement = 0;

		BOOL res1;
		{
			//TimerMeasZone(tSymFromAddr);
			//ZoneScopedN("SymFromAddr");
			res1 = _SymFromAddr(hprocess, (DWORD64)addr, nullptr, &buf.si);
		}
		if (!res1) {
			return;
		}
		
		BOOL res2;
		{
			//TimerMeasZone(tSymGetLineFromAddr64);
			//ZoneScopedN("SymGetLineFromAddr64");

			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			res2 = _SymGetLineFromAddr64(hprocess, (DWORD64)addr, &Displacement, &line);
		}
		
		{
			//ZoneScopedNC("inlining", 0xAC563E);
		
			BOOL doInline = FALSE;
			DWORD ctx = 0;
			DWORD inlineNum = 0;
			if (_SymAddrIncludeInlineTrace) {
				{
					//TimerMeasZone(tSymAddrIncludeInlineTrace);
					//ZoneScopedN("SymAddrIncludeInlineTrace");
					inlineNum = _SymAddrIncludeInlineTrace(hprocess, (DWORD64)addr);
				}

				DWORD idx;
				if (inlineNum != 0) {
					//TimerMeasZone(tSymQueryInlineTrace);
					//ZoneScopedN("SymQueryInlineTrace");
					doInline = _SymQueryInlineTrace(hprocess, (DWORD64)addr, 0, (DWORD64)addr, (DWORD64)addr, &ctx, &idx);
				}
			}
		
			if (doInline) {
				TimerMeasZone(tGetInlines);
				for (DWORD i=0; i<inlineNum; i++) {
					{
						//TimerMeasZone(tSymFromInlineContext);
						//ZoneScopedN("SymFromInlineContext");
						res1 = _SymFromInlineContext(hprocess, (DWORD64)addr, ctx, NULL, &buf.si);
					}
				
					if (res1) {
						//TimerMeasZone(tSymGetLineFromInlineContext);
						//ZoneScopedN("SymGetLineFromInlineContext");

						IMAGEHLP_LINE64 line = {};
						line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
						res2 = _SymGetLineFromInlineContext(hprocess, (DWORD64)addr, ctx, 0, &Displacement, &line);
					}

					ctx++;
				}
			}
		}
	}

	virtual void print_timings () override {
		tDebughelp_init.print();

		tSymFromAddr.print();
		tSymGetLineFromAddr64.print();

		tSymAddrIncludeInlineTrace.print();
		tSymQueryInlineTrace.print();
		tSymFromInlineContext.print();
		tSymGetLineFromInlineContext.print();
		tCombinedAddr2sym.print();
		tGetInlines.print();
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
	
	//if (d.resolver->has_symbol_for_addr(d.addr, dbghelp_res))
	//	symbol_mismatch_overlap++;
	//else
	//	symbol_mismatch++;
	symbol_mismatch++;
}
