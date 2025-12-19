Experimental attempt at replicating functionality from dbghelp.dll, as dbghelp.dll can be very slow especially when used in something like https://github.com/wolfpld/tracy.
tracy has serious performance problems on windows with bevy projects, as the symbol resoltuion is extremely slow.
Tracy only needs to resolve symbols, and I figured that that can't be that hard as it is a simple question of mapping an address to a set of values.
So I decided to attempt to measure, understand and replicate this.

Resolving symbols includes mapping virtual addresses (usually of code) inside a process to:
-module name (exe/dll)
-symbol name (the function at the top of stack)
-source code filename + line number

Another thing are inline stacks, as there is no symbol info for code that resulted from inlined functions,
so for each code address there can be a variable number of inlinesites each with their own:
-function name
-source code filename + line number

In dbghelp.dll the following functions accomplish this (at least that's what tracy uses)
SymSetOptions + SymInitialize
SymFromAddr (symbol name)
SymGetLineFromAddr64 (filename + line number)
SymAddrIncludeInlineTrace + SymQueryInlineTrace (established depth of inlinee stack and returns a context variable needed to access them, but I'm not too sure why this is actually needed...)
SymFromInlineContext (function name)
SymGetLineFromInlineContext (filename + line number)

This process seems simple if brute forced, but a good implementation would compress this such that lookup becomes at least slightly complicated.
Turns out pdb files are old, overcomplicated for this purpose and the format itself along with producers and consumers seem to be buggy and inconsistent, more on that later.

The goal is:
-A resonably robust, fully custom pdb symbol resolver that is significantly faster than dbghelp.dll
-A custom dbghelp.dll specifically meant to act as a drop-in replacement that can fix the performance problems with tracy

Currently this project contains:
-fully custom pdb parser that enables the symbol resultion, explained later
-few example executables (small test executables build in this VS solution, 1 large opengl executable, 1 humongous rust executable (bevy 250MB pdb!), which is really slow in dbghelp)  
-a framework that launches these executables via windows debugging APIs and can then query the loaded modules and can then map queried addresses into these modules.
 It locates the associated pdbs via exe/dll header information according to this process:
  -loads pdbs next to exe
  -else loads pdbs from absolute paths if these are stored in exe (this seems to be the case for VS build executables)
  -else loads pdbs from AppData\Local\Temp\SymbolCache or downloades them from MS symbol servers and caches them itself
-it can test hardcoded addresses, full executable address sweeps or random sampled addresses by comparing their results between dbghelp and the custom implementation
-result comparison logic that logs differences, classifies and counts them

The custom symbol resolver is fully features according to the above set of data, with symbols, line info and inline stack.
However while all the hard-coded test cases for my small test executables match, the larger executables and also the address sweeps in general show mismatches that I cannot solve right now:

This includes overlapping symbols (which means there no longer is a single correct symbol despite SymFromAddr only returning one),
this seems to usually come from functions that happen to have identical code being "folded" into one single piece of code, observed on auto-generated dtors for example.
Unfortunately I can't seem to find any pattern in dbghelp, it sometimes returns the first symbol, sometimes the last, so I cannot match the output exactly, but neither mine nor dbghelp is actually "correct" in this case anyway.

Sometimes I seem to get (seemingly correct) line information while dbghelp fails for some reason.
Sometimes I get more inlinesites than dbghelp, usually all correct as well, the incorrect cases observed in the rust executable may actually be an LLVM bug.
Sometimes there linenumbers will differ (TODO: debug)
Relatively rarely symbols will differ and my code tells me it's not the ambiguous case as above (TODO: debug)

TODO: list all complications here and possibly even explain the overall pdb lookup process
