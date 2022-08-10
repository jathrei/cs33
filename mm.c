/*
 * mm.c -  Simple allocator based on implicit free lists,
 *         first fit placement, and boundary tag coalescing.
 *
 * Each block has header and footer of the form:
 *
 *      63       32   31        1   0
 *      --------------------------------
 *     |   unused   | block_size | a/f |
 *      --------------------------------
 *
 * a/f is 1 iff the block is allocated. The list has the following form:
 *
 * begin                                       end
 * heap                                       heap
 *  ----------------------------------------------
 * | hdr(8:a) | zero or more usr blks | hdr(0:a) |
 *  ----------------------------------------------
 * | prologue |                       | epilogue |
 * | block    |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */
#include "memlib.h"
#include "mm.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Your info */
team_t team = {
    /* First and last name */
    "implicit first fit",
    /* UID */
    "123456789",
    /* Custom message (16 chars) */
    "",
};

typedef struct {
    uint32_t allocated : 1;
    uint32_t block_size : 31;
    uint32_t _;
} header_t;

typedef header_t footer_t;

typedef struct {
    uint32_t allocated : 1;
    uint32_t block_size : 31;
    uint32_t _;
    union {
        struct {
            struct block_t* next;
            struct block_t* prev;
        };
        int payload[0];
    } body;
} block_t;

/* This enum can be used to set the allocated bit in the block */
enum block_state { FREE,
                   ALLOC };

#define CHUNKSIZE (1 << 16) /* initial heap size (bytes) */
#define OVERHEAD (2*HEADER_SIZE) /* overhead of the header and footer of an allocated block */
#define MIN_BLOCK_SIZE (32) /* the minimum block size needed to keep in a freelist (header + footer + next pointer + prev pointer) */
#define HEADER_SIZE (sizeof(header_t))
#define BLOCK_TSIZE (sizeof(block_t))

/* Global variables */
static block_t *free_list_startp; /* pointer to free list start */

/* function prototypes for internal helper routines */
static block_t *extend_heap(size_t words);
static void place(block_t *block, size_t asize);
static block_t *find_fit(size_t asize);
static block_t *coalesce(block_t *block);
static footer_t *get_footer(block_t *block);
static void printblock(block_t *block);
static void checkblock(block_t *block);
static void add_2list(block_t *block);
static void remove_from_list(block_t *block);

/*
 * mm_init - Initialize the memory manager
 */
/* $begin mminit */
int mm_init(void) {
    /* create the initial empty heap */

    //changed from chunksize to headersize + minblocksize + footersize
    block_t *heap_startp;
    if ((heap_startp = mem_sbrk(CHUNKSIZE)) == (void*)-1) //maybe add MIN_BLOCK_SIZE
        return -1;
    /* initialize the prologue */
    heap_startp->allocated = ALLOC;
    heap_startp->block_size = HEADER_SIZE;
    /* initialize the first free block */

    //LETS TRY THIS
    //start free list block
    //initialize free list block
    free_list_startp = (void *)heap_startp + HEADER_SIZE;
    //block_t *init_block = (void *)prologue + sizeof(header_t);
    free_list_startp->allocated = ALLOC;
    //init_block->allocated = FREE;
    free_list_startp->block_size = (MIN_BLOCK_SIZE);
    footer_t *init_free_list_footer = get_footer(free_list_startp);
    init_free_list_footer->allocated = ALLOC;
    init_free_list_footer->block_size = free_list_startp->block_size;

    //create first free block
    block_t *init_block = (void *)free_list_startp + MIN_BLOCK_SIZE;
    init_block->allocated = FREE;
    init_block->block_size = ((CHUNKSIZE - OVERHEAD)-MIN_BLOCK_SIZE);
    footer_t *init_footer = get_footer(init_block);
    init_footer->allocated = FREE;
    init_footer->block_size = init_block->block_size;

    //set pointers
    free_list_startp->body.prev = NULL;
    free_list_startp->body.next = (void *)init_block;
    init_block->body.prev = (void *)free_list_startp;
    init_block->body.next = NULL;

    //maybe remember min_bloxk_szie is actulaly wrong its 24, so maybe CHANGE
    //that later to check efficiency

    /* initialize the epilogue - block size 0 will be used as a terminating condition */
    header_t *epilogue = (void *)init_block + init_block->block_size;
    //block_t *epilogue = (void *)init_block + init_block->block_size;
    //block + blocksize goes to next block
    epilogue->allocated = ALLOC;
    epilogue->block_size = 0;

    return 0;
}
/* $end mminit */

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size) {
    uint32_t asize;       /* adjusted block size */
    uint32_t extendsize;  /* amount to extend heap if no fit */
    //uint32_t extendwords; /* number of words to extend heap if no fit */
    block_t *block;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    size += OVERHEAD;

    asize = ((size + 7) >> 3) << 3; /* align to multiple of 8 */

    if (asize < MIN_BLOCK_SIZE) {
        asize = MIN_BLOCK_SIZE;
    }

    /* Search the free list for a fit */
    //maybe get rid of the NULL comparison, if it exists itll run ->redundant
    //OPTMIZATION HERE??????
    if ((block = find_fit(asize)) != NULL) {
        place(block, asize);
        return block->body.payload;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = (asize > CHUNKSIZE) // extend by the larger of the two
                     ? asize
                     : CHUNKSIZE;
    //extendwords = extendsize >> 3; // extendsize/8
    //redudnan? if it exists wont it run, dont need to compare to null
    //CEHCK THAT LATER ^
    if ((block = extend_heap(extendsize >> 3)) != NULL) {
        place(block, asize);
        return block->body.payload;
    }
    /* no more memory :( */
    return NULL;
}
/* $end mmmalloc */

/*
 * mm_free - Free a block
 */
/* $begin mmfree */
void mm_free(void *payload) {
    block_t *block = payload - HEADER_SIZE;
    block->allocated = FREE;
    footer_t *footer = get_footer(block);
    footer->allocated = FREE;
    //dont include add to list because its included in the coalesce
    coalesce(block);
}

/* $end mmfree */

/*
 * mm_realloc - naive implementation of mm_realloc
 * NO NEED TO CHANGE THIS CODE!
 */
void *mm_realloc(void *ptr, size_t size) {
    void *newp;
    size_t copySize;

    if ((newp = mm_malloc(size)) == NULL) {
        printf("ERROR: mm_malloc failed in mm_realloc\n");
        exit(1);
    }
    block_t* block = ptr - HEADER_SIZE;
    copySize = block->block_size;
    if (size < copySize)
        copySize = size;
    memcpy(newp, ptr, copySize);
    mm_free(ptr);
    return newp;
}


//ADDS THE BLOCK TO THE FREE LIST BRUH
static void add_2list(block_t *block){

    block_t *front = (void *)free_list_startp;

    //start of list
    if(front->body.next == NULL){
      block->body.prev = (void *)front;
      block->body.next = NULL;
      front->body.next = (void *)block;
    }
    else{
      block_t *temp = (void *)front;
      temp = temp->body.next;
      temp->body.prev = (void *)block;
      block->body.next = (void *)temp;
      front->body.next = (void *)block;
      block->body.prev = (void *)front;
    }

}

static void remove_from_list(block_t *block){

    block_t *prev_p = block->body.prev;
    block_t *next_p = block->body.next;

    //EDGE CASE?? maybe see if theres ever a condition in which the alllocated f_p
    //is trying to be removed from the list
    if(next_p == NULL){ //the block being removed is at the front of the list
      prev_p->body.next = NULL;
      block->body.prev = NULL;
    }
    else{
      prev_p->body.next = (void *)next_p;
      next_p->body.prev = (void *)prev_p;
      block->body.prev = NULL;
      block->body.next = NULL;
    }

}


/*
 * mm_checkheap - Check the heap for consistency
 */
void mm_checkheap(int verbose) {
    block_t *block = (void *)free_list_startp - HEADER_SIZE;

    if (verbose)
        printf("Heap (%p):\n", block);

    if (block->block_size != HEADER_SIZE || !block->allocated)
        printf("Bad prologue header\n");
    checkblock(block);

    /* iterate through the heap (both free and allocated blocks will be present) */
    for (block = (void*)free_list_startp; block->block_size > 0; block = (void *)block + block->block_size) {
        if (verbose)
            printblock(block);
        checkblock(block);
    }

    if (verbose)
        printblock(block);
    if (block->block_size != 0 || !block->allocated)
        printf("Bad epilogue header\n");
}

/* The remaining routines are internal helper routines */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static block_t *extend_heap(size_t words) {
    block_t *block;
    uint32_t size;
    size = words << 3; // words*8
    if (size == 0 || (block = mem_sbrk(size)) == (void *)-1)
        return NULL;
    /* The newly acquired region will start directly after the epilogue block */
    /* Initialize free block header/footer and the new epilogue header */
    /* use old epilogue as new free block header */
    block = (void *)block - HEADER_SIZE;
    block->allocated = FREE;
    block->block_size = size;
    /* free block footer */
    footer_t *block_footer = get_footer(block);
    block_footer->allocated = FREE;
    block_footer->block_size = block->block_size;
    /* new epilogue header */
    header_t *new_epilogue = (void *)block_footer + HEADER_SIZE;
    new_epilogue->allocated = ALLOC;
    new_epilogue->block_size = 0;
    /* Coalesce if the previous block was free */
    return coalesce(block);
}
/* $end mmextendheap */

/*
 * place - Place block of asize bytes at start of free block block
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
static void place(block_t *block, size_t asize) {
    size_t split_size = block->block_size - asize;
    if (split_size >= MIN_BLOCK_SIZE) {
        /* split the block by updating the header and marking it allocated*/
        block->block_size = asize;
        block->allocated = ALLOC;
        /* set footer of allocated block*/
        footer_t *footer = get_footer(block);
        footer->block_size = asize;
        footer->allocated = ALLOC;

        //remove from free list
        remove_from_list(block);

        /* update the header of the new free block */
        //TRY LATER TO SEE IF FASTER, SHIFT BLOCK BY BLOCKSIZE instead
        //of making new_block, then after add to free list, shift it back
        block_t *new_block = (void *)block + block->block_size;
        new_block->block_size = split_size;
        new_block->allocated = FREE;
        /* update the footer of the new free block */
        footer_t *new_footer = get_footer(new_block);
        new_footer->block_size = split_size;
        new_footer->allocated = FREE;

        //add to free list
        add_2list(new_block);

    } else {
        /* splitting the block will cause a splinter so we just include it in the allocated block */
        block->allocated = ALLOC;
        footer_t *footer = get_footer(block);
        footer->allocated = ALLOC;

        //remove from free list
        remove_from_list(block);
    }
}
/* $end mmplace */

/*
 * find_fit - Find a fit for a block with asize bytes
 * explicit free list
 */
static block_t *find_fit(size_t asize) {
    /* first fit search */
    block_t *b;


    //NOTE - MAYBE CHANGE HOW THIS IS DONE? from seg list idea
    for (b = (void*)free_list_startp; b != NULL; b = b->body.next) {
        /* block must be free and the size must be large enough to hold the request */
        //EVENTUALLY REMOVE THIS ALLOCATION CHECKER BECAUSE ITLL BE A LIST OF FREE ONLY
        //ONLY FIRST IN FREE LIST IS ALLOCATED

        //removed above condition ^^ really should be fine to take out the allocated check
        if (!b->allocated && asize <= b->block_size) {
            return b;
        }
    }
    return NULL; /* no fit */
}

/*
 * coalesce - boundary tag coalescing. Return ptr to coalesced block
 */
static block_t *coalesce(block_t *block) {
    footer_t *prev_footer = (void *)block - HEADER_SIZE;
    header_t *next_header = (void *)block + block->block_size;
    bool prev_alloc = prev_footer->allocated;
    bool next_alloc = next_header->allocated;

    if (prev_alloc && next_alloc) { /* Case 1 */
        /* no coalesceing */
        add_2list(block);
        return block;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        /* Update header of current block to include next block's size */
        //current block already says free header mate
        //remove next from list
        block_t *next_block = (void *)next_header;
        remove_from_list(next_block);

        block->block_size += next_block->block_size;
        /* Update footer of next block to reflect new size */
        footer_t *next_footer = get_footer(block);
        next_footer->block_size = block->block_size;
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        //remove previous from list
        block_t *prev_block = (void *)prev_footer - prev_footer->block_size + HEADER_SIZE;
        remove_from_list(prev_block);
        /* Update header of prev block to include current block's size */
        prev_block->block_size += block->block_size;
        /* Update footer of current block to reflect new size */
        footer_t *footer = get_footer(prev_block);
        footer->block_size = prev_block->block_size;
        block = prev_block;
    }

    else { /* Case 4 */
        /* Update header of prev block to include current and next block's size */
        //remove from lost prev and next
        block_t *prev_block = (void *)prev_footer - prev_footer->block_size + HEADER_SIZE;
        remove_from_list(prev_block);
        block_t *next_block = (void *)next_header;
        remove_from_list(next_block);

        prev_block->block_size += block->block_size + next_header->block_size;
        /* Update footer of next block to reflect new size */
        footer_t *next_footer = get_footer(prev_block);
        next_footer->block_size = prev_block->block_size;
        block = prev_block;
    }

    add_2list(block);
    return block;
}

static footer_t* get_footer(block_t *block) {
    return (void*)block + block->block_size - sizeof(footer_t);
}

static void printblock(block_t *block) {
    uint32_t hsize, halloc, fsize, falloc;

    hsize = block->block_size;
    halloc = block->allocated;
    footer_t *footer = get_footer(block);
    fsize = footer->block_size;
    falloc = footer->allocated;

    if (hsize == 0) {
        printf("%p: EOL\n", block);
        return;
    }

    printf("%p: header: [%d:%c] footer: [%d:%c]\n", block, hsize,
           (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
}

static void checkblock(block_t *block) {
    if ((uint64_t)block->body.payload % 8) {
        printf("Error: payload for block at %p is not aligned\n", block);
    }
    footer_t *footer = get_footer(block);
    if (block->block_size != footer->block_size) {
        printf("Error: header does not match footer\n");
    }
}
