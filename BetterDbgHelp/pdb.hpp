#pragma once
#include "util.hpp"
#include "codeview.hpp"
#include "pdb_locator.hpp"
#include "address_index.hpp"
#include "lineinfo.hpp"
#include <map>

#define TRACK_ALL_SYMBOLS 0

// https://github.com/PascalBeyer/PDB-Documentation

struct Symbol {
	uintptr_t base_addr = 0; // relative to module
	uint32_t size = 0;
	StrAlloc::sid name;
	
	s16 module_index = -1;
	uint8_t inline_depth = 0;

	uintptr_t lineinfo_base_addr;
	BinAlloc::bid p_lineinfo = -1;
	BinAlloc::bid p_inlinesites = -1;

	__forceinline uintptr_t get_addr () const {
		return base_addr;
	}
	static __forceinline Symbol dummy (uintptr_t base_addr) { // needed for std::upper_bound
		return Symbol { base_addr };
	}
};

struct Module {
	pdb_module_information* mi;
	std::string_view name;
	std::string_view file_name;

	std::vector<char> symbol_stream_data;
	char* sym_info;
		
	// It seems like we can get duplicate entries of the same inlinee id across different modules via InlineeSourceLine
	// So in every module with a INLINESITE, the corresponding InlineeSourceLine and filename in DEBUG_S_FILECHKSMS exist in the same module
	// but the same inlinee file id can exist in multiple modules
	// this could be because due to link time optimization (the same function being inlined into different translation lines)
	// but actually, sometimes the filepath differs (same header, paths, compiled on different machines?)
	// so functions compiled from different copies of a header can get the same id
	char* file_checksum_ptr = nullptr;
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
	CV_off32_t fileId;
	CV_off32_t sourceLineNum;
	BinAlloc::bid pSibling = -1;
	BinAlloc::bid pChildren = -1;
	// followed by Lineinfo
	
	void* get_lineinfo () {
		return this+1;
	}

	Inlinesite () {};
};

// TODO: eventually extract pdb parsing code and have it output symbol resolver with all custom data structure
class PDB_File {
	std::filesystem::path path;

	MemoryMappedFile file;
	
	void* get_page (u32 idx) {
		return (char*)file.data() + idx * header->page_size;
	}
	u32 ceil_div (u32 a, u32 b) {
		return (a + (b-1)) / b;
	}
	char* align_up (char* ptr, u32 align) {
		uintptr_t x = (uintptr_t)ptr;
		return (char*)((x + align-1) / align * align);
	}

	void* read_sts (u32 ptr) {
		u32 page_idx    = ptr / header->page_size;
		u32 ptr_in_page = ptr % header->page_size;
		
		//u32 sts_num_pages = ceil_div(header->stream_table_stream_size, header->page_size);

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
	
	
	msf_header* header;

	struct Stream {
		u32 size;
		std::vector<u32> pages;
	};
	std::vector<Stream> streams;
	
	std::vector<char> pdb_info_data;
	std::vector<char> names_data;
	std::vector<char> TPI_data;
	std::vector<char> IPI_data;
	std::vector<char> DBI_data;
	std::vector<char> section_header_dump_data;
	std::vector<char> symbol_record_stream_data;

	//pdb_information_stream_header* info;

	std::unordered_map<std::string_view, u32> named_streams;

	// contains function names for source line info (both normal and inline stack)
	const char* names = nullptr;

	optional_debug_header_substream* opt_streams;

	// Struct, Class and Union names from TPI, these are already fully formatted
	ankerl::unordered_dense::map<CV_typ_t, const char*> typeid2name;
	// Func and MemberFunc names from IPI (IDs will overlap with typeid2name!)
	ankerl::unordered_dense::map<CV_ItemId, StrAlloc::sid> IPI_id2name;

	//// Final needed data
	struct Section {
		std::string name;

		uintptr_t base_addr;
		size_t size;
	};
	std::vector<Section> sections_sorted;

public:
	BinAlloc binalloc;
	StrAlloc stralloc;
	
	// TODO: huge memory use with rust_bevy
	struct Stats {
		int num_symbols;
		int num_symbol_lookup;
		int num_lineinfos;
		int num_inlinesites;
		int num_strings;
	};
	void print_stats () {
		logf("@ PDB %s:\n", path.string().c_str());
		symbols.print_stats("Symbols");

		logf("binalloc: size: %.1f kB\n", binalloc.buf.size()/1000.0f);
		logf("stralloc: size: %.1f kB\n", stralloc.buf.size()/1000.0f);
	}
private:

	std::vector<Module> modules;

	AddressIndex<Symbol> symbols;

#if TRACK_ALL_SYMBOLS
	AddressIndex<Symbol> sym_unfiltered; // to support has_symbol_for_addr

	void print_all_symbols () {
		for (auto& s : sym_unfiltered) {
			logf(">> %4llx %4x mod=%d %s\n", s.base_addr, s.size, s.module_index, stralloc[s.name]);
		}
	}
#endif

	void* read_stream (u32 stream, u32 ptr) {
		u32 page_idx    = ptr / header->page_size;
		u32 ptr_in_page = ptr % header->page_size;

		assert(stream < streams.size() && ptr < streams[stream].size);
		return (char*)get_page(streams[stream].pages[page_idx]) + ptr_in_page;
	}
	std::vector<char> copy_into_consecutive (u32 streami) {
		std::vector<char> data;

		auto& stream = streams[streami];
		data.resize(stream.size);

		char* cur = data.data();
		size_t remain = stream.size;
		for (u32 pg : stream.pages) {
			memcpy(cur, get_page(pg), (u32)std::min((size_t)header->page_size, remain));
			remain -= header->page_size;
			cur += header->page_size;
		}

		return data;
	}
	
	void read_header () {
		header = (msf_header*)file.data();
		assert(strncmp((const char*)header->signature, "Microsoft C/C++ MSF 7.00\r\n\032DS\0\0\0", 32) == 0);
	}
	
	void read_stream_table () {

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
		
			u32 num_pages = ceil_div(stream.size, header->page_size);
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
		
		pdb_info_data = copy_into_consecutive(1);
		char* ptr = pdb_info_data.data();

		auto* info = (pdb_information_stream_header*)ptr;
		ptr += sizeof(pdb_information_stream_header);

		if (!PDB_Locator::verify_pdb(rsds, info))
			throw std::runtime_error("PDB loaded but GUID or age mismatch for "+ path.string()
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

		named_streams.reserve(32);
		
		//logf("Named Streams:\n");
		for(u32 index = 0, entry_index = 0; index < capacity && entry_index < amount_of_entries; index++){
			u32 word_index = index / (sizeof(u32) * 8);
			u32 bit_index  = index % (sizeof(u32) * 8);
			
			if(word_index < present_word_count && (present_bits[word_index] & (1u << bit_index))){
				auto& kv = entries[entry_index++];

				//std::string key = std::string(&string_buffer[kv.key]);
				//logf("> %s: %d\n", key.c_str(), kv.value);
				//named_streams[std::move(key)] = kv.value;
				std::string key = std::string(&string_buffer[kv.key]);
				//logf("> %s: %d\n", &string_buffer[kv.key], kv.value);
				named_streams[std::string_view(&string_buffer[kv.key])] = kv.value;
				continue;
			}
		}
	}

	void read_names () {
		names_data = copy_into_consecutive(named_streams["/names"]);
		char* ptr = names_data.data();
		
		u32 signature = *(u32*)ptr;
		ptr += sizeof(u32);
		if (signature == 0xEFFEEFFE) {
		
			u32 hash_version = *(u32*)ptr;
			ptr += sizeof(u32);
		
			u32 string_buffer_size = *(u32*)ptr;
			ptr += sizeof(u32);
			
			names = ptr;
			ptr += string_buffer_size;
			
			// There is somd kind of hashmap in here, is this is a hashmap from string -> type/symbol id?
			// Presumably this is not useful to me in this project
			u32 bucket_count = *(u32*)ptr;
			ptr += sizeof(u32);

			u32* buckets = (u32*)ptr;
			ptr += bucket_count * sizeof(u32);

			u32* amount_of_strings = (u32*)ptr;
			ptr += sizeof(u32);
		}
		else {
			// signature == 0xFE
			// this happens with pdbs downloaded from MS symbol servers, possibly because they are stripped
			// Simply leave names as null works, as these files also don't contain references to it
		}
	}
	
	void read_DBI () {
		DBI_data = copy_into_consecutive(3);
		char* ptr = DBI_data.data();

		auto* header = (dbi_stream_header*)ptr;
		ptr += sizeof(dbi_stream_header);
		
		//// module_information_substream
		auto* ptr2 = ptr;

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

			modules.push_back(m);
		}
		assert((ptr - ptr2) == header->byte_size_of_the_module_information_substream);
		ptr = ptr2 + header->byte_size_of_the_module_information_substream;
		
		//// section_contribution_substream
		ptr2 = ptr;

		u32 DBISCImpv = *(u32*)ptr;
		ptr += sizeof(u32);

		assert(DBISCImpv == (0xeffe0000 + 19970605));
		
		u32 num_section_contributions = header->byte_size_of_the_section_contribution_substream / sizeof(pdb_section_contribution);
		auto* section_contributions = (pdb_section_contribution*)ptr;

		// Section contributions seem just be a list of all the symbol address ranges merged, meaning mainly functions
		// For my use case it seems pointless to search section contributions, as finding the entry only tells you the module
		// which you then have to search for the actual symbol, I don't know how dbghelp implemented this, as the symbols in the module appear unsorted and even have duplicates
		// maybe there is a hashmap somehwere where the start address of the SC can be searched, but I can instead simply build a sorted list of all symbols instead
		// Except that in rare cases where functions with identical code get merged, dbghelp returns a seemingly random one, and I can't seem to ever return the same one as it
		// The roundabout way through section contributions does not help afaik
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
		ptr += sizeof(pdb_section_contribution) * num_section_contributions;
		assert((ptr - ptr2) == header->byte_size_of_the_section_contribution_substream);
		
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
		opt_streams = (optional_debug_header_substream*)ptr;
		
		//byte_size_of_the_optional_debug_header_substream
	}

	void read_section_header_dump () {
		section_header_dump_data = copy_into_consecutive(opt_streams->stream_index_of_section_header_dump);
		assert(section_header_dump_data.size() == streams[opt_streams->stream_index_of_section_header_dump].size);

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
				return stralloc.push(begin, end - begin);
			}
		}
		return stralloc.push(name);
	}
	
	[[msvc::forceinline]] bool resolve_rva (u32 offs, u16 seg, uintptr_t* out_addr) {
		if (seg > 0) {
			// Sometimes global symbols have section ids to invalid sections, no idea why this happens
			// ex: __guard_fids_table, __guard_flags, __guard_iat_table, __guard_longjmp_table, __enclave_config, __guard_eh_cont_table
			if (seg > sections_sorted.size()) {
				return false;
			}
			auto seg_addr = sections_sorted[seg-1].base_addr;
			*out_addr = (uintptr_t)offs + (uintptr_t)seg_addr;
			assert(*out_addr < seg_addr + sections_sorted[seg-1].size);
		}
		else {
			// Special __ImageBase symbol has seg==0, so offs already is rva
			// I have not observed any other cases
			assert(offs == 0);
			*out_addr = (uintptr_t)offs;
		}
		return true;
	}
	void read_symbol_record_stream () {
		auto* dbi = (dbi_stream_header*)DBI_data.data();
		symbol_record_stream_data = copy_into_consecutive(dbi->stream_index_of_the_symbol_record_stream);
		char* ptr = symbol_record_stream_data.data();

		char* ptr2 = ptr;
		
		auto push_symbol = [this] (u32 offs, u32 size, u16 seg, const char* name, const char* _sym_type) [[msvc::forceinline]] {
			//if (strstr(name, "__scrt_ucrt_dll_is_in_use")) {
			//	printf("");
			//}

			uintptr_t addr;
			if (!resolve_rva(offs, seg, &addr))
				return;

			auto trimmed_name = trim_mangled_name(name);
			symbols.push_unsorted(Symbol{ addr, size, trimmed_name });
		#if TRACK_ALL_SYMBOLS
			sym_unfiltered.push_unsorted(Symbol{ addr, size, trimmed_name });
		#endif

			//logf("%s: %4d|%8x => %4llx, %s\n", _sym_type, seg, offs, addr, stralloc[trimmed_name]);
		};
		
		while (ptr < ptr2 + symbol_record_stream_data.size()) {
			auto sym = (codeview_symbol_header*)ptr;
			
			ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
			ptr = align_up(ptr, 4);

			switch (sym->kind) {
				case S_PROCREF: case S_DATAREF: case S_LPROCREF: {
					auto* s = (REFSYM2*)sym;
					//logf("REFSYM2: %s\n", s->name);
				} break;
				case S_CONSTANT: case S_MANCONSTANT: { // mostly works, but weirdness with the name? maybe using 1 for zero length array is wrong
					auto* s = (CONSTSYM*)sym;
					//logf("CONSTSYM: %s\n", s->name);
				} break;
				case S_UDT: case S_COBOLUDT: {
					auto* s = (UDTSYM*)sym;
					//logf("UDTSYM: %s\n", s->name);
				} break;
				case S_LDATA32: case S_GDATA32: case S_LMANDATA: case S_GMANDATA: {
					auto* s = (DATASYM32*)sym;
					push_symbol(
						s->off,
						0, // TODO: these symbols don't have a size, possibly becasue the size is implicit based on the data type?,
						s->seg,
						(const char*)s->name,
						"DATASYM32"
					);
				} break;
				case S_PUB32: {
					auto* s = (PUBSYM32*)sym;
					push_symbol(
						s->off,
						0,
						s->seg,
						(const char*)s->name,
						"PUBSYM32"
					);
				} break;
				default: {
					//logf("?: %x\n", sym->kind);
				}
			}
		}
		assert((ptr - ptr2) == symbol_record_stream_data.size());
	}
	void read_module_symbol_stream (s16 module_index) {
		auto& mod = modules[module_index];
		auto* mi = mod.mi;
		
		struct C13Lineinfo {
			codeview_subsection_header* subsec;
			codeview_line_header* header () { return (codeview_line_header*)(subsec+1); }
		};
		std::map<uintptr_t, C13Lineinfo> c13_lineinfo;
		
		// deduplicate symbols with identical addresses TODO: should instead just push all symbols here and then later keep the relevant ones?
		ankerl::unordered_dense::map<uintptr_t, int> module_symbols;
		ankerl::unordered_dense::map<CV_ItemId, InlineeSourceLine*> module_inlinee_c13;

		auto find_first_overlapping_lineinfo = [&] (Symbol const& sym, uintptr_t* out_lineinfo_addr) -> codeview_subsection_header* {
			if (c13_lineinfo.empty()) return nullptr;

			// upper_bound returns first item larger than input (usually one past end of the range of equal items)
			
			// lineinfo to right, higher base address, or null if all lineinfos lower
			auto upper = c13_lineinfo.upper_bound(sym.base_addr);
			
			// lineinfo to left, lower or equal base address, or null if all lineinfos higher
			auto lower = upper; // awkward code with --lower because it-1 is not allowed for std::map
			lower = upper != c13_lineinfo.begin() ? --lower : c13_lineinfo.end();

			// return lower address lineinfo if range overlaps
			if (lower != c13_lineinfo.end()) {
				auto lineinfo_contrib_end = lower->first + lower->second.header()->contribution_size;
				if (sym.base_addr < lineinfo_contrib_end) {
					*out_lineinfo_addr = lower->first;
					return lower->second.subsec;
				}
			}
			// else test higher address lineinfo to handle weird cases of lineinfo having base address before symbol
			// (__security_check_cookie : src\vctools\crt\vcstartup\src\gs\amd64\amdsecgs.asm)
			if (upper != c13_lineinfo.end()) {
				auto lineinfo_contrib_start = upper->first;
				if (sym.base_addr + sym.size > lineinfo_contrib_start) {
					*out_lineinfo_addr = upper->first;
					return upper->second.subsec;
				}
			}
			
			return nullptr;
		};

		if (mi->stream_index_of_module_symbol_stream == 0xffff)
			return; // no symbol data
		mod.symbol_stream_data = copy_into_consecutive(mi->stream_index_of_module_symbol_stream);
		char* ptr = mod.symbol_stream_data.data();

		char* sym_info = ptr;
		mod.sym_info = sym_info;
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
		
		assert((ptr - mod.symbol_stream_data.data()) == streams[mi->stream_index_of_module_symbol_stream].size);
		
		//logf("> Module %d\n", module_index);

		//// Symbol info
		auto parse_symbol_info = [&] () {
			char* ptr = sym_info;

			u32 signature = *(u32*)ptr;
			ptr += sizeof(u32);
			assert(signature == 4); // CV_SIGNATURE_C13
		
			//logf(">> symbol_information\n");
			
			int cur_symbol = -1;
			int inline_depth = 0;

		#define INLINE_TREE_DEPTH_FIRST 0
		#if INLINE_TREE_DEPTH_FIRST // Depth-first inline tree
			//std::vector<INLINESITESYM*> stack;

			int max_inline_depth = 0;
			std::vector<BinAlloc::bid> stack2;
		#else
			struct Site {
				INLINESITESYM* inl;
				BinAlloc::bid site_id = -1;
			};
			std::vector<std::vector<Site>> inlinesites;

			auto push_inline_tree = [&] () {
				for (auto& level : inlinesites) {
					for (auto& site : level) {
						auto it = module_inlinee_c13.find(site.inl->inlinee);
						if (it == module_inlinee_c13.end())
							continue;

						auto name_it = IPI_id2name.find(site.inl->inlinee);
						if (name_it == IPI_id2name.end())
							continue;

						auto* anno_end = (PCompressedAnnotation)((char*)site.inl + sizeof(u16) + site.inl->reclen); // length field of codeview_symbol_header not contained in length
						
						Inlinesite s = {};
						s.fnname = name_it->second;
						s.fileId = it->second->fileId;
						s.sourceLineNum = it->second->sourceLineNum;
						site.site_id = binalloc.push(&s);

						Lineinfo::encode_compressed_annotation(
							site.inl->binaryAnnotations, anno_end,
							it->second->fileId, it->second->sourceLineNum,
							mod.file_checksum_ptr, binalloc);
					}
				}
				
				for (size_t l=0; l<inlinesites.size(); l++) {
					auto& level = inlinesites[l];
					auto* children_cur = l+1 < inlinesites.size() ? inlinesites[l+1].data() : nullptr;
					auto* children_end = l+1 < inlinesites.size() ? inlinesites[l+1].data() + inlinesites[l+1].size() : nullptr;

					for (size_t i=0; i<level.size(); i++) {
						auto& site = level[i];
						auto* right_sibling = i+1 < level.size() ? &level[i+1] : nullptr;

						auto* s = binalloc.get<Inlinesite>(site.site_id);
						if (right_sibling && right_sibling->inl->pParent == site.inl->pParent)
							s->pSibling = right_sibling->site_id; // else leave at -1

						// iterate children list to find first child of this parent
						if (children_cur) {
							size_t pParent = (char*)site.inl - sym_info;
							while (children_cur < children_end && children_cur->inl->pParent < pParent)
								children_cur++;

							if (children_cur < children_end && children_cur->inl->pParent == pParent) {
								s->pChildren = children_cur->site_id;
							}
						}
					}
				}
				
				symbols[cur_symbol].inline_depth = (uint8_t)std::min(inlinesites.size(), (size_t)255);
				if (!inlinesites.empty() && !inlinesites.front().empty())
					symbols[cur_symbol].p_inlinesites = inlinesites.front().front().site_id;
			};
		#endif

			while (ptr < sym_info + mi->byte_size_of_symbol_information) {
				auto sym = (codeview_symbol_header*)ptr;
			
				ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
				ptr = align_up(ptr, 4);

				//int entry_offs = (int)((char*)sym - sym_info);
				//logf("> %7d [%4x] %d %s\n", entry_offs, sym->kind, sym->length, SYM_ENUM_e_str(sym->kind));

				switch (sym->kind) {
					case S_GPROC32: case S_LPROC32:
					case S_GPROC32_ID: case S_LPROC32_ID: {
						auto* proc = (PROCSYM32*)sym;
						uintptr_t module_raddr = proc->off + sections_sorted[proc->seg-1].base_addr;
						
						//if (strcmp((const char*)proc->name, "ext_ms_win_hyperv_hvemulation_l1_1_0_WHvEmulatorCreateEmulator")==0) {
						//	printf("");
						//}
						//logf(">> %s %4d|%8x => %4llx, %4x %s\n", sym->kind == S_LPROC32 ? "L":"G", proc->seg, proc->off, module_raddr, proc->len, proc->name);

						Symbol s;
						s.base_addr = module_raddr;
						s.size = proc->len;
						s.name = stralloc.push( (const char*)proc->name ); // TODO: make sure to not do this for unused symbols improve string buffer cache hit rate
						s.module_index = module_index;

					#if TRACK_ALL_SYMBOLS
						sym_unfiltered.push_unsorted(Symbol(s));
					#endif

						// Functions with identical binary can be merged, in which dbghelp seems to output the symbol of the first entry which is what we will do as well
						// this lookup, which is used to attach lineinfo to the symbols so we don't have to search the address space twice (like dbghelp does?)
						// in case of overlapping symbols, only keep first occurance
						if (module_symbols.find(module_raddr) == module_symbols.end()) {
							auto* lineinfo = find_first_overlapping_lineinfo(s, &s.lineinfo_base_addr);
							if (lineinfo) {
								s.p_lineinfo = Lineinfo::encode_c13_lineinfo(lineinfo, mod.file_checksum_ptr, binalloc);
							}

							int idx = (int)symbols.size();
							symbols.push_unsorted(std::move(s));

							module_symbols.emplace(module_raddr, idx);
							cur_symbol = idx;
						}
						//else {
						//	// experimental, use last occurance
						//
						//	auto it = sym_map.find(module_raddr);
						//	assert(it != sym_map.end());
						//	if (it != sym_map.end()) {
						//		sym_sorted[it->second] = std::move(s);
						//	}
						//}
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

					#if INLINE_TREE_DEPTH_FIRST
						if (cur_symbol >= 0) {
							auto it = module_inlinee_c13.find(inl->inlinee);
							if (it == module_inlinee_c13.end())
								break;

							auto name_it = IPI_id2name.find(inl->inlinee);
							if (name_it == IPI_id2name.end())
								break;

							auto* anno_end = (PCompressedAnnotation)((char*)inl + sizeof(u16) + inl->reclen); // length field of codeview_symbol_header not contained in length
							size_t anno_len = anno_end - inl->binaryAnnotations;

							Inlinesite site = {};
							site.fnname = name_it->second;
							site.fileId = it->second->fileId;
							site.sourceLineNum = it->second->sourceLineNum;
							
							auto site_id = binalloc.push(&site);
							binalloc.push_bytes(inl->binaryAnnotations, anno_len);

							char terminator = -1;
							binalloc.push_bytes(&terminator, 1);

							// why +2? see S_INLINESITE_END
							stack2.resize(inline_depth+2, -1);

							auto* left_sibling = binalloc.get<Inlinesite>(stack2[inline_depth]);
							if (left_sibling) {
								// If we are not the first sibling, set the previous siblings's next pointer
								left_sibling->pSibling = site_id;
							}
							else if (inline_depth > 0) {
								// Find parent site and set it's children pointer
								auto* parent = binalloc.get<Inlinesite>(stack2[inline_depth-1]);
								assert(parent->pChildren < 0);
								parent->pChildren = site_id;
							}
							else {
								// symbol gets pointer to first site
								assert(symbols[cur_symbol].p_inlinesites == -1);
								symbols[cur_symbol].p_inlinesites = site_id;
							}

							stack2[inline_depth] = site_id;
						}

						inline_depth++;
						max_inline_depth = std::max(max_inline_depth, inline_depth);
					#else
						if (inlinesites.size() <= inline_depth)
							inlinesites.emplace_back();
						inlinesites[inline_depth].push_back({ inl });

						inline_depth++;
					#endif
					} break;
					case S_INLINESITE_END: {
						assert(inline_depth > 0);
						inline_depth--;

					#if INLINE_TREE_DEPTH_FIRST
						if (cur_symbol >= 0) {
							// Awkward hack, S_INLINESITE_END, which ends an S_INLINESITE
							// can't reset the current inlinesite, or else this pSibling can't be set in the next S_INLINESITE
							// so instead reset one level up, so that on the next parent site, its children won't link to this child
							stack2[inline_depth+1] = -1;
						}
					#endif
					} break;
					case S_END: {
						assert(inline_depth == 0);

					#if INLINE_TREE_DEPTH_FIRST
						if (cur_symbol >= 0) {
							if (max_inline_depth > 0) {
								symbols[cur_symbol].inline_depth = (uint8_t)std::min(max_inline_depth, 255);
							}
						}
						max_inline_depth = 0;
						stack2.clear();
					#else
						if (cur_symbol >= 0) {
							push_inline_tree();
						}
						inlinesites.clear();
					#endif

						cur_symbol = -1;
					} break;
				}
			}
			assert((ptr - sym_info) == mi->byte_size_of_symbol_information);
		};

		//// C13 line info
		
		auto parse_c13 = [&] () {
			{
				//logf("> c13_line_information\n");
				char* ptr = c13_line_information;
			
				// first pass to find FILECHKSMS ptr
				while (ptr < c13_line_information + mi->byte_size_of_c13_line_information) {
					auto* header = (codeview_subsection_header*)ptr;
					ptr += sizeof(codeview_subsection_header);

					if (header->type == DEBUG_S_FILECHKSMS) {
						mod.file_checksum_ptr = ptr;
						break;
					}
					ptr = (char*)header + sizeof(codeview_subsection_header) + header->length;
				}
			}
			
			auto read_inlinee_line_numbers = [&] (codeview_subsection_header* subsec) {
				auto* ptr = (char*)subsec;
				ptr += sizeof(codeview_subsection_header);
				auto* end = ptr + subsec->length;

				auto* header = (codeview_inlinee_source_line_header*)ptr;
				ptr += sizeof(codeview_inlinee_source_line_header);
				
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
				if (header->signature == CV_INLINEE_SOURCE_LINE_SIGNATURE) {
					while (ptr < end) {
						auto* line = (InlineeSourceLine*)ptr;
						ptr += sizeof(InlineeSourceLine);
						
						//logf(">>  Line %d %s %d\n", line->sourceLineNum, get_lineinfo_source_filepath(mod, line->fileId), line->inlinee);
						
						check_duplicates(line);
						module_inlinee_c13.try_emplace(line->inlinee, line);
					}
				} else if (header->signature == CV_INLINEE_SOURCE_LINE_SIGNATURE_EX) {
					while (ptr < end) {
						auto* line = (InlineeSourceLineEx*)ptr;
						ptr += sizeof(InlineeSourceLineEx);
						
						//logf(">>  Line %d %s %d\n", line->sourceLineNum, get_lineinfo_source_filepath(mod, line->fileId), line->inlinee);
						
						check_duplicates((InlineeSourceLine*)line);
						module_inlinee_c13.try_emplace(line->inlinee, (InlineeSourceLine*)line);

						ptr += line->countOfExtraFiles * sizeof(CV_off32_t);
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
				
				uintptr_t sec_offs = sections_sorted[header->contribution_section_id-1].base_addr;
				uintptr_t module_raddr = header->contribution_offset + sec_offs;
				
				// Why are there always conflicting sets of data in PDBs?
				// Even this basic C13 Data tends to exist for the same function multiple times (which tend to have conflicing contents as well)
				// Picking the first one seems to have been working ok
				auto check_duplicates = [=, &c13_lineinfo] () {
					auto it = c13_lineinfo.find(module_raddr);
					if (it != c13_lineinfo.end()) {
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
							
							auto* cksm = (codeview_file_checksum*)((char*)mod.file_checksum_ptr + line_block->offset_in_file_checksums);
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
				c13_lineinfo.try_emplace(module_raddr, C13Lineinfo{ subsec });
				
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
					}
				}
				ptr = (char*)header + sizeof(codeview_subsection_header) + header->length;
			}
			assert((ptr - c13_line_information) == mi->byte_size_of_c13_line_information);
		};
		
		parse_c13();
		parse_symbol_info();
	}
	
	void read_TPI_stream () {
		TPI_data = copy_into_consecutive(2);
		char* ptr = TPI_data.data();

		auto* header = (tpi_stream_header*)ptr;
		ptr += sizeof(tpi_stream_header);
		
		assert(header->version == 20040203);

		u32 count = header->one_past_last_type_index - header->minimal_type_index;
		u32 id = header->minimal_type_index;

		char* type_info = ptr;
		while (ptr < type_info + header->byte_count_of_type_record_data_following_the_header) {
			auto* lf = (codeview_type_record_header*)ptr;
			ptr += sizeof(u16) + lf->length; // length field of codeview_type_record_header not contained in length (but kind is)
			
			assert(id < header->one_past_last_type_index);
			
			auto push_symbol = [this, id] (unsigned char* data) -> const char* [[msvc::forceinline]] {
				ulong size = 0;
				size_t dcb = CbExtractNumeric(data, &size);
				auto* name = (const char *)data + dcb;

				assert(typeid2name.find(id) == typeid2name.end());
				typeid2name.emplace(id, name);

				return name;
			};

			switch (lf->kind) {
				case LF_STRUCTURE:
				case LF_CLASS: {
					auto* struc = (lfClass*)lf;
					auto* name = push_symbol(struc->data);
					//logf(">> lfClass: [%4x]: %s %x\n", id, name, struc->field);
				} break;
				case LF_UNION: {
					auto* struc = (lfUnion*)lf;
					auto* name = push_symbol(struc->data);
				} break;
				//case LF_PROCEDURE: {
				//	auto* proc = (lfProc*)lf;
				//	logf("Proc: id: [%4x]\n", id);
				//} break;
			}

			id++;
		}
		assert((ptr - type_info) == header->byte_count_of_type_record_data_following_the_header);
	}
	void read_IPI_stream () {
		IPI_data = copy_into_consecutive(4);
		char* ptr = IPI_data.data();

		auto* header = (tpi_stream_header*)ptr;
		ptr += sizeof(tpi_stream_header);
		
		assert(header->version == 20040203);

		u32 count = header->one_past_last_type_index - header->minimal_type_index;

		u32 id = header->minimal_type_index;
		char* type_info = ptr;
		
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
		
		while (ptr < type_info + header->byte_count_of_type_record_data_following_the_header) {
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
		assert((ptr - type_info) == header->byte_count_of_type_record_data_following_the_header);
		
		id = header->minimal_type_index;
		ptr = type_info;

		while (ptr < type_info + header->byte_count_of_type_record_data_following_the_header) {
			auto* lf = (codeview_type_record_header*)ptr;
			ptr += sizeof(u16) + lf->length; // length field of codeview_type_record_header not contained in length (but kind is)
			
			assert(id < header->one_past_last_type_index);

			switch (lf->kind) {
				case LF_FUNC_ID: {
					// free function
					auto* func = (lfFuncId*)lf;
					//logf(">> lfFuncId: [%4x]=%s\n", id, func->name);

					if (func->scopeId != 0) {
						auto formatted_strid = strids.push_scope_prefix(&stralloc, func->scopeId); // pushes "nested::scope::"
						stralloc.push((const char*)func->name); // pushes final name with null terminator

						assert(IPI_id2name.find(id) == IPI_id2name.end());
						IPI_id2name.emplace(id, formatted_strid);
					}
					else {
						assert(IPI_id2name.find(id) == IPI_id2name.end());
						IPI_id2name.emplace(id, stralloc.push((const char*)func->name));
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
						auto formatted_strid = stralloc.push_concat(*parent_name, "::", (const char*)func->name);
						
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
		assert((ptr - type_info) == header->byte_count_of_type_record_data_following_the_header);
	}

	void _reserve () {
		modules.reserve(128);
		sections_sorted.reserve(32);

		typeid2name.reserve(128);
		IPI_id2name.reserve(128);

		symbols.reserve(1024);
	#if TRACK_ALL_SYMBOLS
		sym_unfiltered.reserve(1024);
	#endif
	}
public:
	static std::unique_ptr<PDB_File> try_load_pdb (std::filesystem::path const& exe_path) {
		try {
			PDB_Locator locator(exe_path);
			auto path = locator.get_pdb_path();
			auto rsds = locator.get_rsds();
			return std::make_unique<PDB_File>(std::move(path), rsds);
		} catch (std::exception& ex) {
			logf("!!! PDB loading exception: %s\n", ex.what());
		}
		return nullptr;
	}
	PDB_File (std::filesystem::path&& path, PDB_Locator::PDB_guid_and_age const& rsds): path{std::move(path)} {
		ZoneScopedN("parse_pdb");

		if (!file.open(this->path)) {
			throw std::runtime_error("File not found: "+ path.string());
		}

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
		read_DBI();
		
		// read list of all sections
		assert(opt_streams->stream_index_of_section_header_dump != 0xFFFF);
		read_section_header_dump();

		// read symbol record stream, which contains certain global symbols, including functions
		// In practice any function symbol with code should appear in the modules with more information instead, functions will be appear duplicated here
		// but dbghelp will return symbols from here, right now I return symbols from the modules first and use this as a fallback only
		read_symbol_record_stream();

		// read symbol stream for each module, this contains function symbols and all lineinfo
		for (s16 module_index=0; module_index<(s16)modules.size(); module_index++) {
			read_module_symbol_stream(module_index);
		}
		
		symbols.sort_and_build_index();

	#if TRACK_ALL_SYMBOLS
		sym_unfiltered.sort_and_build_index();

		//print_all_symbols();
	#endif

		//logf("PDB read.\n");
	}


	Symbol* find_symbol_for_addr (uintptr_t addr) {
		ZoneScoped;
		
		// TODO: This comment is confusing, also in case of multiple symbols with same offset, which one do we return (last?)
		// This seems to work currently, but should take another look at this

		// need to find first symbol with lower or equal address than addr, but lower bound only returns that in equal case,
		// so use upper bound instead (returns first item bigger than addr), then use previous
		auto it = symbols.upper_bound((intptr_t)addr);
		if (it <= symbols.begin()) {
			// first symbol after addr is first symbol, search failed
			return nullptr;
		}
		it--;
		
		uintptr_t sym_addr = it->base_addr;
		Symbol* result = &*it;

		// iterate backwards through any symbols with equal address to find first one
		// but ignore ones in the global symbol_record_stream, as they contain mangled version of the symbol without size, but we sometimes want those if there is no real symbol
		// like __ImageBase
		// use raw ptr instead of iterator as we cannot seek before begin, which is annoying
		for (auto cur=(&*it)-1; cur >= &*symbols.begin() && cur->base_addr == sym_addr; --cur) {
			bool cur_from_module = cur->module_index != -1;
			bool result_from_module = result->module_index != -1;
			// earlier one always replaces, unless cur is not from module but previously written one was
			if (cur_from_module || !result_from_module) {
				result = cur;
			}
		}

		return &*result;
	}

	// try diagnosing us returning different symbols from dbghelp by being able to check if overlapping symbol existed but we chose the "wrong" one
	Symbol* has_symbol_for_addr (uintptr_t addr, const char* name) {
	#if TRACK_ALL_SYMBOLS
		// TODO: This comment is confusing, also in case of multiple symbols with same offset, which one do we return (last?)
		// This seems to work currently, but should take another look at this

		// need to find first symbol with lower or equal address than addr, but lower bound only returns that in equal case,
		// so use upper bound instead (returns first item bigger than addr), then use previous
		auto dymmy_Symbol = Symbol{ addr };
		auto it = std::upper_bound(sym_unfiltered.begin(), sym_unfiltered.end(), dymmy_Symbol, [] (Symbol const& l, Symbol const& r) {
			return l.base_addr < r.base_addr;
		});
		if (it <= sym_unfiltered.begin()) {
			// first symbol after addr is first symbol, search failed
			return nullptr;
		}
		it--;
		
		uintptr_t sym_addr = it->base_addr;
		
		//auto* actual_sym = find_symbol_for_addr(addr);
		//printf("> %s\n", stralloc[actual_sym->name]);
		
		// iterate backwards through any symbols with equal address to find first one
		// but ignore ones in the global symbol_record_stream, as they contain mangled version of the symbol without size, but we sometimes want those if there is no real symbol
		// like __ImageBase
		for (Symbol* cur=&*it; cur >= &*sym_unfiltered.begin() && cur->base_addr == sym_addr; --cur) {
			//printf(">> %s\n", stralloc[cur->name]);
			if (strcmp(stralloc[cur->name], name)==0) {
				return cur;
			}
		}
		
		return nullptr;
	#else
		return nullptr;
	#endif
	}
	
	bool find_source_loc_for_addr (Symbol* sym, uintptr_t addr, SourceLoc* out_src_loc) {
		ZoneScoped;
		
		assert(addr >= sym->base_addr);
		if (addr >= sym->base_addr + sym->size) {
			// past symbol address range, no valid line number
			return false;
		}
		uintptr_t rel_addr = addr - sym->lineinfo_base_addr;
		
		auto* lineinfo = binalloc.get<void>(sym->p_lineinfo);
		if (!lineinfo)
			return false;
		return Lineinfo::find_line_for_addr(lineinfo, rel_addr, names, out_src_loc);
	}
	
	int trace_inlinesites_for_addr (Symbol* sym, uintptr_t addr, SourceLocAndFn* out_locs, int num_locs) {
		ZoneScoped;
		assert(sym->inline_depth > 0); // only call when actually needed!

		uintptr_t proc_raddr = addr - sym->base_addr;

		int depth = 0;
		
		BinAlloc::bid site_id = sym->p_inlinesites;
		while (depth < num_locs && site_id >= 0) {
			auto* site = binalloc.get<Inlinesite>(site_id);
			SourceLoc encoded_loc = {};

			if (Lineinfo::find_line_for_addr2(site->get_lineinfo(), proc_raddr, names, &encoded_loc)) {
				// Matching Inlinesite

				out_locs[depth].fnname = stralloc[site->fnname];
				out_locs[depth].filepath = encoded_loc.filepath;
				out_locs[depth].lineno = encoded_loc.lineno;
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
};
