#pragma once
#include <stdio.h>
#include <stdarg.h>

class Logger {
	FILE* log_file = {};

public:
	Logger (const char* filepath) {
		log_file = fopen(filepath, "w");
	}
	~Logger () {
		if (log_file) {
			fclose(log_file);
		}
	}

	void vlogf (const char* format, va_list args) {
		// Cannot reuse va_list!, need to use va_copy
		va_list args2;
		va_copy(args2, args);

		vfprintf(stdout, format, args);
		fflush(stdout);

		va_end(args2);
		
		vfprintf(log_file, format, args);
		fflush(log_file);
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
