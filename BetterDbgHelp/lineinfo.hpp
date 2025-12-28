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

class Lineinfo {
	BinAlloc::bid data = -1;
	u32 num_ranges = 0;
	
	// TODO: in normal c13 lineinfo there is no end, but inline opcodes do encode code size
	// but maybe dbghelp does not use it? check if opcode scheme is always sorted and if it is,
	// check if simply returning the first where offset==addr or next offset > addr gives correct results
	struct CodeRange {
		uint32_t start;
		uint32_t end;
		uint32_t lineno;
		uint32_t sourcefile; // offset in pdb /names string table
	};

public:
	static Lineinfo encode_c13_lineinfo (
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
		
		auto* ptr = (char*)subsec;
		ptr += sizeof(codeview_subsection_header);
		auto* end = ptr + subsec->length;
		
		auto* header = (codeview_line_header*)ptr;
		ptr += sizeof(codeview_line_header);
		assert(header->flags == 0); // CV_LINES_HAVE_COLUMNS not implemented
		
		CodeRange prev_range = {};
		bool has_prev_range = false;

		Lineinfo result = {};
		result.data = alloc.prepare_push<CodeRange>();
		result.num_ranges = 0;

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
					alloc.push(&prev_range);
					result.num_ranges++;
				}

				prev_range = range;
				has_prev_range = true;
			}

			ptr += line_block->amount_of_lines * sizeof(codeview_line);

		}
		assert(ptr == end);
		
		if (has_prev_range) {
			alloc.push(&prev_range);
			result.num_ranges++;
		}
		return result;
	}

	bool find_source_loc_for_addr (uintptr_t sym_addr, uint32_t sym_size,
			uintptr_t addr, int32_t src_offset,
			const char* pdb_names_table, SourceLoc* out_src_loc,
			BinAlloc const& alloc) const {
		ZoneScoped;

		auto* ranges = alloc.get<CodeRange>(data);
		if (!ranges) {
			return false;
		}
		
		intptr_t proc_raddr = (intptr_t)addr - (intptr_t)sym_addr;
		if (proc_raddr >= (intptr_t)sym_size) {
			// past symbol address range, no valid line number
			return false;
		}

		// handle case where line info is before symbol range but still overlaps
		proc_raddr += src_offset;

		CodeRange* found_range = nullptr;

		for (u32 i=0; i<num_ranges; i++) {
			// first line with addr==line is returned
			// if addr in gap between lines, last seen line with addr>line is returned
			if (proc_raddr < (intptr_t)ranges[i].start) {
				break;
			}

			found_range = &ranges[i];

			if (proc_raddr == (intptr_t)ranges[i].start) {
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
};
