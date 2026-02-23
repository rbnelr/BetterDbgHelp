# Introduction
A fast implementation of symbol lookup for pdb files written from scratch, and a drop-in dbghelp.dll replacement to fix [tracy](https://github.com/wolfpld/tracy)'s performance.

Achieved by finding pdb files, download them from symbol servers, extracting the relevant data and caching a fast lookup in memory, with caching of the lookup on disk planned as well.

Lookup times of <1us are achieved, compared to 20-250us or worse with dbghelp. Which often is a 100x speedup in my testing even including my very slow pdb parsing!

Only the dbghelp API needed in tracy is currently implemented, more not planned but may be possible.
99% of symbols found in my test executables are replicated exactly, at least for profilers, though there are puzzling exceptions.
Testing all possible binaries out there is not possible, so expect weird failures for executables that could not be tested.

# Goals
Symbol resolution for profilers or debuggers involves mapping address ranges in the target executable to 'symbols'. Often these are functions, but can be variables or likely many more things.
Function symbols then contains line information and sometimes an entire stack of inlinesites with their own line info.

Symbol resolution with pdb files as implemented by dbghelp.dll is extremely slow, by orders of magnitude, being a `uint64 address` -> `SymbolData` mapping and scales worse then the logN you may expect from a binary search.

This likely has historical reasons and may be acceptable for use in debuggers, but the way it is used in profilers like tracy causes problems.
Users of tracy will find that in sample-based mode, even short profiling runs may require excessively long waiting times afterwards for all the data to come in, which to my knowledge this is almost entirely caused by dbghelp's bad performance, and gets worse with larger pdb files.
I was working with the bevy game engine in rust (built via LLVM on windows), in which sample-based profiling via tracy because essentially impossible.
My frustration there made my want to prove that there was no reason it had to be this slow, learn about what pdb files contain and produce some sort of fix.

Thanks to [PascalBeyer's PDB-Documentation](https://github.com/PascalBeyer/PDB-Documentation) effort, without which I would not have gotten far.

I did not indend to fully understand a pdb's contents, just what was needed for symbol lookup, but the inlinesites ended up making things more and more involved. Still the format is mysterious and feels like it could be hiding a million more things...

# Features
* Finding pdb via RSDSI in executable (.exe/.dll) headers (Not well tested)
* Lookup symbols via Export Tables in dll if pdb missing
* Download pdb from MS Symbol Servers to `%temp%\Hexcoder\SymbolCache` (Not shared with MS SymbolCache to avoid problems as this is not well tested)
* PDB parsing from scratch, which is reasonably optimized
* Building of a custom lookup structure on first access of a pdb, currently stays in memory
* Testing/Profiling framework that launches test executables, and can then verify custom implementation against dbghelp
* Logging of basic profiling and verification results
* Drop-in dbghelp.dll which implements API for tracy and can call original dbghelp if desired, optional verification/profiling against dbghelp planned
### Testing modes
* Example Addresses - Hand-picked symbols for basic verification
* Sweeping Addresses - Byte-by-byte scan of entire executable
* Fuzzing Addresses - Random addresses in executable
### Implemented API
* `SymSetOptions`, `SymSetSearchPath`, `SymInitialize`, `SymCleanup`
* Getting symbol names with namespace prefixing (`SymFromAddr`, but some less important values returned in `SYMBOL_INFO` can not be replicated)  
* Getting basic line info (`SymGetLineFromAddr64`)
* Getting inline stack (`SymAddrIncludeInlineTrace`, `SymQueryInlineTrace` (arguments not fully supported))
* Getting inline name and line ineof (`SymFromInlineContext`, `SymGetLineFromInlineContext`)
* Redundant searches are avoided, via caching across calls, last symbol base information and the slower inline walk cached separately

# Results
### Test executables
| Executable          | pdb size | Details     | Description    |
|---------------------|----------|-------------|----------------|
| TinyProgram         | 796KB    | C++, MSVC   | Just a few functions |
| Namespaces          | 476KB    | C++, MSVC   | Added later to test namespacing |
| CppGame             | 17.4MB   | C++, MSVC   | Personal project, Game written with OpenGL and a couple of libraries |
| RustBevyApp         | 254MB    | rust (LLVM) | Very simple Bevy project, bevy and rust sadly bloat pdb, lots of inlines |

A few dlls and notepad.exe were also tested

### Results
Times are avergage lookup times for symbol name + line info + full inline stack.
Times averaged over as many iterations as can run reasonably fast.
Some testing was done to understand cache behavior of lookups, like polluting the cache between lookups.

| Executable          | Mode | Dbghelp | Custom | Custom w/o PDB read | Speedup w/o PDB read |
|---------------------|------|---------|--------|---------------------|----------------------|
|          TinyProgram | Example |  35.84 us |   0.23 us |   0.22 us |  166x |
|           Namespaces | Example |  16.68 us |   0.19 us |   0.17 us |   98x |
|              CppGame | Example | 139.18 us |   0.81 us |   0.49 us |  281x |
|          RustBevyApp | Example | 248.84 us |  18.09 us |   0.96 us |  260x |
|      TinyProgram.exe | Sweeping |  20.26 us |   0.24 us |   0.18 us |  115x |
|         ucrtbase.dll | Sweeping |   1.77 us |   0.05 us |   0.05 us |   39x |
|  city_builder_rel.exe | Sweeping | 191.16 us |   0.34 us |   0.33 us |  587x |
|  city_builder_rel.exe | Fuzzing | 215.20 us |   2.74 us |   1.27 us |  169x |
|    rust_bevy_test.exe | Fuzzing | 7041.76 us | 211.21 us |   6.48 us | 1086x |

### Accuracy
The C++ apps show only 1-2% of symbols are mismatched with dbghelp, but a lot more testing would have to be done across different compilers.
Some of the mismatches are simply the result of ambiguous symbols placed at the same address due to merged functions, where dbghelp chooses a random one (likely due to using a hashmap so I can't replicate it).

Sadly the Bevy example is at almost 30%, most of which, weirdly, is dbghelp not returning inlinesites that I do, it's not clear yet if this a bug in my code or in LLVM.

# Project Structure
### MSVC Solution
* `Testing` is the testing application, this mostly calls pdb parser directly, without going through custom dbghelp.dll
* `BetterDbgHelp` is the drop-in dll

### Code Overview
* `TestRunner` launches executables for testing
* `SymResult`, `MismatchCounts` Result printing, comparison and mismatch counting (not actually needed in dll API)
* `SymResolver...` are used by the test framework
* `ModuleCache` handles virtual address -> loaded executable mapping
* `PDB_Locator` find pdb and download from symbol servers
* `ExportTableQuery` lookup just exported functions from dll itself if pdb not found
* `PdbReader` entire pdb parsing logic
* `FastPdbLookup` lookup class generated from pdb reading
* `AddressIndex` module relative address to symbol lookup, uses a binary search with a simd optimization
* `lineinfo::` Custom lineinfo encoding, optimized to be compact enough and decode fast
* `DebughelpApi` function pointers loaded via GetProcAddress to enable calling original dbghelp.dll from fake dbghelp.dll
* `DbgHelpWrapperSession`, `DbgHelpWrapper` symbol lookup caching and adapting to dbghelp API
* util.hpp: `StrAlloc`, `BinAlloc` crude push allocators for efficient allocation of lookup data, return ids=offsets for access

# Known Caveats / TODOs
* have not added mismatch logging to dll version yet, which will allow testing in real profiling scenarios
* if app uses many dlls, pdb reading times can eat into performance gains, also lookup data can use lots of RAM -> cache lookup data on disk and read back via memory mapped file!
* lots of mismatches with LLVM, llvm bug? If bug in my code why does it work so well in c++?
* embarrassingly I had only tested release-mode executables at the start, added trampoline symbols later but still need to test debug mode more
* SYMBOL_INFO has some fields not needed by tracy, some cannot be replicated, unclear which use cases they have
* OMAP data is not implemented, when do executables even have OMAP?
* pdb reading reliability, need to turn some asserts into errors

# Overview of symbol resolution in PDBs
TODO
