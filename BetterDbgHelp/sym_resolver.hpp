#pragma once
#include "util.hpp"

#include <psapi.h>
#pragma comment(lib, "Kernel32.lib")

#include <filesystem>
#include <fstream>
#include <unordered_map>

typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;

class PDB_File {
	// https://github.com/PascalBeyer/PDB-Documentation/?tab=readme-ov-file#finding-the-pdb

	std::string path;

	std::vector<char> data;

	static bool load_file (std::string const& filepath, std::vector<char>* out_data) {
		// https://stackoverflow.com/questions/51352863/what-is-the-idiomatic-c17-standard-approach-to-reading-binary-files
		std::ifstream ifs(filepath, std::ios::binary|std::ios::ate);

		if(!ifs)
			return false;

		auto end = ifs.tellg();
		ifs.seekg(0, std::ios::beg);

		auto size = std::size_t(end - ifs.tellg());

		if(size == 0) // avoid undefined behavior
			return false;

		auto buffer = std::vector<char>(size);

		if(!ifs.read(buffer.data(), buffer.size()))
			throw std::runtime_error(filepath/* + ": " + std::strerror(errno)*/);

		*out_data = std::move(buffer);
		return true;
	}

	struct msf_header{
		u8  signature[32];
		u32 page_size;
		u32 active_free_page_map;
		u32 amount_of_pages;
		u32 stream_table_stream_size;
		u32 unused;
		u32 page_list_of_stream_table_stream_page_list[1];
	};

	msf_header* header;
public:
	
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

		assert(page_idx_page == 0);
		u32* sts_pages = (u32*)get_page(header->page_list_of_stream_table_stream_page_list[page_idx_page]);

		if (page_idx > 0)
			printf("");

		return (char*)get_page(sts_pages[page_idx_page_idx]) + ptr_in_page;
	}

	struct Stream {
		u32 size;
		std::vector<u32> pages;
	};
	std::vector<Stream> streams;
	
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
	
	struct pdb_information_stream_header{
		u32 version;
		u32 timestamp;
		u32 age;
		GUID guid;
	};

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
		
		printf("Named Streams:\n");
		for(u32 index = 0, entry_index = 0; index < capacity && entry_index < amount_of_entries; index++){
			u32 word_index = index / (sizeof(u32) * 8);
			u32 bit_index  = index % (sizeof(u32) * 8);
			
			if(word_index < present_word_count && (present_bits[word_index] & (1u << bit_index))){
				auto& kv = entries[entry_index++];

				//std::string key = std::string(&string_buffer[kv.key]);
				//printf("> %s: %d\n", key.c_str(), kv.value);
				//named_streams[std::move(key)] = kv.value;
				std::string key = std::string(&string_buffer[kv.key]);
				printf("> %s: %d\n", &string_buffer[kv.key], kv.value);
				named_streams[&string_buffer[kv.key]] = kv.value;
				continue;
			}
		}
	}

	void read_names () {
		names_data = copy_into_consecutive(named_streams["/names"]);
		char* ptr = names_data.data();
		
		u32 signature = *(u32*)ptr;
		ptr += sizeof(u32);
		
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

	struct dbi_stream_header{
		u32 version_signature;
		u32 version;
		u32 age;
		u16 stream_index_of_the_global_symbol_index_stream;
		struct{
			u16 minor_version : 8;
			u16 major_version : 7;
			u16 is_new_version_format : 1;
		} toolchain_version;
		u16 stream_index_of_the_public_symbol_index_stream;
		u16 version_number_of_mspdb_dll_which_build_the_pdb;
		u16 stream_index_of_the_symbol_record_stream;
		u16 build_number_of_mspdb_dll_which_build_the_pdb;
		
		u32 byte_size_of_the_module_information_substream;   // substream 0
		u32 byte_size_of_the_section_contribution_substream; // substream 1
		u32 byte_size_of_the_section_map_substream;          // substream 2
		u32 byte_size_of_the_source_information_substream;   // substream 3
		u32 byte_size_of_the_type_server_map_substream;      // substream 4
		
		u32 index_of_the_MFC_type_server_in_type_server_map_substream;
		
		u32 byte_size_of_the_optional_debug_header_substream; // substream 6
		u32 byte_size_of_the_edit_and_continue_substream;     // substream 5
		
		struct{
			u16 was_linked_incrementally         : 1;
			u16 private_symbols_were_stripped    : 1;
			u16 the_pdb_allows_conflicting_types : 1; // undocumented /DEBUG:CTYPES flag.
		} flags;
		
		u16 machine_type;
		u32 reserved_padding;
	};

	struct pdb_section_contribution{
		s16 section_id;
		u16 padding1;
		s32 offset;
		s32 size;
		u32 characteristics;
		s16 module_index;
		u16 padding2;
		u32 data_crc;
		u32 reloc_crc;
	};
	struct pdb_module_information{
		u32 unused;
		struct pdb_section_contribution first_code_contribution;
		
		struct{
			u16 was_written : 1;
			u16 edit_and_continue_enabled : 1;
			u16 unused : 6;
			u16 TSM_index : 8;
		} flags;
		
		u16 stream_index_of_module_symbol_stream;
		
		u32 byte_size_of_symbol_information;
		u32 byte_size_of_c11_line_information;
		u32 byte_size_of_c13_line_information;
		
		u16 amount_of_source_files;
		u16 padding;
		u32 unused2;
		
		u32 edit_and_continue_source_file_string_index;
		u32 edit_and_continue_pdb_file_string_index;
		
		//char module_name_and_file_name_and_padding[1];
	};

	struct pdb_section_map_stream_header{
		u16 number_of_section_descriptors;
		u16 number_of_logical_section_descriptors;
	};
	struct pdb_section_map_entry{
		u16 flags;
		u16 logical_overlay_number;
		u16 group;
		u16 frame;
		u16 section_name;
		u16 class_name;
		u32 offset;
		u32 section_size;
	};

	struct optional_debug_header_substream{
		u16 stream_index_of_fpo_data;
		u16 stream_index_of_exception_data;
		u16 stream_index_of_fixup_data;
		u16 stream_index_of_omap_to_src_data;
		u16 stream_index_of_omap_from_src_data;
		u16 stream_index_of_section_header_dump;
		u16 stream_index_of_clr_token_to_clr_record_id;
		u16 stream_index_of_xdata;
		u16 stream_index_of_pdata;
		u16 stream_index_of_new_fpo_data;
		u16 stream_index_of_original_section_header_dump;
	};

	void read_DBI () {
		DBI_data = copy_into_consecutive(3);
		char* ptr = DBI_data.data();

		auto* header = (dbi_stream_header*)ptr;
		ptr += sizeof(dbi_stream_header);
		
		//// module_information_substream
		auto* ptr2 = ptr;

		u32 seen_modules = 0;
		while (ptr < ptr2+header->byte_size_of_the_module_information_substream) { // Not sure if right, where is the actual count for this?
			auto* mod = (pdb_module_information*)ptr;
			ptr += sizeof(pdb_module_information);

			const char* mod_name = ptr;
			ptr += strlen(mod_name)+1;

			const char* file_name = ptr;
			ptr += strlen(file_name)+1;

			ptr = align_up(ptr, 4);

			printf("> %-50s %-50s \n", mod_name, file_name);
			seen_modules++;
		}
		assert((ptr - ptr2) == header->byte_size_of_the_module_information_substream);
		ptr = ptr2 + header->byte_size_of_the_module_information_substream;
		
		//// section_contribution_substream
		ptr2 = ptr;

		u32 DBISCImpv = *(u32*)ptr;
		ptr += sizeof(u32);

		assert(DBISCImpv == (0xeffe0000 + 19970605));

		while (ptr < ptr2+header->byte_size_of_the_section_contribution_substream) {
			auto* sc = (pdb_section_contribution*)ptr;
			ptr += sizeof(pdb_section_contribution);

			printf("> %d %8x %8x %d\n", sc->section_id, sc->offset, sc->size, sc->module_index);
		}
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
		//assert(amount_of_modules == seen_modules);
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
			
			char name[9] = {};
			strncpy_s(name, (const char*)sh->Name, 8); // properly null-terminate
			printf("> %7s %8x %8x\n", name, sh->VirtualAddress, sh->Misc.VirtualSize);
		}

		_assert_sections_sorted();
	}

	
	std::vector<char> pdb_info_data;
	std::vector<char> names_data;
	std::vector<char> DBI_data;
	std::vector<char> section_header_dump_data;

	pdb_information_stream_header* info;

	std::unordered_map<const char*, u32> named_streams;

	const char* names;

	optional_debug_header_substream* opt_streams;


	//// Final needed data
	struct Section {
		std::string name;

		uintptr_t base_addr;
		size_t size;
	};
	std::vector<Section> sections_sorted;
	
	const Section* find_section_for_addr (uintptr_t raddr, u32* out_sec_id) {
		for (u32 id=0; id<sections_sorted.size(); id++) {
			auto& sec = sections_sorted[id];
			if (raddr < sec.base_addr)
				break;
			if (/*raddr >= sec.base_addr && */raddr < sec.base_addr + sec.size) {
				*out_sec_id = id;
				return &sec;
			}
		}
		return nullptr;
	}
	void _assert_sections_sorted () {
		for (size_t i=1; i<sections_sorted.size(); i++) {
			assert(sections_sorted[i].base_addr > sections_sorted[i-1].base_addr + sections_sorted[i-1].size);
		}
	}

	PDB_File (std::string&& path): path{path} {
		if (!load_file(this->path, &data))
			return;
		printf("%s data loaded\n", path.c_str());
		
		header = (msf_header*)data.data();
		
		u32* _sts_pages = (u32*)get_page(header->page_list_of_stream_table_stream_page_list[0]);
		u32* _sts0 = (u32*)get_page(_sts_pages[0]);
		
		u32 cur = 0;
		u32 amount_of_streams = *(u32*)read_sts(0);
		cur += sizeof(u32);
		
		while (streams.size() < amount_of_streams) {
			u32 stream_size = *(u32*)read_sts(cur);
			cur += sizeof(u32);
			if (stream_size == 0xffffffff) {
				// I think, this deleted stream does not count for amount_of_streams, but the link above is not clear on this
				continue;
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
		
		read_pdb_info();
		read_names();
		read_DBI();
		
		assert(opt_streams->stream_index_of_section_header_dump != 0xFFFF);
		read_section_header_dump();

		printf("PDB read\n");
	}

	bool success () { return !data.empty(); }
};

class SymResolver {
	HANDLE inspectee;

	struct LoadedModule {
		std::string path;

		uintptr_t base_addr;
		size_t size;

		std::unique_ptr<PDB_File> pdb;

		LoadedModule (std::string&& path, uintptr_t base_addr, size_t size) {
			auto pdb_path = std::filesystem::path(path);
			this->path = std::move(path);
			this->base_addr = base_addr;
			this->size = size;

			// Techically there might be more correct ways to find the pdb, and also ways that allow getting pdbs from microsoft servers
			// see above link
			pdb_path.replace_extension({".pdb"});
			pdb = std::make_unique<PDB_File>(pdb_path.string());
		}
	};
	struct ModuleCache {
		TimerMeasurement ttry_get_and_cache_module = TimerMeasurement("try_get_and_cache_module");

		std::vector<LoadedModule> sorted;
		
		const LoadedModule* cache (LoadedModule&& m) {
			auto base_addr = m.base_addr;
			sorted.push_back(std::move(m));
			// re-sort
			std::sort(sorted.begin(), sorted.end(), [] (LoadedModule& l, LoadedModule& r) {
				return std::less<uintptr_t>()(l.base_addr, r.base_addr);
			});

			for (auto& m : sorted) {
				if (base_addr == m.base_addr) return &m;
			}
			assert(false);
			return nullptr;
		}

		const LoadedModule* find_module_for_addr (HANDLE inspectee, uintptr_t addr) {
			for (auto& m : sorted) {
				if (addr > m.base_addr && addr <= m.base_addr + m.size) {
					return &m;
				}
			}
			
			TimerMeasZone(ttry_get_and_cache_module);
			return try_get_and_cache_module(inspectee, addr);
		}

		const LoadedModule* try_get_and_cache_module (HANDLE inspectee, uintptr_t addr) {
			// Only works for addresses in this process
			//// Do not use FreeLibrary because we set the flag GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
			//// see https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexa to get more information
			//constexpr DWORD flag = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
			//HMODULE mod = NULL;
			//
			//if (!GetModuleHandleExA(flag, (char*)addr, &mod)) {
			//	MODULEINFO info;
			//	if( GetModuleInformation( proc, mod, &info, sizeof( info ) ) != 0 )
			//	{
			//		const auto base = uint64_t( info.lpBaseOfDll );
			//		if( addr >= base && addr < ( base + info.SizeOfImage ) )
			//		{
			//			char name[1024];
			//			const auto nameLength = GetModuleFileNameA( mod, name, sizeof( name ) );
			//			if( nameLength > 0 )
			//			{
			//				// since this is the first time we encounter this module, load its symbols (needed for modules loaded after SymInitialize)
			//				ImageEntry* cachedModule = LoadSymbolsForModuleAndCache( name, nameLength, (DWORD64)info.lpBaseOfDll, info.SizeOfImage );
			//				return ModuleNameAndBaseAddress{ cachedModule->m_name, cachedModule->m_startAddress };
			//			}
			//		}
			//	}
			//}

			HMODULE modules[1024];
			DWORD needed = 0;
			if (!EnumProcessModules(inspectee, modules, sizeof(modules), &needed) || needed > sizeof(modules)) { // TODO: properly handle error
				print_err("EnumProcessModules");
			}

			// only return, and cache, the module that addr was in (as opposed to simply aching anything GetModuleInformation returns)
			// this causes more EnumProcessModules calls, but could help might make find_module_for_addr faster in the case where modules are never queried
			for (int i=0; i<needed/sizeof(HMODULE); i++) {
				auto& mod = modules[i];

				MODULEINFO info = {};
				if (GetModuleInformation(inspectee, mod, &info, sizeof(info))) {
					auto base = (uintptr_t)info.lpBaseOfDll;
					auto size = (size_t)info.SizeOfImage;
					if (addr >= base && addr < (base + size)) {
						char name[1024];
						auto nameLength = GetModuleFileNameExA(inspectee, mod, name, sizeof(name));
						if (nameLength > 0) {
							return cache(LoadedModule(std::string(name, nameLength), base, size));
						}
					}
				}
			}

			return nullptr;
		}
	};

	ModuleCache mod_cache;

public:
	SymResolver (HANDLE inspectee): inspectee{inspectee} {}

	void show_addr2sym (char* ptr) {
		uintptr_t addr = (uintptr_t)ptr;

		auto* mod = mod_cache.find_module_for_addr(inspectee, addr);
		if (!mod) {
			printf("#[%16llx]: Module not found\n", addr);
			return;
		}

		uintptr_t mod_raddr = addr - mod->base_addr;

		u32 sec_id = 0;
		auto* sec = mod->pdb->find_section_for_addr(mod_raddr, &sec_id);
		if (!sec) {
			printf("#[%16llx]: Section not found\n", addr);
			return;
		}

		uintptr_t sec_raddr = addr - mod->base_addr;

		printf("#[%16llx]: %-15s %-9s (%d) %8llx", addr, mod->path.c_str(), sec->name.c_str(), sec_id, sec_raddr);

		printf("\n");
	}

	void print_timings () {
		mod_cache.ttry_get_and_cache_module.print();
	}
};
