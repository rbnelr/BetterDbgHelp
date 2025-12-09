#pragma once
#include "util.hpp"
#include "codeview.hpp"

// https://github.com/PascalBeyer/PDB-Documentation

struct SourceLoc {
	const char* filepath;
	uint32_t    lineno;
};
struct SourceLocAndFn {
	const char* fnname = nullptr;
	const char* filepath = nullptr;
	uint32_t    lineno = 0;
};

// PDBs are assumed to be next to exe of the same name for the moment
// PDBs of microsoft dlls are not gotten yet (which dbghelp.dll does somehow)

class PDB_File {
	std::vector<char> data;
	
	void* get_page (u32 idx) {
		return (char*)data.data() + idx * header->page_size;
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

	pdb_information_stream_header* info;

	std::unordered_map<std::string_view, u32> named_streams;

	const char* names;

	optional_debug_header_substream* opt_streams;

	std::unordered_map<CV_ItemId, const char*> struct_typeid2name;

	struct Strbuf {
		std::vector<char> buf;

		Strbuf () {
			buf.reserve(1024*8);
		}

		const char* operator[] (u32 offset) {
			return buf.data() + offset;
		}
		
		u32 push (const char* str, size_t len) {
			auto offset = buf.size();
			buf.insert(buf.begin()+offset, str, str+len+1);

			if (offset > 0xffffffff) {
				assert(false);
			}
			return (u32)offset;
		}
		u32 push (const char* str) {
			return push(str, strlen(str));
		}

		u32 push_concat (const char* a, const char* b, const char* c) {
			auto offs = push(a, strlen(a)-1);
			push(b, strlen(b)-1);
			push(c, strlen(c));
			return offs;
		}

		//u32 vpushf (char const* format, va_list vl) {
		//	size_t offset = buf.size();
		//	size_t reserve = 3;
		//	buf.push_back(reserve);
		//
		//	auto ret = vsnprintf(buf.data() + offset, reserve, format, vl);
		//	ret = ret >= 0 ? ret : 0;
		//	bool was_big_enough = (size_t)ret < (reserve-1);
		//	buf.resize(offset + ret + 1);
		//	if (!was_big_enough) {
		//		// buffer was too small, buffer size was increased
		//		// now snprintf has to succeed, so call it again
		//		auto ret2 = vsnprintf(buf.data() + offset, ret + 1, format, vl);
		//		assert(ret2 <= ret);
		//	}
		//	if (offset > 0xffffffff) {
		//		assert(false);
		//	}
		//	return (u32)offset;
		//}
		//
		//u32 pushf (char const* format, ...) {
		//	va_list vl;
		//	va_start(vl, format);
		//
		//	auto ptr = vpushf(format, vl);
		//
		//	va_end(vl);
		//	return ptr;
		//}
	};
	Strbuf strbuf;

	//// Final needed data
	struct Section {
		std::string name;

		uintptr_t base_addr;
		size_t size;
	};
	std::vector<Section> sections_sorted;

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
		header = (msf_header*)data.data();
		assert(strncmp((const char*)header->signature, "Microsoft C/C++ MSF 7.00\r\n\032DS\0\0\0", 32) == 0);
	}
	void read_stream_table () {

		u32* _sts_ppages = (u32*)get_page(header->page_list_of_stream_table_stream_page_list[0]);
		u32* _sts_pages = (u32*)get_page(_sts_ppages[0]);
		char* _sts_start = (char*)read_sts(0);

		u32 cur = 0;
		u32 amount_of_streams = *(u32*)read_sts(cur);
		cur += sizeof(u32);
		
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
			//printf("Stream %3d: { ", si);
		
			u32 num_pages = ceil_div(stream.size, header->page_size);
			for (u32 i=0; i<num_pages; i++) {
				u32 page_idx = *(u32*)read_sts(cur);
				cur += sizeof(u32);
		
				stream.pages.push_back(page_idx);
		
				//printf("%d, ", page_idx);
			}
		
			//printf("}\n");
		}
	}

	void read_pdb_info () {
		
		pdb_info_data = copy_into_consecutive(1);
		char* ptr = pdb_info_data.data();

		info = (pdb_information_stream_header*)ptr;
		ptr += sizeof(pdb_information_stream_header);

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
		
		//printf("Named Streams:\n");
		for(u32 index = 0, entry_index = 0; index < capacity && entry_index < amount_of_entries; index++){
			u32 word_index = index / (sizeof(u32) * 8);
			u32 bit_index  = index % (sizeof(u32) * 8);
			
			if(word_index < present_word_count && (present_bits[word_index] & (1u << bit_index))){
				auto& kv = entries[entry_index++];

				//std::string key = std::string(&string_buffer[kv.key]);
				//printf("> %s: %d\n", key.c_str(), kv.value);
				//named_streams[std::move(key)] = kv.value;
				std::string key = std::string(&string_buffer[kv.key]);
				//printf("> %s: %d\n", &string_buffer[kv.key], kv.value);
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
		assert(signature == 0xEFFEEFFE);
		
		u32 hash_version = *(u32*)ptr;
		ptr += sizeof(u32);
		
		u32 string_buffer_size = *(u32*)ptr;
		ptr += sizeof(u32);
		
		names = ptr;
		ptr += string_buffer_size;
		
		u32 bucket_count = *(u32*)ptr;
		ptr += sizeof(u32);

		u32* buckets = (u32*)ptr;
		ptr += bucket_count * sizeof(u32);

		u32* amount_of_strings = (u32*)ptr;
		ptr += sizeof(u32);
	}
	
	void read_DBI () {
		DBI_data = copy_into_consecutive(3);
		char* ptr = DBI_data.data();

		auto* header = (dbi_stream_header*)ptr;
		ptr += sizeof(dbi_stream_header);
		
		//// module_information_substream
		auto* ptr2 = ptr;

		modules.reserve(64);
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

			//printf("> %d %-50s %-50s\n", mi->stream_index_of_module_symbol_stream, mod_name, file_name);

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

		//for (u32 i=0; i<num_section_contributions; i++) {
		//	auto* sc = &section_contributions[i];
		//	printf("> %d %8x %8x %d\n", sc->section_id, sc->offset, sc->size, sc->module_index);
		//}
		ptr += sizeof(pdb_section_contribution) * num_section_contributions;
		assert((ptr - ptr2) == header->byte_size_of_the_section_contribution_substream); // Why is this not correct?
		
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
			//printf("> %7s %8x %8x\n", name, sh->VirtualAddress, sh->Misc.VirtualSize);
		}

		for (size_t i=1; i<sections_sorted.size(); i++) {
			assert(sections_sorted[i].base_addr > sections_sorted[i-1].base_addr + sections_sorted[i-1].size);
		}
	}
	
	void read_symbol_record_stream () {
		auto* dbi = (dbi_stream_header*)DBI_data.data();
		symbol_record_stream_data = copy_into_consecutive(dbi->stream_index_of_the_symbol_record_stream);
		char* ptr = symbol_record_stream_data.data();

		char* ptr2 = ptr;

		auto push_symbol = [&] (u32 offs, u32 size, u16 seg, const char* name) {
			uintptr_t seg_addr = 0;
			if (seg > 0) {
				if (seg > sections_sorted.size()) {
					return; // No idea why this happens
				}
				seg_addr = sections_sorted[seg-1].base_addr;
			}
			sym_sorted.push_back(Symbol{ offs + seg_addr, size, name });
		};
		
		while (ptr < ptr2 + symbol_record_stream_data.size()) {
			auto sym = (codeview_symbol_header*)ptr;
			
			ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
			ptr = align_up(ptr, 4);

			switch (sym->kind) {
				//case S_PROCREF: case S_DATAREF: case S_LPROCREF: {
				//	auto* s = (REFSYM2*)sym;
				//	printf("REFSYM2: %s\n", s->name);
				//} break;
				//case S_CONSTANT: case S_MANCONSTANT: { // mostly works, but weirdness with the name? maybe using 1 for zero length array is wrong
				//	auto* s = (CONSTSYM*)sym;
				//	printf("CONSTSYM: %s\n", s->name);
				//} break;
				//case S_UDT: case S_COBOLUDT: {
				//	auto* s = (UDTSYM*)sym;
				//	printf("UDTSYM: %s\n", s->name);
				//} break;
				case S_LDATA32: case S_GDATA32: case S_LMANDATA: case S_GMANDATA: {
					auto* s = (DATASYM32*)sym;
					push_symbol(
						s->off,
						0, // TODO: these symbols don't have a size, possibly becasue the size is implicit based on the data type?,
						s->seg,
						(const char*)s->name
					);
					//printf("DATASYM32: seg:%d offs:%4x %s\n", s->seg, s->off, s->name);
				} break;
				case S_PUB32: {
					auto* s = (PUBSYM32*)sym;
					push_symbol(
						s->off,
						0,
						s->seg,
						(const char*)s->name
					);
					//printf("PUBSYM32: seg:%d offs:%4x %s\n", s->seg, s->off, s->name);
				} break;
				default: {

				}
			}
		}
		assert((ptr - ptr2) == symbol_record_stream_data.size());
	}
	void read_module_symbol_stream (s16 module_index) {
		auto& mod = modules[module_index];
		auto* mi = mod.mi;
		
		std::unordered_map<uintptr_t, size_t> lookup_proc_sym;
		size_t first_sym = sym_sorted.size();

		if (mi->stream_index_of_module_symbol_stream == 0xffff)
			return; // no symbol data
		mod.symbol_stream_data = copy_into_consecutive(mi->stream_index_of_module_symbol_stream);
		char* ptr = mod.symbol_stream_data.data();

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
		
		assert((ptr - mod.symbol_stream_data.data()) == streams[mi->stream_index_of_module_symbol_stream].size);
		
		//// Symbol info
		auto parse_symbol_info = [&] () {
			char* ptr = sym_info;

			u32 signature = *(u32*)ptr;
			ptr += sizeof(u32);
			assert(signature == 4); // CV_SIGNATURE_C13
		
			//printf(">> symbol_information\n");

			while (ptr < sym_info + mi->byte_size_of_symbol_information) {
				auto sym = (codeview_symbol_header*)ptr;
			
				ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
				ptr = align_up(ptr, 4);

				//int entry_offs = (int)((char*)sym - sym_info);
				//printf("> %7d [%4x] %d %s\n", entry_offs, sym->kind, sym->length, SYM_ENUM_e_str(sym->kind));

				switch (sym->kind) {
					case S_GPROC32: case S_LPROC32:
					case S_GPROC32_ID: case S_LPROC32_ID: {
						auto* proc = (PROCSYM32*)sym;
						//printf(">> %s %4d %4d %8x %s\n", sym->kind == S_LPROC32 ? "L":"G", proc->seg, proc->len, proc->off, proc->name);

						uintptr_t module_raddr = proc->off + sections_sorted[proc->seg-1].base_addr;

						lookup_proc_sym.emplace(module_raddr, sym_sorted.size());

						Symbol s;
						s.base_addr = module_raddr;
						s.size = proc->len;
						s.name = (const char*)proc->name;
						s.procsym = proc;
						s.module_index = module_index;
						sym_sorted.push_back(std::move(s));

						//if (strcmp((const char*)proc->name, "nlohmann::json_abi_v3_11_2::basic_json<nlohmann::json_abi_v3_11_2::ordered_map,std::vector,std::basic_string<char,std::char_traits<char>,std::allocator<char> >,bool,__int64,unsigned __int64,double,std::allocator,nlohmann::json_abi_v3_11_2::adl_serializer,std::vector<unsigned char,std::allocator<unsigned char> > >::json_value::json_value") == 0) {
						//	printf("");
						//}
					} break;
					case S_INLINESITE: {
						auto* inl = (INLINESITESYM*)sym;
						//inl->pParent // byte offs from symbol_information start of prev PROCSYM32 or INLINESITESYM, ie caller
						//inl->pEnd // byte offs of INLINESITE_END
						// inl->inlinee seems to be some kind of id that lets us look up line info, but not sure where that is and if that lineinfo is encoded horribly
						// no idea what inl->binaryAnnotations is
						//printf(">> INLINESITE inlinee: [%4x] %s\n", inl->inlinee, strbuf[proc_typeid2nameid[inl->inlinee]]);
					} break;
				}
			}
			assert((ptr - sym_info) == mi->byte_size_of_symbol_information);
		};

		//// C13 line info
		auto parse_c13 = [&] () {
			//printf("> c13_line_information\n");
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
			ptr = c13_line_information; // reset ptr
		
			auto read_line_numbers = [&] (codeview_subsection_header* subsec) {
				auto* ptr3 = ptr;

				// With usual compiler, this is once per function
				auto* header = (codeview_line_header*)ptr;
				ptr += sizeof(codeview_line_header);
				assert(header->flags == 0); // CV_LINES_HAVE_COLUMNS not implemented

				//printf(">> Header %d, %8x %8x\n", lines->contribution_section_id, lines->contribution_offset, lines->contribution_size);
			
				uintptr_t sec_offs = sections_sorted[header->contribution_section_id-1].base_addr;
				uintptr_t module_raddr = header->contribution_offset + sec_offs;
				auto it = lookup_proc_sym.find(module_raddr);
				auto* sym = it != lookup_proc_sym.end() ? &sym_sorted[it->second] : nullptr;

				while (ptr < ptr3 + subsec->length) {
					auto* line_block = (codeview_line_block_header*)ptr;
					ptr += sizeof(codeview_line_block_header);
					
					//printf(">> Block %d %d %s\n", line_block->block_size, line_block->offset_in_file_checksums, get_filepath(mod, line_block->offset_in_file_checksums));
			
					for (u32 i=0; i<line_block->amount_of_lines; i++) {
						auto* line = (codeview_line*)ptr;
						ptr += sizeof(codeview_line);
			
						//printf(">>  Line %d %d\n", line->start_line_number, line->offset);
					}
				}
				assert((ptr - ptr3) == subsec->length);

				if (sym) {
					assert(module_raddr == sym->base_addr); // lines section contribtion offset need to be procedure symbol offset
				
					ptr = ptr3;
					ptr += sizeof(codeview_line_header);

					while (ptr < ptr3 + subsec->length) {
						auto* line_block = (codeview_line_block_header*)ptr;
						ptr += sizeof(codeview_line_block_header);

						sym->src.push_back(Symbol::SrcLines {
							line_block->amount_of_lines,
							get_filepath(mod, line_block->offset_in_file_checksums),
							(codeview_line*)ptr,
						});

						ptr += line_block->amount_of_lines * sizeof(codeview_line);
					}
				}
			};
			auto read_inlinee_line_numbers = [&] (codeview_subsection_header* subsec) {
				auto* ptr3 = ptr;

				auto* header = (codeview_inlinee_source_line_header*)ptr;
				ptr += sizeof(codeview_inlinee_source_line_header);
				
				// rust (LLVM) seems to be output duplicate inlinee entries (same inlinee func id) even within the same module
				// filepath can differ:
				//                                   /rustc/1159e78c4747b02ef996e55082b704c09b970588/library\core\src\convert\mod.rs
				// C:\Users\Me\.rustup\toolchains\stable-x86_64-pc-windows-msvc\lib\rustlib\src\rust\library\core\src\convert\mod.rs
				// and even line numbers can differ for some bizzare reason
				// I have no idea how to handle this case correctly, there might be one InlineeSourceLine per INLINESITE, but I can't confirm this
				// I will have to see if my output matches dbghelp when taking the first or last entry instead of trying to match them by order
				auto verify_duplicates = [&] (InlineeSourceLine* line) {
					auto it = mod.inlinee_c13.find(line->inlinee);
					if (it != mod.inlinee_c13.end()) {
						// duplicate entry, verify they are functionally identical
						//assert(line->sourceLineNum == it->second->sourceLineNum);

						//assert(line->fileId == it->second->fileId);
						//auto* a = get_filepath(mod, line->fileId);
						//auto* b = get_filepath(mod, it->second->fileId);
						//assert(strcmp(a,b)==0);

						//printf(">>>>>> %s\n", a);
						//printf(">>>>>> %s\n", b);
					}
				};

				if (header->signature == CV_INLINEE_SOURCE_LINE_SIGNATURE) {
					while (ptr < ptr3 + subsec->length) {
						auto* line = (InlineeSourceLine*)ptr;
						ptr += sizeof(InlineeSourceLine);
						
						//printf(">>  Line %d %s %d\n", line->sourceLineNum, get_filepath(mod, line->fileId), line->inlinee);
						
						verify_duplicates(line);
						mod.inlinee_c13.try_emplace(line->inlinee, line);
					}
				} else if (header->signature == CV_INLINEE_SOURCE_LINE_SIGNATURE_EX) {
					while (ptr < ptr3 + subsec->length) {
						auto* line = (InlineeSourceLineEx*)ptr;
						ptr += sizeof(InlineeSourceLineEx);

						//printf(">>  Line %d %s %d\n", line->sourceLineNum, get_filepath(mod, line->fileId), line->inlinee);
						
						verify_duplicates((InlineeSourceLine*)line);
						mod.inlinee_c13.try_emplace(line->inlinee, (InlineeSourceLine*)line);

						ptr += line->countOfExtraFiles * sizeof(CV_off32_t);
					}
				} else {
					assert(false);
				}
				assert((ptr - ptr3) == subsec->length);
			};

			while (ptr < c13_line_information + mi->byte_size_of_c13_line_information) {
				auto* header = (codeview_subsection_header*)ptr;
				ptr += sizeof(codeview_subsection_header);

				//printf(">> %s\n", DEBUG_S_SUBSECTION_TYPE_e_str(header->type));

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
		
		parse_symbol_info();
		parse_c13();

		//if (strcmp(mod.file_name.data(), "C:\\coding\\BetterDbgHelp\\TinyProgram\\x64\\Release\\main.obj") == 0) {
		//	printf("");
		//}
	}
	
typedef struct lfFieldList {
    unsigned short  leaf;           // LF_FIELDLIST
    char            data[1];         // field list sub lists
} lfFieldList;

	// Only contains type info like arg and return types for type ids, no type names
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
			
			//if (id == 4415) {
			//	printf("");
			//}

			switch (lf->kind) {
				case LF_FIELDLIST: {
					auto* fl = (lfFieldList*)lf;
					
					ulong size = 0;
					size_t dcb = CbExtractNumeric((unsigned char*)fl->data, &size);
					auto* name = (const char *)fl->data + dcb;

					//printf("");
				} break;
				case LF_STRUCTURE:
				case LF_CLASS: {
					auto* struc = (lfClass*)lf;

					ulong size = 0;
					size_t dcb = CbExtractNumeric(struc->data, &size);
					auto* name = (const char *)struc->data + dcb;

					//if (strcmp(name, "Vec3") == 0) {
					//	printf(">> lfClass: [%4x]: %s %x\n", id, name, struc->field);
					//}
					struct_typeid2name.emplace(id, name);
				} break;
				case LF_UNION: {
					auto* struc = (lfUnion*)lf;

					ulong size = 0;
					size_t dcb = CbExtractNumeric(struc->data, &size);
					auto* name = (const char *)struc->data + dcb;
					//printf(">> lfClass: [%4x]: %s\n", id, name);
					struct_typeid2name.emplace(id, name);
				} break;
				case LF_PROCEDURE: {
					auto* proc = (lfProc*)lf;
					//printf("Proc: id: [%4x]\n", id);
				} break;
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
		while (ptr < type_info + header->byte_count_of_type_record_data_following_the_header) {
			auto* lf = (codeview_type_record_header*)ptr;
			ptr += sizeof(u16) + lf->length; // length field of codeview_type_record_header not contained in length (but kind is)

			switch (lf->kind) {
				//case LF_STRUCTURE:
				//case LF_CLASS: {
				//	auto* struc = (lfClass*)lf;
				//	printf(">> lfClass: [%4x]\n", id);
				//} break;
				case LF_FUNC_ID: {
					// free function
					auto* func = (lfFuncId*)lf;
					// not sure what func->type is, counting id like we are doing is the actual type of of the func as referenced by INLINESITE
					// -> Oh, probably function signature in TPI_stream
					//printf(">> lfFuncId: [%4x]=%s\n", id, func->name);
					//if (strcmp((const char*)func->name, "_Allocate") == 0) {
					//	printf("");
					//}
					proc_typeid2nameid.emplace((CV_ItemId)id, strbuf.push((const char*)func->name));
				} break;
				case LF_MFUNC_ID: {
					// member function (just the function name, struct name missing!)
					auto* func = (lfMFuncId*)lf;
					auto* parent_name = struct_typeid2name[func->parentType];
					assert(parent_name);
					if (parent_name) {
						//printf(">> lfMFuncId: [%4x]=%s::%s\n", id, parent_name, func->name);
						auto formatted_strid = strbuf.push_concat(parent_name, "::", (const char*)func->name);
						proc_typeid2nameid.emplace((CV_ItemId)id, formatted_strid);
					}
				} break;
			}

			id++;
		}
		assert((ptr - type_info) == header->byte_count_of_type_record_data_following_the_header);
	}

public:
	static std::unique_ptr<PDB_File> try_load_pdb (std::string&& path) {
		try {
			return std::make_unique<PDB_File>(std::move(path));
		} catch (std::exception&) {
			//fprintf(stderr, "PDB loading exception: %s\n", ex.what());
		}
		return nullptr;
	}
	PDB_File (std::string&& path) {
		if (!load_file(path, &data)) {
			throw std::runtime_error("File not found: "+ path);
		}

		printf("%s data loaded\n", path.c_str());
		
		read_header();
		read_stream_table();
		read_pdb_info();
		read_names();
		read_TPI_stream();
		read_IPI_stream();
		read_DBI();
		
		assert(opt_streams->stream_index_of_section_header_dump != 0xFFFF);
		read_section_header_dump();

		read_symbol_record_stream();

		for (s16 module_index=0; module_index<(s16)modules.size(); module_index++) {
			read_module_symbol_stream(module_index);
		}

		sort_symbols();

		printf("PDB read.\n");
	}
	
	struct Module {
		pdb_module_information* mi;
		std::string_view name;
		std::string_view file_name;

		std::vector<char> symbol_stream_data;
		
		// It seems like we can get duplicate entries of the same inlinee id across different modules via InlineeSourceLine
		// So in every module with a INLINESITE, the corresponding InlineeSourceLine and filename in DEBUG_S_FILECHKSMS exist in the same module
		// but the same inlinee file id can exist in multiple modules
		// this could be because due to link time optimization (the same function being inlined into different translation lines)
		// but actually, sometimes the filepath differs (same header, paths, compiled on different machines?)
		// so functions compiled from different copies of a header can get the same id
		char* file_checksum_ptr = nullptr;
		std::unordered_map<CV_ItemId, InlineeSourceLine*> inlinee_c13;

	};
	std::vector<Module> modules;

	const char* get_filepath (Module const& mod, CV_off32_t fileId) const {
		auto* cksm = (codeview_file_checksum*)(mod.file_checksum_ptr + fileId);
		auto* name = &names[cksm->offset_in_string_table];
		return name;
	}

	struct Symbol {
		uintptr_t base_addr; // relative to module
		size_t size;
		char const* name;
		
		s16 module_index = -1;
		PROCSYM32* procsym = nullptr; // needed for scanning INLINESITEs later

		struct SrcLines {
			uint32_t num_lines = 0;
			char const* filename = nullptr;
			codeview_line* lines = nullptr;
		};
		std::vector<SrcLines> src;
	};
	std::vector<Symbol> sym_sorted;

	std::unordered_map<CV_ItemId, u32> proc_typeid2nameid;

	void sort_symbols () {
		// sort based on base_addr
		std::stable_sort(sym_sorted.begin(), sym_sorted.end(), [] (Symbol const& l, Symbol const& r) {
			return std::less<uintptr_t>()(l.base_addr, r.base_addr);
		});

		// seems like functions like printf will appear both as procedure symbols with size in modules
		// and as PUB32 symbols without a size but with mangled names, and thus we will always have overlapping symbols
		
		//// assert non overlap including size
		//for (size_t i=1; i<sym_sorted.size(); i++) {
		//	//assert(sym_sorted[i].base_addr > sym_sorted[i-1].base_addr + sym_sorted[i-1].size);
		//	if (!(sym_sorted[i].base_addr > sym_sorted[i-1].base_addr + sym_sorted[i-1].size)) {
		//		printf("!! Overlapping symbols: [%8llx] %s/%s\n", sym_sorted[i].base_addr, sym_sorted[i-1].name, sym_sorted[i].name);
		//	}
		//}
	}
	Symbol* find_symbol_for_addr (uintptr_t addr) {
		// need to find first symbol with lower or equal address than addr, but lower bound only returns that in equal case,
		// so use upper bound instead (returns first item bigger than addr), then use previous
		auto dymmy_Symbol = Symbol{
			addr, 0, nullptr
		};
		auto it = std::upper_bound(sym_sorted.begin(), sym_sorted.end(), dymmy_Symbol, [] (Symbol const& l, Symbol const& r) {
			return l.base_addr < r.base_addr;
		});
		if (it <= sym_sorted.begin()) {
			// first symbol after addr is first symbol, search failed
			return nullptr;
		}
		it--;
		return &*it;
	}
	
	bool find_source_loc_for_addr (Symbol* sym, uintptr_t addr, SourceLoc* out_src_loc) {
		uintptr_t proc_raddr = addr - sym->base_addr;
		if (proc_raddr >= sym->size) {
			// past symbol address range, no valid line number
			return false;
		}

		for (auto& src : sym->src) {
			// codeview_lines seems to be sorted by offset, ie code address relative to start of function
			// there is only offset, no size, so I assume any addresses between this offset and the next belong to the line as well
			// lines can be out of order (earlier instructions belonging to later lines due to compiler optimizations for example)
			// lines will be missing (empty lines or lines with no generated code)
			// different entries can have the same line (single line to multiple instruction spans)
			// the same offset can appear twice with different lines (I guess multiple related lines that do one thing, maybe also when a statement is split over lines?)
			//  -> this part makes it confusing to resolve line numbers, as we would likely only return the first line (but debuggers via 'go to disassembly' or breakpoints might need info for each line!)
			//     tracy should never double count samples, and indeed dbghelp only reports one line, which appears the first line
			//     but it's unclear if the first match in this list is chosen or if it actively looks for the lowest line number TODO: determine via fuzzing and consider alternative datastructure)

			// linear scan for the moment, profile to see how much this impacts perf
			codeview_line* prev_line_with_lower_offset = src.lines;
			for (u32 i=1; i<src.num_lines; i++) {
				auto* line = &src.lines[i];

				// scan all lines and pick lowest lineno TODO: this could probably be simplified/accelerated by first deduplicating lines and storing the list of end addresses instead
				if (proc_raddr < line->offset) {
					// proc_raddr is in range [prev_offset, offset), so it belongs to all instructions with prev_offset
					// prev_line_with_lower_offset is the first one of these (lowest line number?)
					break;
				}
				if (line->offset != prev_line_with_lower_offset->offset)
					prev_line_with_lower_offset = line;
			}
			codeview_line* found_line = prev_line_with_lower_offset;

			*out_src_loc = { src.filename, found_line->start_line_number };
			return true;
		}
		return false;
	}
	/*
	void parse_inlinesite_lineno_annotations (Symbol* sym) {
		auto parse = [] (INLINESITESYM* inl, InlineeC13 const& c13) {
			// While BinaryAnnotationOpcode enum was released, the exact definition or code was apparently to released(?)
			// Only possible thanks to these implementations:
			// https://github.com/EpicGamesExt/raddebugger/blob/08642d2745da516387fa0f43639b7a8776a154b0/src/codeview/codeview_parse.c#L277
			// https://github.com/getsentry/pdb/blob/65c5b6d5c38c5f84225bfb3bc5365ea4097c8adf/src/modi/c13.rs#L1135

			PCompressedAnnotation cur = (PCompressedAnnotation)inl->binaryAnnotations;
			PCompressedAnnotation end = (PCompressedAnnotation)((char*)inl + sizeof(u16) + inl->reclen); // length field of codeview_symbol_header not contained in length
			
			u32 file_id = c13.line->fileId;
			u32 code_offset_base = 0;
			u32 code_offset = 0; // I think relative to inliner function (Not parent inlinesite) TODO: true?
			u32 code_length = 0; // 0 = null
			u32 lineno = c13.line->sourceLineNum;
			u32 num_lines = 1;
			u32 kind = 1; // 0 == Expression, 1 == Statement

			u32 prev_code_offset = -1;

			struct Line {
				u32 code_offset;
				u32 code_length;
				u32 lineno;
				u32 num_lines; // could mean one code range can be associated with a range of line numbers(?)
				u32 file_id;
				u32 kind;
			};
			std::vector<Line> lines;

			while (cur < end) {
				auto opcode = (BinaryAnnotationOpcode)CVUncompressData(cur);
				if (opcode == BA_OP_Invalid)
					continue;

				Line* prev = lines.empty() ? nullptr : &lines.back();

				auto param1 = CVUncompressData(cur);

				printf(">>> %s: %d%s\n", BinaryAnnotationOpcode_str(opcode), param1, opcode == BA_OP_ChangeCodeLengthAndCodeOffset ? ", (param2 not printed)":"");

				switch (opcode) {
					case BA_OP_CodeOffset: {
						code_offset = param1;
					} break;
					case BA_OP_ChangeCodeOffsetBase: {
						// Is this never used?
						code_offset_base = param1;
					} break;
					case BA_OP_ChangeCodeOffset: {
						code_offset += param1;
					} break;
					case BA_OP_ChangeCodeLength: {
						if (prev) {
							if (prev->code_length == 0 && prev->kind == kind) {
								prev->code_length = param1;
							}
						}
						code_offset += param1;
					} break;
					case BA_OP_ChangeFile: {
						// supposedly there are bugs with file changes inside functions, but presumably functions are almost always inside one file, so this should be rare anyway
						file_id = param1;
					} break;
					case BA_OP_ChangeLineOffset: {
						lineno += DecodeSignedInt32(param1);
					} break;
					case BA_OP_ChangeLineEndDelta: {
						num_lines = param1;
					} break;
					case BA_OP_ChangeRangeKind: {
						assert(param1 == 0 || param1 == 1);
						kind = param1;
					} break;
					case BA_OP_ChangeCodeOffsetAndLineOffset: {
						// param : ((sourceDelta << 4) | CodeDelta)
						u32 CodeDelta = param1 & 0b1111;
						s32 sourceDelta = DecodeSignedInt32(param1 >> 4); // signed int encoded weirdly because CVUncompressData chops off upper bits

						code_offset += CodeDelta;
						lineno += sourceDelta;
					} break;
					case BA_OP_ChangeCodeLengthAndCodeOffset: {
						auto param2 = CVUncompressData(cur);
						code_length = param1;
						code_offset += param2;
					} break;

					case BA_OP_ChangeColumnStart:
					case BA_OP_ChangeColumnEndDelta:
					case BA_OP_ChangeColumnEnd: {
						// ignore column info
					} break;

					default: {
						assert(false);
					}
				}
					
				switch (opcode) {
					case BA_OP_ChangeCodeOffset:
					case BA_OP_ChangeCodeOffsetAndLineOffset:
					case BA_OP_ChangeCodeLengthAndCodeOffset: {
						// code_length is either explicitly given with BA_OP_ChangeCodeLengthAndCodeOffset
						// or written after push_line with BA_OP_ChangeCodeLength
						// or implicitly computed from last and current offset

						u32 offset = code_offset + code_offset_base;
						if (prev) {
							if (prev->code_length == 0 && prev->kind == kind) {
								prev->code_length = offset - prev->code_offset;
							}
						}

						lines.push_back({
							offset,
							code_length,
							lineno,
							num_lines,
							file_id,
							kind,
						});

						code_length = 0;
					} break;
				}
			}

			for (auto& l : lines) {
				auto* cksm = (codeview_file_checksum*)(c13.file_checksum_ptr + l.file_id);
				auto* name = &names[cksm->offset_in_string_table];

				printf(">>> [%4x, %4x) %s:%d (%d %d)\n", l.code_offset, l.code_offset+l.code_length, name, l.lineno, l.num_lines, l.kind);
			}

			assert(cur == end);
		};
		
		if (sym->procsym == nullptr)
			return;
			
		printf("> for %s:\n", sym->name);

		char* ptr = (char*)sym->procsym;
		for (;;) {
			auto sym = (codeview_symbol_header*)ptr;
			ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
			ptr = align_up(ptr, 4);

			switch (sym->kind) {
				case S_INLINESITE: {
					auto* inl = (INLINESITESYM*)sym;
					printf(">> INLINESITE %d:\n", inl->inlinee);

					auto it = inlinee_c13.find(inl->inlinee);
					if (it == inlinee_c13.end()) {
						assert(false);
					}
					else {
						auto& c13 = it->second;
						parse(inl, c13);
					}
				} break;
				case S_END: {
					return;
				} break;
			}
		}
	}
	*/

	void trace_inlinesites_for_addr (Symbol* sym, uintptr_t addr, SourceLocAndFn* out_locs, int num_locs, int* out_num_locs) {
				*out_num_locs = 0;
		if (sym->procsym == nullptr)
			return;
		assert(sym->module_index >= 0);
		auto& mod = modules[sym->module_index];

		auto find_srcloc_in_encoded = [this, &mod] (INLINESITESYM* inl, uintptr_t proc_raddr, SourceLoc* out_loc) -> bool {
			
			auto it = mod.inlinee_c13.find(inl->inlinee);
			if (it == mod.inlinee_c13.end()) {
				assert(false);
				return false;
			}
			auto* c13_line = it->second;
			
			// While BinaryAnnotationOpcode enum was released, the exact definition or code was apparently to released(?)
			// Only possible thanks to these implementations:
			// https://github.com/EpicGamesExt/raddebugger/blob/08642d2745da516387fa0f43639b7a8776a154b0/src/codeview/codeview_parse.c#L277
			// https://github.com/getsentry/pdb/blob/65c5b6d5c38c5f84225bfb3bc5365ea4097c8adf/src/modi/c13.rs#L1135

			PCompressedAnnotation cur = (PCompressedAnnotation)inl->binaryAnnotations;
			PCompressedAnnotation end = (PCompressedAnnotation)((char*)inl + sizeof(u16) + inl->reclen); // length field of codeview_symbol_header not contained in length
			
			u32 file_id = c13_line->fileId;
			u32 code_offset_base = 0;
			u32 code_offset = 0;
			u32 code_length = 0; // 0 = null
			u32 lineno = c13_line->sourceLineNum;
			u32 num_lines = 1;
			u32 kind = 1; // 0 == Expression, 1 == Statement

			u32 prev_code_offset = -1;

			struct Line {
				u32 code_offset;
				u32 code_length;
				u32 lineno;
				u32 num_lines; // could mean one code range can be associated with a range of line numbers(?)
				u32 file_id;
				u32 kind;
			};
			std::vector<Line> lines;
			lines.reserve(64);

			while (cur < end) {
				auto opcode = (BinaryAnnotationOpcode)CVUncompressData(cur);
				if (opcode == BA_OP_Invalid)
					continue;

				Line* prev = lines.empty() ? nullptr : &lines.back();

				auto param1 = CVUncompressData(cur);
				switch (opcode) {
					case BA_OP_CodeOffset: {
						code_offset = param1;
					} break;
					case BA_OP_ChangeCodeOffsetBase: {
						assert(false);
						// Is this never used?
						code_offset_base = param1;
					} break;
					case BA_OP_ChangeCodeOffset: {
						code_offset += param1;
					} break;
					case BA_OP_ChangeCodeLength: {
						if (prev) {
							if (prev->code_length == 0 && prev->kind == kind) {
								prev->code_length = param1;
							}
						}
						code_offset += param1;
					} break;
					case BA_OP_ChangeFile: {
						// supposedly there are bugs with file changes inside functions, but presumably functions are almost always inside one file, so this should be rare anyway
						// TODO: these file_ids likely are local to the module that contained this INLINESITESYM with CompressedAnnotation,
						// while my current lookup for inlinee id and InlineeSourceLine can come from different modules as that data seems to be duplicated
						file_id = param1;
					} break;
					case BA_OP_ChangeLineOffset: {
						lineno += DecodeSignedInt32(param1);
					} break;
					case BA_OP_ChangeLineEndDelta: {
						num_lines = param1;
					} break;
					case BA_OP_ChangeRangeKind: {
						assert(param1 == 0 || param1 == 1);
						kind = param1;
					} break;
					case BA_OP_ChangeCodeOffsetAndLineOffset: {
						// param : ((sourceDelta << 4) | CodeDelta)
						u32 CodeDelta = param1 & 0b1111;
						s32 sourceDelta = DecodeSignedInt32(param1 >> 4); // signed int encoded weirdly because CVUncompressData chops off upper bits

						code_offset += CodeDelta;
						lineno += sourceDelta;
					} break;
					case BA_OP_ChangeCodeLengthAndCodeOffset: {
						auto param2 = CVUncompressData(cur);
						code_length = param1;
						code_offset += param2;
					} break;

					case BA_OP_ChangeColumnStart:
					case BA_OP_ChangeColumnEndDelta:
					case BA_OP_ChangeColumnEnd: {
						// ignore column info
					} break;

					default: {
						assert(false);
					}
				}
					
				switch (opcode) {
					case BA_OP_ChangeCodeOffset:
					case BA_OP_ChangeCodeOffsetAndLineOffset:
					case BA_OP_ChangeCodeLengthAndCodeOffset: {
						// code_length is either explicitly given with BA_OP_ChangeCodeLengthAndCodeOffset
						// or written after push_line with BA_OP_ChangeCodeLength
						// or implicitly computed from last and current offset

						u32 offset = code_offset + code_offset_base;
						if (prev) {
							if (prev->code_length == 0 && prev->kind == kind) {
								prev->code_length = offset - prev->code_offset;
							}
						}

						lines.push_back({
							offset,
							code_length,
							lineno,
							num_lines,
							file_id,
							kind,
						});

						code_length = 0;
					} break;
				}
			}

			for (auto& l : lines) {
				if (proc_raddr >= l.code_offset && proc_raddr < l.code_offset + l.code_length) {
					out_loc->filepath = get_filepath(mod, l.file_id);
					out_loc->lineno = l.lineno;
					return true;
				}
			}
			return false;
		};

		uintptr_t proc_raddr = addr - sym->base_addr;

		int depth = 0;
		int max_written_depth = 0;
		
		char* ptr = (char*)sym->procsym;
		for (;;) {
			auto sym = (codeview_symbol_header*)ptr;
			ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
			ptr = align_up(ptr, 4);

			switch (sym->kind) {
				case S_INLINESITE: {
					auto* inl = (INLINESITESYM*)sym;
					if (depth < num_locs) { // here: depth > 0 && out_locs[depth-1].filepath != nullptr
						SourceLoc encoded_loc = {};
						if (find_srcloc_in_encoded(inl, proc_raddr, &encoded_loc)) {
							assert(out_locs[depth].filepath == nullptr);

							auto it = proc_typeid2nameid.find(inl->inlinee);
							assert(it != proc_typeid2nameid.end());
							auto* name = it == proc_typeid2nameid.end() ? "[unknown]":strbuf[it->second];
							out_locs[depth].fnname = name;

							out_locs[depth].filepath = encoded_loc.filepath;
							out_locs[depth].lineno = encoded_loc.lineno;

							max_written_depth = std::max(max_written_depth, depth+1);

							// TODO: in theory: only if there is line info can further inlinesites have line info, which would be an optimization
						}
					}
					depth++;
				} break;
				case S_INLINESITE_END: {
					assert(depth > 0);
					depth--;
				} break;
				case S_END: {
					*out_num_locs = max_written_depth;
					return;
				} break;
			}
		}
	}
	
};
