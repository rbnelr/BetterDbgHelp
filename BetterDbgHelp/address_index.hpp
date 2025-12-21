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
#elif 1
private:

	static inline constexpr int BLOCKSZ = 16*4;
	static_assert(BLOCKSZ % 16 == 0); // u16 fits in AVX reg
	
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
	
	//static inline constexpr int TOPLVL_BLOCKSZ = 16;
	//std::vector<uintptr_t> block_addresses_lvl2;

	std::vector<Block> blocks;
	std::vector<uintptr_t> block_addresses;
	std::vector<int> block_indices;

public:
	void print_stats (const char* name) {
		logf("%s: vec: #%llu log2: %f size: %.1f kB\n", name, vec.size(), log2f((float)vec.size()), sizeof(vec[0])*vec.size()/1000.0f);
		
		//logf("%s: block_addresses_lvl2: #%llu log2: %f size: %.1f kB\n", name,
		//	block_addresses_lvl2.size(), log2f((float)block_addresses_lvl2.size()), sizeof(block_addresses_lvl2[0])*block_addresses_lvl2.size()/1000.0f);

		auto used_slots = vec.size();
		auto num_slots = blocks.size()*BLOCKSZ;

		logf("%s: simd: #blocks %llu block_addresses: %.1f kB blocks: %.1f kB block_indices: %.1f kB\nwasted %.2f%%\n", name, blocks.size(),
			block_addresses.size()*sizeof(block_addresses[0])/1000.0f,
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

		blocks.reserve(vec.size()/BLOCKSZ);
		block_addresses.reserve(vec.size()/BLOCKSZ);
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
				block_addresses.emplace_back(block_address);
				block_indices.emplace_back(block_elem_index);

				block_max_addr = block_address + MAX_OFFSET;
			}
			uintptr_t offset = addr - block_address;

			assert(offset < MAX_OFFSET);
			cur_block->offsets[idx_in_block] = (int16_t)offset;
			idx_in_block++;
		}
		
		//size_t lvl2_size = (block_addresses.size()+TOPLVL_BLOCKSZ-1) / TOPLVL_BLOCKSZ;
		//block_addresses_lvl2.reserve(lvl2_size);
		//for (int i=0; i<(int)lvl2_size; i++) {
		//	block_addresses_lvl2.emplace_back( block_addresses[i*TOPLVL_BLOCKSZ] );
		//}
	}
	std::vector<T>::iterator upper_bound (uintptr_t addr) {
		//auto pblock_addr2 = std::upper_bound(block_addresses_lvl2.begin(), block_addresses_lvl2.end(), addr);
		//if (pblock_addr2 <= block_addresses_lvl2.begin()) // addr before first item
		//	return vec.begin();
		//size_t lvl2_idx = pblock_addr2 - block_addresses_lvl2.begin();
		//size_t lvl2_1 = lvl2_idx * TOPLVL_BLOCKSZ;
		//size_t lvl2_0 = lvl2_1 - TOPLVL_BLOCKSZ;
		//lvl2_1 = std::min(lvl2_1, block_addresses.size());
		//
		//auto pblock_addr = std::upper_bound(block_addresses.begin()+lvl2_0, block_addresses.begin()+lvl2_1, addr);
		auto pblock_addr = std::upper_bound(block_addresses.begin(), block_addresses.end(), addr);
		if (pblock_addr <= block_addresses.begin()) // addr before first item
			return vec.begin();
		// upper bound gives one after equal range (ie is larger than addr)
		// instead addr is inside previous block (larger or equal to start address)
		assert(pblock_addr == block_addresses.end() || addr < *pblock_addr);
		pblock_addr--;
		assert(addr >= *pblock_addr);

		size_t bi = pblock_addr - block_addresses.begin();
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

		int mask0 = _mm256_movemask_epi8(vmask0);
		int mask1 = _mm256_movemask_epi8(vmask1);
		int mask2 = _mm256_movemask_epi8(vmask2);
		int mask3 = _mm256_movemask_epi8(vmask3);

		int idx0 = _tzcnt_u32(mask0);
		int idx1 = _tzcnt_u32(mask1);
		int idx2 = _tzcnt_u32(mask2);
		int idx3 = _tzcnt_u32(mask3);

		if (idx0 < 32) {
			upper_bound_idx = idx0/2;
		} else if (idx1 < 32) {
			upper_bound_idx = idx1/2 + 16*1;
		} else if (idx2 < 32) {
			upper_bound_idx = idx2/2 + 16*2;
		} else if (idx3 < 32) {
			upper_bound_idx = idx3/2 + 16*3;
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
