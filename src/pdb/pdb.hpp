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

template <typename K, typename V>
using hashmap = ankerl::unordered_dense::map<K, V>;

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

/*
typedef struct INLINESITESYM {
    unsigned short  reclen;    // Record length
    unsigned short  rectyp;    // S_INLINESITE
    unsigned long   pParent;   // pointer to the inliner
    unsigned long   pEnd;      // pointer to this block's end
    CV_ItemId       inlinee;   // CV_ItemId of inlinee
    unsigned char   binaryAnnotations[1];   // an array of compressed binary annotations.
} INLINESITESYM;
typedef struct tagInlineeSourceLine {
    CV_ItemId      inlinee;       // function id.
    CV_off32_t     fileId;        // offset into file table DEBUG_S_FILECHKSMS
    CV_off32_t     sourceLineNum; // definition start line number.
} InlineeSourceLine;
*/
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

class PdbReader {
	//// Handle pdb paging logic
	MemoryMappedFile file;

	msf_header* header;

	struct Stream {
		u32 size;
		std::vector<u32> pages;
	};
	std::vector<Stream> streams;
	
	void* get_page (u32 idx) {
		return (char*)file.data() + idx * header->page_size;
	}
	static u32 ceil_div (u32 a, u32 b) {
		return (a + (b-1)) / b;
	}
	static size_t align_up (size_t x, size_t align) {
		return (x + align-1) / align * align;
	}
	static char* align_up (char* ptr, size_t align) {
		size_t x = (size_t)ptr;
		return (char*)((x + align-1) / align * align);
	}

	void* read_sts (u32 ptr) {
		u32 page_idx    = ptr / header->page_size;
		u32 ptr_in_page = ptr % header->page_size;
		
		u32 u32_per_page = header->page_size / sizeof(u32);
		u32 page_idx_page     = page_idx / u32_per_page;
		u32 page_idx_page_idx = page_idx % u32_per_page;

		// Only huge pdb files have more then one here, assert meant to test that case, but didn't see it yet
		// code should work fine though
		//assert(page_idx_page == 0);
		assert((char*)&header->page_list_of_stream_table_stream_page_list[page_idx_page] - (char*)header < header->page_size);
		u32* sts_pages = (u32*)get_page(header->page_list_of_stream_table_stream_page_list[page_idx_page]);

		return (char*)get_page(sts_pages[page_idx_page_idx]) + ptr_in_page;
	}
	
	// Its possible to implement a fread-style interface to abstract away the pdb paging scheme
	// but unfortunately due to the complicated nature of the pdb format and the way that many of the records are variable length
	// actually reading via pointer math is way simpler and crucially allows storing references to data for later use, unlike fread io
	// so this function is used to copy an entire stream into a consecutive vector of bytes
	// this avoids all the complexities of the above at the expense of increased ram use, but may be faster
	// however the returned vector can be scoped as to only actually require a fraction of the size of the pdb in ram (mainly the module symbol streams)
	struct StreamData {
		std::unique_ptr<char[]> _data;
		size_t _size = 0;

		char* data () { return _data.get(); }
		size_t size () { return _size; }
	};
	StreamData copy_stream_consecutive (u32 streami) {
		ZoneScoped;

		auto& stream = streams[streami];

		size_t page_size = (size_t)header->page_size;
		size_t padded_stream_size = align_up((size_t)stream.size, page_size);

		// std::make_unique<char[]> initializes memsets the memory for some insane reason, this doesn't which makes this function -20% faster
		StreamData data;
		data._data = std::make_unique_for_overwrite<char[]>(padded_stream_size);
		data._size = stream.size;
		
		char* file_data = (char*)file.data();
		char* dst = data.data();

		for (u32 page_idx : stream.pages) {
			char* src = file_data + page_idx * page_size;
			memcpy(dst, src, page_size);
			dst += header->page_size;
		}

		return data;
	}

	// NOTE: prefer storing stream data for as short as possible to minimize memory use
	StreamData pdb_info_data; // needed for named_streams to have string_view key
	StreamData names_data; // needed for names lookup
	StreamData TPI_data; // needed for typeid2name to have non-owning string
	//StreamData IPI_data;
	StreamData DBI_data; // needed for read_module_symbol_streams to iterate modules
	//StreamData section_header_dump_data;
	//StreamData symbol_record_stream_data;

	hashmap<std::string_view, u32> named_streams;

	optional_debug_header_substream opt_streams;

	// contains function names for source line info (both normal and inline stack)
	const char* names = nullptr;
	size_t names_size = 0;

	// Struct, Class and Union names from TPI, these are already fully formatted
	hashmap<CV_typ_t, const char*> typeid2name;
	// Func and MemberFunc names from IPI (IDs will overlap with typeid2name!)
	hashmap<CV_ItemId, StrAlloc::sid> IPI_id2name;

	struct Section {
		std::string name;

		uint64_t base_addr;
		size_t size;
	};
	std::vector<Section> sections_sorted;

	//std::vector<pdb_module_information*> modules;

#define EXTRACT_ALL_NAMES 0
#if !EXTRACT_ALL_NAMES
	hashmap<u32, StrAlloc::sid> names_extracted;
	// copy string from /names entry to stralloc on first reference
	
	// copy names buffer 1 to 1 simply offset pointers to extract as an optimziation
	// NOTE: /names contains many many filepaths, it does not appear that these are duplicated
	// which is why I removed the string-hashmap based deduplication I did previously
	// but it does contain many strings that are unused by us, for instance it appears that every single included header is referenced, even if it contained no generated code or perhaps only dead code
	// also in just the city builder executable lots of type names is in there (no idea why just in that one)
	
	// TODO: this is a big bottleneck in reading the pdb
	// so the EXTRACT_ALL_NAMES optimization trades memory use for faster pdb reading speed
	// if I created my own file format to be cached after conversion this would be worth it to do
	StrAlloc::sid extract_name (u32 names_offset) {

		auto it = names_extracted.find(names_offset);
		if (it != names_extracted.end())
			return it->second;
		
		auto* str = names + names_offset;

		auto sid = lookup.stralloc.push(str);
		names_extracted.emplace(names_offset, sid);
		return sid;
	}
#else
	// despite the names_extracted lookup showing up in the profiler this only speeds things up by a few percent, probably not worth it
	StrAlloc::sid names_base;
	void copy_names_strings () {
		names_base = stralloc.push_bytes(names, names_size);
	}

	StrAlloc::sid extract_name (u32 names_offset) {
		auto sid = names_base + names_offset;
		return sid;
	}
#endif

	// Turns "?_OptionsStorage@?1??__local_stdio_scanf_options@@9@4_KA" into "_OptionsStorage"
	// like dbghelp does (it does this even without SYMOPT_UNDNAME, ie demangling)
	// dbghelp seems to only process "?..." not "??..." names
	// This is inconsistent however, it does not seem to happen for import symbol names
	// (whenever pdbs are missing dbghelp looks for the name in the import of the dll instead)
	StrAlloc::sid trim_mangled_name (const char* name) {
		if (name[0] == '?' && name[1] != '?') {
			const char* begin = name+1;
			const char* end = strchr(begin, '@');
			if (end && begin != end) {
				return lookup.stralloc.push(begin, end - begin);
			}
		}
		return lookup.stralloc.push(name);
	}

	void read_header () {
		header = (msf_header*)file.data();
		assert(strncmp((const char*)header->signature, "Microsoft C/C++ MSF 7.00\r\n\032DS\0\0\0", 32) == 0);
	}
	
	void read_stream_table () {
		ZoneScoped;

		//u32* _sts_ppages = (u32*)get_page(header->page_list_of_stream_table_stream_page_list[0]);
		//u32* _sts_pages = (u32*)get_page(_sts_ppages[0]);
		//char* _sts_start = (char*)read_sts(0);

		u32 cur = 0;
		u32 amount_of_streams = *(u32*)read_sts(cur);
		cur += sizeof(u32);

		streams.reserve(amount_of_streams);
		
		while (streams.size() < amount_of_streams) {
			u32 stream_size = *(u32*)read_sts(cur);
			cur += sizeof(u32);

			// The assumtion that deleted streams don't count seems to be wrong due to crash and seems to be verified by looking at data
			//if (stream_size == 0xffffffff) {
			//	// I think, this deleted stream does not count for amount_of_streams, but the link above is not clear on this
			//	continue;
			//}
			if (stream_size == 0xffffffff) {
				stream_size = 0;
			}
		
			Stream s;
			s.size = stream_size;
			streams.push_back(s);
		}
		
		for (u32 si=0; si<streams.size(); si++) {
			auto& stream = streams[si];
			//logf("Stream %3d: { ", si);
		
			size_t num_pages = ceil_div(stream.size, header->page_size);
			for (u32 i=0; i<num_pages; i++) {
				u32 page_idx = *(u32*)read_sts(cur);
				cur += sizeof(u32);
		
				stream.pages.push_back(page_idx);
		
				//logf("%d, ", page_idx);
			}
		
			//logf("}\n");
		}
	}

	void read_pdb_info (PDB_Locator::PDB_guid_and_age const& rsds) {
		
		pdb_info_data = copy_stream_consecutive(1);
		char* ptr = pdb_info_data.data();

		auto* info = (pdb_information_stream_header*)ptr;
		ptr += sizeof(pdb_information_stream_header);

		if (!PDB_Locator::verify_pdb(rsds, info))
			throw std::runtime_error("PDB loaded but GUID or age mismatch for "+ lookup.pdb_path.string()
				+"\nThis likely means the pdb is from a different program or an older build, symbols will likely be wrong!");

		// read named stream hashmap
		u32 string_buffer_size = *(u32*)ptr;
		ptr += sizeof(u32);

		char* string_buffer = ptr;
		ptr += string_buffer_size;

		u32 amount_of_entries = *(u32*)ptr;
		ptr += sizeof(u32);
		u32 capacity = *(u32*)ptr;
		ptr += sizeof(u32);
		
		// bit_array present_bits
		u32 present_word_count = *(u32*)ptr;
		ptr += sizeof(u32);
		u32* present_bits = (u32*)ptr;
		ptr += present_word_count * sizeof(u32);

		// bit_array deleted_bits
		u32 deleted_word_count = *(u32*)ptr;
		ptr += sizeof(u32);
		u32* deleted_bits = (u32*)ptr;
		ptr += deleted_word_count * sizeof(u32);

		struct KeyValue {
			u32 key;
			u32 value;
		};
		KeyValue* entries = (KeyValue*)ptr;
		ptr += amount_of_entries * sizeof(KeyValue);

		// unused
		ptr += sizeof(u32);
		
		//logf("Named Streams:\n");
		for(u32 index = 0, entry_index = 0; index < capacity && entry_index < amount_of_entries; index++){
			u32 word_index = index / (sizeof(u32) * 8);
			u32 bit_index  = index % (sizeof(u32) * 8);
			
			if(word_index < present_word_count && (present_bits[word_index] & (1u << bit_index))){
				auto& kv = entries[entry_index++];

				//logf("> %s: %d\n", &string_buffer[kv.key], kv.value);
				named_streams[&string_buffer[kv.key]] = kv.value;
				continue;
			}
		}
	}

	void read_names () {
		ZoneScoped;

		if (!named_streams.contains("/names")) {
			// this happens with pdbs downloaded from MS symbol servers, possibly because they are stripped
			// Simply leave names as null works, as these files also don't contain references to it
			return;
		}

		names_data = copy_stream_consecutive(named_streams.at("/names"));
		char* ptr = names_data.data();

		u32 signature = *(u32*)ptr;
		ptr += sizeof(u32);
		if (signature == 0xEFFEEFFE) {
		
			u32 hash_version = *(u32*)ptr;
			ptr += sizeof(u32);
		
			u32 string_buffer_size = *(u32*)ptr;
			ptr += sizeof(u32);
			
			names = ptr;
			names_size = string_buffer_size;
			ptr += string_buffer_size;

		#if EXTRACT_ALL_NAMES
			copy_names_strings();
		#endif
			
			// There is somd kind of hashmap in here, is this is a hashmap from string -> type/symbol id?
			// Presumably this is not useful to me in this project
			u32 bucket_count = *(u32*)ptr;
			ptr += sizeof(u32);

			u32* buckets = (u32*)ptr;
			ptr += bucket_count * sizeof(u32);

			u32* amount_of_strings = (u32*)ptr;
			ptr += sizeof(u32);
		}
	}
	
	void print_dump_names () const {
		logf("PDB /names string buffer:\n");
		char const* cur = names;
		char const* end = cur + names_size;

		while (cur < end) {
			auto offset = cur - names;
		#if !EXTRACT_ALL_NAMES
			char const* used = names_extracted.contains((uint32_t)offset) ? "U":" ";
		#else
			char const* used = "U";
		#endif

			logf("> [%8llx] %s %s\n", offset, used, cur);
			cur += strlen(cur)+1;
		}
	}

	void read_DBI () {
		ZoneScoped;

		DBI_data = copy_stream_consecutive(3);
		char* ptr = DBI_data.data();

		auto* header = (dbi_stream_header*)ptr;
		ptr += sizeof(dbi_stream_header);
		
		//// module_information_substream
		auto* ptr2 = ptr;

		/*
		while (ptr < ptr2+header->byte_size_of_the_module_information_substream) {
			auto* mi = (pdb_module_information*)ptr;
			
			ptr += sizeof(pdb_module_information);

			const char* mod_name = ptr;
			size_t mod_name_len = strlen(mod_name);
			ptr += mod_name_len+1;

			const char* file_name = ptr;
			size_t file_name_len = strlen(file_name);
			ptr += file_name_len+1;

			ptr = align_up(ptr, 4);

			//logf("> %d %-50s %-50s\n", mi->stream_index_of_module_symbol_stream, mod_name, file_name);

			Module m;
			m.mi = mi;
			m.name = std::string_view(mod_name, mod_name_len);
			m.file_name = std::string_view(file_name, file_name_len);
			//modules.push_back(m);
		}
		assert((ptr - ptr2) == header->byte_size_of_the_module_information_substream);
		*/
		ptr = ptr2 + header->byte_size_of_the_module_information_substream;
		
		//// section_contribution_substream
		u32 DBISCImpv = *(u32*)ptr;

		assert(DBISCImpv == (0xeffe0000 + 19970605));
		auto* section_contributions = (pdb_section_contribution*)(ptr + sizeof(u32)); // skip DBISCImpv
		u32 num_section_contributions = header->byte_size_of_the_section_contribution_substream / sizeof(pdb_section_contribution);
		
		// Section contributions seem just be a list of all the symbol address ranges merged, meaning mainly functions
		// For my use case it seems pointless to search section contributions, as finding the entry only tells you the module
		// which you then have to search for the actual symbol, I don't know how dbghelp implemented this, as the symbols in the module appear unsorted and even have duplicates
		// maybe there is a hashmap somehwere where the start address of the SC can be searched, but I can instead simply build a sorted list of all symbols instead
		// Except that in rare cases where functions with identical code get merged, dbghelp returns a seemingly random one, and I can't seem to ever return the same one as it
		// The roundabout way through section contributions does not help afaik
		/*
		for (u32 i=0; i<num_section_contributions; i++) {
			auto* sc = &section_contributions[i];
			//logf("> %d %8x %4x %d\n", sc->section_id, sc->offset, sc->size, sc->module_index);
			
			// assert that section contribution list entries are non-zero size, sorted and non-overlapping
			assert(sc->size > 0);
			if (i > 0) {
				auto* prev = &section_contributions[i-1];
				assert(sc->section_id > prev->section_id ||
					(sc->section_id == prev->section_id && sc->offset >= prev->offset + prev->size));
			}
		}
		*/
		ptr += header->byte_size_of_the_section_contribution_substream;
		
		//// section_map_substream
		//ptr2 = ptr;
		//
		//auto* sec_header = (pdb_section_map_stream_header*)ptr;
		//ptr += sizeof(pdb_section_map_stream_header);
		//assert(sec_header->number_of_section_descriptors == sec_header->number_of_logical_section_descriptors);
		//
		////while (ptr < ptr2+header->byte_size_of_the_section_map_substream) {
		//for (u32 i=0; i<sec_header->number_of_section_descriptors; i++) {
		//	auto* sm = (pdb_section_map_entry*)ptr;
		//	ptr += sizeof(pdb_section_map_entry);
		//}
		//
		//assert((ptr - ptr2) == header->byte_size_of_the_section_map_substream);
		ptr += header->byte_size_of_the_section_map_substream;

		//// source_information_substream
		//ptr2 = ptr;
		//
		//u16 amount_of_modules = *(u16*)ptr;
		//ptr += sizeof(u16);
		//u16 truncated_amount_of_source_files = *(u16*)ptr;
		//ptr += sizeof(u16);
		//
		//assert(amount_of_modules == modules.size());
		//
		//u16* source_file_base_index_per_module = (u16*)ptr;
		//ptr += amount_of_modules * sizeof(u16);
		//u16* amount_of_source_files_per_module = (u16*)ptr;
		//ptr += amount_of_modules * sizeof(u16);
		//
		//u32* source_file_name_offset_in_string_buffer = (u32*)ptr;
		////ptr += amount_of_source_files * sizeof(u32);
		//
		//u32 byte_size_of_the_source_information_substream;   // substream 3
		//u32 byte_size_of_the_type_server_map_substream;      // substream 4
		//
		//u32 index_of_the_MFC_type_server_in_type_server_map_substream;
		//
		//u32 byte_size_of_the_optional_debug_header_substream; // substream 6
		//u32 byte_size_of_the_edit_and_continue_substream;     // substream 5
		
		ptr += header->byte_size_of_the_source_information_substream;

		ptr += header->byte_size_of_the_type_server_map_substream;

		ptr += header->byte_size_of_the_edit_and_continue_substream;

		//// optional_debug_header_substream
		opt_streams = *(optional_debug_header_substream*)ptr;

		//byte_size_of_the_optional_debug_header_substream
	}
	void read_module_symbol_streams () {
		ZoneScoped;

		char* ptr = DBI_data.data();

		auto* header = (dbi_stream_header*)ptr;
		ptr += sizeof(dbi_stream_header);
		auto* end = ptr + header->byte_size_of_the_module_information_substream;

		// seemingly if private_symbols_were_stripped, there are still entries for functions but dbghelp suddenly
		// prefers the symbol_record_stream PUB32 symbols instead
		//if (header->flags.private_symbols_were_stripped)
		//	return;
		
		s16 module_index = 0;
		while (ptr < end) {
			auto* mi = (pdb_module_information*)ptr;
			ptr += sizeof(pdb_module_information);

			const char* mod_name = ptr;
			size_t mod_name_len = strlen(mod_name);
			ptr += mod_name_len+1;

			const char* file_name = ptr;
			size_t file_name_len = strlen(file_name);
			ptr += file_name_len+1;

			ptr = align_up(ptr, 4);

			// read symbol stream for each module, this contains function symbols and all lineinfo
			read_module_symbol_stream(module_index, mi);
			module_index++;

			//modules.push_back(mi);
		}
		assert(ptr == end);

		process_trampolines();
	}

	void read_section_header_dump () {
		auto section_header_dump_data = copy_stream_consecutive(opt_streams.stream_index_of_section_header_dump);
		assert(section_header_dump_data.size() == streams[opt_streams.stream_index_of_section_header_dump].size);

		char* ptr = section_header_dump_data.data();
		char* ptr2 = ptr;

		while (ptr < ptr2 + section_header_dump_data.size()) {
			auto* sh = (IMAGE_SECTION_HEADER*)ptr;
			ptr += sizeof(IMAGE_SECTION_HEADER);

			sections_sorted.push_back({ std::string((const char*)sh->Name, strnlen_s((const char*)sh->Name, 8)), sh->VirtualAddress, sh->Misc.VirtualSize });
			
			//char name[9] = {};
			//strncpy_s(name, (const char*)sh->Name, 8); // properly null-terminate
			//logf("> %7s %8x %8x\n", name, sh->VirtualAddress, sh->Misc.VirtualSize);
		}

		for (size_t i=1; i<sections_sorted.size(); i++) {
			assert(sections_sorted[i].base_addr > sections_sorted[i-1].base_addr + sections_sorted[i-1].size);
		}
	}
	[[msvc::forceinline]] bool resolve_rva (u32 offs, u16 seg, uint64_t* out_rva) {
		if (seg > 0) {
			// Sometimes global symbols have section ids to invalid sections, no idea why this happens
			// ex: __guard_fids_table, __guard_flags, __guard_iat_table, __guard_longjmp_table, __enclave_config, __guard_eh_cont_table
			if (seg > sections_sorted.size()) {
				return false;
			}
			auto seg_addr = sections_sorted[seg-1].base_addr;
			assert(offs <= sections_sorted[seg-1].size);
			*out_rva = (uint64_t)offs + (uint64_t)seg_addr;
		}
		else {
			// Special __ImageBase symbol has seg==0, so offs already is rva
			// I have not observed any other cases
			assert(offs == 0);
			*out_rva = (uint64_t)offs;
		}
		return true;
	}
	
	// deduplicate symbols with identical addresses, as functions can be merged, pick first one
	// currently in these cases I still regularily pick different symbols compared to dbghelp, I kinda gave up for the moment
	// I pick the first one, I remember seeing no apparent pattern (first one, last one etc.), but I may have messed up
	hashmap<uint64_t, int> extracted_symbol_addresses;
	struct Trampoline {
		TRAMPOLINESYM sym;
		s16 module_index;
	};
	std::vector<Trampoline> trampolines;

	// Did I not comment this 3 times already?
	// Anyway, there are cases where lineinfo exists with a different address then the functions it covers:
	// ex: __security_check_cookie: src\vctools\crt\vcstartup\src\gs\amd64\amdsecgs.asm
	// likely here this is because while the c++ compiler generates one lineinfo entry per function with identical address
	// the assembler for asm files generates just one lineinfo but the asm can contain multiple functions (and other symbols at arbitrary offsets?)
	// In this case the function is 16 bytes after the lineinfo
	// dbghelp likely does an entire second lookup, if you already did a symbol lookup for the lineinfo, which is slow
	// instead my model is that while the addresses may not match, for each function there is likely only one lineinfo it overlaps
	// so instead I lookup the lineinfo range the symbol touches at its base address and link that one to it
	// in fact lineinfo will currently be duplicated in the resultling memory to simplify things, but this could be changed,
	// but if its just asm files this is likely irrelevant
	struct C13Lineinfo {
		codeview_subsection_header* subsec;
		codeview_line_header* header () { return (codeview_line_header*)(subsec+1); }
	};
	std::map<uint64_t, C13Lineinfo> module_c13_lineinfo;
	
	codeview_subsection_header* find_single_overlapping_lineinfo (Symbol const& sym, uint64_t* out_lineinfo_addr) {
		if (module_c13_lineinfo.empty()) return nullptr;

		// upper_bound returns first item larger than input (usually one past end of the range of equal items)
			
		// lineinfo to right, higher base address, or null if all lineinfos lower
		auto upper = module_c13_lineinfo.upper_bound(sym.base_addr);
			
		// lineinfo to left, lower or equal base address, or null if all lineinfos higher
		auto lower = upper; // awkward code with --lower because it-1 is not allowed for std::map
		lower = upper != module_c13_lineinfo.begin() ? --lower : module_c13_lineinfo.end();

		// return lower address lineinfo if range overlaps
		if (lower != module_c13_lineinfo.end()) {
			auto lineinfo_contrib_end = lower->first + lower->second.header()->contribution_size;
			if (sym.base_addr < lineinfo_contrib_end) {
				*out_lineinfo_addr = lower->first;
				return lower->second.subsec;
			}
		}
		// else test higher address lineinfo to handle weird cases of lineinfo having base address before symbol
		// (__security_check_cookie : src\vctools\crt\vcstartup\src\gs\amd64\amdsecgs.asm)
		if (upper != module_c13_lineinfo.end()) {
			auto lineinfo_contrib_start = upper->first;
			if (sym.base_addr + sym.size > lineinfo_contrib_start) {
				*out_lineinfo_addr = upper->first;
				return upper->second.subsec;
			}
		}
		
		return nullptr;
	}

	// inlinee ID (function ID from IPI) -> first matching InlineeSourceLine entry
	// yes another case of conflicting info, quite a lot of it in rust executables as well, afaik this makes no logical sense
	ankerl::unordered_dense::map<CV_ItemId, InlineeSourceLine*> module_inlinee_c13;

	// Push inlinesites breath-first despite pdb storing them depth-first as lookup was found to be faster this way
	struct Site {
		INLINESITESYM* inl;
		BinAlloc::bid site_id = -1;
	};
	inline static constexpr int MAX_INLINE_DEPTH = 512;
	struct InlinesiteTree {
		// Use fixed size buffer to avoid vector of vector complexities which resulted in lots of heap allocation
		// instead each level can be a vector which allows heap allocation to be reused
		// This optimization is -22% to total pdb parsing time
		std::vector<Site> stack[MAX_INLINE_DEPTH];
		int written_depth = 0;
		
		void reserve () {
			for (int i=0; i<16; i++) {
				stack[i].reserve(64);
			};
		}
		void push (int depth, INLINESITESYM* inl) {
			if (depth >= MAX_INLINE_DEPTH)
				return; // Ignore this inlinesite if it exceeds max depth

			written_depth = std::max(written_depth, depth+1);

			stack[depth].push_back({ inl });
		}
		void clear () {
			for (int i=0; i<written_depth; i++) {
				stack[i].clear();
			}
			written_depth = 0;
		}
	} _inlinesites;

	void push_inline_tree (char* sym_info, char* file_checksum_ptr, int cur_symbol) {
		assert(_inlinesites.written_depth > 0);

		// iterate breath first to push data
		for (int depth=0; depth<_inlinesites.written_depth; depth++) {
			for (auto& site : _inlinesites.stack[depth]) {
				auto it = module_inlinee_c13.find(site.inl->inlinee);
				if (it == module_inlinee_c13.end())
					continue;

				auto name_it = IPI_id2name.find(site.inl->inlinee);
				if (name_it == IPI_id2name.end())
					continue;

				auto* anno_end = (PCompressedAnnotation)((char*)site.inl + sizeof(u16) + site.inl->reclen); // length field of codeview_symbol_header not contained in length
				
				Inlinesite s = {};
				s.fnname = name_it->second;
				site.site_id = lookup.binalloc.push(s);

				lookup.stats.num_inlinesites++;

				lineinfo::encode_compressed_annotation(
					site.inl->binaryAnnotations, anno_end,
					it->second->fileId, it->second->sourceLineNum,
					[this, file_checksum_ptr] (u32 offset_in_file_checksums) -> StrAlloc::sid {
						auto* cksm = (codeview_file_checksum*)((char*)file_checksum_ptr + offset_in_file_checksums);
						return extract_name(cksm->offset_in_string_table);
					}, lookup.binalloc, lookup.stats);
			}
		}
		
		// iterate each level and link up siblings and parent-children references
		for (int depth=0; depth<_inlinesites.written_depth; depth++) {
			auto& level = _inlinesites.stack[depth];
			auto* next_level = depth+1 < _inlinesites.written_depth ? &_inlinesites.stack[depth+1] : nullptr;
			
			auto* children_cur = next_level ? next_level->data() : nullptr;
			auto* children_end = next_level ? next_level->data() + next_level->size() : nullptr;

			for (size_t i=0; i<level.size(); i++) {
				auto& site = level[i];
				auto* right_sibling = i+1 < level.size() ? &level[i+1] : nullptr;

				auto* s = lookup.binalloc.get<Inlinesite>(site.site_id);
				// set right sibling reference if pdb pParent indicates relationship
				if (right_sibling && right_sibling->inl->pParent == site.inl->pParent)
					s->pSibling = right_sibling->site_id; // else leave at -1

				// iterate children list to find first child of this parent
				if (children_cur) {
					size_t pParent = (char*)site.inl - sym_info;
					// skipping all previous parents children without having to iterate the entire list every time
					// result is either this parents first child or child of a later parent
					while (children_cur < children_end && children_cur->inl->pParent < pParent)
						children_cur++;
					// set child if it belongs to this parent 
					if (children_cur < children_end && children_cur->inl->pParent == pParent) {
						s->pChildren = children_cur->site_id;
					}
				}
			}
		}
		
		lookup.get_sym(cur_symbol).inline_depth = (uint8_t)std::min(_inlinesites.written_depth, 255);
		if (_inlinesites.written_depth > 0 && !_inlinesites.stack[0].empty())
			lookup.get_sym(cur_symbol).p_inlinesites = _inlinesites.stack[0].front().site_id;
	}


	void read_module_symbol_stream (s16 module_index, pdb_module_information* mi) {
		if (mi->stream_index_of_module_symbol_stream == 0xffff)
			return; // no symbol data

		ZoneScoped;

		auto symbol_stream_data = copy_stream_consecutive(mi->stream_index_of_module_symbol_stream);
		char* ptr = symbol_stream_data.data();

		char* file_checksum_ptr = nullptr;

		char* sym_info = ptr;
		ptr += mi->byte_size_of_symbol_information;
		
		//auto* c11_line_information = (u8*)ptr;
		ptr += mi->byte_size_of_c11_line_information;

		char* c13_line_information = ptr;
		ptr += mi->byte_size_of_c13_line_information;

		auto global_references_bytes_size = *(u32*)ptr;
		auto num_global_references = global_references_bytes_size / 4;
		ptr += sizeof(u32);
		
		auto* global_references = (u32*)ptr;
		ptr += global_references_bytes_size;
		
		assert((ptr - symbol_stream_data.data()) == streams[mi->stream_index_of_module_symbol_stream].size);
		
		//logf("> Module %d\n", module_index);

		//// C13 line info
		{
			{
				//logf("> c13_line_information\n");
				char* ptr = c13_line_information;
			
				// first pass to find FILECHKSMS ptr
				while (ptr < c13_line_information + mi->byte_size_of_c13_line_information) {
					auto* header = (codeview_subsection_header*)ptr;
					ptr += sizeof(codeview_subsection_header);

					if (header->type == DEBUG_S_FILECHKSMS) {
						file_checksum_ptr = ptr;
						break;
					}
					ptr = (char*)header + sizeof(codeview_subsection_header) + header->length;
				}
			}
			
			// It seems like we can get duplicate entries of the same inlinee id across different modules via InlineeSourceLine
			// So in every module with a INLINESITE, the corresponding InlineeSourceLine and filename in DEBUG_S_FILECHKSMS exist in the same module
			// but the same inlinee file id can exist in multiple modules
			// this could be because due to link time optimization (the same function being inlined into different translation lines)
			// but actually, sometimes the filepath differs (same header, paths, compiled on different machines?)
			// so functions compiled from different copies of a header can get the same id
		
			auto read_inlinee_line_numbers = [&] (codeview_subsection_header* subsec) {
				auto* ptr = (char*)subsec;
				ptr += sizeof(codeview_subsection_header);
				auto* end = ptr + subsec->length;

				auto* header = (codeview_inlinee_source_line_header*)ptr;
				ptr += sizeof(codeview_inlinee_source_line_header);
				
				/*
				// rust (LLVM) seems to be output duplicate inlinee entries (same inlinee func id) even within the same module
				// filepath can differ:
				//                                   /rustc/1159e78c4747b02ef996e55082b704c09b970588/library\core\src\convert\mod.rs
				// C:\Users\Me\.rustup\toolchains\stable-x86_64-pc-windows-msvc\lib\rustlib\src\rust\library\core\src\convert\mod.rs
				// and even line numbers can differ for some bizzare reason
				// I have no idea how to handle this case correctly, there might be one InlineeSourceLine per INLINESITE, but I can't confirm this
				// I will have to see if my output matches dbghelp when taking the first or last entry instead of trying to match them by order
				auto check_duplicates = [&] (InlineeSourceLine* line) {
					auto it = module_inlinee_c13.find(line->inlinee);
					if (it != module_inlinee_c13.end()) {
						// duplicate entry, verify they are functionally identical
						//assert(line->sourceLineNum == it->second->sourceLineNum);

						//assert(line->fileId == it->second->fileId);
						//auto* a = get_lineinfo_source_filepath(mod, line->fileId);
						//auto* b = get_lineinfo_source_filepath(mod, it->second->fileId);
						//assert(strcmp(a,b)==0);

						//logf(">>>>>> %s\n", a);
						//logf(">>>>>> %s\n", b);
					}
				};
				*/
				if (header->signature == CV_INLINEE_SOURCE_LINE_SIGNATURE) {
					while (ptr < end) {
						auto* line = (InlineeSourceLine*)ptr;
						ptr += sizeof(InlineeSourceLine);
						
						//logf(">>  Line %d %s %d\n", line->sourceLineNum, get_lineinfo_source_filepath(mod, line->fileId), line->inlinee);
						
						//check_duplicates(line);
						module_inlinee_c13.try_emplace(line->inlinee, line);
					}
				} else if (header->signature == CV_INLINEE_SOURCE_LINE_SIGNATURE_EX) {
					while (ptr < end) {
						auto* line = (InlineeSourceLineEx*)ptr;
						ptr += sizeof(InlineeSourceLineEx);

						ptr += line->countOfExtraFiles * sizeof(CV_off32_t);
						
						//logf(">>  Line %d %s %d\n", line->sourceLineNum, get_lineinfo_source_filepath(mod, line->fileId), line->inlinee);
						
						//check_duplicates((InlineeSourceLine*)line);
						module_inlinee_c13.try_emplace(line->inlinee, (InlineeSourceLine*)line);
					}
				} else {
					assert(false);
				}
				assert(ptr == end);
			};

			auto read_line_numbers = [&] (codeview_subsection_header* subsec) {
				auto* ptr = (char*)subsec;
				ptr += sizeof(codeview_subsection_header);
				auto* end = ptr + subsec->length;
		
				// With usual compiler, this is once per function
				auto* header = (codeview_line_header*)ptr;
				ptr += sizeof(codeview_line_header);
				if (header->flags != 0)
					return; // CV_LINES_HAVE_COLUMNS not implemented
				
				uint64_t sec_offs = sections_sorted[header->contribution_section_id-1].base_addr;
				uint64_t rva = header->contribution_offset + sec_offs;
				
				// Why are there always conflicting sets of data in PDBs?
				// Even this basic C13 Data tends to exist for the same function multiple times (which tend to have conflicing contents as well)
				// Picking the first one seems to have been working ok
				auto check_duplicates = [&] () {
					auto it = module_c13_lineinfo.find(rva);
					if (it != module_c13_lineinfo.end()) {
						logf("> Conflicing C13 Lineinfo\n");

						auto* ptr1 = ptr;
						auto* ptr2 = (char*)it->second.header() + sizeof(codeview_line_header);

						auto* end2 = (char*)it->second.subsec + sizeof(codeview_subsection_header) + it->second.subsec->length;

						assert(subsec->length == it->second.subsec->length);

						while (ptr1 < end && ptr2 < end2) {
							auto* line_block = (codeview_line_block_header*)ptr1;
							ptr1 += sizeof(codeview_line_block_header);
							auto* line_block2 = (codeview_line_block_header*)ptr2;
							ptr2 += sizeof(codeview_line_block_header);
							
							auto* cksm = (codeview_file_checksum*)((char*)file_checksum_ptr + line_block->offset_in_file_checksums);
							auto* sourcefile = names + cksm->offset_in_string_table;

							assert(line_block->amount_of_lines == line_block2->amount_of_lines);
							assert(line_block->block_size == line_block2->block_size);
							assert(line_block->offset_in_file_checksums == line_block2->offset_in_file_checksums);
							logf(">> Block %d %d %s\n", line_block->block_size, line_block->offset_in_file_checksums, sourcefile);
							
							auto* lines = (codeview_line*)ptr1;
							auto* lines2 = (codeview_line*)ptr2;
							for (u32 i=0; i<std::min(line_block->amount_of_lines, line_block2->amount_of_lines); i++) {
								auto& line = lines[i];
								auto& line2 = lines2[i];

								assert(line.offset == line2.offset);
								//assert(line.start_line_number == line2.start_line_number);
								logf(">>  Line %d/%d %d/%d\n", line.start_line_number, line2.start_line_number, line.offset, line2.offset);
							}

							ptr1 += line_block->amount_of_lines * sizeof(codeview_line);
							ptr2 += line_block2->amount_of_lines * sizeof(codeview_line);
						}
						assert(ptr1 == end);
						assert(ptr2 == end2);
					}
				};

				//check_duplicates();
				module_c13_lineinfo.try_emplace(rva, C13Lineinfo{ subsec });
				
				// Line => Code range that has the same line number
				// Block => Consecutive lines (in compiled binary) where all come from the same file
				// Usually all of a functions code comes from one file, exceptions:
				// c++ #include in the middle of functions (very rare)
				// c++ constructors that have assignment of fields in the class in the header, and the actual ctor code in the source

				/*
				
				//logf(">> Header %d, %8x %8x\n", header->contribution_section_id, header->contribution_offset, header->contribution_size);
				
				while (ptr < ptr3 + subsec->length) {
					auto* line_block = (codeview_line_block_header*)ptr;
					ptr += sizeof(codeview_line_block_header);

					// if we get lineinfo, we also expect /names to exist
					assert(names != nullptr);
					
					//logf(">> Block %d %d %s\n", line_block->block_size, line_block->offset_in_file_checksums, get_lineinfo_source_filepath(mod, line_block->offset_in_file_checksums));
					
					auto* lines = (codeview_line*)ptr;
					for (u32 i=0; i<line_block->amount_of_lines; i++) {
						auto& line = lines[i];
			
						//logf(">>  Line %d %d\n", line.start_line_number, line.offset);

						if (i > 0) assert(line.offset >= lines[i-1].offset); // verify sorted
					}

					ptr += line_block->amount_of_lines * sizeof(codeview_line);

				}
				assert((ptr - ptr3) == subsec->length);
				*/
			};
			
			//logf("> c13_line_information\n");
			char* ptr = c13_line_information;

			while (ptr < c13_line_information + mi->byte_size_of_c13_line_information) {
				auto* header = (codeview_subsection_header*)ptr;
				ptr += sizeof(codeview_subsection_header);

				//logf(">> %s\n", DEBUG_S_SUBSECTION_TYPE_e_str(header->type));

				if ((header->type & DEBUG_S_IGNORE) == 0) {
					switch (header->type) {
						case DEBUG_S_LINES: {
							read_line_numbers(header);
						} break;
						case DEBUG_S_INLINEELINES: {
							read_inlinee_line_numbers(header);
						} break;
						//case DEBUG_S_FILECHKSMS: {
						//	file_checksum_ptr = ptr;
						//} break;
					}
				}
				ptr = (char*)header + sizeof(codeview_subsection_header) + header->length;
			}
			assert((ptr - c13_line_information) == mi->byte_size_of_c13_line_information);
		}
		
		//// Symbol info
		{
			char* ptr = sym_info;

			u32 signature = *(u32*)ptr;
			ptr += sizeof(u32);
			assert(signature == 4); // CV_SIGNATURE_C13
		
			//logf(">> symbol_information\n");
			
			int cur_symbol = -1;
			int inline_depth = 0;
			
			while (ptr < sym_info + mi->byte_size_of_symbol_information) {
				auto sym = (codeview_symbol_header*)ptr;
			
				ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
				ptr = align_up(ptr, 4);

				//int entry_offs = (int)((char*)sym - sym_info);
				//logf("> %7d [%4x] %d %s\n", entry_offs, sym->kind, sym->length, SYM_ENUM_e_str(sym->kind));
				
				auto push_symbol = [this, module_index, file_checksum_ptr]
						(u32 offs, u32 size, u16 seg, SymTagEnum tag, const char* name, bool get_lineinfo=false) -> int {
					uint64_t rva = offs + sections_sorted[seg-1].base_addr;
					
					//if (rva == 1057460) {
					//	printf("");
					//}

					if (extracted_symbol_addresses.find(rva) == extracted_symbol_addresses.end()) {
						auto bid = lookup.binalloc.push<Symbol>(Symbol());

						Symbol s = {};
						s.base_addr = rva;
						s.size = size;
						s.name = lookup.stralloc.push(name);
						s.module_index = module_index;
						s.si_tag = tag;

						if (get_lineinfo) {
							uint64_t lineinfo_base_addr;
							auto* lineinfo = find_single_overlapping_lineinfo(s, &lineinfo_base_addr);
							if (lineinfo) {
								int32_t symbol_offset = (int32_t)((int64_t)rva - (int64_t)lineinfo_base_addr);
								s.p_lineinfo = lineinfo::encode_c13_lineinfo(lineinfo, symbol_offset,
								[this, file_checksum_ptr] (u32 offset_in_file_checksums) -> StrAlloc::sid {
									auto* cksm = (codeview_file_checksum*)(file_checksum_ptr + offset_in_file_checksums);
									return extract_name(cksm->offset_in_string_table);
								}, lookup.binalloc, lookup.stats);
							}
						}

						*lookup.binalloc.get<Symbol>(bid) = s;

						int idx = (int)lookup.symbols.size();
						lookup.symbols.push_back(bid);

						extracted_symbol_addresses.emplace(rva, idx);
						return idx;
					}

					return -1;
				};

				switch (sym->kind) {
					//case S_SECTION: {
					//	auto* s = (SECTIONSYM*)sym;
					//	logf(">> S_SECTION: %s\n", s->name);
					//} break;
					//case S_COFFGROUP: {
					//	auto* s = (COFFGROUPSYM*)sym;
					//	logf(">> S_COFFGROUP: %s\n", s->name);
					//} break;
					//case S_EXPORT: {
					//	auto* s = (EXPORTSYM*)sym;
					//	logf(">> S_EXPORT: %s\n", s->name);
					//} break;

					// I think these are dllimport function thunks
					case S_THUNK32: {
						auto* s = (THUNKSYM32*)sym;
						//logf(">> S_THUNK32: %s\n", s->name);

						cur_symbol = push_symbol(s->off, s->len, s->seg, SymTagEnum::SymTagThunk, (const char*)s->name);
					} break;
					// These are tricky:
					// (Stupidly, I used onlyrelease mode executables in my testing until now so I never saw these)
					// In debug mode, all functions calls in your exe go through an indrection (a big table of jmp instructions called thunks, just like how dllimport functions work)
					// afaik this is to support things like edit and continue and incremental linking, so allowing patching function binaries with new versions without invalidating all the code that might be calling the old ones
					// my code was already handling the function body itself, but I was extremly confused when I took function pointers in my executable and tried to find the symbol only to get nothing in my own implementation
					// while dbghelp returned the function name (wrapped in ILT+<offset>(<func_name>))
					// dbghelp probably finds these on lookup, then does a second lookup to find the thunk target (which would be slow)
					// Instead I could add a fallback lookup whenever symbol lookup fails (might be tricky)
					// or I simply add special TRAMPOLINE symbols instead, TODO: see what other info dbghelp returns here (size? lineinfo?)
					case S_TRAMPOLINE: {
						auto* s = (TRAMPOLINESYM*)sym;
						//logf(">> S_TRAMPOLINE: %6llx\n", s->offThunk);
						
						trampolines.emplace_back(*s, module_index);
					} break;

					case S_GPROC32: case S_LPROC32:
					case S_GPROC32_ID: case S_LPROC32_ID: {
						auto* proc = (PROCSYM32*)sym;
						
						//uint64_t rva = proc->off + sections_sorted[proc->seg-1].base_addr;
						//logf(">> %s %4d|%8x => %4llx, %4x flags%4x %s\n", sym->kind == S_LPROC32 ? "S_LPROC32":"S_GPROC32", proc->seg, proc->off, rva, proc->len, proc->flags.bAll, proc->name);

						//uint32_t flags = 0;
						//if (proc->flags.CV_PFLAG_NEVER) flags |= SYMFLAG_FUNC_NO_RETURN;
						
						cur_symbol = push_symbol(proc->off, proc->len, proc->seg, SymTagEnum::SymTagFunction, (const char*)proc->name, true);
					} break;
					case S_INLINESITE: {
						auto* inl = (INLINESITESYM*)sym;

						// There is a weird test case I found where both dbghelp and my code return wrong results in a different way
						// In my case one function name in the inlinee stack is wrong, yet the source line is correct
						// I tried hard to find a bug in my code yet all seems to point to an inlinee id simply being wrong
						// (INLINESITE and InlineeSourceLine match correctly, but inlinee id in IPI points to wrong name, correct name is at different id)
						// This could suggest that I misunderstnad the way IDs work, yet all the other test cases work as expected
						// I am 99% sure that the inlinee stack I return is correct other than the single wrong function name
						// but the interesting bit is that dbghelp actually has the inlinee stack stop at this the level where I have the wrong function name
						// this likely means that it detects something is wrong, but I can't seem to figure out how it does that
						// so I have to assume a bug in LLVM, ideally I could also detect this error, but afaik there is no way of checking the correct callstack other than what I'm already doing
						
						//auto* parent = stack.empty() ? nullptr : stack.back();
						//stack.push_back(inl);

						auto* parent_entry = (codeview_symbol_header*)(sym_info + inl->pParent);
						auto* end_entry = (codeview_symbol_header*)(sym_info + inl->pEnd);
						assert(parent_entry->kind == S_INLINESITE
							|| parent_entry->kind == S_GPROC32
							|| parent_entry->kind == S_LPROC32
							|| parent_entry->kind == S_GPROC32_ID
							|| parent_entry->kind == S_LPROC32_ID
						);
						//if (parent_entry->kind == S_INLINESITE) {
						//	assert(parent && parent == (INLINESITESYM*)parent_entry);
						//}
						assert(end_entry->kind == S_INLINESITE_END);

						//inl->pParent // byte offs from symbol_information start of prev PROCSYM32 or INLINESITESYM, ie caller
						//inl->pEnd // byte offs of INLINESITE_END
						// inl->inlinee seems to be some kind of id that lets us look up line info, but not sure where that is and if that lineinfo is encoded horribly
						// no idea what inl->binaryAnnotations is
						
						//if (cur_symbol >= 0 && symbols[cur_symbol].base_addr == 8864) {
						//for (int i=0; i<inline_depth; i++) logf("  ");
						//logf(">> INLINESITE inlinee: [%4x] %s\n", inl->inlinee, stralloc[IPI_id2name.find(inl->inlinee)->second]);
						//}

						if (cur_symbol > 0) {
							//if (_inlinesites.size() <= inline_depth)
							//	_inlinesites.emplace_back();
							_inlinesites.push(inline_depth, inl);
						}

						inline_depth++;
					} break;
					case S_INLINESITE_END: {
						assert(inline_depth > 0);
						inline_depth--;
					} break;
					case S_END: {
						assert(inline_depth == 0);

						if (cur_symbol >= 0 && _inlinesites.written_depth > 0) {
							push_inline_tree(sym_info, file_checksum_ptr, cur_symbol);
						}
						_inlinesites.clear();

						cur_symbol = -1;
					} break;
				}
			}
			assert((ptr - sym_info) == mi->byte_size_of_symbol_information);
		}

		module_c13_lineinfo.clear();
		module_inlinee_c13.clear();
	}
	void process_trampolines () {
		if (trampolines.empty())
			return;

		// Don't know how to actually get start of "ILT" for Trampoline naming like "ILT+1555(<func name>)"
		// but first trampoline seems to be that start address
		auto& first_tramp = trampolines.front();
		uint64_t first_rva = first_tramp.sym.offThunk + sections_sorted[first_tramp.sym.sectThunk-1].base_addr;
		
		for (auto& tramp : trampolines) {
			//auto* mi = modules[tramp.module_index];

			uint64_t rva = tramp.sym.offThunk + sections_sorted[tramp.sym.sectThunk-1].base_addr;
			uint64_t target_rva = tramp.sym.offTarget + sections_sorted[tramp.sym.sectTarget-1].base_addr;

			if (extracted_symbol_addresses.find(rva) == extracted_symbol_addresses.end()) {
				auto bid = lookup.binalloc.push<Symbol>(Symbol());

				auto targ = extracted_symbol_addresses.find(target_rva);
				if (targ != extracted_symbol_addresses.end()) {
					assert(first_tramp.sym.sectThunk == tramp.sym.sectThunk);
					uint32_t offset_in_name = (uint32_t)(rva - first_rva);

					auto& targ_sym = lookup.get_sym(targ->second);

					char name_buf[MAX_SYM_NAME+1];
					int name_len = snprintf(name_buf, MAX_SYM_NAME+1, "ILT+%u(%s)", offset_in_name, lookup.stralloc[targ_sym.name]);
					
					auto name = lookup.stralloc.push(name_buf, name_len);

					//// Warning: has to be done in this order as stralloc pushes invalidate stralloc lookups
					//// TODO: this needs a way to avoid terminators to actually concatenate
					//         can't just combine all into one push_format, as it resizes in the middle, which invalidates targ_sym.name we pass in...
					//StrAlloc::sid name = lookup.stralloc.push_format("ILT+%u(", offset_in_name);
					//lookup.stralloc.push( lookup.stralloc[targ_sym.name] );
					//lookup.stralloc.push(")");

					auto* test = lookup.stralloc[name];

					Symbol s = {};
					s.base_addr = rva;
					s.size = tramp.sym.cbThunk;
					s.name = name;
					s.module_index = tramp.module_index;
					s.si_tag = SymTagEnum::SymTagPublicSymbol;

					*lookup.binalloc.get<Symbol>(bid) = s;

					int idx = (int)lookup.symbols.size();
					lookup.symbols.push_back(bid);

					extracted_symbol_addresses.emplace(rva, idx);
				}
				else {
					assert(false);
				}
			}
		}
	}
	
	void read_symbol_record_stream_before_modules () {
		ZoneScoped;

		auto* dbi = (dbi_stream_header*)DBI_data.data();
		auto symbol_record_stream_data = copy_stream_consecutive(dbi->stream_index_of_the_symbol_record_stream);
		char* ptr = symbol_record_stream_data.data();
		char* end = ptr + symbol_record_stream_data.size();
		
		auto push_symbol = [this] (SYM_ENUM_e kind, u32 offs, u32 size, u16 seg,
				SymTagEnum tag, const char* name, const char* _sym_type, u32 _flags) [[msvc::forceinline]] {
			//logf("%s: %4d|%8x, %4x %s\n", _sym_type, seg, offs, _flags, name);
			
			uint64_t rva;
			if (!resolve_rva(offs, seg, &rva))
				return;
			
			if (extracted_symbol_addresses.find(rva) == extracted_symbol_addresses.end()) {
				Symbol s = {};
				s.base_addr = rva;
				s.size = size;
				s.name = trim_mangled_name(name);
				s.si_tag = tag;
				
				auto bid = lookup.binalloc.push(s);

				int idx = (int)lookup.symbols.size();
				lookup.symbols.push_back(bid);

				extracted_symbol_addresses.emplace(rva, idx);
			}
		};
		
		while (ptr < end) {
			auto sym = (codeview_symbol_header*)ptr;
			
			ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
			ptr = align_up(ptr, 4);

			switch (sym->kind) {
				case S_LDATA32: case S_GDATA32: case S_LMANDATA: case S_GMANDATA: {
					auto* s = (DATASYM32*)sym;
					
					uint64_t _rva = 0;
					resolve_rva(s->off, s->seg, &_rva);

					//if (_rva == 0x31f0) {
					//	printf("");
					//}

					auto size = 0;
					// HACK: __ImageBase seems to come from this entry, and none of these have sizes
					// yet dbghelp returns size=0x40 for __ImageBase, which matches sizeof(IMAGE_DOS_HEADER), so maybe this is correct
					// There doesn't seem to be any other way of querying symbol sizes, sections contributions don't work as __ImageBase is not in a section
					//auto size = s->seg == 0 && s->off == 0 ? (uint32_t)sizeof(IMAGE_DOS_HEADER) : (uint32_t)1;
					//
					//int computed_size = -1;
					//uint64_t rva;
					//if (resolve_rva(s->off, s->seg, &rva))
					//	computed_size = _sym_size_from_typind(rva, s->typind);
					//if (computed_size >= 0)
					//	size = (uint32_t)computed_size;

					push_symbol(
						sym->kind,
						s->off,
						size,
						s->seg,
						SymTagEnum::SymTagData,
						(const char*)s->name,
						"DATASYM32",
						0
					);
				} break;
			}
		}
		assert(ptr == end);
	}
	void read_symbol_record_stream () {
		ZoneScoped;

		auto* dbi = (dbi_stream_header*)DBI_data.data();
		auto symbol_record_stream_data = copy_stream_consecutive(dbi->stream_index_of_the_symbol_record_stream);
		char* ptr = symbol_record_stream_data.data();
		char* end = ptr + symbol_record_stream_data.size();
		
		auto push_symbol = [this] (SYM_ENUM_e kind, u32 offs, u32 size, u16 seg,
				SymTagEnum tag, const char* name, const char* _sym_type, u32 _flags) [[msvc::forceinline]] {
			//logf("%s: %4d|%8x, %4x %s\n", _sym_type, seg, offs, _flags, name);
			
			uint64_t rva;
			if (!resolve_rva(offs, seg, &rva))
				return;
			
			//if (rva == 0x56f0) {
			//	printf("");
			//}

			if (extracted_symbol_addresses.find(rva) == extracted_symbol_addresses.end()) {
				
				Symbol s = {};
				s.base_addr = rva;
				s.size = size;
				s.name = trim_mangled_name(name);
				s.si_tag = tag;
				
				auto bid = lookup.binalloc.push(s);

				int idx = (int)lookup.symbols.size();
				lookup.symbols.push_back(bid);

				extracted_symbol_addresses.emplace(rva, idx);
			}
		};
		
		while (ptr < end) {
			auto sym = (codeview_symbol_header*)ptr;
			
			ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
			ptr = align_up(ptr, 4);

			switch (sym->kind) {
				//case S_PROCREF: case S_DATAREF: case S_LPROCREF: {
				//	auto* s = (REFSYM2*)sym;
				//	//logf("REFSYM2: %s\n", s->name);
				//} break;
				//case S_CONSTANT: case S_MANCONSTANT: { // mostly works, but weirdness with the name? maybe using 1 for zero length array is wrong
				//	auto* s = (CONSTSYM*)sym;
				//	//logf("CONSTSYM: %s\n", s->name);
				//} break;
				//case S_UDT: case S_COBOLUDT: {
				//	auto* s = (UDTSYM*)sym;
				//	//logf("UDTSYM: %s\n", s->name);
				//} break;
				/*
				case S_LDATA32: case S_GDATA32: case S_LMANDATA: case S_GMANDATA: {
					auto* s = (DATASYM32*)sym;
					
					uint64_t _rva = 0;
					resolve_rva(s->off, s->seg, &_rva);

					//if (_rva == 0x31f0) {
					//	printf("");
					//}

					auto size = 0;
					// HACK: __ImageBase seems to come from this entry, and none of these have sizes
					// yet dbghelp returns size=0x40 for __ImageBase, which matches sizeof(IMAGE_DOS_HEADER), so maybe this is correct
					// There doesn't seem to be any other way of querying symbol sizes, sections contributions don't work as __ImageBase is not in a section
					//auto size = s->seg == 0 && s->off == 0 ? (uint32_t)sizeof(IMAGE_DOS_HEADER) : (uint32_t)1;
					//
					//int computed_size = -1;
					//uint64_t rva;
					//if (resolve_rva(s->off, s->seg, &rva))
					//	computed_size = _sym_size_from_typind(rva, s->typind);
					//if (computed_size >= 0)
					//	size = (uint32_t)computed_size;

					push_symbol(
						sym->kind,
						s->off,
						size,
						s->seg,
						0,
						SymTagEnum::SymTagData,
						(const char*)s->name,
						"DATASYM32",
						0
					);
				} break;
				*/
				case S_PUB32: {
					auto* s = (PUBSYM32*)sym;

					//if (s->pubsymflags.fFunction == false) {
					//	// TODO: despite there being no size stored in PUBSYM32, and also no typind which can be used to compute the size
					//	// dbghelp still returns a non-zero size for ?_OptionsStorage@?1??__local_stdio_printf_options@@9@4_KA
					//	// which may be because the 4_KA encodes the "unsigned __int64" in unsigned __int64 `__local_stdio_printf_options'::`2'::_OptionsStorage
					//	// Indeed this is a data symbol which for some reason exists as S_PUB32 but there is no other entry (no S_LDATA32 etc.) at that address
					//}

					push_symbol(
						sym->kind,
						s->off,
						0,
						s->seg,
						//s->pubsymflags.fFunction ? SymTagEnum::SymTagPublicSymbol : SymTagEnum::SymTagData, // No idea if this is right
						SymTagEnum::SymTagPublicSymbol,
						(const char*)s->name,
						"PUBSYM32",
						(u32)s->pubsymflags.grfFlags
					);
				} break;
				//default: {
				//	//logf("?: %x\n", sym->kind);
				//}
			}
		}
		assert(ptr == end);
	}
	
	void read_TPI_stream () {
		ZoneScoped;

		TPI_data = copy_stream_consecutive(2);
		char* ptr = TPI_data.data();

		auto* header = (tpi_stream_header*)ptr;
		ptr += sizeof(tpi_stream_header);
		
		assert(header->version == 20040203);

		u32 count = header->one_past_last_type_index - header->minimal_type_index;
		u32 id = header->minimal_type_index;
		
		auto push_type = [this] (u32 id, unsigned char* data) -> const char* [[msvc::forceinline]] {
			ulong size = 0;
			size_t dcb = CbExtractNumeric(data, &size);
			auto* name = (const char *)data + dcb;

			assert(typeid2name.find(id) == typeid2name.end());
			typeid2name.emplace(id, name);

			return name;
		};

		char* end = ptr + header->byte_count_of_type_record_data_following_the_header;
		while (ptr < end) {
			auto* lf = (codeview_type_record_header*)ptr;
			ptr += sizeof(u16) + lf->length; // length field of codeview_type_record_header not contained in length (but kind is)
			
			assert(id < header->one_past_last_type_index);
			
			switch (lf->kind) {
				case LF_STRUCTURE:
				case LF_CLASS: {
					auto* struc = (lfClass*)lf;
					auto* name = push_type(id, struc->data);
					//logf(">> lfClass: [%4x]: %s %x\n", id, name, struc->field);
				} break;
				case LF_UNION: {
					auto* struc = (lfUnion*)lf;
					auto* name = push_type(id, struc->data);
				} break;
				//case LF_PROCEDURE: {
				//	auto* proc = (lfProc*)lf;
				//	logf("Proc: id: [%4x]\n", id);
				//} break;
			}

			id++;
		}
		assert(ptr == end);
	}
	void read_IPI_stream () {
		ZoneScoped;

		auto IPI_data = copy_stream_consecutive(4);
		char* ptr = IPI_data.data();

		auto* header = (tpi_stream_header*)ptr;
		ptr += sizeof(tpi_stream_header);

		assert(header->version == 20040203);

		u32 count = header->one_past_last_type_index - header->minimal_type_index;

		u32 id = header->minimal_type_index;
		char* type_info = ptr;
		char* type_info_end = type_info + header->byte_count_of_type_record_data_following_the_header;
		
		struct IPI_STRING_IDs {
			// LF_STRING_ID is what lfFuncId.scopeId points to
			// do a pass first
			ankerl::unordered_dense::map<u32, codeview_type_record_header*> map;

			void recurse_append_scope (StrAlloc* stralloc, CV_ItemId id) const {
				auto it = map.find(id);
				if (it == map.end()) {
					assert(false);
					return; // don't append
				}

				auto* lf = it->second;
				if (lf->kind == LF_STRING_ID) {
					auto* si = (lfStringId*)lf;
					// lfStringId -> lfStringId.name  or
					// lfStringId (Composite) -> string for lfStringId.id + lfStringId.name

					if (si->id != 0) {
						recurse_append_scope(stralloc, si->id);
					}

					stralloc->push_concat_scope_no_terminate((const char*)si->name);
				}
				else {
					assert(lf->kind == LF_SUBSTR_LIST);
					auto* si = (lfArgList*)lf;
					
					// lfArgList -> string for lfArgList.arg[0] + string for lfArgList.arg[1] + ...
					for (u32 i=0; i<si->count; i++) {
						recurse_append_scope(stralloc, si->arg[i]);
					}
				}
			}

			StrAlloc::sid push_scope_prefix (StrAlloc* stralloc, CV_ItemId id) const {
				auto offset = stralloc->get_offset();

				recurse_append_scope(stralloc, id);

				return offset;
			}
		};
		IPI_STRING_IDs strids;
		strids.map.reserve(32);
		
		while (ptr < type_info_end) {
			auto* lf = (codeview_type_record_header*)ptr;
			ptr += sizeof(u16) + lf->length; // length field of codeview_type_record_header not contained in length (but kind is)
			//assert((sizeof(u16) + lf->length)%4 == 0);
			assert(id < header->one_past_last_type_index);

			/*
			// LF_STRING_ID sometimes hold "id", which points to an earlier LF_SUBSTR_LIST, which holds a VLA of other LF_STRING_IDs
			// in this case LF_STRING_ID seems to essentially represent the concatenated string lfStringId.name + LF_SUBSTR_LIST at lfStringId.id
			// while LF_SUBSTR_LIST itself holds the concatenation of what each of its IDs point towards
			// this appears to be used for strings past a certain length, which has to be chopped up for some reason
			// I have no idea why, maybe because the pdb format is really old and hardware was limited
			// I could not properly test the above, but lets see if it passes all my test executables vs dbghelp.dll later
			// TODO: this should be rewritten to use a not build up a unordered_map, but instead provide a function that builds a queried LF_STRING_ID recursively to avoid processing and allocation
			switch (lf->kind) {
				case LF_SUBSTR_LIST: {
					auto* str = (lfArgList*)lf;
					//logf(">> lfArgList: [%4x]:\n", id);

					auto s = std::string();
					for (u32 i=0; i<str->count; i++) {
						s += strids.map[str->arg[i]];
						//logf(">>> [%4x] \"%s\"\n", str->arg[i], stringids[str->arg[i]].c_str());
					}
					
					stringids.emplace(id, std::move(s));
				} break;
				case LF_STRING_ID: {
					auto* str = (lfStringId*)lf;
					if (str->id == 0) {
						//logf(">> lfStringId: [%4x] \"%s\"\n", id, str->name);
						stringids.emplace(id, std::string((const char*)str->name));
					}
					else {
						auto* other = try_get(stringids, str->id);
						assert(other);
						if (other) {
							auto s = *other + (const char*)str->name;
							
							//logf(">> lfStringId (Composite): [%4x] id=%4x => \"%s\"\n", id, str->id, s.c_str());
							stringids.emplace(id, std::move(s));
						}
					}

				} break;
			}
			*/

			switch (lf->kind) {
				case LF_SUBSTR_LIST:
				case LF_STRING_ID: {
					strids.map.emplace(id, lf);
				} break;
			}

			id++;
		}
		assert(ptr == type_info_end);
		
		id = header->minimal_type_index;
		ptr = type_info;
		while (ptr < type_info_end) {
			auto* lf = (codeview_type_record_header*)ptr;
			ptr += sizeof(u16) + lf->length; // length field of codeview_type_record_header not contained in length (but kind is)
			
			assert(id < header->one_past_last_type_index);

			switch (lf->kind) {
				case LF_FUNC_ID: {
					// free function
					auto* func = (lfFuncId*)lf;
					//logf(">> lfFuncId: [%4x]=%s\n", id, func->name);

					if (func->scopeId != 0) {
						auto formatted_strid = strids.push_scope_prefix(&lookup.stralloc, func->scopeId); // pushes "nested::scope::"
						lookup.stralloc.push((const char*)func->name); // pushes final name with null terminator

						assert(IPI_id2name.find(id) == IPI_id2name.end());
						IPI_id2name.emplace(id, formatted_strid);
					}
					else {
						assert(IPI_id2name.find(id) == IPI_id2name.end());
						IPI_id2name.emplace(id, lookup.stralloc.push((const char*)func->name));
					}

					//if (strcmp((const char*)func->name, "from_axis_angle")==0) {
					//	logf(""); // 24649
					//}
				} break;
				case LF_MFUNC_ID: {
					// member function (just the function name, struct name missing!)
					auto* func = (lfMFuncId*)lf;
					auto* parent_name = try_get(typeid2name, func->parentType);
					assert(parent_name);
					if (parent_name) {
						//logf(">> lfMFuncId: [%4x]=%s::%s\n", id, parent_name->c_str(), (const char*)func->name);
						auto formatted_strid = lookup.stralloc.push_concat(*parent_name, "::", (const char*)func->name);
						
						assert(IPI_id2name.find(id) == IPI_id2name.end());
						IPI_id2name.emplace(id, formatted_strid);
					}
					
					//if (strcmp((const char*)func->name, "from_axis_angle")==0) {
					//	logf("");
					//}
				} break;
			}

			id++;
		}
		assert(ptr == type_info_end);
	}
	
	// Symbols as encountered in the symbol record stream and in each of the module streams are not sorted by address
	// I will need to sort symbols for the address to symbol lookup however
	// I want the custom extracted symbol data + lineinfo + inline lineinfo data to be consecutive in memory to achieve best performance on lookup
	// There are two choices: keep this entire piece of memory sorted by symbol address or
	// keep it in the originally encountered order and have the address lookup at the start be a random access
	// to achieve the sorted version, I'd need to either:
	// find symbols first, then sort them, then extract all their data out of order
	//  this is complicated as I need to store data temporarily per symbol or per module, which is complex
	// or extract data first, then somehow copy it into the correct order later
	//  but due to how my data structures work, this may be harder or error prone than it appears, eg. a memcpy might violate alignment
	// So instead I simply choose to keep symbols out of order and order just their indices instead

	void finalize_symbols () {
		ZoneScoped;
		
		typedef BinAlloc::bid Id;
		auto _cmp = [&] (Id l, Id r) -> int {
			return std::less<uint64_t>()(lookup.binalloc.get_unchecked<Symbol>(l)->get_addr(), lookup.binalloc.get_unchecked<Symbol>(r)->get_addr());
		};
		//auto _less = [&] (Id l, Id r) -> bool {
		//	return binalloc.get<Symbol>(l)->get_addr() < binalloc.get<Symbol>(r)->get_addr();
		//};

		// sort based on base_addr
		// use stable sorts as symbol can and will overlap, so preserve insertion order
		std::stable_sort(lookup.symbols.begin(), lookup.symbols.end(), _cmp);

		lookup.symbol_index.build_index((uint32_t)lookup.symbols.size(), [&] (uint32_t idx) {
			return lookup.get_sym(idx).get_addr();
		});
	}

	void _reserve () {
		named_streams.reserve(32);
		sections_sorted.reserve(32);

		typeid2name.reserve(128);
		IPI_id2name.reserve(128);

		extracted_symbol_addresses.reserve(4096);
		trampolines.reserve(128);
		
		lookup.symbols.reserve(1024);

		_inlinesites.reserve();
	}

	
	static BOOL PsymEnumeratesymbolsCallback (PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext) {
		logf(">>> [%llx] %4d %4d %s\n", pSymInfo->Address, pSymInfo->Index, pSymInfo->Tag, pSymInfo->Name);
		return TRUE;
	}
	void _dbghelp_get_sym (uint64_t rva) {
		auto address = (DWORD64)(_mod_base + rva);

		auto res1 = real_dbghelp.SymSearch(_hprocess, _mod_base, 0, 0, NULL, address, PsymEnumeratesymbolsCallback, NULL, SYMSEARCH_ALLITEMS);
		
		SYMBOL_INFO_PACKAGE buf;
		buf.si = {};
		buf.si.SizeOfStruct = sizeof(buf.si);
		buf.si.MaxNameLen = MAX_SYM_NAME;

		auto res2 = real_dbghelp.SymFromAddr(_hprocess, address, nullptr, &buf.si);
	}
	int _sym_size_from_typind (uint64_t rva, CV_typ_t typind) {
		SYMBOL_INFO_PACKAGE buf;
		buf.si = {};
		buf.si.SizeOfStruct = sizeof(buf.si);
		buf.si.MaxNameLen = MAX_SYM_NAME;

		DWORD Displacement = 0;

		auto res1 = real_dbghelp.SymFromAddr(_hprocess, (DWORD64)(_mod_base + rva), nullptr, &buf.si);
		
		ULONG64 len = -1;
		auto res2 = real_dbghelp.SymGetTypeInfo(_hprocess, _mod_base, buf.si.TypeIndex, TI_GET_LENGTH, &len);

		int dbghelp_size = len != -1 ? (int)len : -1;
		int my_size = -1;
		
		char* ptr = TPI_data.data();

		auto* header = (tpi_stream_header*)ptr;
		ptr += sizeof(tpi_stream_header);
		
		assert(header->version == 20040203);

		if (typind >= header->minimal_type_index) {
			u32 count = header->one_past_last_type_index - header->minimal_type_index;
			u32 id = header->minimal_type_index;
		
			char* end = ptr + header->byte_count_of_type_record_data_following_the_header;
			while (ptr < end) {
				auto* lf = (codeview_type_record_header*)ptr;
				ptr += sizeof(u16) + lf->length; // length field of codeview_type_record_header not contained in length (but kind is)
			
				assert(id < header->one_past_last_type_index);
				if (id == typind) {

					switch (lf->kind) {
						case LF_STRUCTURE:
						case LF_CLASS: {
							auto* struc = (lfClass*)lf;
							//auto* name = push_type(id, struc->data);

							ulong size = 0;
							size_t dcb = CbExtractNumeric(struc->data, &size);
							auto* name = (const char *)struc->data + dcb;

							//logf(">> lfClass: [%4x]: %s %x\n", id, name, struc->field);

							my_size = (int)size;
						} break;
						case LF_UNION: {
							auto* struc = (lfUnion*)lf;
							//auto* name = push_type(id, struc->data);
						
							ulong size = 0;
							size_t dcb = CbExtractNumeric(struc->data, &size);
							auto* name = (const char *)struc->data + dcb;

							my_size = (int)size;
						} break;

						case LF_POINTER: {
							my_size = 8; // ??
						} break;
					}
					if (my_size >= 0)
						break;
				}

				id++;
			}
			//assert(my_size >= 0);
			//assert(my_size == dbghelp_size);
		}

		return dbghelp_size;
	}

//// Final needed data
	FastPdbLookup lookup;

public:

	static std::unique_ptr<PdbReader> pdb_for_exe (std::filesystem::path const& exe_path) {
		PDB_Locator locator(exe_path);
		auto path = locator.get_pdb_path();
		auto rsds = locator.get_rsds();
		return std::make_unique<PdbReader>(std::move(path), rsds);
	}

	PdbReader (std::filesystem::path&& pdb_path, PDB_Locator::PDB_guid_and_age const& rsds) {
		ZoneScopedN("parse_pdb");

		if (!file.open(pdb_path)) {
			throw std::runtime_error("File not found: "+ pdb_path.string());
		}

		lookup.pdb_path = std::move(pdb_path);

		lookup.binalloc.init();
		lookup.stralloc.init();

		_reserve();
		
		// check PDB header signature
		read_header();
		// read STS, which contains list of pdb streams and list of page ids for these streams
		read_stream_table();
		// read pdb_information_stream, specifically the named streams hashmap
		// some streams are fixed index, some are referenced via index from this file
		// but some are indexed using a string->index lookup instead
		read_pdb_info(rsds);
		// copy '/names' string table for later use
		read_names();
		// read type information stream, extract names of class, struct, union (these seem to be already formatted as scope::Class)
		read_TPI_stream();
		// read IPI (not sure what is stands for, something with IDs)
		// assemble and extract fully scoped names for functions and member functions, referencing the TPI
		read_IPI_stream();
		
		// read DBI, which contains list of all modules
		// and "section contributions", which can be used for address -> module -> symbol lookups, but I am not using as I fully extract symbols from each module anyway
		// also extracts section dump stream id
		read_DBI();

		// read list of all sections, needed to map section-relative offsets to rva
		if (opt_streams.stream_index_of_section_header_dump == 0xFFFF)
			throw std::runtime_error("Section dump not found!");

		// Note: I leaned about OMAP long after implementing the pdb parsing code, and was worried this was causing some of the mismatches vs dbghelp
		// but none of my example executables have OMAP data, perhaps this only exists in some rare case, perhaps with profile guided optimization?
		if (opt_streams.stream_index_of_omap_from_src_data != 0xFFFF ||
			opt_streams.stream_index_of_omap_to_src_data != 0xFFFF)
			throw std::runtime_error("PDB contains OMAP data, which invalidates some addresses, this is currently not implemented!");

		read_section_header_dump();
		
		// read data symbols first as it appears that dbghelp prefers returing data symbols rather than module symbols
		// yet prefers module symbols over module_symbol_stream PUB32 symbols
		read_symbol_record_stream_before_modules();

		// read all module symbols streams via DBI, which contain function symbol data and all lineinfo
		read_module_symbol_streams();
		
		// read symbol record stream, which contains certain global symbols, including functions
		// In practice any function symbol with code should appear in the modules with more information instead, functions will be appear duplicated here
		// but dbghelp will return symbols from here, right now I return symbols from the modules first and use this as a fallback only
		read_symbol_record_stream();

		finalize_symbols();

		//print_dump_names();
		//stralloc.print_dump();

		//lookup.print_symbols();
		//_dbghelp_get_sym(0x1B370);

		//logf("PDB read.\n");
	}
	
	static std::unique_ptr<FastPdbLookup> try_load_lookup_for_exe (std::filesystem::path const& exe_path) {
		try {
			auto reader = PdbReader::pdb_for_exe(exe_path);
			return std::make_unique<FastPdbLookup>( std::move(reader->lookup) );
		} catch (std::exception& ex) {
			logf("!!! PDB loading exception: %s\n", ex.what());
		}
		return nullptr;
	}
};
