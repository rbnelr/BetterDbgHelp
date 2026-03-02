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

	std::unique_ptr<FastPdbLookup> pdb = nullptr;
	std::unique_ptr<ExportTableQuery> exports = nullptr;

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
		
		pdb = load_pdb(path);
		// fallback to export table
		if (!pdb) {
			exports = load_export_table(path);
		}
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

	}
	void clear () {
		sorted_modules.clear();
		sorted_modules.shrink_to_fit();
		modules_by_handle.clear();
	}
	
	// TODO: handle kernel addresses somehow, there is a pdb even for ntoskrnl.exe!
	static inline bool IsKernelAddress(uint64_t addr) {
		return (addr >> 63) != 0;
	}

	LoadedModule* find_module_for_addr (uint64_t addr) {
		//ZoneScoped;

		if (IsKernelAddress(addr)) {
			// These addresses apparently are actual kernel addresses, but VirtualQueryEx returns nothing for them
			// bail to avoid wasting time on search + VirtualQueryEx

			// TODO: Implement this, tracy uses CacheProcessDrivers to find these
			return nullptr;
		}
		
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

			pmod->load_pdb_or_export_table();
		}
		return pmod;
	}
};
