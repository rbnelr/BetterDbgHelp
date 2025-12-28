#pragma once
#include "util.hpp"
#include "module_lookup.hpp"

class SymResolver;
struct SymResult;

struct MismatchCounts {
	int symbol_mismatch = 0;
	int source_mismatch = 0;
	int inline_mimatch = 0;

	int symbol_mismatch_overlap = 0; // different symbol name (but overlapping symbol with correct name existed)
	//int symbol_name_mangled = 0; // correct symbol but we did not filer out mangled scoped info (Note: I did not enable demangling, but dbghelp still 'stripped' the name of the mangled parts)
	int DH_no_source = 0; // we have source info, dbghelp does not

	int other = 0;

	void print () {
		logf("Mismatches:\n");
		logf("  symbol_mismatch: %d\n", symbol_mismatch);
		logf("  source_mismatch: %d\n", source_mismatch);
		logf("  inline_mimatch: %d\n", inline_mimatch);
		logf("  symbol_mismatch_overlap: %d\n", symbol_mismatch_overlap);
		//logf("  symbol_name_mangled: %d\n", symbol_name_mangled);
		logf("  DH_no_source: %d\n", DH_no_source);
		logf("  other: %d\n", other);
	}
	
	// horrible way to pass this data
	struct Data {
		MismatchCounts* counts = nullptr;
		SymResolver* resolver;
		void* addr;
	};
	void symbol_mismatch_or_mangled (SymResult const& res, SymResult const& dbghelp_res, Data const& d);
};


struct SymResult {
	static inline constexpr unsigned MAX_INLINES = 64;

	const char* err;

	const char* module_path;
	const char* sym_name;

	const char* src_filepath;
	uint32_t    src_lineno;
	
	int num_inlines;
	SourceLocAndFn inlines[MAX_INLINES];
	
	// my own symbol resolver returns stable pointers but dbghelp needs to copy the strings
	// I could have just resorted to using std::string everywhere
	// but i REALLY want to avoid this as it would insert slow-ish heap allocations in the code I want to optimize and profile
	// especially since in 90% of the cases we can get away without the heap, by using this string buffer
	// and because if I mimic dbghelp's API later, i get passed user-allocated buffers I can write into (though only for some of the functions)
	
	// Source Filenames (path is included) and function names with C++ templates can be extremely long
	// And 64 Inline sites cause huge string output amounts occasionally
	// Use reasonably sized stack buffer first, then heap if it overflows
	// Note that this struct can't be moved or copied trivially anymore as it points to the internal string buffer
	// Could write a working move and copy constructor but for now just use std::unique_ptr of SymResult
	SmallStringAlloc<4096> str_alloc;
	
	SymResult (SymResult&& other) = delete;
	SymResult& operator= (SymResult&& other) = delete;
	SymResult (SymResult const& other) = delete;
	SymResult& operator= (SymResult const& other) = delete;

	SymResult () {
		clear();
	}
	~SymResult () = default;
	
	// call this from symbol resolver as it is faster to only clear relevant fields rather than sym={}, at that copies entire str_buf!
	void clear () {
		err = nullptr;

		module_path = nullptr;
		sym_name = nullptr;
		
		src_filepath = nullptr;
		src_lineno = 0;
		
		num_inlines = 0;

		// I had a bug with inlinesites not getting properly cleared(?)
		// but this memset takes time, so let's just get this right instead
		//#ifndef NDEBUG
		//memset((char*)this + STRBUF_SIZE, 0, sizeof(SymResult)-STRBUF_SIZE);
		//memset(inlines, 0, sizeof(inlines));
		//#endif
	}
	void clear_inlines () {
		for (int i=num_inlines; i<MAX_INLINES; i++) {
			inlines[i] = {};
		}
	}

	bool valid () const {
		return sym_name != nullptr;
	}

	bool has_source () const {
		return src_filepath != nullptr;
	}
	
	bool equal_sym (SymResult const& r) const {
		if (valid() != r.valid()) return false;

		if (valid()) {
			// dbghelp.dll not returning module name, assume it's correct
			//if (my_strcmp(module_path, r.module_path) != 0) return false;
			if (!my_strcmp(sym_name, r.sym_name)) return false;
		}
		else {
			// If both throw error it counts as a match as comparing errors may be hard
		}
		return true;
	}
	bool equal_no_inline (SymResult const& r) const {
		if (valid() != r.valid()) return false;

		if (valid()) {
			// dbghelp.dll not returning module name, assume it's correct
			//if (my_strcmp(module_path, r.module_path) != 0) return false;
			if (!my_strcmp(sym_name, r.sym_name)) return false;

			if (has_source() != r.has_source()) return false;
			if (has_source()) {
				if (!my_strcmp(src_filepath, r.src_filepath)) return false;
				if (src_lineno != r.src_lineno) return false;
			}
		}
		else {
			// If both throw error it counts as a match as comparing errors may be hard
		}
		return true;
	}

	/*
	bool operator== (SymResult const& r) const {
		if (valid() != r.valid()) return false;

		if (valid()) {
			// dbghelp.dll not returning module name, assume it's correct
			//if (my_strcmp(module_path, r.module_path) != 0) return false;
			if (!my_strcmp(sym_name, r.sym_name)) return false;

			if (has_source() != r.has_source()) return false;
			if (has_source()) {
				if (!my_strcmp(src_filepath, r.src_filepath)) return false;
				if (src_lineno != r.src_lineno) return false;
			}
				
			if (num_inlines != r.num_inlines) return false;
			for (int i=0; i<num_inlines; i++) {
				auto& li = inlines[i];
				auto& ri = r.inlines[i];
				if (!my_strcmp(li.fnname, ri.fnname)) return false;
				if (!my_strcmp(li.filepath, ri.filepath)) return false;
				if (li.lineno != ri.lineno) return false;
			}
		}
		else {
			// If both throw error it counts as a match as comparing errors may be hard
		}
		return true;
	}
	bool operator!= (SymResult const& r) const {
		return !(*this == r);
	}
	*/
	
	bool equal (SymResult const& r, MismatchCounts::Data c={}) const {
		auto* counts = c.counts;

		if (valid() != r.valid()) {
			if (counts)
				counts->other++;
			return false;
		}

		if (valid()) {
			// dbghelp.dll not returning module name, assume it's correct
			//if (my_strcmp(module_path, r.module_path) != 0) return false;
			if (!my_strcmp(sym_name, r.sym_name)) {
				if (counts)
					counts->symbol_mismatch_or_mangled(*this, r, c);
				return false;
			}

			if (has_source() != r.has_source()) {
				if (has_source())
					if (counts)
						counts->DH_no_source++;
				else
					if (counts)
						counts->source_mismatch++;
				return false;
			}
			if (has_source()) {
				if (!my_strcmp(src_filepath, r.src_filepath)) {
					if (counts)
						counts->source_mismatch++;
					return false;
				}
				if (src_lineno != r.src_lineno) {
					if (counts)
						counts->source_mismatch++;
					return false;
				}
			}
				
			if (num_inlines != r.num_inlines) {
				if (counts)
					counts->inline_mimatch++;
				return false;
			}
			for (int i=0; i<num_inlines; i++) {
				auto& li = inlines[i];
				auto& ri = r.inlines[i];
				if (    !my_strcmp(li.fnname, ri.fnname)
					 || !my_strcmp(li.filepath, ri.filepath)
					 || li.lineno != ri.lineno ) {
					if (counts)
						counts->inline_mimatch++;
					return false;
				}
			}
		}
		else {
			// If both throw error it counts as a match as comparing errors may be hard
		}
		return true;
	}
	
	void print_sym () {
		if (!valid()) {
			logf("%s\n", err);
			return;
		}

		logf("%15s!%s\n", module_path, sym_name);
	}
	void print_no_inline () {
		if (!valid()) {
			logf("%s\n", err);
			return;
		}

		logf("%15s!%s ", module_path, sym_name);
		if (has_source()) {
			logf("%-15s:%d\n", src_filepath, src_lineno);
		}
		else {
			logf("(No source info)\n");
		}
	}
	void print () {
		if (!valid()) {
			logf("%s\n", err);
			return;
		}

		logf("%15s!%s ", module_path, sym_name);
		if (has_source()) {
			logf("%-15s:%d\n", src_filepath, src_lineno);
		}
		else {
			logf("(No source info)\n");
		}

		for (int i=0; i<num_inlines; i++) {
			if (inlines[i].filepath) {
				logf(" |inl %-15s %15s:%d\n", inlines[i].fnname, inlines[i].filepath, inlines[i].lineno);
			}
			else {
				logf(" |inl (No info)\n");
			}
		}
	}

	static bool my_strcmp (const char* l, const char* r) {
		if ((l == nullptr) != (r == nullptr))
			return false;
		if (l == nullptr)
			return true;
		return strcmp(l, r) == 0;
	}
	void print_diff (const char* mod_name, intptr_t rel_addr, SymResult const& r) const {
		if (r.valid())
			logf("!!! [%s+%llx] (%s) Result Mismatch:\n", mod_name, rel_addr, r.sym_name);
		else
			logf("!!! [%s+%llx] (SymResolver: %s) Result Mismatch:\n", mod_name, rel_addr, sym_name);

		if (valid() != r.valid()) {
			if (!  valid()) logf("> SymResolver: %s\n", err);
			if (!r.valid()) logf("> dbghelp:dll: %s\n", r.err);
			return;
		}

		//if (   strcmp(module_path, r.module_path) != 0
		//	|| strcmp(sym_name, r.sym_name) != 0 ) {
		//	logf("> sym:         \"%s!%s\" !=\n", module_path,sym_name);
		//	logf("> dbghelp:dll: \"%s!%s\"\n", r.module_path,r.sym_name);
		//}
		if (!my_strcmp(sym_name, r.sym_name)) {
			logf("> SymResolver: \"%s!%s\" !=\n", module_path,sym_name);
			logf("> dbghelp:dll: \"[unknown]!%s\"\n", r.sym_name);
		}
		if (   has_source() != r.has_source()
			|| !my_strcmp(src_filepath, r.src_filepath) || src_lineno != r.src_lineno) {
			
			if (has_source()) {
				logf("> SymResolver: \"%s:%d\" !=\n", src_filepath,src_lineno);
			}
			else {
				logf("> SymResolver: (No source info) !=\n");
			}
				
			if (r.has_source()) {
				logf("> dbghelp:dll: \"%s:%d\"\n", r.src_filepath,r.src_lineno);
			}
			else {
				logf("> dbghelp:dll: (No source info)\n");
			}
		}
		
		for (int i=0; i<std::max(num_inlines, r.num_inlines); i++) {
			// inlines past num_inlines no longer cleared as an optimization, need to compare correctly!
			auto l = i < num_inlines ? inlines[i] : SourceLocAndFn{};
			auto dh = i < r.num_inlines ? r.inlines[i] : SourceLocAndFn{};

			if (   !my_strcmp(l.fnname, dh.fnname)
				|| !my_strcmp(l.filepath, dh.filepath)
				||  l.lineno != dh.lineno) {
				logf(" |inl%d SymResolver: %-15s %15s:%d\n", i, l.fnname, l.filepath, l.lineno);
				logf(" |inl%d dbghelp:dll: %-15s %15s:%d\n", i, dh.fnname, dh.filepath, dh.lineno);
			}
		}
	}
	
	static inline volatile int _vol_idx{};
	static inline volatile int _vol_sink{};
	void dont_optimize_away () {
		if (module_path) _vol_sink = (int)module_path[_vol_idx];
		if (sym_name) _vol_sink = (int)sym_name[_vol_idx];
		if (src_filepath) _vol_sink = (int)src_filepath[_vol_idx];
		_vol_sink = num_inlines;

		if (_vol_idx < num_inlines) {
			auto i = inlines[_vol_idx];
			if (i.fnname) _vol_sink = i.fnname[_vol_idx];
			if (i.filepath) _vol_sink = i.filepath[_vol_idx];
			if (i.lineno) _vol_sink = i.lineno;
		}
	}
};

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

		auto sym = mod->pdb->find_symbol_for_addr(mod_raddr);
		if (!sym) {
			res->err = "Symbol not found";
			return false;
		}
		
		res->sym_name = mod->pdb->stralloc[sym->name];

		SourceLoc src_loc = {};
		if (mod->pdb->find_source_loc_for_addr(sym, mod_raddr, &src_loc)) {
			res->src_filepath = src_loc.filepath;
			res->src_lineno = src_loc.lineno;
		}
		
		if (sym->inline_depth > 0) {
			mod->pdb->trace_inlinesites_for_addr(sym, mod_raddr, res->inlines, SymResult::MAX_INLINES, &res->num_inlines);
		}

		return res->valid();
	}

	bool has_symbol_for_addr (void* ptr, SymResult const& dbghelp_res) {
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
	}
	
	__declspec(noinline)
	void measure_pdb_parse (void* ptr) {
		auto* mod = mod_cache.find_module_for_addr(inspectee, (uintptr_t)ptr);
		mod_cache.clear(); // clear cache so this can be called repeatedly
	}

	// Hopefully noinline will ensure the compiler never optimized away unused SymResult, thus invalidating measurement!
	// if observed will need to fix this at the callsite
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

		{ ZoneScopedN("without pdb load");

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
			TimerMeasZone(tfind_symbol_for_addr);
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
			TimerMeasZone(tfind_source_loc_for_addr);
			if (mod->pdb->find_source_loc_for_addr(sym, mod_raddr, &src_loc)) {
				res->src_filepath = src_loc.filepath;
				res->src_lineno = src_loc.lineno;
			}
		}

		if (sym->inline_depth > 0) {
			TimerMeasZone(ttrace_inlinesites);
			mod->pdb->trace_inlinesites_for_addr(sym, mod_raddr, res->inlines, SymResult::MAX_INLINES, &res->num_inlines);
		}

		return res->valid();
		}
	}
	
	void print_pdb_stats (void* ptr) {
		auto* mod = mod_cache.find_module_for_addr(inspectee, (uintptr_t)ptr);
		if (mod->pdb) mod->pdb->print_stats_for_lookup();
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
