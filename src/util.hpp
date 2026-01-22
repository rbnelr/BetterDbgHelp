#pragma once
//#ifndef _CRT_SECURE_NO_WARNINGS
//	#define _CRT_SECURE_NO_WARNINGS 1
//#endif

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

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

#include "util/logger.hpp"
#include "util/timer.hpp"
using namespace kiss;

#include "ankerl/unordered_dense.h"

#include "tracy/Tracy.hpp"

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

#define TRACY_MEMORY_PROFILING 1

#if TRACY_MEMORY_PROFILING
// Track standard memory allocation via tracy
// (VirtualAlloc is tracked seperately)
void* operator new (std::size_t count) {
	auto ptr = malloc(count);
	TracyAlloc(ptr, count);
	return ptr;
}
void operator delete (void* ptr) noexcept {
	TracyFree(ptr);
	free(ptr);
}
#endif

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

inline std::wstring str2wstr (const std::string& str) {
	int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstr(size_needed, L'\0');
	auto res = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), wstr.data(), size_needed);
	if (res <= 0) {
		return std::wstring();
	}
	return wstr;
}

std::wstring GetEnvVar (const wchar_t* name) {
	DWORD len = GetEnvironmentVariableW(name, nullptr, 0);
	if (len == 0) {
		return L"";
	}

	std::wstring value(len, L'\0');
	DWORD written = GetEnvironmentVariableW(name, &value[0], len);
	if (written == 0 || written >= len) {
		return L"";
	}

	value.resize(written);
	return value;
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
	
	// Move-only class
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

#if 1
class VirtualMemoryVector {
	char* _data = nullptr;
	int32_t _size = 0;
	int32_t _capacity = 0;
public:

	static inline constexpr size_t MAX_CAPACITY = 1llu << 31;
	static inline constexpr size_t GROW_BLOCK_SIZE = 1024*32;

	const char* data () const { return _data; }
	char* data () { return _data; }
	int32_t size () const { return _size; }
	
	VirtualMemoryVector () {}
	// need this to properly support move semantics because move semantics are designed badly
	void init () {
		_data = (char*)VirtualAlloc(nullptr, MAX_CAPACITY, MEM_RESERVE, PAGE_NOACCESS);
		_realloc(GROW_BLOCK_SIZE);
	}
	~VirtualMemoryVector () {
		if (_data) {
		#if TRACY_MEMORY_PROFILING
			//if (_capacity > 0) { TracyFree(_data); }
			size_t offs = 0;
			while (offs < _capacity) {
				TracyFree(_data + offs);
				offs += GROW_BLOCK_SIZE;
			}
		#endif
			VirtualFree(_data, 0, MEM_RELEASE);
		}
	}

	// Move-only class
	friend void swap (VirtualMemoryVector& l, VirtualMemoryVector& r) {
		std::swap(l._data, r._data);
		std::swap(l._size, r._size);
		std::swap(l._capacity, r._capacity);
	}
	VirtualMemoryVector& operator= (VirtualMemoryVector& r) = delete;
	VirtualMemoryVector (VirtualMemoryVector& r) = delete;
	VirtualMemoryVector& operator= (VirtualMemoryVector&& r) {	swap(*this, r);	return *this; }
	VirtualMemoryVector (VirtualMemoryVector&& r) {				swap(*this, r); }

	__declspec(noinline) void _realloc (int32_t min_capacity) {
		auto new_capacity = (min_capacity + GROW_BLOCK_SIZE-1) & ~(GROW_BLOCK_SIZE-1);
		
	#if TRACY_MEMORY_PROFILING
		//if (_capacity > 0) { TracyFree(_data); }
		//TracyAlloc(_data, new_capacity);
		size_t offs = _capacity;
		while (offs < new_capacity) {
			TracyAlloc(_data + offs, GROW_BLOCK_SIZE);
			offs += GROW_BLOCK_SIZE;
		}
	#endif

		VirtualAlloc(_data + _capacity, new_capacity-_capacity, MEM_COMMIT, PAGE_READWRITE);
		_capacity = (int32_t)new_capacity;
	}
	
	__forceinline int32_t _grow_to_align (size_t size, size_t align) {
		auto offset = (size_t)_size;
		auto padded_offset = (offset + align-1) & ~(align-1);
		auto padding_size = padded_offset - offset;

		auto new_size = padded_offset + size;
		assert(new_size <= MAX_CAPACITY);

		if (new_size > _capacity) {
			_realloc((int32_t)new_size);
		}

		_size = (int32_t)new_size;

		if (padding_size > 0)
			memset(_data + padded_offset, 0, padding_size); // Always pad with zeros
		
		return (int32_t)padded_offset;
	}

	__forceinline int32_t _grow_noalign (size_t size, size_t align) {
		auto offset = (size_t)_size;
		assert((offset % align) == 0);

		auto new_size = offset + size;
		assert(new_size <= MAX_CAPACITY);

		if (new_size > _capacity) {
			_realloc((int32_t)new_size);
		}

		_size = (int32_t)new_size;
		
		return (int32_t)offset;
	}
};
typedef VirtualMemoryVector MemoryVector;
#else
class MemoryVector {
	std::vector<char> vec;
public:
	static inline constexpr size_t MAX_CAPACITY = 1llu << 31;

	const char* data () const { return vec.data(); }
	char* data () { return vec.data(); }
	int32_t size () const { return (int32_t)vec.size(); }
	
	MemoryVector () {}
	void init () {
		vec.reserve(1024*32);
	}

	__declspec(noinline) void _realloc (int32_t min_capacity) {
		vec.reserve(min_capacity);
	}
	
	__forceinline int32_t _grow_to_align (size_t size, size_t align) {
		auto offset = vec.size();
		auto padded_offset = (offset + align-1) & ~(align-1);
		auto padding_size = padded_offset - offset;

		auto new_size = padded_offset + size;
		assert(new_size <= MAX_CAPACITY);

		vec.resize((int32_t)new_size);

		if (padding_size > 0)
			memset(vec.data() + padded_offset, 0, padding_size); // Always pad with zeros
		
		return (int32_t)padded_offset;
	}

	__forceinline int32_t _grow_noalign (size_t size, size_t align) {
		auto offset = vec.size();
		assert((offset % align) == 0);

		auto new_size = offset + size;
		assert(new_size <= MAX_CAPACITY);

		vec.resize((int32_t)new_size);
		
		return (int32_t)offset;
	}
};
#endif

struct StrAlloc {
	typedef int32_t sid; // Strbuf index
	MemoryVector v;

	void init () {
		v.init();
	}
	
	size_t size () const {
		return v.size();
	}
	
	char* operator[] (sid offset) {
		return v.data() + offset;
	}
	const char* operator[] (sid offset) const {
		return v.data() + offset;
	}
	
	static_assert(sizeof(sid) == sizeof(int32_t));
	sid _grow (size_t len) {
		return (sid)v._grow_noalign(len, 1);
	}
	sid get_offset () {
		return (sid)v.size();
	}
	
	// push null-terminated
	sid push (const char* str) {
		return push(str, strlen(str));
	}
	// push substr (non-null-terminated substr, will be null-terminated on write)
	sid push (const char* str, size_t len) {
		sid offset = _grow(len+1);
		auto* cur = v.data() + offset;

		memcpy(cur, str, len);
		cur += len;
		*cur++ = '\0';

		return offset;
	}
	
	// concat null-terminated strings
	sid push_concat (const char* a, const char* b, const char* c) {
		auto la = strlen(a);
		auto lb = strlen(b);
		auto lc = strlen(c);
		
		sid offset = _grow(la+lb+lc+1);
		auto* cur = v.data() + offset;

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
		auto* cur = v.data() + offset;

		memcpy(cur, scope, len); cur += len;
		*cur++ = ':';
		*cur++ = ':';

		return offset;
	}

	sid push_bytes (const void* ptr, size_t len) {
		sid offset = _grow(len);
		auto* cur = v.data() + offset;

		memcpy(cur, ptr, len);
		cur += len;

		return offset;
	}

	sid push_format (const char* format, ...) {
		va_list vl;
		va_start(vl, format);

		std::string ret;

		int count = vsnprintf(nullptr, 0, format, vl);
		if (count < 0)
			return -1;

		sid offset = _grow(count+1);
		auto* cur = v.data() + offset;

		int count2 = vsnprintf(cur, count+1, format, vl);
		if (count2 < 0)
			return -1;
		assert(count2 == count);
		assert(cur[count] == '\0');

		va_end(vl);

		return offset;
	}

	void print_dump () const {
		logf("StrAlloc:\n");

		char const* cur = v.data();
		char const* end = cur + v.size();

		while (cur < end) {
			logf("> [%8llx] %s\n", cur - v.data(), cur);
			cur += strlen(cur)+1;
		}
	}
};

// Use memory reservation to push data with having to copy on grow
// gives a very good pdb parsing speedup, added benefit of stable pointers, but I'm not sure is smart to utilize that
struct BinAlloc {
	typedef int32_t bid;
	
	MemoryVector v;

	void init () {
		v.init();
	}

	size_t size () const {
		return v.size();
	}
	
	template <typename T>
	__forceinline T* _push (bid* out_offset) {
		//static_assert(std::is_trivial_v<T>);
		static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>);

		bid offset = v._grow_to_align(sizeof(T), alignof(T));
		T* ptr = (T*)(v.data() + offset);

		*out_offset = offset;
		return ptr;
	}
	
	template <typename T>
	__forceinline bool is_aliged () {
		return ((size_t)v.size() % alignof(T)) == 0;
	}
	template <typename T>
	__forceinline T* _push_noalign (bid* out_offset) {
		//static_assert(std::is_trivial_v<T>);
		static_assert(std::is_standard_layout_v<std::remove_reference_t<T>>);

		bid offset = v._grow_noalign(sizeof(T), alignof(T));
		T* ptr = (T*)(v.data() + offset);

		*out_offset = offset;
		return ptr;
	}
	template <typename T>
	bid push_noalign (T const& data) {
		bid offset;
		auto* ptr = _push_noalign<T>(&offset);

		*ptr = data;
		return offset;
	}

	// get offset returned by next push<T>
	// so aligned offset for T
	// does not actually push yet!
	template <typename T>
	bid prepare_push () {
		//static_assert(std::is_trivial_v<T>);
		static_assert(std::is_standard_layout_v<T>);

		bid offset = v._grow_to_align(0, alignof(T));
		return offset;
	}
	
	template <typename T>
	bid push (T&& data) {
		bid offset;
		auto* ptr = _push<std::remove_reference_t<T>>(&offset);

		//memcpy(cur, data, sizeof(T));
		*ptr = data;
		return offset;
	}
	template <typename T>
	bid push (T const& data) {
		bid offset;
		auto* ptr = _push<T>(&offset);

		//memcpy(cur, data, sizeof(T));
		*ptr = data;

		return offset;
	}

	bid push_bytes (const void* ptr, size_t len) {
		bid offset = v._grow_to_align(len, 1);
		auto* cur = v.data() + offset;

		memcpy(cur, ptr, len);

		return offset;
	}
	
	template <typename T>
	__forceinline T* get (bid offset) const {
		if (offset >= 0) {
			assert(offset % alignof(T) == 0);
			return (T*)(v.data() + offset);
		}
		return nullptr;
	}
	template <typename T>
	__forceinline T* get_unchecked (bid offset) const {
		assert(offset >= 0);
		assert(offset % alignof(T) == 0);
		return (T*)(v.data() + offset);
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
