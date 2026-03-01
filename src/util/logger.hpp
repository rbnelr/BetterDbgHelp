#pragma once
#include <stdio.h>
#include <stdarg.h>

class Logger {
	const char* filepath = nullptr;
	FILE* log_file = {};

	__declspec(noinline) void open () {
		fprintf(stdout, "Opening log file %s.\n", filepath);

		log_file = fopen(filepath, "w");
	}

public:
	Logger (const char* filepath): filepath{filepath} {
		
	}
	~Logger () {
		if (log_file) {
			fclose(log_file);
		}
	}

	void vlogf (const char* format, va_list args) {
		if (!log_file && filepath) {
			open();
		}

		// Cannot reuse va_list!, need to use va_copy
		va_list args2;
		va_copy(args2, args);

		vfprintf(stdout, format, args);
		fflush(stdout);

		if (log_file) {
			vfprintf(log_file, format, args2);
			fflush(log_file);
		}

		va_end(args2);
	}
};

// define elsewhere
extern Logger g_logger;

inline void logf (const char* format, ...) {
	va_list args;
	va_start(args, format);

	g_logger.vlogf(format, args);

	va_end(args);
}
