#define _GNU_SOURCE
#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

// The minimum size returned by malloc
#define MIN_MALLOC_SIZE 16

// Round a value x up to the next multiple of y
#define ROUND_UP(x,y) ((x) % (y) == 0 ? (x) : (x) + ((y) - (x) % (y)))

// The size of a single page of memory, in bytes
#define PAGE_SIZE 0x1000

// Used to verify if it's a valid block from heap
#define MAGIC_NUMBER 0xCA75

// Struct to keep track of start and end of pages for each freelist element
typedef struct node {
  void* head;
  void* tail;
  void* next;
} node_t;

// Freelist struct
typedef struct freelist {
  long *header;
  long magic;
  size_t size;
  void **next;
  void* end;
  node_t* pages;
} freelist_t;

// Global flistarray of size 8 for each size class, from 16 to 2048
freelist_t flistarray[8];

freelist_t create_freelist(void* header, size_t size);
size_t xxmalloc_usable_size(void* ptr);

/**
 * Rounds up to the next size class
 * \param size The number of bytes that must be allocated
 * \returns The smallest multiple of 16 for param size, which is the class size
 */
size_t power_of_two(size_t size) {
  int rounder = MIN_MALLOC_SIZE;

  while(true) {
    if(size <= rounder){
      size = rounder;
      break;
    } else {
      rounder = rounder*2;
    }
  }
  return size;
}

/**
 * Allocate space on the heap.
 * \param size  The minimium number of bytes that must be allocated
 * \returns     A pointer to the beginning of the allocated space.
 *              This function may return NULL when an error occurs.
 */
void* xxmalloc(size_t size) {
  
  void* p = NULL;
    
  // Special case for large objects 
  if(size > PAGE_SIZE / 2) {
    // Round up large size to multiple of PAGE_SIZE
    size = ROUND_UP(size, PAGE_SIZE);
    // Create new mmaping for large size
    p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    return p;
  }
  
  // Round the size up to the next multiple of the page size
  size = power_of_two(size);
  

  int empty_freelist;
  
  // Loop through the freelists to find the correct class size
  for(int i = 0; i < 8; i++) {
    // If we find the correct size freelist, then flistarray[i].next points to the next element
    if(flistarray[i].size == size) {  
      p = flistarray[i].next;
      flistarray[i].next = flistarray[i].next+size;
      // Traverse to the end of flistarray and then mmap with a size of PAGE_SIZE
      if(flistarray[i].next == flistarray[i].end){
        flistarray[i].next = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        // Append new mapping to the end of flistarray
        flistarray[i].end = flistarray[i].next+PAGE_SIZE;


        // Set MAGIC_NUMBER in header
        *flistarray[i].header = MAGIC_NUMBER;
        flistarray[i].header++;
        // Set size in header
        *flistarray[i].header = flistarray[i].size;
        flistarray[i].header--;

        // Create new node
        node_t newnode = {
          flistarray[i].next, flistarray[i].end, flistarray[i].pages
        };

        // Add new node to pages to keep track of head and tail of flistarray[i]
        flistarray[i].pages = &newnode;
           
      }
      return p;

      // If freelist of this size does not exist, then we record this as empty_freelist
    } else if (flistarray[i].size == 0) {
      empty_freelist = i;
    }
  }

  // Create new mapping of size PAGE_SIZE
  void* newHeader =  mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  // Create a new freelist for this size
  flistarray[empty_freelist] = create_freelist(newHeader, size);
  p = flistarray[empty_freelist].next;
  
  // Set MAGIC_NUMBER in header
  *flistarray[empty_freelist].header = MAGIC_NUMBER;
  flistarray[empty_freelist].header++;
  // Set size in header
  *flistarray[empty_freelist].header = flistarray[empty_freelist].size;
  flistarray[empty_freelist].header--;

  // Newly created freelist points to the next elements in the freelist
  flistarray[empty_freelist].next = flistarray[empty_freelist].next+size;
  return p;
}

/**
 * Initializes freelist
 * \param header This is the start of the freelist
 * \param size   The size of object
 * \returns      The newly created freelist for size
 */
freelist_t create_freelist(void* header, size_t size) {
  
  node_t node = {
    header,
    header+PAGE_SIZE,
    NULL
  };
  
  freelist_t flist = {
    header,
    MAGIC_NUMBER,
    power_of_two(size),
    header+power_of_two(size),
    header+PAGE_SIZE,
    &node
  };
  
  return flist;
}

/**
 * Free space occupied by a heap object.
 * \param ptr   A pointer somewhere inside the object that is being freed
 */
void xxfree(void* ptr) {
  
  // If pointer is NULL, don't free it, return
  if(ptr == NULL) return;

  // Find the size that is being freed
  size_t size = xxmalloc_usable_size(ptr);

  // If it is a large object, return
  if(size > PAGE_SIZE) return;
  
  // If we're outside the area malloc'd, return
  if(size == 0) return;

  freelist_t cur;
  int j;
  
  // Find the corresponding sized freelist
  for(int i = 0; i < 8; i++) {
    if(size == flistarray[i].size) {
      cur = flistarray[i];
      j = i;
    }
  }

  // Find where the beginning of the freelist where ptr resides
  void* beginning = (void*) ROUND_UP((intptr_t) ptr, size) - size;

  // Create node of freed space
  node_t new_node = {
    beginning,
    beginning+size,
    cur.next
  };

  // Add node to flistarray[j] which is the freelist for ptr's class size 
  flistarray[j].pages = &new_node;

  // Update next element of freelist to the beginning of ptr's freelist
  void* address = flistarray[j].next;
  flistarray[j].next = beginning;
  *flistarray[j].next = address;
}

/**
 * Get the available size of an allocated object
 * \param ptr   A pointer somewhere inside the allocated object
 * \returns     The number of bytes available for use in this object
 */
size_t xxmalloc_usable_size(void* ptr) {
  intptr_t pointer = (intptr_t) ptr;

  // If pointer is NULL, return 1
  if (ptr == NULL) {
    return 1; // if 0 is returned, malloc-test will do an invalid operation on 0 resulting in a floating point exception
  } else {
    // Find the header of ptr
    intptr_t header = pointer - (pointer % PAGE_SIZE);
    long * header_ptr = (long *) header;
    
    // If MAGIC_NUMBER matches, then return the size stored in header
    if(*header_ptr == MAGIC_NUMBER) {
      header_ptr++;
      return *header_ptr;
    }
    // If ptr does not fall within a block, then MAGIC_NUMBER is never found, return 1
    return 1;
  }
}