#include "util.hpp"
#include <functional>
#include <random>

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
		size_t size;
	};
	struct LoadedModules {
		std::vector<LoadedModule> list;

		void add (std::string path, void* addr, size_t size) {
			list.push_back({ path, addr, size });
		}
		
		LoadedModule const& find (std::string_view name_suffix) {
			for (auto& m : list) {
				if (ends_with(m.path, name_suffix)) {
					return m;
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
			print_err_throw("CreateProcess");
		}

		auto timer = Timer::start();
		DWORD timeout = 100; // 100 ms polling

		for (;;) {
			de = {};
			auto res = WaitForDebugEvent(&de, timeout);
			if (!res && GetLastError() != ERROR_SEM_TIMEOUT) {
				print_err_throw("WaitForDebugEvent\n");
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
				//logf("Exited\n");
				// Not sure if actually exited completely or stuck just before exiting
				break; // don't call ContinueDebugEvent, effectively suspended
			}

			auto handle_loaded_image = [&] (HANDLE hFile, void* baseAddr) {
				char name[1024] = {};
				GetFinalPathNameByHandleA(hFile, name, sizeof(name), FILE_NAME_NORMALIZED);

				HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
				if (hMapping) {
					LPVOID pView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
					if (pView) {
						PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)pView;
						PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)pView + dosHeader->e_lfanew);
						DWORD imageSize = ntHeaders->OptionalHeader.SizeOfImage;
						
						loaded_modules.add(std::string(name), baseAddr, imageSize);
						
						UnmapViewOfFile(pView);
					}
					CloseHandle(hMapping);
				}
				CloseHandle(hFile);
			};
			switch (de.dwDebugEventCode) {
				case CREATE_PROCESS_DEBUG_EVENT: {
					HANDLE hFile = de.u.CreateProcessInfo.hFile;
					LPVOID addr  = de.u.CreateProcessInfo.lpBaseOfImage;
					handle_loaded_image(hFile, addr);
				} break;
				case LOAD_DLL_DEBUG_EVENT: {
					HANDLE hFile = de.u.LoadDll.hFile;
					LPVOID addr  = de.u.LoadDll.lpBaseOfDll;
					handle_loaded_image(hFile, addr);
				} break;
				case CREATE_THREAD_DEBUG_EVENT: {
					//logf("CREATE_THREAD_DEBUG_EVENT:\n");
				} break;
				case EXIT_THREAD_DEBUG_EVENT: {
					//logf("EXIT_THREAD_DEBUG_EVENT:\n");
				} break;
				default: {
					//logf("Other event [%d]\n", de.dwDebugEventCode);
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
					print_err_throw("WaitForDebugEvent\n");
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

	std::default_random_engine init_rng (uint32_t seed) {
		return std::default_random_engine(seed);
	}
	std::default_random_engine init_rng () {
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		auto seed = std::hash<uint64_t>()(li.QuadPart);

		return init_rng((uint32_t)seed);
	}
public:
	//bool tests_failed = false;
	MismatchCounts mismatch_counts;

	SymTesting (std::string const& exe_filepath, float max_run_time = 2.0f) {
		logf("\n-------------------------------------\n\n");
		logf("Starting %s\n", exe_filepath.c_str());
		start_debugging_child_process(exe_filepath, max_run_time);
		dbghelp = std::make_unique<Debughelp>(pi.hProcess);
		resolver = std::make_unique<SymResolver>(pi.hProcess);
	}

	~SymTesting () {
		dbghelp = nullptr;
		resolver = nullptr;

		logf("Killing process\n");
		finish_debugging_and_kill_child_process();

	}

	char* get_addr (std::string_view filter) {
		return (char*)loaded_modules.find(filter).addr;
	}

	void measure_pdb_parse (char* addr) {
		//dbghelp->measure_addr2sym(addr);
		resolver->measure_pdb_parse(addr);
	}

	void show_addr2sym (char* addr) {
		SymResult res={}, res_dbghelp={};

		dbghelp->addr2sym(addr, &res_dbghelp);
		logf("dbghelp.dll: [%llx] ", (uintptr_t)addr);
		res_dbghelp.print();

		resolver->addr2sym(addr, &res);
		logf("SymResolver: [%llx] ", (uintptr_t)addr);
		res.print();
	}
	void measure_addr2sym (char* addr) {
		dbghelp->measure_addr2sym(addr);
		resolver->measure_addr2sym(addr);
	}
	void test_addr2sym (char* addr) {
		SymResult res={}, res_dbghelp={};

		auto err_dbghelp = dbghelp->addr2sym(addr, &res_dbghelp);
		auto err         = resolver->addr2sym(addr, &res);
		
		if (!res.equal(res_dbghelp, { &mismatch_counts, resolver.get(), addr })) {
			logf("!!! [%llx] (%s) Result Mismatch:\n", (uintptr_t)addr, res_dbghelp.sym_name);
			res.print_diff(res_dbghelp);
			//tests_failed = true;
		}
	}
	
	SymResult prev_res = {};
	SymResult prev_res_dbghelp = {};
	
	void test_distinct_addr2sym (uintptr_t addr) {
		show_and_test_distinct_addr2sym(addr, false);
	}
	void show_and_test_distinct_addr2sym (uintptr_t addr, bool show=true) {
		SymResult res={}, res_dbghelp={};

		dbghelp->addr2sym((void*)addr, &res_dbghelp);
		resolver->addr2sym((void*)addr, &res);
		
		if (!res.equal(prev_res) || !res_dbghelp.equal(prev_res_dbghelp)) {
			if (show) {
				logf("SymResolver: [%llx] ", addr);
				res.print();
			}
			
			if (!res.equal(res_dbghelp, { &mismatch_counts, resolver.get(), (void*)addr })) {
				logf("!!! [%llx] (%s) Result Mismatch:\n", addr, res_dbghelp.sym_name);
				res.print_diff(res_dbghelp);
				//tests_failed = true;
			}

			prev_res = res;
			prev_res_dbghelp = res_dbghelp;
		}
	}
	void show_distinct_sym_lineinfo (LoadedModule const& mod, uintptr_t addr) {
		SymResult res={};

		resolver->addr2sym((void*)addr, &res);
		
		if (!res.equal_no_inline(prev_res)) {
			logf("[%llx] ", addr - (uintptr_t)mod.addr);
			res.print_no_inline();

			prev_res = res;
		}
	}
	void show_distinct_sym (LoadedModule const& mod, uintptr_t addr) {
		SymResult res={};

		resolver->addr2sym((void*)addr, &res);
		
		if (!res.equal_sym(prev_res)) {
			logf("[%llx] ", addr - (uintptr_t)mod.addr);
			res.print_sym();

			prev_res = res;
		}
	}

	template <typename FUNC>
	void run_examples_addresses (bool show, bool test, int meas_count, FUNC run_examples) {
		using std::placeholders::_1;
		std::function<void(char*)> fshow = std::bind(&SymTesting::show_addr2sym, this, _1);
		std::function<void(char*)> fmeas = std::bind(&SymTesting::measure_addr2sym, this, _1);
		std::function<void(char*)> ftest = std::bind(&SymTesting::test_addr2sym, this, _1);

		if (show) run_examples(fshow);
		if (test) run_examples(ftest);

		mismatch_counts.print();

		for (int i=0; i<meas_count; i++) {
			run_examples(fmeas);
		}
		dbghelp->print_timings();
		logf("---\n");
		resolver->print_timings();
	}

	// try to exclude pdb parsing from measurement
	void warmup (char* addr) {
		SymResult sym = {};
		dbghelp->addr2sym(addr, &sym);
		resolver->addr2sym(addr, &sym);
	}

	void sweep_mod (std::string_view filter) {
		auto& mod = loaded_modules.find(filter);
		auto start = (uintptr_t)mod.addr;
		auto end = (uintptr_t)mod.addr + mod.size;

	#if 1
		int before = 0x100;
		int after = 0x100;

		logf("Sweep for module %s: [%llx-%llx] (-%x +%x)\n", mod.path.c_str(), start, end, before, after);
		for (uintptr_t addr = start - before; addr < end + after; addr++) {
			//show_and_test_distinct_addr2sym(addr);
			test_distinct_addr2sym(addr);
		}
	#else
		logf("Sweep for module %s: [%llx-%llx]\n", mod.path.c_str(), start, end);
		for (uintptr_t addr = start; addr < end; addr++) {
			show_distinct_sym(mod, addr);
		}
	#endif

		mismatch_counts.print();
	}
	
	void sweep_mod_measure (std::string_view filter) {
		auto& mod = loaded_modules.find(filter);
		auto start = (uintptr_t)mod.addr;
		auto end = (uintptr_t)mod.addr + mod.size;

		logf("Sweep for module %s: [%llx-%llx]\n", mod.path.c_str(), start, end);
		for (uintptr_t addr = start; addr < end; addr++) {
			measure_addr2sym((char*)addr);
		}

		mismatch_counts.print();
	}

	// seed=-1 => random seed
	void fuzz_mod_measure (std::string_view filter, int count=10000, int seed=-1) {
		auto& mod = loaded_modules.find(filter);
		auto start = (uintptr_t)mod.addr;
		auto end = (uintptr_t)mod.addr + mod.size;
		
		auto rng = seed < 0 ? init_rng() : init_rng((uint64_t)seed);
		std::uniform_int_distribution<uintptr_t> uniform_rng (start, end);
		
		logf("Fuzz for module %s: [%llx-%llx]\n", mod.path.c_str(), start, end);
		for (int i=0; i<count; i++) {
			auto addr = uniform_rng(rng);
			measure_addr2sym((char*)addr);
		}
	}
};

void example_addresses (bool test=true, bool show=false, int meas_count=1000) {
	ZoneScoped;
	
	try {
		ZoneScopedN("TinyProgram.exe");
		SymTesting sym("TinyProgram.exe", 0.5f);

		char* exe = sym.get_addr(".exe");
		char* ucrtbase = sym.get_addr("ucrtbase.dll");
		
		sym.warmup(exe + 0x21F0);
		sym.warmup(ucrtbase + 0x1B370);

		sym.run_examples_addresses(show, test, meas_count, [=] (std::function<void(char*)> at_addr) {
			at_addr(exe + 0);
			at_addr(exe + 5);
			
			at_addr(exe + 0x1000); // __local_stdio_printf_options line 90
			at_addr(exe + 0x1001); // __local_stdio_printf_options line 92 (weird dbghelp behavior)

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
			
			at_addr(exe + 0x102B); // inlining()

			at_addr(exe + 0x1010); // printf
			at_addr(exe + 0x102E); // printf lea
			at_addr(exe + 0x1000); // __local_stdio_printf_options

			at_addr(exe + 0x2154); // malloc()
			at_addr(exe + 0x16b0); // mainCRTStartup

			at_addr(ucrtbase + 0x1B370); // __stdio_common_vfprintf
		});
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	//Sleep(1000);
	
	try {
		ZoneScopedN("Namespaces.exe");
		SymTesting sym("Namespaces.exe", 0.5f);

		char* exe = sym.get_addr(".exe");
		
		sym.warmup(exe + 0x11AC0);

		sym.run_examples_addresses(show, test, meas_count, [=] (std::function<void(char*)> at_addr) {
			at_addr(exe + 0);

			at_addr(exe + 0x10D0); // main()
			at_addr(exe + 0x1070); // global()
			at_addr(exe + 0x1090); // space::namespaced() (This actually calls the the same address as namespaced2_same_code, as the functions are identical)
			at_addr(exe + 0x1090); // space::nested::namespaced2_same_code()
			at_addr(exe + 0x1080); // StructA::memberA()
			at_addr(exe + 0x10A0); // space::StructB::memberB()
			at_addr(exe + 0x10B0); // space::nested::StructC::StructD::memberCD()
			at_addr(exe + 0x10C0); // space::nested::StructC::StructD::smemberCD()
		});
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }\

	try {
		ZoneScopedN("city_builder_rel.exe");
		SymTesting sym("CityBuilderExample/city_builder_rel.exe");
		
		char* exe = sym.get_addr(".exe");
		char* assimp = sym.get_addr("assimp-vc143-mt.dll");
		char* ucrtbase = sym.get_addr("ucrtbase.dll");
		
		sym.warmup(exe + 0x21FA0);
		sym.warmup(assimp + 0x23990); // assimp, no pdb! (for testing)
		sym.warmup(ucrtbase + 0x1B370); // ucrtbase.dll!__stdio_common_vfprintf

		sym.run_examples_addresses(show, test, meas_count, [=] (std::function<void(char*)> at_addr) {
			at_addr(exe + 0);
			at_addr(exe + 5);

			at_addr(exe + 0x21FA0); // main()  -  This one does not resolve correctly for some reason, with both dbghelp.dll and my code
	
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
	
			at_addr(assimp + 0x23990); // assimp, no pdb!
	
			at_addr(ucrtbase + 0x1B370); // ucrtbase.dll!__stdio_common_vfprintf
		});

	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("rust_bevy_test.exe");
		SymTesting sym("RustBevyExample/rust_bevy_test.exe");

		char* exe = sym.get_addr(".exe");
		char* ucrtbase = sym.get_addr("ucrtbase.dll");
		
		sym.warmup(exe + 0x3011B80);
		sym.warmup(ucrtbase + 0x1B370);

		sym.run_examples_addresses(show, test, meas_count, [=] (std::function<void(char*)> at_addr) {
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
	
			at_addr(ucrtbase + 0x69FB30); // ucrtbase.dll!sinf(), returns wrong symbol for some reason, I even double checked everything, am I missing something?
			at_addr(ucrtbase + 0x1B370); // ucrtbase.dll!__stdio_common_vfprintf, weirdly this one works, so it's even the same ucrtbase.dll as the two other executables
		});
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
}

void sweep_tests () {
	ZoneScoped;

	try {
		ZoneScopedN("TinyProgram.exe");
		SymTesting sym("TinyProgram.exe", 0.5f);
		char* exe = sym.get_addr(".exe");

		sym.sweep_mod(".exe");
		sym.sweep_mod("ucrtbase.dll");

		//sym.show_addr2sym(exe + 0x1130);
		//sym.show_addr2sym(exe + 0x13c0); // TODO: linoinfo not found by me because lineheader stores address before symbol (and first line offset is symbol)
		//sym.show_addr2sym(exe + 0x56f0); // _OptionsStorage
		// not exact start address for some dumb reason, need to lookup not per hashmap but per binary search for each module, could then merge sorted lists per module into global sorted list with one scan
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }

	try {
		ZoneScopedN("Namespaces.exe");
		SymTesting sym("Namespaces.exe", 0.5f);
		char* exe = sym.get_addr(".exe");

		sym.sweep_mod(".exe");

		//sym.show_addr2sym(exe + 0x1090);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("notepad.exe");
		SymTesting sym("C:/Windows/System32/notepad.exe", 0.5f);
	
		char* exe = sym.get_addr(".exe");
		
		sym.sweep_mod(".exe");
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("city_builder_rel.exe");
		SymTesting sym("CityBuilderExample/city_builder_rel.exe");
		char* exe = sym.get_addr(".exe");

		sym.sweep_mod(".exe");
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }

	try {
		ZoneScopedN("rust_bevy_test.exe");
		SymTesting sym("RustBevyExample/rust_bevy_test.exe");
		char* exe = sym.get_addr(".exe");

		sym.sweep_mod(".exe");
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
}

void profiling_pdb_parse (int count) {
	ZoneScoped;

	try {
		ZoneScopedN("TinyProgram.exe");
		SymTesting sym("TinyProgram.exe", 0.5f);
		for (int i=0; i<count; i++) {
			sym.measure_pdb_parse(sym.get_addr(".exe"));
		}
		for (int i=0; i<count; i++) {
			sym.measure_pdb_parse(sym.get_addr("ucrtbase.dll"));
		}
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("city_builder_rel.exe");
		SymTesting sym("city_builder_rel.exe");
		for (int i=0; i<count; i++) {
			sym.measure_pdb_parse(sym.get_addr(".exe"));
		}
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("rust_bevy_test.exe");
		SymTesting sym("rust_bevy_test.exe");
		for (int i=0; i<count; i++) {
			sym.measure_pdb_parse(sym.get_addr(".exe"));
		}
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
}
void profiling_fuzz () {
	ZoneScoped;

	try {
		ZoneScopedN("TinyProgram.exe");
		SymTesting sym("TinyProgram.exe", 0.5f);
		char* exe = sym.get_addr(".exe");

		sym.sweep_mod_measure(".exe");
		sym.sweep_mod_measure("ucrtbase.dll");

		//sym.show_addr2sym(exe + 0x1130);
		//sym.show_addr2sym(exe + 0x13c0); // TODO: linoinfo not found by me because lineheader stores address before symbol (and first line offset is symbol)
		//sym.show_addr2sym(exe + 0x56f0); // _OptionsStorage
		// not exact start address for some dumb reason, need to lookup not per hashmap but per binary search for each module, could then merge sorted lists per module into global sorted list with one scan
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("city_builder_rel.exe");
		SymTesting sym("CityBuilderExample/city_builder_rel.exe");
		char* exe = sym.get_addr(".exe");

		sym.fuzz_mod_measure(".exe", 50000, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("rust_bevy_test.exe");
		SymTesting sym("RustBevyExample/rust_bevy_test.exe");
		char* exe = sym.get_addr(".exe");

		sym.fuzz_mod_measure(".exe", 10000, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
}

void profiling_run () {
	example_addresses(false, false, 4000);

	profiling_pdb_parse(4000);

	profiling_fuzz();
}

int main(int argc, const char** argv) {
	//example_addresses(true, false);
	//sweep_tests();
	profiling_run();

	return 0;
}
