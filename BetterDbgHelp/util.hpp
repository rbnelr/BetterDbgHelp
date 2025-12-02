#pragma once
#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stdio.h>
#include <assert.h>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <algorithm>
#include "timer.hpp"
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

#undef near
#undef far
#undef min
#undef max
#undef BF_BOTTOM
#undef BF_TOP
#undef ERROR


#include <stdexcept>

inline std::string print_err(const char* operation) {
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

	fprintf(stderr, "%s failed: [%lu] {\n%s}\n", operation, err, msgBuf);
	throw std::runtime_error("Win32 Error");
}

static bool ends_with(std::string_view str, std::string_view suffix) {
	return str.size() >= suffix.size() && str.compare(str.size()-suffix.size(), suffix.size(), suffix) == 0;
}
