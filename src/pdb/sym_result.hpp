#pragma once
#include "util.hpp"
#include "lookup.hpp"
#include "dbghelp_api.hpp"

class SymResolverBase;
struct SymResult;

inline bool nullable_strcmp (const char* l, const char* r) {
	if ((l == nullptr) != (r == nullptr))
		return false;
	if (l == nullptr)
		return true;
	return strcmp(l, r) == 0;
}

/*
> TypeIndex && Index
appear to indices that do not correspond to the indices used internally in the pdb, and are unstable across different dbghelp runs
They may be returned only to allow the user to do faster lookups that direcly go into the dbghelp/DIA caches, which is why I will never be able to replicate them
For mismatch checking these need to be ignored

> ModBase
Sometimes is 0 for no reason

> Address
Virtual start address of symbol, ie sym->base_addr + mod->base_addr
For inlinesites this is sometimes the function symbol/root callsite start address, but sometimes it is something else I don't understand
__ImageBase returns 0 instead of mod->base_addr for some stupid reason

> Size
for module symbols makes sense, but for global data symbols it has to be "estimated" based on the typeinfo, which I cannot easily replicate
for global function symbols it's even more weird and dbghelp seemingly sometimes computes it based on types extracted from name mangling
__ImageBase behaves weirdly, Size=64 == sizeof(IMAGE_DOS_HEADER)? It also seems to be the only one where Address=0

> Tag: SymTagEnum
Function, SymTagData, SymTagPublicSymbol, SymTagThunk, SymTagInlineSite have been observed, I can mostly replicate these

> Flags
Don't bother returning flags, i've only observed very few of them appear and
at least SYMFLAG_EXPORT seems to be determined based on PE export table instead of pdb, which is overcomplicated imho
a full 1-1 match of dbghelp behavior is probably only possibly by caching dbghelp results, which is not my goal

> Value, Register, Scope
Seen only 0 for these?
*/

inline void print_diff_SYMBOL_INFO (const SYMBOL_INFO& l, const SYMBOL_INFO& r) {
	logf(" > SYMBOL_INFO:       custom |      dbghelp\n");
	//logf("   |TypeIndex : %12d | %12d \n",     l.TypeIndex, r.TypeIndex);
	//logf("   |Index     : %12d | %12d \n",     l.Index    , r.Index    );
	logf("   |Size      : %12x | %12x \n",     l.Size     , r.Size     );
	//logf("   |ModBase   : %12llx | %12llx \n", l.ModBase  , r.ModBase  );
	logf("   |Flags     : %12x | %12x \n",     l.Flags    , r.Flags    );
	logf("   |Value     : %12llx | %12llx \n", l.Value    , r.Value    );
	logf("   |Address   : %12llx | %12llx \n", l.Address  , r.Address  );
	logf("   |Register  : %12d | %12d \n",     l.Register , r.Register );
	logf("   |Scope     : %12d | %12d \n",     l.Scope    , r.Scope    );
	logf("   |Tag       : %12s | %12s \n",     SymTagEnum_str((SymTagEnum)l.Tag), SymTagEnum_str((SymTagEnum)r.Tag));
}
inline bool sym_info_equal_diff (const SYMBOL_INFO& l, const SYMBOL_INFO& r) {
	return true
		//&& l.TypeIndex    == r.TypeIndex
		//&& l.Index    == r.Index
		&& l.Scope    == r.Scope
		//&& l.Size     == r.Size // Not replicating __ImageBase Size=64 right now
		&& l.ModBase    == r.ModBase // For some godforsaken reason __stdio_common_vfprintf counts is detected as export symbol and is randomly ModBase=0
		//&& l.Flags    == r.Flags
		&& l.Value    == r.Value
		//&& l.Address  == r.Address
		&& l.Register == r.Register
		&& l.Scope    == r.Scope
		&& l.Tag      == r.Tag;
}

struct MismatchCounts {
	int total_checks = 0;
	int total_mismatches = 0;

	int symbol_mismatch = 0;
	int source_mismatch = 0;
	int inline_mimatch = 0;
	int info_mimatch = 0;
	int info_size_mimatch = 0;
	int info_flags_mimatch = 0;
	int info_tag_mimatch = 0;

	//int symbol_mismatch_overlap = 0; // different symbol name (but overlapping symbol with correct name existed)
	//int symbol_name_mangled = 0; // correct symbol but we did not filer out mangled scoped info (Note: I did not enable demangling, but dbghelp still 'stripped' the name of the mangled parts)
	int DH_no_source = 0; // we have source info, dbghelp does not

	int other = 0;

	void print () {
		logf("Mismatches Summay:\n");
		logf("  symbol_mismatch: %d\n", symbol_mismatch);
		logf("  source_mismatch: %d\n", source_mismatch);
		logf("  inline_mimatch: %d\n", inline_mimatch);
		logf("  info_mimatch: %d\n", info_mimatch);
		logf("  info_size_mimatch: %d\n", info_size_mimatch);
		logf("  info_flags_mimatch: %d\n", info_flags_mimatch);
		logf("  info_tag_mimatch: %d\n", info_tag_mimatch);
		//logf("  symbol_mismatch_overlap: %d\n", symbol_mismatch_overlap);
		//logf("  symbol_name_mangled: %d\n", symbol_name_mangled);
		logf("  DH_no_source: %d\n", DH_no_source);
		logf("  other: %d\n", other);
		logf("= (%d/%d) %.1f%% mismatches\n", total_mismatches, total_checks, (float)total_mismatches / (float)total_checks * 100.0f);
	}
	
	// horrible way to pass this data
	struct Data {
		MismatchCounts* counts = nullptr;
		SymResolverBase* resolver;
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

	static inline constexpr size_t INFO_RELEVANT_SIZE = offsetof(SYMBOL_INFO, NameLen); // exclude NameLen, MaxNameLen, and Name[1] at end of struct
	SYMBOL_INFO info; // full dbghelp info, all of which other than the name is irrelevant for tracy use case
	
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

		// Need to do this if we want to memcmp to test symbol equality
		info = {};
		info.SizeOfStruct = sizeof(SYMBOL_INFO);
		info.MaxNameLen = MAX_SYM_NAME;
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
			//if (nullable_strcmp(module_path, r.module_path) != 0) return false;
			if (!nullable_strcmp(sym_name, r.sym_name)) return false;
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
			//if (nullable_strcmp(module_path, r.module_path) != 0) return false;
			if (!nullable_strcmp(sym_name, r.sym_name)) return false;

			if (has_source() != r.has_source()) return false;
			if (has_source()) {
				if (!nullable_strcmp(src_filepath, r.src_filepath)) return false;
				if (src_lineno != r.src_lineno) return false;
			}
		}
		else {
			// If both throw error it counts as a match as comparing errors may be hard
		}
		return true;
	}
	
	bool info_equal (SymResult const& r) const {
		// Assume SizeOfStruct, Reserved, and MaxNameLen are set up identically to make this simple
		// We should be able to ignore NameLen though as the actual string is compared
		//return memcmp(&info, &r.info, INFO_RELEVANT_SIZE) == 0;
		
		// TODO: for now just compare the values I actively try to get, later try to achive exact matches
		//return true
		//	&& info.Scope == r.info.Scope
		//	//&& info.Size == r.info.Size
		//	//&& info.Flags == r.info.Flags
		//	&& info.Value == r.info.Value
		//	&& info.Address == r.info.Address
		//	&& info.Register == r.info.Register
		//	&& info.Scope == r.info.Scope
		//	&& info.Tag == r.info.Tag
		//;
		return sym_info_equal_diff(info, r.info);
	}
	
	bool _equal (SymResult const& r, MismatchCounts::Data c={}) const {
		auto* counts = c.counts;

		if (valid() != r.valid()) {
			if (counts)
				counts->other++;
			return false;
		}

		if (valid()) {
			// dbghelp.dll not returning module name, assume it's correct
			//if (my_strcmp(module_path, r.module_path) != 0) return false;
			if (!nullable_strcmp(sym_name, r.sym_name)) {
				if (counts) {
					counts->symbol_mismatch_or_mangled(*this, r, c);
				}
				return false;
			}

			if (!info_equal(r)) {
				if (counts) {
					counts->info_mimatch++;

					if (info.Size != r.info.Size)
						counts->info_size_mimatch++;
					if (info.Flags != r.info.Flags)
						counts->info_flags_mimatch++;
					if (info.Tag != r.info.Tag)
						counts->info_tag_mimatch++;
				}
				return false;
			}

			if (has_source() != r.has_source()) {
				if (has_source()) {
					if (counts) {
						counts->DH_no_source++;
					}
				} else {
					if (counts) {
						counts->source_mismatch++;
					}
				}
				return false;
			}
			if (has_source()) {
				if (!nullable_strcmp(src_filepath, r.src_filepath)) {
					if (counts) {
						counts->source_mismatch++;
					}
					return false;
				}
				if (src_lineno != r.src_lineno) {
					if (counts) {
						counts->source_mismatch++;
					}
					return false;
				}
			}
				
			if (num_inlines != r.num_inlines) {
				if (counts) {
					counts->inline_mimatch++;
				}
				return false;
			}
			for (int i=0; i<num_inlines; i++) {
				auto& li = inlines[i];
				auto& ri = r.inlines[i];
				if (    !nullable_strcmp(li.fnname, ri.fnname)
					 || !nullable_strcmp(li.filepath, ri.filepath)
					 || li.lineno != ri.lineno ) {
					if (counts) {
						counts->inline_mimatch++;
					}
					return false;
				}
			}
		}
		else {
			// If both throw error it counts as a match as comparing errors may be hard
		}
		return true;
	}
	bool equal (SymResult const& r, MismatchCounts::Data c={}) const {
		bool match = _equal(r, c);
		if (c.counts) {
			c.counts->total_checks++;
			if (!match) {
				c.counts->total_mismatches++;
			}
		}
		return match;
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


		//logf("<<TypeInd:%d>> ", info.TypeIndex);
		//logf("<<Tag:%d>> ", info.Tag);

		//logf("%15s!%s ", module_path ? module_path : "[unknown]", sym_name);
		logf("%s ", sym_name);

		if (has_source()) {
			logf("%-15s:%d\n", src_filepath, src_lineno);
		}
		else {
			logf("(No source info)\n");
		}

		//print_info();

		for (int i=0; i<num_inlines; i++) {
			if (inlines[i].filepath) {
				logf(" |inl %-15s %15s:%d\n", inlines[i].fnname, inlines[i].filepath, inlines[i].lineno);
			}
			else {
				logf(" |inl (No info)\n");
			}
		}
	}
	
	void print_diff (const char* mod_name, int64_t rel_addr, SymResult const& r) const {
		if (r.valid())
			logf("!! [%s+%llx] (%s) Result Mismatch:\n", mod_name, rel_addr, r.sym_name);
		else
			logf("!! [%s+%llx] (SymResolver: %s) Result Mismatch:\n", mod_name, rel_addr, sym_name);

		if (valid() != r.valid()) {
			if (!  valid()) logf("> SymResolver: %s\n", err);
			if (!r.valid()) logf("> dbghelp.dll: %s\n", r.err);
			return;
		}

		//if (   strcmp(module_path, r.module_path) != 0
		//	|| strcmp(sym_name, r.sym_name) != 0 ) {
		//	logf("> sym:         \"%s!%s\" !=\n", module_path,sym_name);
		//	logf("> dbghelp.dll: \"%s!%s\"\n", r.module_path,r.sym_name);
		//}
		if (!nullable_strcmp(sym_name, r.sym_name)) {
			logf("> SymResolver: \"%s!%s\" !=\n", module_path,sym_name);
			logf("> dbghelp.dll: \"[unknown]!%s\"\n", r.sym_name);
		}
		if (!info_equal(r)) {
			print_diff_SYMBOL_INFO(info, r.info);
		}
		if (   has_source() != r.has_source()
			|| !nullable_strcmp(src_filepath, r.src_filepath) || src_lineno != r.src_lineno) {
			
			if (has_source()) {
				logf("> SymResolver: \"%s:%d\" !=\n", src_filepath,src_lineno);
			}
			else {
				logf("> SymResolver: (No source info) !=\n");
			}
				
			if (r.has_source()) {
				logf("> dbghelp.dll: \"%s:%d\"\n", r.src_filepath,r.src_lineno);
			}
			else {
				logf("> dbghelp.dll: (No source info)\n");
			}
		}
		
		for (int i=0; i<std::max(num_inlines, r.num_inlines); i++) {
			// inlines past num_inlines no longer cleared as an optimization, need to compare correctly!
			auto l = i < num_inlines ? inlines[i] : SourceLocAndFn{};
			auto dh = i < r.num_inlines ? r.inlines[i] : SourceLocAndFn{};

			if (   !nullable_strcmp(l.fnname, dh.fnname)
				|| !nullable_strcmp(l.filepath, dh.filepath)
				||  l.lineno != dh.lineno) {
				logf(" |inl%d SymResolver: %-15s %15s:%d\n", i, l.fnname, l.filepath, l.lineno);
				logf(" |inl%d dbghelp.dll: %-15s %15s:%d\n", i, dh.fnname, dh.filepath, dh.lineno);
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
