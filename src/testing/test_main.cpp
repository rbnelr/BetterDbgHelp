#include "util.hpp"
#include "test_runner.hpp"

// NOTE: tracy seems to not support running in a dll and exe at the same time
// so the testing application has to have TRACY_ENABLE not set if the dll want it
// Actually it seems like TRACY_ENABLE in the dll causes problems, as tracy is actually calling our functions for symbol resolution
// interestingly, if we name our dll dbghelp.dll, it seems to resolve symbols correctly but crash at some point as our dll (like dbghelp originally) is not thread safe
// I thought naming the dll something like my_dbghelp would fix this, but somehow it's still broken
// maybe because tracy is importing our exported dbghelp functions
// so for now put tracy in the exe instead, but from tracy manual: TRACY_DBGHELP_LOCK this could be used to fix it

// from util/logger.hpp; used everywhere instead of printf
Logger g_logger("output.txt");

void example_addresses (bool test=true, bool show=false, int meas_count=1000) {
	ZoneScoped;
	
	logf("================ Example Addresses ================\n");

	try {
		ZoneScopedN("TinyProgram.exe");
		TestRunner sym("test_executables/TinyProgram/TinyProgram.exe", 0.5f);

		char* exe = sym.get_addr(".exe");
		char* ucrtbase = sym.get_addr("ucrtbase.dll");
		sym.print_pdb_stats(".exe");
		sym.print_pdb_stats("ucrtbase.dll");
		
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

			at_addr(exe + 0x13c0); // __security_check_cookie

			at_addr(ucrtbase + 0x1B370); // __stdio_common_vfprintf
		});
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	//Sleep(1000);

	try {
		ZoneScopedN("Namespaces.exe");
		TestRunner sym("test_executables/Namespaces/Namespaces.exe", 0.5f);

		char* exe = sym.get_addr(".exe");
		sym.print_pdb_stats(".exe");
		
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
		TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
		
		char* exe = sym.get_addr(".exe");
		char* assimp = sym.get_addr("assimp-vc143-mt.dll");
		char* ucrtbase = sym.get_addr("ucrtbase.dll");
		sym.print_pdb_stats(".exe");
		sym.print_pdb_stats("assimp-vc143-mt.dll");
		
		sym.warmup(exe + 0x121FA0);
		sym.warmup(assimp + 0x23990); // assimp, no pdb! (for testing)
		sym.warmup(ucrtbase + 0x1B370); // ucrtbase.dll!__stdio_common_vfprintf

		sym.run_examples_addresses(show, test, meas_count, [=] (std::function<void(char*)> at_addr) {
			at_addr(exe + 0);
			at_addr(exe + 5);

			at_addr(exe + 0x121FA0); // main()  -  This one does not resolve correctly for some reason, with both dbghelp.dll and my code
	
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
		TestRunner sym("test_executables/RustBevyApp/rust_bevy_test.exe");

		char* exe = sym.get_addr(".exe");
		char* ucrtbase = sym.get_addr("ucrtbase.dll");
		sym.print_pdb_stats(".exe");
		
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
	
			//at_addr(ucrtbase + 0x69FB30); // ucrtbase.dll!sinf(), returns wrong symbol for some reason, I even double checked everything, am I missing something?
			at_addr(ucrtbase + 0x1B370); // ucrtbase.dll!__stdio_common_vfprintf, weirdly this one works, so it's even the same ucrtbase.dll as the two other executables
		});
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
}

void sweep_tests () {
	ZoneScoped;
	
	logf("================= Sweep Addresses =================\n");

	try {
		ZoneScopedN("TinyProgram.exe");
		TestRunner sym("test_executables/TinyProgram/TinyProgram.exe", 0.5f);
		char* exe = sym.get_addr(".exe");

		sym.sweep_mod(".exe");
		//sym.sweep_mod("ucrtbase.dll");

		//sym.show_addr2sym(exe + 0x1130);
		//sym.show_addr2sym(exe + 0x13c0); // TODO: linoinfo not found by me because lineheader stores address before symbol (and first line offset is symbol)
		//sym.show_addr2sym(exe + 0x56f0); // _OptionsStorage
		// not exact start address for some dumb reason, need to lookup not per hashmap but per binary search for each module, could then merge sorted lists per module into global sorted list with one scan
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }

	try {
		ZoneScopedN("Namespaces.exe");
		TestRunner sym("test_executables/Namespaces/Namespaces.exe", 0.5f);
		char* exe = sym.get_addr(".exe");

		sym.sweep_mod(".exe");

		//sym.show_addr2sym(exe + 0x1090);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	// NOTE: For some reason I can download the pdb for notepad.exe from the symbol servers and use it successfully
	// yet dbghelp throws errors no matter the address, I initially though that me writing the pdb to SymbolCache somehow broke dbghelp
	// but actually, dbghelp fails even with an empty SymbolCache, so I have no idea, maybe it refuses to provide symbols for anything but dlls or something
	try {
		ZoneScopedN("notepad.exe");
		TestRunner sym("C:/Windows/System32/notepad.exe", 0.5f);
	
		char* exe = sym.get_addr(".exe");
		
		sym.sweep_mod(".exe");
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("city_builder_rel.exe");
		TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
		char* exe = sym.get_addr(".exe");

		sym.sweep_mod(".exe");
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }

	try {
		ZoneScopedN("rust_bevy_test.exe");
		TestRunner sym("test_executables/RustBevyApp/rust_bevy_test.exe");
		char* exe = sym.get_addr(".exe");

		sym.sweep_mod(".exe");
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
}

void profiling_sweep (int mult=1) {
	ZoneScoped;

	logf("================ Sweeping Addresses ================\n");

	try {
		ZoneScopedN("TinyProgram.exe");
		TestRunner sym("test_executables/TinyProgram/TinyProgram.exe", 0.5f);
		for (int i=0; i<mult; i++) sym.sweep_mod_measure(".exe");
		for (int i=0; i<mult; i++) sym.sweep_mod_measure("ucrtbase.dll");
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("city_builder_rel.exe");
		TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
		// This is too slow to run more than once
		sym.sweep_mod_measure(".exe");
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
}
void profiling_fuzz (int mult=1) {
	ZoneScoped;

	logf("================ Fuzzing Addresses ================\n");

	//try {
	//	ZoneScopedN("TinyProgram.exe");
	//	TestRunner sym("test_executables/TinyProgram/TinyProgram.exe", 0.5f);
	//	sym.fuzz_mod_measure(".exe", 20000*mult);
	//} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("city_builder_rel.exe");
		TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
		sym.fuzz_mod_measure(".exe", 20000*mult, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("rust_bevy_test.exe");
		TestRunner sym("test_executables/RustBevyApp/rust_bevy_test.exe");
		sym.fuzz_mod_measure(".exe", 4000*mult, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
}

void profiling_pdb_parse (int iterations) {
	ZoneScoped;

	logf("=============== PDB parsing Profiling =============\n");

	try {
		ZoneScopedN("TinyProgram.exe");
		TestRunner sym("test_executables/TinyProgram/TinyProgram.exe", 0.5f);
		sym.measure_pdb_parse(".exe", iterations);
		sym.measure_pdb_parse("ucrtbase.dll", iterations);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("city_builder_rel.exe");
		TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
		sym.measure_pdb_parse(".exe", iterations/2);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }

	try {
		ZoneScopedN("rust_bevy_test.exe");
		TestRunner sym("test_executables/RustBevyApp/rust_bevy_test.exe");
		sym.measure_pdb_parse(".exe", iterations/5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
}

// Run a bunch of passes of symbol resolution to produce interesting timing results
// either with dbghelp for comparison or without to optimize my code by inspecting via tracy
// examples, sweeps, and fuzzing likely all have different performance characteristics,
// so just try to find a good balance that is not to quick to get bad sampling results in tracy, nor too long to have to sit around waiting
// that's why I play with the iteration counts so much
void profiling_run () {
	run_dbghelp = false;
	int mult = run_dbghelp ? 1 : 20;

	example_addresses(false, false, 4000 * mult);

	profiling_sweep(mult);
	profiling_fuzz(run_dbghelp ? 1 : 60); // this is fast enough without dbghelp to run more often

	//profiling_pdb_parse(1000);
}

//
void profiling_run_cached () {
	int count = 2000000;
	run_dbghelp = false;

	try {
		ZoneScopedN("city_builder (cached)");
		TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
		sym.print_pdb_stats(".exe");
		sym.fuzz_mod_measure(".exe", count, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("bevy (cached)");
		TestRunner sym("test_executables/RustBevyApp/rust_bevy_test.exe");
		sym.print_pdb_stats(".exe");
		sym.fuzz_mod_measure(".exe", count, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
}
void profiling_run_cachemiss () {
	int count = 10000;
	run_dbghelp = false;
	
	clear_cpu_cache = false;

	try {
		ZoneScopedN("city_builder (cached)");
		TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
		sym.print_pdb_stats(".exe");
		sym.fuzz_mod_measure(".exe", count, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("bevy (cached)");
		TestRunner sym("test_executables/RustBevyApp/rust_bevy_test.exe");
		sym.print_pdb_stats(".exe");
		sym.fuzz_mod_measure(".exe", count, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }

	clear_cpu_cache = true;

	try {
		ZoneScopedN("city_builder");
		TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
		sym.fuzz_mod_measure(".exe", count, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	
	try {
		ZoneScopedN("bevy");
		TestRunner sym("test_executables/RustBevyApp/rust_bevy_test.exe");
		sym.fuzz_mod_measure(".exe", count, 5);
	} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
}

inline __declspec(noinline) void mySuperLongFunctionName_asikudghauyfghisudgfhiusdghfiusdghfiuysghdfiudshgf () {
	printf("");
}

#if 0
void test_dll () {
#if NDEBUG
	auto* path = "x64/Release/dbghelp.dll";
#else
	auto* path = "x64/Debug/dbghelp.dll";
#endif
	auto dll = LoadLibraryA(path);
	auto _SymInitialize = (t_SymInitialize)GetProcAddress(dll, "SymInitialize");
	auto _SymCleanup = (t_SymCleanup)GetProcAddress(dll, "SymCleanup");
	auto _SymFromAddr = (t_SymFromAddr)GetProcAddress(dll, "SymFromAddr");
	auto _SymGetLineFromAddr64 = (t_SymGetLineFromAddr64)GetProcAddress(dll, "SymGetLineFromAddr64");
	
	auto proc = GetCurrentProcess();

	//_SymInitialize(proc, "C:\\;D:\\a;E:\\a\\b;", false);
	_SymInitialize(proc, nullptr, true);
	
	auto func_ptr = &mySuperLongFunctionName_asikudghauyfghisudgfhiusdghfiusdghfiuysghdfiudshgf;
	func_ptr();
	
	void* addr0 = &test_dll;
	void* addr1 = func_ptr;
	void* addr2 = (void*)0x0000000140117420;
	void* addr3 = (void*)(0x0000000140117420+20);

	SYMBOL_INFO_PACKAGE buf;
	buf.si = {};
	buf.si.SizeOfStruct = sizeof(buf.si);
	buf.si.MaxNameLen = MAX_SYM_NAME;
	//buf.si.MaxNameLen = 20;
	
	DWORD Displacement = 0;
	IMAGEHLP_LINE64 line = {};
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
	
	_SymFromAddr(proc, (DWORD64)addr0, NULL, &buf.si);
	_SymGetLineFromAddr64(proc, (DWORD64)addr0, &Displacement, &line);

	_SymFromAddr(proc, (DWORD64)addr1, NULL, &buf.si);
	_SymGetLineFromAddr64(proc, (DWORD64)addr1, &Displacement, &line);
	
	_SymFromAddr(proc, (DWORD64)addr2, NULL, &buf.si);
	_SymGetLineFromAddr64(proc, (DWORD64)addr2, &Displacement, &line);

	_SymFromAddr(proc, (DWORD64)addr3, NULL, &buf.si);
	_SymGetLineFromAddr64(proc, (DWORD64)addr3, &Displacement, &line);

	_SymCleanup(proc);

	FreeLibrary(dll);
}
#endif
void test_dll () {
	run_dll_wrapper_as_resolver = true;
	//example_addresses(true, true, 0);
	//example_addresses(true);

	profiling_run();
}

int main(int argc, const char** argv) {
	test_dll();
	//example_addresses(true, true, 0);
	
	//print_timings = false;
	//example_addresses(true, false, 0);
	//sweep_tests();

	//profiling_run();
	//profiling_run_cached();
	//profiling_run_cachemiss();

	//profiling_pdb_parse(100);
	
	//try {
	//	TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
	//	
	//	char* exe = sym.get_addr(".exe");
	//	//sym.show_addr2sym(exe + 0xfa4f4);
	//	//sym.show_addr2sym(exe + 0x6c7fc);
	//	sym.show_addr2sym(exe + 0x73217);
	//	
	//	//sym.sweep_mod(".exe", 0xfa4d2, 0xfa74a, true); // Engine::Engine
	//
	//	//sym.sweep_mod(".exe");
	//} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }

	//try {
	//	TestRunner sym("test_executables/TinyProgram/TinyProgram.exe", 0.5f);
	//	char* exe = sym.get_addr(".exe");
	//	sym.show_addr2sym(exe + 0x22a0);
	//} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	//
	//try {
	//	ZoneScopedN("city_builder_rel.exe");
	//	TestRunner sym("test_executables/CppGame/city_builder_rel.exe");
	//	sym.print_pdb_stats(".exe");
	//	sym.fuzz_mod_measure(".exe", 60000, 5);
	//} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }
	//try {
	//	ZoneScopedN("rust_bevy_test.exe");
	//	TestRunner sym("test_executables/RustBevyApp/rust_bevy_test.exe");
	//	sym.print_pdb_stats(".exe");
	//	sym.fuzz_mod_measure(".exe", 6000, 5);
	//} catch (std::exception& err) { logf("!! Exception: %s\n", err.what()); }

	return 0;
}

// Function to wipe the cache
void _clear_cpu_cache () {
	// L3 sized buffer 12MB
	constexpr size_t L3_size = 12 * 1024 * 1024;
	static std::vector<char> g_trash_buffer(L3_size);

	ZoneScopedC(0xff0000);

	volatile char* sink = g_trash_buffer.data();
	
	for (size_t i = 0; i < L3_size; i += 64) {
		// i += 64 because a cache line is typically 64 bytes.
		// Touching one byte loads the whole line.
		sink[i] = (char)i;
	}
	
	std::atomic_thread_fence(std::memory_order_seq_cst);
}
