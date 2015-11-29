# cse824-sim
CSE824 Tossim and Tinyos health management simulation 

Installation:
The TOSSIM simulation runs on Cygwin in Windows.  The following instructions are for Windows 10 only.

Clone nesc from the tinyos/nesc repository and build it:
Install the following dependencies in cygwin: autoconf(2.64 or 2.69 or later. 2.67 and 2.68 are known not to work.), automake, emacs (not version 24.3), gperf, bison, flex
- ./Bootstrap
- ./configure --prefix=/usr; 
- make
- make install

Clone tinyos from the tinyos/tinyos-main repository and build it:
Install the following dependencies in cygwin: autoconf, automake, python3, gcc-g++, mingw-gcc-core, mingw-gcc-g++, mingw64-x86_64-gcc-core, mingw64-x86_64-gcc-g++
- ./Bootstrap
- ./configure --prefix=/usr; 
- make
- make install

The makefile for building the sim will need to be modified:
In support/make/extras/sim.extra, 
-PYTHON_VERSION = 2.7
-PLATFORM_BUILD_FLAGS= -fpic #-W1,--enabled-auto-image-base

Several files in cygwin and tinyos will need to be modified:
Update /tools/tinyos/misc/tos-serial-debug.c 215: handle func (osf) with (_osf). Buliding tinyos tools will probably throw an error depending on windows version.
Update /usr/include/cygwin/signal.h to (102): } __attribute__ ((aligned (16))) ; nescc doesnt like the __attribute__ after struct

Once this is completed, overwrite all the files found in the tinyos_fixes folder in this repository.

To build and run the simulation:
- make
- ./launch.sh