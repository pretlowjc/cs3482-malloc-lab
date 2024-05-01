/*
 * mmExplicit.c - The file contains the implementation of a memory manager
 *                implemented using explicit lists.  Your job is to implement
 *                the following functions:
 *                removeBlock
 *                insertInFront
 *                coalesce
 *
 *                For extra credit:
 *                next fit
 *                best fit
 *
 * Student(s) put your names here.
 *
 * name: Nickolos Monk
 *
 * name: Justin Pretlow
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mmExplicit.h"
#include "memlib.h"
/*
* Each allocated block has the form:
* 
*      31                     3  2  1  0 
*      ------------------------------------
*     | s  s  s  s  ... s  s  s  0  0  1   |
*      ------------------------------------
*     |                                    |
*     |     payload and padding            |
*     |                                    |
*      ------------------------------------
*     | s  s  s  s  ... s  s  s  0  0  1   |
*      ------------------------------------
* 
* where s are the meaningful size bits.
*
* If the block is empty, it has pointers to the
* previous free block and the successor free blocks.
*
*      31                     3  2  1  0 
*      ------------------------------------
*     | s  s  s  s  ... s  s  s  0  0  0   |
*      ------------------------------------
*     |        predecessor free block      |
*      ------------------------------------
*     |        successor free block        |
*      ------------------------------------
*     |                                    |
*     |                                    |
*      ------------------------------------
*     | s  s  s  s  ... s  s  s  0  0  0   |
*      ------------------------------------
*
* In addition, the heap is organized in the following way:
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

//MACROS
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

//create a header or footer by ORing the size and allocation bit
#define PACK(size, alloc) ((size) | (alloc))

//get the word stored in address p
#define GET(p) (*(unsigned int *)(p))
//store a word in memory at address p
#define PUT(p, val) (*(unsigned int *)(p) = (val))

//size is going to be a multiple of 8 so ignore the lower
//three bits when getting the size out of the header or footer
#define GET_SIZE(p) (GET(p) & ~0x7)

//get the allocation bit out of the header or footer
#define GET_ALLOC(p) (GET(p) & 0x1)

//bp is the address of the payload
//HDRP returns the address of the header
#define HDRP(bp) ((char *)(bp) - WSIZE)
//FTRP returns the address of the footer
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
//PRED returns the address of the pred field
#define PRED(bp) ((char *)(bp))
//SUCC returns the address of the succ field
#define SUCC(bp) ((char *)(bp) + WSIZE)

//bp is the pointer to the payload
//NEXT_BLKP returns a pointer to the payload of the next block
//next in this case refers to the next physically located block 
//(not next in free list)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
//PREV_BLKP returns a pointer to the payload of the previous block
//previous in this case refers to the previous physically located block
//(not previous in free list)
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

//Memory Pointer
//points to payload (footer) of first block in heap, which is prologue block
static char * heap_listp; 
//used for next fit allocation; points to the next free block in the
//explicit list after the last one that was allocated
static char * current;
//points to the first free block in explicit list
static char * firstFree;
//points to the last free block in explicit list
static char * lastFree;

//Helper Functions
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *first_fit(size_t asize);
static void *next_fit(size_t asize);
static void *best_fit(size_t asize);
static void place(void *bp, size_t asize);
static void removeBlock(void * bp);
static void insertInFront(void * bp);

/* which fitting technique to use */
/* default is first fit */
int whichfit = FIRSTFIT;

/* 
 * mm_init - initialize the malloc package for implict list
 *           allocation. Specifically, initialize the heap 
 *           to contain an allocated block with no payload (prologue), 
 *           followed by a big free block, followed by an allocated 
 *           block with no footer (epilogue). The prologue and epilogue 
 *           blocks make the coalescing logic simpler.
 */
int mm_init(void)
{
   if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *) - 1)
      return -1;

   PUT(heap_listp, 0);                          //Padding
   PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); //Prologue header
   PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); //Prologue footer
   PUT(heap_listp + (3*WSIZE), PACK(0, 1));     //Epilogue header

   //Make heap_listp point to footer of Prologue block
   //This is the payload of the first (and only) allocated block
   heap_listp += (2*WSIZE);
   firstFree = lastFree = 0;
   char * bp;
   if ((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL)
      return -1;

   //current is used for next fit placement
   //current always points to a free block or is NULL if
   //there are no free blocks
   current = firstFree;
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

    if(size == 0) return NULL;

    //Adjust the size to make room for the header, footer,
    //predecessor, and successor and adhere to the alignment restrictions.
    if(size <= DSIZE)
    {
       //minimum block size is 2*DSIZE
       asize = 2*DSIZE;
    }
    else
    {
       //size must be a multiple of DSIZE
       asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); 
    }

   //Search free list for fit.

   if (whichfit == BESTFIT)
      bp = best_fit(asize);
   else if (whichfit == NEXTFIT)
      bp = next_fit(asize);
   else
      bp = first_fit(asize); //default

   //If a free block was found then use it
   if (bp != NULL)
   {
      current = (char *) GET(SUCC(bp)); //for next fit
      place(bp, asize);
      return bp;
   }

   //No free block found, extend the heap 
   extendsize = MAX(asize, CHUNKSIZE);
   if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
   {
      return NULL;
   }

   //allocate the block
   current = (char *) GET(SUCC(bp)); //for next fit
   place(bp, asize);
   return bp;
}

/*
 * mm_free - Free the block and coalesce it with adjacent free blocks.
 *           ptr points to the payload of the block to be free.
 */
void mm_free(void *ptr)
{
   size_t size = GET_SIZE(HDRP(ptr));
   if (GET_ALLOC(HDRP(ptr)) == 0) return;

   PUT(HDRP(ptr), PACK(size, 0));
   PUT(FTRP(ptr), PACK(size, 0));
   insertInFront(ptr);
   coalesce(ptr);
}

/*
 * mm_realloc - Allocate a new block of size bytes.  Copy the contents
 *              of the block pointed to by ptr into that block.
 *              Returns a pointer to the payload of the new block.
 */
void *mm_realloc(void *ptr, size_t size)
{
    if(ptr == NULL)
    {
       return mm_malloc(size);
    }
    if(size == 0)
    {
       mm_free(ptr);
       return NULL;
    }
    //See if block is already big enough
    //Need room for payload of size bytes plus header and footer
    if(GET_SIZE(HDRP(ptr)) >= (size + WSIZE + WSIZE))
    {
       return ptr;
    }
    
    void *oldptr = ptr;
    void *newptr = mm_malloc(size);

    //if malloc fails, give up
    if (newptr == NULL) return NULL;

    size_t copySize = GET_SIZE(HDRP(oldptr));    
    
    //if the original block is bigger than size parameter,
    //only copy what will fit into the new block
    if (size < copySize) copySize = size;
    
    //Copy the contents of the old block into
    //the new block
    memcpy(newptr, oldptr, copySize);
    //Free the old block
    mm_free(oldptr);
    
    return newptr;
}

/*
 * insertFront - Takes a pointer to a free block and inserts the
 *               block so that it is the first block in the
 *               explicit list.
 *               The global firstFree and possibly lastFree are
 *               modified by the function.
 */
static void insertInFront(void * bp)
{
    // Indicate that BP is the head of the list.
    // Set bp->successor to the current first block.
    // 
    PUT(PRED(bp), (unsigned int)0);
    PUT(SUCC(bp), (unsigned int)firstFree);
    
    // Change PRED of old first block to bp
    if (firstFree != 0) {
        PUT(PRED(firstFree), (unsigned int)bp);
    }

    // Since there is now a free block at the front,
    firstFree = bp;
    
    if (lastFree == 0) {
        lastFree = firstFree;
    }
}


/* 
 * extend_heap - extends the size of the heap by words * WSIZE bytes
 *               
 */
static void *extend_heap(size_t words)
{
   char *bp;
   size_t size;

   size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

   if(((long)(bp = mem_sbrk(size)) == -1))
      return NULL;

   PUT(HDRP(bp), PACK(size, 0));
   PUT(FTRP(bp), PACK(size, 0));
   PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

   //put this new block in the front of the free list
   insertInFront(bp);

   //may be able to coalesce with the block in
   //memory above it
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


//TODO 
   //Use the slides to help you figure out how to implement this.
   //Also take a look at the implicit list code.
   //removeBlock function is very helpful here.
   //Macros defined at the top of the file are also very helpful.
   size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
   size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
   size_t size = GET_SIZE(HDRP(bp));
   
   // NOTE: In the slides it states the following:
   // Some extra space for the links (2 extra words
   // needed for each block)
   //
   // This may be important?? - Justin
   
   // case 1: Insert the freed block at the root of the list.
   if (prev_alloc && next_alloc
   {
   
   }
   // case 2: Splice out the succ block, coalesce both memory
   // blocks and insert the new block at the root of the list.
   else if (prev_alloc && !next_alloc)
   {

   }
   // case 3: Splice out the pred block, coalesce both memory 
   // blocks, and insert the new block at the root of the list.
   else if (!prev_alloc && next_alloc)
   {

   }
   // case 4: Splice out pred and succ blocks, coalesce all 3 memory
   // blocks and insert the new block at the root of the list.    
   else
   {
       
   }
   //If you implement next_fit for extra credit then check to see if
   //current needs to be modified

   return bp;
}

/*
 * first_fit - Starts at the begining of the list and returns the
 *             first free block with a size that is greater than or
 *             equal to asize.
 *             Returns a pointer to the found block or NULL if no
 *             block fits.
 */
static void *first_fit(size_t asize)
{
   char *bp;
   //firstFree points to the first block in the free list
   //the successor word in the block points to the next block 
   for (bp = firstFree; bp != 0; bp = (char *) GET(SUCC(bp)))
   {
      if (asize <= GET_SIZE(HDRP(bp)))
      {
         return bp;
      }
   }

   return NULL;
}

/*
 * next_fit - Starts at the free block in the list after the last block that
 *            was allocated and searches in the list for the next block with 
 *            a size that is greater than or equal to asize. 
 *            Returns a pointer to the found block or NULL if no
 *            block fits.
 */
static void *next_fit(size_t asize)
{
//TODO 
/******** You can implement this for extra credit if you like. ********/   
   return first_fit(asize);
}

/*
 * best_fit - Looks for a block of size asize bytes by going through all of
 *            the blocks in the free list and returning the one that is 
 *            smallest and also greater than or equal to asize.
 *
 */
static void *best_fit(size_t asize)
{
//TODO 
/******** You can implement this for extra credit if you like. ********/   
   return first_fit(asize);
}

/*
 * place - Adds a header and footer to allocated block. If the block
 *         is larger than needed and the leftover portion is at 
 *         least the minimum block size then mark the remaining portion
 *         as a free block and add it to the free list (but not in
 *         front; see Slides9-9-12 to 9-9-14).
 */
static void place(void *bp, size_t asize)
{
   //get the size of the free block
   size_t csize = GET_SIZE(HDRP(bp));
   unsigned int * pred = (unsigned int *) GET(PRED(bp));
   unsigned int * succ = (unsigned int *) GET(SUCC(bp));

   //if the unused portion is at least 2*DSIZE
   //then split the block into two
   if((csize - asize) >= (2*DSIZE))
   {
      //add the header and footer to the allocated block 
      PUT(HDRP(bp), PACK(asize, 1));
      PUT(FTRP(bp), PACK(asize, 1));
      void * nxtbp = NEXT_BLKP(bp);
      PUT(HDRP(nxtbp), PACK(csize - asize, 0));
      PUT(FTRP(nxtbp), PACK(csize - asize, 0));

      if (pred != 0) PUT(SUCC(pred), (unsigned int) nxtbp);
      if (succ != 0) PUT(PRED(succ), (unsigned int) nxtbp);

      if (firstFree == bp) firstFree = (char *) nxtbp;
      PUT(PRED(nxtbp), (unsigned int) pred);

      //add the header and footer to the unallocated block
      if (bp == lastFree) lastFree = (char *) nxtbp;
      PUT(SUCC(nxtbp), (unsigned int) succ);
   }
   else
   {
      //remove entire block from the free list
      removeBlock(bp);
      //add the header and footer
      PUT(HDRP(bp), PACK(csize, 1));
      PUT(FTRP(bp), PACK(csize, 1));
   }
}

/* 
 * removeBlock - Takes as input a pointer to a free block and removes it
 *               from the explicit list by adjusting the pointers in the
 *               previous and next blocks.
 *               Will also change the values of firstFree and lastFree if
 *               the first block and/or the last block are being removed.
 */
static void removeBlock(void * bp)
{
    //TODO 
   //Remove a block by changing the pointers in the previous
   //block and the next block (if present) so that they point to each
   //other.  Be careful about edge cases.
   //
   //You may also need to change firstFree and/or lastFree.
    
    
    char * previousElement = (char *)GET(PRED(bp));
    char * nextElement = (char *)GET(SUCC(bp));
    if (previousElement) {
        PUT(SUCC(previousElement), (unsigned int)nextElement);
    }
    else {
        firstFree = nextElement;
    }
    if (nextElement) {
        PUT(PRED(nextElement), (unsigned int)previousElement);
    }
    else {
        lastFree = previousElement;  
    }
}

/*
 * printBlocks - Prints the entire heap indicating which blocks are
 *               allocated and which are free.
 */
void printBlocks()
{
   char * bp;
   printf("Entire heap\n");
   printf("%10s %10s %1s %10s %10s\n", "Addr", "Size", "a", "Pred", "Succ");
   for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
   {
      printf("%#10x %#10x %d ", (unsigned int) bp, GET_SIZE(HDRP(bp)),
             GET_ALLOC(HDRP(bp)));
      if (!GET_ALLOC(HDRP(bp)))
         printf("%#10x %#10x", GET(PRED(bp)), GET(SUCC(bp)));
      printf("\n");
   }
}

/*
 * printFreeList - Prints the blocks in the free list in the order
 *                 in which they are in the free list.
 */
void printFreeList()
{
   char * bp;
   printf("Free list\n");
   printf("%10s %10s %1s %10s %10s\n", "Addr", "Size", "a", "Pred", "Succ");
   for (bp = firstFree; bp != 0; bp = (char *)GET(SUCC(bp)))
   {
      printf("%#10x %#10x %d ", (unsigned int) bp, GET_SIZE(HDRP(bp)),
             GET_ALLOC(HDRP(bp)));
      if (!GET_ALLOC(HDRP(bp)))
         printf("%#10x %#10x", GET(PRED(bp)), GET(SUCC(bp)));
      printf("\n");
   }
   printf("firstFree: %x, lastFree: %x\n", (unsigned int) firstFree,
          (unsigned int) lastFree);
}
