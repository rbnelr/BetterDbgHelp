#pragma once
#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stdio.h>
#include <assert.h>
#include <string>
#include <string_view>
#include <vector>
//#include <unordered_map>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <fstream>

#include "ankerl/unordered_dense.h"

#include "tracy/Tracy.hpp"

#include "timer.hpp"
#include "logger.hpp"
using namespace kiss;

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#ifndef WIN32_NOMINMAX
	#define WIN32_NOMINMAX
#endif
#ifndef NOMINMAX
	#define NOMINMAX
#endif

#include <windows.h>

//#undef near
//#undef far
//#undef min
//#undef max
//#undef BF_BOTTOM
//#undef BF_TOP
//#undef ERROR

inline void print_err(const char* operation) {
	auto err = GetLastError();

	LPSTR msgBuf = nullptr;

	DWORD size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM     |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&msgBuf,
		0,
		nullptr);

	logf("%s failed: [%lu] {\n%s}\n", operation, err, msgBuf);
}
inline std::string print_err_throw(const char* operation) {
	print_err(operation);
	throw std::runtime_error("Win32 Error");
}

inline bool ends_with(std::string_view str, std::string_view suffix) {
	return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}

template <typename K, typename V, typename K2>
inline V* try_get (std::unordered_map<K, V>& map, K2 const& key) {
	auto it = map.find(key);
	if (it == map.end())
		return nullptr;
	return &it->second;
}

template <typename K, typename V, typename K2>
inline V* try_get (ankerl::unordered_dense::map<K, V>& map, K2 const& key) {
	auto it = map.find(key);
	if (it == map.end())
		return nullptr;
	return &it->second;
}

inline bool load_file (std::string const& filepath, std::vector<char>* out_data) {
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

	bool open (std::filesystem::path const& filepath) {
		HANDLE file_handle = CreateFileW(
			filepath.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr);
		if (file_handle != INVALID_HANDLE_VALUE) {
			file_mapping_handle = CreateFileMappingW(
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
