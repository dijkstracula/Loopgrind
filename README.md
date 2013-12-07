Loopgrind - an event loop analyzer
=========

Overview
---------

This repository contains a mirror of the Subversion repo for Nathan Taylor's final project in CPSC 538: Execution Mining, Winter 2010, at the University of British Columbia.

>At the heart of nearly every modern program is a main event loop which typically runs con- tinuously until the termination of the program. This loop is significant for the purposes of exe- cution analysis because it is in some sense the ”root” of the program’s behaviour, be it the message pump in a GUI-driven program or an accept() loop for a network server.
>
>In the absence of of having the source for a given program, it is fruitful to analyze ex- ecution traces by studying their characteristic event loops. If one were to posit that programs of a similar purpose have similar event loops, then studying the main loop of a program as a sketch of its high level behaviour may yield interesting insights.
>
>To this end, I present Loopgrind, a tool to instrument a running process with the intent of finding and analyzing the primary loops in a program. The first goal, loop discovery, is at- tained by capturing a basic block-level control flow graph, optimizing out linear block chains, and applying a simple heuristic that, in most cases, found the correct loop. The second, loop analysis, is done by logging changes in memory across loop iterations, giving a concise sum- mary of state changes as the program runs.  Loopgrind is implemented in two parts. As stated above, loop detection and memory anal- ysis is done through a Valgrind plugin, and the loop analysis and miscellaneous utilities are im- plemented with a Perl script.
>
>The default behaviour for Loopgrind is to gen- erate a control flow graph of the execution trace. In such a graph, the vertices correspond to Val- grind superblocks (a single-entry, multiple-exit segment of instructions), and the weight of the edge linking a given superblock to its jump tar- get is increased every time the jump is taken.
>
>Loopgrind’s analysis component is performed offline, in a helper Perl script that the Val- grind tool’s output is piped to. There is no fundamental reason why any of the operations we perform in this script could not have been done during instrumentation, but making use of Perl’s graph algorithms module allowed for rapid prototyping and experimentation.
>
>Once we have identified a primary loop, we would next like to analyze how the program’s state changes across iterations. To this end, we hash the address and value of each memory write, and each time the running executable jumps back to the loop header, the table is reset and its contents are printed out. As a result, across iterations, it becomes simple to identify commonalities between writes, such as correlating a value that increments by a fixed amount over each iteration as a counter.

For more detailed information, please see the project writeup, 538w-project-paper.pdf.

Installation and Setup
----------

Loopgrind is a plugin for the Valgrind dynamic analysis framework.  Download the Valgrind source, and copy the loopgrind/ directory into your Valgrind root directory.  You will have to change the build scripts so they know about Loopgrind:

1. In configure.am, add loopgrind to TOOLS.
2. In configure.in, add loopgrind/Makefile, loopgrind/docs/Makefile, and loopgrind/tests/Makefile to the AC_OUTPUT list.

Loopgrind requires a semi-modern ia32 Linux system.  While Valgrind
may be compiled under OSX, the analysis script scrapes output
from GNU Binutils, so at the moment we are locked into Linux.
From the root loopgrind directory, type

 $ make -C valgrind-3.5.0/

The core Valgrind source, as well as all plugins, will compile.

The loopgrind source is located in valgrind-3.5.0/loopgrind/ .

The Perl analysis script requires several modules to be installed
via CPAN or, distribution-allowing, your package manager of choice:

   - Data::Dumper;
   - Graph::Directed;
   - GraphViz;
   - List::Util;
   
Also included are a series of test programs in the tests/ directory.

Usage
-------

The main loopgrind kickoff script is ./loopgrind.sh, located in the
root directory.  Its usage is as follows:

Usage: 

1. loopgrind.sh <program path>
2. loopgrind.sh -p <program path>
3. loopgrind.sh -a <loop address> <program path>

The first usage is the default behaviour; it simply outputs the results of the
Valgrind tool to standard out.  the -p flag pipes the output to the 
analysis Perl script.  the -a <addr> flag can be run with a loop header
address to track memory changes through each iteration of that address.

This repository contains Valgrind 3.5.0 - loopgrind's source is to be found in valgrind-3.5.0/loopgrind.

Caveats
-------

This is research code, written in haste.  It's perhaps instructive as another example of writing a Valgrind plugin but I'd be surprised if you can do much with it.  If you do, drop me a line and tell me about it.
