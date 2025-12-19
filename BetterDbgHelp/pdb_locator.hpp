#pragma once
#include "util.hpp"

#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <Urlmon.h>
#pragma comment(lib, "Urlmon.lib")
//#include <Wininet.h>
//#pragma comment(lib, "Wininet.lib")

// https://www.jeremyong.com/winapi/io/2024/11/03/windows-memory-mapped-file-io/
class MemoryMappedFile {
	HANDLE file_mapping_handle = INVALID_HANDLE_VALUE;
	void* _data = nullptr;
	
	friend void swap (MemoryMappedFile& l, MemoryMappedFile& r) {
		std::swap(l.file_mapping_handle, r.file_mapping_handle);
		std::swap(l._data, r._data);
	}
	MemoryMappedFile& operator= (MemoryMappedFile& r) = delete;
	MemoryMappedFile (MemoryMappedFile& r) = delete;
	MemoryMappedFile& operator= (MemoryMappedFile&& r) {	swap(*this, r);	return *this; }
	MemoryMappedFile (MemoryMappedFile&& r) {				swap(*this, r); }

public:
	void* data () { return _data; }

	bool open (const char* filepath) {
		HANDLE file_handle = CreateFileA(
			filepath,
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr);
		if (file_handle != INVALID_HANDLE_VALUE) {
			file_mapping_handle = CreateFileMappingA(
				file_handle,
				nullptr,
				PAGE_READONLY,
				// Passing zeroes for the high and low max-size params here will allow the
				// entire file to be mappable.
				0,
				0,
				nullptr);

			// We can close this now because the file mapping retains an open handle to
			// the underlying file.
			CloseHandle(file_handle);

			if (file_mapping_handle != NULL) {
				_data = MapViewOfFile(
					file_mapping_handle,
					FILE_MAP_READ,
					0, // Offset high
					0, // Offset low
					// A zero here indicates we want to map the entire range.
					0);

				if (_data != NULL) {
					return true;
				}
			}
		}
		return false;
	}

	MemoryMappedFile () {}
	~MemoryMappedFile () {
		if (file_mapping_handle != INVALID_HANDLE_VALUE) {
			// When we are done, closing the file mapping handle releases the file for use
			// by other applications.
			UnmapViewOfFile(_data);
			CloseHandle(file_mapping_handle);
		}
	}
};

// The executable (.exe or .dll) itself contains data which can be used to find the associated pdb file
// commonly when building your own application or when sometimes when downloading application, a pdb with the same name as the exe will simply be placed next to the exe
// TODO: explain where pdbs end up being pulled from
class PDB_Locator {
	MemoryMappedFile file;
	
	struct RSDSI { // RSDS debug info
		DWORD   dwSig; // RSDS
		GUID    guidSig;
		DWORD   age;
		char    pdb_name[1];
	};

	IMAGE_DOS_HEADER* dos_header = {};
	IMAGE_NT_HEADERS* nt_header = {}; // includes optional header
	IMAGE_SECTION_HEADER* section_headers = {};
	RSDSI* rsds = {};

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

	void open_image_and_find_rsds (std::string const& filepath) {
		if (!file.open(filepath.c_str()))
			throw std::runtime_error("File not found: "+ filepath);

		dos_header = (IMAGE_DOS_HEADER*)file.data();
		if (dos_header->e_magic != 0x5A4D) // MZ
			throw std::runtime_error("Error parsing image: "+ filepath);
		
		nt_header = (IMAGE_NT_HEADERS*)((char*)file.data() + dos_header->e_lfanew);
		if (nt_header->Signature != 0x00004550) // PE\0\0
			throw std::runtime_error("Error parsing image: "+ filepath);
		
		section_headers = (IMAGE_SECTION_HEADER*)((char*)file.data() + dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS));

		auto dbg = nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
		auto* debug_directory = (IMAGE_DEBUG_DIRECTORY*)map_rva(dbg.VirtualAddress);
		auto num_debug_directory = dbg.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
		for (int i=0; i<num_debug_directory; i++) {
			auto& d = debug_directory[i];

			if (d.Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
				rsds = (RSDSI*)((char*)file.data() + d.PointerToRawData);
				if (rsds->dwSig != 0x53445352) // "RSDS"
					throw std::runtime_error("Error parsing image: "+ filepath);

				return;
			}
		}
	}
	
	std::filesystem::path exe_path;
	std::filesystem::path pdb_path_in_exe;
	std::string guid_age_str;
	
	std::string symbol_server_url;
	std::filesystem::path cache_path;

	std::string get_guid_age_str () {
		auto& guid = rsds->guidSig;

		char guid_age_str[128];
		// guid in hex + age (is this also hex?)
		sprintf_s(guid_age_str, "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%X",
			guid.Data1, guid.Data2, guid.Data3,
			guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7],
			rsds->age);

		return guid_age_str;
	}
	
	std::string get_symbol_server_url () {
		auto pdb_name = pdb_path_in_exe.filename().u8string();

		return "https://msdl.microsoft.com/download/symbols/"+ pdb_name +"/"+ guid_age_str +"/"+ pdb_name;
	}
	std::filesystem::path get_cache_path () {
		auto pdb_name = pdb_path_in_exe.filename().u8string();
		// hardcode cache temp, I'm not sure if there's a more correct way to come up with this
		// supposedly the _NT_SYMBOL_PATH env var exists, but apparently that's not what Visual Studio used, so screw that

		// This cache often will contain no pdb at this path, but another subfolder called "stripped" which contains the file, presumably with just some functions names left in
		auto path = std::filesystem::temp_directory_path() / "SymbolCache" / pdb_name / guid_age_str / pdb_name;
		return path;
	}

	std::filesystem::path try_downloading_pdb_if_not_in_cache () {
		if (std::filesystem::exists(cache_path)) {
			return cache_path;
		}

		std::filesystem::create_directories(cache_path.parent_path());

		if (URLDownloadToFileA(NULL, symbol_server_url.c_str(), cache_path.u8string().c_str(), 0, NULL) == S_OK) {
			return cache_path;
		}
		print_err("URLDownloadToFileA");

		// Alternative:
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

		logf("Could not download %s!\n", cache_path.u8string().c_str());
		return {};
	}

public:
	PDB_Locator (std::string const& filepath) {
		exe_path = filepath;

		open_image_and_find_rsds(filepath);

		pdb_path_in_exe = std::string( (const char*)rsds->pdb_name );
		guid_age_str = get_guid_age_str();

		symbol_server_url = get_symbol_server_url();
		cache_path = get_cache_path();

		//printf(">> %s ->\n>>> symbol_server_url: %s\n>>> cache_path: %s\n", filepath.c_str(), symbol_server_url.c_str(), cache_path.u8string().c_str());
	}

	std::filesystem::path get_pdb_path () {
		if (pdb_path_in_exe.is_absolute()) {
			if (std::filesystem::exists(pdb_path_in_exe)) {
				return pdb_path_in_exe;
			}
		}
		
		{
			std::filesystem::path pdb_path = exe_path;
			pdb_path.replace_extension({".pdb"});

			if (std::filesystem::exists(pdb_path)) {
				return pdb_path;
			}
		}

		auto cached_pdb_path = try_downloading_pdb_if_not_in_cache();
		if (!cached_pdb_path.empty()) {
			return cached_pdb_path;
		}

		throw std::runtime_error("Cannot locate pdb for: "+ exe_path.u8string());
	}
};
