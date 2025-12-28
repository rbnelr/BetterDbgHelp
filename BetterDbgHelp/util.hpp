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

struct StrAlloc {
	typedef uint32_t sid; // Strbuf index
	std::vector<char> buf;

	StrAlloc () {
		buf.reserve(1024*8);
	}

	const char* operator[] (sid offset) {
		return buf.data() + offset;
	}
	
	static_assert(sizeof(sid) == sizeof(uint32_t));
	sid _grow (size_t len) {
		auto offset = buf.size();
		buf.resize(offset + len);
		
		if (offset > 0xffffffff) {
			assert(false);
		}
		return (sid)offset;
	}
	sid get_offset () {
		auto offset = buf.size();
		if (offset > 0xffffffff) {
			assert(false);
		}
		return (sid)offset;
	}
	
	// push null-terminated
	sid push (const char* str) {
		return push(str, strlen(str));
	}
	// push substr (non-null-terminated substr, will be null-terminated on write)
	sid push (const char* str, size_t len) {
		sid offset = _grow(len+1);
		auto* cur = buf.data() + offset;

		memcpy(cur, str, len); cur += len;
		*cur++ = '\0';

		return offset;
	}
	
	// concat null-terminated strings
	sid push_concat (const char* a, const char* b, const char* c) {
		auto la = strlen(a);
		auto lb = strlen(b);
		auto lc = strlen(c);
		
		sid offset = _grow(la+lb+lc+1);
		auto* cur = buf.data() + offset;

		memcpy(cur, a, la); cur += la;
		memcpy(cur, b, lb); cur += lb;
		memcpy(cur, c, lc+1);

		return offset;
	}
	// push null terminated string + "::" but omit null-terminator to allow easy concatination
	// (followed by a normal push to terminate)
	sid push_concat_scope_no_terminate (const char* scope) {
		auto len = strlen(scope);
		
		sid offset = _grow(len + 2);
		auto* cur = buf.data() + offset;

		memcpy(cur, scope, len); cur += len;
		*cur++ = ':';
		*cur++ = ':';

		return offset;
	}
};

struct BinAlloc {
	typedef int32_t bid;
	std::vector<char> buf;

	BinAlloc () {
		buf.reserve(1024*8);
	}

	static_assert(sizeof(bid) == sizeof(uint32_t));
	__forceinline bid _grow_to_align (size_t size, size_t align) {
		auto offset = buf.size();
		offset = (offset + align-1) & ~(align-1);
		buf.resize(offset + size);
		
		if (offset > 0xffffffff) {
			assert(false);
		}
		return (bid)offset;
	}
	
	// get offset returned by next push<T>
	template <typename T>
	bid prepare_push () {
		//static_assert(std::is_trivial_v<T>);
		static_assert(std::is_standard_layout_v<T>);

		bid offset = _grow_to_align(0, alignof(T));
		return offset;
	}
	
	template <typename T>
	bid push (T&& data) {
		//static_assert(std::is_trivial_v<T>);
		static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>);

		bid offset = _grow_to_align(sizeof(T), alignof(T));
		auto* cur = buf.data() + offset;

		//memcpy(cur, data, sizeof(T));
		*(std::remove_reference_t<T>*)cur = std::move(data);

		return offset;
	}
	template <typename T>
	bid push_default () {
		return push(T());
	}

	template <typename T>
	bid push (T* data) {
		//static_assert(std::is_trivial_v<T>);
		static_assert(std::is_standard_layout_v<T>);

		bid offset = _grow_to_align(sizeof(T), alignof(T));
		auto* cur = buf.data() + offset;

		//memcpy(cur, data, sizeof(T));
		*(T*)cur = *data;

		return offset;
	}
	bid push_bytes (void* data, size_t len) {
		bid offset = _grow_to_align(len, 1);
		auto* cur = buf.data() + offset;

		memcpy(cur, data, len);

		return offset;
	}
	
	template <typename T>
	T* get (bid offset) const {
		if (offset >= 0)
			return (T*)(buf.data() + offset);
		return nullptr;
	}
};

// Small string allocator that uses a fixed amount of stack space before spilling to heap
template <size_t STACK_BUF>
struct SmallStringAlloc {
	typedef std::unique_ptr<char[]> heap_str;

	char buf[STACK_BUF];
	size_t cur = 0;
	std::vector<heap_str> large_strs;

	// push null-terminated onto stack buf if it fits, otherwise onto vector
	// return stable C string pointer
	const char* push (const char* str, size_t len) {
		size_t size = len + 1;
		size_t remain = STACK_BUF - cur;
		if (size <= remain) {
			char* res = &buf[cur];

			memcpy(res, str, len);
			res[len] = '\0';

			cur += size;
			return res;
		}
		else {
			auto heap = std::make_unique<char[]>(size);
			memcpy(heap.get(), str, len);
			heap[len] = '\0';

			auto& res = large_strs.emplace_back(std::move(heap));
			return res.get();
		}
	}

	// This cannot be moved without invalidating returned pointers
	SmallStringAlloc () = default;
	~SmallStringAlloc () = default;
	SmallStringAlloc (SmallStringAlloc&& other) = delete;
	SmallStringAlloc& operator= (SmallStringAlloc&& other) = delete;
	SmallStringAlloc (SmallStringAlloc const& other) = delete;
	SmallStringAlloc& operator= (SmallStringAlloc const& other) = delete;
};

// https://stackoverflow.com/questions/60169819/modern-approach-to-making-stdvector-allocate-aligned-memory
/**
 * Returns aligned pointers when allocations are requested. Default alignment
 * is 64B = 512b, sufficient for AVX-512 and most cache line sizes.
 *
 * @tparam ALIGNMENT_IN_BYTES Must be a positive power of 2.
 */
template<typename    ElementType,
         std::size_t ALIGNMENT_IN_BYTES = 64>
class AlignedAllocator
{
private:
    static_assert(
        ALIGNMENT_IN_BYTES >= alignof( ElementType ),
        "Beware that types like int have minimum alignment requirements "
        "or access will result in crashes."
    );

public:
    using value_type = ElementType;
    static std::align_val_t constexpr ALIGNMENT{ ALIGNMENT_IN_BYTES };

    /**
     * This is only necessary because AlignedAllocator has a second template
     * argument for the alignment that will make the default
     * std::allocator_traits implementation fail during compilation.
     * @see https://stackoverflow.com/a/48062758/2191065
     */
    template<class OtherElementType>
    struct rebind
    {
        using other = AlignedAllocator<OtherElementType, ALIGNMENT_IN_BYTES>;
    };

public:
    constexpr AlignedAllocator() noexcept = default;

    constexpr AlignedAllocator( const AlignedAllocator& ) noexcept = default;

    template<typename U>
    constexpr AlignedAllocator( AlignedAllocator<U, ALIGNMENT_IN_BYTES> const& ) noexcept
    {}

    [[nodiscard]] ElementType*
    allocate( std::size_t nElementsToAllocate )
    {
        if ( nElementsToAllocate
             > std::numeric_limits<std::size_t>::max() / sizeof( ElementType ) ) {
            throw std::bad_array_new_length();
        }

        auto const nBytesToAllocate = nElementsToAllocate * sizeof( ElementType );
        return reinterpret_cast<ElementType*>(
            ::operator new[]( nBytesToAllocate, ALIGNMENT ) );
    }

    void
    deallocate(                  ElementType* allocatedPointer,
                [[maybe_unused]] std::size_t  nBytesAllocated )
    {
        /* According to the C++20 draft n4868 § 17.6.3.3, the delete operator
         * must be called with the same alignment argument as the new expression.
         * The size argument can be omitted but if present must also be equal to
         * the one used in new. */
        ::operator delete[]( allocatedPointer, ALIGNMENT );
    }
};
