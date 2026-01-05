#pragma once
#include "util.hpp"

#include <immintrin.h>

class AddressIndex {
	template <typename T> using align_alloc = AlignedAllocator<T, 64>; // cache line alignment

	static inline constexpr int BLOCKSZ = 16*2; // Two AVX regs
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
	void print_stats (size_t count, const char* name) {
		auto used_slots = count;
		auto num_slots = blocks.size()*BLOCKSZ;

		logf("%s: simd blocks: #blocks %llu addresses: %.1f kB blocks: %.1f kB block_indices: %.1f kB\nwasted %.2f%%\n", name, blocks.size(),
			addresses.size()*sizeof(addresses[0])/1000.0f,
			blocks.size()*sizeof(blocks[0])/1000.0f,
			block_indices.size()*sizeof(block_indices[0])/1000.0f,
			(1.0f - (float)used_slots / num_slots) * 100.0f);
	}
	
	template <typename FUNC>
	void build_index (size_t count, FUNC get_addr) {
		ZoneScoped;

		if (count <= 0) return;
		assert(count <= INT_MAX);

		addresses.reserve(count/BLOCKSZ);
		blocks.reserve(count/BLOCKSZ);
		block_indices.reserve(count/BLOCKSZ);

		// use signed 64 bit addresses as they are more convenient, should never overflow
		Block* cur_block = nullptr;
		intptr_t block_address = 0;
		intptr_t block_max_addr = 0;
		unsigned block_elem_index;
		unsigned idx_in_block = BLOCKSZ; // causes initial block push
		
		for (unsigned i=0; i<(unsigned)count; i++) {
			intptr_t addr = (intptr_t)get_addr(i);

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

	__forceinline int upper_bound (intptr_t addr) {
		auto it = std::upper_bound(addresses.begin(), addresses.end(), addr);
		if (it <= addresses.begin()) // addr before first item
			return 0;
		
		assert(it == addresses.end() || addr < *it);
		it--;
		assert(addr >= *it);
		int block_idx = (int)(it - addresses.begin());
		
		static_assert(BLOCKSZ == (16*2));

		// do memory loads early to hide latency (?)
		auto* pvalues = (__m256i const*)blocks[block_idx].offsets;
		__m256i values0 = _mm256_load_si256(pvalues);
		__m256i values1 = _mm256_load_si256(pvalues+1);

		int upper_bound_idx = block_indices[block_idx];

		intptr_t block_addr = *it;
		intptr_t local_addr64 = addr - block_addr;

		// whenever we find see address we want to build into a block, but it is out of range for the block (past 16 bit range)
		// we begin a new block instead, the new block is at the address we saw, so there are gaps between blocks!
		// so this can happen and unfortunately breaks our simd logic
		// can either clamp it and run it through simd, or early out by looking at new block
		int16_t local_addr = (int16_t)std::min(local_addr64, (intptr_t)(MAX_OFFSET-1));
		__m256i simd_addr = _mm256_set1_epi16(local_addr);
		
		__m256i mask0 = _mm256_cmpgt_epi16(values0, simd_addr);
		__m256i mask1 = _mm256_cmpgt_epi16(values1, simd_addr);
		
		uint64_t bitmask0 = (uint64_t)(unsigned)_mm256_movemask_epi8(mask0);
		uint64_t bitmask1 = (uint64_t)(unsigned)_mm256_movemask_epi8(mask1);
		
		unsigned idx = (unsigned)_tzcnt_u64((bitmask1 << 32llu) | bitmask0);
		assert(idx <= 64);
		upper_bound_idx += idx>>1;

		return upper_bound_idx;
	}
};
