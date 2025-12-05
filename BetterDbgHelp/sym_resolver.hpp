#pragma once
#include "util.hpp"

#include <psapi.h>
#pragma comment(lib, "Kernel32.lib")

#include <unordered_map>

typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;

typedef uint32_t CV_typ_t; // ?? Couldn't find real code or doc for this
typedef uint32_t CV_uoff32_t; // assumed based on name


typedef enum SYM_ENUM_e : u16 {
    S_COMPILE       =  0x0001,  // Compile flags symbol
    S_REGISTER_16t  =  0x0002,  // Register variable
    S_CONSTANT_16t  =  0x0003,  // constant symbol
    S_UDT_16t       =  0x0004,  // User defined type
    S_SSEARCH       =  0x0005,  // Start Search
    S_END           =  0x0006,  // Block, procedure, "with" or thunk end
    S_SKIP          =  0x0007,  // Reserve symbol space in $$Symbols table
    S_CVRESERVE     =  0x0008,  // Reserved symbol for CV internal use
    S_OBJNAME_ST    =  0x0009,  // path to object file name
    S_ENDARG        =  0x000a,  // end of argument/return list
    S_COBOLUDT_16t  =  0x000b,  // special UDT for cobol that does not symbol pack
    S_MANYREG_16t   =  0x000c,  // multiple register variable
    S_RETURN        =  0x000d,  // return description symbol
    S_ENTRYTHIS     =  0x000e,  // description of this pointer on entry

    S_BPREL16       =  0x0100,  // BP-relative
    S_LDATA16       =  0x0101,  // Module-local symbol
    S_GDATA16       =  0x0102,  // Global data symbol
    S_PUB16         =  0x0103,  // a public symbol
    S_LPROC16       =  0x0104,  // Local procedure start
    S_GPROC16       =  0x0105,  // Global procedure start
    S_THUNK16       =  0x0106,  // Thunk Start
    S_BLOCK16       =  0x0107,  // block start
    S_WITH16        =  0x0108,  // with start
    S_LABEL16       =  0x0109,  // code label
    S_CEXMODEL16    =  0x010a,  // change execution model
    S_VFTABLE16     =  0x010b,  // address of virtual function table
    S_REGREL16      =  0x010c,  // register relative address

    S_BPREL32_16t   =  0x0200,  // BP-relative
    S_LDATA32_16t   =  0x0201,  // Module-local symbol
    S_GDATA32_16t   =  0x0202,  // Global data symbol
    S_PUB32_16t     =  0x0203,  // a public symbol (CV internal reserved)
    S_LPROC32_16t   =  0x0204,  // Local procedure start
    S_GPROC32_16t   =  0x0205,  // Global procedure start
    S_THUNK32_ST    =  0x0206,  // Thunk Start
    S_BLOCK32_ST    =  0x0207,  // block start
    S_WITH32_ST     =  0x0208,  // with start
    S_LABEL32_ST    =  0x0209,  // code label
    S_CEXMODEL32    =  0x020a,  // change execution model
    S_VFTABLE32_16t =  0x020b,  // address of virtual function table
    S_REGREL32_16t  =  0x020c,  // register relative address
    S_LTHREAD32_16t =  0x020d,  // local thread storage
    S_GTHREAD32_16t =  0x020e,  // global thread storage
    S_SLINK32       =  0x020f,  // static link for MIPS EH implementation

    S_LPROCMIPS_16t =  0x0300,  // Local procedure start
    S_GPROCMIPS_16t =  0x0301,  // Global procedure start

    // if these ref symbols have names following then the names are in ST format
    S_PROCREF_ST    =  0x0400,  // Reference to a procedure
    S_DATAREF_ST    =  0x0401,  // Reference to data
    S_ALIGN         =  0x0402,  // Used for page alignment of symbols

    S_LPROCREF_ST   =  0x0403,  // Local Reference to a procedure
    S_OEM           =  0x0404,  // OEM defined symbol

    // sym records with 32-bit types embedded instead of 16-bit
    // all have 0x1000 bit set for easy identification
    // only do the 32-bit target versions since we don't really
    // care about 16-bit ones anymore.
    S_TI16_MAX          =  0x1000,

    S_REGISTER_ST   =  0x1001,  // Register variable
    S_CONSTANT_ST   =  0x1002,  // constant symbol
    S_UDT_ST        =  0x1003,  // User defined type
    S_COBOLUDT_ST   =  0x1004,  // special UDT for cobol that does not symbol pack
    S_MANYREG_ST    =  0x1005,  // multiple register variable
    S_BPREL32_ST    =  0x1006,  // BP-relative
    S_LDATA32_ST    =  0x1007,  // Module-local symbol
    S_GDATA32_ST    =  0x1008,  // Global data symbol
    S_PUB32_ST      =  0x1009,  // a public symbol (CV internal reserved)
    S_LPROC32_ST    =  0x100a,  // Local procedure start
    S_GPROC32_ST    =  0x100b,  // Global procedure start
    S_VFTABLE32     =  0x100c,  // address of virtual function table
    S_REGREL32_ST   =  0x100d,  // register relative address
    S_LTHREAD32_ST  =  0x100e,  // local thread storage
    S_GTHREAD32_ST  =  0x100f,  // global thread storage

    S_LPROCMIPS_ST  =  0x1010,  // Local procedure start
    S_GPROCMIPS_ST  =  0x1011,  // Global procedure start

    S_FRAMEPROC     =  0x1012,  // extra frame and proc information
    S_COMPILE2_ST   =  0x1013,  // extended compile flags and info

    // new symbols necessary for 16-bit enumerates of IA64 registers
    // and IA64 specific symbols

    S_MANYREG2_ST   =  0x1014,  // multiple register variable
    S_LPROCIA64_ST  =  0x1015,  // Local procedure start (IA64)
    S_GPROCIA64_ST  =  0x1016,  // Global procedure start (IA64)

    // Local symbols for IL
    S_LOCALSLOT_ST  =  0x1017,  // local IL sym with field for local slot index
    S_PARAMSLOT_ST  =  0x1018,  // local IL sym with field for parameter slot index

    S_ANNOTATION    =  0x1019,  // Annotation string literals

    // symbols to support managed code debugging
    S_GMANPROC_ST   =  0x101a,  // Global proc
    S_LMANPROC_ST   =  0x101b,  // Local proc
    S_RESERVED1     =  0x101c,  // reserved
    S_RESERVED2     =  0x101d,  // reserved
    S_RESERVED3     =  0x101e,  // reserved
    S_RESERVED4     =  0x101f,  // reserved
    S_LMANDATA_ST   =  0x1020,
    S_GMANDATA_ST   =  0x1021,
    S_MANFRAMEREL_ST=  0x1022,
    S_MANREGISTER_ST=  0x1023,
    S_MANSLOT_ST    =  0x1024,
    S_MANMANYREG_ST =  0x1025,
    S_MANREGREL_ST  =  0x1026,
    S_MANMANYREG2_ST=  0x1027,
    S_MANTYPREF     =  0x1028,  // Index for type referenced by name from metadata
    S_UNAMESPACE_ST =  0x1029,  // Using namespace

    // Symbols w/ SZ name fields. All name fields contain utf8 encoded strings.
    S_ST_MAX        =  0x1100,  // starting point for SZ name symbols

    S_OBJNAME       =  0x1101,  // path to object file name
    S_THUNK32       =  0x1102,  // Thunk Start
    S_BLOCK32       =  0x1103,  // block start
    S_WITH32        =  0x1104,  // with start
    S_LABEL32       =  0x1105,  // code label
    S_REGISTER      =  0x1106,  // Register variable
    S_CONSTANT      =  0x1107,  // constant symbol
    S_UDT           =  0x1108,  // User defined type
    S_COBOLUDT      =  0x1109,  // special UDT for cobol that does not symbol pack
    S_MANYREG       =  0x110a,  // multiple register variable
    S_BPREL32       =  0x110b,  // BP-relative
    S_LDATA32       =  0x110c,  // Module-local symbol
    S_GDATA32       =  0x110d,  // Global data symbol
    S_PUB32         =  0x110e,  // a public symbol (CV internal reserved)
    S_LPROC32       =  0x110f,  // Local procedure start
    S_GPROC32       =  0x1110,  // Global procedure start
    S_REGREL32      =  0x1111,  // register relative address
    S_LTHREAD32     =  0x1112,  // local thread storage
    S_GTHREAD32     =  0x1113,  // global thread storage

    S_LPROCMIPS     =  0x1114,  // Local procedure start
    S_GPROCMIPS     =  0x1115,  // Global procedure start
    S_COMPILE2      =  0x1116,  // extended compile flags and info
    S_MANYREG2      =  0x1117,  // multiple register variable
    S_LPROCIA64     =  0x1118,  // Local procedure start (IA64)
    S_GPROCIA64     =  0x1119,  // Global procedure start (IA64)
    S_LOCALSLOT     =  0x111a,  // local IL sym with field for local slot index
    S_SLOT          = S_LOCALSLOT,  // alias for LOCALSLOT
    S_PARAMSLOT     =  0x111b,  // local IL sym with field for parameter slot index

    // symbols to support managed code debugging
    S_LMANDATA      =  0x111c,
    S_GMANDATA      =  0x111d,
    S_MANFRAMEREL   =  0x111e,
    S_MANREGISTER   =  0x111f,
    S_MANSLOT       =  0x1120,
    S_MANMANYREG    =  0x1121,
    S_MANREGREL     =  0x1122,
    S_MANMANYREG2   =  0x1123,
    S_UNAMESPACE    =  0x1124,  // Using namespace

    // ref symbols with name fields
    S_PROCREF       =  0x1125,  // Reference to a procedure
    S_DATAREF       =  0x1126,  // Reference to data
    S_LPROCREF      =  0x1127,  // Local Reference to a procedure
    S_ANNOTATIONREF =  0x1128,  // Reference to an S_ANNOTATION symbol
    S_TOKENREF      =  0x1129,  // Reference to one of the many MANPROCSYM's

    // continuation of managed symbols
    S_GMANPROC      =  0x112a,  // Global proc
    S_LMANPROC      =  0x112b,  // Local proc

    // short, light-weight thunks
    S_TRAMPOLINE    =  0x112c,  // trampoline thunks
    S_MANCONSTANT   =  0x112d,  // constants with metadata type info

    // native attributed local/parms
    S_ATTR_FRAMEREL =  0x112e,  // relative to virtual frame ptr
    S_ATTR_REGISTER =  0x112f,  // stored in a register
    S_ATTR_REGREL   =  0x1130,  // relative to register (alternate frame ptr)
    S_ATTR_MANYREG  =  0x1131,  // stored in >1 register

    // Separated code (from the compiler) support
    S_SEPCODE       =  0x1132,

    S_LOCAL_2005    =  0x1133,  // defines a local symbol in optimized code
    S_DEFRANGE_2005 =  0x1134,  // defines a single range of addresses in which symbol can be evaluated
    S_DEFRANGE2_2005 =  0x1135,  // defines ranges of addresses in which symbol can be evaluated

    S_SECTION       =  0x1136,  // A COFF section in a PE executable
    S_COFFGROUP     =  0x1137,  // A COFF group
    S_EXPORT        =  0x1138,  // A export

    S_CALLSITEINFO  =  0x1139,  // Indirect call site information
    S_FRAMECOOKIE   =  0x113a,  // Security cookie information

    S_DISCARDED     =  0x113b,  // Discarded by LINK /OPT:REF (experimental, see richards)

    S_COMPILE3      =  0x113c,  // Replacement for S_COMPILE2
    S_ENVBLOCK      =  0x113d,  // Environment block split off from S_COMPILE2

    S_LOCAL         =  0x113e,  // defines a local symbol in optimized code
    S_DEFRANGE      =  0x113f,  // defines a single range of addresses in which symbol can be evaluated
    S_DEFRANGE_SUBFIELD =  0x1140,           // ranges for a subfield

    S_DEFRANGE_REGISTER =  0x1141,           // ranges for en-registered symbol
    S_DEFRANGE_FRAMEPOINTER_REL =  0x1142,   // range for stack symbol.
    S_DEFRANGE_SUBFIELD_REGISTER =  0x1143,  // ranges for en-registered field of symbol
    S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE =  0x1144, // range for stack symbol span valid full scope of function body, gap might apply.
    S_DEFRANGE_REGISTER_REL =  0x1145, // range for symbol address as register + offset.

    // S_PROC symbols that reference ID instead of type
    S_LPROC32_ID     =  0x1146,
    S_GPROC32_ID     =  0x1147,
    S_LPROCMIPS_ID   =  0x1148,
    S_GPROCMIPS_ID   =  0x1149,
    S_LPROCIA64_ID   =  0x114a,
    S_GPROCIA64_ID   =  0x114b,

    S_BUILDINFO      = 0x114c, // build information.
    S_INLINESITE     = 0x114d, // inlined function callsite.
    S_INLINESITE_END = 0x114e,
    S_PROC_ID_END    = 0x114f,

    S_DEFRANGE_HLSL  = 0x1150,
    S_GDATA_HLSL     = 0x1151,
    S_LDATA_HLSL     = 0x1152,

    S_FILESTATIC     = 0x1153,

#if defined(CC_DP_CXX) && CC_DP_CXX

    S_LOCAL_DPC_GROUPSHARED = 0x1154, // DPC groupshared variable
    S_LPROC32_DPC = 0x1155, // DPC local procedure start
    S_LPROC32_DPC_ID =  0x1156,
    S_DEFRANGE_DPC_PTR_TAG =  0x1157, // DPC pointer tag definition range
    S_DPC_SYM_TAG_MAP = 0x1158, // DPC pointer tag value to symbol record map

#endif // CC_DP_CXX
    
    S_ARMSWITCHTABLE  = 0x1159,
    S_CALLEES = 0x115a,
    S_CALLERS = 0x115b,
    S_POGODATA = 0x115c,
    S_INLINESITE2 = 0x115d,      // extended inline site information

    S_HEAPALLOCSITE = 0x115e,    // heap allocation site

    S_MOD_TYPEREF = 0x115f,      // only generated at link time

    S_REF_MINIPDB = 0x1160,      // only generated at link time for mini PDB
    S_PDBMAP      = 0x1161,      // only generated at link time for mini PDB

    S_GDATA_HLSL32 = 0x1162,
    S_LDATA_HLSL32 = 0x1163,

    S_GDATA_HLSL32_EX = 0x1164,
    S_LDATA_HLSL32_EX = 0x1165,

    S_RECTYPE_MAX,               // one greater than last
    S_RECTYPE_LAST  = S_RECTYPE_MAX - 1,
    S_RECTYPE_PAD   = S_RECTYPE_MAX + 0x100 // Used *only* to verify symbol record types so that current PDB code can potentially read
                                // future PDBs (assuming no format change, etc).

} SYM_ENUM_e;


typedef struct CV_PROCFLAGS {
    union {
        unsigned char   bAll;
        unsigned char   grfAll;
        struct {
            unsigned char CV_PFLAG_NOFPO     :1; // frame pointer present
            unsigned char CV_PFLAG_INT       :1; // interrupt return
            unsigned char CV_PFLAG_FAR       :1; // far return
            unsigned char CV_PFLAG_NEVER     :1; // function does not return
            unsigned char CV_PFLAG_NOTREACHED:1; // label isn't fallen into
            unsigned char CV_PFLAG_CUST_CALL :1; // custom calling convention
            unsigned char CV_PFLAG_NOINLINE  :1; // function marked as noinline
            unsigned char CV_PFLAG_OPTDBGINFO:1; // function has debug information for optimized code
        };
    };
} CV_PROCFLAGS;
typedef struct PROCSYM32 {
    unsigned short  reclen;     // Record length
    unsigned short  rectyp;     // S_GPROC32, S_LPROC32, S_GPROC32_ID, S_LPROC32_ID, S_LPROC32_DPC or S_LPROC32_DPC_ID
    unsigned long   pParent;    // pointer to the parent
    unsigned long   pEnd;       // pointer to this blocks end
    unsigned long   pNext;      // pointer to next symbol
    unsigned long   len;        // Proc length
    unsigned long   DbgStart;   // Debug start offset
    unsigned long   DbgEnd;     // Debug end offset
    CV_typ_t        typind;     // Type index or ID
    CV_uoff32_t     off;
    unsigned short  seg;
    CV_PROCFLAGS    flags;      // Proc flags
    unsigned char   name[1];    // Length-prefixed name
} PROCSYM32;

// https://github.com/PascalBeyer/PDB-Documentation/?tab=readme-ov-file
struct msf_header{
	u8  signature[32];
	u32 page_size;
	u32 active_free_page_map;
	u32 amount_of_pages;
	u32 stream_table_stream_size;
	u32 unused;
	u32 page_list_of_stream_table_stream_page_list[1];
};
	
struct pdb_information_stream_header{
	u32 version;
	u32 timestamp;
	u32 age;
	GUID guid;
};

struct dbi_stream_header{
	u32 version_signature;
	u32 version;
	u32 age;
	u16 stream_index_of_the_global_symbol_index_stream;
	struct{
		u16 minor_version : 8;
		u16 major_version : 7;
		u16 is_new_version_format : 1;
	} toolchain_version;
	u16 stream_index_of_the_public_symbol_index_stream;
	u16 version_number_of_mspdb_dll_which_build_the_pdb;
	u16 stream_index_of_the_symbol_record_stream;
	u16 build_number_of_mspdb_dll_which_build_the_pdb;
		
	u32 byte_size_of_the_module_information_substream;   // substream 0
	u32 byte_size_of_the_section_contribution_substream; // substream 1
	u32 byte_size_of_the_section_map_substream;          // substream 2
	u32 byte_size_of_the_source_information_substream;   // substream 3
	u32 byte_size_of_the_type_server_map_substream;      // substream 4
		
	u32 index_of_the_MFC_type_server_in_type_server_map_substream;
		
	u32 byte_size_of_the_optional_debug_header_substream; // substream 6
	u32 byte_size_of_the_edit_and_continue_substream;     // substream 5
		
	struct{
		u16 was_linked_incrementally         : 1;
		u16 private_symbols_were_stripped    : 1;
		u16 the_pdb_allows_conflicting_types : 1; // undocumented /DEBUG:CTYPES flag.
	} flags;
		
	u16 machine_type;
	u32 reserved_padding;
};

struct pdb_section_contribution{
	s16 section_id;
	u16 padding1;
	s32 offset;
	s32 size;
	u32 characteristics;
	s16 module_index;
	u16 padding2;
	u32 data_crc;
	u32 reloc_crc;
};
struct pdb_module_information{
	u32 unused;
	struct pdb_section_contribution first_code_contribution;
		
	struct{
		u16 was_written : 1;
		u16 edit_and_continue_enabled : 1;
		u16 unused : 6;
		u16 TSM_index : 8;
	} flags;
		
	u16 stream_index_of_module_symbol_stream;
		
	u32 byte_size_of_symbol_information;
	u32 byte_size_of_c11_line_information;
	u32 byte_size_of_c13_line_information;
		
	u16 amount_of_source_files;
	u16 padding;
	u32 unused2;
		
	u32 edit_and_continue_source_file_string_index;
	u32 edit_and_continue_pdb_file_string_index;
		
	//char module_name_and_file_name_and_padding[1];
};

struct pdb_section_map_stream_header{
	u16 number_of_section_descriptors;
	u16 number_of_logical_section_descriptors;
};
struct pdb_section_map_entry{
	u16 flags;
	u16 logical_overlay_number;
	u16 group;
	u16 frame;
	u16 section_name;
	u16 class_name;
	u32 offset;
	u32 section_size;
};

struct optional_debug_header_substream{
	u16 stream_index_of_fpo_data;
	u16 stream_index_of_exception_data;
	u16 stream_index_of_fixup_data;
	u16 stream_index_of_omap_to_src_data;
	u16 stream_index_of_omap_from_src_data;
	u16 stream_index_of_section_header_dump;
	u16 stream_index_of_clr_token_to_clr_record_id;
	u16 stream_index_of_xdata;
	u16 stream_index_of_pdata;
	u16 stream_index_of_new_fpo_data;
	u16 stream_index_of_original_section_header_dump;
};

struct codeview_symbol_header{ // Also see SYMTYPE in cvinfo.h
	u16 length;
	//u16 kind;
	SYM_ENUM_e kind;
};

enum DEBUG_S_SUBSECTION_TYPE : u32 {
	DEBUG_S_IGNORE = 0x80000000,    // if this bit is set in a subsection type then ignore the subsection contents

	DEBUG_S_SYMBOLS = 0xf1,
	DEBUG_S_LINES,
	DEBUG_S_STRINGTABLE,
	DEBUG_S_FILECHKSMS,
	DEBUG_S_FRAMEDATA,
	DEBUG_S_INLINEELINES,
	DEBUG_S_CROSSSCOPEIMPORTS,
	DEBUG_S_CROSSSCOPEEXPORTS,

	DEBUG_S_IL_LINES,
	DEBUG_S_FUNC_MDTOKEN_MAP,
	DEBUG_S_TYPE_MDTOKEN_MAP,
	DEBUG_S_MERGED_ASSEMBLYINPUT,

	DEBUG_S_COFF_SYMBOL_RVA,
};
struct codeview_subsection_header{
	DEBUG_S_SUBSECTION_TYPE type;
	u32 length;
};
struct codeview_file_checksum{
	u32 offset_in_string_table;
	u8  checksum_size;
	u8  checksum_kind;
	//u8  checksum[];
};
struct codeview_line_header{
	u32 contribution_offset;
	u16 contribution_section_id;
	u16 flags;
	u32 contribution_size;
};
struct codeview_line_block_header{
	u32 offset_in_file_checksums;
	u32 amount_of_lines; // codeview_line_block_header followed by codeview_line[amount_of_lines]
	u32 block_size; // unsure what is is for, could be sizeof(codeview_line_block_header + codeview_line[])
};
struct codeview_line{
	u32 offset;
	u32 start_line_number     : 24;
	u32 optional_delta_to_end : 7;
	u32 is_a_statement        : 1;
};

struct SourceLoc {
	const char* filename;
	u32         lineno;
};

class PDB_File {
	std::vector<char> data;
	
	void* get_page (u32 idx) {
		return (char*)data.data() + idx * header->page_size;
	}
	u32 ceil_div (u32 a, u32 b) {
		return (a + (b-1)) / b;
	}
	char* align_up (char* ptr, u32 align) {
		uintptr_t x = (uintptr_t)ptr;
		return (char*)((x + align-1) / align * align);
	}

	void* read_sts (u32 ptr) {
		u32 page_idx    = ptr / header->page_size;
		u32 ptr_in_page = ptr % header->page_size;
		
		//u32 sts_num_pages = ceil_div(header->stream_table_stream_size, header->page_size);

		u32 u32_per_page = header->page_size / sizeof(u32);
		u32 page_idx_page     = page_idx / u32_per_page;
		u32 page_idx_page_idx = page_idx % u32_per_page;

		assert(page_idx_page == 0);
		u32* sts_pages = (u32*)get_page(header->page_list_of_stream_table_stream_page_list[page_idx_page]);

		return (char*)get_page(sts_pages[page_idx_page_idx]) + ptr_in_page;
	}
	
	
	msf_header* header;

	struct Stream {
		u32 size;
		std::vector<u32> pages;
	};
	std::vector<Stream> streams;
	
	std::vector<char> pdb_info_data;
	std::vector<char> names_data;
	std::vector<char> DBI_data;
	std::vector<char> section_header_dump_data;

	pdb_information_stream_header* info;

	std::unordered_map<std::string_view, u32> named_streams;

	const char* names;

	optional_debug_header_substream* opt_streams;

	//// Final needed data
	struct Section {
		std::string name;

		uintptr_t base_addr;
		size_t size;
	};
	std::vector<Section> sections_sorted;

	u32 num_section_contributions;
	pdb_section_contribution* section_contributions;
	
	void* read_stream (u32 stream, u32 ptr) {
		u32 page_idx    = ptr / header->page_size;
		u32 ptr_in_page = ptr % header->page_size;

		assert(stream < streams.size() && ptr < streams[stream].size);
		return (char*)get_page(streams[stream].pages[page_idx]) + ptr_in_page;
	}
	std::vector<char> copy_into_consecutive (u32 streami) {
		std::vector<char> data;

		auto& stream = streams[streami];
		data.resize(stream.size);

		char* cur = data.data();
		size_t remain = stream.size;
		for (u32 pg : stream.pages) {
			memcpy(cur, get_page(pg), (u32)std::min((size_t)header->page_size, remain));
			remain -= header->page_size;
			cur += header->page_size;
		}

		return data;
	}
	
	void read_header () {
		header = (msf_header*)data.data();
		assert(strncmp((const char*)header->signature, "Microsoft C/C++ MSF 7.00\r\n\032DS\0\0\0", 32) == 0);
	}
	void read_stream_table () {

		u32* _sts_ppages = (u32*)get_page(header->page_list_of_stream_table_stream_page_list[0]);
		u32* _sts_pages = (u32*)get_page(_sts_ppages[0]);
		char* _sts_start = (char*)read_sts(0);

		u32 cur = 0;
		u32 amount_of_streams = *(u32*)read_sts(cur);
		cur += sizeof(u32);
		
		while (streams.size() < amount_of_streams) {
			u32 stream_size = *(u32*)read_sts(cur);
			cur += sizeof(u32);

			// The assumtion that deleted streams don't count seems to be wrong due to crash and seems to be verified by looking at data
			//if (stream_size == 0xffffffff) {
			//	// I think, this deleted stream does not count for amount_of_streams, but the link above is not clear on this
			//	continue;
			//}
			if (stream_size == 0xffffffff) {
				stream_size = 0;
			}
		
			Stream s;
			s.size = stream_size;
			streams.push_back(s);
		}
		
		for (u32 si=0; si<streams.size(); si++) {
			auto& stream = streams[si];
			printf("Stream %3d: { ", si);
		
			u32 num_pages = ceil_div(stream.size, header->page_size);
			for (u32 i=0; i<num_pages; i++) {
				u32 page_idx = *(u32*)read_sts(cur);
				cur += sizeof(u32);
		
				stream.pages.push_back(page_idx);
		
				printf("%d, ", page_idx);
			}
		
			printf("}\n");
		}
	}

	void read_pdb_info () {
		
		pdb_info_data = copy_into_consecutive(1);
		char* ptr = pdb_info_data.data();

		info = (pdb_information_stream_header*)ptr;
		ptr += sizeof(pdb_information_stream_header);

		// read named stream hashmap
		u32 string_buffer_size = *(u32*)ptr;
		ptr += sizeof(u32);

		char* string_buffer = ptr;
		ptr += string_buffer_size;

		u32 amount_of_entries = *(u32*)ptr;
		ptr += sizeof(u32);
		u32 capacity = *(u32*)ptr;
		ptr += sizeof(u32);
		
		// bit_array present_bits
		u32 present_word_count = *(u32*)ptr;
		ptr += sizeof(u32);
		u32* present_bits = (u32*)ptr;
		ptr += present_word_count * sizeof(u32);

		// bit_array deleted_bits
		u32 deleted_word_count = *(u32*)ptr;
		ptr += sizeof(u32);
		u32* deleted_bits = (u32*)ptr;
		ptr += deleted_word_count * sizeof(u32);

		struct KeyValue {
			u32 key;
			u32 value;
		};
		KeyValue* entries = (KeyValue*)ptr;
		ptr += amount_of_entries * sizeof(KeyValue);

		// unused
		ptr += sizeof(u32);
		
		printf("Named Streams:\n");
		for(u32 index = 0, entry_index = 0; index < capacity && entry_index < amount_of_entries; index++){
			u32 word_index = index / (sizeof(u32) * 8);
			u32 bit_index  = index % (sizeof(u32) * 8);
			
			if(word_index < present_word_count && (present_bits[word_index] & (1u << bit_index))){
				auto& kv = entries[entry_index++];

				//std::string key = std::string(&string_buffer[kv.key]);
				//printf("> %s: %d\n", key.c_str(), kv.value);
				//named_streams[std::move(key)] = kv.value;
				std::string key = std::string(&string_buffer[kv.key]);
				printf("> %s: %d\n", &string_buffer[kv.key], kv.value);
				named_streams[std::string_view(&string_buffer[kv.key])] = kv.value;
				continue;
			}
		}
	}

	void read_names () {
		names_data = copy_into_consecutive(named_streams["/names"]);
		char* ptr = names_data.data();
		
		u32 signature = *(u32*)ptr;
		ptr += sizeof(u32);
		assert(signature == 0xEFFEEFFE);
		
		u32 hash_version = *(u32*)ptr;
		ptr += sizeof(u32);
		
		u32 string_buffer_size = *(u32*)ptr;
		ptr += sizeof(u32);
		
		names = ptr;
		ptr += string_buffer_size;
		
		u32 bucket_count = *(u32*)ptr;
		ptr += sizeof(u32);

		u32* buckets = (u32*)ptr;
		ptr += bucket_count * sizeof(u32);

		u32* amount_of_strings = (u32*)ptr;
		ptr += sizeof(u32);
	}
	
	void read_DBI () {
		DBI_data = copy_into_consecutive(3);
		char* ptr = DBI_data.data();

		auto* header = (dbi_stream_header*)ptr;
		ptr += sizeof(dbi_stream_header);
		
		//// module_information_substream
		auto* ptr2 = ptr;

		modules.reserve(64);
		while (ptr < ptr2+header->byte_size_of_the_module_information_substream) {
			auto* mi = (pdb_module_information*)ptr;
			ptr += sizeof(pdb_module_information);

			const char* mod_name = ptr;
			size_t mod_name_len = strlen(mod_name);
			ptr += mod_name_len+1;

			const char* file_name = ptr;
			size_t file_name_len = strlen(file_name);
			ptr += file_name_len+1;

			ptr = align_up(ptr, 4);

			printf("> %d %-50s %-50s\n", mi->stream_index_of_module_symbol_stream, mod_name, file_name);

			Module m;
			m.mi = mi;
			m.name = std::string_view(mod_name, mod_name_len);
			m.file_name = std::string_view(file_name, file_name_len);

			auto module_index = modules.size();
			modules.push_back(m);

			assert(module_index < 0xffff);
			read_module_symbol_stream((s16)module_index);
		}
		assert((ptr - ptr2) == header->byte_size_of_the_module_information_substream);
		ptr = ptr2 + header->byte_size_of_the_module_information_substream;
		
		//// section_contribution_substream
		ptr2 = ptr;

		u32 DBISCImpv = *(u32*)ptr;
		ptr += sizeof(u32);

		assert(DBISCImpv == (0xeffe0000 + 19970605));

		num_section_contributions = header->byte_size_of_the_section_contribution_substream / sizeof(pdb_section_contribution);
		section_contributions = (pdb_section_contribution*)ptr;

		//while (ptr < ptr2+header->byte_size_of_the_section_contribution_substream) {
			//auto* sc = (pdb_section_contribution*)ptr;
			//ptr += sizeof(pdb_section_contribution);
		for (u32 i=0; i<num_section_contributions; i++) {
			auto* sc = &section_contributions[i];
			printf("> %d %8x %8x %d\n", sc->section_id, sc->offset, sc->size, sc->module_index);
		}
		ptr += sizeof(pdb_section_contribution) * num_section_contributions;
		assert((ptr - ptr2) == header->byte_size_of_the_section_contribution_substream); // Why is this not correct?
		
		//// section_map_substream
		//ptr2 = ptr;
		//
		//auto* sec_header = (pdb_section_map_stream_header*)ptr;
		//ptr += sizeof(pdb_section_map_stream_header);
		//assert(sec_header->number_of_section_descriptors == sec_header->number_of_logical_section_descriptors);
		//
		////while (ptr < ptr2+header->byte_size_of_the_section_map_substream) {
		//for (u32 i=0; i<sec_header->number_of_section_descriptors; i++) {
		//	auto* sm = (pdb_section_map_entry*)ptr;
		//	ptr += sizeof(pdb_section_map_entry);
		//}
		//
		//assert((ptr - ptr2) == header->byte_size_of_the_section_map_substream);
		ptr += header->byte_size_of_the_section_map_substream;

		//// source_information_substream
		//ptr2 = ptr;
		//
		//u16 amount_of_modules = *(u16*)ptr;
		//ptr += sizeof(u16);
		//u16 truncated_amount_of_source_files = *(u16*)ptr;
		//ptr += sizeof(u16);
		//
		//assert(amount_of_modules == modules.size());
		//
		//u16* source_file_base_index_per_module = (u16*)ptr;
		//ptr += amount_of_modules * sizeof(u16);
		//u16* amount_of_source_files_per_module = (u16*)ptr;
		//ptr += amount_of_modules * sizeof(u16);
		//
		//u32* source_file_name_offset_in_string_buffer = (u32*)ptr;
		////ptr += amount_of_source_files * sizeof(u32);
		//
		//u32 byte_size_of_the_source_information_substream;   // substream 3
		//u32 byte_size_of_the_type_server_map_substream;      // substream 4
		//
		//u32 index_of_the_MFC_type_server_in_type_server_map_substream;
		//
		//u32 byte_size_of_the_optional_debug_header_substream; // substream 6
		//u32 byte_size_of_the_edit_and_continue_substream;     // substream 5
		
		ptr += header->byte_size_of_the_source_information_substream;

		ptr += header->byte_size_of_the_type_server_map_substream;

		ptr += header->byte_size_of_the_edit_and_continue_substream;

		//// optional_debug_header_substream
		opt_streams = (optional_debug_header_substream*)ptr;
		
		//byte_size_of_the_optional_debug_header_substream
	}

	void read_section_header_dump () {
		section_header_dump_data = copy_into_consecutive(opt_streams->stream_index_of_section_header_dump);
		assert(section_header_dump_data.size() == streams[opt_streams->stream_index_of_section_header_dump].size);

		char* ptr = section_header_dump_data.data();
		char* ptr2 = ptr;

		while (ptr < ptr2 + section_header_dump_data.size()) {
			auto* sh = (IMAGE_SECTION_HEADER*)ptr;
			ptr += sizeof(IMAGE_SECTION_HEADER);

			sections_sorted.push_back({ std::string((const char*)sh->Name, strnlen_s((const char*)sh->Name, 8)), sh->VirtualAddress, sh->Misc.VirtualSize });
			
			char name[9] = {};
			strncpy_s(name, (const char*)sh->Name, 8); // properly null-terminate
			printf("> %7s %8x %8x\n", name, sh->VirtualAddress, sh->Misc.VirtualSize);
		}

		_assert_sections_sorted();
	}
	
	void read_module_symbol_stream (s16 module_index) {
		auto& mod = modules[module_index];
		auto* mi = mod.mi;

		mod.symbol_stream_data = copy_into_consecutive(mi->stream_index_of_module_symbol_stream);
		char* ptr = mod.symbol_stream_data.data();
		
		//// Symbol info
		char* ptr2 = ptr;
		u32 signature = *(u32*)ptr;
		ptr += sizeof(u32);
		assert(signature == 4); // CV_SIGNATURE_C13
		
		while (ptr < ptr2 + mi->byte_size_of_symbol_information) {
			auto sym = (codeview_symbol_header*)ptr;
			
			ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
			ptr = align_up(ptr, 4);

			printf("> %4x %d\n", sym->kind, sym->length);

			switch (sym->kind) {
				case S_GPROC32: case S_LPROC32:
				case S_GPROC32_ID: case S_LPROC32_ID: {
					auto* proc = (PROCSYM32*)sym;
					printf(">> %s %4d %4d %8x %s\n",
						sym->kind == S_LPROC32 ? "L":"G",
						proc->seg, proc->len, proc->off, proc->name);

					mod.procsyms.push_back({ proc });
				} break;
			}
		}
		assert((ptr - ptr2) == mi->byte_size_of_symbol_information);
		
		//auto* c11_line_information = (u8*)ptr;
		ptr += mi->byte_size_of_c11_line_information;
		
		//// C13 line info
		printf("> c13_line_information\n");

		ptr2 = ptr;

		// first pass to find FILECHKSMS ptr
		char* filechksms_ptr = nullptr;
		while (ptr < ptr2 + mi->byte_size_of_c13_line_information) {
			auto* header = (codeview_subsection_header*)ptr;
			ptr += sizeof(codeview_subsection_header);

			if (header->type == DEBUG_S_FILECHKSMS) {
				filechksms_ptr = ptr;
				//read_file_checksum(header);
				break;
			}
			else {
				ptr += header->length;
			}
		}
		ptr = ptr2; // reset ptr
		
		auto read_line_numbers = [&] (codeview_subsection_header* header) {
			auto* ptr3 = ptr;

			// With usual compiler, this is once per function
			auto* lines = (codeview_line_header*)ptr;
			ptr += sizeof(codeview_line_header);
			assert(lines->flags == 0); // CV_LINES_HAVE_COLUMNS not implemented

			printf(">> Header %d, %8x %8x\n", lines->contribution_section_id, lines->contribution_offset, lines->contribution_size);
			
			while (ptr < ptr3 + header->length) {
				auto* line_block = (codeview_line_block_header*)ptr;
				ptr += sizeof(codeview_line_block_header);

				auto* cksm = (codeview_file_checksum*)(filechksms_ptr + line_block->offset_in_file_checksums);
				auto* name = &names[cksm->offset_in_string_table];

				printf(">> Block %d %d %s\n", line_block->block_size, line_block->offset_in_file_checksums, name);

				for (u32 i=0; i<line_block->amount_of_lines; i++) {
					auto* line = (codeview_line*)ptr;
					ptr += sizeof(codeview_line);

					printf(">>  Line %d %d\n", line->start_line_number, line->offset);
				}
			}
			assert((ptr - ptr3) == header->length);
		};
		auto read_file_checksums = [&] (codeview_subsection_header* header) {
			auto* ptr3 = ptr;
			while (ptr < ptr3 + header->length) {
				auto* file = (codeview_file_checksum*)ptr;
				ptr += sizeof(codeview_file_checksum);
				ptr += file->checksum_size;
				ptr = align_up(ptr, 4);

				auto* name = &names[file->offset_in_string_table];

				printf(">> File Checksum %s\n", name);
			}
			assert((ptr - ptr3) == header->length);
		};

		while (ptr < ptr2 + mi->byte_size_of_c13_line_information) {
			auto* header = (codeview_subsection_header*)ptr;
			ptr += sizeof(codeview_subsection_header);

			switch (header->type) {
				case DEBUG_S_LINES: {
					read_line_numbers(header);
				} break;
				//case DEBUG_S_FILECHKSMS: {
				//	read_file_checksum(header);
				//} break;
				default: {
					ptr += header->length;
				}
			}
		}
		assert((ptr - ptr2) == mi->byte_size_of_c13_line_information);

		auto global_references_bytes_size = *(u32*)ptr;
		auto num_global_references = global_references_bytes_size / 4;
		ptr += sizeof(u32);
		
		auto* global_references = (u32*)ptr;
		ptr += global_references_bytes_size;
		
		assert((ptr - mod.symbol_stream_data.data()) == streams[mi->stream_index_of_module_symbol_stream].size);
	}
	
public:
	struct ProcSym {
		PROCSYM32* proc;
	};
	struct Module {
		pdb_module_information* mi;
		std::string_view name;
		std::string_view file_name;

		std::vector<char> symbol_stream_data;
		std::vector<ProcSym> procsyms;
	};
	std::vector<Module> modules;

	const Section* find_section_for_addr (uintptr_t raddr, u32* out_sec_id) {
		for (u32 id=0; id<sections_sorted.size(); id++) {
			auto& sec = sections_sorted[id];
			if (raddr < sec.base_addr)
				break;
			if (/*raddr >= sec.base_addr && */raddr < sec.base_addr + sec.size) {
				*out_sec_id = id+1; // one based!
				return &sec;
			}
		}
		return nullptr;
	}
	void _assert_sections_sorted () {
		for (size_t i=1; i<sections_sorted.size(); i++) {
			assert(sections_sorted[i].base_addr > sections_sorted[i-1].base_addr + sections_sorted[i-1].size);
		}
	}
	
	// section_contributions.offset & size are explicitly signed for some reason despite the fact that they logically cannot be negative
	// (as negative sizes don't make sense and the relative address had to be positive to even cause us to look up this section)
	const pdb_section_contribution* find_section_contribution (u32 sec_id, s32 raddr) {
		// supposedly this is meant to be binary searched, but we could cache the subrange for each section to avoid like half the cost
		for (u32 i=0; i<num_section_contributions; i++) {
			auto& sc = section_contributions[i];
			if (sec_id == sc.section_id && raddr >= sc.offset && raddr < sc.offset + sc.size) {
				return &sc;
			}
		}

		return nullptr;
	}

	const ProcSym* find_procsym (Module& mod, u32 sec_id, u32 sec_raddr) {
		for (auto& ps : mod.procsyms) {
			if (sec_id == ps.proc->seg && sec_raddr >= ps.proc->off && sec_raddr < ps.proc->off + ps.proc->len) {
				return &ps;
			}
		}

		return nullptr;
	}

	bool find_source_loc (Module& mod, u32 sec_id, u32 sec_raddr, SourceLoc* out_src_loc) {
		auto* mi = mod.mi;
		char* ptr = mod.symbol_stream_data.data();
		
		ptr += mi->byte_size_of_symbol_information;
		ptr += mi->byte_size_of_c11_line_information;
		
		//// C13 line info
		char* c13_line_information = ptr;

		// first pass to find FILECHKSMS ptr
		char* filechksms_ptr = nullptr;
		while (ptr < c13_line_information + mi->byte_size_of_c13_line_information) {
			auto* header = (codeview_subsection_header*)ptr;
			ptr += sizeof(codeview_subsection_header);

			if (header->type == DEBUG_S_FILECHKSMS) {
				filechksms_ptr = ptr;
				break;
			}
			else {
				ptr += header->length;
			}
		}
		ptr = c13_line_information; // reset ptr
		
		while (ptr < c13_line_information + mi->byte_size_of_c13_line_information) {
			auto* header = (codeview_subsection_header*)ptr;
			ptr += sizeof(codeview_subsection_header);

			if (header->type == DEBUG_S_LINES) {
				auto* lines = (codeview_line_header*)ptr;
				ptr += sizeof(codeview_line_header);
				assert(lines->flags == 0); // CV_LINES_HAVE_COLUMNS not implemented
					
				if (  lines->contribution_section_id == sec_id &&
					  sec_raddr >= lines->contribution_offset && sec_raddr < lines->contribution_offset + lines->contribution_size) {
					u32 proc_raddr = sec_raddr - lines->contribution_offset;

					while (ptr < (char*)lines + header->length) {
						auto* line_block = (codeview_line_block_header*)ptr;
						ptr += sizeof(codeview_line_block_header);
						if (line_block->amount_of_lines > 0) {
							auto* lines = (codeview_line*)ptr;

							// codeview_lines seems to be sorted by offset, ie code address relative to start of function
							// there is only offset, no size, so I assume any addresses between this offset and the next belong to the line as well
							// lines can be out of order (earlier instructions belonging to later lines due to compiler optimizations for example)
							// lines will be missing (empty lines or lines with no generated code)
							// different entries can have the same line (single line to multiple instruction spans)
							// the same offset can appear twice with different lines (I guess multiple related lines that do one thing, maybe also when a statement is split over lines?)
							//  -> this part makes it confusing to resolve line numbers, as we would usually only return one (but go to disassembly and possibly breakpoints need info for each line!)
							//     tracy should never double count samples, and indeed dbghelp only reports one line, which appears like the lower one
							//     but it's unclear if the first match in the list is chosen or if the lower line number is actively chosen TODO:

							codeview_line* prev_line_with_lower_offset = &lines[0];
							for (u32 i=1; i<line_block->amount_of_lines; i++) {
								auto* line = &lines[i];

								// scan all lines and pick lowest lineno TODO: this could probably be simplified/accelerated by first deduplicating lines and storing the list of end addresses instead
								if (proc_raddr < line->offset) {
									// proc_raddr is in range [prev_offset, offset), so it belongs to all instructions with prev_offset
									// prev_line_with_lower_offset is the first one of these (lowest line number?)
									break;
								}
								if (line->offset != prev_line_with_lower_offset->offset)
									prev_line_with_lower_offset = line;
							}
							codeview_line* found_line = prev_line_with_lower_offset;

							auto* cksm = (codeview_file_checksum*)(filechksms_ptr + line_block->offset_in_file_checksums);
							auto* name = &names[cksm->offset_in_string_table];

							*out_src_loc = { name, found_line->start_line_number };
							return true;
						}
					}
					assert(ptr == (char*)lines + header->length);
				}
			}
			
			ptr = (char*)header + sizeof(codeview_subsection_header) + header->length;
		}
		return false;
	}

	static std::unique_ptr<PDB_File> try_load_pdb (std::string&& path) {
		try {
			return std::make_unique<PDB_File>(std::move(path));
		} catch (std::exception&) {
			//fprintf(stderr, "PDB loading exception: %s\n", ex.what());
		}
		return nullptr;
	}
	PDB_File (std::string&& path) {
		if (!load_file(path, &data)) {
			throw std::runtime_error("File not found: "+ path);
		}

		printf("%s data loaded\n", path.c_str());
		
		read_header();
		read_stream_table();
		read_pdb_info();
		read_names();
		read_DBI();
		
		assert(opt_streams->stream_index_of_section_header_dump != 0xFFFF);
		read_section_header_dump();

		printf("PDB read.\n");
	}
};

class SymResolver {
	HANDLE inspectee;

	struct LoadedModule {
		std::string path;

		uintptr_t base_addr;
		size_t size;

		std::unique_ptr<PDB_File> pdb;

		LoadedModule (std::string&& path, uintptr_t base_addr, size_t size) {
			auto pdb_path = std::filesystem::path(path);
			this->path = std::move(path);
			this->base_addr = base_addr;
			this->size = size;

			// Techically there might be more correct ways to find the pdb, and also ways that allow getting pdbs from microsoft servers
			// see above link
			pdb_path.replace_extension({".pdb"});
			pdb = PDB_File::try_load_pdb(pdb_path.string());
		}
	};
	struct ModuleCache {
		TimerMeasurement ttry_get_and_cache_module = TimerMeasurement("try_get_and_cache_module");

		std::vector<LoadedModule> sorted;
		
		const LoadedModule* cache (LoadedModule&& m) {
			auto base_addr = m.base_addr;
			sorted.push_back(std::move(m));
			// re-sort
			std::sort(sorted.begin(), sorted.end(), [] (LoadedModule& l, LoadedModule& r) {
				return std::less<uintptr_t>()(l.base_addr, r.base_addr);
			});

			for (auto& m : sorted) {
				if (base_addr == m.base_addr) return &m;
			}
			assert(false);
			return nullptr;
		}

		const LoadedModule* find_module_for_addr (HANDLE inspectee, uintptr_t addr) {
			for (auto& m : sorted) {
				if (addr > m.base_addr && addr <= m.base_addr + m.size) {
					return &m;
				}
			}
			
			TimerMeasZone(ttry_get_and_cache_module);
			return try_get_and_cache_module(inspectee, addr);
		}

		const LoadedModule* try_get_and_cache_module (HANDLE inspectee, uintptr_t addr) {
			// Only works for addresses in this process
			//// Do not use FreeLibrary because we set the flag GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
			//// see https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexa to get more information
			//constexpr DWORD flag = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
			//HMODULE mod = NULL;
			//
			//if (!GetModuleHandleExA(flag, (char*)addr, &mod)) {
			//	MODULEINFO info;
			//	if( GetModuleInformation( proc, mod, &info, sizeof( info ) ) != 0 )
			//	{
			//		const auto base = uint64_t( info.lpBaseOfDll );
			//		if( addr >= base && addr < ( base + info.SizeOfImage ) )
			//		{
			//			char name[1024];
			//			const auto nameLength = GetModuleFileNameA( mod, name, sizeof( name ) );
			//			if( nameLength > 0 )
			//			{
			//				// since this is the first time we encounter this module, load its symbols (needed for modules loaded after SymInitialize)
			//				ImageEntry* cachedModule = LoadSymbolsForModuleAndCache( name, nameLength, (DWORD64)info.lpBaseOfDll, info.SizeOfImage );
			//				return ModuleNameAndBaseAddress{ cachedModule->m_name, cachedModule->m_startAddress };
			//			}
			//		}
			//	}
			//}

			HMODULE modules[1024];
			DWORD needed = 0;
			if (!EnumProcessModules(inspectee, modules, sizeof(modules), &needed) || needed > sizeof(modules)) { // TODO: properly handle error
				print_err("EnumProcessModules");
			}

			// only return, and cache, the module that addr was in (as opposed to simply aching anything GetModuleInformation returns)
			// this causes more EnumProcessModules calls, but could help might make find_module_for_addr faster in the case where modules are never queried
			for (int i=0; i<needed/sizeof(HMODULE); i++) {
				auto& mod = modules[i];

				MODULEINFO info = {};
				if (GetModuleInformation(inspectee, mod, &info, sizeof(info))) {
					auto base = (uintptr_t)info.lpBaseOfDll;
					auto size = (size_t)info.SizeOfImage;
					if (addr >= base && addr < (base + size)) {
						char name[1024];
						auto nameLength = GetModuleFileNameExA(inspectee, mod, name, sizeof(name));
						if (nameLength > 0) {
							return cache(LoadedModule(std::string(name, nameLength), base, size));
						}
					}
				}
			}

			return nullptr;
		}
	};

	ModuleCache mod_cache;

public:
	SymResolver (HANDLE inspectee): inspectee{inspectee} {}

	void show_addr2sym (char* ptr) {
		uintptr_t addr = (uintptr_t)ptr;

		auto* mod = mod_cache.find_module_for_addr(inspectee, addr);
		if (!mod) {
			printf("#[%16llx]: Module not found\n", addr);
			return;
		}
		if (!mod->pdb) {
			printf("#[%16llx]: Module pdb not found\n", addr);
			return;
		}

		uintptr_t mod_raddr = addr - mod->base_addr;

		u32 sec_id = 0;
		auto* sec = mod->pdb->find_section_for_addr(mod_raddr, &sec_id);
		if (!sec) {
			printf("#[%16llx]: Section not found\n", addr);
			return;
		}

		uintptr_t sec_raddr = mod_raddr - sec->base_addr;
		assert(sec_raddr < 0x7fffffff);
		
		auto* sc = mod->pdb->find_section_contribution(sec_id, (s32)sec_raddr);
		if (!sc) {
			printf("#[%16llx]: Section contribution not found\n", addr);
			return;
		}

		auto& pdb_mod = mod->pdb->modules[sc->module_index];
		auto* ps = mod->pdb->find_procsym(pdb_mod, sec_id, (u32)sec_raddr);
		if (!ps) {
			printf("#[%16llx]: Symbol not found\n", addr);
			return;
		}
		
		SourceLoc src_loc = {};
		if (!mod->pdb->find_source_loc(pdb_mod, sec_id, (u32)sec_raddr, &src_loc)) {
			printf("#[%16llx]: Source location not found\n", addr);
			return;
		}

		printf("#[%16llx]: %-9s (%d) %4llx %-15s!%s %s:%d", addr, sec->name.c_str(), sec_id, sec_raddr, mod->path.c_str(), ps->proc->name, src_loc.filename, src_loc.lineno);

		printf("\n");
	}

	void print_timings () {
		mod_cache.ttry_get_and_cache_module.print();
	}
};
