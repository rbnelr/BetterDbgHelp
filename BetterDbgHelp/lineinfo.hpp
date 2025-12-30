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

#if 0
class Lineinfo {
	// TODO: in normal c13 lineinfo there is no end, but inline opcodes do encode code size
	// but maybe dbghelp does not use it? check if opcode scheme is always sorted and if it is,
	// check if simply returning the first where offset==addr or next offset > addr gives correct results
	struct CodeRange {
		uint32_t start;
		uint32_t end;
		uint32_t lineno;
		uint32_t sourcefile; // offset in pdb /names string table
	};

	u32 num_ranges = 0;
	CodeRange const* get_ranges () const { return (CodeRange*)(this+1); }

public:
	static BinAlloc::bid encode_c13_lineinfo (
			codeview_subsection_header* subsec,
			void* file_checksums,
			BinAlloc& alloc
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
		
		auto result_offs = alloc.push_default<Lineinfo>();
		Lineinfo result = {};
		result.num_ranges = 0;

		auto* ptr = (char*)subsec;
		ptr += sizeof(codeview_subsection_header);
		auto* end = ptr + subsec->length;
		
		auto* header = (codeview_line_header*)ptr;
		ptr += sizeof(codeview_line_header);
		assert(header->flags == 0); // CV_LINES_HAVE_COLUMNS not implemented
		
		CodeRange prev_range = {};
		bool has_prev_range = false;

		while (ptr < end) {
			auto* line_block = (codeview_line_block_header*)ptr;
			ptr += sizeof(codeview_line_block_header);
			
			auto* cksm = (codeview_file_checksum*)((char*)file_checksums + line_block->offset_in_file_checksums);
			auto sourcefile = cksm->offset_in_string_table;

			auto* lines = (codeview_line*)ptr;
			for (u32 i=0; i<line_block->amount_of_lines; i++) {
				auto& line = lines[i];
				
				if (i > 0) assert(line.offset >= lines[i-1].offset); // verify sorted

				prev_range.end = line.offset;
				CodeRange range{ line.offset, 0, line.start_line_number, sourcefile };

				if (has_prev_range) {
					alloc.push(prev_range);
					result.num_ranges++;
				}

				prev_range = range;
				has_prev_range = true;
			}

			ptr += line_block->amount_of_lines * sizeof(codeview_line);

		}
		assert(ptr == end);
		
		if (has_prev_range) {
			alloc.push(prev_range);
			result.num_ranges++;
		}

		*alloc.get<Lineinfo>(result_offs) = result;
		return result_offs;
	}
	
	static BinAlloc::bid encode_compressed_annotation (
			PCompressedAnnotation annotations, PCompressedAnnotation anno_end,
			u32 initial_fileId, u32 initialSourceLineNum,
			void* file_checksums,
			BinAlloc& alloc
		) {
		auto result_offs = alloc.push_default<Lineinfo>();
		Lineinfo result = {};
		result.num_ranges = 0;

		auto* cur = annotations;
		
		u32 file_id = initial_fileId;
		u32 code_offset_base = 0;
		u32 code_offset = 0;
		u32 code_length = 0; // 0 = null
		u32 lineno = initialSourceLineNum;
		u32 num_lines = 1;
		u32 kind = 1; // 0 == Expression, 1 == Statement
		
		// Optimized away vector entirely

		struct Line {
			u32 code_offset;
			u32 code_length;
			u32 lineno;
			u32 num_lines; // could mean one code range can be associated with a range of line numbers(?)
			u32 file_id;
			u32 kind;
		};
		Line prev_line;
		bool has_prev_line = false;
		

		auto emit_prev_line = [&] () {
			auto* cksm = (codeview_file_checksum*)((char*)file_checksums + prev_line.file_id);
			auto sourcefile = cksm->offset_in_string_table;

			CodeRange range;
			range.start = prev_line.code_offset;
			range.end = range.start + prev_line.code_length;
			range.lineno = prev_line.lineno;
			range.sourcefile = sourcefile;

			alloc.push(&range);
			result.num_ranges++;
		};

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

							emit_prev_line();
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
							
							emit_prev_line();
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
						emit_prev_line();
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
		
		*alloc.get<Lineinfo>(result_offs) = result;
		return result_offs;
	}

	bool find_line_for_addr (uintptr_t rel_addr, const char* pdb_names_table, SourceLoc* out_src_loc) const {
		auto* ranges = get_ranges();

		CodeRange const* found_range = nullptr;

		for (u32 i=0; i<num_ranges; i++) {
			// first line with addr==line is returned
			// if addr in gap between lines, last seen line with addr>line is returned
			if (rel_addr < ranges[i].start) {
				break;
			}

			found_range = &ranges[i];

			if (rel_addr == ranges[i].start) {
				break;
			}
		}

		if (found_range) {
			*out_src_loc = {
				pdb_names_table + found_range->sourcefile,
				found_range->lineno
			};
			return true;
		}
		return false;
	}

	// TODO: due to lineinfo acting weird,
	// for the moment develop a replacement for binary annotations first, then make it work with lineinfo afterwards
	bool find_line_for_addr2 (uintptr_t rel_addr, const char* pdb_names_table, SourceLoc* out_src_loc) const {
		auto* ranges = get_ranges();

		CodeRange const* found_range = nullptr;

		for (u32 i=0; i<num_ranges; i++) {
			// first line with addr==line is returned
			// if addr in gap between lines, last seen line with addr>line is returned
			if (rel_addr < ranges[i].start) {
				break;
			}

			found_range = &ranges[i];

			if (rel_addr == ranges[i].start) {
				break;
			}
		}

		if (found_range) assert(rel_addr >= found_range->start);
		if (found_range && rel_addr < found_range->end) {
			*out_src_loc = {
				pdb_names_table + found_range->sourcefile,
				found_range->lineno
			};
			return true;
		}
		return false;
	}
};
#else
class Lineinfo {
	// TODO: explain encoding

	struct alignas(2) CodeRangeCompact {
		uint8_t is_compact : 1;
		uint8_t length : 6;
		int8_t lineno_delta;

		bool is_gap () const { return lineno_delta == INT8_MIN; }
	};

	struct alignas(4) CodeRangeFull {
		uint8_t is_compact : 1;
		uint32_t length : 30;
		uint32_t lineno;
		uint32_t sourcefile;

		bool is_gap () const { return lineno == UINT32_MAX; }
	};

	struct CodeRangeSource {
		uint32_t offset;
		uint32_t length;
		uint32_t lineno;
		uint32_t sourcefile;

		bool is_gap () const { return lineno == UINT32_MAX; }
	};
	
	struct Encoder {
		Lineinfo& lineinfo;
		CodeRangeSource prev;
		
		inline static int _total_FULL = 0;
		inline static int _total_COMPACT = 0;
		inline static int _total_GAP = 0;
		
		Encoder (Lineinfo& lineinfo): lineinfo{lineinfo} {
			prev.offset = 0;
			prev.length = 0;
			prev.lineno = lineinfo.init_lineno;
			prev.sourcefile = lineinfo.init_sourcefile;
		}
		
		void push (BinAlloc& alloc, CodeRangeSource const& cur) {
			assert(cur.offset >= prev.offset);
			assert(cur.lineno < UINT32_MAX);
			assert(cur.length < 0x3fffffff);

			if (prev.offset + prev.length != cur.offset) {
				uint32_t gap_length = cur.offset - (prev.offset + prev.length);

				CodeRangeCompact compact = {};
				compact.is_compact = true;
				compact.length = (uint8_t)gap_length;
				compact.lineno_delta = INT8_MIN;

				bool is_compact = compact.length == gap_length;
				if (is_compact) {
					alloc.push(&compact);
				}
				else {
					CodeRangeFull range = {};
					compact.is_compact = false;
					range.length = gap_length;
					range.lineno = UINT32_MAX;
					range.sourcefile = 0;
					alloc.push(&range);
				}
				_total_GAP++;
				lineinfo.num_ranges++;
			}

			CodeRangeCompact compact = {};
			compact.is_compact = true;
			compact.length = (uint8_t)cur.length;
			compact.lineno_delta = (int8_t)(cur.lineno - prev.lineno);

			bool is_compact =
				compact.length == cur.length &&
				(prev.lineno + compact.lineno_delta) == cur.lineno &&
				prev.sourcefile == cur.sourcefile;
			if (is_compact) {
				alloc.push(&compact);
				_total_COMPACT++;
			}
			else {
				CodeRangeFull range = {};
				compact.is_compact = false;
				range.length = cur.length;
				range.lineno = cur.lineno;
				range.sourcefile = cur.sourcefile;
				// HACK: push can end up aligning to 4 bytes, so if previous range was a compact one
				// decode will read padding instead of actual type bits, but padding is 0
				// and since FULL type is 0, this works out...
				alloc.push(&range);
				_total_FULL++;
			}
			lineinfo.num_ranges++;

			prev = cur;
		}
	};
	struct Decoder {
		uint8_t const* cur;
		uint32_t offset;
		uint32_t end_offset;
		uint32_t lineno;
		uint32_t sourcefile;

		__forceinline Decoder (Lineinfo const* lineinfo) {
			cur = lineinfo->get_ranges();
			offset = 0;
			end_offset = 0;
			lineno = lineinfo->init_lineno;
			sourcefile = lineinfo->init_sourcefile;
		}

		__forceinline bool decode () {
			offset = end_offset;
			bool is_gap;

			// Reading struct with bitfield has bad codegen, do it manually
			uint16_t compact = *(uint16_t*)cur;
			if (compact & 1) { // is_comapct
				cur += sizeof(CodeRangeCompact);
				
				auto length = ((uint8_t)compact) >> 1;
				end_offset = offset + length;
				int8_t line = (int8_t)(compact >> 8);

				is_gap = line == INT8_MIN; //.is_gap();
				if (!is_gap)
					lineno += line;
				return is_gap;
			}
			else {
				// align up to 4 bytes
				cur = (uint8_t const*)(((uintptr_t)cur + 0b11) & ~0b11);

				auto* range = (CodeRangeFull const*)cur;
				cur += sizeof(CodeRangeFull);
				
				is_gap = range->is_gap();
				auto length = range->length;
				end_offset = offset + length;
				sourcefile = range->sourcefile;
				if (!is_gap)
					lineno = range->lineno;
				return is_gap;
			}
		}
	};

	u32 num_ranges = 0;
	u32 init_lineno;
	u32 init_sourcefile; // offset in pdb /names string table
	uint8_t const* get_ranges () const { return (uint8_t*)(this+1); }

public:
	static BinAlloc::bid encode_c13_lineinfo (
			codeview_subsection_header* subsec,
			void* file_checksums,
			BinAlloc& alloc
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
		
		auto result_offs = alloc.push_default<Lineinfo>();
		Lineinfo result = {};
		result.num_ranges = 0;
		result.init_lineno = 0;
		result.init_sourcefile = 0;
		
		Encoder encoder(result);

		auto* ptr = (char*)subsec;
		ptr += sizeof(codeview_subsection_header);
		auto* end = ptr + subsec->length;
		
		auto* header = (codeview_line_header*)ptr;
		ptr += sizeof(codeview_line_header);
		assert(header->flags == 0); // CV_LINES_HAVE_COLUMNS not implemented
		
		bool has_prev_range = false;
		CodeRangeSource prev_range = {};

		while (ptr < end) {
			auto* line_block = (codeview_line_block_header*)ptr;
			ptr += sizeof(codeview_line_block_header);
			
			auto* cksm = (codeview_file_checksum*)((char*)file_checksums + line_block->offset_in_file_checksums);
			auto sourcefile = cksm->offset_in_string_table;

			auto* lines = (codeview_line*)ptr;
			for (u32 i=0; i<line_block->amount_of_lines; i++) {
				auto& line = lines[i];
				
				if (i > 0) assert(line.offset >= lines[i-1].offset); // verify sorted

				// Since I'm not using the end field at all here, we don't need to overcomplicate via this prev_range thing
				// TODO: see how compressed annotations work and if we can get rid of the end field
				if (has_prev_range) {
					prev_range.length = line.offset - prev_range.offset;

					encoder.push(alloc, prev_range);
				}
				else {
					// HACK: easiest way of getting first line in C13 data, and this avoid always pushing a full range as the first one
					result.init_lineno = line.start_line_number;
					result.init_sourcefile = sourcefile;
					encoder.prev.lineno = line.start_line_number;
					encoder.prev.sourcefile = sourcefile;
				}

				prev_range.offset = line.offset;
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

		*alloc.get<Lineinfo>(result_offs) = result;
		return result_offs;
	}
	
	static BinAlloc::bid encode_compressed_annotation (
			PCompressedAnnotation annotations, PCompressedAnnotation anno_end,
			u32 initial_fileId, u32 initialSourceLineNum,
			void* file_checksums,
			BinAlloc& alloc
		) {
		struct Line {
			u32 code_offset;
			u32 code_length;
			u32 lineno;
			u32 num_lines; // could mean one code range can be associated with a range of line numbers(?)
			u32 file_id;
			u32 kind;
		};

		auto result_offs = alloc.push_default<Lineinfo>();
		Lineinfo result = {};
		result.num_ranges = 0;
		result.init_lineno = initialSourceLineNum;
		result.init_sourcefile = initial_fileId;

		Encoder encoder(result);

		auto emit_range = [&] (Line& line) {
			auto* cksm = (codeview_file_checksum*)((char*)file_checksums + line.file_id);
			auto sourcefile = cksm->offset_in_string_table;

			CodeRangeSource range;
			range.offset = line.code_offset;
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
		
		*alloc.get<Lineinfo>(result_offs) = result;
		return result_offs;
	}

	bool find_line_for_addr (uintptr_t rel_addr, const char* pdb_names_table, SourceLoc* out_src_loc) const {
		Decoder decoder(this);

		uint32_t found_sourcefile = 0;
		uint32_t found_lineno = UINT32_MAX;

		for (u32 i=0; i<num_ranges; i++) {
			bool is_gap = decoder.decode();
			
			// first line with addr==line is returned
			// if addr in gap between lines, last seen line with addr>line is returned
			if (rel_addr < decoder.offset) {
				break;
			}

			found_sourcefile = decoder.sourcefile;
			found_lineno = is_gap ? UINT32_MAX : decoder.lineno;

			if (rel_addr == decoder.offset) {
				break;
			}
		}

		if (found_lineno != UINT32_MAX) {
			*out_src_loc = {
				pdb_names_table + found_sourcefile,
				found_lineno
			};
			return true;
		}
		return false;
	}

	// TODO: due to lineinfo acting weird,
	// for the moment develop a replacement for binary annotations first, then make it work with lineinfo afterwards
	bool find_line_for_addr2 (uintptr_t rel_addr, const char* pdb_names_table, SourceLoc* out_src_loc) const {
		Decoder decoder(this);
		
		for (u32 i=0; i<num_ranges; i++) {
			bool is_gap = decoder.decode();

			// return addr>offset && addr<=offset+length if not gap
			if (rel_addr < decoder.end_offset) {
				if (!is_gap && rel_addr >= decoder.offset) {
					*out_src_loc = {
						pdb_names_table + decoder.sourcefile,
						decoder.lineno
					};
					return true;
				}
				// ranges are sorted, so any later ranges cannot match
				return false;
			}
		}
		return false;
	}
};
#endif
