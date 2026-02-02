#pragma once
#include "util.hpp"
#include <codecvt>

#include <Urlmon.h>
#pragma comment(lib, "Urlmon.lib")

class ExeParser {
	MemoryMappedFile file;

	IMAGE_DOS_HEADER* dos_header = {};
	IMAGE_NT_HEADERS* nt_header = {}; // includes optional header
	IMAGE_SECTION_HEADER* section_headers = {};

	// map loaded virtual address to exe file address
	char* map_rva (DWORD rva) {
		for (int i=0; i<nt_header->FileHeader.NumberOfSections; i++) {
			auto& sec = section_headers[i];
			if (rva >= sec.VirtualAddress && rva < sec.VirtualAddress + sec.Misc.VirtualSize) {
				auto offset = rva - sec.VirtualAddress + sec.PointerToRawData;
				return (char*)file.data() + offset;
			}
		}
		throw std::runtime_error("");
	}

public:
	void open_image (std::filesystem::path const& filepath) {
		if (!file.open(filepath.c_str()))
			throw std::runtime_error("File not found");

		dos_header = (IMAGE_DOS_HEADER*)file.data();
		if (dos_header->e_magic != 0x5A4D) // MZ
			throw std::runtime_error("Error parsing image");
		
		nt_header = (IMAGE_NT_HEADERS*)((char*)file.data() + dos_header->e_lfanew);
		if (nt_header->Signature != 0x00004550) // PE\0\0
			throw std::runtime_error("Error parsing image");
		
		section_headers = (IMAGE_SECTION_HEADER*)((char*)file.data() + dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS));
	}
	
	
	struct RSDSI { // RSDS debug info
		DWORD   dwSig; // RSDS
		GUID    guidSig;
		DWORD   age;
		char    pdb_name[1];
	};
	RSDSI* find_rsds () {
		auto& dbg = nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
		if (dbg.VirtualAddress) {
			auto* debug_directory = (IMAGE_DEBUG_DIRECTORY*)map_rva(dbg.VirtualAddress);
			auto num_debug_directory = dbg.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
			for (int i=0; i<num_debug_directory; i++) {
				auto& d = debug_directory[i];

				if (d.Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
					auto* rsds = (RSDSI*)((char*)file.data() + d.PointerToRawData);
					if (rsds->dwSig != 0x53445352) // "RSDS"
						throw std::runtime_error("Error parsing image");

					return rsds;
				}
			}
		}
		throw std::runtime_error("RSDSI not found");
	}

	template <typename FUNC>
	bool find_exports (StrAlloc& strs, FUNC func_and_name) {
		auto& exports = nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		if (exports.VirtualAddress) {
			auto* export_directory = (IMAGE_EXPORT_DIRECTORY*)map_rva(exports.VirtualAddress);
			
			// list[NumberOfFunctions] of RVA of function addresses
			// Not sorted!
			auto* functions = (DWORD*)map_rva(export_directory->AddressOfFunctions);
			// list[NumberOfNames] of RVA of function name strings
			auto* names = (DWORD*)map_rva(export_directory->AddressOfNames);
			// list[NumberOfNames] of RVA of indices into functions, for each name
			auto* ordinals = (WORD*)map_rva(export_directory->AddressOfNameOrdinals);
			
			// export table can have functions without name entry,
			// which is why we iterate list of ordinals instead to skip nameless function
			for (DWORD i=0; i<export_directory->NumberOfNames; i++) {
				auto ord = ordinals[i];
				auto* name = (const char*)map_rva(names[i]);
				auto func_rva = functions[ord];

				func_and_name(func_rva, strs.push(name));
			}

			return true;
		}
		// don't throw exception
		// exports.VirtualAddress being null may be expected for .exe (unlike .dll)
		return false;
	}
};

// The executable (.exe or .dll) itself contains data which can be used to find the associated pdb file
// commonly when building your own application or when sometimes when downloading application, a pdb with the same name as the exe will simply be placed next to the exe
// so we first look for a pdb of the same name next to the exe
// then we look at the absolute pdb path stored inside the exe RSDS debug info, which likely is where it was built in VS
// if neither are found, we look at %temp%/SymbolCache/<pdb_filename>/<pdb_guid><pdb_age>/<pdb_filename>, which is where symbol server pdbs get placed by VS
// if not, try to fetch it from https://msdl.microsoft.com/download/symbols/<pdb_name>/<pdb_guid><pdb_age>/<pdb_name> and cache it inside this folder ourselves
class PDB_Locator {
	ExeParser exe;

	ExeParser::RSDSI* rsds = {};

	std::filesystem::path const& exe_path;
	std::filesystem::path pdb_path_in_exe;
	
	std::wstring symbol_server_url;
	std::filesystem::path cache_path;

	void parse_rsds () {
		auto& guid = rsds->guidSig;

		wchar_t guid_age_str[128];
		// guid in hex + age (is this also hex?)
		swprintf(guid_age_str, 128, L"%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%X",
			guid.Data1, guid.Data2, guid.Data3,
			guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7],
			rsds->age);
		
		pdb_path_in_exe = std::string( (const char*)rsds->pdb_name );
		auto pdb_name = pdb_path_in_exe.filename().wstring();

		symbol_server_url = get_symbol_server_url(guid_age_str, pdb_name);
		cache_path = get_cache_path(guid_age_str, pdb_name);
	}
	
	std::wstring get_symbol_server_url (const wchar_t* guid_age_str, std::wstring const& pdb_name) {
		return std::wstring(L"https://msdl.microsoft.com/download/symbols/")+ pdb_name +L"/"+ guid_age_str +L"/"+ pdb_name;
	}
	std::filesystem::path get_cache_path (const wchar_t* guid_age_str, std::wstring const& pdb_name) {
		// hardcode cache temp, I'm not sure if there's a more correct way to come up with this
		// supposedly the _NT_SYMBOL_PATH env var exists, but apparently that's not what Visual Studio used, so screw that

		// This cache often will contain no pdb at this path, but another subfolder called "stripped" which contains the file, presumably with just some functions names left in
		// I don't know I need to somehow respect that or not
		
		// I initially thought that with notepad.exe, I may have written downloaded the pdb to the wrong spot or in a wrong way
		// and that this caused dbghelp to fail, but actually it fails on notepad even without me interfering (despite me using the pdb successfully)
		// But still, let's create a custom cache folder to avoid future problems with peoples debugging, since I have no idea what the microsoft code is actually doing

		//return std::filesystem::temp_directory_path() / "SymbolCache" / pdb_name / guid_age_str / pdb_name;
		return std::filesystem::temp_directory_path() / "Hexcoder" / "SymbolCache" / pdb_name / guid_age_str / pdb_name;
	}

	std::filesystem::path try_downloading_pdb_if_not_in_cache () {
		if (std::filesystem::exists(cache_path)) {
			return cache_path;
		}
		
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::string url = converter.to_bytes(symbol_server_url);
		
		// Abuse URLOpenBlockingStreamW to avoid creating empty directories when no file can actually be downloaded
		// Ideally I'd use the blocking stream to actually write the file out myself, but I don't want to incur overhead from manually doing writing out in chunks
		// (maybe URLDownloadToFileW could be faster? It's simpler for me at least)
		IStream* stream = nullptr;
		HRESULT hr = URLOpenBlockingStreamW(NULL, symbol_server_url.c_str(), &stream, 0, NULL);
		if (stream) stream->Release();
		if (hr == S_OK) {
			logf("[BetterDbgHelp] Downloading pdb from %s to %s.\n", url.c_str(), cache_path.string().c_str());
			
			std::filesystem::create_directories(cache_path.parent_path());

			if (URLDownloadToFileW(NULL, symbol_server_url.c_str(), cache_path.c_str(), 0, NULL) == S_OK) {
				return cache_path;
			}
		}
		
		// fail, return empty path
		return {};

		// Alternative:
		//#include <Wininet.h>
		//#pragma comment(lib, "Wininet.lib")
		//auto hInternet = InternetOpenA("SymbolServerDownload", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		//if (!hInternet) {
		//	print_err("SymbolServerDownload");
		//}
		//
		//auto hConnect  = InternetOpenUrlA(hInternet, symbol_server_url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
		//if (!hConnect) {
		//	print_err("InternetOpenUrlA");
		//}
		//
		//char buffer[1024];
		//DWORD bytesRead;
		//while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
		//	//outputFile.write(buffer, bytesRead);
		//	printf("");
		//}
		//
		//InternetCloseHandle(hConnect);
		//InternetCloseHandle(hInternet);
	}

public:
	// global search path across all DbgHelpWrapper sessions
	static inline std::vector<std::filesystem::path> extra_search_paths;

	PDB_Locator (std::filesystem::path const& filepath): exe_path{filepath} {
		ZoneScoped;

		try {
			exe.open_image(exe_path);
			rsds = exe.find_rsds();

			parse_rsds();
		}
		catch (std::exception& ex) {
			throw std::runtime_error((exe_path.string() +": ")+ ex.what());
		}
		//logf(">> %s ->\n>>> symbol_server_url: %s\n>>> cache_path: %s\n", filepath.c_str(), symbol_server_url.c_str(), cache_path.u8string().c_str());
	}

	// This way of finding pdbs may not actually match what dbghelp does, but it made sense for me
	// only file existance is checked, not if file is actually the correct pdb
	// instead pdb parsing will later check PDB_guid_and_age
	std::filesystem::path get_pdb_path () {
		ZoneScoped;

		// prefer pdb next to exe
		{
			std::filesystem::path pdb_path = exe_path;
			pdb_path.replace_extension({".pdb"});

			if (std::filesystem::exists(pdb_path)) {
				return pdb_path;
			}
		}

		// look for pdb at absolute path if exe contains abolute path
		if (pdb_path_in_exe.is_absolute()) {
			if (std::filesystem::exists(pdb_path_in_exe)) {
				return pdb_path_in_exe;
			}
		}
		
		// try to emulate extra search paths like dbghelp (?) no testing was done if this actually matches what dbghelp does
		std::filesystem::path pdb_filename = exe_path.filename();
		pdb_filename.replace_extension({".pdb"});

		for (auto& search_path : extra_search_paths) {
			auto pdb_path = search_path / pdb_filename;
			if (std::filesystem::exists(pdb_path)) {
				return pdb_path;
			}
		}
		
		// else look for file via MS symbol servers (or cached files)
		auto cached_pdb_path = try_downloading_pdb_if_not_in_cache();
		if (!cached_pdb_path.empty()) {
			return cached_pdb_path;
		}

		throw std::runtime_error("Cannot locate pdb for: "+ exe_path.string());
	}
	
	struct PDB_guid_and_age {
		GUID    guidSig;
		DWORD   age;
	};
	PDB_guid_and_age get_rsds () {
		return { rsds->guidSig, rsds->age };
	}

	// compare GUID and age from exe and pdb, this helps detect when the wrong pdb is used by accident
	// TODO: I interpretet GUID to be created on exe creation, then modifications to the exe (edit and conitinue?) increase the age
	// I thought the pdb would be modified at the same time, so guid and age have to match
	// but asking for ucrtbase.dll with age 1 gives us a pdb with matching guid but the pdb has age 3
	// does this mean exe and pdb can be modified seperately and the age does not have to match?
	static bool verify_pdb (PDB_guid_and_age const& exe, pdb_information_stream_header const* pdb) {
		return memcmp(&exe.guidSig, &pdb->guid, sizeof(GUID)) == 0; /* && exe.age == pdb->age;*/
	}
};

class ExportTableQuery {
	StrAlloc names;

	struct Function {
		uint32_t address;
		StrAlloc::sid mangled_name;
	};
	std::vector<Function> functions_sorted;
	
	static __forceinline int _cmp (Function const& l, Function const& r) {
		return std::less<uint64_t>()(l.address, r.address);
	}
	static __forceinline bool _less (Function const& l, Function const& r) {
		return l.address < r.address;
	}

public:
	ExportTableQuery (std::filesystem::path const& filepath) {
		ZoneScoped;

		try {
			ExeParser exe;
			names.init();
			
			exe.open_image(filepath);
			exe.find_exports(names, [this] (uint32_t func_rva, StrAlloc::sid mangled_name) {
				functions_sorted.emplace_back(func_rva, mangled_name);
			});
			
			std::stable_sort(functions_sorted.begin(), functions_sorted.end(), _cmp);
		}
		catch (std::exception& ex) {
			throw std::runtime_error((filepath.string() +": ")+ ex.what());
		}
	}

	const char* query (uint64_t mod_rva, uint64_t* out_sym_rva, uint32_t* out_idx) {
		uint32_t idx;
		if (!query_index(mod_rva, &idx))
			return nullptr;

		*out_idx = idx;
		return get(idx, out_sym_rva);
	}
	
	bool query_index (uint64_t mod_rva, uint32_t* out_idx) {
		if (mod_rva >= INT_MAX) {
			assert(false); // If exe is ever over 4GB, likely exports can't be in upper addresses
			return false;
		}

		auto dummy = Function{ (uint32_t)mod_rva, 0 };
		auto it = std::upper_bound(functions_sorted.begin(), functions_sorted.end(), dummy, _less);
		if (it <= functions_sorted.begin())
			return false;
		it--;
		
		assert(mod_rva >= it->address);

		*out_idx = (uint32_t)(it - functions_sorted.begin());
		return true;
	}
	const char* get (uint32_t idx, uint64_t* out_sym_rva) {
		auto& func = functions_sorted[idx];
		*out_sym_rva = func.address;
		return names[func.mangled_name];
	}
};
