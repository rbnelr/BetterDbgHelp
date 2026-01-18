#pragma once
#include "util.hpp"
#include <functional>
#include <random>

#include <psapi.h>
#pragma comment(lib, "Kernel32.lib")

bool run_dbghelp = true;
bool clear_cpu_cache = false;
void _clear_cpu_cache ();
bool print_timings = true;

#include "dbghelp_api.hpp"
#include "pdb/sym_resolvers.hpp"

class TestRunner {
	STARTUPINFOA si{};
	PROCESS_INFORMATION pi{};
	
	DEBUG_EVENT de;
	
	struct LoadedModule {
		std::string path;
		std::string name;
		void* addr; // Virtual memory address module was loaded at in child process
		size_t size;
	};
	struct LoadedModules {
		std::vector<LoadedModule> list;

		void add (std::string path, void* addr, size_t size) {
			std::filesystem::path p = path;
			auto name = p.filename().string();
			list.push_back({ path, name, addr, size });
		}
		
		LoadedModule const& find (std::string_view name_suffix) {
			for (auto& m : list) {
				if (ends_with(m.path, name_suffix)) {
					return m;
				}
			}
			throw std::runtime_error(std::string(name_suffix) + " not found");
		}
		LoadedModule const& find (char* addr) {
			for (auto& m : list) {
				if (addr >= m.addr && addr < (char*)m.addr + m.size) {
					return m;
				}
			}
			throw std::runtime_error("not found");
		}
	};

	LoadedModules loaded_modules;

	std::unique_ptr<SymResolverDebughelp> dbghelp;
	std::unique_ptr<SymResolver> resolver;
	
	void start_debugging_child_process (std::string const& exe_filepath, float max_run_time) {
		ZoneScopedC(0xff0000);

		// Start exe as child process with DEBUG_ONLY_THIS_PROCESS
		// let it run until it exits on its own or until max run time is reached
		// meanwhile react to module load events to record their names and addresses
		// then leave process suspended so we can simulate symbol resolving

		std::filesystem::path path = exe_filepath;
		std::string working_dir = path.has_parent_path() ? path.parent_path().string() : ".";

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
		ZoneScopedC(0xff0000);

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

	TestRunner (std::string const& exe_filepath, float max_run_time = 2.0f) {
		logf("\n---------- Starting %s -----------\n", exe_filepath.c_str());
		start_debugging_child_process(exe_filepath, max_run_time);
		if (run_dbghelp)
			dbghelp = std::make_unique<SymResolverDebughelp>(pi.hProcess);
		resolver = std::make_unique<SymResolver>(pi.hProcess);
	}

	~TestRunner () {
		dbghelp = nullptr;
		resolver = nullptr;

		logf("Killing process\n");
		finish_debugging_and_kill_child_process();

	}
	
	LoadedModule const& get_mod (char* addr) {
		return loaded_modules.find(addr);
	}
	char* get_addr (std::string_view filter) {
		return (char*)loaded_modules.find(filter).addr;
	}

	void print_addr (const char* context, char* addr) {
		auto& mod = get_mod(addr);
		auto rel_addr = (int64_t)addr - (int64_t)mod.addr;
		logf("%s: [%s+%llx] ", context, mod.name.c_str(), rel_addr);
	}

	void show_addr2sym (char* addr) {
		SymResult res={}, res_dbghelp={};

		if (dbghelp) {
			dbghelp->addr2sym(addr, &res_dbghelp);
			print_addr("dbghelp.dll", addr);
			res_dbghelp.print(); // TODO: calculate and print speedup percent after both timings, probably should move all measurements to central class for that, then pass ptr to that to both classes
		}

		resolver->addr2sym(addr, &res);
		print_addr("SymResolver", addr);
		res.print();
	}
	void measure_addr2sym (char* addr) {
		if (dbghelp)
			dbghelp->measure_addr2sym(addr);
		resolver->measure_addr2sym(addr);
	}
	void test_addr2sym (char* addr) {
		assert(dbghelp); // need dbghelp to be able to compare

		SymResult res={}, res_dbghelp={};
		dbghelp->addr2sym(addr, &res_dbghelp);
		resolver->addr2sym(addr, &res);
		
		if (!res.equal(res_dbghelp, { &mismatch_counts, resolver.get(), addr })) {
			auto& mod = get_mod(addr);
			auto rel_addr = (int64_t)addr - (int64_t)mod.addr;

			res.print_diff(mod.name.c_str(), rel_addr, res_dbghelp);
			//tests_failed = true;
		}
	}
	
	std::unique_ptr<SymResult> prev_res = std::make_unique<SymResult>();
	std::unique_ptr<SymResult> prev_res_dbghelp = std::make_unique<SymResult>();
	
	void test_distinct_addr2sym (uint64_t addr) {
		show_and_test_distinct_addr2sym(addr, false);
	}
	void show_and_test_distinct_addr2sym (uint64_t addr, bool show=true) {
		auto res = std::make_unique<SymResult>();
		auto res_dbghelp = std::make_unique<SymResult>();

		dbghelp->addr2sym((void*)addr, res_dbghelp.get());
		resolver->addr2sym((void*)addr, res.get());
		
		if (!res->equal(*prev_res) || !res_dbghelp->equal(*prev_res_dbghelp)) {
			if (show) {
				print_addr("SymResolver.dll", (char*)addr);
				res->print();
			}

			auto _old = mismatch_counts.symbol_mismatch_overlap;

			if (!res->equal(*res_dbghelp, { &mismatch_counts, resolver.get(), (void*)addr }) &&
				mismatch_counts.symbol_mismatch_overlap == _old // HACK: Avoid showing diff for mismatched due to overlapping symbols, to better find issues I can fix
				) {
				
				auto& mod = get_mod((char*)addr);
				auto rel_addr = (int64_t)addr - (int64_t)mod.addr;

				res->print_diff(mod.name.c_str(), rel_addr, *res_dbghelp);
				//tests_failed = true;
			}

			prev_res = std::move(res);
			prev_res_dbghelp = std::move(res_dbghelp);
		}
	}
	void show_distinct_sym_lineinfo (LoadedModule const& mod, uint64_t addr) {
		auto res = std::make_unique<SymResult>();

		resolver->addr2sym((void*)addr, res.get());
		
		if (!res->equal_no_inline(*prev_res)) {
			logf("[%llx] ", addr - (uint64_t)mod.addr);
			res->print_no_inline();

			prev_res = std::move(res);
		}
	}
	void show_distinct_sym (LoadedModule const& mod, uint64_t addr) {
		auto res = std::make_unique<SymResult>();

		resolver->addr2sym((void*)addr, res.get());
		
		if (!res->equal_sym(*prev_res)) {
			logf("[%llx] ", addr - (uint64_t)mod.addr);
			res->print_sym();

			prev_res = std::move(res);
		}
	}
	void show_distinct_sym_dbghelp (LoadedModule const& mod, uint64_t addr) {
		auto res = std::make_unique<SymResult>();

		dbghelp->addr2sym((void*)addr, res.get());
		
		if (!res->equal_sym(*prev_res)) {
			logf("[%llx] ", addr - (uint64_t)mod.addr);
			res->print_sym();

			prev_res = std::move(res);
		}
	}

	template <typename FUNC>
	void run_examples_addresses (bool show, bool test, int meas_iterations, FUNC run_examples) {
		using std::placeholders::_1;
		std::function<void(char*)> fshow = std::bind(&TestRunner::show_addr2sym, this, _1);
		std::function<void(char*)> fmeas = std::bind(&TestRunner::measure_addr2sym, this, _1);
		std::function<void(char*)> ftest = std::bind(&TestRunner::test_addr2sym, this, _1);

		if (show) {
			logf("@ Show Addr2Sym\n");
			run_examples(fshow);
		}
		if (test) {
			logf("@ Test Addr2Sym\n");
			run_examples(ftest);

			mismatch_counts.print();
		}
		
		if (meas_iterations > 0) {
			logf("@ Timing %d Iterations\n", meas_iterations);
			for (int i=0; i<meas_iterations; i++) {
				run_examples(fmeas);
			}
			if (print_timings) {
				if (dbghelp) {
					dbghelp->print_timings();
					logf("---\n");
				}
				resolver->print_timings();
			}
		}
	}

	// try to exclude pdb parsing from measurement
	void warmup (char* addr) {
		SymResult sym = {};
		if (dbghelp)
			dbghelp->addr2sym(addr, &sym);
		resolver->addr2sym(addr, &sym);
	}

	void sweep_mod (std::string_view filter, bool show=false) {
		auto& mod = loaded_modules.find(filter);
		sweep_mod(filter, 0, mod.size, show);
	}
	void sweep_mod (std::string_view filter, uint64_t rva_start, uint64_t rva_end, bool show=false) {
		auto& mod = loaded_modules.find(filter);
		auto start = (uint64_t)mod.addr + rva_start;
		auto end = (uint64_t)mod.addr + rva_end;
		
		logf("@ Sweep for module %s: [%llx-%llx]\n", mod.path.c_str(), start, end);
		if (show) {
			for (uint64_t addr = start; addr < end; addr++) {
				show_and_test_distinct_addr2sym(addr);
			}
			/*
			logf("@ Dbghelp:\n");
			for (uint64_t addr = start; addr < end; addr++) {
				show_distinct_sym_dbghelp(mod, addr);
			}
			logf("@ SymResolver:\n");
			for (uint64_t addr = start; addr < end; addr++) {
				show_distinct_sym(mod, addr);
			}*/
		}
		else {
			for (uint64_t addr = start; addr < end; addr++) {
				test_distinct_addr2sym(addr);
			}
		}

		mismatch_counts.print();
	}
	
	void sweep_mod_measure (std::string_view filter) {
		auto& mod = loaded_modules.find(filter);
		auto start = (uint64_t)mod.addr;
		auto end = (uint64_t)mod.addr + mod.size;

		logf("@ Sweep for module %s: [%llx-%llx]\n", mod.path.c_str(), start, end);
		for (uint64_t addr = start; addr < end; addr++) {
			measure_addr2sym((char*)addr);
		}
		if (print_timings) {
			if (dbghelp) {
				dbghelp->print_timings();
				logf("---\n");
			}
			resolver->print_timings();
		}
	}

	// seed=-1 => random seed
	void fuzz_mod_measure (std::string_view filter, int count=10000, int seed=-1) {
		auto& mod = loaded_modules.find(filter);
		auto start = (uint64_t)mod.addr;
		auto end = (uint64_t)mod.addr + mod.size;
		
		auto rng = seed < 0 ? init_rng() : init_rng((uint64_t)seed);
		std::uniform_int_distribution<uint64_t> uniform_rng (start, end);
		
		logf("@ Fuzz for module %s: [%llx-%llx]\n", mod.path.c_str(), start, end);
		for (int i=0; i<count; i++) {
			//if (clear_cpu_cache && i % 10 == 0)
			if (clear_cpu_cache)
				_clear_cpu_cache();

			auto addr = uniform_rng(rng);
			measure_addr2sym((char*)addr);
		}
		
		if (print_timings) {
			if (dbghelp) {
				dbghelp->print_timings();
				logf("---\n");
			}
			resolver->print_timings();
		}
	}

	void measure_pdb_parse (std::string_view filter, int iterations) {
		auto addr = get_addr(filter);
		auto& mod = loaded_modules.find(filter);

		logf("@ Measure PDB parse for module %s\n", mod.path.c_str());

		for (int i=0; i<iterations; i++) {
			//_clear_cpu_cache();
			//dbghelp->measure_addr2sym(addr);

			_clear_cpu_cache();
			resolver->measure_pdb_parse(addr);
		}
		if (print_timings)
			resolver->print_timings();
	}

	void print_pdb_stats (std::string_view filter) {
		auto addr = get_addr(filter);
		resolver->print_pdb_stats(addr);
	}
};
