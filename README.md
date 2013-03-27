qemu-tsx
========

Add Transactional Synchronization eXtensions to QEMU

About
========

Intel's Haswell microarchitecture will support Transactional Synchronization eXtensions (TSX)
including Restricted Transactional Memory (RTM) and Hardware Lock Elision (HLE). The specification
can be found at http://software.intel.com/sites/default/files/m/3/2/1/0/b/41417-319433-012.pdf

This project is an implementation of TSX on top of the QEMU emulator.


Installation
========

./configure --target-list=x86_64-softmmu

make

make install

Usage
========

qemu-system-x86_64 -cpu Haswell

This creates a file 'execlog', which can be parsed with 'htmtest/parselog.py'.

Design
========

Several changes were made to the i386 target to support TSX:

* New cpu type "Haswell". '-cpu Haswell' must be specified on the command line to QEMU to support TSX instructions.
* New common memory interface in 'target-i386/mem_wrap.h'. This is used to wrap all memory micro-operations in 
'target-i386/translate.c' to efficiently switch behavior when RTM mode is active.
* Changes to CPUX86State to support RTM and store register state.
* Simulate a configurable transaction cache with 64 byte cache lines.
* Instruction interleaving. To force as much contention as possible (and really stress the design) QEMU was modified to 
execute one emulated instruction at a time on processors in transactional mode before yielding to other 
emulated processors. This mode can be explicitly triggered using the dummy interrupt 'int $0xFF' and exited using the
dummy interrupt 'int $0xFE'. This can be disabled by undefining DEBUG_SINGLESTEP in 'target-i386/tsx.h'.

TODO
========

* Implement Hardware Lock Elision (HLE). Probably not that difficult given RTM is implemented.
* DONE: Configurable caches. Intel's SDE (http://software.intel.com/en-us/articles/intel-software-development-emulator) supports 
configurable cache sizes, defaulting to 32KB 8-way set associative. This should be implemented as well.
* Non-transactional memory access killing transactions. This seems to be possible in Intel's SDE, but this 
project does not yet support it.
* Better options to disable/enable DEBUG_SINGLESTEP mode.


 

