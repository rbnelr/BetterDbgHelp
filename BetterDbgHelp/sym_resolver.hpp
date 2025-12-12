#pragma once
#include "util.hpp"
#include "module_lookup.hpp"

struct SymResult {
	// TODO: dbghelp.dll requires us to pass in a string buffer, and I want to avoid heap alloc for the moment
	static inline constexpr unsigned STRBUF_SIZE = 4096;
	char str_buf[STRBUF_SIZE];

	union {
		const char* err;

		struct { // valid if sym_name!=null, otherwise err set
			const char* module_path;
			const char* sym_name;

			const char* src_filepath;
			uint32_t    src_lineno;
		
			int num_inlines;
			SourceLocAndFn inlines[64];
		};
	};

	SymResult () {
		module_path = nullptr;
		sym_name = nullptr;
		
		src_filepath = nullptr;
		src_lineno = 0;
		
		num_inlines = 0;

		memset(inlines, 0, sizeof(inlines));

		//memset((char*)this + STRBUF_SIZE, 0, sizeof(SymResult)-STRBUF_SIZE);
	}

	bool valid () const {
		return sym_name != nullptr;
	}

	bool has_source () const {
		return src_filepath != nullptr;
	}

	// TODO: inline frames

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
		return true;
	}
	bool operator!= (SymResult const& r) const {
		return !(*this == r);
	}

	void print () {
		if (!valid()) {
			printf("%s\n", err);
			return;
		}

		printf("%15s!%s ", module_path, sym_name);
		if (has_source()) {
			printf("%-15s:%d\n", src_filepath, src_lineno);
		}
		else {
			printf("(No source info)\n");
		}

		for (int i=0; i<num_inlines; i++) {
			if (inlines[i].filepath) {
				printf(" |inl %-15s %15s:%d\n", inlines[i].fnname, inlines[i].filepath, inlines[i].lineno);
			}
			else {
				printf(" |inl (No info)\n");
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
	void print_diff (SymResult const& r) const {
		if (valid() != r.valid()) {
			if (!  valid()) printf("> SymResolver: %s\n", err);
			if (!r.valid()) printf("> dbghelp:dll: %s\n", r.err);
			return;
		}

		//if (   strcmp(module_path, r.module_path) != 0
		//	|| strcmp(sym_name, r.sym_name) != 0 ) {
		//	printf("> sym:         \"%s!%s\" !=\n", module_path,sym_name);
		//	printf("> dbghelp:dll: \"%s!%s\"\n", r.module_path,r.sym_name);
		//}
		if (!my_strcmp(sym_name, r.sym_name)) {
			printf("> SymResolver: \"%s!%s\" !=\n", module_path,sym_name);
			printf("> dbghelp:dll: \"[unknown]!%s\"\n", r.sym_name);
		}
		if (   has_source() != r.has_source()
			|| !my_strcmp(src_filepath, r.src_filepath) || src_lineno != r.src_lineno) {
			
			if (has_source()) {
				printf("> SymResolver: \"%s:%d\" !=\n", src_filepath,src_lineno);
			}
			else {
				printf("> SymResolver: (No source info) !=\n");
			}
				
			if (r.has_source()) {
				printf("> dbghelp:dll: \"%s:%d\"\n", r.src_filepath,r.src_lineno);
			}
			else {
				printf("> dbghelp:dll: (No source info)\n");
			}
		}
		for (int i=0; i<std::max(num_inlines, r.num_inlines); i++) {
			if (   !my_strcmp(inlines[i].fnname, r.inlines[i].fnname)
				|| !my_strcmp(inlines[i].filepath, r.inlines[i].filepath)
				||  inlines[i].lineno != r.inlines[i].lineno) {
				printf(" |inl%d SymResolver: %-15s %15s:%d\n", i, inlines[i].fnname, inlines[i].filepath, inlines[i].lineno);
				printf(" |inl%d dbghelp:dll: %-15s %15s:%d\n", i, r.inlines[i].fnname, r.inlines[i].filepath, r.inlines[i].lineno);
			}
		}
	}
};

class SymResolver {
	HANDLE inspectee;

	ModuleCache mod_cache;
	
	// warmup time not meaningful as it includes pdb loading
	// only warmup to avoid including pdb loading in later measurement
	//TimerMeasurement twarmup = TimerMeasurement("warmup");
	TimerMeasurement taddr2sym = TimerMeasurement("addr2sym");
	TimerMeasurement ttrace_inlinesites = TimerMeasurement("trace_inlinesites");
	TimerMeasurement tCombinedAddr2sym = TimerMeasurement("CombinedAddr2sym");

public:
	SymResolver (HANDLE inspectee): inspectee{inspectee} {}
	
	void measure_addr2sym (char* ptr) {
		SymResult res = {};
		measure_addr2sym(ptr, &res);
	}
	
	bool addr2sym (void* ptr, SymResult* res) {
		uintptr_t addr = (uintptr_t)ptr;
		*res = {};

		auto* mod = mod_cache.find_module_for_addr(inspectee, addr);
		if (!mod) {
			res->err = "Module not found";
			return false;
		}
		if (!mod->pdb) {
			res->err = "Module pdb not found";
			return false;
		}

		uintptr_t mod_raddr = addr - mod->base_addr;
		
		auto sym = mod->pdb->find_symbol_for_addr(mod_raddr);
		if (!sym) {
			res->err = "Symbol not found";
			return false;
		}
		
		res->module_path = mod->path.c_str();
		res->sym_name = sym->name;
		res->src_filepath = nullptr;
		res->src_lineno = 0;

		SourceLoc src_loc = {};
		if (mod->pdb->find_source_loc_for_addr(sym, mod_raddr, &src_loc)) {
			res->src_filepath = src_loc.filepath;
			res->src_lineno = src_loc.lineno;
		}

		{
			mod->pdb->trace_inlinesites_for_addr(sym, mod_raddr, res->inlines, 64, &res->num_inlines);
		}

		return res->valid();
	}

	bool measure_addr2sym (void* ptr, SymResult* res) {
		auto _tCombinedAddr2sym = kiss::TimerMeasureZone(&tCombinedAddr2sym);
		auto _taddr2sym = kiss::TimerMeasureZone(&taddr2sym);
		ZoneScoped;

		uintptr_t addr = (uintptr_t)ptr;
		*res = {};

		auto* mod = mod_cache.find_module_for_addr(inspectee, addr);
		if (!mod) {
			res->err = "Module not found";
			return false;
		}
		if (!mod->pdb) {
			res->err = "Module pdb not found";
			return false;
		}

		uintptr_t mod_raddr = addr - mod->base_addr;
		
		auto sym = mod->pdb->find_symbol_for_addr(mod_raddr);
		if (!sym) {
			res->err = "Symbol not found";
			return false;
		}
		
		res->module_path = mod->path.c_str();
		res->sym_name = sym->name;
		res->src_filepath = nullptr;
		res->src_lineno = 0;

		SourceLoc src_loc = {};
		if (mod->pdb->find_source_loc_for_addr(sym, mod_raddr, &src_loc)) {
			res->src_filepath = src_loc.filepath;
			res->src_lineno = src_loc.lineno;
		}

		{
			auto _ttrace_inlinesites = kiss::TimerMeasureZone(&ttrace_inlinesites);

			mod->pdb->trace_inlinesites_for_addr(sym, mod_raddr, res->inlines, 64, &res->num_inlines);
			
			_ttrace_inlinesites.end();
			_taddr2sym.exclude(_ttrace_inlinesites);
		}

		return res->valid();
	}

	void print_timings () {
		mod_cache.ttry_get_and_cache_module.print();
		mod_cache.tload_pdb.print();
		taddr2sym.print();
		ttrace_inlinesites.print();
		tCombinedAddr2sym.print();
	}
};
