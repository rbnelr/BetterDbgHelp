#pragma once
#include "util.hpp"

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

// Tags used by codeview and returned in SYMBOL_INFO
enum class SymTagEnum : uint8_t { // NOTE: type size not specified in the original headers, but we can make this compact
    SymTagNull,
    SymTagExe,
    SymTagCompiland,
    SymTagCompilandDetails,
    SymTagCompilandEnv,
    SymTagFunction, // 5
    SymTagBlock,
    SymTagData, // 7
    SymTagAnnotation,
    SymTagLabel,
    SymTagPublicSymbol, // 10
    SymTagUDT,
    SymTagEnum,
    SymTagFunctionType,
    SymTagPointerType,
    SymTagArrayType,
    SymTagBaseType,
    SymTagTypedef,
    SymTagBaseClass,
    SymTagFriend,
    SymTagFunctionArgType,
    SymTagFuncDebugStart,
    SymTagFuncDebugEnd,
    SymTagUsingNamespace,
    SymTagVTableShape,
    SymTagVTable,
    SymTagCustom,
    SymTagThunk, // 27
    SymTagCustomType,
    SymTagManagedType,
    SymTagDimension,
    SymTagCallSite,
    SymTagInlineSite, // 32
    SymTagBaseInterface,
    SymTagVectorType,
    SymTagMatrixType,
    SymTagHLSLType,
    SymTagCaller,
    SymTagCallee,
    SymTagExport,
    SymTagHeapAllocationSite,
    SymTagCoffGroup,
    SymTagMax
};

// seems like CV_typ_t are ids in TPI and IPI
// while CV_ItemId are not, but they are for inlinees?

typedef unsigned short ushort;
typedef unsigned long  ulong;

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

typedef enum LEAF_ENUM_e {
    // leaf indices starting records but referenced from symbol records

    LF_MODIFIER_16t     = 0x0001,
    LF_POINTER_16t      = 0x0002,
    LF_ARRAY_16t        = 0x0003,
    LF_CLASS_16t        = 0x0004,
    LF_STRUCTURE_16t    = 0x0005,
    LF_UNION_16t        = 0x0006,
    LF_ENUM_16t         = 0x0007,
    LF_PROCEDURE_16t    = 0x0008,
    LF_MFUNCTION_16t    = 0x0009,
    LF_VTSHAPE          = 0x000a,
    LF_COBOL0_16t       = 0x000b,
    LF_COBOL1           = 0x000c,
    LF_BARRAY_16t       = 0x000d,
    LF_LABEL            = 0x000e,
    LF_NULL             = 0x000f,
    LF_NOTTRAN          = 0x0010,
    LF_DIMARRAY_16t     = 0x0011,
    LF_VFTPATH_16t      = 0x0012,
    LF_PRECOMP_16t      = 0x0013,       // not referenced from symbol
    LF_ENDPRECOMP       = 0x0014,       // not referenced from symbol
    LF_OEM_16t          = 0x0015,       // oem definable type string
    LF_TYPESERVER_ST    = 0x0016,       // not referenced from symbol

    // leaf indices starting records but referenced only from type records

    LF_SKIP_16t         = 0x0200,
    LF_ARGLIST_16t      = 0x0201,
    LF_DEFARG_16t       = 0x0202,
    LF_LIST             = 0x0203,
    LF_FIELDLIST_16t    = 0x0204,
    LF_DERIVED_16t      = 0x0205,
    LF_BITFIELD_16t     = 0x0206,
    LF_METHODLIST_16t   = 0x0207,
    LF_DIMCONU_16t      = 0x0208,
    LF_DIMCONLU_16t     = 0x0209,
    LF_DIMVARU_16t      = 0x020a,
    LF_DIMVARLU_16t     = 0x020b,
    LF_REFSYM           = 0x020c,

    LF_BCLASS_16t       = 0x0400,
    LF_VBCLASS_16t      = 0x0401,
    LF_IVBCLASS_16t     = 0x0402,
    LF_ENUMERATE_ST     = 0x0403,
    LF_FRIENDFCN_16t    = 0x0404,
    LF_INDEX_16t        = 0x0405,
    LF_MEMBER_16t       = 0x0406,
    LF_STMEMBER_16t     = 0x0407,
    LF_METHOD_16t       = 0x0408,
    LF_NESTTYPE_16t     = 0x0409,
    LF_VFUNCTAB_16t     = 0x040a,
    LF_FRIENDCLS_16t    = 0x040b,
    LF_ONEMETHOD_16t    = 0x040c,
    LF_VFUNCOFF_16t     = 0x040d,

// 32-bit type index versions of leaves, all have the 0x1000 bit set
//
    LF_TI16_MAX         = 0x1000,

    LF_MODIFIER         = 0x1001,
    LF_POINTER          = 0x1002,
    LF_ARRAY_ST         = 0x1003,
    LF_CLASS_ST         = 0x1004,
    LF_STRUCTURE_ST     = 0x1005,
    LF_UNION_ST         = 0x1006,
    LF_ENUM_ST          = 0x1007,
    LF_PROCEDURE        = 0x1008,
    LF_MFUNCTION        = 0x1009,
    LF_COBOL0           = 0x100a,
    LF_BARRAY           = 0x100b,
    LF_DIMARRAY_ST      = 0x100c,
    LF_VFTPATH          = 0x100d,
    LF_PRECOMP_ST       = 0x100e,       // not referenced from symbol
    LF_OEM              = 0x100f,       // oem definable type string
    LF_ALIAS_ST         = 0x1010,       // alias (typedef) type
    LF_OEM2             = 0x1011,       // oem definable type string

    // leaf indices starting records but referenced only from type records

    LF_SKIP             = 0x1200,
    LF_ARGLIST          = 0x1201,
    LF_DEFARG_ST        = 0x1202,
    LF_FIELDLIST        = 0x1203,
    LF_DERIVED          = 0x1204,
    LF_BITFIELD         = 0x1205,
    LF_METHODLIST       = 0x1206,
    LF_DIMCONU          = 0x1207,
    LF_DIMCONLU         = 0x1208,
    LF_DIMVARU          = 0x1209,
    LF_DIMVARLU         = 0x120a,

    LF_BCLASS           = 0x1400,
    LF_VBCLASS          = 0x1401,
    LF_IVBCLASS         = 0x1402,
    LF_FRIENDFCN_ST     = 0x1403,
    LF_INDEX            = 0x1404,
    LF_MEMBER_ST        = 0x1405,
    LF_STMEMBER_ST      = 0x1406,
    LF_METHOD_ST        = 0x1407,
    LF_NESTTYPE_ST      = 0x1408,
    LF_VFUNCTAB         = 0x1409,
    LF_FRIENDCLS        = 0x140a,
    LF_ONEMETHOD_ST     = 0x140b,
    LF_VFUNCOFF         = 0x140c,
    LF_NESTTYPEEX_ST    = 0x140d,
    LF_MEMBERMODIFY_ST  = 0x140e,
    LF_MANAGED_ST       = 0x140f,

    // Types w/ SZ names

    LF_ST_MAX           = 0x1500,

    LF_TYPESERVER       = 0x1501,       // not referenced from symbol
    LF_ENUMERATE        = 0x1502,
    LF_ARRAY            = 0x1503,
    LF_CLASS            = 0x1504,
    LF_STRUCTURE        = 0x1505,
    LF_UNION            = 0x1506,
    LF_ENUM             = 0x1507,
    LF_DIMARRAY         = 0x1508,
    LF_PRECOMP          = 0x1509,       // not referenced from symbol
    LF_ALIAS            = 0x150a,       // alias (typedef) type
    LF_DEFARG           = 0x150b,
    LF_FRIENDFCN        = 0x150c,
    LF_MEMBER           = 0x150d,
    LF_STMEMBER         = 0x150e,
    LF_METHOD           = 0x150f,
    LF_NESTTYPE         = 0x1510,
    LF_ONEMETHOD        = 0x1511,
    LF_NESTTYPEEX       = 0x1512,
    LF_MEMBERMODIFY     = 0x1513,
    LF_MANAGED          = 0x1514,
    LF_TYPESERVER2      = 0x1515,

    LF_STRIDED_ARRAY    = 0x1516,    // same as LF_ARRAY, but with stride between adjacent elements
    LF_HLSL             = 0x1517,
    LF_MODIFIER_EX      = 0x1518,
    LF_INTERFACE        = 0x1519,
    LF_BINTERFACE       = 0x151a,
    LF_VECTOR           = 0x151b,
    LF_MATRIX           = 0x151c,

    LF_VFTABLE          = 0x151d,      // a virtual function table
    LF_ENDOFLEAFRECORD  = LF_VFTABLE,

    LF_TYPE_LAST,                    // one greater than the last type record
    LF_TYPE_MAX         = LF_TYPE_LAST - 1,

    LF_FUNC_ID          = 0x1601,    // global func ID
    LF_MFUNC_ID         = 0x1602,    // member func ID
    LF_BUILDINFO        = 0x1603,    // build info: tool, version, command line, src/pdb file
    LF_SUBSTR_LIST      = 0x1604,    // similar to LF_ARGLIST, for list of sub strings
    LF_STRING_ID        = 0x1605,    // string ID

    LF_UDT_SRC_LINE     = 0x1606,    // source and line on where an UDT is defined
                                     // only generated by compiler

    LF_UDT_MOD_SRC_LINE = 0x1607,    // module, source and line on where an UDT is defined
                                     // only generated by linker

    LF_ID_LAST,                      // one greater than the last ID record
    LF_ID_MAX           = LF_ID_LAST - 1,

    LF_NUMERIC          = 0x8000,
    LF_CHAR             = 0x8000,
    LF_SHORT            = 0x8001,
    LF_USHORT           = 0x8002,
    LF_LONG             = 0x8003,
    LF_ULONG            = 0x8004,
    LF_REAL32           = 0x8005,
    LF_REAL64           = 0x8006,
    LF_REAL80           = 0x8007,
    LF_REAL128          = 0x8008,
    LF_QUADWORD         = 0x8009,
    LF_UQUADWORD        = 0x800a,
    LF_REAL48           = 0x800b,
    LF_COMPLEX32        = 0x800c,
    LF_COMPLEX64        = 0x800d,
    LF_COMPLEX80        = 0x800e,
    LF_COMPLEX128       = 0x800f,
    LF_VARSTRING        = 0x8010,

    LF_OCTWORD          = 0x8017,
    LF_UOCTWORD         = 0x8018,

    LF_DECIMAL          = 0x8019,
    LF_DATE             = 0x801a,
    LF_UTF8STRING       = 0x801b,

    LF_REAL16           = 0x801c,
    
    LF_PAD0             = 0xf0,
    LF_PAD1             = 0xf1,
    LF_PAD2             = 0xf2,
    LF_PAD3             = 0xf3,
    LF_PAD4             = 0xf4,
    LF_PAD5             = 0xf5,
    LF_PAD6             = 0xf6,
    LF_PAD7             = 0xf7,
    LF_PAD8             = 0xf8,
    LF_PAD9             = 0xf9,
    LF_PAD10            = 0xfa,
    LF_PAD11            = 0xfb,
    LF_PAD12            = 0xfc,
    LF_PAD13            = 0xfd,
    LF_PAD14            = 0xfe,
    LF_PAD15            = 0xff,

} LEAF_ENUM_e;

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

typedef struct THUNKSYM32 {
    unsigned short  reclen;     // Record length
    unsigned short  rectyp;     // S_THUNK32
    unsigned long   pParent;    // pointer to the parent
    unsigned long   pEnd;       // pointer to this blocks end
    unsigned long   pNext;      // pointer to next symbol
    CV_uoff32_t     off;
    unsigned short  seg;
    unsigned short  len;        // length of thunk
    unsigned char   ord;        // THUNK_ORDINAL specifying type of thunk
    unsigned char   name[1];    // Length-prefixed name
    //unsigned char   variant[CV_ZEROLEN]; // variant portion of thunk
} THUNKSYM32;
typedef struct TRAMPOLINESYM {  // Trampoline thunk symbol
    unsigned short  reclen;     // Record length
    unsigned short  rectyp;     // S_TRAMPOLINE
    unsigned short  trampType;  // trampoline sym subtype
    unsigned short  cbThunk;    // size of the thunk
    CV_uoff32_t     offThunk;   // offset of the thunk
    CV_uoff32_t     offTarget;  // offset of the target of the thunk
    unsigned short  sectThunk;  // section index of the thunk
    unsigned short  sectTarget; // section index of the target of the thunk
} TRAMPOLINE;

typedef struct SECTIONSYM {
    unsigned short  reclen;             // Record length
    unsigned short  rectyp;             // S_SECTION

    unsigned short  isec;               // Section number
    unsigned char   align;              // Alignment of this section (power of 2)
    unsigned char   bReserved;          // Reserved.  Must be zero.
    unsigned long   rva;
    unsigned long   cb;
    unsigned long   characteristics;
    unsigned char   name[1];            // name
} SECTIONSYM;
typedef struct COFFGROUPSYM {
    unsigned short  reclen;             // Record length
    unsigned short  rectyp;             // S_COFFGROUP

    unsigned long   cb;
    unsigned long   characteristics;
    CV_uoff32_t     off;                // Symbol offset
    unsigned short  seg;                // Symbol segment
    unsigned char   name[1];            // name
} COFFGROUPSYM;
typedef struct EXPORTSYM {
    unsigned short  reclen;             // Record length
    unsigned short  rectyp;             // S_EXPORT

    unsigned short  ordinal;
    unsigned short  fConstant : 1;      // CONSTANT
    unsigned short  fData : 1;          // DATA
    unsigned short  fPrivate : 1;       // PRIVATE
    unsigned short  fNoName : 1;        // NONAME
    unsigned short  fOrdinal : 1;       // Ordinal was explicitly assigned
    unsigned short  fForwarder : 1;     // This is a forwarder
    unsigned short  reserved : 10;      // Reserved. Must be zero.
    unsigned char   name[1];            // name of
} EXPORTSYM;

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

typedef struct CV_funcattr_t {
    unsigned char  cxxreturnudt :1;  // true if C++ style ReturnUDT
    unsigned char  ctor         :1;  // true if func is an instance constructor
    unsigned char  ctorvbase    :1;  // true if func is an instance constructor of a class with virtual bases
    unsigned char  unused       :5;  // unused
} CV_funcattr_t;
typedef struct lfProc {
    unsigned short  _len;
    unsigned short  leaf;           // LF_PROCEDURE
    CV_typ_t        rvtype;         // type index of return value
    unsigned char   calltype;       // calling convention (CV_call_t)
    CV_funcattr_t   funcattr;       // attributes
    unsigned short  parmcount;      // number of parameters
    CV_typ_t        arglist;        // type index of argument list
} lfProc;

typedef struct lfFuncId {
    unsigned short  _len;
    unsigned short  leaf;       // LF_FUNC_ID
    CV_ItemId       scopeId;    // parent scope of the ID, 0 if global
    CV_typ_t        type;       // function type
    unsigned char   name[1]; 
} lfFuncId;
typedef struct lfMFuncId {
    unsigned short  _len;
    unsigned short  leaf;       // LF_MFUNC_ID
    CV_typ_t        parentType; // type index of parent
    CV_typ_t        type;       // function type
    unsigned char   name[1]; 
} lfMFuncId;

//typedef struct lfFieldList {
//    unsigned short  leaf;           // LF_FIELDLIST
//    char            data[1];         // field list sub lists
//} lfFieldList;
typedef struct lfArgList {
    unsigned short  _len;
    unsigned short  leaf;           // LF_ARGLIST, LF_SUBSTR_LIST
    unsigned long   count;          // number of arguments
    CV_typ_t        arg[1];      // number of arguments
} lfArgList;
typedef struct lfStringId {
    unsigned short  _len;
    unsigned short  leaf;       // LF_STRING_ID
    CV_ItemId       id;         // ID to list of sub string IDs
    unsigned char   name[1];
} lfStringId;

typedef struct CV_prop_t {
    unsigned short  packed      :1;     // true if structure is packed
    unsigned short  ctor        :1;     // true if constructors or destructors present
    unsigned short  ovlops      :1;     // true if overloaded operators present
    unsigned short  isnested    :1;     // true if this is a nested class
    unsigned short  cnested     :1;     // true if this class contains nested types
    unsigned short  opassign    :1;     // true if overloaded assignment (=)
    unsigned short  opcast      :1;     // true if casting methods
    unsigned short  fwdref      :1;     // true if forward reference (incomplete defn)
    unsigned short  scoped      :1;     // scoped definition
    unsigned short  hasuniquename :1;   // true if there is a decorated name following the regular name
    unsigned short  sealed      :1;     // true if class cannot be used as a base class
    unsigned short  hfa         :2;     // CV_HFA_e
    unsigned short  intrinsic   :1;     // true if class is an intrinsic type (e.g. __m128d)
    unsigned short  mocom       :2;     // CV_MOCOM_UDT_e
} CV_prop_t;
typedef struct lfClass {
    unsigned short  _len;
    unsigned short  leaf;           // LF_CLASS, LF_STRUCT, LF_INTERFACE
    unsigned short  count;          // count of number of elements in class
    CV_prop_t       property;       // property attribute field (prop_t)
    CV_typ_t        field;          // type index of LF_FIELD descriptor list
    CV_typ_t        derived;        // type index of derived from list if not zero
    CV_typ_t        vshape;         // type index of vshape table for this class
    unsigned char   data[1];         // data describing length of structure in
                                    // bytes and name
} lfClass;
typedef lfClass lfStructure;
typedef lfClass lfInterface;

typedef struct lfUnion {
    unsigned short  _len;
    unsigned short  leaf;           // LF_UNION
    unsigned short  count;          // count of number of elements in class
    CV_prop_t       property;       // property attribute field
    CV_typ_t        field;          // type index of LF_FIELD descriptor list
    unsigned char   data[1];         // variable length data describing length of
                                    // structure and name
} lfUnion;

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

struct tpi_stream_header{
    u32 version;
    u32 header_size;
    u32 minimal_type_index;
    u32 one_past_last_type_index;
    u32 byte_count_of_type_record_data_following_the_header;
    
    u16 stream_index_of_hash_stream;
    u16 stream_index_of_auxiliary_hash_stream;
    
    u32 hash_key_size;
    u32 number_of_hash_buckets;
    
    u32 hash_table_index_buffer_offset;
    u32 hash_table_index_buffer_length;
    
    u32 index_offset_buffer_offset;
    u32 index_offset_buffer_length;
    
    u32 udt_order_adjust_table_offset;
    u32 udt_order_adjust_table_length;
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
	SYM_ENUM_e kind;
};
struct codeview_type_record_header{
	u16 length;
	u16 kind;
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
    //CV_off32_t     extraFileId[1]; // remove VLA as we need sizeof(InlineeSourceLineEx)
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

size_t CbExtractNumeric(BYTE *pb, ulong *pul)
{
    ushort leaf = *(ushort *) pb;

    if (leaf < LF_NUMERIC) {
        *pul = leaf;

        return sizeof(leaf);
    }

    switch (leaf) {
        case LF_CHAR:
            *pul = *(char *) pb;
            return sizeof(leaf) + sizeof(char);

        case LF_SHORT:
            *pul = *(short *) pb;
            return sizeof(leaf) + sizeof(short);

        case LF_USHORT:
            *pul = *(ushort *) pb;
            return sizeof(leaf) + sizeof(ushort);

        case LF_LONG:
            *pul = *(long *) pb;
            return sizeof(leaf) + sizeof(long);

        case LF_ULONG:
            *pul = *(ulong *) pb;
            return sizeof(leaf) + sizeof(ulong);

        case LF_QUADWORD:
            return sizeof(leaf) + sizeof(__int64);

        case LF_UQUADWORD:
            return sizeof(leaf) + sizeof(unsigned __int64);
    }

    return 0;
}
