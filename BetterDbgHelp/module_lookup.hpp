#pragma once
#include "util.hpp"
#include "pdb.hpp"

#include <psapi.h>
#pragma comment(lib, "Kernel32.lib")

struct LoadedModule {
	std::string path;

	uintptr_t base_addr;
	size_t size;

	std::unique_ptr<PDB_File> pdb;

	LoadedModule (std::string&& path, uintptr_t base_addr, size_t size) {
		this->path = std::move(path);
		this->base_addr = base_addr;
		this->size = size;
	}
	void load_pdb () {
		// Techically there might be more correct ways to find the pdb, and also ways that allow getting pdbs from microsoft servers
		// see above link
		auto pdb_path = std::filesystem::path(path);
		pdb_path.replace_extension({".pdb"});
		pdb = PDB_File::try_load_pdb(pdb_path.string());
	}
};
struct ModuleCache {
	TimerMeasurement ttry_get_and_cache_module = TimerMeasurement("try_get_and_cache_module");
	TimerMeasurement tload_pdb = TimerMeasurement("load_pdb");

	std::vector<LoadedModule> sorted;
		
	LoadedModule* cache (LoadedModule&& m) {
		auto base_addr = m.base_addr;
		sorted.push_back(std::move(m));
		// re-sort
		std::sort(sorted.begin(), sorted.end(), [] (LoadedModule const& l, LoadedModule const& r) {
			return std::less<uintptr_t>()(l.base_addr, r.base_addr);
		});

		for (auto& m : sorted) {
			if (base_addr == m.base_addr) return &m;
		}
		assert(false);
		return nullptr;
	}

	const LoadedModule* find_module_for_addr (HANDLE inspectee, uintptr_t addr) {
		for (auto& m : sorted) {
			if (addr >= m.base_addr && addr < m.base_addr + m.size) {
				return &m;
			}
		}
			
		return try_get_and_cache_module(inspectee, addr);
	}

	const LoadedModule* try_get_and_cache_module (HANDLE inspectee, uintptr_t addr) {
		LoadedModule* loaded = nullptr;
		{
			TimerMeasZone(ttry_get_and_cache_module);
			HMODULE modules[1024];
			DWORD needed = 0;
			if (!EnumProcessModules(inspectee, modules, sizeof(modules), &needed) || needed > sizeof(modules)) { // TODO: properly handle error
				print_err_throw("EnumProcessModules");
			}

			// only return, and cache, the module that addr was in (as opposed to simply aching anything GetModuleInformation returns)
			// this causes more EnumProcessModules calls, but could help might make find_module_for_addr faster in the case where modules are never queried
			for (int i=0; i<needed/sizeof(HMODULE); i++) {
				auto& mod = modules[i];

				MODULEINFO info = {};
				if (GetModuleInformation(inspectee, mod, &info, sizeof(info))) {
					auto base = (uintptr_t)info.lpBaseOfDll;
					auto size = (size_t)info.SizeOfImage;
					if (addr >= base && addr < base + size) {
						char name[1024];
						auto nameLength = GetModuleFileNameExA(inspectee, mod, name, sizeof(name));
						if (nameLength > 0) {
							loaded = cache(LoadedModule(std::string(name, nameLength), base, size));
							break;
						}
					}
				}
			}
		}
		if (loaded) {
			TimerMeasZone(tload_pdb);
			loaded->load_pdb();
		}
		return loaded;
	}
};
