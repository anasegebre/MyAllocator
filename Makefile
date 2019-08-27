CC := clang
CXX := clang++
CFLAGS := -g -Wall -Werror -fPIC

all: myallocator.so

clean:
	rm -rf obj myallocator.so

obj/allocator.o: allocator.c
	mkdir -p obj
	$(CC) $(CFLAGS) -c -o obj/allocator.o allocator.c

myallocator.so: gnuwrapper.cpp obj/allocator.o wrapper.h
	$(CXX) -shared $(CFLAGS) -o myallocator.so gnuwrapper.cpp obj/allocator.o
