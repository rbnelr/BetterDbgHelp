#pragma once
#include "util.hpp"
#include "codeview.hpp"

struct SourceLoc {
	const char* filepath;
	uint32_t    lineno;

	bool operator== (SourceLoc const& r) const {
		return strcmp(filepath, r.filepath)==0 && lineno == r.lineno;
	}
};
struct SourceLocAndFn {
	const char* fnname = nullptr;
	const char* filepath = nullptr;
	uint32_t    lineno = 0;
};

namespace lineinfo {
	// TODO: explain encoding
	
	inline constexpr size_t ALIGN = 4;

	struct alignas(2) DeltaCoded {
		uint8_t length;
		int8_t lineno_delta; // INT8_MIN => is_gap
	};
	struct alignas(4) Block {
		int32_t start_offset; // first range start
		uint32_t first_length; // handle case of single range length > 255
		uint32_t start_lineno : 31; // first range absolute lineno
		uint32_t is_last : 1; // is this the last block?
		uint32_t sourcefile; // offset in pdb /names string table for all ranges in block
		uint16_t num_deltas; // number of DeltaCoded entires following
	};

	struct SourceRange {
		int32_t offset;
		uint32_t length;
		uint32_t lineno;
		uint32_t sourcefile;
	};
	
	struct Encoder {
		BinAlloc::bid result;

		BinAlloc::bid pblock = -1;
		SourceRange prev = { INT_MIN, 0, 0, 0 };

		Stats& stats;
		
		Encoder (BinAlloc& alloc, Stats& stats): stats{stats} {
			result = alloc.prepare_push<Block>();
		}

		void push (BinAlloc& alloc, SourceRange const& cur) {
			assert(cur.offset >= prev.offset);
			assert(cur.lineno < UINT32_MAX);

			auto try_delta_code = [&] () [[msvc::forceinline]]  {
				if (pblock == -1)
					return false;

				DeltaCoded gap = {};
				bool push_gap;

				// push a gap if we need to
				push_gap = prev.offset + prev.length != cur.offset;
				if (push_gap) {
					int32_t gap_length = cur.offset - (prev.offset + prev.length);
					
					gap.length = (uint8_t)gap_length;
					gap.lineno_delta = INT8_MIN;
			
					bool can_delta_code = gap.length == gap_length;
					if (!can_delta_code) {
						return false;
					}
				}
				
				DeltaCoded range = {};
				range.length = (uint8_t)cur.length;
				range.lineno_delta = (int8_t)(cur.lineno - prev.lineno);
				bool can_delta_code =
					range.length == cur.length &&
					(prev.lineno + range.lineno_delta) == cur.lineno &&
					prev.sourcefile == cur.sourcefile;

				if (!can_delta_code) {
					return false;
				}
				
				int num_push = 1 + (push_gap?1:0);

				auto block = *alloc.get<Block>(pblock);
				if ((uint32_t)block.num_deltas + num_push >= UINT32_MAX) {
					return false;
				}

				if (push_gap) {
					alloc.push_noalign(gap);
					stats.num_lineinfo_delta_coded++;
				}
				alloc.push_noalign(range);
				stats.num_lineinfo_delta_coded++;

				block.num_deltas += num_push;

				*alloc.get<Block>(pblock) = block;
				return true;
			};

			if (!try_delta_code()) {
				auto* prev_block = alloc.get<Block>(pblock);
				if (prev_block) {
					prev_block->is_last = false;
				}

				// Manually push alignment as an optimization vs BinAlloc alignement memset logic
				if (!alloc.is_aliged<Block>()) {
					alloc.v._grow_noalign(sizeof(DeltaCoded), alignof(DeltaCoded));
				}

				Block* block = alloc._push_noalign<Block>(&pblock);
				block->start_offset = cur.offset;
				block->first_length = cur.length;
				block->start_lineno = cur.lineno;
				block->is_last = true;
				block->sourcefile = cur.sourcefile;
				block->num_deltas = 0;

				stats.num_lineinfo_blocks++;
			}
			
			prev = cur;
		}
	};

	template <typename FUNC>
	inline void decode (char* data, FUNC test_range) {
		char* cur = (char*)data;
		Block* block;
		do {
			block = (Block*)cur;
			cur += sizeof(Block);

			int32_t offset = block->start_offset;
			uint32_t length = block->first_length;
			uint32_t lineno = block->start_lineno;
			uint32_t sourcefile = block->sourcefile;
			bool is_gap = false;

			int32_t end_offset = offset + length;
			
			//if (test_range(offset, end_offset, lineno, sourcefile, is_gap))
			//	return;
			//
			//for (uint32_t i=0; i<block->num_deltas; i++) {
			//	offset = end_offset;
			//
			//	auto* delta = (DeltaCoded*)cur;
			//	cur += sizeof(DeltaCoded);
			//
			//	length = delta->length;
			//	end_offset = offset + length;
			//
			//	is_gap = delta->lineno_delta == INT8_MIN;
			//	if (!is_gap)
			//		lineno += delta->lineno_delta;
			//
			//	if (test_range(offset, end_offset, lineno, sourcefile, is_gap))
			//		return;
			//}
			
			char* end = cur + sizeof(DeltaCoded)*block->num_deltas;
			DeltaCoded* delta;
			goto Ltest;

			do {
				
				delta = (DeltaCoded*)cur;
				cur += sizeof(DeltaCoded);
			
				offset = end_offset;
				length = delta->length;
				end_offset = offset + length;
				
				is_gap = delta->lineno_delta == INT8_MIN;
				if (!is_gap)
					lineno += delta->lineno_delta;
				
			Ltest:
				if (test_range(offset, end_offset, lineno, sourcefile, is_gap))
					return;
			} while (cur < end);

			// align up to 4 bytes
			cur = (char*)(((uintptr_t)cur + 0b11) & ~0b11);
		} while (!block->is_last);
	}

	// use template for lambda to avoid header source due to circular dep PDB_File <-> Lineinfo
	template <typename FUNC>
	inline BinAlloc::bid encode_c13_lineinfo (
			codeview_subsection_header* subsec, int32_t symbol_offset,
			FUNC extract_checksums_str, BinAlloc& alloc, Stats& stats
		) {
		// Normally one Line header exists per function
		// with the section and offset being equal, ie. the resulting module_raddr being equal
		// so we can assign the lineinfo to the symbol and find lineinfo for a symbol after the symbol lookup
		// but I saw an exception:
		// __security_check_cookie : src\vctools\crt\vcstartup\src\gs\amd64\amdsecgs.asm
		// This asm file has the codeview_line_header with an offset 16 bytes before the function symbol
		// I fixed this by making the lookup for the symbol more complicated
		// but this suggests that for asm files, there may only be one set of lineinfo, but there can be multiple functions and thus symbols in this asm file
		// this would complicate my approach, either do a separate lookup for lineinfo like dbghelp (slow!)
		// (but could do this second lookup only as a fallback, and cache lineinfo reference in symbol if there is one match)
		// but likely the model that each symbol has zero or one lineinfo, not 2 still holds, so could place lineinfo data between symbols and place references in symbols that overlap it
		
		// Line => Code range that has the same line number
		// Block => Consecutive lines where all come from the same file
		// Usually all of a functions code comes from one file, exceptions:
		// c++ #include in the middle of functions (very rare)
		// c++ constructors that have assignment of fields in the class in the header, and the actual ctor code in the source

		// codeview_lines seems to be sorted by offset, ie code address relative to start of function
		// there is only offset, no size, so I assume any addresses between this offset and the next belong to the line as well
		// lines can be out of order (earlier instructions belonging to later lines due to compiler optimizations for example)
		// lines will be missing (empty lines or lines with no generated code)
		// different entries can have the same line (single line to multiple instruction spans)
		// the same offset can appear twice with different lines (I guess multiple related lines that do one thing, maybe also when a statement is split over lines?)
		// dbghelp does this in a somewhat unexpected way (which seems wrong to me, but I'll match its behavior here)
		// Example:
		// offset 0: Lino: 90
		// offset 0: Lino: 92
		// offset 7: Lino: 99
		// SymGetLineFromAddr64(function+0) => Lino:90
		// SymGetLineFromAddr64(function+1) => Lino:92
		// SymGetLineFromAddr64(function+5) => Lino:92
		// SymGetLineFromAddr64(function+7) => Lino:99

		// the algorithm seems to be simple, remember line on addr>=line, break if addr<line, last line is returned
		
		stats.num_lineinfo++;
		Encoder encoder(alloc, stats);

		auto* ptr = (char*)subsec;
		ptr += sizeof(codeview_subsection_header);
		auto* end = ptr + subsec->length;
		
		auto* header = (codeview_line_header*)ptr;
		ptr += sizeof(codeview_line_header);
		assert(header->flags == 0); // CV_LINES_HAVE_COLUMNS not implemented
		
		bool has_prev_range = false;
		SourceRange prev_range = {};

		while (ptr < end) {
			auto* line_block = (codeview_line_block_header*)ptr;
			ptr += sizeof(codeview_line_block_header);
			
			auto sourcefile = extract_checksums_str(line_block->offset_in_file_checksums);

			auto* lines = (codeview_line*)ptr;
			for (u32 i=0; i<line_block->amount_of_lines; i++) {
				auto& line = lines[i];
				
				if (i > 0) assert(line.offset >= lines[i-1].offset); // verify sorted

				int32_t offset = (int32_t)line.offset - symbol_offset;

				// Since I'm not using the end field at all here, we don't need to overcomplicate via this prev_range thing
				// TODO: see how compressed annotations work and if we can get rid of the end field
				if (has_prev_range) {
					prev_range.length = offset - prev_range.offset;

					encoder.push(alloc, prev_range);
				}

				prev_range.offset = offset;
				prev_range.length = 0;
				prev_range.lineno = line.start_line_number;
				prev_range.sourcefile = sourcefile;
				has_prev_range = true;
			}

			ptr += line_block->amount_of_lines * sizeof(codeview_line);
		}
		assert(ptr == end);
		
		if (has_prev_range) {
			// Currently unused during decoding, as c13 data has no gaps
			// could try setting this to max allowed length as well, or set it based on symbol size
			// or making 0 mean infinite if decoding is find with an extra branch
			prev_range.length = 0;

			encoder.push(alloc, prev_range);
		}

		return encoder.result;
	}
	
	template <typename FUNC>
	inline BinAlloc::bid encode_compressed_annotation (
			PCompressedAnnotation annotations, PCompressedAnnotation anno_end,
			u32 initial_fileId, u32 initialSourceLineNum,
			FUNC extract_checksums_str, BinAlloc& alloc, Stats& stats
		) {

		struct Line {
			u32 code_offset;
			u32 code_length;
			u32 lineno;
			u32 num_lines; // could mean one code range can be associated with a range of line numbers(?)
			u32 file_id;
			u32 kind;
		};

		stats.num_lineinfo++;
		Encoder encoder(alloc, stats);

		auto emit_range = [&] (Line& line) {
			auto sourcefile = extract_checksums_str(line.file_id);

			SourceRange range;
			range.offset = (int32_t)line.code_offset;
			range.length = line.code_length;
			range.lineno = line.lineno;
			range.sourcefile = sourcefile;

			encoder.push(alloc, range);
		};

		auto* cur = annotations;
		
		u32 file_id = initial_fileId;
		u32 code_offset_base = 0;
		u32 code_offset = 0;
		u32 code_length = 0; // 0 = null
		u32 lineno = initialSourceLineNum;
		u32 num_lines = 1;
		u32 kind = 1; // 0 == Expression, 1 == Statement
		
		Line prev_line;
		bool has_prev_line = false;

		while (cur < anno_end) {
			auto opcode = (BinaryAnnotationOpcode)CVUncompressData(cur);
			if (opcode == BA_OP_Invalid)
				continue;

			auto param1 = CVUncompressData(cur);
			switch (opcode) {
				case BA_OP_CodeOffset: {
					code_offset = param1;
				} break;
				case BA_OP_ChangeCodeOffsetBase: {
					assert(false);
					// Is this never used?
					code_offset_base = param1;
				} break;
				case BA_OP_ChangeCodeOffset: {
					code_offset += param1;
				} break;
				case BA_OP_ChangeCodeLength: {
					if (has_prev_line) {
						if (prev_line.code_length == 0 && prev_line.kind == kind) {
							prev_line.code_length = param1;

							emit_range(prev_line);
						}
					}
					code_offset += param1;
				} break;
				case BA_OP_ChangeFile: {
					// supposedly there are bugs with file changes inside functions, but presumably functions are almost always inside one file, so this should be rare anyway
					// TODO: these file_ids likely are local to the module that contained this INLINESITESYM with CompressedAnnotation,
					// while my current lookup for inlinee id and InlineeSourceLine can come from different modules as that data seems to be duplicated
					file_id = param1;
				} break;
				case BA_OP_ChangeLineOffset: {
					lineno += DecodeSignedInt32(param1);
				} break;
				case BA_OP_ChangeLineEndDelta: {
					num_lines = param1;
				} break;
				case BA_OP_ChangeRangeKind: {
					assert(param1 == 0 || param1 == 1);
					kind = param1;
				} break;
				case BA_OP_ChangeCodeOffsetAndLineOffset: {
					// param : ((sourceDelta << 4) | CodeDelta)
					u32 CodeDelta = param1 & 0b1111;
					s32 sourceDelta = DecodeSignedInt32(param1 >> 4); // signed int encoded weirdly because CVUncompressData chops off upper bits

					code_offset += CodeDelta;
					lineno += sourceDelta;
				} break;
				case BA_OP_ChangeCodeLengthAndCodeOffset: {
					auto param2 = CVUncompressData(cur);
					code_length = param1;
					code_offset += param2;
				} break;

				case BA_OP_ChangeColumnStart:
				case BA_OP_ChangeColumnEndDelta:
				case BA_OP_ChangeColumnEnd: {
					// ignore column info
				} break;

				default: {
					assert(false);
				}
			}
			
			switch (opcode) {
				case BA_OP_ChangeCodeOffset:
				case BA_OP_ChangeCodeOffsetAndLineOffset:
				case BA_OP_ChangeCodeLengthAndCodeOffset: {
					// code_length is either explicitly given with BA_OP_ChangeCodeLengthAndCodeOffset
					// or written after push_line with BA_OP_ChangeCodeLength
					// or implicitly computed from last and current offset

					u32 offset = code_offset + code_offset_base;
					if (has_prev_line) {
						if (prev_line.code_length == 0 && prev_line.kind == kind) {
							prev_line.code_length = offset - prev_line.code_offset;
							
							emit_range(prev_line);
						}
					}

					// 'push' new line, instead of vector just keep last line
					prev_line = {
						offset,
						code_length,
						lineno,
						num_lines,
						file_id,
						kind,
					};
					has_prev_line = true;
					
					if (code_length > 0) {
						emit_range(prev_line);
					}

					code_length = 0;
				} break;
			}
		}

		if (has_prev_line) {
			//assert(prev_line.code_length > 0); // We expect see a code_length established at the end
			// I see cases where we got BA_OP_ChangeCodeLength with length 0, which trips the assert, I don't know why the code was emitted like this
			// since length 0 presumably means line info for a 0 byte range?
		}
		
		return encoder.result;
	}

	inline bool find_line_for_addr (char* data, uintptr_t rel_addr, StrAlloc const& stralloc, SourceLoc* out_src_loc) {
		assert((uintptr_t)data % ALIGN == 0);

		uint32_t found_sourcefile = 0;
		uint32_t found_lineno = UINT32_MAX;
		
		decode(data, [&] (uint32_t offset, uint32_t end_offset, uint32_t lineno, uint32_t sourcefile, bool is_gap) {
			if (is_gap) lineno = UINT32_MAX;

			// first line with addr==line is returned
			// if addr in gap between lines, last seen line with addr>line is returned
			if (rel_addr < offset) {
				return true; // stop iterating
			}
		
			found_sourcefile = sourcefile;
			found_lineno = lineno;
		
			if (rel_addr == offset) {
				return true; // stop iterating
			}

			return false;
		});
		
		if (found_lineno != UINT32_MAX) {
			*out_src_loc = {
				stralloc[found_sourcefile],
				found_lineno
			};
			return true;
		}
		return false;
	}

	// TODO: due to lineinfo acting weird,
	// for the moment develop a replacement for binary annotations first, then make it work with lineinfo afterwards
	inline bool find_line_for_addr_for_inline (char* data, uintptr_t rel_addr, StrAlloc const& stralloc, SourceLoc* out_src_loc) {
		assert((uintptr_t)data % ALIGN == 0);

		bool found = false;
		decode(data, [&] (uint32_t offset, uint32_t end_offset, uint32_t lineno, uint32_t sourcefile, bool is_gap) {
			// return addr>offset && addr<=offset+length if not gap
			if (rel_addr < end_offset) {
				if (!is_gap && rel_addr >= offset) {
					*out_src_loc = {
						stralloc[sourcefile],
						lineno
					};
					found = true;
					return true; // stop iterating
				}
				// ranges are sorted, so any later ranges cannot match
				return true; // stop iterating
			}
			return false;
		});
		return found;
	}
}
