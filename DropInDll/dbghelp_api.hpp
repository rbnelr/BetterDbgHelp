#pragma once
#include "util.hpp"

// dbghelp.h would normally define dllimport functions, but we're trying to make a dll mimicing dbghelp.dll here
// by defining _IMAGEHLP_SOURCE_, the header declares the functions without dllimport, which we then ignore and export custom wrapper ones
// but we need all the types structs and function pointer typdefs (callbacks) to delegate to the original dll
// (despite the fact that delegating a function call could simply be done with a single jmp instruction)
#define _IMAGEHLP_SOURCE_
#include <dbghelp.h>

// =============================================================
// Function Typdefs

typedef BOOL (__stdcall* t_EnumDirTree) (
	HANDLE                hProcess,
	PCSTR                 RootPath,
	PCSTR                 InputPathName,
	PSTR                  OutputPathBuffer,
	PENUMDIRTREE_CALLBACK cb,
	PVOID                 data
	);

typedef LPAPI_VERSION (__stdcall* t_ImagehlpApiVersion) ();

typedef LPAPI_VERSION (__stdcall* t_ImagehlpApiVersionEx) (
	LPAPI_VERSION AppVersion
	);


typedef BOOL (__stdcall* t_MakeSureDirectoryPathExists) (
	PCSTR DirPath
	);

typedef BOOL (__stdcall* t_SearchTreeForFile) (
	PCSTR RootPath,
	PCSTR InputPathName,
	PSTR  OutputPathBuffer
	);

typedef BOOL (__stdcall* t_EnumerateLoadedModules64) (
	HANDLE                         hProcess,
	PENUMLOADED_MODULES_CALLBACK64 EnumLoadedModulesCallback,
	PVOID                          UserContext
	);

typedef BOOL (__stdcall* t_EnumerateLoadedModulesEx) (
	HANDLE                         hProcess,
	PENUMLOADED_MODULES_CALLBACK64 EnumLoadedModulesCallback,
	PVOID                          UserContext
	);

typedef HANDLE (__stdcall* t_FindDebugInfoFile) (
	PCSTR FileName,
	PCSTR SymbolPath,
	PSTR  DebugFilePath
	);

typedef HANDLE (__stdcall* t_FindDebugInfoFileEx) (
	PCSTR                     FileName,
	PCSTR                     SymbolPath,
	PSTR                      DebugFilePath,
	PFIND_DEBUG_FILE_CALLBACK Callback,
	PVOID                     CallerData
	);

typedef HANDLE (__stdcall* t_FindExecutableImage) (
	PCSTR FileName,
	PCSTR SymbolPath,
	PSTR  ImageFilePath
	);

typedef HANDLE (__stdcall* t_FindExecutableImageEx) (
	PCSTR                   FileName,
	PCSTR                   SymbolPath,
	PSTR                    ImageFilePath,
	PFIND_EXE_FILE_CALLBACK Callback,
	PVOID                   CallerData
	);

typedef BOOL (__stdcall* t_StackWalk64) (
	DWORD                            MachineType,
	HANDLE                           hProcess,
	HANDLE                           hThread,
	LPSTACKFRAME64                   StackFrame,
	PVOID                            ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE64   ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64       GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64     TranslateAddress
	);

typedef BOOL (__stdcall* t_SymSetParentWindow) (
	HWND hwnd
	);

typedef DWORD (__stdcall* t_UnDecorateSymbolName) (
	PCSTR name,
	PSTR  outputString,
	DWORD maxStringLength,
	DWORD flags
	);

typedef DWORD (__stdcall* t_GetTimestampForLoadedLibrary) (
	HMODULE Module
	);

typedef PVOID (__stdcall* t_ImageDirectoryEntryToData) (
	PVOID   Base,
	BOOLEAN MappedAsImage,
	USHORT  DirectoryEntry,
	PULONG  Size
	);

typedef PVOID (__stdcall* t_ImageDirectoryEntryToDataEx) (
	PVOID                 Base,
	BOOLEAN               MappedAsImage,
	USHORT                DirectoryEntry,
	PULONG                Size,
	PIMAGE_SECTION_HEADER* FoundHeader
	);

typedef PIMAGE_NT_HEADERS (__stdcall* t_ImageNtHeader) (
	PVOID Base
	);

typedef PIMAGE_SECTION_HEADER (__stdcall* t_ImageRvaToSection) (
	PIMAGE_NT_HEADERS NtHeaders,
	PVOID             Base,
	ULONG             Rva
	);

typedef PVOID (__stdcall* t_ImageRvaToVa) (
	PIMAGE_NT_HEADERS     NtHeaders,
	PVOID                 Base,
	ULONG                 Rva,
	PIMAGE_SECTION_HEADER* LastRvaSection
	);

typedef BOOL (__stdcall* t_SymAddSourceStream) (
	HANDLE  hProcess,
	ULONG64 Base,
	PCSTR   StreamFile,
	PBYTE   Buffer,
	size_t  Size
	);

typedef BOOL (__stdcall* t_SymAddSymbol) (
	HANDLE  hProcess,
	ULONG64 BaseOfDll,
	PCSTR   Name,
	DWORD64 Address,
	DWORD   Size,
	DWORD   Flags
	);

typedef BOOL (__stdcall* t_SymCleanup) (
	HANDLE hProcess
	);

typedef BOOL (__stdcall* t_SymDeleteSymbol) (
	HANDLE  hProcess,
	ULONG64 BaseOfDll,
	PCSTR   Name,
	DWORD64 Address,
	DWORD   Flags
	);

typedef BOOL (__stdcall* t_SymEnumerateModules64) (
	HANDLE                      hProcess,
	PSYM_ENUMMODULES_CALLBACK64 EnumModulesCallback,
	PVOID                       UserContext
	);

typedef BOOL (__stdcall* t_SymEnumLines) (
	HANDLE                  hProcess,
	ULONG64                 Base,
	PCSTR                   Obj,
	PCSTR                   File,
	PSYM_ENUMLINES_CALLBACK EnumLinesCallback,
	PVOID                   UserContext
	);

typedef BOOL (__stdcall* t_SymEnumProcesses) (
	PSYM_ENUMPROCESSES_CALLBACK EnumProcessesCallback,
	PVOID                       UserContext
	);

typedef BOOL (__stdcall* t_SymEnumSourceFiles) (
	HANDLE                        hProcess,
	ULONG64                       ModBase,
	PCSTR                         Mask,
	PSYM_ENUMSOURCEFILES_CALLBACK cbSrcFiles,
	PVOID                         UserContext
	);

typedef BOOL (__stdcall* t_SymEnumSourceLines) (
	HANDLE                  hProcess,
	ULONG64                 Base,
	PCSTR                   Obj,
	PCSTR                   File,
	DWORD                   Line,
	DWORD                   Flags,
	PSYM_ENUMLINES_CALLBACK EnumLinesCallback,
	PVOID                   UserContext
	);

typedef BOOL (__stdcall* t_SymEnumSymbols) (
	HANDLE                         hProcess,
	ULONG64                        BaseOfDll,
	PCSTR                          Mask,
	PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
	PVOID                          UserContext
	);

typedef BOOL (__stdcall* t_SymEnumSymbolsForAddr) (
	HANDLE                         hProcess,
	DWORD64                        Address,
	PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
	PVOID                          UserContext
	);

typedef BOOL (__stdcall* t_SymEnumTypes) (
	HANDLE                         hProcess,
	ULONG64                        BaseOfDll,
	PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
	PVOID                          UserContext
	);

typedef BOOL (__stdcall* t_SymEnumTypesByName) (
	HANDLE                         hProcess,
	ULONG64                        BaseOfDll,
	PCSTR                          mask,
	PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
	PVOID                          UserContext
	);

typedef HANDLE (__stdcall* t_SymFindDebugInfoFile) (
	HANDLE                    hProcess,
	PCSTR                     FileName,
	PSTR                      DebugFilePath,
	PFIND_DEBUG_FILE_CALLBACK Callback,
	PVOID                     CallerData
	);

typedef HANDLE (__stdcall* t_SymFindExecutableImage) (
	HANDLE                  hProcess,
	PCSTR                   FileName,
	PSTR                    ImageFilePath,
	PFIND_EXE_FILE_CALLBACK Callback,
	PVOID                   CallerData
	);

typedef BOOL (__stdcall* t_SymFindFileInPath) (
	HANDLE                  hprocess,
	PCSTR                   SearchPath,
	PCSTR                   FileName,
	PVOID                   id,
	DWORD                   two,
	DWORD                   three,
	DWORD                   flags,
	PSTR                    FoundFile,
	PFINDFILEINPATHCALLBACK callback,
	PVOID                   context
	);

typedef BOOL (__stdcall* t_SymFromAddr) (
	HANDLE       hProcess,
	DWORD64      Address,
	PDWORD64     Displacement,
	PSYMBOL_INFO Symbol
	);

typedef BOOL (__stdcall* t_SymFromIndex) (
	HANDLE       hProcess,
	ULONG64      BaseOfDll,
	DWORD        Index,
	PSYMBOL_INFO Symbol
	);

typedef BOOL (__stdcall* t_SymFromName) (
	HANDLE       hProcess,
	PCSTR        Name,
	PSYMBOL_INFO Symbol
	);

typedef BOOL (__stdcall* t_SymFromToken) (
	HANDLE       hProcess,
	DWORD64      Base,
	DWORD        Token,
	PSYMBOL_INFO Symbol
	);

typedef PVOID (__stdcall* t_SymFunctionTableAccess64) (
	HANDLE  hProcess,
	DWORD64 AddrBase
	);

typedef ULONG (__stdcall* t_SymGetFileLineOffsets64) (
	HANDLE   hProcess,
	PCSTR    ModuleName,
	PCSTR    FileName,
	PDWORD64 Buffer,
	ULONG    BufferLines
	);

typedef PCHAR (__stdcall* t_SymGetHomeDirectory) (
	DWORD  type,
	PSTR   dir,
	size_t size
	);

typedef BOOL (__stdcall* t_SymGetLineFromAddr64) (
	HANDLE           hProcess,
	DWORD64          qwAddr,
	PDWORD           pdwDisplacement,
	PIMAGEHLP_LINE64 Line64
	);

typedef BOOL (__stdcall* t_SymGetLineFromName64) (
	HANDLE           hProcess,
	PCSTR            ModuleName,
	PCSTR            FileName,
	DWORD            dwLineNumber,
	PLONG            plDisplacement,
	PIMAGEHLP_LINE64 Line
	);

typedef BOOL (__stdcall* t_SymGetLineNext64) (
	HANDLE           hProcess,
	PIMAGEHLP_LINE64 Line
	);

typedef BOOL (__stdcall* t_SymGetLinePrev64) (
	HANDLE           hProcess,
	PIMAGEHLP_LINE64 Line
	);

typedef DWORD64 (__stdcall* t_SymGetModuleBase64) (
	HANDLE  hProcess,
	DWORD64 qwAddr
	);

typedef BOOL (__stdcall* t_SymGetModuleInfo64) (
	HANDLE             hProcess,
	DWORD64            qwAddr,
	PIMAGEHLP_MODULE64 ModuleInfo
	);

typedef BOOL (__stdcall* t_SymGetOmaps) (
	HANDLE   hProcess,
	DWORD64  BaseOfDll,
	POMAP* OmapTo,
	PDWORD64 cOmapTo,
	POMAP* OmapFrom,
	PDWORD64 cOmapFrom
	);

typedef DWORD (__stdcall* t_SymGetOptions) ();

typedef BOOL (__stdcall* t_SymGetScope) (
	HANDLE       hProcess,
	ULONG64      BaseOfDll,
	DWORD        Index,
	PSYMBOL_INFO Symbol
	);

typedef BOOL (__stdcall* t_SymGetSearchPath) (
	HANDLE hProcess,
	PSTR   SearchPath,
	DWORD  SearchPathLength
	);

typedef BOOL (__stdcall* t_SymGetSymbolFile) (
	HANDLE hProcess,
	PCSTR  SymPath,
	PCSTR  ImageFile,
	DWORD  Type,
	PSTR   SymbolFile,
	size_t cSymbolFile,
	PSTR   DbgFile,
	size_t cDbgFile
	);

typedef BOOL (__stdcall* t_SymGetTypeFromName) (
	HANDLE       hProcess,
	ULONG64      BaseOfDll,
	PCSTR        Name,
	PSYMBOL_INFO Symbol
	);

typedef BOOL (__stdcall* t_SymGetTypeInfo) (
	HANDLE                    hProcess,
	DWORD64                   ModBase,
	ULONG                     TypeId,
	IMAGEHLP_SYMBOL_TYPE_INFO GetType,
	PVOID                     pInfo
	);

typedef BOOL (__stdcall* t_SymGetTypeInfoEx) (
	HANDLE                         hProcess,
	DWORD64                        ModBase,
	PIMAGEHLP_GET_TYPE_INFO_PARAMS Params
	);

typedef BOOL (__stdcall* t_SymInitialize) (
	HANDLE hProcess,
	PCSTR  UserSearchPath,
	BOOL   fInvadeProcess
	);

typedef DWORD64 (__stdcall* t_SymLoadModule64) (
	HANDLE  hProcess,
	HANDLE  hFile,
	PCSTR   ImageName,
	PCSTR   ModuleName,
	DWORD64 BaseOfDll,
	DWORD   SizeOfDll
	);

typedef DWORD64 (__stdcall* t_SymLoadModuleEx) (
	HANDLE        hProcess,
	HANDLE        hFile,
	PCSTR         ImageName,
	PCSTR         ModuleName,
	DWORD64       BaseOfDll,
	DWORD         DllSize,
	PMODLOAD_DATA Data,
	DWORD         Flags
	);

typedef BOOL (__stdcall* t_SymMatchFileName) (
	PCSTR FileName,
	PCSTR Match,
	PSTR* FileNameStop,
	PSTR* MatchStop
	);

typedef BOOL (__stdcall* t_SymMatchString) (
	PCSTR string,
	PCSTR expression,
	BOOL  fCase
	);

typedef BOOL (__stdcall* t_SymNext) (
	HANDLE       hProcess,
	PSYMBOL_INFO si
	);

typedef BOOL (__stdcall* t_SymPrev) (
	HANDLE       hProcess,
	PSYMBOL_INFO si
	);

typedef BOOL (__stdcall* t_SymRefreshModuleList) (
	HANDLE hProcess
	);

typedef BOOL (__stdcall* t_SymRegisterCallback64) (
	HANDLE                        hProcess,
	PSYMBOL_REGISTERED_CALLBACK64 CallbackFunction,
	ULONG64                       UserContext
	);

typedef BOOL (__stdcall* t_SymRegisterFunctionEntryCallback64) (
	HANDLE                       hProcess,
	PSYMBOL_FUNCENTRY_CALLBACK64 CallbackFunction,
	ULONG64                      UserContext
	);

typedef BOOL (__stdcall* t_SymSearch) (
	HANDLE                         hProcess,
	ULONG64                        BaseOfDll,
	DWORD                          Index,
	DWORD                          SymTag,
	PCSTR                          Mask,
	DWORD64                        Address,
	PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
	PVOID                          UserContext,
	DWORD                          Options
	);

typedef BOOL (__stdcall* t_SymSetContext) (
	HANDLE                hProcess,
	PIMAGEHLP_STACK_FRAME StackFrame,
	PIMAGEHLP_CONTEXT     Context
	);

typedef PCHAR (__stdcall* t_SymSetHomeDirectory) (
	HANDLE hProcess,
	PCSTR  dir
	);

typedef DWORD (__stdcall* t_SymSetOptions) (
	DWORD SymOptions
	);

typedef BOOL (__stdcall* t_SymSetScopeFromAddr) (
	HANDLE  hProcess,
	ULONG64 Address
	);

typedef BOOL (__stdcall* t_SymSetScopeFromIndex) (
	HANDLE  hProcess,
	ULONG64 BaseOfDll,
	DWORD   Index
	);

typedef BOOL (__stdcall* t_SymSetSearchPath) (
	HANDLE hProcess,
	PCSTR  SearchPath
	);

typedef BOOL (__stdcall* t_SymUnDName64) (
	PIMAGEHLP_SYMBOL64 sym,
	PSTR               UnDecName,
	DWORD              UnDecNameLength
	);

typedef BOOL (__stdcall* t_SymUnloadModule64) (
	HANDLE  hProcess,
	DWORD64 BaseOfDll
	);

typedef PCSTR (__stdcall* t_SymSrvDeltaName) (
	HANDLE hProcess,
	PCSTR  SymPath,
	PCSTR  Type,
	PCSTR  File1,
	PCSTR  File2
	);

typedef BOOL (__stdcall* t_SymSrvGetFileIndexes) (
	PCSTR  File,
	GUID* Id,
	PDWORD Val1,
	PDWORD Val2,
	DWORD  Flags
	);

typedef BOOL (__stdcall* t_SymSrvGetFileIndexInfo) (
	PCSTR              File,
	PSYMSRV_INDEX_INFO Info,
	DWORD              Flags
	);

typedef BOOL (__stdcall* t_SymSrvGetFileIndexString) (
	HANDLE hProcess,
	PCSTR  SrvPath,
	PCSTR  File,
	PSTR   Index,
	size_t Size,
	DWORD  Flags
	);

typedef PCSTR (__stdcall* t_SymSrvGetSupplement) (
	HANDLE hProcess,
	PCSTR  SymPath,
	PCSTR  Node,
	PCSTR  File
	);

typedef BOOL (__stdcall* t_SymSrvIsStore) (
	HANDLE hProcess,
	PCSTR  path
	);

typedef PCSTR (__stdcall* t_SymSrvStoreFile) (
	HANDLE hProcess,
	PCSTR  SrvPath,
	PCSTR  File,
	DWORD  Flags
	);

typedef PCSTR (__stdcall* t_SymSrvStoreSupplement) (
	HANDLE hProcess,
	PCSTR  SrvPath,
	PCSTR  Node,
	PCSTR  File,
	DWORD  Flags
	);

typedef BOOL (__stdcall* t_SymGetSourceFile) (
	HANDLE  hProcess,
	ULONG64 Base,
	PCSTR   Params,
	PCSTR   FileSpec,
	PSTR    FilePath,
	DWORD   Size
	);

typedef BOOL (__stdcall* t_SymEnumSourceFileTokens) (
	HANDLE                        hProcess,
	ULONG64                       Base,
	PENUMSOURCEFILETOKENSCALLBACK Callback
	);

typedef BOOL (__stdcall* t_SymGetSourceFileFromToken) (
	HANDLE hProcess,
	PVOID  Token,
	PCSTR  Params,
	PSTR   FilePath,
	DWORD  Size
	);

typedef BOOL (__stdcall* t_SymGetSourceFileToken) (
	HANDLE  hProcess,
	ULONG64 Base,
	PCSTR   FileSpec,
	PVOID* Token,
	DWORD* Size
	);

typedef BOOL (__stdcall* t_SymGetSourceVarFromToken) (
	HANDLE hProcess,
	PVOID  Token,
	PCSTR  Params,
	PCSTR  VarName,
	PSTR   Value,
	DWORD  Size
	);

typedef BOOL (__stdcall* t_SymInitializeW) (
	HANDLE hProcess,
	PCWSTR UserSearchPath,
	BOOL   fInvadeProcess
	);

typedef BOOL (__stdcall* t_SymFromAddrW) (
	HANDLE        hProcess,
	DWORD64       Address,
	PDWORD64      Displacement,
	PSYMBOL_INFOW Symbol
	);

typedef BOOL (__stdcall* t_SymFromNameW) (
	HANDLE        hProcess,
	PCWSTR        Name,
	PSYMBOL_INFOW Symbol
	);

typedef BOOL (__stdcall* t_SymGetLineFromAddrW64) (
	HANDLE            hProcess,
	DWORD64           dwAddr,
	PDWORD            pdwDisplacement,
	PIMAGEHLP_LINEW64 Line
	);

typedef BOOL (__stdcall* t_SymGetModuleInfoW64) (
	HANDLE              hProcess,
	DWORD64             qwAddr,
	PIMAGEHLP_MODULEW64 ModuleInfo
	);

typedef BOOL (__stdcall* t_SymEnumSymbolsW) (
	HANDLE                          hProcess,
	ULONG64                         BaseOfDll,
	PCWSTR                          Mask,
	PSYM_ENUMERATESYMBOLS_CALLBACKW EnumSymbolsCallback,
	PVOID                           UserContext
	);

typedef BOOL (__stdcall* t_SymEnumLinesW) (
	HANDLE                   hProcess,
	ULONG64                  Base,
	PCWSTR                   Obj,
	PCWSTR                   File,
	PSYM_ENUMLINES_CALLBACKW EnumLinesCallback,
	PVOID                    UserContext
	);

typedef BOOL (__stdcall* t_SymEnumSourceFilesW) (
	HANDLE                         hProcess,
	ULONG64                        ModBase,
	PCWSTR                         Mask,
	PSYM_ENUMSOURCEFILES_CALLBACKW cbSrcFiles,
	PVOID                          UserContext
	);

typedef BOOL (__stdcall* t_SymSetSearchPathW) (
	HANDLE hProcess,
	PCWSTR SearchPath
	);

typedef BOOL (__stdcall* t_SymGetSearchPathW) (
	HANDLE hProcess,
	PWSTR  SearchPath,
	DWORD  SearchPathLength
	);

typedef DWORD64 (__stdcall* t_SymLoadModuleExW) (
	HANDLE        hProcess,
	HANDLE        hFile,
	PCWSTR        ImageName,
	PCWSTR        ModuleName,
	DWORD64       BaseOfDll,
	DWORD         DllSize,
	PMODLOAD_DATA Data,
	DWORD         Flags
	);

typedef BOOL (__stdcall* t_SymEnumTypesW) (
	HANDLE                          hProcess,
	ULONG64                         BaseOfDll,
	PSYM_ENUMERATESYMBOLS_CALLBACKW EnumSymbolsCallback,
	PVOID                           UserContext
	);

typedef BOOL (__stdcall* t_SymEnumTypesByNameW) (
	HANDLE                          hProcess,
	ULONG64                         BaseOfDll,
	PCWSTR                          mask,
	PSYM_ENUMERATESYMBOLS_CALLBACKW EnumSymbolsCallback,
	PVOID                           UserContext
	);

typedef BOOL (__stdcall* t_SymSearchW) (
	HANDLE                          hProcess,
	ULONG64                         BaseOfDll,
	DWORD                           Index,
	DWORD                           SymTag,
	PCWSTR                          Mask,
	DWORD64                         Address,
	PSYM_ENUMERATESYMBOLS_CALLBACKW EnumSymbolsCallback,
	PVOID                           UserContext,
	DWORD                           Options
	);

typedef BOOL (__stdcall* t_SymGetSymbolFileW) (
	HANDLE hProcess,
	PCWSTR SymPath,
	PCWSTR ImageFile,
	DWORD  Type,
	PWSTR  SymbolFile,
	size_t cSymbolFile,
	PWSTR  DbgFile,
	size_t cDbgFile
	);

typedef BOOL (__stdcall* t_SymGetSourceFileW) (
	HANDLE  hProcess,
	ULONG64 Base,
	PCWSTR  Params,
	PCWSTR  FileSpec,
	PWSTR   FilePath,
	DWORD   Size
	);

typedef BOOL (__stdcall* t_SymGetSourceFileFromTokenW) (
	HANDLE hProcess,
	PVOID  Token,
	PCWSTR Params,
	PWSTR  FilePath,
	DWORD  Size
	);

typedef BOOL (__stdcall* t_SymFindFileInPathW) (
	HANDLE                   hprocess,
	PCWSTR                   SearchPath,
	PCWSTR                   FileName,
	PVOID                    id,
	DWORD                    two,
	DWORD                    three,
	DWORD                    flags,
	PWSTR                    FoundFile,
	PFINDFILEINPATHCALLBACKW callback,
	PVOID                    context
	);

typedef HANDLE (__stdcall* t_FindDebugInfoFileExW) (
	PCWSTR                     FileName,
	PCWSTR                     SymbolPath,
	PWSTR                      DebugFilePath,
	PFIND_DEBUG_FILE_CALLBACKW Callback,
	PVOID                      CallerData
	);

typedef HANDLE (__stdcall* t_FindExecutableImageExW) (
	PCWSTR                   FileName,
	PCWSTR                   SymbolPath,
	PWSTR                    ImageFilePath,
	PFIND_EXE_FILE_CALLBACKW Callback,
	PVOID                    CallerData
	);

typedef BOOL (__stdcall* t_EnumDirTreeW) (
	HANDLE                 hProcess,
	PCWSTR                 RootPath,
	PCWSTR                 InputPathName,
	PWSTR                  OutputPathBuffer,
	PENUMDIRTREE_CALLBACKW cb,
	PVOID                  data
	);

typedef BOOL (__stdcall* t_SearchTreeForFileW) (
	PCWSTR RootPath,
	PCWSTR InputPathName,
	PWSTR  OutputPathBuffer
	);

typedef DWORD (__stdcall* t_UnDecorateSymbolNameW) (
	PCWSTR name,
	PWSTR  outputString,
	DWORD  maxStringLength,
	DWORD  flags
	);

typedef BOOL (__stdcall* t_SymAddSymbolW) (
	HANDLE  hProcess,
	ULONG64 BaseOfDll,
	PCWSTR  Name,
	DWORD64 Address,
	DWORD   Size,
	DWORD   Flags
	);

typedef BOOL (__stdcall* t_SymDeleteSymbolW) (
	HANDLE  hProcess,
	ULONG64 BaseOfDll,
	PCWSTR  Name,
	DWORD64 Address,
	DWORD   Flags
	);

typedef BOOL (__stdcall* t_SymFromIndexW) (
	HANDLE        hProcess,
	ULONG64       BaseOfDll,
	DWORD         Index,
	PSYMBOL_INFOW Symbol
	);

typedef BOOL (__stdcall* t_SymGetTypeFromNameW) (
	HANDLE        hProcess,
	ULONG64       BaseOfDll,
	PCWSTR        Name,
	PSYMBOL_INFOW Symbol
	);

typedef BOOL (__stdcall* t_SymMatchFileNameW) (
	PCWSTR FileName,
	PCWSTR Match,
	PWSTR* FileNameStop,
	PWSTR* MatchStop
	);

typedef BOOL (__stdcall* t_StackWalkEx) (
	DWORD                            MachineType,
	HANDLE                           hProcess,
	HANDLE                           hThread,
	LPSTACKFRAME_EX                  StackFrame,
	PVOID                            ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE64   ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64       GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64     TranslateAddress,
	DWORD                            Flags
	);

typedef BOOL (__stdcall* t_StackWalk2) (
	DWORD                            MachineType,
	HANDLE                           hProcess,
	HANDLE                           hThread,
	LPSTACKFRAME_EX                  StackFrame,
	PVOID                            ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE64   ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64       GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64     TranslateAddress,
	PGET_TARGET_ATTRIBUTE_VALUE64    GetTargetAttributeValue,
	DWORD                            Flags
	);

// =============================================================
// Function Pointer Table
struct DebughelpApi {
	HMODULE dll_handle = NULL;

	t_EnumDirTree EnumDirTree = nullptr;
	t_ImagehlpApiVersion ImagehlpApiVersion = nullptr;
	t_ImagehlpApiVersionEx ImagehlpApiVersionEx = nullptr;
	t_MakeSureDirectoryPathExists MakeSureDirectoryPathExists = nullptr;
	t_SearchTreeForFile SearchTreeForFile = nullptr;
	t_EnumerateLoadedModules64 EnumerateLoadedModules64 = nullptr;
	t_EnumerateLoadedModulesEx EnumerateLoadedModulesEx = nullptr;
	t_FindDebugInfoFile FindDebugInfoFile = nullptr;
	t_FindDebugInfoFileEx FindDebugInfoFileEx = nullptr;
	t_FindExecutableImage FindExecutableImage = nullptr;
	t_FindExecutableImageEx FindExecutableImageEx = nullptr;
	t_StackWalk64 StackWalk64 = nullptr;
	t_SymSetParentWindow SymSetParentWindow = nullptr;
	t_UnDecorateSymbolName UnDecorateSymbolName = nullptr;
	t_GetTimestampForLoadedLibrary GetTimestampForLoadedLibrary = nullptr;
	t_ImageDirectoryEntryToData ImageDirectoryEntryToData = nullptr;
	t_ImageDirectoryEntryToDataEx ImageDirectoryEntryToDataEx = nullptr;
	t_ImageNtHeader ImageNtHeader = nullptr;
	t_ImageRvaToSection ImageRvaToSection = nullptr;
	t_ImageRvaToVa ImageRvaToVa = nullptr;
	t_SymAddSourceStream SymAddSourceStream = nullptr;
	t_SymAddSymbol SymAddSymbol = nullptr;
	t_SymCleanup SymCleanup = nullptr;
	t_SymDeleteSymbol SymDeleteSymbol = nullptr;
	t_SymEnumerateModules64 SymEnumerateModules64 = nullptr;
	t_SymEnumLines SymEnumLines = nullptr;
	t_SymEnumProcesses SymEnumProcesses = nullptr;
	t_SymEnumSourceFiles SymEnumSourceFiles = nullptr;
	t_SymEnumSourceLines SymEnumSourceLines = nullptr;
	t_SymEnumSymbols SymEnumSymbols = nullptr;
	t_SymEnumSymbolsForAddr SymEnumSymbolsForAddr = nullptr;
	t_SymEnumTypes SymEnumTypes = nullptr;
	t_SymEnumTypesByName SymEnumTypesByName = nullptr;
	t_SymFindDebugInfoFile SymFindDebugInfoFile = nullptr;
	t_SymFindExecutableImage SymFindExecutableImage = nullptr;
	t_SymFindFileInPath SymFindFileInPath = nullptr;
	t_SymFromAddr SymFromAddr = nullptr;
	t_SymFromIndex SymFromIndex = nullptr;
	t_SymFromName SymFromName = nullptr;
	t_SymFromToken SymFromToken = nullptr;
	t_SymFunctionTableAccess64 SymFunctionTableAccess64 = nullptr;
	t_SymGetFileLineOffsets64 SymGetFileLineOffsets64 = nullptr;
	t_SymGetHomeDirectory SymGetHomeDirectory = nullptr;
	t_SymGetLineFromAddr64 SymGetLineFromAddr64 = nullptr;
	t_SymGetLineFromName64 SymGetLineFromName64 = nullptr;
	t_SymGetLineNext64 SymGetLineNext64 = nullptr;
	t_SymGetLinePrev64 SymGetLinePrev64 = nullptr;
	t_SymGetModuleBase64 SymGetModuleBase64 = nullptr;
	t_SymGetModuleInfo64 SymGetModuleInfo64 = nullptr;
	t_SymGetOmaps SymGetOmaps = nullptr;
	t_SymGetOptions SymGetOptions = nullptr;
	t_SymGetScope SymGetScope = nullptr;
	t_SymGetSearchPath SymGetSearchPath = nullptr;
	t_SymGetSymbolFile SymGetSymbolFile = nullptr;
	t_SymGetTypeFromName SymGetTypeFromName = nullptr;
	t_SymGetTypeInfo SymGetTypeInfo = nullptr;
	t_SymGetTypeInfoEx SymGetTypeInfoEx = nullptr;
	t_SymInitialize SymInitialize = nullptr;
	t_SymLoadModule64 SymLoadModule64 = nullptr;
	t_SymLoadModuleEx SymLoadModuleEx = nullptr;
	t_SymMatchFileName SymMatchFileName = nullptr;
	t_SymMatchString SymMatchString = nullptr;
	t_SymNext SymNext = nullptr;
	t_SymPrev SymPrev = nullptr;
	t_SymRefreshModuleList SymRefreshModuleList = nullptr;
	t_SymRegisterCallback64 SymRegisterCallback64 = nullptr;
	t_SymRegisterFunctionEntryCallback64 SymRegisterFunctionEntryCallback64 = nullptr;
	t_SymSearch SymSearch = nullptr;
	t_SymSetContext SymSetContext = nullptr;
	t_SymSetHomeDirectory SymSetHomeDirectory = nullptr;
	t_SymSetOptions SymSetOptions = nullptr;
	t_SymSetScopeFromAddr SymSetScopeFromAddr = nullptr;
	t_SymSetScopeFromIndex SymSetScopeFromIndex = nullptr;
	t_SymSetSearchPath SymSetSearchPath = nullptr;
	t_SymUnDName64 SymUnDName64 = nullptr;
	t_SymUnloadModule64 SymUnloadModule64 = nullptr;
	t_SymSrvDeltaName SymSrvDeltaName = nullptr;
	t_SymSrvGetFileIndexes SymSrvGetFileIndexes = nullptr;
	t_SymSrvGetFileIndexInfo SymSrvGetFileIndexInfo = nullptr;
	t_SymSrvGetFileIndexString SymSrvGetFileIndexString = nullptr;
	t_SymSrvGetSupplement SymSrvGetSupplement = nullptr;
	t_SymSrvIsStore SymSrvIsStore = nullptr;
	t_SymSrvStoreFile SymSrvStoreFile = nullptr;
	t_SymSrvStoreSupplement SymSrvStoreSupplement = nullptr;
	t_SymGetSourceFile SymGetSourceFile = nullptr;
	t_SymEnumSourceFileTokens SymEnumSourceFileTokens = nullptr;
	t_SymGetSourceFileFromToken SymGetSourceFileFromToken = nullptr;
	t_SymGetSourceFileToken SymGetSourceFileToken = nullptr;
	t_SymGetSourceVarFromToken SymGetSourceVarFromToken = nullptr;
	t_SymInitializeW SymInitializeW = nullptr;
	t_SymFromAddrW SymFromAddrW = nullptr;
	t_SymFromNameW SymFromNameW = nullptr;
	t_SymGetLineFromAddrW64 SymGetLineFromAddrW64 = nullptr;
	t_SymGetModuleInfoW64 SymGetModuleInfoW64 = nullptr;
	t_SymEnumSymbolsW SymEnumSymbolsW = nullptr;
	t_SymEnumLinesW SymEnumLinesW = nullptr;
	t_SymEnumSourceFilesW SymEnumSourceFilesW = nullptr;
	t_SymSetSearchPathW SymSetSearchPathW = nullptr;
	t_SymGetSearchPathW SymGetSearchPathW = nullptr;
	t_SymLoadModuleExW SymLoadModuleExW = nullptr;
	t_SymEnumTypesW SymEnumTypesW = nullptr;
	t_SymEnumTypesByNameW SymEnumTypesByNameW = nullptr;
	t_SymSearchW SymSearchW = nullptr;
	t_SymGetSymbolFileW SymGetSymbolFileW = nullptr;
	t_SymGetSourceFileW SymGetSourceFileW = nullptr;
	t_SymGetSourceFileFromTokenW SymGetSourceFileFromTokenW = nullptr;
	t_SymFindFileInPathW SymFindFileInPathW = nullptr;
	t_FindDebugInfoFileExW FindDebugInfoFileExW = nullptr;
	t_FindExecutableImageExW FindExecutableImageExW = nullptr;
	t_EnumDirTreeW EnumDirTreeW = nullptr;
	t_SearchTreeForFileW SearchTreeForFileW = nullptr;
	t_UnDecorateSymbolNameW UnDecorateSymbolNameW = nullptr;
	t_SymAddSymbolW SymAddSymbolW = nullptr;
	t_SymDeleteSymbolW SymDeleteSymbolW = nullptr;
	t_SymFromIndexW SymFromIndexW = nullptr;
	t_SymGetTypeFromNameW SymGetTypeFromNameW = nullptr;
	t_SymMatchFileNameW SymMatchFileNameW = nullptr;
	t_StackWalkEx StackWalkEx = nullptr;
	t_StackWalk2 StackWalk2 = nullptr;

	void unload () {
		if (dll_handle != NULL) {
			FreeLibrary(dll_handle);
		}
	}
	bool load () {
		// system32
		wchar_t system_path[MAX_PATH];
		auto res = GetSystemDirectoryW(system_path, MAX_PATH);
		if (res <= 0 || res > MAX_PATH)
			return false;
		auto dbghelp_path = std::filesystem::path(system_path) / L"dbghelp.dll";

		auto dll = LoadLibraryW(dbghelp_path.c_str());
		if (dll == NULL)
			return false;

		dll_handle = dll;

		EnumDirTree = (t_EnumDirTree)GetProcAddress(dll, "EnumDirTree");
		ImagehlpApiVersion = (t_ImagehlpApiVersion)GetProcAddress(dll, "ImagehlpApiVersion");
		ImagehlpApiVersionEx = (t_ImagehlpApiVersionEx)GetProcAddress(dll, "ImagehlpApiVersionEx");
		MakeSureDirectoryPathExists = (t_MakeSureDirectoryPathExists)GetProcAddress(dll, "MakeSureDirectoryPathExists");
		SearchTreeForFile = (t_SearchTreeForFile)GetProcAddress(dll, "SearchTreeForFile");
		EnumerateLoadedModules64 = (t_EnumerateLoadedModules64)GetProcAddress(dll, "EnumerateLoadedModules64");
		EnumerateLoadedModulesEx = (t_EnumerateLoadedModulesEx)GetProcAddress(dll, "EnumerateLoadedModulesEx");
		FindDebugInfoFile = (t_FindDebugInfoFile)GetProcAddress(dll, "FindDebugInfoFile");
		FindDebugInfoFileEx = (t_FindDebugInfoFileEx)GetProcAddress(dll, "FindDebugInfoFileEx");
		FindExecutableImage = (t_FindExecutableImage)GetProcAddress(dll, "FindExecutableImage");
		FindExecutableImageEx = (t_FindExecutableImageEx)GetProcAddress(dll, "FindExecutableImageEx");
		StackWalk64 = (t_StackWalk64)GetProcAddress(dll, "StackWalk64");
		SymSetParentWindow = (t_SymSetParentWindow)GetProcAddress(dll, "SymSetParentWindow");
		UnDecorateSymbolName = (t_UnDecorateSymbolName)GetProcAddress(dll, "UnDecorateSymbolName");
		GetTimestampForLoadedLibrary = (t_GetTimestampForLoadedLibrary)GetProcAddress(dll, "GetTimestampForLoadedLibrary");
		ImageDirectoryEntryToData = (t_ImageDirectoryEntryToData)GetProcAddress(dll, "ImageDirectoryEntryToData");
		ImageDirectoryEntryToDataEx = (t_ImageDirectoryEntryToDataEx)GetProcAddress(dll, "ImageDirectoryEntryToDataEx");
		ImageNtHeader = (t_ImageNtHeader)GetProcAddress(dll, "ImageNtHeader");
		ImageRvaToSection = (t_ImageRvaToSection)GetProcAddress(dll, "ImageRvaToSection");
		ImageRvaToVa = (t_ImageRvaToVa)GetProcAddress(dll, "ImageRvaToVa");
		SymAddSourceStream = (t_SymAddSourceStream)GetProcAddress(dll, "SymAddSourceStream");
		SymAddSymbol = (t_SymAddSymbol)GetProcAddress(dll, "SymAddSymbol");
		SymCleanup = (t_SymCleanup)GetProcAddress(dll, "SymCleanup");
		SymDeleteSymbol = (t_SymDeleteSymbol)GetProcAddress(dll, "SymDeleteSymbol");
		SymEnumerateModules64 = (t_SymEnumerateModules64)GetProcAddress(dll, "SymEnumerateModules64");
		SymEnumLines = (t_SymEnumLines)GetProcAddress(dll, "SymEnumLines");
		SymEnumProcesses = (t_SymEnumProcesses)GetProcAddress(dll, "SymEnumProcesses");
		SymEnumSourceFiles = (t_SymEnumSourceFiles)GetProcAddress(dll, "SymEnumSourceFiles");
		SymEnumSourceLines = (t_SymEnumSourceLines)GetProcAddress(dll, "SymEnumSourceLines");
		SymEnumSymbols = (t_SymEnumSymbols)GetProcAddress(dll, "SymEnumSymbols");
		SymEnumSymbolsForAddr = (t_SymEnumSymbolsForAddr)GetProcAddress(dll, "SymEnumSymbolsForAddr");
		SymEnumTypes = (t_SymEnumTypes)GetProcAddress(dll, "SymEnumTypes");
		SymEnumTypesByName = (t_SymEnumTypesByName)GetProcAddress(dll, "SymEnumTypesByName");
		SymFindDebugInfoFile = (t_SymFindDebugInfoFile)GetProcAddress(dll, "SymFindDebugInfoFile");
		SymFindExecutableImage = (t_SymFindExecutableImage)GetProcAddress(dll, "SymFindExecutableImage");
		SymFindFileInPath = (t_SymFindFileInPath)GetProcAddress(dll, "SymFindFileInPath");
		SymFromAddr = (t_SymFromAddr)GetProcAddress(dll, "SymFromAddr");
		SymFromIndex = (t_SymFromIndex)GetProcAddress(dll, "SymFromIndex");
		SymFromName = (t_SymFromName)GetProcAddress(dll, "SymFromName");
		SymFromToken = (t_SymFromToken)GetProcAddress(dll, "SymFromToken");
		SymFunctionTableAccess64 = (t_SymFunctionTableAccess64)GetProcAddress(dll, "SymFunctionTableAccess64");
		SymGetFileLineOffsets64 = (t_SymGetFileLineOffsets64)GetProcAddress(dll, "SymGetFileLineOffsets64");
		SymGetHomeDirectory = (t_SymGetHomeDirectory)GetProcAddress(dll, "SymGetHomeDirectory");
		SymGetLineFromAddr64 = (t_SymGetLineFromAddr64)GetProcAddress(dll, "SymGetLineFromAddr64");
		SymGetLineFromName64 = (t_SymGetLineFromName64)GetProcAddress(dll, "SymGetLineFromName64");
		SymGetLineNext64 = (t_SymGetLineNext64)GetProcAddress(dll, "SymGetLineNext64");
		SymGetLinePrev64 = (t_SymGetLinePrev64)GetProcAddress(dll, "SymGetLinePrev64");
		SymGetModuleBase64 = (t_SymGetModuleBase64)GetProcAddress(dll, "SymGetModuleBase64");
		SymGetModuleInfo64 = (t_SymGetModuleInfo64)GetProcAddress(dll, "SymGetModuleInfo64");
		SymGetOmaps = (t_SymGetOmaps)GetProcAddress(dll, "SymGetOmaps");
		SymGetOptions = (t_SymGetOptions)GetProcAddress(dll, "SymGetOptions");
		SymGetScope = (t_SymGetScope)GetProcAddress(dll, "SymGetScope");
		SymGetSearchPath = (t_SymGetSearchPath)GetProcAddress(dll, "SymGetSearchPath");
		SymGetSymbolFile = (t_SymGetSymbolFile)GetProcAddress(dll, "SymGetSymbolFile");
		SymGetTypeFromName = (t_SymGetTypeFromName)GetProcAddress(dll, "SymGetTypeFromName");
		SymGetTypeInfo = (t_SymGetTypeInfo)GetProcAddress(dll, "SymGetTypeInfo");
		SymGetTypeInfoEx = (t_SymGetTypeInfoEx)GetProcAddress(dll, "SymGetTypeInfoEx");
		SymInitialize = (t_SymInitialize)GetProcAddress(dll, "SymInitialize");
		SymLoadModule64 = (t_SymLoadModule64)GetProcAddress(dll, "SymLoadModule64");
		SymLoadModuleEx = (t_SymLoadModuleEx)GetProcAddress(dll, "SymLoadModuleEx");
		SymMatchFileName = (t_SymMatchFileName)GetProcAddress(dll, "SymMatchFileName");
		SymMatchString = (t_SymMatchString)GetProcAddress(dll, "SymMatchString");
		SymNext = (t_SymNext)GetProcAddress(dll, "SymNext");
		SymPrev = (t_SymPrev)GetProcAddress(dll, "SymPrev");
		SymRefreshModuleList = (t_SymRefreshModuleList)GetProcAddress(dll, "SymRefreshModuleList");
		SymRegisterCallback64 = (t_SymRegisterCallback64)GetProcAddress(dll, "SymRegisterCallback64");
		SymRegisterFunctionEntryCallback64 = (t_SymRegisterFunctionEntryCallback64)GetProcAddress(dll, "SymRegisterFunctionEntryCallback64");
		SymSearch = (t_SymSearch)GetProcAddress(dll, "SymSearch");
		SymSetContext = (t_SymSetContext)GetProcAddress(dll, "SymSetContext");
		SymSetHomeDirectory = (t_SymSetHomeDirectory)GetProcAddress(dll, "SymSetHomeDirectory");
		SymSetOptions = (t_SymSetOptions)GetProcAddress(dll, "SymSetOptions");
		SymSetScopeFromAddr = (t_SymSetScopeFromAddr)GetProcAddress(dll, "SymSetScopeFromAddr");
		SymSetScopeFromIndex = (t_SymSetScopeFromIndex)GetProcAddress(dll, "SymSetScopeFromIndex");
		SymSetSearchPath = (t_SymSetSearchPath)GetProcAddress(dll, "SymSetSearchPath");
		SymUnDName64 = (t_SymUnDName64)GetProcAddress(dll, "SymUnDName64");
		SymUnloadModule64 = (t_SymUnloadModule64)GetProcAddress(dll, "SymUnloadModule64");
		SymSrvDeltaName = (t_SymSrvDeltaName)GetProcAddress(dll, "SymSrvDeltaName");
		SymSrvGetFileIndexes = (t_SymSrvGetFileIndexes)GetProcAddress(dll, "SymSrvGetFileIndexes");
		SymSrvGetFileIndexInfo = (t_SymSrvGetFileIndexInfo)GetProcAddress(dll, "SymSrvGetFileIndexInfo");
		SymSrvGetFileIndexString = (t_SymSrvGetFileIndexString)GetProcAddress(dll, "SymSrvGetFileIndexString");
		SymSrvGetSupplement = (t_SymSrvGetSupplement)GetProcAddress(dll, "SymSrvGetSupplement");
		SymSrvIsStore = (t_SymSrvIsStore)GetProcAddress(dll, "SymSrvIsStore");
		SymSrvStoreFile = (t_SymSrvStoreFile)GetProcAddress(dll, "SymSrvStoreFile");
		SymSrvStoreSupplement = (t_SymSrvStoreSupplement)GetProcAddress(dll, "SymSrvStoreSupplement");
		SymGetSourceFile = (t_SymGetSourceFile)GetProcAddress(dll, "SymGetSourceFile");
		SymEnumSourceFileTokens = (t_SymEnumSourceFileTokens)GetProcAddress(dll, "SymEnumSourceFileTokens");
		SymGetSourceFileFromToken = (t_SymGetSourceFileFromToken)GetProcAddress(dll, "SymGetSourceFileFromToken");
		SymGetSourceFileToken = (t_SymGetSourceFileToken)GetProcAddress(dll, "SymGetSourceFileToken");
		SymGetSourceVarFromToken = (t_SymGetSourceVarFromToken)GetProcAddress(dll, "SymGetSourceVarFromToken");
		SymInitializeW = (t_SymInitializeW)GetProcAddress(dll, "SymInitializeW");
		SymFromAddrW = (t_SymFromAddrW)GetProcAddress(dll, "SymFromAddrW");
		SymFromNameW = (t_SymFromNameW)GetProcAddress(dll, "SymFromNameW");
		SymGetLineFromAddrW64 = (t_SymGetLineFromAddrW64)GetProcAddress(dll, "SymGetLineFromAddrW64");
		SymGetModuleInfoW64 = (t_SymGetModuleInfoW64)GetProcAddress(dll, "SymGetModuleInfoW64");
		SymEnumSymbolsW = (t_SymEnumSymbolsW)GetProcAddress(dll, "SymEnumSymbolsW");
		SymEnumLinesW = (t_SymEnumLinesW)GetProcAddress(dll, "SymEnumLinesW");
		SymEnumSourceFilesW = (t_SymEnumSourceFilesW)GetProcAddress(dll, "SymEnumSourceFilesW");
		SymSetSearchPathW = (t_SymSetSearchPathW)GetProcAddress(dll, "SymSetSearchPathW");
		SymGetSearchPathW = (t_SymGetSearchPathW)GetProcAddress(dll, "SymGetSearchPathW");
		SymLoadModuleExW = (t_SymLoadModuleExW)GetProcAddress(dll, "SymLoadModuleExW");
		SymEnumTypesW = (t_SymEnumTypesW)GetProcAddress(dll, "SymEnumTypesW");
		SymEnumTypesByNameW = (t_SymEnumTypesByNameW)GetProcAddress(dll, "SymEnumTypesByNameW");
		SymSearchW = (t_SymSearchW)GetProcAddress(dll, "SymSearchW");
		SymGetSymbolFileW = (t_SymGetSymbolFileW)GetProcAddress(dll, "SymGetSymbolFileW");
		SymGetSourceFileW = (t_SymGetSourceFileW)GetProcAddress(dll, "SymGetSourceFileW");
		SymGetSourceFileFromTokenW = (t_SymGetSourceFileFromTokenW)GetProcAddress(dll, "SymGetSourceFileFromTokenW");
		SymFindFileInPathW = (t_SymFindFileInPathW)GetProcAddress(dll, "SymFindFileInPathW");
		FindDebugInfoFileExW = (t_FindDebugInfoFileExW)GetProcAddress(dll, "FindDebugInfoFileExW");
		FindExecutableImageExW = (t_FindExecutableImageExW)GetProcAddress(dll, "FindExecutableImageExW");
		EnumDirTreeW = (t_EnumDirTreeW)GetProcAddress(dll, "EnumDirTreeW");
		SearchTreeForFileW = (t_SearchTreeForFileW)GetProcAddress(dll, "SearchTreeForFileW");
		UnDecorateSymbolNameW = (t_UnDecorateSymbolNameW)GetProcAddress(dll, "UnDecorateSymbolNameW");
		SymAddSymbolW = (t_SymAddSymbolW)GetProcAddress(dll, "SymAddSymbolW");
		SymDeleteSymbolW = (t_SymDeleteSymbolW)GetProcAddress(dll, "SymDeleteSymbolW");
		SymFromIndexW = (t_SymFromIndexW)GetProcAddress(dll, "SymFromIndexW");
		SymGetTypeFromNameW = (t_SymGetTypeFromNameW)GetProcAddress(dll, "SymGetTypeFromNameW");
		SymMatchFileNameW = (t_SymMatchFileNameW)GetProcAddress(dll, "SymMatchFileNameW");
		StackWalkEx = (t_StackWalkEx)GetProcAddress(dll, "StackWalkEx");
		StackWalk2 = (t_StackWalk2)GetProcAddress(dll, "StackWalk2");

		return true;
	}
};
inline DebughelpApi real_debughelp;

// =============================================================
// Dllexport Api

extern "C" {
	__declspec(dllexport) BOOL __stdcall hook_EnumDirTree (
		HANDLE                hProcess,
		PCSTR                 RootPath,
		PCSTR                 InputPathName,
		PSTR                  OutputPathBuffer,
		PENUMDIRTREE_CALLBACK cb,
		PVOID                 data
	) {
		return real_debughelp.EnumDirTree(hProcess, RootPath, InputPathName, OutputPathBuffer, cb, data);
	}
	#pragma comment(linker, "/EXPORT:EnumDirTree=hook_EnumDirTree")

	__declspec(dllexport) LPAPI_VERSION __stdcall hook_ImagehlpApiVersion () {
		return real_debughelp.ImagehlpApiVersion();
	}

	__declspec(dllexport) LPAPI_VERSION __stdcall hook_ImagehlpApiVersionEx (
		LPAPI_VERSION AppVersion
	) {
		return real_debughelp.ImagehlpApiVersionEx(AppVersion);
	}

	__declspec(dllexport) BOOL __stdcall hook_MakeSureDirectoryPathExists (
		PCSTR DirPath
	) {
		return real_debughelp.MakeSureDirectoryPathExists(DirPath);
	}

	__declspec(dllexport) BOOL __stdcall hook_SearchTreeForFile (
		PCSTR RootPath,
		PCSTR InputPathName,
		PSTR  OutputPathBuffer
	) {
		return real_debughelp.SearchTreeForFile(RootPath, InputPathName, OutputPathBuffer);
	}

	__declspec(dllexport) BOOL __stdcall hook_EnumerateLoadedModules64 (
		HANDLE                         hProcess,
		PENUMLOADED_MODULES_CALLBACK64 EnumLoadedModulesCallback,
		PVOID                          UserContext
	) {
		return real_debughelp.EnumerateLoadedModules64(hProcess, EnumLoadedModulesCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_EnumerateLoadedModulesEx (
		HANDLE                         hProcess,
		PENUMLOADED_MODULES_CALLBACK64 EnumLoadedModulesCallback,
		PVOID                          UserContext
	) {
		return real_debughelp.EnumerateLoadedModulesEx(hProcess, EnumLoadedModulesCallback, UserContext);
	}

	__declspec(dllexport) HANDLE __stdcall hook_FindDebugInfoFile (
		PCSTR FileName,
		PCSTR SymbolPath,
		PSTR  DebugFilePath
	) {
		return real_debughelp.FindDebugInfoFile(FileName, SymbolPath, DebugFilePath);
	}

	__declspec(dllexport) HANDLE __stdcall hook_FindDebugInfoFileEx (
		PCSTR                     FileName,
		PCSTR                     SymbolPath,
		PSTR                      DebugFilePath,
		PFIND_DEBUG_FILE_CALLBACK Callback,
		PVOID                     CallerData
	) {
		return real_debughelp.FindDebugInfoFileEx(FileName, SymbolPath, DebugFilePath, Callback, CallerData);
	}

	__declspec(dllexport) HANDLE __stdcall hook_FindExecutableImage (
		PCSTR FileName,
		PCSTR SymbolPath,
		PSTR  ImageFilePath
	) {
		return real_debughelp.FindExecutableImage(FileName, SymbolPath, ImageFilePath);
	}

	__declspec(dllexport) HANDLE __stdcall hook_FindExecutableImageEx (
		PCSTR                   FileName,
		PCSTR                   SymbolPath,
		PSTR                    ImageFilePath,
		PFIND_EXE_FILE_CALLBACK Callback,
		PVOID                   CallerData
	) {
		return real_debughelp.FindExecutableImageEx(FileName, SymbolPath, ImageFilePath, Callback, CallerData);
	}

	__declspec(dllexport) BOOL __stdcall hook_StackWalk64 (
		DWORD                            MachineType,
		HANDLE                           hProcess,
		HANDLE                           hThread,
		LPSTACKFRAME64                   StackFrame,
		PVOID                            ContextRecord,
		PREAD_PROCESS_MEMORY_ROUTINE64   ReadMemoryRoutine,
		PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
		PGET_MODULE_BASE_ROUTINE64       GetModuleBaseRoutine,
		PTRANSLATE_ADDRESS_ROUTINE64     TranslateAddress
	) {
		return real_debughelp.StackWalk64(MachineType, hProcess, hThread, StackFrame, ContextRecord, ReadMemoryRoutine, FunctionTableAccessRoutine, GetModuleBaseRoutine, TranslateAddress);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSetParentWindow (
		HWND hwnd
	) {
		return real_debughelp.SymSetParentWindow(hwnd);
	}

	__declspec(dllexport) DWORD __stdcall hook_UnDecorateSymbolName (
		PCSTR name,
		PSTR  outputString,
		DWORD maxStringLength,
		DWORD flags
	) {
		return real_debughelp.UnDecorateSymbolName(name, outputString, maxStringLength, flags);
	}

	__declspec(dllexport) DWORD __stdcall hook_GetTimestampForLoadedLibrary (
		HMODULE Module
	) {
		return real_debughelp.GetTimestampForLoadedLibrary(Module);
	}

	__declspec(dllexport) PVOID __stdcall hook_ImageDirectoryEntryToData (
		PVOID   Base,
		BOOLEAN MappedAsImage,
		USHORT  DirectoryEntry,
		PULONG  Size
	) {
		return real_debughelp.ImageDirectoryEntryToData(Base, MappedAsImage, DirectoryEntry, Size);
	}

	__declspec(dllexport) PVOID __stdcall hook_ImageDirectoryEntryToDataEx (
		PVOID                 Base,
		BOOLEAN               MappedAsImage,
		USHORT                DirectoryEntry,
		PULONG                Size,
		PIMAGE_SECTION_HEADER* FoundHeader
	) {
		return real_debughelp.ImageDirectoryEntryToDataEx(Base, MappedAsImage, DirectoryEntry, Size, FoundHeader);
	}

	__declspec(dllexport) PIMAGE_NT_HEADERS __stdcall hook_ImageNtHeader (
		PVOID Base
	) {
		return real_debughelp.ImageNtHeader(Base);
	}

	__declspec(dllexport) PIMAGE_SECTION_HEADER __stdcall hook_ImageRvaToSection (
		PIMAGE_NT_HEADERS NtHeaders,
		PVOID             Base,
		ULONG             Rva
	) {
		return real_debughelp.ImageRvaToSection(NtHeaders, Base, Rva);
	}

	__declspec(dllexport) PVOID __stdcall hook_ImageRvaToVa (
		PIMAGE_NT_HEADERS     NtHeaders,
		PVOID                 Base,
		ULONG                 Rva,
		PIMAGE_SECTION_HEADER* LastRvaSection
	) {
		return real_debughelp.ImageRvaToVa(NtHeaders, Base, Rva, LastRvaSection);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymAddSourceStream (
		HANDLE  hProcess,
		ULONG64 Base,
		PCSTR   StreamFile,
		PBYTE   Buffer,
		size_t  Size
	) {
		return real_debughelp.SymAddSourceStream(hProcess, Base, StreamFile, Buffer, Size);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymAddSymbol (
		HANDLE  hProcess,
		ULONG64 BaseOfDll,
		PCSTR   Name,
		DWORD64 Address,
		DWORD   Size,
		DWORD   Flags
	) {
		return real_debughelp.SymAddSymbol(hProcess, BaseOfDll, Name, Address, Size, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymCleanup (
		HANDLE hProcess
	) {
		return real_debughelp.SymCleanup(hProcess);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymDeleteSymbol (
		HANDLE  hProcess,
		ULONG64 BaseOfDll,
		PCSTR   Name,
		DWORD64 Address,
		DWORD   Flags
	) {
		return real_debughelp.SymDeleteSymbol(hProcess, BaseOfDll, Name, Address, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumerateModules64 (
		HANDLE                      hProcess,
		PSYM_ENUMMODULES_CALLBACK64 EnumModulesCallback,
		PVOID                       UserContext
	) {
		return real_debughelp.SymEnumerateModules64(hProcess, EnumModulesCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumLines (
		HANDLE                  hProcess,
		ULONG64                 Base,
		PCSTR                   Obj,
		PCSTR                   File,
		PSYM_ENUMLINES_CALLBACK EnumLinesCallback,
		PVOID                   UserContext
	) {
		return real_debughelp.SymEnumLines(hProcess, Base, Obj, File, EnumLinesCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumProcesses (
		PSYM_ENUMPROCESSES_CALLBACK EnumProcessesCallback,
		PVOID                       UserContext
	) {
		return real_debughelp.SymEnumProcesses(EnumProcessesCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumSourceFiles (
		HANDLE                        hProcess,
		ULONG64                       ModBase,
		PCSTR                         Mask,
		PSYM_ENUMSOURCEFILES_CALLBACK cbSrcFiles,
		PVOID                         UserContext
	) {
		return real_debughelp.SymEnumSourceFiles(hProcess, ModBase, Mask, cbSrcFiles, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumSourceLines (
		HANDLE                  hProcess,
		ULONG64                 Base,
		PCSTR                   Obj,
		PCSTR                   File,
		DWORD                   Line,
		DWORD                   Flags,
		PSYM_ENUMLINES_CALLBACK EnumLinesCallback,
		PVOID                   UserContext
	) {
		return real_debughelp.SymEnumSourceLines(hProcess, Base, Obj, File, Line, Flags, EnumLinesCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumSymbols (
		HANDLE                         hProcess,
		ULONG64                        BaseOfDll,
		PCSTR                          Mask,
		PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
		PVOID                          UserContext
	) {
		return real_debughelp.SymEnumSymbols(hProcess, BaseOfDll, Mask, EnumSymbolsCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumSymbolsForAddr (
		HANDLE                         hProcess,
		DWORD64                        Address,
		PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
		PVOID                          UserContext
	) {
		return real_debughelp.SymEnumSymbolsForAddr(hProcess, Address, EnumSymbolsCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumTypes (
		HANDLE                         hProcess,
		ULONG64                        BaseOfDll,
		PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
		PVOID                          UserContext
	) {
		return real_debughelp.SymEnumTypes(hProcess, BaseOfDll, EnumSymbolsCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumTypesByName (
		HANDLE                         hProcess,
		ULONG64                        BaseOfDll,
		PCSTR                          mask,
		PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
		PVOID                          UserContext
	) {
		return real_debughelp.SymEnumTypesByName(hProcess, BaseOfDll, mask, EnumSymbolsCallback, UserContext);
	}

	__declspec(dllexport) HANDLE __stdcall hook_SymFindDebugInfoFile (
		HANDLE                    hProcess,
		PCSTR                     FileName,
		PSTR                      DebugFilePath,
		PFIND_DEBUG_FILE_CALLBACK Callback,
		PVOID                     CallerData
	) {
		return real_debughelp.SymFindDebugInfoFile(hProcess, FileName, DebugFilePath, Callback, CallerData);
	}

	__declspec(dllexport) HANDLE __stdcall hook_SymFindExecutableImage (
		HANDLE                  hProcess,
		PCSTR                   FileName,
		PSTR                    ImageFilePath,
		PFIND_EXE_FILE_CALLBACK Callback,
		PVOID                   CallerData
	) {
		return real_debughelp.SymFindExecutableImage(hProcess, FileName, ImageFilePath, Callback, CallerData);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymFindFileInPath (
		HANDLE                  hprocess,
		PCSTR                   SearchPath,
		PCSTR                   FileName,
		PVOID                   id,
		DWORD                   two,
		DWORD                   three,
		DWORD                   flags,
		PSTR                    FoundFile,
		PFINDFILEINPATHCALLBACK callback,
		PVOID                   context
	) {
		return real_debughelp.SymFindFileInPath(hprocess, SearchPath, FileName, id, two, three, flags, FoundFile, callback, context);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymFromAddr (
		HANDLE       hProcess,
		DWORD64      Address,
		PDWORD64     Displacement,
		PSYMBOL_INFO Symbol
	) {
		printf("SymFromAddr (%8llx)\n", Address);

		return real_debughelp.SymFromAddr(hProcess, Address, Displacement, Symbol);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymFromIndex (
		HANDLE       hProcess,
		ULONG64      BaseOfDll,
		DWORD        Index,
		PSYMBOL_INFO Symbol
	) {
		return real_debughelp.SymFromIndex(hProcess, BaseOfDll, Index, Symbol);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymFromName (
		HANDLE       hProcess,
		PCSTR        Name,
		PSYMBOL_INFO Symbol
	) {
		return real_debughelp.SymFromName(hProcess, Name, Symbol);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymFromToken (
		HANDLE       hProcess,
		DWORD64      Base,
		DWORD        Token,
		PSYMBOL_INFO Symbol
	) {
		return real_debughelp.SymFromToken(hProcess, Base, Token, Symbol);
	}

	__declspec(dllexport) PVOID __stdcall hook_SymFunctionTableAccess64 (
		HANDLE  hProcess,
		DWORD64 AddrBase
	) {
		return real_debughelp.SymFunctionTableAccess64(hProcess, AddrBase);
	}

	__declspec(dllexport) ULONG __stdcall hook_SymGetFileLineOffsets64 (
		HANDLE   hProcess,
		PCSTR    ModuleName,
		PCSTR    FileName,
		PDWORD64 Buffer,
		ULONG    BufferLines
	) {
		return real_debughelp.SymGetFileLineOffsets64(hProcess, ModuleName, FileName, Buffer, BufferLines);
	}

	__declspec(dllexport) PCHAR __stdcall hook_SymGetHomeDirectory (
		DWORD  type,
		PSTR   dir,
		size_t size
	) {
		return real_debughelp.SymGetHomeDirectory(type, dir, size);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetLineFromAddr64 (
		HANDLE           hProcess,
		DWORD64          qwAddr,
		PDWORD           pdwDisplacement,
		PIMAGEHLP_LINE64 Line64
	) {
		return real_debughelp.SymGetLineFromAddr64(hProcess, qwAddr, pdwDisplacement, Line64);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetLineFromName64 (
		HANDLE           hProcess,
		PCSTR            ModuleName,
		PCSTR            FileName,
		DWORD            dwLineNumber,
		PLONG            plDisplacement,
		PIMAGEHLP_LINE64 Line
	) {
		return real_debughelp.SymGetLineFromName64(hProcess, ModuleName, FileName, dwLineNumber, plDisplacement, Line);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetLineNext64 (
		HANDLE           hProcess,
		PIMAGEHLP_LINE64 Line
	) {
		return real_debughelp.SymGetLineNext64(hProcess, Line);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetLinePrev64 (
		HANDLE           hProcess,
		PIMAGEHLP_LINE64 Line
	) {
		return real_debughelp.SymGetLinePrev64(hProcess, Line);
	}

	__declspec(dllexport) DWORD64 __stdcall hook_SymGetModuleBase64 (
		HANDLE  hProcess,
		DWORD64 qwAddr
	) {
		return real_debughelp.SymGetModuleBase64(hProcess, qwAddr);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetModuleInfo64 (
		HANDLE             hProcess,
		DWORD64            qwAddr,
		PIMAGEHLP_MODULE64 ModuleInfo
	) {
		return real_debughelp.SymGetModuleInfo64(hProcess, qwAddr, ModuleInfo);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetOmaps (
		HANDLE   hProcess,
		DWORD64  BaseOfDll,
		POMAP* OmapTo,
		PDWORD64 cOmapTo,
		POMAP* OmapFrom,
		PDWORD64 cOmapFrom
	) {
		return real_debughelp.SymGetOmaps(hProcess, BaseOfDll, OmapTo, cOmapTo, OmapFrom, cOmapFrom);
	}

	__declspec(dllexport) DWORD __stdcall hook_SymGetOptions () {
		return real_debughelp.SymGetOptions();
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetScope (
		HANDLE       hProcess,
		ULONG64      BaseOfDll,
		DWORD        Index,
		PSYMBOL_INFO Symbol
	) {
		return real_debughelp.SymGetScope(hProcess, BaseOfDll, Index, Symbol);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSearchPath (
		HANDLE hProcess,
		PSTR   SearchPath,
		DWORD  SearchPathLength
	) {
		return real_debughelp.SymGetSearchPath(hProcess, SearchPath, SearchPathLength);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSymbolFile (
		HANDLE hProcess,
		PCSTR  SymPath,
		PCSTR  ImageFile,
		DWORD  Type,
		PSTR   SymbolFile,
		size_t cSymbolFile,
		PSTR   DbgFile,
		size_t cDbgFile
	) {
		return real_debughelp.SymGetSymbolFile(hProcess, SymPath, ImageFile, Type, SymbolFile, cSymbolFile, DbgFile, cDbgFile);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetTypeFromName (
		HANDLE       hProcess,
		ULONG64      BaseOfDll,
		PCSTR        Name,
		PSYMBOL_INFO Symbol
	) {
		return real_debughelp.SymGetTypeFromName(hProcess, BaseOfDll, Name, Symbol);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetTypeInfo (
		HANDLE                    hProcess,
		DWORD64                   ModBase,
		ULONG                     TypeId,
		IMAGEHLP_SYMBOL_TYPE_INFO GetType,
		PVOID                     pInfo
	) {
		return real_debughelp.SymGetTypeInfo(hProcess, ModBase, TypeId, GetType, pInfo);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetTypeInfoEx (
		HANDLE                         hProcess,
		DWORD64                        ModBase,
		PIMAGEHLP_GET_TYPE_INFO_PARAMS Params
	) {
		return real_debughelp.SymGetTypeInfoEx(hProcess, ModBase, Params);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymInitialize (
		HANDLE hProcess,
		PCSTR  UserSearchPath,
		BOOL   fInvadeProcess
	) {
		return real_debughelp.SymInitialize(hProcess, UserSearchPath, fInvadeProcess);
	}

	__declspec(dllexport) DWORD64 __stdcall hook_SymLoadModule64 (
		HANDLE  hProcess,
		HANDLE  hFile,
		PCSTR   ImageName,
		PCSTR   ModuleName,
		DWORD64 BaseOfDll,
		DWORD   SizeOfDll
	) {
		return real_debughelp.SymLoadModule64(hProcess, hFile, ImageName, ModuleName, BaseOfDll, SizeOfDll);
	}

	__declspec(dllexport) DWORD64 __stdcall hook_SymLoadModuleEx (
		HANDLE        hProcess,
		HANDLE        hFile,
		PCSTR         ImageName,
		PCSTR         ModuleName,
		DWORD64       BaseOfDll,
		DWORD         DllSize,
		PMODLOAD_DATA Data,
		DWORD         Flags
	) {
		return real_debughelp.SymLoadModuleEx(hProcess, hFile, ImageName, ModuleName, BaseOfDll, DllSize, Data, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymMatchFileName (
		PCSTR FileName,
		PCSTR Match,
		PSTR* FileNameStop,
		PSTR* MatchStop
	) {
		return real_debughelp.SymMatchFileName(FileName, Match, FileNameStop, MatchStop);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymMatchString (
		PCSTR string,
		PCSTR expression,
		BOOL  fCase
	) {
		return real_debughelp.SymMatchString(string, expression, fCase);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymNext (
		HANDLE       hProcess,
		PSYMBOL_INFO si
	) {
		return real_debughelp.SymNext(hProcess, si);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymPrev (
		HANDLE       hProcess,
		PSYMBOL_INFO si
	) {
		return real_debughelp.SymPrev(hProcess, si);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymRefreshModuleList (
		HANDLE hProcess
	) {
		return real_debughelp.SymRefreshModuleList(hProcess);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymRegisterCallback64 (
		HANDLE                        hProcess,
		PSYMBOL_REGISTERED_CALLBACK64 CallbackFunction,
		ULONG64                       UserContext
	) {
		return real_debughelp.SymRegisterCallback64(hProcess, CallbackFunction, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymRegisterFunctionEntryCallback64 (
		HANDLE                       hProcess,
		PSYMBOL_FUNCENTRY_CALLBACK64 CallbackFunction,
		ULONG64                      UserContext
	) {
		return real_debughelp.SymRegisterFunctionEntryCallback64(hProcess, CallbackFunction, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSearch (
		HANDLE                         hProcess,
		ULONG64                        BaseOfDll,
		DWORD                          Index,
		DWORD                          SymTag,
		PCSTR                          Mask,
		DWORD64                        Address,
		PSYM_ENUMERATESYMBOLS_CALLBACK EnumSymbolsCallback,
		PVOID                          UserContext,
		DWORD                          Options
	) {
		return real_debughelp.SymSearch(hProcess, BaseOfDll, Index, SymTag, Mask, Address, EnumSymbolsCallback, UserContext, Options);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSetContext (
		HANDLE                hProcess,
		PIMAGEHLP_STACK_FRAME StackFrame,
		PIMAGEHLP_CONTEXT     Context
	) {
		return real_debughelp.SymSetContext(hProcess, StackFrame, Context);
	}

	__declspec(dllexport) PCHAR __stdcall hook_SymSetHomeDirectory (
		HANDLE hProcess,
		PCSTR  dir
	) {
		return real_debughelp.SymSetHomeDirectory(hProcess, dir);
	}

	__declspec(dllexport) DWORD __stdcall hook_SymSetOptions (
		DWORD SymOptions
	) {
		return real_debughelp.SymSetOptions(SymOptions);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSetScopeFromAddr (
		HANDLE  hProcess,
		ULONG64 Address
	) {
		return real_debughelp.SymSetScopeFromAddr(hProcess, Address);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSetScopeFromIndex (
		HANDLE  hProcess,
		ULONG64 BaseOfDll,
		DWORD   Index
	) {
		return real_debughelp.SymSetScopeFromIndex(hProcess, BaseOfDll, Index);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSetSearchPath (
		HANDLE hProcess,
		PCSTR  SearchPath
	) {
		return real_debughelp.SymSetSearchPath(hProcess, SearchPath);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymUnDName64 (
		PIMAGEHLP_SYMBOL64 sym,
		PSTR               UnDecName,
		DWORD              UnDecNameLength
	) {
		return real_debughelp.SymUnDName64(sym, UnDecName, UnDecNameLength);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymUnloadModule64 (
		HANDLE  hProcess,
		DWORD64 BaseOfDll
	) {
		return real_debughelp.SymUnloadModule64(hProcess, BaseOfDll);
	}

	__declspec(dllexport) PCSTR __stdcall hook_SymSrvDeltaName (
		HANDLE hProcess,
		PCSTR  SymPath,
		PCSTR  Type,
		PCSTR  File1,
		PCSTR  File2
	) {
		return real_debughelp.SymSrvDeltaName(hProcess, SymPath, Type, File1, File2);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSrvGetFileIndexes (
		PCSTR  File,
		GUID* Id,
		PDWORD Val1,
		PDWORD Val2,
		DWORD  Flags
	) {
		return real_debughelp.SymSrvGetFileIndexes(File, Id, Val1, Val2, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSrvGetFileIndexInfo (
		PCSTR              File,
		PSYMSRV_INDEX_INFO Info,
		DWORD              Flags
	) {
		return real_debughelp.SymSrvGetFileIndexInfo(File, Info, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSrvGetFileIndexString (
		HANDLE hProcess,
		PCSTR  SrvPath,
		PCSTR  File,
		PSTR   Index,
		size_t Size,
		DWORD  Flags
	) {
		return real_debughelp.SymSrvGetFileIndexString(hProcess, SrvPath, File, Index, Size, Flags);
	}

	__declspec(dllexport) PCSTR __stdcall hook_SymSrvGetSupplement (
		HANDLE hProcess,
		PCSTR  SymPath,
		PCSTR  Node,
		PCSTR  File
	) {
		return real_debughelp.SymSrvGetSupplement(hProcess, SymPath, Node, File);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSrvIsStore (
		HANDLE hProcess,
		PCSTR  path
	) {
		return real_debughelp.SymSrvIsStore(hProcess, path);
	}

	__declspec(dllexport) PCSTR __stdcall hook_SymSrvStoreFile (
		HANDLE hProcess,
		PCSTR  SrvPath,
		PCSTR  File,
		DWORD  Flags
	) {
		return real_debughelp.SymSrvStoreFile(hProcess, SrvPath, File, Flags);
	}

	__declspec(dllexport) PCSTR __stdcall hook_SymSrvStoreSupplement (
		HANDLE hProcess,
		PCSTR  SrvPath,
		PCSTR  Node,
		PCSTR  File,
		DWORD  Flags
	) {
		return real_debughelp.SymSrvStoreSupplement(hProcess, SrvPath, Node, File, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSourceFile (
		HANDLE  hProcess,
		ULONG64 Base,
		PCSTR   Params,
		PCSTR   FileSpec,
		PSTR    FilePath,
		DWORD   Size
	) {
		return real_debughelp.SymGetSourceFile(hProcess, Base, Params, FileSpec, FilePath, Size);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumSourceFileTokens (
		HANDLE                        hProcess,
		ULONG64                       Base,
		PENUMSOURCEFILETOKENSCALLBACK Callback
	) {
		return real_debughelp.SymEnumSourceFileTokens(hProcess, Base, Callback);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSourceFileFromToken (
		HANDLE hProcess,
		PVOID  Token,
		PCSTR  Params,
		PSTR   FilePath,
		DWORD  Size
	) {
		return real_debughelp.SymGetSourceFileFromToken(hProcess, Token, Params, FilePath, Size);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSourceFileToken (
		HANDLE  hProcess,
		ULONG64 Base,
		PCSTR   FileSpec,
		PVOID* Token,
		DWORD* Size
	) {
		return real_debughelp.SymGetSourceFileToken(hProcess, Base, FileSpec, Token, Size);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSourceVarFromToken (
		HANDLE hProcess,
		PVOID  Token,
		PCSTR  Params,
		PCSTR  VarName,
		PSTR   Value,
		DWORD  Size
	) {
		return real_debughelp.SymGetSourceVarFromToken(hProcess, Token, Params, VarName, Value, Size);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymInitializeW (
		HANDLE hProcess,
		PCWSTR UserSearchPath,
		BOOL   fInvadeProcess
	) {
		return real_debughelp.SymInitializeW(hProcess, UserSearchPath, fInvadeProcess);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymFromAddrW (
		HANDLE        hProcess,
		DWORD64       Address,
		PDWORD64      Displacement,
		PSYMBOL_INFOW Symbol
	) {
		return real_debughelp.SymFromAddrW(hProcess, Address, Displacement, Symbol);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymFromNameW (
		HANDLE        hProcess,
		PCWSTR        Name,
		PSYMBOL_INFOW Symbol
	) {
		return real_debughelp.SymFromNameW(hProcess, Name, Symbol);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetLineFromAddrW64 (
		HANDLE            hProcess,
		DWORD64           dwAddr,
		PDWORD            pdwDisplacement,
		PIMAGEHLP_LINEW64 Line
	) {
		return real_debughelp.SymGetLineFromAddrW64(hProcess, dwAddr, pdwDisplacement, Line);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetModuleInfoW64 (
		HANDLE              hProcess,
		DWORD64             qwAddr,
		PIMAGEHLP_MODULEW64 ModuleInfo
	) {
		return real_debughelp.SymGetModuleInfoW64(hProcess, qwAddr, ModuleInfo);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumSymbolsW (
		HANDLE                          hProcess,
		ULONG64                         BaseOfDll,
		PCWSTR                          Mask,
		PSYM_ENUMERATESYMBOLS_CALLBACKW EnumSymbolsCallback,
		PVOID                           UserContext
	) {
		return real_debughelp.SymEnumSymbolsW(hProcess, BaseOfDll, Mask, EnumSymbolsCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumLinesW (
		HANDLE                   hProcess,
		ULONG64                  Base,
		PCWSTR                   Obj,
		PCWSTR                   File,
		PSYM_ENUMLINES_CALLBACKW EnumLinesCallback,
		PVOID                    UserContext
	) {
		return real_debughelp.SymEnumLinesW(hProcess, Base, Obj, File, EnumLinesCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumSourceFilesW (
		HANDLE                         hProcess,
		ULONG64                        ModBase,
		PCWSTR                         Mask,
		PSYM_ENUMSOURCEFILES_CALLBACKW cbSrcFiles,
		PVOID                          UserContext
	) {
		return real_debughelp.SymEnumSourceFilesW(hProcess, ModBase, Mask, cbSrcFiles, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSetSearchPathW (
		HANDLE hProcess,
		PCWSTR SearchPath
	) {
		return real_debughelp.SymSetSearchPathW(hProcess, SearchPath);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSearchPathW (
		HANDLE hProcess,
		PWSTR  SearchPath,
		DWORD  SearchPathLength
	) {
		return real_debughelp.SymGetSearchPathW(hProcess, SearchPath, SearchPathLength);
	}

	__declspec(dllexport) DWORD64 __stdcall hook_SymLoadModuleExW (
		HANDLE        hProcess,
		HANDLE        hFile,
		PCWSTR        ImageName,
		PCWSTR        ModuleName,
		DWORD64       BaseOfDll,
		DWORD         DllSize,
		PMODLOAD_DATA Data,
		DWORD         Flags
	) {
		return real_debughelp.SymLoadModuleExW(hProcess, hFile, ImageName, ModuleName, BaseOfDll, DllSize, Data, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumTypesW (
		HANDLE                          hProcess,
		ULONG64                         BaseOfDll,
		PSYM_ENUMERATESYMBOLS_CALLBACKW EnumSymbolsCallback,
		PVOID                           UserContext
	) {
		return real_debughelp.SymEnumTypesW(hProcess, BaseOfDll, EnumSymbolsCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymEnumTypesByNameW (
		HANDLE                          hProcess,
		ULONG64                         BaseOfDll,
		PCWSTR                          mask,
		PSYM_ENUMERATESYMBOLS_CALLBACKW EnumSymbolsCallback,
		PVOID                           UserContext
	) {
		return real_debughelp.SymEnumTypesByNameW(hProcess, BaseOfDll, mask, EnumSymbolsCallback, UserContext);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymSearchW (
		HANDLE                          hProcess,
		ULONG64                         BaseOfDll,
		DWORD                           Index,
		DWORD                           SymTag,
		PCWSTR                          Mask,
		DWORD64                         Address,
		PSYM_ENUMERATESYMBOLS_CALLBACKW EnumSymbolsCallback,
		PVOID                           UserContext,
		DWORD                           Options
	) {
		return real_debughelp.SymSearchW(hProcess, BaseOfDll, Index, SymTag, Mask, Address, EnumSymbolsCallback, UserContext, Options);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSymbolFileW (
		HANDLE hProcess,
		PCWSTR SymPath,
		PCWSTR ImageFile,
		DWORD  Type,
		PWSTR  SymbolFile,
		size_t cSymbolFile,
		PWSTR  DbgFile,
		size_t cDbgFile
	) {
		return real_debughelp.SymGetSymbolFileW(hProcess, SymPath, ImageFile, Type, SymbolFile, cSymbolFile, DbgFile, cDbgFile);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSourceFileW (
		HANDLE  hProcess,
		ULONG64 Base,
		PCWSTR  Params,
		PCWSTR  FileSpec,
		PWSTR   FilePath,
		DWORD   Size
	) {
		return real_debughelp.SymGetSourceFileW(hProcess, Base, Params, FileSpec, FilePath, Size);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetSourceFileFromTokenW (
		HANDLE hProcess,
		PVOID  Token,
		PCWSTR Params,
		PWSTR  FilePath,
		DWORD  Size
	) {
		return real_debughelp.SymGetSourceFileFromTokenW(hProcess, Token, Params, FilePath, Size);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymFindFileInPathW (
		HANDLE                   hprocess,
		PCWSTR                   SearchPath,
		PCWSTR                   FileName,
		PVOID                    id,
		DWORD                    two,
		DWORD                    three,
		DWORD                    flags,
		PWSTR                    FoundFile,
		PFINDFILEINPATHCALLBACKW callback,
		PVOID                    context
	) {
		return real_debughelp.SymFindFileInPathW(hprocess, SearchPath, FileName, id, two, three, flags, FoundFile, callback, context);
	}

	__declspec(dllexport) HANDLE __stdcall hook_FindDebugInfoFileExW (
		PCWSTR                     FileName,
		PCWSTR                     SymbolPath,
		PWSTR                      DebugFilePath,
		PFIND_DEBUG_FILE_CALLBACKW Callback,
		PVOID                      CallerData
	) {
		return real_debughelp.FindDebugInfoFileExW(FileName, SymbolPath, DebugFilePath, Callback, CallerData);
	}

	__declspec(dllexport) HANDLE __stdcall hook_FindExecutableImageExW (
		PCWSTR                   FileName,
		PCWSTR                   SymbolPath,
		PWSTR                    ImageFilePath,
		PFIND_EXE_FILE_CALLBACKW Callback,
		PVOID                    CallerData
	) {
		return real_debughelp.FindExecutableImageExW(FileName, SymbolPath, ImageFilePath, Callback, CallerData);
	}

	__declspec(dllexport) BOOL __stdcall hook_EnumDirTreeW (
		HANDLE                 hProcess,
		PCWSTR                 RootPath,
		PCWSTR                 InputPathName,
		PWSTR                  OutputPathBuffer,
		PENUMDIRTREE_CALLBACKW cb,
		PVOID                  data
	) {
		return real_debughelp.EnumDirTreeW(hProcess, RootPath, InputPathName, OutputPathBuffer, cb, data);
	}

	__declspec(dllexport) BOOL __stdcall hook_SearchTreeForFileW (
		PCWSTR RootPath,
		PCWSTR InputPathName,
		PWSTR  OutputPathBuffer
	) {
		return real_debughelp.SearchTreeForFileW(RootPath, InputPathName, OutputPathBuffer);
	}

	__declspec(dllexport) DWORD __stdcall hook_UnDecorateSymbolNameW (
		PCWSTR name,
		PWSTR  outputString,
		DWORD  maxStringLength,
		DWORD  flags
	) {
		return real_debughelp.UnDecorateSymbolNameW(name, outputString, maxStringLength, flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymAddSymbolW (
		HANDLE  hProcess,
		ULONG64 BaseOfDll,
		PCWSTR  Name,
		DWORD64 Address,
		DWORD   Size,
		DWORD   Flags
	) {
		return real_debughelp.SymAddSymbolW(hProcess, BaseOfDll, Name, Address, Size, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymDeleteSymbolW (
		HANDLE  hProcess,
		ULONG64 BaseOfDll,
		PCWSTR  Name,
		DWORD64 Address,
		DWORD   Flags
	) {
		return real_debughelp.SymDeleteSymbolW(hProcess, BaseOfDll, Name, Address, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymFromIndexW (
		HANDLE        hProcess,
		ULONG64       BaseOfDll,
		DWORD         Index,
		PSYMBOL_INFOW Symbol
	) {
		return real_debughelp.SymFromIndexW(hProcess, BaseOfDll, Index, Symbol);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymGetTypeFromNameW (
		HANDLE        hProcess,
		ULONG64       BaseOfDll,
		PCWSTR        Name,
		PSYMBOL_INFOW Symbol
	) {
		return real_debughelp.SymGetTypeFromNameW(hProcess, BaseOfDll, Name, Symbol);
	}

	__declspec(dllexport) BOOL __stdcall hook_SymMatchFileNameW (
		PCWSTR FileName,
		PCWSTR Match,
		PWSTR* FileNameStop,
		PWSTR* MatchStop
	) {
		return real_debughelp.SymMatchFileNameW(FileName, Match, FileNameStop, MatchStop);
	}

	__declspec(dllexport) BOOL __stdcall hook_StackWalkEx (
		DWORD                            MachineType,
		HANDLE                           hProcess,
		HANDLE                           hThread,
		LPSTACKFRAME_EX                  StackFrame,
		PVOID                            ContextRecord,
		PREAD_PROCESS_MEMORY_ROUTINE64   ReadMemoryRoutine,
		PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
		PGET_MODULE_BASE_ROUTINE64       GetModuleBaseRoutine,
		PTRANSLATE_ADDRESS_ROUTINE64     TranslateAddress,
		DWORD                            Flags
	) {
		return real_debughelp.StackWalkEx(MachineType, hProcess, hThread, StackFrame, ContextRecord, ReadMemoryRoutine, FunctionTableAccessRoutine, GetModuleBaseRoutine, TranslateAddress, Flags);
	}

	__declspec(dllexport) BOOL __stdcall hook_StackWalk2 (
		DWORD                            MachineType,
		HANDLE                           hProcess,
		HANDLE                           hThread,
		LPSTACKFRAME_EX                  StackFrame,
		PVOID                            ContextRecord,
		PREAD_PROCESS_MEMORY_ROUTINE64   ReadMemoryRoutine,
		PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
		PGET_MODULE_BASE_ROUTINE64       GetModuleBaseRoutine,
		PTRANSLATE_ADDRESS_ROUTINE64     TranslateAddress,
		PGET_TARGET_ATTRIBUTE_VALUE64    GetTargetAttributeValue,
		DWORD                            Flags
	) {
		return real_debughelp.StackWalk2(MachineType, hProcess, hThread, StackFrame, ContextRecord, ReadMemoryRoutine, FunctionTableAccessRoutine, GetModuleBaseRoutine, TranslateAddress, GetTargetAttributeValue, Flags);
	}
}
