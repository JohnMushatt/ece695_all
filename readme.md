Lab3: John Mushatt

For all parts: set .sh scripts to be executable and run them in the order of the questions

To build:
./compile_os.sh <- OS compile


Notes for TA:
Dyanmic heap management logic consumes about 21 bytes per node, with 2048 nodes allocated per PCB this consumes 42KB of memory.
After OS is done allcoated all PCB pages, 128 pages are left for the heap/stack which equats to 128*4KB = 512KB of memory left for heap.
That leaves about 512KB/64KB = 8 processes can utilize their full heap.

File Modifications:
    One-Level:
        - memory.c/h
        - memory_constants.h
        - process.c/h
        - traps.c
        - usertraps.s
    Fork:
        - memory.c/h
        - memory_constants.h
        - process.c/h
        - traps.c
        - usertraps.s
    Heap-Management:
        - memory.c/h
        - memory_constants.h
        - process.c/h
        - traps.c
        - usertraps.s
    Heap-Management-Dynamic:
        - memory.c/h
        - memory_constants.h
        - process.c/h
        - traps.c
        - usertraps.s