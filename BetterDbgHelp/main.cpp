#include "util.hpp"
#include <functional>

#include <psapi.h>
#pragma comment(lib, "Kernel32.lib")

#include "dbghelp.hpp"
#include "sym_resolver.hpp"

class SymTesting {
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
		
		void* get_ptr (std::string_view name_suffix) {
			for (auto& m : list) {
				if (ends_with(m.path, name_suffix)) {
					return m.addr;
				}
			}
			throw std::runtime_error(std::string(name_suffix) + " not found");
		}
	};

	LoadedModules loaded_modules;

	std::unique_ptr<Debughelp> dbghelp;
	std::unique_ptr<SymResolver> resolver;
	
	void start_debugging_child_process (std::string const& exe_filepath, float max_run_time) {
		// Start exe as child process with DEBUG_ONLY_THIS_PROCESS
		// let it run until it exits on its own or until max run time is reached
		// meanwhile react to module load events to record their names and addresses
		// then leave process suspended so we can simulate symbol resolving

		std::filesystem::path path = exe_filepath;
		std::string working_dir = path.has_parent_path() ? path.parent_path().u8string() : ".";

		if (!CreateProcessA(exe_filepath.c_str(), NULL, NULL, NULL, FALSE,
				DEBUG_ONLY_THIS_PROCESS | CREATE_NEW_CONSOLE,
				NULL, working_dir.c_str(), &si, &pi)) {
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
	SymTesting (std::string const& exe_filepath, float max_run_time = 2.0f) {
		printf("Starting %s\n", exe_filepath.c_str());
		start_debugging_child_process(exe_filepath, max_run_time);
		dbghelp = std::make_unique<Debughelp>(pi.hProcess);
		resolver = std::make_unique<SymResolver>(pi.hProcess);
	}

	~SymTesting () {
		dbghelp = nullptr;
		resolver = nullptr;

		printf("Killing process\n");
		finish_debugging_and_kill_child_process();

	}

	char* get_addr (std::string_view name) {
		return (char*)loaded_modules.get_ptr(name);
	}
	void show_addr2sym (char* addr) {
		dbghelp->show_addr2sym(addr);
		resolver->show_addr2sym(addr);
	}
	void measure_addr2sym (char* addr) {
		dbghelp->measure_addr2sym(addr);
		resolver->measure_addr2sym(addr);
	}

	template <typename FUNC>
	void run_examples_addresses (FUNC run_examples) {
		using std::placeholders::_1;
		std::function<void(char*)> fshow = std::bind(&SymTesting::show_addr2sym, this, _1);
		std::function<void(char*)> fmeas = std::bind(&SymTesting::measure_addr2sym, this, _1);

		run_examples(fshow);

		for (int i=0; i<1000; i++) {
			run_examples(fmeas);
		}
		dbghelp->print_timings();
		printf("---\n");
		resolver->print_timings();
	}
	void warmup (char* addr) {
		dbghelp->warmup_addr2sym(addr);
		resolver->warmup_addr2sym(addr);
	}
};

int main(int argc, const char** argv) {

	try {
		SymTesting sym("TinyProgram.exe", 0.5f);

		char* exe = sym.get_addr(".exe");
		char* ucrtbase = sym.get_addr("ucrtbase.dll");
		
		sym.warmup(exe + 0x21F0);
		sym.run_examples_addresses([=] (std::function<void(char*)> at_addr) {
			at_addr(exe + 0);
			at_addr(exe + 5);

			at_addr(exe + 0x21F0); // main()
			at_addr(exe + 0x2219); // main() printf call

			at_addr(exe + 0x24D0); // print()

			at_addr(exe + 0x2480); // fib()
			at_addr(exe + 0x2488); // fib() test ecx,ecx
			at_addr(exe + 0x2493); // fib() ret
			at_addr(exe + 0x24CA); // past fib()

			at_addr(exe + 0x2400); // fib_iter()
			at_addr(exe + 0x2414); // fib_iter() mov instr

			at_addr(exe + 0x23A0); // sqrt()
			at_addr(exe + 0x23EA); // sqrt() return
	
			at_addr(exe + 0x22A0); // inlining()
			at_addr(exe + 0x22B9); // inlining() addps
			at_addr(exe + 0x22F6); // inlining() sprintf_s
			at_addr(exe + 0x22A0 + 75); // inlining()
			at_addr(exe + 0x22A0 + 20); // inlining()
			at_addr(exe + 0x22A0 + 25); // inlining()
			at_addr(exe + 0x22A0 + 30); // inlining()
			at_addr(exe + 0x22A0 + 40); // inlining()
			at_addr(exe + 0x22A0 + 50); // inlining()

			at_addr(ucrtbase + 0x1B370); // __stdio_common_vfprintf
		});
	} catch (std::exception& err) { fprintf(stderr, "!! Exception: %s\n", err.what()); }
	Sleep(1000);
	
	try {
		SymTesting sym("CityBuilderExample/city_builder_rel.exe");
		
		char* exe = sym.get_addr(".exe");
		char* assimp = sym.get_addr("assimp-vc143-mt.dll");
		char* ucrtbase = sym.get_addr("ucrtbase.dll");
		
		sym.warmup(exe + 0x21FA0);
		sym.run_examples_addresses([=] (std::function<void(char*)> at_addr) {
			at_addr(exe + 0);
			at_addr(exe + 5);

			at_addr(exe + 0x21FA0); // main()
	
			at_addr(exe + 0x2C1E0); // json_load
			at_addr(exe + 0x2C1F4); // json_load - save.load_graphics_settings
			at_addr(exe + 0x37810); // nlohmann
			at_addr(exe + 0x37861); // array = create<array_t>(); - mov ecx,18h
			at_addr(exe + 0x8D7B0); // operator new D:\a\_work\1\s\src\vctools\crt\vcstartup\src\heap\new_scalar.cpp
			at_addr(exe + 0x30FC2); // load
			at_addr(exe + 0x30FC2+1); // load
			at_addr(exe + 0x30FC2+5); // load
			at_addr(exe + 0x30FC2+8); // load
			at_addr(exe + 0x30FC2+12); // load
			at_addr(exe + 0x30FC2+15); // load
	
			at_addr(exe + 0x12ECF1); // clac_seg lambda + inline rotate90_right
			at_addr(exe + 0x12EDB6+4); // clac_seg lambda + inline pick + get_dir_to_node
			at_addr(exe + 0x12EDB6+6); // clac_seg lambda + inline pick + get_dir_to_node
			at_addr(exe + 0x12EDB6+8); // clac_seg lambda + inline pick + get_dir_to_node
			at_addr(exe + 0x12EDB6+10); // clac_seg lambda + inline pick + get_dir_to_node
	
			at_addr(assimp + 0x23990); // assimp
	
			at_addr(ucrtbase + 0x1B370); // ucrtbase.dll!__stdio_common_vfprintf
		});

	} catch (std::exception& err) { fprintf(stderr, "!! Exception: %s\n", err.what()); }
	Sleep(1000);

	try {
		SymTesting sym("RustBevyExample/rust_bevy_test.exe");

		char* exe = sym.get_addr(".exe");
		char* ucrtbase = sym.get_addr("ucrtbase.dll");
		
		sym.warmup(exe + 0x3011B80);
		sym.run_examples_addresses([=] (std::function<void(char*)> at_addr) {
			at_addr(exe + 0);
			at_addr(exe + 5);

			at_addr(exe + 0x3011B80); // main()

			at_addr(exe + 0x3008AE0); // update_cubes_dyn
			at_addr(exe + 0x3008B2A); // update_cubes_dyn info_span!
			at_addr(exe + 0x3008BB4); // update_cubes_dyn .to_radians()
	
			at_addr(exe + 0x2FC20D0); // par_iter_mut follow_waves
			at_addr(exe + 0x2FC20D0+10); // par_iter_mut follow_waves
	
			at_addr(exe + 0x2DB2BF0); // rand_chacha::guts::refill_wide
			at_addr(exe + 0x2DB2BF0+21); // rand_chacha::guts::refill_wide
	
			//at_addr(ucrtbase + 0x69FB30); // ucrtbase.dll!sinf(), returns wrong symbol for some reason, I even double checked everything, am I missing something?
			at_addr(ucrtbase + 0x1B370); // ucrtbase.dll!__stdio_common_vfprintf, weirdly this one works, so it's even the same ucrtbase.dll as the two other executables
		});
	} catch (std::exception& err) { fprintf(stderr, "!! Exception: %s\n", err.what()); }
	Sleep(1000);
	
	return 0;
}
