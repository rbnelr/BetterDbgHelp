#pragma once
#include "util.hpp"

#include <immintrin.h>

template <typename T>
class AddressIndex {
	std::vector<T> vec;

public:
	void reserve (size_t size) {
		vec.reserve(size);
	}
	void push_unsorted (T&& val) {
		vec.push_back(val);
	}

	static __forceinline int _cmp (T const& l, T const& r) {
		return std::less<uintptr_t>()(l.get_addr(), r.get_addr());
	}
	static __forceinline bool _less (T const& l, T const& r) {
		return l.get_addr() < r.get_addr();
	}

	auto begin () { return vec.begin(); }
	auto end () { return vec.end(); }
	auto size () { return vec.size(); }

	T& operator[] (int idx) {
		return vec[idx];
	}

	void sort_vec () {
		// sort based on base_addr
		// use stable sorts as symbol can and will overlap, so try and preserve insertion order
		// TODO: insertion order may not always actually replicate dbghelp.dll behavior though(?)
		std::stable_sort(vec.begin(), vec.end(), _cmp);
	}
	std::vector<T>::iterator _upper_bound_on_vec (uintptr_t addr) {
		auto dummy = T::dummy(addr);
		return std::upper_bound(vec.begin(), vec.end(), dummy, _less);
	}
#if 0
	// 'Upoptimized' = Normal binary search on sorted vector
	void sort_and_build_index () {
		ZoneScoped;
		sort_vec();
	}
	std::vector<T>::iterator upper_bound (uintptr_t addr) {
		return std::upper_bound(vec.begin(), vec.end(), T::dummy(addr), _less);
	}

	void print_stats (const char* name) {
		logf("%s: vec #%llu log2: %f size: %.1f kB\n", name, vec.size(), log2f((float)vec.size()), sizeof(vec[0])*vec.size()/1000.0f);
	}
#elif 0
private:

	// Simply keep a hot list with only addresses, for binary search to get index with better cache usage
	std::vector<uintptr_t> addr_list;

public:
	// 'Upoptimized' = Normal binary search on sorted vector
	void sort_and_build_index () {
		ZoneScoped;
		sort_vec();

		addr_list.resize(vec.size());
		for (size_t i=0; i<vec.size(); i++) {
			addr_list[i] = vec[i].get_addr();
		}
	}
	std::vector<T>::iterator upper_bound (uintptr_t addr) {
		auto it = std::upper_bound(addr_list.begin(), addr_list.end(), addr);
		auto res = vec.begin() + (it - addr_list.begin());

		if (res < vec.end()) assert(res->get_addr() == *it);
		assert(res == _upper_bound_on_vec(addr));
		return res;
	}

	void print_stats (const char* name) {
		logf("%s: vec #%llu log2: %f size: %.1f kB\n", name, vec.size(), log2f((float)vec.size()), sizeof(vec[0])*vec.size()/1000.0f);
		logf("%s: addr_list #%llu log2: %f size: %.1f kB\n", name, addr_list.size(), log2f((float)addr_list.size()), sizeof(addr_list[0])*addr_list.size()/1000.0f);
	}
#elif 0
private:

	static inline constexpr int BLOCKSZ = 16*4;
	static_assert(BLOCKSZ % 16 == 0); // 16*u16 fits in AVX reg
	
	// TODO: only use 15 bits as avx comparison is only signed, could fix this by offsetting
	// unused offsets are =MAX_OFFSET, and used offsets are <MAX_OFFSET
	// so simd comparison will correctly return upper bound index even on unused slots
	static inline constexpr int16_t MAX_OFFSET = INT16_MAX;

	struct alignas(32) Block {
		int16_t offsets[BLOCKSZ];

		Block () {
			//memset(offsets, 0xff, sizeof(offsets)); // not possible with signed
			for (int i=0; i<BLOCKSZ; i++)
				offsets[i] = MAX_OFFSET;
		}
	};
	
	std::vector<uintptr_t> addresses;

	std::vector<Block> blocks;
	std::vector<int> block_indices;

public:
	void print_stats (const char* name) {
		logf("%s: vec: #%llu log2: %f size: %.1f kB\n", name, vec.size(), log2f((float)vec.size()), sizeof(vec[0])*vec.size()/1000.0f);
		
		auto used_slots = vec.size();
		auto num_slots = blocks.size()*BLOCKSZ;

		logf("%s: simd: #blocks %llu addresses: %.1f kB blocks: %.1f kB block_indices: %.1f kB\nwasted %.2f%%\n", name, blocks.size(),
			addresses.size()*sizeof(addresses[0])/1000.0f,
			blocks.size()*sizeof(blocks[0])/1000.0f,
			block_indices.size()*sizeof(block_indices[0])/1000.0f,
			(1.0f - (float)used_slots / num_slots) * 100.0f);
	}

	// 'Upoptimized' = Normal binary search on sorted vector
	void sort_and_build_index () {
		ZoneScoped;
		sort_vec();

		if (vec.empty()) return;
		assert(vec.size() <= INT_MAX);

		//addresses.reserve(vec.size()/BLOCKSZ);
		blocks.reserve(vec.size()/BLOCKSZ);
		block_indices.reserve(vec.size()/BLOCKSZ);

		// use signed 64 bit addresses as they are more convenient, should never overflow
		Block* cur_block = nullptr;
		uintptr_t block_address = 0;
		uintptr_t block_max_addr = 0;
		int block_elem_index;
		int idx_in_block = BLOCKSZ; // causes initial block push

		for (int i=0; i<(int)vec.size(); i++) {
			uintptr_t addr = vec[i].get_addr();

			// if all slots are filled (or initial case)
			// or if offset would be ==MAX_OFFSET, start new block
			if (idx_in_block >= BLOCKSZ || addr >= block_max_addr) {
				cur_block = &blocks.emplace_back();

				block_address = addr;
				block_elem_index = i;
				idx_in_block = 0;
				addresses.emplace_back(block_address);
				block_indices.emplace_back(block_elem_index);

				block_max_addr = block_address + MAX_OFFSET;
			}
			uintptr_t offset = addr - block_address;

			assert(offset < MAX_OFFSET);
			cur_block->offsets[idx_in_block] = (int16_t)offset;
			idx_in_block++;
		}
	}


	std::vector<T>::iterator upper_bound (uintptr_t addr) {
		auto pblock_addr = std::upper_bound(addresses.begin(), addresses.end(), addr);
		if (pblock_addr <= addresses.begin()) // addr before first item
			return vec.begin();
		// upper bound gives one after equal range (ie is larger than addr)
		// instead addr is inside previous block (larger or equal to start address)
		assert(pblock_addr == addresses.end() || addr < *pblock_addr);
		pblock_addr--;
		assert(addr >= *pblock_addr);

		size_t bi = pblock_addr - addresses.begin();
		uintptr_t block_addr = *pblock_addr;
		uintptr_t local_addr64 = addr - block_addr;

		// whenever we find see address we want to build into a block, but it is out of range for the block (past 16 bit range)
		// we begin a new block instead, the new block is at the address we saw, so there are gaps between blocks!
		// so this can happen and unfortunately breaks our simd logic
		if (local_addr64 >= MAX_OFFSET) {
			// can either clamp it and run it through simd, or early out by looking at new block
			
			//auto upper_bound_idx = block_indices[bi+1];
			//assert(upper_bound_idx > 0 && upper_bound_idx <= vec.size());
			//auto res = vec.begin() + upper_bound_idx;
			//assert(res == _upper_bound_on_vec(addr));
			//return res;

			local_addr64 = MAX_OFFSET-1;
		}
		int16_t local_addr = (int16_t)local_addr64;
		__m256i simd_addr = _mm256_set1_epi16(local_addr);

		int upper_bound_idx = BLOCKSZ;
		
		Block& block = blocks[bi];
	#if 0
		for (int vi=0; vi<BLOCKSZ/16; vi++) {
			int i = vi*16;

			__m256i values = _mm256_load_si256((__m256i const*)&block.offsets[i]);
			// there are only two comparison ops available as AVX, > and ==
			// val > addr => addr < val = !(addr >= val)
			__m256i vmask = _mm256_cmpgt_epi16(values, simd_addr); // 0x0000 => addr>=val, 0xffff => addr<val
			int mask = _mm256_movemask_epi8(vmask); // each u16 mask => 2 bits each
			// count trailing zeros (trailing = lsb) => idx = index of first value > addr, 16 if none
			int idx2 = _tzcnt_u32(mask);
			if (idx2 < 32) {
				upper_bound_idx = idx2/2 + i;
				break;
			}
		}
	#else
		static_assert(BLOCKSZ/16 == 4);
		__m256i values0 = _mm256_load_si256((__m256i const*)&block.offsets[0]);
		__m256i values1 = _mm256_load_si256((__m256i const*)&block.offsets[16*1]);
		__m256i values2 = _mm256_load_si256((__m256i const*)&block.offsets[16*2]);
		__m256i values3 = _mm256_load_si256((__m256i const*)&block.offsets[16*3]);

		__m256i vmask0 = _mm256_cmpgt_epi16(values0, simd_addr);
		__m256i vmask1 = _mm256_cmpgt_epi16(values1, simd_addr);
		__m256i vmask2 = _mm256_cmpgt_epi16(values2, simd_addr);
		__m256i vmask3 = _mm256_cmpgt_epi16(values3, simd_addr);

		int idx;
		if (     (idx = _tzcnt_u32(_mm256_movemask_epi8(vmask0))) < 32) {
			upper_bound_idx = idx/2;
		}
		else if ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask1))) < 32) {
			upper_bound_idx = idx/2 + 16*1;
		}
		else if ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask2))) < 32) {
			upper_bound_idx = idx/2 + 16*2;
		}
		else if ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask3))) < 32) {
			upper_bound_idx = idx/2 + 16*3;
		}
	#endif

		upper_bound_idx += block_indices[bi];
		assert(upper_bound_idx > 0 && upper_bound_idx <= vec.size());
		
		auto res = vec.begin() + upper_bound_idx;
		//auto res2 = _upper_bound_on_vec(addr);
		//auto idx2 = res2 - vec.begin();
		//
		//if (res != res2) {
		//	printf("");
		//}
		//assert(res == res2);
		assert(res == _upper_bound_on_vec(addr));
		return res;
	}
#elif 1
private:

	static inline constexpr int BLOCKSZ = 16*2;
	static_assert(BLOCKSZ % 16 == 0); // 16*u16 fits in AVX reg
	
	// TODO: only use 15 bits as avx comparison is only signed, could fix this by offsetting
	// unused offsets are =MAX_OFFSET, and used offsets are <MAX_OFFSET
	// so simd comparison will correctly return upper bound index even on unused slots
	static inline constexpr int16_t MAX_OFFSET = INT16_MAX;

	struct alignas(32) Block {
		int16_t offsets[BLOCKSZ];

		Block () {
			//memset(offsets, 0xff, sizeof(offsets)); // not possible with signed
			for (int i=0; i<BLOCKSZ; i++)
				offsets[i] = MAX_OFFSET;
		}
	};
	
	static inline constexpr int TOP_BLOCKSZ = 4*2;
	static_assert(BLOCKSZ % 4 == 0); // 4*u64 fits in AVX reg
	
	struct Range { int start; int count; };
	std::vector<Range> tree_levels;
	std::vector<uintptr_t> addresses_tree;
		
	std::vector<uintptr_t> _addresses;

	std::vector<Block> blocks;
	std::vector<int> block_indices;

	void prepare_simd_binary_search_tree (std::vector<uintptr_t> const& addresses) {
		addresses_tree.reserve(addresses.size());
		tree_levels.reserve(8);
		std::vector<int> tree_counts;

		int count = (int)addresses.size();
		do {
			int new_count = (count +TOP_BLOCKSZ-1) / TOP_BLOCKSZ;
			int padded = new_count * TOP_BLOCKSZ;
			tree_levels.push_back({ 0, padded });
			count = new_count;
		} while (count > 1);
		
		std::reverse(tree_levels.rbegin(), tree_levels.rend());

		int total_count = 0;
		for (auto& lvl : tree_levels) {
			lvl.start = total_count;
			total_count += lvl.count;
		}
		addresses_tree.resize(total_count);
		
		int levels = (int)tree_levels.size();

		{
			auto* src = addresses.data();
			auto* dst = addresses_tree.data() + tree_levels[levels-1].start;

			int i=0;
			for (; i<(int)addresses.size(); i++) {
				assert(i < tree_levels[levels-1].count);
				assert(i < addresses.size());
				dst[i] = src[i];
			}
			for (; i<tree_levels[levels-1].count; i++) {
				assert(i < tree_levels[levels-1].count);
				dst[i] = INT64_MAX;
			}
		}
		for (int lvl=levels-2; lvl>=0; lvl--) {
			auto* src = addresses_tree.data() + tree_levels[lvl+1].start;
			auto* dst = addresses_tree.data() + tree_levels[lvl].start;

			int i=0;
			for (; i<tree_levels[lvl+1].count / TOP_BLOCKSZ; i++) {
				assert(i < tree_levels[lvl].count);
				dst[i] = src[i*TOP_BLOCKSZ];
			}
			for (; i<tree_levels[lvl].count; i++) {
				assert(i < tree_levels[lvl].count);
				dst[i] = INT64_MAX;
			}
		}
	}

	__forceinline int search_address_tree (uintptr_t addr, uintptr_t* block_addr) {
	#if 1
		auto pblock_addr = std::upper_bound(_addresses.begin(), _addresses.end(), addr);
		if (pblock_addr <= _addresses.begin()) // addr before first item
			return 0;
		assert(pblock_addr == _addresses.end() || addr < *pblock_addr);
		auto lo = pblock_addr-1;
		assert(addr >= *lo);

		*block_addr = *lo;
		return (int)(pblock_addr - _addresses.begin());
	#elif 0
		if (addresses_tree.empty() || addresses_tree[0] > addr) {
			return -1;
		}

		
		int levels = (int)tree_levels.size();
		assert(levels > 0);

		int match = 1;

		for (int lvl=0;; lvl++) {
			int range0 = (match-1) * TOP_BLOCKSZ;
			int range1 = (match  ) * TOP_BLOCKSZ;
			
			int offs = tree_levels[lvl].start;
			range1 = std::min(tree_levels[lvl].count, range1);

			match = range1;
			for (int i=range0; i<range1; i++) {
				auto val = addresses_tree[i+offs];
				if (val > addr) {
					match = i;
					break;
				}
			}

			assert(match > 0);
			if (lvl >= levels-1) {
				int lo = match-1 + offs;
				int hi = match   + offs;

				assert(hi == tree_levels[lvl].count || addr < addresses_tree[hi]);
				assert(addr >= addresses_tree[lo]);

				*block_addr = addresses_tree[lo];
				return match;
			}
		}
	#else
		if (addresses_tree.empty() || addresses_tree[0] > addr) {
			return -1;
		}

		__m256i simd_addr = _mm256_set1_epi64x(addr);
		
		int levels = (int)tree_levels.size();
		assert(levels > 0);

		int match = 1;

		for (int lvl=0;; lvl++) {
			int range1 = match * TOP_BLOCKSZ;
			int range0 = range1 - TOP_BLOCKSZ;
			
			int offs = tree_levels[lvl].start;
			range1 = std::min(tree_levels[lvl].count, range1);

		#if 0
			match = range1;
			for (int i=0; i<TOP_BLOCKSZ; i+=4) {
				__m256i values0 = _mm256_load_si256((__m256i const*)&addresses_tree[offs + range0 + i]);
				__m256i vmask0 = _mm256_cmpgt_epi64(values0, simd_addr);

				int idx = _tzcnt_u32(_mm256_movemask_epi8(vmask0));
				if (idx < 32) {
					match = range0 + idx/8 + i;
					break;
				}
			}
		#elif 0
			static_assert(TOP_BLOCKSZ == 4*4);
			__m256i values0 = _mm256_load_si256((__m256i const*)&addresses_tree[offs + range0]);
			__m256i values1 = _mm256_load_si256((__m256i const*)&addresses_tree[offs + range0 + 4]);
			__m256i values2 = _mm256_load_si256((__m256i const*)&addresses_tree[offs + range0 + 8]);
			__m256i values3 = _mm256_load_si256((__m256i const*)&addresses_tree[offs + range0 + 12]);

			__m256i vmask0 = _mm256_cmpgt_epi64(values0, simd_addr);
			__m256i vmask1 = _mm256_cmpgt_epi64(values1, simd_addr);
			__m256i vmask2 = _mm256_cmpgt_epi64(values2, simd_addr);
			__m256i vmask3 = _mm256_cmpgt_epi64(values3, simd_addr);

			int idx;
			if      ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask0))) < 32) {
				match = range0 + idx/8;
			}
			else if ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask1))) < 32) {
				match = range0 + idx/8 + 4;
			}
			else if ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask2))) < 32) {
				match = range0 + idx/8 + 8;
			}
			else if ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask3))) < 32) {
				match = range0 + idx/8 + 12;
			}
			else {
				match = range1;
			}
		#else
			match = range1;
			
			static_assert(TOP_BLOCKSZ == (4*2));
			{ int i=0;
			//static_assert(TOP_BLOCKSZ % (4*2) == 0);
			//for (int i=0; i<TOP_BLOCKSZ; i+=8) {
				auto* ptr = (__m256i const*)&addresses_tree[offs + range0 + i];
				__m256i values0 = _mm256_load_si256(ptr);
				__m256i values1 = _mm256_load_si256(ptr+1);

				__m256i vmask0 = _mm256_cmpgt_epi64(values0, simd_addr);
				__m256i vmask1 = _mm256_cmpgt_epi64(values1, simd_addr);
			
				uint64_t bitmask0 =  (uint64_t)(uint32_t)_mm256_movemask_epi8(vmask0);
				uint64_t bitmask1 = ((uint64_t)(uint32_t)_mm256_movemask_epi8(vmask1)) << 32llu;
				uint64_t bitmask = bitmask0 | bitmask1;

				uint64_t idx = _tzcnt_u64(bitmask);
				if (idx < 64llu) {
					match = range0 + (int)(idx/8) + i;
					//break;
				}
			}
		#endif

			assert(match > 0);
			if (lvl >= levels-1) {
				int lo = match-1 + offs;
				int hi = match   + offs;

				assert(hi == tree_levels[lvl].count || addr < addresses_tree[hi]);
				assert(addr >= addresses_tree[lo]);

				*block_addr = addresses_tree[lo];
				return match;
			}
		}
	#endif
	}

public:
	void print_stats (const char* name) {
		logf("%s: vec: #%llu log2: %f size: %.1f kB\n", name, vec.size(), log2f((float)vec.size()), sizeof(vec[0])*vec.size()/1000.0f);
		
		auto used_slots = vec.size();
		auto num_slots = blocks.size()*BLOCKSZ;

		logf("%s: simd: #blocks %llu addresses_tree: %.1f kB blocks: %.1f kB block_indices: %.1f kB\nwasted %.2f%%\n", name, blocks.size(),
			addresses_tree.size()*sizeof(addresses_tree[0])/1000.0f,
			blocks.size()*sizeof(blocks[0])/1000.0f,
			block_indices.size()*sizeof(block_indices[0])/1000.0f,
			(1.0f - (float)used_slots / num_slots) * 100.0f);
	}

	// 'Upoptimized' = Normal binary search on sorted vector
	void sort_and_build_index () {
		ZoneScoped;
		sort_vec();

		if (vec.empty()) return;
		assert(vec.size() <= INT_MAX);

		//addresses.reserve(vec.size()/BLOCKSZ);
		blocks.reserve(vec.size()/BLOCKSZ);
		block_indices.reserve(vec.size()/BLOCKSZ);

		// use signed 64 bit addresses as they are more convenient, should never overflow
		Block* cur_block = nullptr;
		uintptr_t block_address = 0;
		uintptr_t block_max_addr = 0;
		int block_elem_index;
		int idx_in_block = BLOCKSZ; // causes initial block push
		
		for (int i=0; i<(int)vec.size(); i++) {
			uintptr_t addr = vec[i].get_addr();

			// if all slots are filled (or initial case)
			// or if offset would be ==MAX_OFFSET, start new block
			if (idx_in_block >= BLOCKSZ || addr >= block_max_addr) {
				cur_block = &blocks.emplace_back();

				block_address = addr;
				block_elem_index = i;
				idx_in_block = 0;
				_addresses.emplace_back(block_address);
				block_indices.emplace_back(block_elem_index);

				block_max_addr = block_address + MAX_OFFSET;
			}
			uintptr_t offset = addr - block_address;

			assert(offset < MAX_OFFSET);
			cur_block->offsets[idx_in_block] = (int16_t)offset;
			idx_in_block++;
		}

		prepare_simd_binary_search_tree(_addresses);
	}


	__forceinline std::vector<T>::iterator upper_bound (uintptr_t addr) {
		uintptr_t block_addr;
		int bi = search_address_tree(addr, &block_addr);
		if (bi <= 0)
			return vec.begin();
		bi--;

		uintptr_t local_addr64 = addr - block_addr;

		// whenever we find see address we want to build into a block, but it is out of range for the block (past 16 bit range)
		// we begin a new block instead, the new block is at the address we saw, so there are gaps between blocks!
		// so this can happen and unfortunately breaks our simd logic
		if (local_addr64 >= MAX_OFFSET) {
			// can either clamp it and run it through simd, or early out by looking at new block
			
			//auto upper_bound_idx = block_indices[bi+1];
			//assert(upper_bound_idx > 0 && upper_bound_idx <= vec.size());
			//auto res = vec.begin() + upper_bound_idx;
			//assert(res == _upper_bound_on_vec(addr));
			//return res;

			local_addr64 = MAX_OFFSET-1;
		}
		int16_t local_addr = (int16_t)local_addr64;
		__m256i simd_addr = _mm256_set1_epi16(local_addr);

		int upper_bound_idx = BLOCKSZ;
		
		Block& block = blocks[bi];
	#if 0
		for (int vi=0; vi<BLOCKSZ/16; vi++) {
			int i = vi*16;

			__m256i values = _mm256_load_si256((__m256i const*)&block.offsets[i]);
			// there are only two comparison ops available as AVX, > and ==
			// val > addr => addr < val = !(addr >= val)
			__m256i vmask = _mm256_cmpgt_epi16(values, simd_addr); // 0x0000 => addr>=val, 0xffff => addr<val
			int mask = _mm256_movemask_epi8(vmask); // each u16 mask => 2 bits each
			// count trailing zeros (trailing = lsb) => idx = index of first value > addr, 16 if none
			int idx2 = _tzcnt_u32(mask);
			if (idx2 < 32) {
				upper_bound_idx = idx2/2 + i;
				break;
			}
		}
	#elif 0
		static_assert(BLOCKSZ/16 == 4);
		__m256i values0 = _mm256_load_si256((__m256i const*)&block.offsets[0]);
		__m256i values1 = _mm256_load_si256((__m256i const*)&block.offsets[16*1]);
		__m256i values2 = _mm256_load_si256((__m256i const*)&block.offsets[16*2]);
		__m256i values3 = _mm256_load_si256((__m256i const*)&block.offsets[16*3]);

		__m256i vmask0 = _mm256_cmpgt_epi16(values0, simd_addr);
		__m256i vmask1 = _mm256_cmpgt_epi16(values1, simd_addr);
		__m256i vmask2 = _mm256_cmpgt_epi16(values2, simd_addr);
		__m256i vmask3 = _mm256_cmpgt_epi16(values3, simd_addr);

		int idx;
		if (     (idx = _tzcnt_u32(_mm256_movemask_epi8(vmask0))) < 32) {
			upper_bound_idx = idx/2;
		}
		else if ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask1))) < 32) {
			upper_bound_idx = idx/2 + 16*1;
		}
		else if ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask2))) < 32) {
			upper_bound_idx = idx/2 + 16*2;
		}
		else if ((idx = _tzcnt_u32(_mm256_movemask_epi8(vmask3))) < 32) {
			upper_bound_idx = idx/2 + 16*3;
		}
	#else
		static_assert(BLOCKSZ == (16*2));
		{ int i=0;
		//static_assert(BLOCKSZ % (16*2) == 0);
		//for (int i=0; i<BLOCKSZ; i+=16*2) {
			auto* ptr = (__m256i const*)&block.offsets[i];
			__m256i values0 = _mm256_load_si256(ptr);
			__m256i values1 = _mm256_load_si256(ptr+1);
			
			__m256i mask0 = _mm256_cmpgt_epi16(values0, simd_addr);
			__m256i mask1 = _mm256_cmpgt_epi16(values1, simd_addr);

			uint64_t bitmask0 =  (uint64_t)(uint32_t)_mm256_movemask_epi8(mask0);
			uint64_t bitmask1 = ((uint64_t)(uint32_t)_mm256_movemask_epi8(mask1)) << 32llu;
			uint64_t bitmask = bitmask0 | bitmask1;

			uint64_t idx = _tzcnt_u64(bitmask);
			if (idx < 64llu) {
				upper_bound_idx = (int)(idx/2) + i;
				//break;
			}
		}
	#endif

		upper_bound_idx += block_indices[bi];
		assert(upper_bound_idx > 0 && upper_bound_idx <= vec.size());
		
		auto res = vec.begin() + upper_bound_idx;
		//auto res2 = _upper_bound_on_vec(addr);
		//auto idx2 = res2 - vec.begin();
		//
		//if (res != res2) {
		//	printf("");
		//}
		//assert(res == res2);
		assert(res == _upper_bound_on_vec(addr));
		return res;
	}
#else
#endif
};
