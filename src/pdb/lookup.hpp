#pragma once
#include "util.hpp"
#include "codeview.hpp"
#include "pdb_locator.hpp"
#include "address_index.hpp"

#include <map>
#include <cstdlib>
#include <variant>

// HACK: for the moment so I can check things during pdb parsing against dbghelp
inline HANDLE _hprocess = INVALID_HANDLE_VALUE;
inline uint64_t _mod_base = 0;

struct Stats {
	size_t num_inlinesites = 0;
	size_t num_lineinfo = 0;
	size_t num_lineinfo_blocks = 0;
	size_t num_lineinfo_delta_coded = 0;
	size_t num_strings = (size_t)-1;
};

#include "lineinfo.hpp"

// https://github.com/PascalBeyer/PDB-Documentation

struct Symbol {
	uint64_t base_addr = 0; // relative to module
	uint32_t size = 0;
	StrAlloc::sid name = -1;
	
//// Info not needed by profilers but needed to replicate SYMBOL_INFO from SymFromAddr
	// Value, Register, Scope seem to always be 0 for SymFromAddr
	//uint32_t si_flags = 0;
	SymTagEnum si_tag = SymTagEnum::SymTagNull;

	uint8_t inline_depth = 0;
	int16_t module_index = -1;

	// Could eliminate now that allocated consecutive
	// possibly via single inlinesites offset
	BinAlloc::bid p_lineinfo = -1;
	BinAlloc::bid p_inlinesites = -1;

	__forceinline uint64_t get_addr () const {
		return base_addr;
	}
	static __forceinline Symbol dummy (uint64_t base_addr) { // needed for std::upper_bound
		return Symbol { base_addr };
	}
};

struct Inlinesite {
	StrAlloc::sid fnname;
	BinAlloc::bid pSibling = -1;
	BinAlloc::bid pChildren = -1;
	// followed by Lineinfo

	const char* get_lineinfo () const {
		return (const char*)(this+1);
	}

	Inlinesite () {};
};
static_assert(alignof(Inlinesite) >= lineinfo::ALIGN);
static_assert(sizeof(Inlinesite) % lineinfo::ALIGN == 0);

static_assert(alignof(Symbol) == 8);
static_assert(alignof(Inlinesite) == 4);
static_assert(lineinfo::ALIGN == 4);

// Lookup structure that contains spans to the actual data living somewhere else
class FastPdbLookupBase {
public:
	const std::filesystem::path pdb_path;
	
	const AddressIndex symbol_index;

	const std::span<const BinAlloc::bid> symbols;

	const BinAlloc::Span binalloc;
	const StrAlloc::Span stralloc;

	// copy since it's easier
	Stats stats;
	uint64_t fastpdb_file_size = 0;

	Symbol const& get_sym (uint32_t idx) {
		return *binalloc.get<Symbol>(symbols[idx]);
	}
	
	void print_symbols () {
		for (uint32_t i=0; i<(uint32_t)symbols.size(); i++) {
			auto& s = get_sym(i);
			logf(">> %4llx %4x mod=%4d %s\n", s.base_addr, s.size, s.module_index, stralloc[s.name]);
		}
	}

	Symbol const* find_symbol_for_addr (uint64_t addr, uint32_t* out_sym_idx) {
		//ZoneScoped;
		
		uint32_t idx = symbol_index.upper_bound((int64_t)addr);
		if (idx <= 0) {
			// first symbol after addr is first symbol, search failed
			return nullptr;
		}
		idx--;
		
		*out_sym_idx = idx;
		return binalloc.get_unchecked<const Symbol>(symbols[idx]);
	}

	bool find_source_loc_for_addr (Symbol const* sym, uint64_t addr, SourceLoc* out_src_loc) {
		//ZoneScoped;
		
		auto* lineinfo = binalloc.get<const char>(sym->p_lineinfo);
		if (!lineinfo)
			return false;

		assert(addr >= sym->base_addr);
		if (addr >= sym->base_addr + sym->size) {
			// past symbol address range, no valid line number
			return false;
		}
		uint64_t sym_raddr = addr - sym->base_addr;
		
		return lineinfo::find_line_for_addr(lineinfo, sym_raddr, stralloc, out_src_loc);
	}
	
	int trace_inlinesites_for_addr (Symbol const* sym, uint64_t addr, SourceLocAndFn* out_locs, int num_locs) {
		//ZoneScoped;
		assert(sym->inline_depth > 0); // only call when actually needed!

		uint64_t proc_raddr = addr - sym->base_addr;

		int depth = 0;
		
		BinAlloc::bid site_id = sym->p_inlinesites;
		while (depth < num_locs && site_id >= 0) {
			auto* site = binalloc.get<Inlinesite>(site_id);
			SourceLoc encoded_loc = {};

			if (lineinfo::find_line_for_addr_for_inline(site->get_lineinfo(), proc_raddr, stralloc, &encoded_loc)) {
				// Matching Inlinesite

				out_locs[depth].fnname = stralloc[site->fnname];
				out_locs[depth].filepath = encoded_loc.filepath;
				out_locs[depth].lineno = encoded_loc.lineno;
				out_locs[depth].line_start_offset = encoded_loc.line_start_offset;
				depth++;

				// site->pChildren != null: recurse into subtree
				// site->pChildren == null: leaf site found => matching branch explored; stop
				site_id = site->pChildren;
			}
			else {
				// Non-matching Inlinesite
				
				// site->pSibling != null: look at next sibling
				// site->pSibling == null: last sibling checked => matching branch explored; stop
				site_id = site->pSibling;
			}
		}
		return depth;
	}

	void print_stats () {
		// do this lazily as string pushes are complex
		if (stats.num_strings == -1) {
			stats.num_strings = count_strings();
		}

		logf("@ PDB %s:\n", pdb_path.string().c_str());
		if (fastpdb_file_size > 0)
			logf("@ fastpdb file size: %.3f MB\n", fastpdb_file_size/(1024.0f*1024.0f));

		symbol_index.print_stats(symbols.size(), "symbol index");
		logf("binalloc             :         | %.1f kB\n", binalloc.size()/1024.0f);
		logf("stralloc             : %7llu | %.1f kB\n", stats.num_strings, stralloc.size()/1024.0f);
		logf("symbols              : %7llu | %.1f kB\n", symbols.size(), symbols.size() * sizeof(Symbol)/1024.0f);
		logf("inlinesites          : %7llu | %.1f kB\n", stats.num_inlinesites, stats.num_inlinesites * sizeof(Inlinesite)/1024.0f);
		logf("lineinfos            : %7llu\n", stats.num_lineinfo);
		logf("lineinfo_blocks      : %7llu | %.1f kB\n", stats.num_lineinfo_blocks, stats.num_lineinfo_blocks * sizeof(lineinfo::Block)/1024.0f);
		logf("lineinfo_delta_coded : %7llu | %.1f kB\n", stats.num_lineinfo_delta_coded, stats.num_lineinfo_delta_coded * sizeof(lineinfo::DeltaCoded)/1024.0f);
	}
	size_t count_strings () const {
		char const* cur = stralloc.v.data();
		char const* end = cur + stralloc.v.size();

		size_t count = 0;
		while (cur < end) {
			cur += strlen(cur)+1;
			count++;
		}
		return count;
	}
};

// Lookup data stored in-memory in vectors for non cached use case
// Temporarily created by PdbReader, then serialized into file or directly used by testing code
class FastPdbLookupData {
public:
	std::filesystem::path pdb_path;
	
	AddressIndex::Data symbol_index;

	std::vector<BinAlloc::bid> symbols;

	BinAlloc binalloc;
	StrAlloc stralloc;

	Stats stats;

	Symbol& get_sym (uint32_t idx) {
		return *binalloc.get<Symbol>(symbols[idx]);
	}
	
	FastPdbLookupBase get_lookup () {
		return FastPdbLookupBase{
			pdb_path, // copy path
			symbol_index.get_lookup(),
			symbols,
			binalloc.span(),
			stralloc.span(),
			stats
		};
	}
};

// Lookup data stored in previously serialized memory-mapped file
class FastPdbLookupFile {
public:
	struct Header {
		static inline constexpr char FILE_SIGNATURE[12] = "hxc_fastpdb";
		static inline constexpr uint32_t FORMAT_VERSION = 1;

		char     signature[12];
		uint32_t format_version;
		GUID     guid;
		DWORD    age;

		uint32_t index_blocks_count;
		uint32_t symbols_count;
		
		uint64_t size_of_file;

		uint64_t index_addresses_offset;
		uint64_t index_blocks_offset;
		uint64_t index_block_indices_offset;

		uint64_t symbols_list_offset;

		uint64_t binary_data_offset;
		uint64_t binary_data_size;

		uint64_t string_data_offset;
		uint64_t string_data_size;

		Stats    stats;
	};
	
	//std::filesystem::path fastpdb_path;
	MemoryMappedFile file;

	Header* header () {
		return (Header*)file.data();
	}
	static size_t align_up (size_t x, size_t align) {
		return (x + align-1) / align * align;
	}

	// Always place our cached .fastpdb next to wherever we would normally attempt to load the pdb to parse from
	// This way symbol cache entries can get permanently cache .fastpdb
	// and when executables a user recompiles overwrite the pdb, we also overwrite our .fastpdb by detecting the guid mismatch, and avoid bloating temp or similar folders
	// Note that this pdb was not yet opened, just that the file exists
	static std::filesystem::path get_fastpdb_cache_path (std::filesystem::path const& located_pdb_path) {
		auto fastpdb_path = located_pdb_path;
		fastpdb_path.replace_extension({".fastpdb"});
		return fastpdb_path;
	}
	static std::optional<FastPdbLookupFile> try_load_fastpdb_file (std::filesystem::path const& fastpdb_path, PDB_Locator::PDB_guid_and_age const& check_rsds) {
		FastPdbLookupFile f;
		uint64_t file_size;
		if (!f.file.open_read_only(fastpdb_path, &file_size)) {
			// File not found
			return {};
		}

		if (file_size < sizeof(Header)) return {};

		auto* header = f.header();
		// check file signature
		if (memcmp(header->signature, Header::FILE_SIGNATURE, sizeof(Header::FILE_SIGNATURE)) != 0) return {};
		// check version compatibility
		if (header->format_version != Header::FORMAT_VERSION) return {};
		// check pdb match
		if (memcmp(&header->guid, &check_rsds.guidSig, sizeof(header->guid)) != 0 ||
			header->age != check_rsds.age) return {};
		// check file size
		if (file_size < header->size_of_file) return {};

		return f;
	}
	FastPdbLookupBase get_lookup (std::filesystem::path&& pdb_path) {
		auto* h = header();
		auto* ptr = (const char*)file.data();
		return FastPdbLookupBase{
			pdb_path,
			AddressIndex{
				h->index_blocks_count,
				(const int64_t*             )(ptr + h->index_addresses_offset),
				(const AddressIndex::Block* )(ptr + h->index_blocks_offset),
				(const uint32_t*            )(ptr + h->index_block_indices_offset)
			},
			std::span<const BinAlloc::bid>( (BinAlloc::bid*)(ptr + h->symbols_list_offset), h->symbols_count ),
			BinAlloc::Span{ std::span<const char>( ptr + h->binary_data_offset, h->binary_data_size ) },
			StrAlloc::Span{ std::span<const char>( ptr + h->string_data_offset, h->string_data_size ) },
			h->stats,
			h->size_of_file
		};
	}

	static bool try_cache_fastpdb_file (FastPdbLookupData const& data, std::filesystem::path const& fastpdb_path, PDB_Locator::PDB_guid_and_age const& rsds) {
		
		static constexpr size_t CACHELINE_ALIGN = 64;
		static_assert(AddressIndex::ALIGNMENT == CACHELINE_ALIGN);
		
		assert(data.symbol_index.addresses.size() == data.symbol_index.blocks.size());
		assert(data.symbol_index.addresses.size() == data.symbol_index.block_indices.size());

		uint64_t total_size = 0;

		Header header = {};
		total_size += sizeof(Header);

		memcpy(header.signature, Header::FILE_SIGNATURE, sizeof(Header::FILE_SIGNATURE));
		header.format_version = Header::FORMAT_VERSION;
		header.guid = rsds.guidSig;
		header.age = rsds.age;

		header.index_blocks_count = (uint32_t)data.symbol_index.block_indices.size();
		header.symbols_count = (uint32_t)data.symbols.size();
		
		header.index_addresses_offset = align_up(total_size, CACHELINE_ALIGN);
		uint64_t index_addresses_size = sizeof(int64_t) * header.index_blocks_count;
		total_size = header.index_addresses_offset + index_addresses_size;

		header.index_blocks_offset = align_up(total_size, CACHELINE_ALIGN);
		uint64_t index_blocks_size = sizeof(AddressIndex::Block) * header.index_blocks_count;
		total_size = header.index_blocks_offset + index_blocks_size;

		header.index_block_indices_offset = align_up(total_size, CACHELINE_ALIGN);
		uint64_t index_block_indices_size = sizeof(uint32_t) * header.index_blocks_count;
		total_size = header.index_block_indices_offset + index_block_indices_size;
		
		header.symbols_list_offset = align_up(total_size, CACHELINE_ALIGN);
		uint64_t symbols_list_size = sizeof(BinAlloc::bid) * header.symbols_count;
		total_size = header.symbols_list_offset + symbols_list_size;
		
		header.binary_data_offset = align_up(total_size, CACHELINE_ALIGN);
		header.binary_data_size = data.binalloc.size();
		total_size = header.binary_data_offset + header.binary_data_size;

		header.string_data_offset = align_up(total_size, CACHELINE_ALIGN);
		header.string_data_size = data.stralloc.size();
		total_size = header.string_data_offset + header.string_data_size;
		total_size += 64; // crude pad_for_simd

		header.size_of_file = total_size;

		header.stats = data.stats;
		
		MemoryMappedFile file;
		if (!file.open_new_readwrite(fastpdb_path, total_size)) {
			return false;
		}
		auto* ptr = (char*)file.data();

		memcpy(ptr+0, &header, sizeof(header));

		memcpy(ptr+header.index_addresses_offset, data.symbol_index.addresses.data(), index_addresses_size);
		memcpy(ptr+header.index_blocks_offset, data.symbol_index.blocks.data(), index_blocks_size);
		memcpy(ptr+header.index_block_indices_offset, data.symbol_index.block_indices.data(), index_block_indices_size);

		memcpy(ptr+header.symbols_list_offset, data.symbols.data(), symbols_list_size);

		memcpy(ptr+header.binary_data_offset, data.binalloc.v.data(), header.binary_data_size);
		memcpy(ptr+header.string_data_offset, data.stralloc.v.data(), header.string_data_size);

		return true;
	}
};

// Simple wrapper with span-based lookup, but holds either loaded cached file or in-memory data variant
// No overhead on lookups, so can be used without knowing if caching to files happens or not
class FastPdbLookup : public FastPdbLookupBase {
	std::variant<FastPdbLookupData, FastPdbLookupFile> backing;
public:
	static inline bool allow_caching = true;
	
	// Throws exceptions on failure
	static FastPdbLookupData load_non_cached (std::filesystem::path const& exe_path) {
		logf("[BetterDbgHelp] Loading pdb for %s.\n", exe_path.string().c_str());

		// Find pdb_path via file existance, not yet checked
		PDB_Locator locator(exe_path);
		auto pdb_path = locator.get_pdb_path();
		auto rsds = locator.get_rsds();
		// check guid and try parse pdb
		// don't attempt different location if failed
		return parse_pdb(std::move(pdb_path), rsds);
	}
	
	// Throws exceptions on failure
	static FastPdbLookup load (std::filesystem::path const& exe_path) {
		logf("[BetterDbgHelp] Loading pdb for %s.\n", exe_path.string().c_str());

		// Find pdb_path via file existance, not yet checked
		PDB_Locator locator(exe_path);
		auto pdb_path = locator.get_pdb_path();
		auto fastpdb_path = FastPdbLookupFile::get_fastpdb_cache_path(pdb_path);
		auto rsds = locator.get_rsds();

		if (allow_caching) {
			auto file = FastPdbLookupFile::try_load_fastpdb_file(fastpdb_path, rsds);
			if (file.has_value()) {
				logf("[BetterDbgHelp] Loaded cached %s.\n", fastpdb_path.string().c_str());
				return FastPdbLookup(std::move(*file), std::move(pdb_path));
			}
		}
		
		logf("[BetterDbgHelp] Parsing %s.\n", pdb_path.string().c_str());

		// check guid and try parse pdb
		// don't attempt different location if failed
		auto data = parse_pdb(std::move(pdb_path), rsds);

		if (allow_caching) {
			// try caching
			auto success = FastPdbLookupFile::try_cache_fastpdb_file(data, fastpdb_path, rsds);
			if (success) {
				logf("[BetterDbgHelp] Cached %s for faster future lookups.\n", fastpdb_path.string().c_str());

				// try loading just cached file
				auto file = FastPdbLookupFile::try_load_fastpdb_file(fastpdb_path, rsds);
				if (file.has_value()) {
					return FastPdbLookup(std::move(*file), std::move(pdb_path));
				}
			}
		}

		return FastPdbLookup(std::move(data));
	}
	
	static FastPdbLookupData parse_pdb (std::filesystem::path&& pdb_path, PDB_Locator::PDB_guid_and_age const& rsds);

	FastPdbLookup (FastPdbLookupData&& data):
		FastPdbLookupBase(data.get_lookup()),
		backing{std::move(data)} {}

	FastPdbLookup (FastPdbLookupFile&& file, std::filesystem::path&& pdb_path):
		FastPdbLookupBase(file.get_lookup(std::move(pdb_path))),
		backing{std::move(file)} {}
};
