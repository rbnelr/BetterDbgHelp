#pragma once
#include "util.hpp"
#include "pdb_reader.hpp"

#include <psapi.h>
#pragma comment(lib, "Kernel32.lib")

#if 0
struct LoadedModule {
	std::filesystem::path path;
	std::string ansi_path;

	HMODULE handle;

	uint64_t base_addr;
	size_t size;

	// Loading state is kinda complex because I optimized GetModuleInformation + GetModuleFileNameExW calls
	// by using modules_by_handle, cache all modules found on first EnumProcessModules immediately
	// but do not attempt to load all pdbs, as is very unlikely all of them will be hit even by a profiler!
	bool has_attempted_load = false;

	std::unique_ptr<FastPdbLookup> pdb;
	std::unique_ptr<ExportTableQuery> exports;

	LoadedModule (std::filesystem::path&& path, HMODULE handle, uint64_t base_addr, size_t size) {
		this->path = std::move(path);
		ansi_path = this->path.string();
		this->handle = handle;
		this->base_addr = base_addr;
		this->size = size;
	}
	void load_pdb () {
		assert(pdb == nullptr);

		logf("[BetterDbgHelp] Loading pdb for %s.\n", path.string().c_str());

		pdb = PdbReader::try_load_lookup_for_exe(path);
	}

	ExportTableQuery* load_export_table () {
		if (!exports) {
			logf("[BetterDbgHelp] Loading ExportTable for %s.\n", path.string().c_str());
			//try {
				exports = std::make_unique<ExportTableQuery>(path);
			// Image parser should never fail!
			//} catch (std::exception& ex) {
			//	logf("!!! Export Table loading exception: %s\n", ex.what());
			//	return nullptr;
			//}
		}
		return exports.get();
	}

	void attempt_load () {
		assert(!has_attempted_load && !pdb && !exports);
		ZoneScopedC(0xffff00);

		load_pdb();
		has_attempted_load = true;
	}
};
// Holds the cached pdb or export table lookup data for the .exe and all loaded .dll of a process
struct ModuleCache {
	HANDLE hprocess;
	std::vector<std::unique_ptr<LoadedModule>> sorted_modules;
	ankerl_hashmap<HMODULE, LoadedModule*> modules_by_handle;
	
	TimerMeasurement ttry_get_and_cache_module = TimerMeasurement("try_get_and_cache_module");
	TimerMeasurement tload_pdb = TimerMeasurement("load_pdb");

	ModuleCache (HANDLE hprocess): hprocess{hprocess} {

	}
	void clear () {
		sorted_modules.clear();
		sorted_modules.shrink_to_fit();
		modules_by_handle.clear();
	}

	LoadedModule* find_module_for_addr (uint64_t addr) {
		//ZoneScoped;

		LoadedModule* mod = nullptr;

		// Try find cached data for module loaded in process
		// Linear search shoule be fast enough for the moment
		for (auto& m : sorted_modules) {
			if (addr >= m->base_addr && addr < m->base_addr + m->size) {
				mod = m.get();
				break;
			}
		}
		
		if (!mod) {
			// Module is not cached, try find module for address
			mod = try_get_and_cache_module(addr);
		}
		if (mod && !mod->has_attempted_load) {
			TimerMeasZone(tload_pdb);

			_hprocess = hprocess;
			_mod_base = mod->base_addr;

			mod->attempt_load();
		}
		return mod;
	}

	LoadedModule* try_get_and_cache_module (uint64_t addr) {
		ZoneScoped;
		
		LoadedModule* newly_found_module_for_address = nullptr;
		{
			TimerMeasZone(ttry_get_and_cache_module);

			// Get a list of handles of all loaded modules in process
			// NOTE: that later calls can return different lists as dlls can be loaded and unloaded
			// We never check for dlls having been unloaded for performance reasons, this matches tracy behavior, but is technically wrong? (TODO: properly think about this and document?)
			// Instead once an symbol at an address that is not in a cached module is queried,
			// we check all modules that are loaded at that point in time and then pretend that any modules we find are never unloaded
			
			// TODO: Tracy switched to VirtualQueryEx at some point, not sure if I was using an older version of tracy at when I learned how to do this using EnumProcessModules
			// VirtualQueryEx can directly tell you the image base via the returned AllocationBase (the start of the VirtualAlloc call that allocated the image data?)
			
			HMODULE modules[1024];
			DWORD needed = 0;
			if (!EnumProcessModules(hprocess, modules, sizeof(modules), &needed) || needed > sizeof(modules)) { // TODO: properly handle error
				print_err_throw("EnumProcessModules");
			}

			// NOTE: in cases of addresses being queried that lie outside of any module, we will end up doing repeated EnumProcessModules calls as this cannot be cached
			// but this should not happen in the use case of a profiler like tracy
			
			bool pushed_any = false;

			for (int i=0; i<needed/sizeof(HMODULE); i++) {
				auto& hmod = modules[i];
				// optimization: avoid GetModuleInformation calls
				if (modules_by_handle.contains(hmod)) {
					// address failed module cache lookup, but module returned by EnumProcessModules was in cache, avoid calling GetModuleInformation
					continue;
				}

				// hmod is unseen module
				// cache it (even if unrelated to queried address to enable GetModuleInformation-optimization)
				MODULEINFO info = {};
				if (GetModuleInformation(hprocess, hmod, &info, sizeof(info))) {
					auto base = (uint64_t)info.lpBaseOfDll;
					auto size = (size_t)info.SizeOfImage;

					wchar_t name[MAX_PATH];
					auto nameLength = GetModuleFileNameExW(hprocess, hmod, name, sizeof(name));
					if (nameLength > 0) {
						auto path = std::wstring_view(name, nameLength);
						auto mod = std::make_unique<LoadedModule>(std::filesystem::path(path), hmod, base, size);
						auto* pmod = mod.get();

						sorted_modules.push_back(std::move(mod));
						modules_by_handle.emplace(pmod->handle, pmod);
						pushed_any = true;

						if (addr >= base && addr < base + size) {
							newly_found_module_for_address = pmod;
						}
					}
				}
			}

			if (pushed_any) {
				// re-sort
				std::sort(sorted_modules.begin(), sorted_modules.end(),
				[] (std::unique_ptr<LoadedModule> const& l, std::unique_ptr<LoadedModule> const& r) {
					return std::less<uint64_t>()(l->base_addr, r->base_addr);
				});
			}
		}

		return newly_found_module_for_address;
	}
};
#else
struct LoadedModule {
	std::filesystem::path path;
	std::string ansi_path;

	HMODULE handle () {
		return (HMODULE)base_addr;
	}

	uint64_t base_addr;
	size_t size;

	std::unique_ptr<FastPdbLookup> pdb;
	std::unique_ptr<ExportTableQuery> exports;

	LoadedModule (std::filesystem::path&& path, uint64_t base_addr, size_t size) {
		this->path = std::move(path);
		ansi_path = this->path.string();
		this->base_addr = base_addr;
		this->size = size;
	}
	void load_pdb () {
		assert(!pdb && !exports);
		ZoneScopedC(0xffff00);

		logf("[BetterDbgHelp] Loading pdb for %s.\n", path.string().c_str());

		pdb = PdbReader::try_load_lookup_for_exe(path);
	}

	ExportTableQuery* load_export_table () {
		if (!exports) {
			logf("[BetterDbgHelp] Loading ExportTable for %s.\n", path.string().c_str());
			//try {
				exports = std::make_unique<ExportTableQuery>(path);
			// Image parser should never fail!
			//} catch (std::exception& ex) {
			//	logf("!!! Export Table loading exception: %s\n", ex.what());
			//	return nullptr;
			//}
		}
		return exports.get();
	}
};
// Holds the cached pdb or export table lookup data for the .exe and all loaded .dll of a process
struct ModuleCache {
	HANDLE hprocess;
	std::vector<std::unique_ptr<LoadedModule>> sorted_modules;
	ankerl_hashmap<HMODULE, LoadedModule*> modules_by_handle;
	
	TimerMeasurement ttry_get_and_cache_module = TimerMeasurement("try_get_and_cache_module");
	TimerMeasurement tload_pdb = TimerMeasurement("load_pdb");

	ModuleCache (HANDLE hprocess): hprocess{hprocess} {

	}
	void clear () {
		sorted_modules.clear();
		sorted_modules.shrink_to_fit();
		modules_by_handle.clear();
	}

	LoadedModule* find_module_for_addr (uint64_t addr) {
		//ZoneScoped;
		
		// Try find cached data for module loaded in process
		// Linear search shoule be fast enough for the moment
		for (auto& m : sorted_modules) {
			if (addr >= m->base_addr && addr < m->base_addr + m->size) {
				return m.get();
			}
		}
		
		return try_get_and_cache_module(addr);
	}

	LoadedModule* try_get_and_cache_module (uint64_t addr) {
		ZoneScoped;
		TimerMeasZone(ttry_get_and_cache_module);

		// Get a list of handles of all loaded modules in process
		// NOTE: that later calls can return different lists as dlls can be loaded and unloaded
		// We never check for dlls having been unloaded for performance reasons, this matches tracy behavior, but is technically wrong? (TODO: properly think about this and document?)
		// Instead once an symbol at an address that is not in a cached module is queried,
		// we check all modules that are loaded at that point in time and then pretend that any modules we find are never unloaded
		
		// VirtualQueryEx mainly is meant to return consecutive ranges of pages with identical properies
		// but apparently it also returns AllocationBase, which I think is the base address of the VirtualAlloc call that the page was part of
		// (despite the resulting pages later getting various properies)
		// Apparently this means any address inside a loaded module will make VirtualQueryEx return the base address of it in AllocationBase (same thing as HMODULE)
		// Which is way faster than EnumProcessModules
		MEMORY_BASIC_INFORMATION vq = {};
		SIZE_T res = VirtualQueryEx(hprocess, (LPCVOID)addr, &vq, sizeof(vq));
		// AllocationBase == 0 is returned if querying free pages, but passing 0 into GetModuleInformation seems to cause it to return current exe instead?
		if (res != sizeof(vq) || vq.Type != MEM_IMAGE || vq.AllocationBase == 0) {
			return nullptr;
		}
		
		MODULEINFO info = {};
		if (!GetModuleInformation(hprocess, (HMODULE)vq.AllocationBase, &info, sizeof(info))) {
			return nullptr;
		}

		// sanity check
		auto base = (uint64_t)info.lpBaseOfDll;
		auto size = (size_t)info.SizeOfImage;

		assert(vq.AllocationBase == info.lpBaseOfDll);
		assert(addr >= base && addr < base + size);

		if (!(vq.AllocationBase == info.lpBaseOfDll && addr >= base && addr < base + size)) {
			return nullptr;
		}

		wchar_t name[MAX_PATH];
		auto name_len = GetModuleFileNameExW(hprocess, (HMODULE)vq.AllocationBase, name, sizeof(name));
		if (name_len <= 0) {
			return nullptr;
		}
		
		// cache newly seen module
		auto path = std::wstring_view(name, name_len);
		auto mod = std::make_unique<LoadedModule>(std::filesystem::path(path), base, size);
		auto* pmod = mod.get();

		sorted_modules.emplace_back(std::move(mod));

		// re-sort
		std::sort(sorted_modules.begin(), sorted_modules.end(),
		[] (std::unique_ptr<LoadedModule> const& l, std::unique_ptr<LoadedModule> const& r) {
			return std::less<uint64_t>()(l->base_addr, r->base_addr);
		});

		{
			TimerMeasZone(tload_pdb);

			_hprocess = hprocess;
			_mod_base = pmod->base_addr;

			pmod->load_pdb();
		}
		return pmod;
	}
};
#endif
