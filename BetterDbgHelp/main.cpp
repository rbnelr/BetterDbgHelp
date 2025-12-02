#include <stdio.h>
#include <assert.h>
#include <string>
#include <vector>
#include <stdexcept>
#include "timer.hpp"
using namespace kiss;

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "Kernel32.lib")

std::string print_err(const char* operation) {
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

	printf("%s failed: [%lu] {\n%s}\n", operation, err, msgBuf);
	throw std::runtime_error("Win32 Error");
}

class SymbolResolver {
	STARTUPINFOA si{};
	PROCESS_INFORMATION pi{};
	
	DEBUG_EVENT de;
	
	struct LoadedModule {
		std::string path;
		void* addr; // Virtual memory address module was loaded at in child process
	};
	struct LoadedModules {
		std::vector<LoadedModule> list;

		void add (std::string path, void* addr) {
			list.push_back({ path, addr });
		}
	};

	LoadedModules loaded_modules;
	
	void start_debugging_child_process (std::string const& exe_filepath, float max_run_time) {
		// Start exe as child process with DEBUG_ONLY_THIS_PROCESS
		// let it run until it exits on its own or until max run time is reached
		// meanwhile react to module load events to record their names and addresses
		// then leave process suspended so we can simulate symbol resolving

		if (!CreateProcessA(exe_filepath.c_str(), NULL, NULL, NULL, FALSE,
				DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE,
				NULL, NULL, &si, &pi)) {
			print_err("CreateProcess");
		}

		auto timer = Timer::start();
		DWORD timeout = 100; // 100 ms polling

		for (;;) {
			de = {};
			auto res = WaitForDebugEvent(&de, timeout);
			if (!res && GetLastError() != ERROR_SEM_TIMEOUT) {
				print_err("WaitForDebugEvent\n");
			}

			// check time elapsed both on normal events and timeouts
			if (timer.elapsed_sec() > max_run_time) {
				// Child process stil running but max time elapsed
				break; // don't call ContinueDebugEvent, effectively suspended
			}

			if (!res) { // timeout but still time remaining, we effectively polled withot an event coming in
				continue; // continue polling or reacting to events
			}
			
			assert(de.dwProcessId == pi.dwProcessId);

			if (de.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
				//printf("Exited\n");
				// Not sure if actually exited completely or stuck just before exiting
				break; // don't call ContinueDebugEvent, effectively suspended
			}
			switch (de.dwDebugEventCode) {
				case CREATE_PROCESS_DEBUG_EVENT: {
					HANDLE hFile = de.u.CreateProcessInfo.hFile;
					LPVOID addr  = de.u.CreateProcessInfo.lpBaseOfImage;

					char name[1024] = {};
					GetFinalPathNameByHandleA(hFile, name, sizeof(name), FILE_NAME_NORMALIZED);
					loaded_modules.add(std::string(name), addr);

					//printf("CREATE_PROCESS_DEBUG_EVENT: Module \"%50s\" at %p\n", name, addr);

					CloseHandle(hFile);
					de.u.CreateProcessInfo.hFile = NULL;
				} break;
				case LOAD_DLL_DEBUG_EVENT: {
					HANDLE hFile = de.u.LoadDll.hFile;
					LPVOID addr  = de.u.LoadDll.lpBaseOfDll;

					char name[1024] = {};
					GetFinalPathNameByHandleA(hFile, name, sizeof(name), FILE_NAME_NORMALIZED);
					loaded_modules.add(std::string(name), addr);

					//printf("LOAD_DLL_DEBUG_EVENT:       Module \"%50s\" at %p\n", name, addr);

					CloseHandle(hFile);
					de.u.LoadDll.hFile = NULL;
				} break;
				case CREATE_THREAD_DEBUG_EVENT: {
					//printf("CREATE_THREAD_DEBUG_EVENT:\n");
				} break;
				case EXIT_THREAD_DEBUG_EVENT: {
					//printf("EXIT_THREAD_DEBUG_EVENT:\n");
				} break;
				default: {
					//printf("Other event [%d]\n", de.dwDebugEventCode);
				} break;
			}

			ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
		}
	}
	void finish_debugging_and_kill_child_process () {
		// Tell the process to die
		TerminateProcess(pi.hProcess, 0);
		
		// But unless it actually exited on its own, the debugging api does not allow you to just stop the session for some dumb reason
		if (de.dwDebugEventCode != EXIT_PROCESS_DEBUG_EVENT) {
			// Need to continue from the event we left off (this is needed even if we had a timeout previously, why?)
			ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);

			// Net to receive all the dll unloaded events etc. until finally seeing the EXIT_PROCESS_DEBUG_EVENT
			// or else the debugging session never stops and future debugging sessions return events from this old process
			for (;;) {
				de = {};
				if (!WaitForDebugEvent(&de, INFINITE)) {
					print_err("WaitForDebugEvent\n");
				}

				assert(de.dwProcessId == pi.dwProcessId);
				if (de.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
					break;
				}
				ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
			}
		}
		DebugActiveProcessStop(de.dwProcessId); // not sure this even does anything (probably needed for attached debuggers)

		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
public:
	char* exe_base_addr = nullptr;

	SymbolResolver (std::string const& exe_filepath, float max_run_time = 2.0f) {
		printf("Starting %s\n", exe_filepath.c_str());
		start_debugging_child_process(exe_filepath, max_run_time);
	}

	~SymbolResolver () {
		printf("Killing process\n");
		finish_debugging_and_kill_child_process();
	}

	void addr2loc (char* addr) {
		
	}
};

int main(int argc, const char** argv) {

	try {
		SymbolResolver sym("TinyProgram.exe", 0.5f);
		sym.addr2loc(sym.exe_base_addr + 0);
		sym.addr2loc(sym.exe_base_addr + 5);

		sym.addr2loc(sym.exe_base_addr + 0x1010); // printf()

		sym.addr2loc(sym.exe_base_addr + 0x1080); // fib()
		sym.addr2loc(sym.exe_base_addr + 0x1088); // fib() test instr
		sym.addr2loc(sym.exe_base_addr + 0x10C8); // past fib()

		sym.addr2loc(sym.exe_base_addr + 0x10D0); // fib_iter()
		sym.addr2loc(sym.exe_base_addr + 0x1114); // fib_iter() mov instr

		sym.addr2loc(sym.exe_base_addr + 0x1140); // sqrt()

		// Find a way to query ucrtbase.dll base addr so we can find sym for __stdio_common_vfprintf
	} catch (...) {}
	Sleep(1000);
	
	try {
		SymbolResolver sym("city_builder_rel.exe");
	} catch (...) {}
	Sleep(1000);

	try {
		SymbolResolver sym("rust_bevy_test.exe");
	} catch (...) {}
	Sleep(1000);
	
	return 0;
}
