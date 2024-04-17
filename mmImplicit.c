/*
 * mm-Implicit.c - The file contains the implementation of a memory manager
 *                 implemented using implicit lists.  Your job is to implement
 *                 the two missing fit functions: next_fit and best_fit.
 *                 In addition, implementing the next_fit function will require that you
 *                 make a change to the coalesce function.
 *
 * Student(s) put your names here.
 *
 * name: Justin Pretlow
 *
 * name: Nick Monk
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mmImplicit.h"
#include "memlib.h"
/*
 * Each block has header and footer of the form:
 *
 *      31                     3  2  1  0
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      -----------------------------------
 *
 * where s are the meaningful size bits and a/f is set
 * if the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap
 *  -----------------------------------------------------------------
 * |  pad   | hdr(8/1) | ftr(8/1) | zero or more usr blks | hdr(0/1) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 *
 * The code targets an X86 32-bit machine, which means double word
 * alignment.  Each block is a multiple of 8 bytes in size and the
 * address of the payload is a multiple of 8.  Headers and footers
 * are 4 bytes each.
 */

// MACROS
#define WSIZE 4 // size of header and footer
#define DSIZE 8 // used for alignment
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// create a header or footer by ORing the size and allocation bit
#define PACK(size, alloc) ((size) | (alloc))

// get the word (i.e., unsigned int) stored in address p
#define GET(p) (*(unsigned int *)(p))
// store a word in memory at address p
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// size is going to be a multiple of 8 so ignore the lower
// three bits when getting the size out of the header or footer
#define GET_SIZE(p) (GET(p) & ~0x7)

// get the allocation bit out of the header or footer
#define GET_ALLOC(p) (GET(p) & 0x1)

// bp is the address of the payload
// HDRP returns the address of the header, which starts four bytes before payload
#define HDRP(bp) ((char *)(bp)-WSIZE)
// FTRP returns the address of the footer; uses size in header to calc address
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// bp is the pointer to the payload
// NEXT_BLKP returns a pointer to the payload of the next block
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
// PREV_BLKP returns a pointer to the payload of the previous block
// accesses the footer in the previous block to get the size of that block
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

// Memory Pointers
// Points to the beginning of the heap (payload/footer of prologue block)
static char *heap_listp;
// Points to the block after the last allocated block; used for next fit
static char *current;

// Helper Functions
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *first_fit(size_t asize);
static void *next_fit(size_t asize);
static void *best_fit(size_t asize);
static void place(void *bp, size_t asize);

/* which placement technique to use */
/* default is first fit */
int whichfit = FIRSTFIT;

/*
 * mm_init - initialize the malloc package for implict list
 *           allocation. Specifically, initialize the heap
 *           to contain an allocated block with no payload (prologue),
 *           followed by a big free block, followed by an allocated
 *           block with no footer and no size (epilogue). The prologue and epilogue
 *           blocks make the coalescing logic simpler.
 */
int mm_init(void)
{
	if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
		return -1;

	PUT(heap_listp, 0);							   // Padding
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue header
	PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // Prologue footer
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));	   // Epilogue header

	// Make heap_listp point to payload/footer of Prologue block
	// This enables the code to use the header of the Prologue block
	// to find the next block.
	heap_listp += (2 * WSIZE);

	current = NEXT_BLKP(heap_listp); // for next fit placement

	// now add a big free block
	if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
		return -1;

	return 0;
}

/*
 * mm_malloc - Finds a free block of size bytes using the
 *     placement policy indicated by whichfit.
 *     If no block can be found, extend the heap.
 *
 */
void *mm_malloc(size_t size)
{
	size_t asize;
	size_t extendsize;
	char *bp;

	if (size == 0)
		return NULL;

	// Adjust the size to make room for the header and
	// footer and adhere to the alignment restrictions.
	// The block must be a size that is a multiple of DSIZE
	if (size <= DSIZE)
	{
		// minimum block size is 2*DSIZE
		asize = 2 * DSIZE;
	}
	else
	{
		// size must be a multiple of DSIZE
		asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
	}

	// Search free list for fit.
	if (whichfit == BESTFIT)
		bp = best_fit(asize);
	else if (whichfit == NEXTFIT)
		bp = next_fit(asize);
	else
		bp = first_fit(asize); // default

	// If a free block was found then use it
	if (bp != NULL)
	{
		place(bp, asize);
		current = NEXT_BLKP(bp); // for next fit placement
		return bp;
	}

	// No free block found, extend the heap
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
	{
		return NULL;
	}

	// allocate the block
	place(bp, asize);
	current = NEXT_BLKP(bp); // for next fit placement
	return bp;
}

/*
 * mm_free - Free the block and coalesce it with adjacent free blocks.
 *           ptr points to the payload of the block to be free.
 */
void mm_free(void *ptr)
{
	size_t size = GET_SIZE(HDRP(ptr));

	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));

	coalesce(ptr);
}

/*
 * mm_realloc - Allocate a new block of size bytes.  Copy the contents
 *              of the block pointed to by ptr into that block.
 *              Returns a pointer to the payload of the new block.
 */
void *mm_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
	{
		return mm_malloc(size);
	}
	if (size == 0)
	{
		mm_free(ptr);
		return NULL;
	}
	// See if block is already big enough
	// need room for payload of size bytes, header, and footer
	if (GET_SIZE(HDRP(ptr)) >= (size + WSIZE + WSIZE))
	{
		return ptr;
	}

	void *oldptr = ptr;
	void *newptr = mm_malloc(size);

	// if malloc fails, give up
	if (newptr == NULL)
		return NULL;

	size_t copySize = GET_SIZE(HDRP(oldptr));

	// if the original block is bigger then
	// only copy what will fit into the new block
	if (size < copySize)
		copySize = size;

	// Copy the contents of the old block into
	// the new block
	memcpy(newptr, oldptr, copySize);
	// Free the old block
	mm_free(oldptr);

	return newptr;
}

/*
 * extend_heap - extends the size of the heap by words * WSIZE bytes
 *
 */
static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;

	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	if (((long)(bp = mem_sbrk(size)) == -1))
		return NULL;

	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size, 0));
	// add an epilogue at the end
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

	return coalesce(bp);
}

/*
 * coalesce - coalesces two or more blocks.
 *            bp points to the payload of the free block.
 *            Tries to coalesce that block with the one before
 *            it and the one after it.
 *            Returns a pointer to the possibly bigger free block.
 */
static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	// case 1
	if (prev_alloc && next_alloc)
	{
		return bp;
	}
	// previous block is allocated and the next is free : case 2
	else if (prev_alloc && !next_alloc)
	{
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	// previous block is free and the next is allocated : case 3
	else if (!prev_alloc && next_alloc)
	{
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}
	// previous block is free and the next is free : case 4
	else if (!prev_alloc && !next_alloc)
	{
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		bp = PREV_BLKP(bp);
	}

	/*
	   TODO

	   A problem that can occur with the next fit policy
	   is that after coalescing, the current pointer points
	   to the middle of the block pointed to by bp.

	   Add code that looks for that.

	   If current points to the middle of the block then set
	   it to be equal to bp.

	   (Don't just always set it to be. That is incorrect.)

	   Note: current is a global variable.
	*/

	return bp;
}

/*
 * first_fit - Starts at the beginning of the list and returns the
 *             first free block with a size that is greater than or
 *             equal to asize.
 *             Returns a pointer to the found block or NULL if no
 *             block fits.
 */
static void *first_fit(size_t asize)
{
	void *bp;
	// start at the beginning of heap
	// use size in the header to calculate the address of the next block
	// when size is 0 then the epilogue block has been reached
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
	{
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
		{
			return bp;
		}
	}

	return NULL;
}

/*
 * next_fit - Starts right after the last block that was allocated and searches
 *            in the list for the next block with a size that is greater than
 *            or equal to asize.
 *            Assumes that current points to the block after the last block
 *            that was allocated.
 *            Returns a pointer to the found block or NULL if no
 *            block fits.
 *            Note: current isn't modified by this function.
 */
static void *next_fit(size_t asize)
{

	// TODO
	// Replace this statement by an implementation of the next_fit
	// algorithm.
	return first_fit(asize);
	//
	// current points to the block after the last block allocated.
	// Start looking through the blocks starting from there. If you find
	// a large enough block, return it.
	//
	// If you don't find a large enough block in the first loop, then
	// control should enter your second loop that will start at the
	// beginning of the heap.  Don't loop all the way to the end; just
	// go to current in the second loop.
	//
	// If you don't find a large enough block in the second loop then
	// return NULL.
	//
	// Don't modify current here.  It is modified by mm_malloc.
}

/*
 * best_fit - Looks for a block of size asize bytes by going through all of
 *            the blocks and returning the one that is smallest and also
 *            greater than or equal to asize.
 */
static void *best_fit(size_t asize)
{

	// TODO
	// Replace this statement by an implementation of the best_fit
	// algorithm.
	return first_fit(asize);

	// This code needs to loop through all of the blocks in the
	// heap and find and return a free one that fits and is the smallest
	// block among all of the blocks that are free and fit.
	//
	// If no block can be found that fits then return NULL.
}

/*
 * place - Adds a header and footer to allocated block. If the block
 *         is larger than needed and the leftover portion is at
 *         least the minimum block size then mark the remaining portion
 *         as a free block.
 */
static void place(void *bp, size_t asize)
{
	// get the size of the free block
	size_t csize = GET_SIZE(HDRP(bp));

	// if the unused portion is at least 2*DSIZE
	// then split the block into two
	if ((csize - asize) >= (2 * DSIZE))
	{
		// add the header and footer to the allocated block
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		bp = NEXT_BLKP(bp);
		// add the header and footer to the unallocated block
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
	}
	else
	{
		// use the entire block
		// add the header and footer
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}

/*
 * printBlocks - Prints the heap, block by block.  This is useful for debugging.
 *               This is used with the implicitTester program.
 */
void printBlocks()
{
	char *bp;
	printf("%10s %10s %1s\n", "Addr", "Size", "a");
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
	{
		printf("%#10x %#10x %d\n", (unsigned int)bp, GET_SIZE(HDRP(bp)),
			   GET_ALLOC(HDRP(bp)));
	}
}
