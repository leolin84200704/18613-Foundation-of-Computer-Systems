/**
 * @file mm.c
 * @brief A 64-bit struct-based implicit free list memory allocator
 *
 * 15-213: Introduction to Computer Systems
 *
 * This memory allocator implements an array of free linked-list
 * that allows the program find a fit free memory space
 * without going through all the blocks in the memory.
 * Everytime a block is freed or initialized,
 * the free list will add the block that is available for allocation to the
 * first of the corresponding list. Whenever a free block is allocated,
 * coalesced, it will be removed from the list. Every block stores the
 * information including size and whether it is allocated in the header.
 *
 * @author Leo Lin <hungfanl@andrew.cmu.edu>
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/* Do not change the following! */

#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

/* You can change anything from here onward */

/*
 *****************************************************************************
 * If DEBUG is defined (such as when running mdriver-dbg), these macros      *
 * are enabled. You can use them to print debugging output and to check      *
 * contracts only in debug mode.                                             *
 *                                                                           *
 * Only debugging macros with names beginning "dbg_" are allowed.            *
 * You may not define any other macros having arguments.                     *
 *****************************************************************************
 */
#ifdef DEBUG
/* When DEBUG is defined, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(expr) assert(expr)
#define dbg_assert(expr) assert(expr)
#define dbg_ensures(expr) assert(expr)
#define dbg_printheap(...) print_heap(__VA_ARGS__)
#else
/* When DEBUG is not defined, no code gets generated for these */
/* The sizeof() hack is used to avoid "unused variable" warnings */
#define dbg_printf(...) (sizeof(__VA_ARGS__), -1)
#define dbg_requires(expr) (sizeof(expr), 1)
#define dbg_assert(expr) (sizeof(expr), 1)
#define dbg_ensures(expr) (sizeof(expr), 1)
#define dbg_printheap(...) ((void)sizeof(__VA_ARGS__))

#endif

/* Basic constants */

typedef uint64_t word_t;

/** @brief Word and header size (bytes) */
static const size_t wsize = sizeof(word_t);

/** @brief Double word size (bytes) */
static const size_t dsize = 2 * wsize;

/** @brief Minimum block size (bytes) */
static const size_t min_block_size = 2 * dsize;

/**
 * TODO: The size of heap increase every time we call sbrk(4096 bytes)
 * (Must be divisible by dsize)
 */
static const size_t chunksize = (1 << 12);

/**
 * TODO: A alloc_mask is used to & with a header to see if this block is freed
 * or allocated.
 */
static const word_t alloc_mask = 0x1;
// static int count = 0;
/**
 * TODO: To see if the previous block is allocated or not
 */
static const word_t pre_alloc_mask = 0x2;

/**
 * TODO: Since the size must be a multiplication of 16, the last 4 digit of the
 * size will be 0, So the size_mask is used to & with a header to check the size
 * of the block
 */
static const word_t size_mask = ~(word_t)0xF;

/** @brief Represents the header and payload of one block in the heap */
struct block {
    /** @brief Header contains size + allocation flag */
    word_t header;
    /**
     * @brief A pointer to the block payload.
     *
     * TODO: Besides a header, a block also contian a struct with a next
     * pointer, a pre pointer, and an array of char(payload) We no longer need
     * footer.
     */
    union {
        struct {
            struct block *next;
            struct block *pre;
        };
        char payload[0];
    };
};

typedef struct block block_t;

/* Global variables */

/** @brief Pointer to first block in the heap */
static const int group_count = 15;
static block_t *heap_start = NULL;
static block_t *list_start[group_count];
/*
 *****************************************************************************
 * The functions below are short wrapper functions to perform                *
 * bit manipulation, pointer arithmetic, and other helper operations.        *
 *                                                                           *
 * We've given you the function header comments for the functions below      *
 * to help you understand how this baseline code works.                      *
 *                                                                           *
 * Note that these function header comments are short since the functions    *
 * they are describing are short as well; you will need to provide           *
 * adequate details for the functions that you write yourself!               *
 *****************************************************************************
 */

/*
 * ---------------------------------------------------------------------------
 *                        BEGIN SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Returns the maximum of two integers.
 * @param[in] x
 * @param[in] y
 * @return `x` if `x > y`, and `y` otherwise.
 */
static size_t max(size_t x, size_t y) {
    return (x > y) ? x : y;
}

/**
 * @brief Rounds `size` up to next multiple of n
 * @param[in] size
 * @param[in] n
 * @return The size after rounding up
 */
static size_t round_up(size_t size, size_t n) {
    return n * ((size + (n - 1)) / n);
}

/**
 * @brief get the group that a freed block belongs to increase utilization
 * @param[in] size
 * @return The group that the block belongs
 */
static int calculate_group(size_t size) {
    if (size < 33)
        return 0;
    if (size < 49)
        return 1;
    if (size < 65)
        return 2;
    if (size < 81)
        return 3;
    if (size < 161)
        return 4;
    if (size < 321)
        return 5;
    if (size < 433)
        return 6;
    if (size < 641)
        return 7;
    if (size < 840)
        return 8;
    if (size < 1500)
        return 9;
    if (size < 2000)
        return 10;
    if (size < 3000)
        return 11;
    if (size < 5000)
        return 12;
    if (size < 10000)
        return 13;
    return 14;
}

/**
 * @brief Packs the `size` and `alloc` of a block into a word suitable for
 *        use as a packed value.
 *
 * Packed values are used for both headers and footers.
 *
 * The allocation status is packed into the lowest bit of the word.
 *
 * @param[in] size The size of the block being represented
 * @param[in] alloc True if the block is allocated
 * @return The packed value
 */
static word_t pack(size_t size, bool alloc) {
    word_t word = size;
    if (alloc) {
        word |= alloc_mask;
    }
    return word;
}

/**
 * @brief Extracts the size represented in a packed word.
 *
 * This function simply clears the lowest 4 bits of the word, as the heap
 * is 16-byte aligned.
 *
 * @param[in] word
 * @return The size of the block represented by the word
 */
static size_t extract_size(word_t word) {
    return (word & size_mask);
}

/**
 * @brief Extracts the size of a block from its header.
 * @param[in] block
 * @return The size of the block
 */
static size_t get_size(block_t *block) {
    return extract_size(block->header);
}

/**
 * @brief Given a payload pointer, returns a pointer to the corresponding
 *        block.
 * @param[in] bp A pointer to a block's payload
 * @return The corresponding block
 */
static block_t *payload_to_header(void *bp) {
    return (block_t *)((char *)bp - offsetof(block_t, payload));
}

/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        payload.
 * @param[in] block
 * @return A pointer to the block's payload
 * @pre The block must be a valid block, not a boundary tag.
 */
static void *header_to_payload(block_t *block) {
    dbg_requires(get_size(block) != 0);
    return (void *)(block->payload);
}

/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        footer.
 * @param[in] block
 * @return A pointer to the block's footer
 * @pre The block must be a valid block, not a boundary tag.
 */
static word_t *header_to_footer(block_t *block) {
    dbg_requires(get_size(block) != 0 &&
                 "Called header_to_footer on the epilogue block");
    return (word_t *)(block->payload + get_size(block) - dsize);
}

/**
 * @brief Given a block footer, returns a pointer to the corresponding
 *        header.
 * @param[in] footer A pointer to the block's footer
 * @return A pointer to the start of the block
 * @pre The footer must be the footer of a valid block, not a boundary tag.
 */
static block_t *footer_to_header(word_t *footer) {
    size_t size = extract_size(*footer);
    dbg_assert(size != 0 && "Called footer_to_header on the prologue block");
    return (block_t *)((char *)footer + wsize - size);
}

/**
 * @brief Returns the payload size of a given block.
 *
 * The payload size is equal to the entire block size minus the sizes of the
 * block's header and footer.
 *
 * @param[in] block
 * @return The size of the block's payload
 */
static size_t get_payload_size(block_t *block) {
    size_t asize = get_size(block);
    return asize - wsize;
}

/**
 * @brief Returns the allocation status of a given header value.
 *
 * This is based on the lowest bit of the header value.
 *
 * @param[in] word
 * @return The allocation status correpsonding to the word
 */
static bool extract_alloc(word_t word) {
    return (bool)(word & alloc_mask);
}

/**
 * @brief Returns the allocation status of a block, based on its header.
 * @param[in] block
 * @return The allocation status of the block
 */
static bool get_alloc(block_t *block) {
    return extract_alloc(block->header);
}

/**
 * @brief Returns the previous block allocation status of a given header value.
 *
 * This is based on the lowest bit of the header value.
 *
 * @param[in] word
 * @return The pre_allocation status correpsonding to the word
 */
static bool extract_pre_alloc(word_t word) {
    return (bool)(word & pre_alloc_mask);
}

/**
 * @brief Returns the previous block allocation status of a block, based on its
 * header.
 * @param[in] block
 * @return The pre_allocation status of the block
 */
static bool get_pre_alloc(block_t *block) {
    return extract_pre_alloc(block->header);
}

/**
 * @brief Remove a Node from the list.
 */
static void remove_from_list(block_t *block) {
    int i = calculate_group(get_size(block));
    if (block == list_start[i]) {
        // The block is the only Node in the list.
        if (block->next == NULL) {
            list_start[i] = NULL;
        }
        // There are other Nodes in the list.
        else {
            block->next->pre = NULL;
            list_start[i] = block->next;
            block->next = NULL;
        }
    } else {
        if (block->next == NULL) {
            block->pre->next = NULL;
            block->pre = NULL;
        } else {
            block->pre->next = block->next;
            block->next->pre = block->pre;
            block->pre = NULL;
            block->next = NULL;
        }
    }
}

/**
 * @brief Add the Node to the first Node of the List.
 *
 *
 */
static void add_to_first(block_t *block) {
    int i = calculate_group(get_size(block));
    if (list_start[i] == NULL) {
        block->next = NULL;
        list_start[i] = block;
    }

    else {
        block->next = list_start[i];
        list_start[i]->pre = block;
        list_start[i] = block;
    }
}

/**
 * @brief Writes an epilogue header at the given address.
 *
 * The epilogue header has size 0, and is marked as allocated.
 *
 * @param[out] block The location to write the epilogue header
 */
static void write_epilogue(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires((char *)block == mem_heap_hi() - 7);
    block->header = pack(0, true);
}

/**
 * @brief Writes a block starting at the given address.
 *
 * @param[out] block The location to begin writing the block header
 * @param[in] size The size of the new block
 * @param[in] alloc The allocation status of the new block
 */
static void write_block(block_t *block, size_t size, bool alloc) {
    dbg_requires(block != NULL);
    dbg_requires(size > 0);
    block->header = pack(size, alloc);
    word_t *footerp = header_to_footer(block);
    *footerp = pack(size, alloc);
}

/**
 * @brief Writes a block starting at the given address.
 *
 * TODO: Are there any preconditions or postconditions?
 *
 * @param[out] block The location to begin writing the block header
 * @param[in] size The size of the new block
 * @param[in] alloc The allocation status of the new block
 */
static void write_header(block_t *block, size_t size, bool alloc) {
    dbg_requires(block != NULL);
    dbg_requires(size > 0);
    block->header = pack(size, alloc);
}

/**
 * @brief Writes a block starting at the given address.
 *
 * @param[out] block The location to begin writing the block header
 * @param[in] size The size of the new block
 * @param[in] alloc The allocation status of the new block
 */
static void write_pre_alloc(block_t *block, bool pre_alloc) {
    size_t size = extract_size(block->header);
    bool alloc = extract_alloc(block->header);
    if (alloc) {
        size |= alloc_mask;
    }
    if (pre_alloc) {
        size |= pre_alloc_mask;
    }
    block->header = size;
    if (!alloc) {
        word_t *footerp = header_to_footer(block);
        *footerp = size;
    }
}

/**
 * @brief Finds the next consecutive block on the heap.
 *
 * This function accesses the next block in the "implicit list" of the heap
 * by adding the size of the block.
 *
 * @param[in] block A block in the heap
 * @return The next consecutive block on the heap
 * @pre The block is not the epilogue
 */
static block_t *find_next(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires(get_size(block) != 0 &&
                 "Called find_next on the last block in the heap");
    return (block_t *)((char *)block + get_size(block));
}

/**
 * @brief Finds the footer of the previous block on the heap.
 * @param[in] block A block in the heap
 * @return The location of the previous block's footer
 */
static word_t *find_prev_footer(block_t *block) {
    // Compute previous footer position as one word before the header
    return &(block->header) - 1;
}

/**
 * @brief Finds the previous consecutive block on the heap.
 * This is the previous block in the "implicit list" of the heap.
 *
 * If the function is called on the first block in the heap, NULL will be
 * returned, since the first block in the heap has no previous block!
 *
 * The position of the previous block is found by reading the previous
 * block's footer to determine its size, then calculating the start of the
 * previous block based on its size.
 *
 * @param[in] block A block in the heap
 * @return The previous consecutive block in the heap.
 */
static block_t *find_prev(block_t *block) {
    dbg_requires(block != NULL);
    word_t *footerp = find_prev_footer(block);

    // Return NULL if called on first block in the heap
    if (extract_size(*footerp) == 0) {
        return NULL;
    }

    return footer_to_header(footerp);
}

/*
 * ---------------------------------------------------------------------------
 *                        END SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/******** The remaining content below are helper and debug routines ********/

/**
 * @brief This function examine the previous block and the next block,
 * 1. Check if the next block is freed (get_alloc == false),
 *    True -> get the size of the next block (find_next(block) )and add to the
 * header False -> does nothing.
 * 2. Check if the previous block is freed by checking the header's pre_alloc
 * (get_pre_alloc == false), True -> get the size of the previous block and the
 * address of the previous header (footer_to_header(find_prev_footer(block)))
 * Change the header to the previous header and add the size of the current
 * block to the previous header False -> does nothing. Copy the information of
 * the header to the footer Return the block address.
 *
 * <Are there any preconditions or postconditions?>
 * Precondition: the argument block must be a freed block
 *
 * @param[in] block The address of a block so that the function can trace one
 * block before and after.
 * @return A pointer pointing to the header of a block.
 */
static block_t *coalesce_block(block_t *block) {
    // 1. Find if the next block is freed, combined the two blocks
    size_t block_size = get_size(block);
    block_t *next_block = find_next(block);
    bool pre_allocate = get_pre_alloc(block);
    if (!get_alloc(next_block)) {
        remove_from_list(next_block);
        block_size += get_size(next_block);
        write_block(block, block_size, false);
        write_pre_alloc(block, true);
        write_pre_alloc(find_next(block), false);
    }
    if (!pre_allocate) {
        block_t *pre_block = find_prev(block);
        remove_from_list(pre_block);
        block_size += get_size(pre_block);
        block = pre_block;
        write_block(block, block_size, false);
        // The pre_alloc of pre_block must be 1
        write_pre_alloc(block, true);
    }
    return block;
}

/**
 * @brief The function is called when initaizing (mm_init) and when there is not
 * enough space for a new request. The function requires a requested size, round
 * the size up to make sure it's a multiplicaiton of d_size, and then try to
 * extend the heap by the size. Fail -> Return null Succeed -> 1. Find the
 * header(the previous epilogue header), write the header with the size just
 * allocated and mark allocate as false.
 *            2. Write the epilogue.
 *            3. Try to coalesce_block with the previous block(The epilogue
 * block is marked as alloctekd so it will not be coalesced)
 *            4. Return the pointer to the header of the newly created block
 * The block should not have any unallocated block size bigger than the "size".
 *
 * @param[in] size The size of the required extend heap.
 * @return The pointer to the header of the newly created block if created
 * successfully Null if not.
 */
static block_t *extend_heap(size_t size) {
    void *bp;

    // Allocate an even number of words to maintain alignment
    size = round_up(size, dsize);
    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }
    // Initialize free block header/footer
    block_t *block = payload_to_header(bp);
    bool pre_allocate = get_pre_alloc(block);
    write_block(block, size, false);
    write_pre_alloc(block, pre_allocate);
    // Create new epilogue header
    block_t *block_next = find_next(block);
    write_epilogue(block_next);
    block = coalesce_block(block);
    add_to_first(block);
    return block;
}

/**
 * @brief This function split a block that to two smaller blocks under two
 * conditions
 * 1. The first block is equal to asize(the requested size)
 * 2. Both block is bigger than the min_block_size(2 * dsize)
 * After spliting, change the header and the footer of the two blocks.
 * Note: the first block is marked as allocted and the second block is marked as
 * not allocated.
 * This function will examine if the block can be split, so no precondition.
 * The post condition is written above.
 *
 * @param[in] block The block that needs to be split and the size to keep
 * @param[in] asize The size that is need for allocation
 */
static void split_block(block_t *block, size_t asize) {
    dbg_requires(get_alloc(block));

    size_t block_size = get_size(block);

    if ((block_size - asize) >= min_block_size) {
        block_t *block_next;
        bool pre_allocate = get_pre_alloc(block);
        write_header(block, asize, true);
        write_pre_alloc(block, pre_allocate);
        block_next = find_next(block);
        write_block(block_next, block_size - asize, false);
        write_pre_alloc(block_next, true);
        write_pre_alloc(find_next(block_next), false);
        add_to_first(block_next);
    }

    dbg_ensures(get_alloc(block));
}

/**
 * @brief Iterate through whole heap to find a block that is
 *  1. Freed
 *  2. Have a size bigger than the required size
 *  If find one -> return the header of the block.
 *  If can't find one -> return null (Call heap extension)
 * Pre -> None
 * Post -> The assigned block might be too big and required a split.
 *
 * @param[in] asize The required size
 * @return The address of the found block
 */
static block_t *find_fit(size_t asize) {
    int i;
    for (i = calculate_group(asize); i < group_count; i++) {
        block_t *cur_node = list_start[i];
        block_t *last_node = NULL;
        int j = 0;
        while (cur_node != NULL) {
            if (!(get_alloc(cur_node)) && (asize <= get_size(cur_node))) {
                j++;
                if (last_node == NULL ||
                    get_size(cur_node) < get_size(last_node)) {
                    last_node = cur_node;
                }
            }
            cur_node = cur_node->next;
            if (j == 7) {
                return last_node;
            }
        }
        if (last_node != NULL) {
            return last_node;
        }
    }

    return NULL; // no fit found
}

/**
 * @brief Check if the heap follow all the rule applied.
 * @param[in] line The line
 * @return false if any condition is not met
 */
bool mm_checkheap(int line) {
    // Check if the heap_start has been initialized
    if (heap_start == NULL)
        return false;
    block_t *cur_block = heap_start;
    bool pre_alloc = true;
    // Check the prologue
    word_t *pro = find_prev_footer(heap_start);
    if (extract_size(*pro) != 0 || !extract_alloc(*pro)) {
        printf("prologue wrong setting\n");
        return false;
    }

    while (get_size(cur_block) != 0) {
        // Check if header and footer matching each other.
        if (cur_block->header != *header_to_footer(cur_block)) {
            printf("header footer mismatch\n");
            return false;
        }
        cur_block = find_next(cur_block);
        // Check for the alignment of the payload
        if (round_up((size_t)header_to_payload(cur_block), dsize) !=
            (size_t)header_to_payload(cur_block)) {
            printf("payload not aligned\n");
            return false;
        }

        // Check coalescing:  no consecutive free blocks in the heap.
        if (!pre_alloc && get_alloc(cur_block)) {
            printf("consecutive free\n");
            return false;
        }

        // Check blocks lie within heap boundaries.
        if ((void *)cur_block > mem_heap_hi() ||

            (void *)cur_block < mem_heap_lo()) {
            printf("Seg fault\n");
            return false;
        }

        // Store the alloc information of the current block
        // pre_alloc = get_alloc(cur_block);
        cur_block = find_next(cur_block);
    }

    // Check epilogue
    if (!get_alloc(cur_block)) {
        printf("epi not alloc\n");
        return false;
    }

    // Check for circular LinkedList
    int i;
    for (i = 0; i < 9; i++) {
        block_t *slow_pointer = list_start[i];
        block_t *fast_pointer = list_start[i];
        while (slow_pointer != NULL && fast_pointer != NULL &&
               slow_pointer->next != NULL && fast_pointer->next != NULL) {
            slow_pointer = slow_pointer->next;
            fast_pointer = fast_pointer->next->next;
            if (slow_pointer == fast_pointer) {
                printf("Circular list\n");
                return false;
            }
        }
    }
    // Check if the ListNode is in the right group
    for (i = 0; i < group_count; i++) {
        block_t *pointer = list_start[i];
        while (pointer != NULL && pointer->next != NULL) {
            if (calculate_group(get_size(pointer)) != i) {
                printf("Wrong group of linkedlist\n");
                return false;
            }
            pointer = pointer->next;
        }
    }

    return true;
}

/**
 * @brief Initiate the heap by
 *  1. Getting a memory by sbrk(size of 2 word_t, one for prologue(size of 0 and
 * marked as allocated) the other as epilogue)
 *  2. extend the heap by a chunksize
 * Only when heap_start is null.
 * @return If the allocation of both start and heap extension succeed -> return
 * true Else -> return false
 */
bool mm_init(void) {
    // Create the initial empty heap
    word_t *start = (word_t *)(mem_sbrk(2 * wsize));
    if (start == (void *)-1) {
        return false;
    }
    for (int i = 0; i < group_count; i++) {
        list_start[i] = NULL;
    }
    /*
     * TODO: delete or replace this comment once you've thought about it.
     * Think about why we need a heap prologue and epilogue. Why do
     * they correspond to a block footer and header respectively?
     */

    start[0] = pack(0, true); // Heap prologue (block footer)
    start[1] = pack(0, true); // Heap epilogue (block header)
    start[1] = start[1] |= pre_alloc_mask;
    // Heap starts with first "block header", currently the epilogue
    heap_start = (block_t *)&(start[1]);
    // Extend the empty heap with a free block of chunksize bytes
    if (extend_heap(chunksize) == NULL) {
        return false;
    }
    return true;
}

/**
 * @brief
 * 1. Check if heap_start == NULL
 *    If true -> mm_init to initiate.
 * 2. If the required size == 0 return NULL
 * 3. Round up to make sure the size is a multiplication of 16 bytes, and find
 * blocks If the block is not found -> extend the heap and get the block's
 * pointer, if these is no more space -> Return NULL
 * 4. get the allocted block and write the header and the footer.
 * 5. Try to split the block if the block can be split
 * 6. Return the address of the payload
 *
 * @param[in] size The size that the user requires
 * @return The pointer to the payload for the user to write
 */
void *malloc(size_t size) {
    dbg_requires(mm_checkheap(__LINE__));
    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit is found
    block_t *block;
    void *bp = NULL;

    // Initialize heap if it isn't initialized
    if (heap_start == NULL) {
        mm_init();
    }

    // Ignore spurious request
    if (size == 0) {
        dbg_ensures(mm_checkheap(__LINE__));
        return bp;
    }

    // Adjust block size to include overhead and to meet alignment requirements
    asize = max(round_up(size + wsize, dsize), min_block_size);

    // Search the free list for a fit
    block = find_fit(asize);
    // If no fit is found, request more memory, and then and place the block
    if (block == NULL) {
        // Always request at least chunksize
        extendsize = max(asize, chunksize);
        block = extend_heap(extendsize);
        // extend_heap returns an error
        if (block == NULL) {
            return bp;
        }
    }
    // The block should be marked as free
    dbg_assert(!get_alloc(block));

    // Mark block as allocated
    size_t block_size = get_size(block);
    // write_block(block, block_size, true);
    bool pre_allocate = get_pre_alloc(block);
    write_header(block, block_size, true);
    write_pre_alloc(block, pre_allocate);
    write_pre_alloc(find_next(block), true);

    // Try to split the block if too large
    remove_from_list(block);
    split_block(block, asize);
    bp = header_to_payload(block);

    dbg_ensures(mm_checkheap(__LINE__));
    return bp;
}

/**
 * @brief Mark the block as freed and coalesce with the previous and next block.
 * The block must not be freed already.
 *
 * @param[in] bp A pointer that points to a starting point of a payload.
 */
void free(void *bp) {
    dbg_requires(mm_checkheap(__LINE__));

    if (bp == NULL) {
        return;
    }

    block_t *block = payload_to_header(bp);
    size_t size = get_size(block);

    // The block should be marked as allocated
    dbg_assert(get_alloc(block));

    // Mark the block as free
    bool pre_allocate = get_pre_alloc(block);
    write_block(block, size, false);
    write_pre_alloc(block, pre_allocate);
    write_pre_alloc(find_next(block), false);
    // Try to coalesce the block with its neighbors
    block = coalesce_block(block);
    add_to_first(block);
    dbg_ensures(mm_checkheap(__LINE__));
}

/**
 * @brief Reallocate the size of a allocated block.
 *  1. If the required size is 0 -> Same as freeing the block.
 *  2. If the ptr points to a null space, return the generic pointer generated
 * by malloc.
 *  3. If the malloc fails, return null and the original block is left
 * untouched.
 *  4. if the malloc is successful, copy the payload to the new allocated heap
 * and freed the original heap.
 *
 * @param[in] ptr A generic pointer that needs to be reallocated.
 * @param[in] size The size of the newly requested block.
 * @return The original pointer and the size that the user wants to re-allocate.
 */
void *realloc(void *ptr, size_t size) {

    block_t *block = payload_to_header(ptr);
    size_t copysize;
    void *newptr;

    // If size == 0, then free block and return NULL
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // If ptr is NULL, then equivalent to malloc
    if (ptr == NULL) {
        return malloc(size);
    }

    // Otherwise, proceed with reallocation
    newptr = malloc(size);

    // If malloc fails, the original block is left untouched
    if (newptr == NULL) {
        return NULL;
    }

    // Copy the old data
    copysize = get_payload_size(block); // gets size of old payload
    if (size < copysize) {
        copysize = size;
    }

    memcpy(newptr, ptr, copysize);

    // Free the old block
    free(ptr);

    return newptr;
}

/**
 * @brief Same as Malloc, but set all the payload bits to 0.
 *
 * @param[in] elements The size of the single element based on its datatype
 * @param[in] size The number of the variable.
 * @return A generic pointer.
 */
void *calloc(size_t elements, size_t size) {
    void *bp;
    size_t asize = elements * size;

    if (elements == 0) {
        return NULL;
    }
    if (asize / elements != size) {
        // Multiplication overflowed
        return NULL;
    }

    bp = malloc(asize);
    if (bp == NULL) {
        return NULL;
    }

    // Initialize all bits to 0
    memset(bp, 0, asize);

    return bp;
}

/*
 *****************************************************************************
 * Do not delete the following super-secret(tm) lines!                       *
 *                                                                           *
 * 53 6f 20 79 6f 75 27 72 65 20 74 72 79 69 6e 67 20 74 6f 20               *
 *                                                                           *
 * 66 69 67 75 72 65 20 6f 75 74 20 77 68 61 74 20 74 68 65 20               *
 * 68 65 78 61 64 65 63 69 6d 61 6c 20 64 69 67 69 74 73 20 64               *
 * 6f 2e 2e 2e 20 68 61 68 61 68 61 21 20 41 53 43 49 49 20 69               *
 *                                                                           *
 * 73 6e 27 74 20 74 68 65 20 72 69 67 68 74 20 65 6e 63 6f 64               *
 * 69 6e 67 21 20 4e 69 63 65 20 74 72 79 2c 20 74 68 6f 75 67               *
 * 68 21 20 2d 44 72 2e 20 45 76 69 6c 0a c5 7c fc 80 6e 57 0a               *
 *                                                                           *
 *****************************************************************************
 */
