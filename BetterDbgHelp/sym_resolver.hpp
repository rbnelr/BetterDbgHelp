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

typedef unsigned long   CV_uoff32_t;
typedef          long   CV_off32_t;
typedef unsigned short  CV_uoff16_t;
typedef          short  CV_off16_t;
typedef unsigned short  CV_typ16_t;
typedef unsigned long   CV_typ_t;
typedef unsigned long   CV_pubsymflag_t;    // must be same as CV_typ_t.
typedef unsigned short  _2BYTEPAD;
typedef unsigned long   CV_tkn_t;
typedef CV_typ_t CV_ItemId;

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
inline const char* SYM_ENUM_e_str (SYM_ENUM_e val) {
	switch (val) {
		case S_COMPILE: return "S_COMPILE";
		case S_REGISTER_16t: return "S_REGISTER_16t";
		case S_CONSTANT_16t: return "S_CONSTANT_16t";
		case S_UDT_16t: return "S_UDT_16t";
		case S_SSEARCH: return "S_SSEARCH";
		case S_END: return "S_END";
		case S_SKIP: return "S_SKIP";
		case S_CVRESERVE: return "S_CVRESERVE";
		case S_OBJNAME_ST: return "S_OBJNAME_ST";
		case S_ENDARG: return "S_ENDARG";
		case S_COBOLUDT_16t: return "S_COBOLUDT_16t";
		case S_MANYREG_16t: return "S_MANYREG_16t";
		case S_RETURN: return "S_RETURN";
		case S_ENTRYTHIS: return "S_ENTRYTHIS";
		case S_BPREL16: return "S_BPREL16";
		case S_LDATA16: return "S_LDATA16";
		case S_GDATA16: return "S_GDATA16";
		case S_PUB16: return "S_PUB16";
		case S_LPROC16: return "S_LPROC16";
		case S_GPROC16: return "S_GPROC16";
		case S_THUNK16: return "S_THUNK16";
		case S_BLOCK16: return "S_BLOCK16";
		case S_WITH16: return "S_WITH16";
		case S_LABEL16: return "S_LABEL16";
		case S_CEXMODEL16: return "S_CEXMODEL16";
		case S_VFTABLE16: return "S_VFTABLE16";
		case S_REGREL16: return "S_REGREL16";
		case S_BPREL32_16t: return "S_BPREL32_16t";
		case S_LDATA32_16t: return "S_LDATA32_16t";
		case S_GDATA32_16t: return "S_GDATA32_16t";
		case S_PUB32_16t: return "S_PUB32_16t";
		case S_LPROC32_16t: return "S_LPROC32_16t";
		case S_GPROC32_16t: return "S_GPROC32_16t";
		case S_THUNK32_ST: return "S_THUNK32_ST";
		case S_BLOCK32_ST: return "S_BLOCK32_ST";
		case S_WITH32_ST: return "S_WITH32_ST";
		case S_LABEL32_ST: return "S_LABEL32_ST";
		case S_CEXMODEL32: return "S_CEXMODEL32";
		case S_VFTABLE32_16t: return "S_VFTABLE32_16t";
		case S_REGREL32_16t: return "S_REGREL32_16t";
		case S_LTHREAD32_16t: return "S_LTHREAD32_16t";
		case S_GTHREAD32_16t: return "S_GTHREAD32_16t";
		case S_SLINK32: return "S_SLINK32";
		case S_LPROCMIPS_16t: return "S_LPROCMIPS_16t";
		case S_GPROCMIPS_16t: return "S_GPROCMIPS_16t";
		case S_PROCREF_ST: return "S_PROCREF_ST";
		case S_DATAREF_ST: return "S_DATAREF_ST";
		case S_ALIGN: return "S_ALIGN";
		case S_LPROCREF_ST: return "S_LPROCREF_ST";
		case S_OEM: return "S_OEM";
		case S_TI16_MAX: return "S_TI16_MAX";
		case S_REGISTER_ST: return "S_REGISTER_ST";
		case S_CONSTANT_ST: return "S_CONSTANT_ST";
		case S_UDT_ST: return "S_UDT_ST";
		case S_COBOLUDT_ST: return "S_COBOLUDT_ST";
		case S_MANYREG_ST: return "S_MANYREG_ST";
		case S_BPREL32_ST: return "S_BPREL32_ST";
		case S_LDATA32_ST: return "S_LDATA32_ST";
		case S_GDATA32_ST: return "S_GDATA32_ST";
		case S_PUB32_ST: return "S_PUB32_ST";
		case S_LPROC32_ST: return "S_LPROC32_ST";
		case S_GPROC32_ST: return "S_GPROC32_ST";
		case S_VFTABLE32: return "S_VFTABLE32";
		case S_REGREL32_ST: return "S_REGREL32_ST";
		case S_LTHREAD32_ST: return "S_LTHREAD32_ST";
		case S_GTHREAD32_ST: return "S_GTHREAD32_ST";
		case S_LPROCMIPS_ST: return "S_LPROCMIPS_ST";
		case S_GPROCMIPS_ST: return "S_GPROCMIPS_ST";
		case S_FRAMEPROC: return "S_FRAMEPROC";
		case S_COMPILE2_ST: return "S_COMPILE2_ST";
		case S_MANYREG2_ST: return "S_MANYREG2_ST";
		case S_LPROCIA64_ST: return "S_LPROCIA64_ST";
		case S_GPROCIA64_ST: return "S_GPROCIA64_ST";
		case S_LOCALSLOT_ST: return "S_LOCALSLOT_ST";
		case S_PARAMSLOT_ST: return "S_PARAMSLOT_ST";
		case S_ANNOTATION: return "S_ANNOTATION";
		case S_GMANPROC_ST: return "S_GMANPROC_ST";
		case S_LMANPROC_ST: return "S_LMANPROC_ST";
		case S_RESERVED1: return "S_RESERVED1";
		case S_RESERVED2: return "S_RESERVED2";
		case S_RESERVED3: return "S_RESERVED3";
		case S_RESERVED4: return "S_RESERVED4";
		case S_LMANDATA_ST: return "S_LMANDATA_ST";
		case S_GMANDATA_ST: return "S_GMANDATA_ST";
		case S_MANFRAMEREL_ST: return "S_MANFRAMEREL_ST";
		case S_MANREGISTER_ST: return "S_MANREGISTER_ST";
		case S_MANSLOT_ST: return "S_MANSLOT_ST";
		case S_MANMANYREG_ST: return "S_MANMANYREG_ST";
		case S_MANREGREL_ST: return "S_MANREGREL_ST";
		case S_MANMANYREG2_ST: return "S_MANMANYREG2_ST";
		case S_MANTYPREF: return "S_MANTYPREF";
		case S_UNAMESPACE_ST: return "S_UNAMESPACE_ST";
		case S_ST_MAX: return "S_ST_MAX";
		case S_OBJNAME: return "S_OBJNAME";
		case S_THUNK32: return "S_THUNK32";
		case S_BLOCK32: return "S_BLOCK32";
		case S_WITH32: return "S_WITH32";
		case S_LABEL32: return "S_LABEL32";
		case S_REGISTER: return "S_REGISTER";
		case S_CONSTANT: return "S_CONSTANT";
		case S_UDT: return "S_UDT";
		case S_COBOLUDT: return "S_COBOLUDT";
		case S_MANYREG: return "S_MANYREG";
		case S_BPREL32: return "S_BPREL32";
		case S_LDATA32: return "S_LDATA32";
		case S_GDATA32: return "S_GDATA32";
		case S_PUB32: return "S_PUB32";
		case S_LPROC32: return "S_LPROC32";
		case S_GPROC32: return "S_GPROC32";
		case S_REGREL32: return "S_REGREL32";
		case S_LTHREAD32: return "S_LTHREAD32";
		case S_GTHREAD32: return "S_GTHREAD32";
		case S_LPROCMIPS: return "S_LPROCMIPS";
		case S_GPROCMIPS: return "S_GPROCMIPS";
		case S_COMPILE2: return "S_COMPILE2";
		case S_MANYREG2: return "S_MANYREG2";
		case S_LPROCIA64: return "S_LPROCIA64";
		case S_GPROCIA64: return "S_GPROCIA64";
		case S_LOCALSLOT: return "S_LOCALSLOT";
		case S_PARAMSLOT: return "S_PARAMSLOT";
		case S_LMANDATA: return "S_LMANDATA";
		case S_GMANDATA: return "S_GMANDATA";
		case S_MANFRAMEREL: return "S_MANFRAMEREL";
		case S_MANREGISTER: return "S_MANREGISTER";
		case S_MANSLOT: return "S_MANSLOT";
		case S_MANMANYREG: return "S_MANMANYREG";
		case S_MANREGREL: return "S_MANREGREL";
		case S_MANMANYREG2: return "S_MANMANYREG2";
		case S_UNAMESPACE: return "S_UNAMESPACE";
		case S_PROCREF: return "S_PROCREF";
		case S_DATAREF: return "S_DATAREF";
		case S_LPROCREF: return "S_LPROCREF";
		case S_ANNOTATIONREF: return "S_ANNOTATIONREF";
		case S_TOKENREF: return "S_TOKENREF";
		case S_GMANPROC: return "S_GMANPROC";
		case S_LMANPROC: return "S_LMANPROC";
		case S_TRAMPOLINE: return "S_TRAMPOLINE";
		case S_MANCONSTANT: return "S_MANCONSTANT";
		case S_ATTR_FRAMEREL: return "S_ATTR_FRAMEREL";
		case S_ATTR_REGISTER: return "S_ATTR_REGISTER";
		case S_ATTR_REGREL: return "S_ATTR_REGREL";
		case S_ATTR_MANYREG: return "S_ATTR_MANYREG";
		case S_SEPCODE: return "S_SEPCODE";
		case S_LOCAL_2005: "S_LOCAL_2005";
		case S_DEFRANGE_2005: return "S_DEFRANGE_2005";
		case S_DEFRANGE2_2005: return "S_DEFRANGE2_2005";
		case S_SECTION: return "S_SECTION";
		case S_COFFGROUP: return "S_COFFGROUP";
		case S_EXPORT: return "S_EXPORT";
		case S_CALLSITEINFO: return "S_CALLSITEINFO";
		case S_FRAMECOOKIE: return "S_FRAMECOOKIE";
		case S_DISCARDED: return "S_DISCARDED";
		case S_COMPILE3: return "S_COMPILE3";
		case S_ENVBLOCK: return "S_ENVBLOCK";
		case S_LOCAL: return "S_LOCAL";
		case S_DEFRANGE: return "S_DEFRANGE";
		case S_DEFRANGE_SUBFIELD: return "S_DEFRANGE_SUBFIELD";
		case S_DEFRANGE_REGISTER: return "S_DEFRANGE_REGISTER";
		case S_DEFRANGE_FRAMEPOINTER_REL: return "S_DEFRANGE_FRAMEPOINTER_REL";
		case S_DEFRANGE_SUBFIELD_REGISTER: return "S_DEFRANGE_SUBFIELD_REGISTER";
		case S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE: return "S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE";
		case S_DEFRANGE_REGISTER_REL: return "S_DEFRANGE_REGISTER_REL";
		case S_LPROC32_ID: return "S_LPROC32_ID";
		case S_GPROC32_ID: return "S_GPROC32_ID";
		case S_LPROCMIPS_ID: return "S_LPROCMIPS_ID";
		case S_GPROCMIPS_ID: return "S_GPROCMIPS_ID";
		case S_LPROCIA64_ID: return "S_LPROCIA64_ID";
		case S_GPROCIA64_ID: return "S_GPROCIA64_ID";
		case S_BUILDINFO: return "S_BUILDINFO";
		case S_INLINESITE: return "S_INLINESITE";
		case S_INLINESITE_END: return "S_INLINESITE_END";
		case S_PROC_ID_END: return "S_PROC_ID_END";
		case S_DEFRANGE_HLSL: return "S_DEFRANGE_HLSL";
		case S_GDATA_HLSL: return "S_GDATA_HLSL";
		case S_LDATA_HLSL: return "S_LDATA_HLSL";
		case S_FILESTATIC: return "S_FILESTATIC";
		case S_ARMSWITCHTABLE: return "S_ARMSWITCHTABLE";
		case S_CALLEES: return "S_CALLEES";
		case S_CALLERS: return "S_CALLERS";
		case S_POGODATA: return "S_POGODATA";
		case S_INLINESITE2: return "S_INLINESITE2";
		case S_HEAPALLOCSITE: return "S_HEAPALLOCSITE";
		case S_MOD_TYPEREF: return "S_MOD_TYPEREF";
		case S_REF_MINIPDB: return "S_REF_MINIPDB";
		case S_PDBMAP: return "S_PDBMAP";
		case S_GDATA_HLSL32: return "S_GDATA_HLSL32";
		case S_LDATA_HLSL32: return "S_LDATA_HLSL32";
		case S_GDATA_HLSL32_EX: return "S_GDATA_HLSL32_EX";
		case S_LDATA_HLSL32_EX: return "S_LDATA_HLSL32_EX";
	}
	return "[unknown]";
}


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
typedef union CV_PUBSYMFLAGS {
    CV_pubsymflag_t grfFlags;
    struct {
        CV_pubsymflag_t fCode       :  1;    // set if public symbol refers to a code address
        CV_pubsymflag_t fFunction   :  1;    // set if public symbol is a function
        CV_pubsymflag_t fManaged    :  1;    // set if managed code (native or IL)
        CV_pubsymflag_t fMSIL       :  1;    // set if managed IL code
        CV_pubsymflag_t __unused    : 28;    // must be zero
    };
} CV_PUBSYMFLAGS;

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
typedef struct REFSYM2 {
    unsigned short  reclen;     // Record length
    unsigned short  rectyp;     // S_PROCREF, S_DATAREF, or S_LPROCREF
    unsigned long   sumName;    // SUC of the name
    unsigned long   ibSym;      // Offset of actual symbol in $$Symbols
    unsigned short  imod;       // Module containing the actual symbol
    unsigned char   name[1];    // hidden name made a first class member
} REFSYM2;
typedef struct CONSTSYM {
    unsigned short  reclen;     // Record length
    unsigned short  rectyp;     // S_CONSTANT or S_MANCONSTANT
    CV_typ_t        typind;     // Type index (containing enum if enumerate) or metadata token
    unsigned short  value;      // numeric leaf containing value
    unsigned char   name[1];     // Length-prefixed name
} CONSTSYM;
typedef struct UDTSYM {
    unsigned short  reclen;     // Record length
    unsigned short  rectyp;     // S_UDT | S_COBOLUDT
    CV_typ_t        typind;     // Type index
    unsigned char   name[1];    // Length-prefixed name
} UDTSYM;
typedef struct DATASYM32 {
    unsigned short  reclen;     // Record length
    unsigned short  rectyp;     // S_LDATA32, S_GDATA32, S_LMANDATA, S_GMANDATA
    CV_typ_t        typind;     // Type index, or Metadata token if a managed symbol
    CV_uoff32_t     off;
    unsigned short  seg;
    unsigned char   name[1];    // Length-prefixed name
} DATASYM32;
typedef struct PUBSYM32 {
    unsigned short  reclen;     // Record length
    unsigned short  rectyp;     // S_PUB32
    CV_PUBSYMFLAGS  pubsymflags;
    CV_uoff32_t     off;
    unsigned short  seg;
    unsigned char   name[1];    // Length-prefixed name
} PUBSYM32;
typedef struct INLINESITESYM {
    unsigned short  reclen;    // Record length
    unsigned short  rectyp;    // S_INLINESITE
    unsigned long   pParent;   // pointer to the inliner
    unsigned long   pEnd;      // pointer to this block's end
    CV_ItemId       inlinee;   // CV_ItemId of inlinee
    unsigned char   binaryAnnotations[1];   // an array of compressed binary annotations.
} INLINESITESYM;

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
inline const char* DEBUG_S_SUBSECTION_TYPE_e_str (DEBUG_S_SUBSECTION_TYPE val) {
	switch (val) {
		case DEBUG_S_SYMBOLS				: return "DEBUG_S_SYMBOLS";
		case DEBUG_S_LINES					: return "DEBUG_S_LINES";
		case DEBUG_S_STRINGTABLE			: return "DEBUG_S_STRINGTABLE";
		case DEBUG_S_FILECHKSMS				: return "DEBUG_S_FILECHKSMS";
		case DEBUG_S_FRAMEDATA				: return "DEBUG_S_FRAMEDATA";
		case DEBUG_S_INLINEELINES			: return "DEBUG_S_INLINEELINES";
		case DEBUG_S_CROSSSCOPEIMPORTS		: return "DEBUG_S_CROSSSCOPEIMPORTS";
		case DEBUG_S_CROSSSCOPEEXPORTS		: return "DEBUG_S_CROSSSCOPEEXPORTS";
		case DEBUG_S_IL_LINES				: return "DEBUG_S_IL_LINES";
		case DEBUG_S_FUNC_MDTOKEN_MAP		: return "DEBUG_S_FUNC_MDTOKEN_MAP";
		case DEBUG_S_TYPE_MDTOKEN_MAP		: return "DEBUG_S_TYPE_MDTOKEN_MAP";
		case DEBUG_S_MERGED_ASSEMBLYINPUT	: return "DEBUG_S_MERGED_ASSEMBLYINPUT	";
		case DEBUG_S_COFF_SYMBOL_RVA		: return "DEBUG_S_COFF_SYMBOL_RVA";
	}
	return "[unknown]";
}

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

struct codeview_inlinee_source_line_header{
	u32 signature;
};
#define CV_INLINEE_SOURCE_LINE_SIGNATURE     0x0
#define CV_INLINEE_SOURCE_LINE_SIGNATURE_EX  0x1

typedef struct tagInlineeSourceLine {
    CV_ItemId      inlinee;       // function id.
    CV_off32_t     fileId;        // offset into file table DEBUG_S_FILECHKSMS
    CV_off32_t     sourceLineNum; // definition start line number.
} InlineeSourceLine;

typedef struct tagInlineeSourceLineEx {
    CV_ItemId      inlinee;       // function id
    CV_off32_t     fileId;        // offset into file table DEBUG_S_FILECHKSMS
    CV_off32_t     sourceLineNum; // definition start line number
    unsigned int   countOfExtraFiles;
    CV_off32_t     extraFileId[1];
} InlineeSourceLineEx;

typedef uint8_t CompressedAnnotation;
typedef CompressedAnnotation* PCompressedAnnotation;

///////////////////////////////////////////////////////////////////////////////
//
// Uncompress the data in pData and store the result into pDataOut.
//
// Return value is the uncompressed unsigned integer.  pData is incremented to
// point to the next piece of uncompressed data.
// 
// Returns -1 if what is passed in is incorrectly compressed data, such as
// (*pBytes & 0xE0) == 0xE0.
//
///////////////////////////////////////////////////////////////////////////////

inline uint32_t CVUncompressData(
    PCompressedAnnotation & pData)    // [IN,OUT] compressed data 
{
    uint32_t res = (uint32_t)(-1);

    if ((*pData & 0x80) == 0x00) {
        // 0??? ????

        res = (uint32_t)(*pData++);
    }
    else if ((*pData & 0xC0) == 0x80) {
        // 10?? ????

        res = (uint32_t)((*pData++ & 0x3f) << 8);
        res |= *pData++;
    }
    else if ((*pData & 0xE0) == 0xC0) {
        // 110? ???? 

        res = (*pData++ & 0x1f) << 24;
        res |= *pData++ << 16;
        res |= *pData++ << 8;
        res |= *pData++;
    }

    return res; 
}
inline __int32 DecodeSignedInt32(unsigned __int32 input)
{
    __int32 rotatedInput;

    if (input & 1) {
        rotatedInput = - (int)(input >> 1);
    } else {
        rotatedInput = input >> 1;
    }

    return rotatedInput;
}

// BinaryAnnotations ::= BinaryAnnotationInstruction+
// BinaryAnnotationInstruction ::= BinaryAnnotationOpcode Operand+
//
// The binary annotation mechanism supports recording a list of annotations
// in an instruction stream.  The X64 unwind code and the DWARF standard have
// similar design.
//
// One annotation contains opcode and a number of 32bits operands.
//
// The initial set of annotation instructions are for line number table
// encoding only.  These annotations append to S_INLINESITE record, and
// operands are unsigned except for BA_OP_ChangeLineOffset.

enum BinaryAnnotationOpcode : u32 {
    BA_OP_Invalid,               // link time pdb contains PADDINGs
    BA_OP_CodeOffset,            // param : start offset 
    BA_OP_ChangeCodeOffsetBase,  // param : nth separated code chunk (main code chunk == 0)
    BA_OP_ChangeCodeOffset,      // param : delta of offset
    BA_OP_ChangeCodeLength,      // param : length of code, default next start
    BA_OP_ChangeFile,            // param : fileId 
    BA_OP_ChangeLineOffset,      // param : line offset (signed)
    BA_OP_ChangeLineEndDelta,    // param : how many lines, default 1
    BA_OP_ChangeRangeKind,       // param : either 1 (default, for statement)
                                 //         or 0 (for expression)

    BA_OP_ChangeColumnStart,     // param : start column number, 0 means no column info
    BA_OP_ChangeColumnEndDelta,  // param : end column number delta (signed)

    // Combo opcodes for smaller encoding size.

    BA_OP_ChangeCodeOffsetAndLineOffset,  // param : ((sourceDelta << 4) | CodeDelta)
    BA_OP_ChangeCodeLengthAndCodeOffset,  // param : codeLength, codeOffset

    BA_OP_ChangeColumnEnd,       // param : end column number
};
inline const char* BinaryAnnotationOpcode_str (BinaryAnnotationOpcode val) {
	switch (val) {
		case BA_OP_Invalid							: return "BA_OP_Invalid";
		case BA_OP_CodeOffset						: return "BA_OP_CodeOffset";
		case BA_OP_ChangeCodeOffsetBase				: return "BA_OP_ChangeCodeOffsetBase";
		case BA_OP_ChangeCodeOffset					: return "BA_OP_ChangeCodeOffset";
		case BA_OP_ChangeCodeLength					: return "BA_OP_ChangeCodeLength";
		case BA_OP_ChangeFile						: return "BA_OP_ChangeFile";
		case BA_OP_ChangeLineOffset					: return "BA_OP_ChangeLineOffset";
		case BA_OP_ChangeLineEndDelta				: return "BA_OP_ChangeLineEndDelta";
		case BA_OP_ChangeRangeKind					: return "BA_OP_ChangeRangeKind";
		case BA_OP_ChangeColumnStart				: return "BA_OP_ChangeColumnStart";
		case BA_OP_ChangeColumnEndDelta				: return "BA_OP_ChangeColumnEndDelta";
		case BA_OP_ChangeCodeOffsetAndLineOffset	: return "BA_OP_ChangeCodeOffsetAndLineOffset";
		case BA_OP_ChangeCodeLengthAndCodeOffset	: return "BA_OP_ChangeCodeLengthAndCodeOffset";
		case BA_OP_ChangeColumnEnd					: return "BA_OP_ChangeColumnEnd";
	}
	return "[unknown]";
}

inline int BinaryAnnotationInstructionOperandCount(BinaryAnnotationOpcode op)
{
    return (op == BA_OP_ChangeCodeLengthAndCodeOffset) ? 2 : 1;
}

struct SourceLoc {
	const char* filepath;
	u32         lineno;
};

// PDBs are assumed to be next to exe of the same name for the moment
// PDBs of microsoft dlls are not gotten yet (which dbghelp.dll does somehow)

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

		// Only huge pdb files have more then one here, assert meant to test that case, but didn't see it yet
		// code should work fine though
		//assert(page_idx_page == 0);
		assert((char*)&header->page_list_of_stream_table_stream_page_list[page_idx_page] - (char*)header < header->page_size);
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
	std::vector<char> symbol_record_stream_data;

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
			//printf("Stream %3d: { ", si);
		
			u32 num_pages = ceil_div(stream.size, header->page_size);
			for (u32 i=0; i<num_pages; i++) {
				u32 page_idx = *(u32*)read_sts(cur);
				cur += sizeof(u32);
		
				stream.pages.push_back(page_idx);
		
				//printf("%d, ", page_idx);
			}
		
			//printf("}\n");
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
		
		//printf("Named Streams:\n");
		for(u32 index = 0, entry_index = 0; index < capacity && entry_index < amount_of_entries; index++){
			u32 word_index = index / (sizeof(u32) * 8);
			u32 bit_index  = index % (sizeof(u32) * 8);
			
			if(word_index < present_word_count && (present_bits[word_index] & (1u << bit_index))){
				auto& kv = entries[entry_index++];

				//std::string key = std::string(&string_buffer[kv.key]);
				//printf("> %s: %d\n", key.c_str(), kv.value);
				//named_streams[std::move(key)] = kv.value;
				std::string key = std::string(&string_buffer[kv.key]);
				//printf("> %s: %d\n", &string_buffer[kv.key], kv.value);
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

			//printf("> %d %-50s %-50s\n", mi->stream_index_of_module_symbol_stream, mod_name, file_name);

			Module m;
			m.mi = mi;
			m.name = std::string_view(mod_name, mod_name_len);
			m.file_name = std::string_view(file_name, file_name_len);

			modules.push_back(m);
		}
		assert((ptr - ptr2) == header->byte_size_of_the_module_information_substream);
		ptr = ptr2 + header->byte_size_of_the_module_information_substream;
		
		//// section_contribution_substream
		ptr2 = ptr;

		u32 DBISCImpv = *(u32*)ptr;
		ptr += sizeof(u32);

		assert(DBISCImpv == (0xeffe0000 + 19970605));
		
		u32 num_section_contributions = header->byte_size_of_the_section_contribution_substream / sizeof(pdb_section_contribution);
		auto* section_contributions = (pdb_section_contribution*)ptr;

		//for (u32 i=0; i<num_section_contributions; i++) {
		//	auto* sc = &section_contributions[i];
		//	printf("> %d %8x %8x %d\n", sc->section_id, sc->offset, sc->size, sc->module_index);
		//}
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
			
			//char name[9] = {};
			//strncpy_s(name, (const char*)sh->Name, 8); // properly null-terminate
			//printf("> %7s %8x %8x\n", name, sh->VirtualAddress, sh->Misc.VirtualSize);
		}

		for (size_t i=1; i<sections_sorted.size(); i++) {
			assert(sections_sorted[i].base_addr > sections_sorted[i-1].base_addr + sections_sorted[i-1].size);
		}
	}
	
	void read_symbol_record_stream () {
		auto* dbi = (dbi_stream_header*)DBI_data.data();
		symbol_record_stream_data = copy_into_consecutive(dbi->stream_index_of_the_symbol_record_stream);
		char* ptr = symbol_record_stream_data.data();

		char* ptr2 = ptr;

		auto push_symbol = [&] (u32 offs, u32 size, u16 seg, const char* name) {
			uintptr_t seg_addr = 0;
			if (seg > 0) {
				if (seg > sections_sorted.size()) {
					return; // No idea why this happens
				}
				seg_addr = sections_sorted[seg-1].base_addr;
			}
			sym_sorted.push_back(Symbol{ offs + seg_addr, size, name });
		};
		
		// TODO: these should be 
		while (ptr < ptr2 + symbol_record_stream_data.size()) {
			auto sym = (codeview_symbol_header*)ptr;
			
			ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
			ptr = align_up(ptr, 4);

			switch (sym->kind) {
				//case S_PROCREF: case S_DATAREF: case S_LPROCREF: {
				//	auto* s = (REFSYM2*)sym;
				//	printf("REFSYM2: %s\n", s->name);
				//} break;
				//case S_CONSTANT: case S_MANCONSTANT: { // mostly works, but weirdness with the name? maybe using 1 for zero length array is wrong
				//	auto* s = (CONSTSYM*)sym;
				//	printf("CONSTSYM: %s\n", s->name);
				//} break;
				//case S_UDT: case S_COBOLUDT: {
				//	auto* s = (UDTSYM*)sym;
				//	printf("UDTSYM: %s\n", s->name);
				//} break;
				case S_LDATA32: case S_GDATA32: case S_LMANDATA: case S_GMANDATA: {
					auto* s = (DATASYM32*)sym;
					push_symbol(
						s->off,
						0, // TODO: these symbols don't have a size, possibly becasue the size is implicit based on the data type?,
						s->seg,
						(const char*)s->name
					);
					//printf("DATASYM32: seg:%d offs:%4x %s\n", s->seg, s->off, s->name);
				} break;
				case S_PUB32: {
					auto* s = (PUBSYM32*)sym;
					push_symbol(
						s->off,
						0,
						s->seg,
						(const char*)s->name
					);
					//printf("PUBSYM32: seg:%d offs:%4x %s\n", s->seg, s->off, s->name);
				} break;
				default: {

				}
			}
		}
		assert((ptr - ptr2) == symbol_record_stream_data.size());
	}
	void read_module_symbol_stream (s16 module_index) {
		auto& mod = modules[module_index];
		auto* mi = mod.mi;
		
		std::unordered_map<uintptr_t, size_t> lookup_proc_sym;
		std::unordered_map<CV_ItemId, InlineeSourceLine*> inlinee_c13;
		size_t first_sym = sym_sorted.size();

		if (mi->stream_index_of_module_symbol_stream == 0xffff)
			return; // no symbol data
		mod.symbol_stream_data = copy_into_consecutive(mi->stream_index_of_module_symbol_stream);
		char* ptr = mod.symbol_stream_data.data();

		char* sym_info = ptr;
		ptr += mi->byte_size_of_symbol_information;
		
		//auto* c11_line_information = (u8*)ptr;
		ptr += mi->byte_size_of_c11_line_information;

		char* c13_line_information = ptr;
		ptr += mi->byte_size_of_c13_line_information;

		auto global_references_bytes_size = *(u32*)ptr;
		auto num_global_references = global_references_bytes_size / 4;
		ptr += sizeof(u32);
		
		auto* global_references = (u32*)ptr;
		ptr += global_references_bytes_size;
		
		assert((ptr - mod.symbol_stream_data.data()) == streams[mi->stream_index_of_module_symbol_stream].size);
		
		char* filechksms_ptr = nullptr;

		if (strcmp(mod.file_name.data(), "C:\\coding\\BetterDbgHelp\\TinyProgram\\x64\\Release\\main.obj") == 0) {
			printf("");
		}
		
		//// Symbol info
		auto parse_symbol_info = [&] () {
			char* ptr = sym_info;

			u32 signature = *(u32*)ptr;
			ptr += sizeof(u32);
			assert(signature == 4); // CV_SIGNATURE_C13
		
			printf(">> symbol_information\n");

			while (ptr < sym_info + mi->byte_size_of_symbol_information) {
				auto sym = (codeview_symbol_header*)ptr;
			
				ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
				ptr = align_up(ptr, 4);

				int entry_offs = (int)((char*)sym - sym_info);
				printf("> %7d [%4x] %d %s\n", entry_offs, sym->kind, sym->length, SYM_ENUM_e_str(sym->kind));

				switch (sym->kind) {
					case S_GPROC32: case S_LPROC32:
					case S_GPROC32_ID: case S_LPROC32_ID: {
						auto* proc = (PROCSYM32*)sym;
						printf(">> %s %4d %4d %8x %s\n",
							sym->kind == S_LPROC32 ? "L":"G",
							proc->seg, proc->len, proc->off, proc->name);

						uintptr_t module_raddr = proc->off + sections_sorted[proc->seg-1].base_addr;

						lookup_proc_sym.emplace(module_raddr, sym_sorted.size());

						Symbol s;
						s.base_addr = module_raddr;
						s.size = proc->len;
						s.name = (const char*)proc->name;
						s.sym_entry = proc;
						sym_sorted.push_back(std::move(s));
					} break;
					case S_INLINESITE: {
						auto* inl = (INLINESITESYM*)sym;
						//inl->pParent // byte offs from symbol_information start of prev PROCSYM32 or INLINESITESYM, ie caller
						//inl->pEnd // byte offs of INLINESITE_END
						// inl->inlinee seems to be some kind of id that lets us look up line info, but not sure where that is and if that lineinfo is encoded horribly
						// no idea what inl->binaryAnnotations is
						//printf(">> INLINESITE inlinee: %d\n", inl->inlinee);
					} break;
				}
			}
			assert((ptr - sym_info) == mi->byte_size_of_symbol_information);
		};
		auto parse_inlinesite_lineno_annotations = [&] (Symbol* sym) {
			auto parse = [&] (INLINESITESYM* inl) {
				PCompressedAnnotation cur = (PCompressedAnnotation)inl->binaryAnnotations;
				PCompressedAnnotation end = (PCompressedAnnotation)((char*)inl + sizeof(u16) + inl->reclen); // length field of codeview_symbol_header not contained in length
			
				auto it = inlinee_c13.find(inl->inlinee);
				if (it == inlinee_c13.end()) {
					assert(false);
					return;
				}
				auto* c13 = it->second;

				u32 file_index = c13->fileId;
				u32 code_offset_base = 0;
				u32 code_offset = 0; // TODO: according to getsentry, has to be parent offset, so partent inlinesite/or actual function?
				u32 code_length = 0; // 0 = null
				u32 lineno = c13->sourceLineNum;
				u32 line_length = 1;
				u32 kind = 1; // 0 == Expression, 1 == Statement

				u32 prev_code_offset = -1;

				struct Line {
					u32 code_offset;
					u32 code_length;
					u32 lineno;
					u32 line_length;
					u32 file_index;
					u32 kind;
				};
				std::vector<Line> lines;
				auto prev = [&] () -> Line* {
					if (lines.empty()) return nullptr;
					return &lines.back();
				};

				while (cur < end) {

					auto opcode = (BinaryAnnotationOpcode)CVUncompressData(cur);
					if (opcode == BA_OP_Invalid)
						continue;
					//if (BinaryAnnotationInstructionOperandCount(opcode) == 1) {
					//	auto val1 = CVUncompressData(cur);
					//	printf(">>> %s: %d\n", BinaryAnnotationOpcode_str(opcode), val1);
					//}
					//else {
					//	auto val1 = CVUncompressData(cur);
					//	auto val2 = CVUncompressData(cur);
					//	printf(">>> %s: %d %d\n", BinaryAnnotationOpcode_str(opcode), val1, val2);
					//}

					Line* prev = lines.empty() ? nullptr : &lines.back();
					bool emit = false;

					auto arg1 = CVUncompressData(cur);
					switch (opcode) {
						case BA_OP_CodeOffset: {
							code_offset = arg1;
						} break;
						case BA_OP_ChangeCodeOffsetBase: {
							code_offset_base = arg1;
						} break;
						case BA_OP_ChangeCodeOffset: {
							code_offset += arg1;
							emit = true;
						} break;
						case BA_OP_ChangeCodeLength: {
							if (prev) {
								if (prev->code_length == 0 && prev->kind == kind) {
									prev->code_length = arg1;
								}
							}
							code_offset += arg1;
						} break;
						case BA_OP_ChangeFile: {
							// https://github.com/getsentry/pdb/blob/master/src/modi/c13.rs
							// NOTE: There seems to be a bug in VS2015-VS2019 compilers that generates
							// invalid binary annotations when file changes are involved. This can be
							// triggered by #including files directly into inline functions. The
							// `ChangeFile` annotations are generated in the wrong spot or missing
							// completely. This renders information on the file effectively useless in a lot
							// of cases.

							file_index = arg1;
						} break;
						case BA_OP_ChangeLineOffset: {
							lineno += DecodeSignedInt32(arg1);
						} break;
						case BA_OP_ChangeLineEndDelta: {
							line_length = arg1;
						} break;
						case BA_OP_ChangeRangeKind: {
							assert(arg1 == 0 || arg1 == 1);
							kind = arg1;
						} break;
						case BA_OP_ChangeColumnStart: {
							// ignore column info
						} break;
						case BA_OP_ChangeColumnEndDelta: {
							// ignore column info
						} break;
						case BA_OP_ChangeCodeOffsetAndLineOffset: {
							// param : ((sourceDelta << 4) | CodeDelta)
							u32 CodeDelta = arg1 & 0b1111;
							s32 sourceDelta = DecodeSignedInt32(arg1 >> 4);

							code_offset += CodeDelta;
							lineno += sourceDelta;
							emit = true;
						} break;
						case BA_OP_ChangeCodeLengthAndCodeOffset: {
							auto arg2 = CVUncompressData(cur);
							code_length = arg1;
							code_offset += arg2;
							emit = true;
						} break;
						case BA_OP_ChangeColumnEnd: {
							// ignore column info
						} break;
						default: {
							assert(false);
						}
					}

					if (!emit) {
						continue;
					}

					u32 offset = code_offset + code_offset_base;
					if (prev) {
						if (prev->code_length == 0 && prev->kind == kind) {
							prev->code_length = offset - prev->code_offset;
						}
					}

					lines.push_back({
						offset,
						code_length,
						lineno,
						line_length,
						file_index,
						kind,
					});

					// Code length resets with every line record.
					code_length = 0;
				}

				for (auto& l : lines) {
					auto* cksm = (codeview_file_checksum*)(filechksms_ptr + l.file_index);
					auto* name = &names[cksm->offset_in_string_table];

					printf(">>> %4x %4x %s:%d (%d %d)\n", l.code_offset, l.code_length, name, l.lineno, l.line_length, l.kind);
				}

				assert(cur == end);
			};
			
			assert(sym->sym_entry);
			if (sym->sym_entry == nullptr)
				return;
			
			printf("> for %s:\n", sym->name);

			char* ptr = (char*)sym->sym_entry;
			for (;;) {
				auto sym = (codeview_symbol_header*)ptr;
				ptr += sizeof(u16) + sym->length; // length field of codeview_symbol_header not contained in length (but kind is)
				ptr = align_up(ptr, 4);

				switch (sym->kind) {
					case S_INLINESITE: {
						auto* inl = (INLINESITESYM*)sym;
						printf(">> INLINESITE %d:\n", inl->inlinee);
						parse(inl);
					} break;
					case S_END: {
						return;
					} break;
				}
			}
		};

		//// C13 line info
		auto parse_c13 = [&] () {
			//printf("> c13_line_information\n");
			char* ptr = c13_line_information;
			
			// first pass to find FILECHKSMS ptr
			while (ptr < c13_line_information + mi->byte_size_of_c13_line_information) {
				auto* header = (codeview_subsection_header*)ptr;
				ptr += sizeof(codeview_subsection_header);

				if (header->type == DEBUG_S_FILECHKSMS) {
					filechksms_ptr = ptr;
					//read_file_checksum(header);
					break;
				}
				ptr = (char*)header + sizeof(codeview_subsection_header) + header->length;
			}
			ptr = c13_line_information; // reset ptr
		
			auto read_line_numbers = [&] (codeview_subsection_header* subsec) {
				auto* ptr3 = ptr;

				// With usual compiler, this is once per function
				auto* header = (codeview_line_header*)ptr;
				ptr += sizeof(codeview_line_header);
				assert(header->flags == 0); // CV_LINES_HAVE_COLUMNS not implemented

				//printf(">> Header %d, %8x %8x\n", lines->contribution_section_id, lines->contribution_offset, lines->contribution_size);
			
				uintptr_t sec_offs = sections_sorted[header->contribution_section_id-1].base_addr;
				uintptr_t module_raddr = header->contribution_offset + sec_offs;
				auto it = lookup_proc_sym.find(module_raddr);
				auto* sym = it != lookup_proc_sym.end() ? &sym_sorted[it->second] : nullptr;

				while (ptr < ptr3 + subsec->length) {
					auto* line_block = (codeview_line_block_header*)ptr;
					ptr += sizeof(codeview_line_block_header);
			
					auto* cksm = (codeview_file_checksum*)(filechksms_ptr + line_block->offset_in_file_checksums);
					auto* name = &names[cksm->offset_in_string_table];
			
					//printf(">> Block %d %d %s\n", line_block->block_size, line_block->offset_in_file_checksums, name);
			
					for (u32 i=0; i<line_block->amount_of_lines; i++) {
						auto* line = (codeview_line*)ptr;
						ptr += sizeof(codeview_line);
			
						//printf(">>  Line %d %d\n", line->start_line_number, line->offset);
					}
				}
				assert((ptr - ptr3) == subsec->length);

				if (sym) {
					assert(module_raddr == sym->base_addr); // lines section contribtion offset need to be procedure symbol offset
				
					//if (strcmp(sym->name, "main") == 0) {
					//	printf("");
					//}

					ptr = ptr3;
					ptr += sizeof(codeview_line_header);

					while (ptr < ptr3 + subsec->length) {
						auto* line_block = (codeview_line_block_header*)ptr;
						ptr += sizeof(codeview_line_block_header);

						auto* cksm = (codeview_file_checksum*)(filechksms_ptr + line_block->offset_in_file_checksums);
						auto* name = &names[cksm->offset_in_string_table];

						sym->src.push_back(Symbol::SrcLines {
							line_block->amount_of_lines,
							name,
							(codeview_line*)ptr,
						});

						ptr += line_block->amount_of_lines * sizeof(codeview_line);
					}
				}
			};
			auto read_inlinee_line_numbers = [&] (codeview_subsection_header* subsec) {
				auto* ptr3 = ptr;

				///// Flag indicating the default format of `DEBUG_S_INLINEELINEINFO`
				//pub const CV_INLINEE_SOURCE_LINE_SIGNATURE: u32 = 0x0;
				///// Flag indicating the extended format of `DEBUG_S_INLINEELINEINFO`
				//pub const CV_INLINEE_SOURCE_LINE_SIGNATURE_EX: u32 = 0x1;

				auto* header = (codeview_inlinee_source_line_header*)ptr;
				ptr += sizeof(codeview_inlinee_source_line_header);
				assert(header->signature == CV_INLINEE_SOURCE_LINE_SIGNATURE);
			
				while (ptr < ptr3 + subsec->length) {
					auto* line = (InlineeSourceLine*)ptr;
					ptr += sizeof(InlineeSourceLine);
			
					auto* cksm = (codeview_file_checksum*)(filechksms_ptr + line->fileId);
					auto* name = &names[cksm->offset_in_string_table];

					printf(">>  Line %d %s %d\n", line->sourceLineNum, name, line->inlinee);

					inlinee_c13.emplace(line->inlinee, line);
				}
				assert((ptr - ptr3) == subsec->length);
			};

			while (ptr < c13_line_information + mi->byte_size_of_c13_line_information) {
				auto* header = (codeview_subsection_header*)ptr;
				ptr += sizeof(codeview_subsection_header);

				printf(">> %s\n", DEBUG_S_SUBSECTION_TYPE_e_str(header->type));

				if ((header->type & DEBUG_S_IGNORE) == 0) {
					switch (header->type) {
						case DEBUG_S_LINES: {
							read_line_numbers(header);
						} break;
						case DEBUG_S_INLINEELINES: {
							read_inlinee_line_numbers(header);
						} break;
					}
				}
				ptr = (char*)header + sizeof(codeview_subsection_header) + header->length;
			}
			assert((ptr - c13_line_information) == mi->byte_size_of_c13_line_information);
		};
		
		parse_symbol_info();
		parse_c13();

		for (size_t i=first_sym; i<sym_sorted.size(); i++) {
			parse_inlinesite_lineno_annotations(&sym_sorted[i]);
		}

		if (strcmp(mod.file_name.data(), "C:\\coding\\BetterDbgHelp\\TinyProgram\\x64\\Release\\main.obj") == 0) {
			printf("");
		}
	}

public:
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

		read_symbol_record_stream();

		for (s16 module_index=0; module_index<(s16)modules.size(); module_index++) {
			read_module_symbol_stream(module_index);
		}

		sort_symbols();

		printf("PDB read.\n");
	}

	struct Module {
		pdb_module_information* mi;
		std::string_view name;
		std::string_view file_name;

		std::vector<char> symbol_stream_data;
	};
	std::vector<Module> modules;

	struct Symbol {
		uintptr_t base_addr; // relative to module
		size_t size;
		char const* name;

		struct SrcLines {
			uint32_t num_lines = 0;
			char const* filename = nullptr;
			codeview_line* lines = nullptr;
		};
		std::vector<SrcLines> src;

		PROCSYM32* sym_entry; // needed for scanning INLINESITEs later
	};
	std::vector<Symbol> sym_sorted;

	void sort_symbols () {
		// sort based on base_addr
		std::stable_sort(sym_sorted.begin(), sym_sorted.end(), [] (Symbol const& l, Symbol const& r) {
			return std::less<uintptr_t>()(l.base_addr, r.base_addr);
		});

		// seems like functions like printf will appear both as procedure symbols with size in modules
		// and as PUB32 symbols without a size but with mangled names, and thus we will always have overlapping symbols
		
		//// assert non overlap including size
		//for (size_t i=1; i<sym_sorted.size(); i++) {
		//	//assert(sym_sorted[i].base_addr > sym_sorted[i-1].base_addr + sym_sorted[i-1].size);
		//	if (!(sym_sorted[i].base_addr > sym_sorted[i-1].base_addr + sym_sorted[i-1].size)) {
		//		printf("!! Overlapping symbols: [%8llx] %s/%s\n", sym_sorted[i].base_addr, sym_sorted[i-1].name, sym_sorted[i].name);
		//	}
		//}
	}
	Symbol* find_symbol_for_addr (uintptr_t addr) {
		// need to find first symbol with lower or equal address than addr, but lower bound only returns that in equal case,
		// so use upper bound instead (returns first item bigger than addr), then use previous
		auto dymmy_Symbol = Symbol{
			addr, 0, nullptr
		};
		auto it = std::upper_bound(sym_sorted.begin(), sym_sorted.end(), dymmy_Symbol, [] (Symbol const& l, Symbol const& r) {
			return l.base_addr < r.base_addr;
		});
		if (it <= sym_sorted.begin()) {
			// first symbol after addr is first symbol, search failed
			return nullptr;
		}
		it--;
		return &*it;
	}
	
	bool find_source_loc_for_addr (Symbol* sym, uintptr_t addr, SourceLoc* out_src_loc) {
		uintptr_t proc_raddr = addr - sym->base_addr;
		if (proc_raddr >= sym->size) {
			// past symbol address range, no valid line number
			return false;
		}

		for (auto& src : sym->src) {
			// codeview_lines seems to be sorted by offset, ie code address relative to start of function
			// there is only offset, no size, so I assume any addresses between this offset and the next belong to the line as well
			// lines can be out of order (earlier instructions belonging to later lines due to compiler optimizations for example)
			// lines will be missing (empty lines or lines with no generated code)
			// different entries can have the same line (single line to multiple instruction spans)
			// the same offset can appear twice with different lines (I guess multiple related lines that do one thing, maybe also when a statement is split over lines?)
			//  -> this part makes it confusing to resolve line numbers, as we would likely only return the first line (but debuggers via 'go to disassembly' or breakpoints might need info for each line!)
			//     tracy should never double count samples, and indeed dbghelp only reports one line, which appears the first line
			//     but it's unclear if the first match in this list is chosen or if it actively looks for the lowest line number TODO: determine via fuzzing and consider alternative datastructure)

			// linear scan for the moment, profile to see how much this impacts perf
			codeview_line* prev_line_with_lower_offset = src.lines;
			for (u32 i=1; i<src.num_lines; i++) {
				auto* line = &src.lines[i];

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

			*out_src_loc = { src.filename, found_line->start_line_number };
			return true;
		}
		return false;
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
			this->path = std::move(path);
			this->base_addr = base_addr;
			this->size = size;
		}
		void load_pdb () {
			// Techically there might be more correct ways to find the pdb, and also ways that allow getting pdbs from microsoft servers
			// see above link
			auto pdb_path = std::filesystem::path(path);
			pdb_path.replace_extension({".pdb"});
			pdb = PDB_File::try_load_pdb(pdb_path.string());
		}
	};
	struct ModuleCache {
		TimerMeasurement ttry_get_and_cache_module = TimerMeasurement("try_get_and_cache_module");
		TimerMeasurement tload_pdb = TimerMeasurement("load_pdb");

		std::vector<LoadedModule> sorted;
		
		LoadedModule* cache (LoadedModule&& m) {
			auto base_addr = m.base_addr;
			sorted.push_back(std::move(m));
			// re-sort
			std::sort(sorted.begin(), sorted.end(), [] (LoadedModule const& l, LoadedModule const& r) {
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
				if (addr >= m.base_addr && addr < m.base_addr + m.size) {
					return &m;
				}
			}
			
			return try_get_and_cache_module(inspectee, addr);
		}

		const LoadedModule* try_get_and_cache_module (HANDLE inspectee, uintptr_t addr) {
			LoadedModule* loaded = nullptr;
			{
				TimerMeasZone(ttry_get_and_cache_module);
				HMODULE modules[1024];
				DWORD needed = 0;
				if (!EnumProcessModules(inspectee, modules, sizeof(modules), &needed) || needed > sizeof(modules)) { // TODO: properly handle error
					print_err_throw("EnumProcessModules");
				}

				// only return, and cache, the module that addr was in (as opposed to simply aching anything GetModuleInformation returns)
				// this causes more EnumProcessModules calls, but could help might make find_module_for_addr faster in the case where modules are never queried
				for (int i=0; i<needed/sizeof(HMODULE); i++) {
					auto& mod = modules[i];

					MODULEINFO info = {};
					if (GetModuleInformation(inspectee, mod, &info, sizeof(info))) {
						auto base = (uintptr_t)info.lpBaseOfDll;
						auto size = (size_t)info.SizeOfImage;
						if (addr >= base && addr < base + size) {
							char name[1024];
							auto nameLength = GetModuleFileNameExA(inspectee, mod, name, sizeof(name));
							if (nameLength > 0) {
								loaded = cache(LoadedModule(std::string(name, nameLength), base, size));
								break;
							}
						}
					}
				}
			}
			if (loaded) {
				TimerMeasZone(tload_pdb);
				loaded->load_pdb();
			}
			return loaded;
		}
	};

	ModuleCache mod_cache;
	
	// warmup time not meaningful as it includes pdb loading
	// only warmup to avoid including pdb loading in later measurement
	//TimerMeasurement twarmup = TimerMeasurement("warmup");
	TimerMeasurement taddr2sym = TimerMeasurement("addr2sym");

public:
	typedef const char* err_t;
	struct Result {
		// TODO: dbghelp.dll requires us to pass in a string buffer, and I want to avoid heap alloc for the moment
		static inline constexpr unsigned STRBUF_SIZE = 4096;
		char str_buf[STRBUF_SIZE];

		const char* module_path = nullptr;
		const char* sym_name = nullptr;

		const char* src_filepath = nullptr;
		uint32_t    src_lineno = 0;

		bool has_source () const {
			return src_filepath != nullptr;
		}

		// TODO: inline frames

		bool operator== (Result const& r) const {
			// dbghelp.dll not returning module name, assume it's correct
			//if (strcmp(module_path, r.module_path) != 0) return false;
			if (strcmp(sym_name, r.sym_name) != 0) return false;

			if (has_source() != r.has_source()) return false;
			if (has_source()) {
				if (strcmp(src_filepath, r.src_filepath) != 0) return false;
				if (src_lineno != r.src_lineno) return false;
			}

			return true;
		}
		bool operator!= (Result const& r) const {
			return !(*this == r);
		}

		void print_diff (Result const& r) const {
			//if (   strcmp(module_path, r.module_path) != 0
			//	|| strcmp(sym_name, r.sym_name) != 0 ) {
			//	printf("> sym:         \"%s!%s\" !=\n", module_path,sym_name);
			//	printf("> dbghelp:dll: \"%s!%s\"\n", r.module_path,r.sym_name);
			//}
			if (strcmp(sym_name, r.sym_name) != 0) {
				printf("> sym:         \"%s!%s\" !=\n", module_path,sym_name);
				printf("> dbghelp:dll: \"?!%s\"\n", r.sym_name);
			}
			if (   has_source() != r.has_source()
				|| strcmp(src_filepath, r.src_filepath) != 0 || src_lineno != r.src_lineno) {
				
				if (has_source()) {
					printf("> sym:         \"%s:%d\" !=\n", src_filepath,src_lineno);
				}
				else {
					printf("> sym:         (No source info) !=\n");
				}

				if (r.has_source()) {
					printf("> dbghelp:dll: \"%s:%d\"\n", r.src_filepath,r.src_lineno);
				}
				else {
					printf("> dbghelp:dll: (No source info)\n");
				}
			}
		}
	};

	SymResolver (HANDLE inspectee): inspectee{inspectee} {}
	
	bool show_addr2sym (char* ptr) {
		Result res = {};
		auto err = addr2sym(ptr, &res);
		if (err) {
			printf("#[%16llx]: %s\n", (uintptr_t)ptr, err);
			return false;
		}

		printf("#[%16llx]: %-15s!%s ", (uintptr_t)ptr, res.module_path, res.sym_name);
		if (res.has_source()) {
			printf("%s:%d\n", res.src_filepath, res.src_lineno);
		}
		else {
			printf("(No source info)\n");
		}
		return true;
	}
	void warmup_addr2sym (char* ptr) {
		Result res = {};
		addr2sym(ptr, &res);
	}
	void measure_addr2sym (char* ptr) {
		Result res = {};
		const char* err = 0;
		{
			TimerMeasZone(taddr2sym);
			addr2sym(ptr, &res);
		}
	}

	err_t addr2sym (void* ptr, Result* res) {
		uintptr_t addr = (uintptr_t)ptr;

		auto* mod = mod_cache.find_module_for_addr(inspectee, addr);
		if (!mod) {
			return "Module not found";
		}
		if (!mod->pdb) {
			return "Module pdb not found";
		}

		uintptr_t mod_raddr = addr - mod->base_addr;
		
		auto sym = mod->pdb->find_symbol_for_addr(mod_raddr);
		if (!sym) {
			return "Symbol not found";
		}
		
		res->module_path = mod->path.c_str();
		res->sym_name = sym->name;
		res->src_filepath = nullptr;
		res->src_lineno = 0;

		SourceLoc src_loc = {};
		if (!mod->pdb->find_source_loc_for_addr(sym, mod_raddr, &src_loc)) {
			return nullptr;
		}

		res->src_filepath = src_loc.filepath;
		res->src_lineno = src_loc.lineno;
		return nullptr;
	}

	void print_timings () {
		mod_cache.ttry_get_and_cache_module.print();
		mod_cache.tload_pdb.print();
		taddr2sym.print();
	}
};
