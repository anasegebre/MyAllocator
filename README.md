# MyAllocator

Implementation of **malloc** and **free** functions. It is a BiBoP allocator that uses a size-segregated heap and utilizes a 16-byte allignment. 

To compile the program, run the command:

$make

Then you can preload the program to other commands:

$LD_PRELOAD=./myallocator.so `<command of choice>`

Some examples of commands can be: **ls**, **vim**


Note: This program is built in a Linux environment and utilizes the file *malloc.h*, which may not be found in every machine as easily.
