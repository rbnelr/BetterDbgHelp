#pragma once
#include "util.hpp"
#include "codeview.hpp"
#include "pdb_locator.hpp"
#include "address_index.hpp"

#include <map>
#include <cstdlib>

// HACK: for the moment so I can check things during pdb parsing against dbghelp
inline HANDLE _hprocess = INVALID_HANDLE_VALUE;
inline uint64_t _mod_base = 0;

struct Stats {
	size_t num_inlinesites = 0;
	size_t num_lineinfo = 0;
	size_t num_lineinfo_blocks = 0;
	size_t num_lineinfo_delta_coded = 0;
	size_t num_strings = -1;
};

#include "lineinfo.hpp"

// https://github.com/PascalBeyer/PDB-Documentation

struct Symbol {
	uint64_t base_addr = 0; // relative to module
	uint32_t size = 0;
	StrAlloc::sid name = -1;
	
//// Info not needed by profilers but needed to replicate SYMBOL_INFO from SymFromAddr
	// Value, Register, Scope seem to always be 0 for SymFromAddr
	//uint32_t si_flags = 0;
	SymTagEnum si_tag = SymTagEnum::SymTagNull;

	uint8_t inline_depth = 0;
	int16_t module_index = -1;

	// Could eliminate now that allocated consecutive
	// possibly via single inlinesites offset
	BinAlloc::bid p_lineinfo = -1;
	BinAlloc::bid p_inlinesites = -1;

	__forceinline uint64_t get_addr () const {
		return base_addr;
	}
	static __forceinline Symbol dummy (uint64_t base_addr) { // needed for std::upper_bound
		return Symbol { base_addr };
	}
};

struct Inlinesite {
	StrAlloc::sid fnname;
	BinAlloc::bid pSibling = -1;
	BinAlloc::bid pChildren = -1;
	// followed by Lineinfo

	char* get_lineinfo () {
		return (char*)(this+1);
	}

	Inlinesite () {};
};
static_assert(alignof(Inlinesite) >= lineinfo::ALIGN);
static_assert(sizeof(Inlinesite) % lineinfo::ALIGN == 0);

static_assert(alignof(Symbol) == 8);
static_assert(alignof(Inlinesite) == 4);
static_assert(lineinfo::ALIGN == 4);

class FastPdbLookup {
public:

	std::filesystem::path pdb_path;

	BinAlloc binalloc;
	StrAlloc stralloc;
	
	AddressIndex symbol_index;

	std::vector<BinAlloc::bid> symbols;

	Symbol& get_sym (uint32_t idx) {
		return *binalloc.get<Symbol>(symbols[idx]);
	}
	
	void print_symbols () {
		for (uint32_t i=0; i<(uint32_t)symbols.size(); i++) {
			auto& s = get_sym(i);
			logf(">> %4llx %4x mod=%4d %s\n", s.base_addr, s.size, s.module_index, stralloc[s.name]);
		}
	}

	Symbol* find_symbol_for_addr (uint64_t addr, uint32_t* out_sym_idx) {
		//ZoneScoped;
		
		uint32_t idx = symbol_index.upper_bound((int64_t)addr);
		if (idx <= 0) {
			// first symbol after addr is first symbol, search failed
			return nullptr;
		}
		idx--;
		
		*out_sym_idx = idx;
		return binalloc.get_unchecked<Symbol>(symbols[idx]);
	}

	bool find_source_loc_for_addr (Symbol* sym, uint64_t addr, SourceLoc* out_src_loc) {
		//ZoneScoped;
		
		auto* lineinfo = binalloc.get<char>(sym->p_lineinfo);
		if (!lineinfo)
			return false;

		assert(addr >= sym->base_addr);
		if (addr >= sym->base_addr + sym->size) {
			// past symbol address range, no valid line number
			return false;
		}
		uint64_t sym_raddr = addr - sym->base_addr;
		
		return lineinfo::find_line_for_addr(lineinfo, sym_raddr, stralloc, out_src_loc);
	}
	
	int trace_inlinesites_for_addr (Symbol* sym, uint64_t addr, SourceLocAndFn* out_locs, int num_locs) {
		//ZoneScoped;
		assert(sym->inline_depth > 0); // only call when actually needed!

		uint64_t proc_raddr = addr - sym->base_addr;

		int depth = 0;
		
		BinAlloc::bid site_id = sym->p_inlinesites;
		while (depth < num_locs && site_id >= 0) {
			auto* site = binalloc.get<Inlinesite>(site_id);
			SourceLoc encoded_loc = {};

			if (lineinfo::find_line_for_addr_for_inline(site->get_lineinfo(), proc_raddr, stralloc, &encoded_loc)) {
				// Matching Inlinesite

				out_locs[depth].fnname = stralloc[site->fnname];
				out_locs[depth].filepath = encoded_loc.filepath;
				out_locs[depth].lineno = encoded_loc.lineno;
				out_locs[depth].line_start_offset = encoded_loc.line_start_offset;
				depth++;

				// site->pChildren != null: recurse into subtree
				// site->pChildren == null: leaf site found => matching branch explored; stop
				site_id = site->pChildren;
			}
			else {
				// Non-matching Inlinesite
				
				// site->pSibling != null: look at next sibling
				// site->pSibling == null: last sibling checked => matching branch explored; stop
				site_id = site->pSibling;
			}
		}
		return depth;
	}

	Stats stats;

	void print_stats () {
		// do this lazily as string pushes are complex
		if (stats.num_strings == -1) {
			stats.num_strings = count_strings();
		}

		logf("@ PDB %s:\n", pdb_path.string().c_str());

		symbol_index.print_stats(symbols.size(), "symbol index");
		logf("binalloc             :         | %.1f kB\n", binalloc.size()/1000.0f);
		logf("stralloc             : %7llu | %.1f kB\n", stats.num_strings, stralloc.size()/1000.0f);
		logf("symbols              : %7llu | %.1f kB\n", symbols.size(), symbols.size() * sizeof(Symbol)/1000.0f);
		logf("inlinesites          : %7llu | %.1f kB\n", stats.num_inlinesites, stats.num_inlinesites * sizeof(Inlinesite)/1000.0f);
		logf("lineinfos            : %7llu\n", stats.num_lineinfo);
		logf("lineinfo_blocks      : %7llu | %.1f kB\n", stats.num_lineinfo_blocks, stats.num_lineinfo_blocks * sizeof(lineinfo::Block)/1000.0f);
		logf("lineinfo_delta_coded : %7llu | %.1f kB\n", stats.num_lineinfo_delta_coded, stats.num_lineinfo_delta_coded * sizeof(lineinfo::DeltaCoded)/1000.0f);
	}
	size_t count_strings () const {
		char const* cur = stralloc.v.data();
		char const* end = cur + stralloc.v.size();

		size_t count = 0;
		while (cur < end) {
			cur += strlen(cur)+1;
			count++;
		}
		return count;
	}
};
