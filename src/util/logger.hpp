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
		vfprintf(stdout, format, args);
		vfprintf(log_file, format, args);
	}
};

inline Logger g_logger("output.txt");

inline void logf (const char* format, ...) {
	va_list args;
	va_start(args, format);

	g_logger.vlogf(format, args);

	va_end(args);
}
