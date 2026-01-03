#pragma once
#include "util.hpp"

#include <immintrin.h>

template <typename T>
class AddressIndex {
public:
	std::vector<T> vec;

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
	//T& get (BinAlloc& alloc, int idx) {
	//	return *alloc.get<T>(vec[idx]);
	//}

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
#if 1
	// 'Upoptimized' = Normal binary search on sorted vector
	void sort_and_build_index () {
		ZoneScoped;


		sort_vec();
	}
	std::vector<T>::iterator upper_bound (uintptr_t addr) {
		return std::upper_bound(vec.begin(), vec.end(), T::dummy(addr), _less);
	}

	void print_stats (const char* name) {
		logf("%s: symbol_lookup: #%llu log2: %f size: %.1f kB\n", name, vec.size(), log2f((float)vec.size()), sizeof(vec[0])*vec.size()/1000.0f);
	}
#else
private:
	template <typename T> using align_alloc = AlignedAllocator<T, 64>; // cache line alignment

	static inline constexpr int BLOCKSZ = 16*2;
	static_assert(BLOCKSZ*sizeof(int16_t) % 32 == 0); // 16*u16 fits in AVX reg
	
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
	
	std::vector<intptr_t, align_alloc<intptr_t>> addresses;
	std::vector<Block, align_alloc<Block>> blocks;
	std::vector<unsigned, align_alloc<unsigned>> block_indices;
	
public:
	void print_stats (const char* name) {
		logf("%s: symbol_table: #%llu lookup log2: %f size: %.1f kB\n", name, vec.size(), log2f((float)vec.size()), sizeof(vec[0])*vec.size()/1000.0f);
		
		auto used_slots = vec.size();
		auto num_slots = blocks.size()*BLOCKSZ;

		logf("%s: simd blocks: #blocks %llu addresses: %.1f kB blocks: %.1f kB block_indices: %.1f kB\nwasted %.2f%%\n", name, blocks.size(),
			addresses.size()*sizeof(addresses[0])/1000.0f,
			blocks.size()*sizeof(blocks[0])/1000.0f,
			block_indices.size()*sizeof(block_indices[0])/1000.0f,
			(1.0f - (float)used_slots / num_slots) * 100.0f);
	}

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
		intptr_t block_address = 0;
		intptr_t block_max_addr = 0;
		unsigned block_elem_index;
		unsigned idx_in_block = BLOCKSZ; // causes initial block push
		
		for (unsigned i=0; i<(unsigned)vec.size(); i++) {
			intptr_t addr = (intptr_t)vec[i].get_addr();

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
			intptr_t offset = addr - block_address;

			assert(offset < MAX_OFFSET);
			cur_block->offsets[idx_in_block] = (int16_t)offset;
			idx_in_block++;
		}
	}

	__forceinline std::vector<T>::iterator upper_bound (intptr_t addr) {
		auto it = std::upper_bound(addresses.begin(), addresses.end(), addr);
		if (it <= addresses.begin()) // addr before first item
			return vec.begin();
		
		assert(it == addresses.end() || addr < *it);
		it--;
		assert(addr >= *it);
		int block_idx = (int)(it - addresses.begin());
		
		intptr_t block_addr = *it;
		intptr_t local_addr64 = addr - block_addr;

		// whenever we find see address we want to build into a block, but it is out of range for the block (past 16 bit range)
		// we begin a new block instead, the new block is at the address we saw, so there are gaps between blocks!
		// so this can happen and unfortunately breaks our simd logic
		// can either clamp it and run it through simd, or early out by looking at new block
		int16_t local_addr = (int16_t)std::min(local_addr64, (intptr_t)(MAX_OFFSET-1));
		__m256i simd_addr = _mm256_set1_epi16(local_addr);
		
		auto* ptr = (__m256i const*)blocks[block_idx].offsets;
	#if 0
		unsigned idx = 0;
		for (int i=0; i<BLOCKSZ/16; i++) {
			__m256i values = _mm256_load_si256(ptr + i);
			// there are only two comparison ops available as AVX, > and ==
			// val > addr => addr < val = !(addr >= val)
			// 16x 2 bytes: 0x0000 => addr>=val, 0xffff => addr<val
			__m256i mask = _mm256_cmpgt_epi16(values, simd_addr);
			// turn 16 bit masks into 2 bit masks (1 bit mask version does not exist)
			int bitmask = _mm256_movemask_epi8(mask);
			// count lsb zero bits, ie find index of first 1
			unsigned tzc = (unsigned)_tzcnt_u32(bitmask);
			idx += tzc;
			if (tzc < 32) break;
		}
		unsigned upper_bound_idx = idx / 2;
	#else
		// Unrolled
		static_assert(BLOCKSZ == (16*2));

		__m256i values0 = _mm256_load_si256(ptr);
		__m256i values1 = _mm256_load_si256(ptr+1);
		
		__m256i mask0 = _mm256_cmpgt_epi16(values0, simd_addr);
		__m256i mask1 = _mm256_cmpgt_epi16(values1, simd_addr);
		
		uint64_t bitmask0 = (uint64_t)(unsigned)_mm256_movemask_epi8(mask0);
		uint64_t bitmask1 = (uint64_t)(unsigned)_mm256_movemask_epi8(mask1);
		
		unsigned idx = (unsigned)_tzcnt_u64((bitmask1 << 32llu) | bitmask0);
		assert(idx <= 64);
		int upper_bound_idx = idx>>1;
	#endif

		upper_bound_idx += block_indices[block_idx];
		assert(upper_bound_idx > 0 && upper_bound_idx <= vec.size());
		
		auto res = vec.begin() + upper_bound_idx;
		assert(res == _upper_bound_on_vec(addr));
		return res;
	}
#endif
};
