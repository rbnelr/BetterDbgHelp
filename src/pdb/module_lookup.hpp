#pragma once
#include "util.hpp"
#include "pdb_reader.hpp"

#include <psapi.h>
#pragma comment(lib, "Kernel32.lib")

class LoadedModule {
public:
	std::filesystem::path path;
	std::string ansi_path;
	std::string ansi_filename;

	HMODULE handle () const {
		return (HMODULE)base_addr;
	}

	uint64_t base_addr;
	size_t size;

	// rust enum would be nice here
	bool attempted_load = false;
	std::unique_ptr<FastPdbLookup> pdb = nullptr;
	std::unique_ptr<ExportTableQuery> exports = nullptr;
	
	inline bool is_kernel_module () const {
		return (base_addr & (1llu << 63)) != 0;
	}

	LoadedModule (std::filesystem::path&& path, uint64_t base_addr, size_t size) {
		this->path = std::move(path);
		this->base_addr = base_addr;
		this->size = size;

		ansi_path = this->path.string();
		ansi_filename = this->path.filename().string();
	}
	void load_pdb_or_export_table () {
		assert(!pdb && !exports);
		ZoneScopedC(0xffff00);
		
		// export tables are enough for kernel addresses
		if (!is_kernel_module()) {
			pdb = load_pdb(path);
		}
		// fallback to export table
		if (!pdb) {
			exports = load_export_table(path);
		}
		attempted_load = true;
	}

	// lazy load, only needed for kernel modules
	REL_FORCEINLINE void lazy_load () {
		// This check avoids trying to load pdb over and over if both load_pdb and load_export_table ever fail
		if (attempted_load)
			return;
		load_pdb_or_export_table();
	}
	
	static std::unique_ptr<FastPdbLookup> load_pdb (std::filesystem::path const& path) {
		try {
			logf("[BetterDbgHelp] Loading pdb for %s.\n", path.string().c_str());
			return PdbReader::load_lookup_for_exe(path);
		}
		catch (std::exception& ex) {
			logf("!!! PDB loading exception: %s\n", ex.what());
			return nullptr;
		}
	}
	static std::unique_ptr<ExportTableQuery> load_export_table (std::filesystem::path const& path) {
		try {
			logf("[BetterDbgHelp] Loading ExportTable for %s.\n", path.string().c_str());
			return std::make_unique<ExportTableQuery>(path);
		}
		catch (std::exception& ex) {
			logf("!!! Export Table loading exception: %s\n", ex.what());
			return nullptr;
		}
	}
};
// Holds the cached pdb or export table lookup data for the .exe and all loaded .dll of a process
class ModuleCache {
public:
	HANDLE hprocess;
	std::vector<std::unique_ptr<LoadedModule>> sorted_modules;
	ankerl_hashmap<HMODULE, LoadedModule*> modules_by_handle;

	TimerMeasurement ttry_get_and_cache_module = TimerMeasurement("try_get_and_cache_module");
	TimerMeasurement tload_pdb = TimerMeasurement("load_pdb");

	ModuleCache (HANDLE hprocess): hprocess{hprocess} {

		cache_process_driver();
	}
	void clear () {
		sorted_modules.clear();
		sorted_modules.shrink_to_fit();
		modules_by_handle.clear();
	}

	
	void load_module_lookup (LoadedModule* pmod) {
		TimerMeasZone(tload_pdb);

		_hprocess = hprocess;
		_mod_base = pmod->base_addr;

		pmod->load_pdb_or_export_table();
	}

	// I don't fully understand this, but this is what tracy uses to find kernel exe and other files for kernel addresses, like C:\WINDOWS\system32\ntoskrnl.exe
	// TODO: rewrite to avoid stealing code?
	void cache_process_driver () {
		DWORD needed;
		LPVOID dev[4096];
		if (EnumDeviceDrivers(dev, sizeof(dev), &needed) != 0) {
			char windir[MAX_PATH];
			if (!GetWindowsDirectoryA(windir, sizeof(windir))) memcpy(windir, "c:\\windows", 11);
			const auto windirlen = strlen(windir);

			const auto sz = needed / sizeof(LPVOID);
			for (size_t i = 0; i < sz; i++) {
				char fn[MAX_PATH];
				const auto len = GetDeviceDriverBaseNameA(dev[i], fn, sizeof(fn));
				if (len != 0) {

					const auto len = GetDeviceDriverFileNameA(dev[i], fn, sizeof(fn));
					if (len != 0) {
						char full[MAX_PATH];
						char* path = fn;

						if (memcmp(fn, "\\SystemRoot\\", 12) == 0) {
							memcpy(full, windir, windirlen);
							strcpy(full + windirlen, fn + 11);
							path = full;
						}

						DWORD64 baseOfDll = (DWORD64)dev[i];
						DWORD dllSize = 0;

						// cache newly seen module
						auto mod = std::make_unique<LoadedModule>(std::filesystem::path(full), baseOfDll, dllSize);
						auto* pmod = mod.get();
						
						sorted_modules.emplace_back(std::move(mod));

						//load_module_lookup(pmod);
					}
				}
			}
		}

		// re-sort
		std::sort(sorted_modules.begin(), sorted_modules.end(),
		[] (std::unique_ptr<LoadedModule> const& l, std::unique_ptr<LoadedModule> const& r) {
			return std::less<uint64_t>()(l->base_addr, r->base_addr);
		});

		//for (auto& m : sorted_modules) {
		//	printf("%llx %s\n", m->base_addr, m->ansi_filename.c_str());
		//}
	}

	LoadedModule* find_module_for_addr (uint64_t addr) {
		//ZoneScoped;

	#if 0
		// Handle module.size==0, which means assume it goes until next module
		size_t count = sorted_modules.size();
		for (size_t i=0; i<count; i++) {
			if (addr < sorted_modules[i]->base_addr) {
				if (i > 0) {
					// prev module is what 
					auto* mod = sorted_modules[i-1].get();
					assert(addr >= mod->base_addr);

					if (mod->size == 0 || addr < mod->base_addr + mod->size) {
						mod->lazy_load();
						return mod;
					}
				}
				// addr before first module or between modules
				break;
			}
		}
	#else
		// Binary search for modules
		// TODO: user space modules are loaded lazily, but kernel modules are pre-cached
		// could possibly speed this up by keeping two lists, kernel and user space and branching?
		// Should cut down on total iteration count, if kernel/user space is too unpredictable and causes branch misses
		// could also try branchless selection of list
		auto begin = sorted_modules.begin();
		auto it = std::upper_bound(begin, sorted_modules.end(), addr,
		[] (uint64_t addr, std::unique_ptr<LoadedModule> const& entry) {
			return addr < entry->base_addr;
		});

		if (it != begin) {
			assert(addr < (*it)->base_addr);
			it--;
			// prev module is what 
			auto* mod = it->get();
			assert(addr >= mod->base_addr);

			if (mod->size == 0 || addr < mod->base_addr + mod->size) {
				mod->lazy_load();
				return mod;
			}
		}
	#endif

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

		load_module_lookup(pmod);
		return pmod;
	}
};
