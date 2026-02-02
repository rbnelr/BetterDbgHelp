#pragma once
#include "util.hpp"
#include "pdb.hpp"

#include <psapi.h>
#pragma comment(lib, "Kernel32.lib")

struct LoadedModule {
	std::filesystem::path path;
	std::string ansi_path;

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
	std::vector<LoadedModule> sorted;
	
	TimerMeasurement ttry_get_and_cache_module = TimerMeasurement("try_get_and_cache_module");
	TimerMeasurement tload_pdb = TimerMeasurement("load_pdb");

	ModuleCache (HANDLE hprocess): hprocess{hprocess} {

	}
	void clear () {
		sorted.clear();
		sorted.shrink_to_fit();
	}

	LoadedModule* cache (LoadedModule&& m) {
		auto base_addr = m.base_addr;
		sorted.push_back(std::move(m));
		// re-sort
		std::sort(sorted.begin(), sorted.end(), [] (LoadedModule const& l, LoadedModule const& r) {
			return std::less<uint64_t>()(l.base_addr, r.base_addr);
		});

		for (auto& m : sorted) {
			if (base_addr == m.base_addr) return &m;
		}
		assert(false);
		return nullptr;
	}

	LoadedModule* find_module_for_addr (uint64_t addr) {
		//ZoneScoped;

		// Linear search shoule be fast enough for the moment
		for (auto& m : sorted) {
			if (addr >= m.base_addr && addr < m.base_addr + m.size) {
				return &m;
			}
		}
			
		return try_get_and_cache_module(addr);
	}

	LoadedModule* try_get_and_cache_module (uint64_t addr) {
		ZoneScopedC(0xffff00);

		LoadedModule* loaded = nullptr;
		{
			TimerMeasZone(ttry_get_and_cache_module);
			HMODULE modules[1024];
			DWORD needed = 0;
			if (!EnumProcessModules(hprocess, modules, sizeof(modules), &needed) || needed > sizeof(modules)) { // TODO: properly handle error
				print_err_throw("EnumProcessModules");
			}

			// only return, and cache, the module that addr was in (as opposed to simply caching anything GetModuleInformation returns)
			// this causes more EnumProcessModules calls, but might make find_module_for_addr faster in the case where modules are never queried (tracy probably does not hit many of the dll ever or only rarely)
			// NOTE: I kinda forgot that if an adress outside of a module is queried, we can't cache anything and thus we end up doing EnumProcessModules over and over, which may be slow
			// but I'm not sure there's anything I can do about this, the idea is that at the time of the query, new dlls could have been loaded
			// EnumProcessModules is probably reasonably fast and a profiler practically never hits cases like that (unless code is injected and executed lol)
			// this could still be relevant in some contexts, like debuggers?, but again, not sure how to fix
			// TODO: it may be possible to be notified of dlls when they get loaded and thus handle it more efficiently, but this it's not worth it for now
			for (int i=0; i<needed/sizeof(HMODULE); i++) {
				auto& mod = modules[i];

				MODULEINFO info = {};
				if (GetModuleInformation(hprocess, mod, &info, sizeof(info))) {
					auto base = (uint64_t)info.lpBaseOfDll;
					auto size = (size_t)info.SizeOfImage;
					if (addr >= base && addr < base + size) {
						wchar_t name[MAX_PATH];
						auto nameLength = GetModuleFileNameExW(hprocess, mod, name, sizeof(name));
						if (nameLength > 0) {
							auto path = std::wstring_view(name, nameLength);
							loaded = cache(LoadedModule(std::filesystem::path(path), base, size));
							break;
						}
					}
				}
			}
		}
		if (loaded) {
			TimerMeasZone(tload_pdb);

			_hprocess = hprocess;
			_mod_base = loaded->base_addr;

			loaded->load_pdb();
		}
		return loaded;
	}
};
