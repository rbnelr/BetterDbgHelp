#!/usr/bin/env python3
import requests
import re
import time
from html.parser import HTMLParser
from urllib.parse import urljoin

# from dumpbin /exports on dbghelp.dll
DBGHELP_FUNCTIONS = [
"DbgHelpCreateUserDump",
"DbgHelpCreateUserDumpW",
"EnumDirTree",
"EnumDirTreeW",
"EnumerateLoadedModules",
"EnumerateLoadedModules64",
"EnumerateLoadedModulesEx",
"EnumerateLoadedModulesExW",
"EnumerateLoadedModulesW64",
"ExtensionApiVersion",
"FindDebugInfoFile",
"FindDebugInfoFileEx",
"FindDebugInfoFileExW",
"FindExecutableImage",
"FindExecutableImageEx",
"FindExecutableImageExW",
"FindFileInPath",
"FindFileInSearchPath",
"GetSymLoadError",
"GetTimestampForLoadedLibrary",
"ImageDirectoryEntryToData",
"ImageDirectoryEntryToDataEx",
"ImageNtHeader",
"ImageRvaToSection",
"ImageRvaToVa",
"ImagehlpApiVersion",
"ImagehlpApiVersionEx",
"MakeSureDirectoryPathExists",
"MiniDumpReadDumpStream",
"MiniDumpWriteDump",
"RangeMapAddPeImageSections",
"RangeMapCreate",
"RangeMapFree",
"RangeMapRead",
"RangeMapRemove",
"RangeMapWrite",
"RemoveInvalidModuleList",
"ReportSymbolLoadSummary",
"SearchTreeForFile",
"SearchTreeForFileW",
"SetCheckUserInterruptShared",
"SetSymLoadError",
"StackWalk",
"StackWalk64",
"StackWalkEx",
"SymAddSourceStream",
"SymAddSourceStreamA",
"SymAddSourceStreamW",
"SymAddSymbol",
"SymAddSymbolW",
"SymAddrIncludeInlineTrace",
"SymAllocDiaString",
"SymCleanup",
"SymCompareInlineTrace",
"SymDeleteSymbol",
"SymDeleteSymbolW",
"SymEnumLines",
"SymEnumLinesW",
"SymEnumProcesses",
"SymEnumSourceFileTokens",
"SymEnumSourceFiles",
"SymEnumSourceFilesW",
"SymEnumSourceLines",
"SymEnumSourceLinesW",
"SymEnumSym",
"SymEnumSymbols",
"SymEnumSymbolsEx",
"SymEnumSymbolsExW",
"SymEnumSymbolsForAddr",
"SymEnumSymbolsForAddrW",
"SymEnumSymbolsW",
"SymEnumTypes",
"SymEnumTypesByName",
"SymEnumTypesByNameW",
"SymEnumTypesW",
"SymEnumerateModules",
"SymEnumerateModules64",
"SymEnumerateModulesW64",
"SymEnumerateSymbols",
"SymEnumerateSymbols64",
"SymEnumerateSymbolsW",
"SymEnumerateSymbolsW64",
"SymFindDebugInfoFile",
"SymFindDebugInfoFileW",
"SymFindExecutableImage",
"SymFindExecutableImageW",
"SymFindFileInPath",
"SymFindFileInPathW",
"SymFreeDiaString",
"SymFromAddr",
"SymFromAddrW",
"SymFromIndex",
"SymFromIndexW",
"SymFromInlineContext",
"SymFromInlineContextW",
"SymFromName",
"SymFromNameW",
"SymFromToken",
"SymFromTokenW",
"SymFunctionTableAccess",
"SymFunctionTableAccess64",
"SymFunctionTableAccess64AccessRoutines",
"SymGetDiaSession",
"SymGetExtendedOption",
"SymGetFileLineOffsets64",
"SymGetHomeDirectory",
"SymGetHomeDirectoryW",
"SymGetLineFromAddr",
"SymGetLineFromAddr64",
"SymGetLineFromAddrEx",
"SymGetLineFromAddrW64",
"SymGetLineFromInlineContext",
"SymGetLineFromInlineContextW",
"SymGetLineFromName",
"SymGetLineFromName64",
"SymGetLineFromNameEx",
"SymGetLineFromNameW64",
"SymGetLineNext",
"SymGetLineNext64",
"SymGetLineNextEx",
"SymGetLineNextW64",
"SymGetLinePrev",
"SymGetLinePrev64",
"SymGetLinePrevEx",
"SymGetLinePrevW64",
"SymGetModuleBase",
"SymGetModuleBase64",
"SymGetModuleInfo",
"SymGetModuleInfo64",
"SymGetModuleInfoW",
"SymGetModuleInfoW64",
"SymGetOmapBlockBase",
"SymGetOmaps",
"SymGetOptions",
"SymGetScope",
"SymGetScopeW",
"SymGetSearchPath",
"SymGetSearchPathW",
"SymGetSourceFile",
"SymGetSourceFileChecksum",
"SymGetSourceFileChecksumW",
"SymGetSourceFileFromToken",
"SymGetSourceFileFromTokenW",
"SymGetSourceFileToken",
"SymGetSourceFileTokenW",
"SymGetSourceFileW",
"SymGetSourceVarFromToken",
"SymGetSourceVarFromTokenW",
"SymGetSymFromAddr",
"SymGetSymFromAddr64",
"SymGetSymFromName",
"SymGetSymFromName64",
"SymGetSymNext",
"SymGetSymNext64",
"SymGetSymPrev",
"SymGetSymPrev64",
"SymGetSymbolFile",
"SymGetSymbolFileW",
"SymGetTypeFromName",
"SymGetTypeFromNameW",
"SymGetTypeInfo",
"SymGetTypeInfoEx",
"SymGetUnwindInfo",
"SymInitialize",
"SymInitializeW",
"SymLoadModule",
"SymLoadModule64",
"SymLoadModuleEx",
"SymLoadModuleExW",
"SymMatchFileName",
"SymMatchFileNameW",
"SymMatchString",
"SymMatchStringA",
"SymMatchStringW",
"SymNext",
"SymNextW",
"SymPrev",
"SymPrevW",
"SymQueryInlineTrace",
"SymRefreshModuleList",
"SymRegisterCallback",
"SymRegisterCallback64",
"SymRegisterCallbackW64",
"SymRegisterFunctionEntryCallback",
"SymRegisterFunctionEntryCallback64",
"SymSearch",
"SymSearchW",
"SymSetContext",
"SymSetDiaSession",
"SymSetExtendedOption",
"SymSetHomeDirectory",
"SymSetHomeDirectoryW",
"SymSetOptions",
"SymSetParentWindow",
"SymSetScopeFromAddr",
"SymSetScopeFromIndex",
"SymSetScopeFromInlineContext",
"SymSetSearchPath",
"SymSetSearchPathW",
"SymSrvDeltaName",
"SymSrvDeltaNameW",
"SymSrvGetFileIndexInfo",
"SymSrvGetFileIndexInfoW",
"SymSrvGetFileIndexString",
"SymSrvGetFileIndexStringW",
"SymSrvGetFileIndexes",
"SymSrvGetFileIndexesW",
"SymSrvGetSupplement",
"SymSrvGetSupplementW",
"SymSrvIsStore",
"SymSrvIsStoreW",
"SymSrvStoreFile",
"SymSrvStoreFileW",
"SymSrvStoreSupplement",
"SymSrvStoreSupplementW",
"SymUnDName",
"SymUnDName64",
"SymUnloadModule",
"SymUnloadModule64",
"UnDecorateSymbolName",
"UnDecorateSymbolNameW",
"WinDbgExtensionDllInit",
]

# can find most functions via the first url + lower case function anem
# but some functions are at other base url
# some are undocumented but are in dbghelp.h (mostly deprecated functions0
# others don't appear in header and give no results when googled, possibly exported by accident, ignore all of these
BASE_URLS = [
    "https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-",
    "https://learn.microsoft.com/en-us/windows/win32/api/minidumpapiset/nf-minidumpapiset-",
]

# NOTE: Throwaway code, partially AI generated

def fetch_function_page(func_name: str) -> str | None:
    """Fetch the documentation page for a function."""
    for base_url in BASE_URLS:
        url = base_url + func_name.lower()
        try:
            resp = requests.get(url, timeout=10)
            if resp.status_code == 200:
                return resp.text
            else:
                pass
        except Exception as e:
            pass
    print(f"  {func_name}: Not found")
    return None

def extract_syntax(html: str) -> str | None:
    """Extract the C++ syntax block from the HTML."""
    # Look for the syntax section - it's in a <pre><code> block
    # Pattern: find content between "Syntax" header and the code block
    
    # The syntax is typically in: <pre><code class="lang-cpp">...</code></pre>
    pattern = r'<pre[^>]*><code[^>]*class="[^"]*cpp[^"]*"[^>]*>(.*?)</code></pre>'
    match = re.search(pattern, html, re.DOTALL | re.IGNORECASE)
    
    if not match:
        # Try alternative pattern
        pattern = r'<pre[^>]*><code[^>]*>(.*?)</code></pre>'
        match = re.search(pattern, html, re.DOTALL)
    
    if match:
        code = match.group(1)
        # Decode HTML entities
        code = code.replace('&lt;', '<').replace('&gt;', '>')
        code = code.replace('&amp;', '&').replace('&quot;', '"')
        code = code.replace('&#39;', "'")
        # Remove HTML tags (like <span>)
        code = re.sub(r'<[^>]+>', '', code)
        # Clean up whitespace
        code = code.strip()
        return code
    
    return None

def main():
    print("Fetching DbgHelp function signatures from Microsoft Learn...")
    print("=" * 60)
    
    code1 = ""
    code2 = ""
    code3 = ""
    code4 = ""
    code5 = ""
    failed = []
    
    for i, func in enumerate(DBGHELP_FUNCTIONS):
        print(f"[{i+1}/{len(DBGHELP_FUNCTIONS)}] {func}...", end=" ", flush=True)
        
        html = fetch_function_page(func)
        if not html:
            failed.append(func)
            continue
        
        sig_code = extract_syntax(html)
        if not sig_code:
            print("no sig_code found")
            failed.append(func)
            continue

        # remove [in] [out] [in, optional] etc.
        sig_code = re.sub(r'\[[^\]]*\]\s*', '', sig_code)
    
        m = re.search(r'(?:DBHLP_DEPRECIATED\s+)?(\w+)\s+(IMAGEAPI|WINAPI|CALLBACK)?\s*(\w+)\s*(\(.*\));', sig_code, re.DOTALL)
        ret_type, call_conv, func_name, param_list = m.groups()
    
        # Extract param names
        #m = re.findall(r'([\w\*]+)\s+(\w+)\s*[,)]', param_list)
        m = re.findall(r'(\w+)\s*[,)]', param_list)
        param_names = ', '.join(m)

        typedef = f'typedef {ret_type} (__stdcall *t_{func_name}) {param_list};'
        func_ptr = f't_{func_name} {func_name} = nullptr;'
        getproc = f'{func_name} = (t_{func_name})GetProcAddress(dll, \"{func_name}\");'

        replacement_func = f'__declspec(dllexport) {ret_type} __stdcall hook_{func_name} {param_list} {{\n'\
        +f'  return real_dbghelp.{func_name}({param_names});\n'\
        +f'}}'

        def_file = f'\t{func_name}=hook_{func_name}'

        #print(f"")
        #print(f"// {func}")
        #print(typedef)
        #print(f"//")
        #print(func_ptr)
        #print(f"//")
        #print(replacement_func)

        code1 = code1 + typedef + '\n'
        code2 = code2 + func_ptr + '\n'
        code3 = code3 + getproc + '\n'
        code4 = code4 + replacement_func + '\n'
        code5 = code5 + def_file + '\n'
        
        print("OK")

        # Be nice to the server
        time.sleep(0.2)
    
    print()
    print("=" * 60)
    print(f"Successfully parsed: {len(code1)}")
    print(f"Failed: {len(failed)}")
    if failed:
        print(f"  {', '.join(failed)}")
    print()
    
    print(code1)
    print("======== \n\n")
    print(code2)
    print("======== \n\n")
    print(code3)
    print("======== \n\n")
    print(code4)
    print("======== \n\n")
    print(code5)
    

if __name__ == "__main__":
    main()
