/* 
 * This program is used to test the implicit lists code.
 * You need to modify the program so that it provides a
 * good test of first fit, best fit, and next fit.
 * This program has a very simple test of first fit to give
 * a demonstration of how to write it.  You need to write a  
 * better test that includes tests of next fit and best fit.
 */
#include <string.h>
#include <stdlib.h>
#include "mmImplicit.h"
#include "memlib.h"

void parseArgs(int argc, char * argv[]);
void addressCompare(void * correct, void * returned);
void usage();

/* 
 * After calling mem_init and mm_init, this program makes
 * a series of calls to mm_malloc, mm_free, and printBlocks
 * to see the results
 */
int main(int argc, char * argv[])
{
   void *bp1, *bp2, *bp3, *bp4;

   parseArgs(argc, argv);
   //don't delete these calls. You need these.
   mem_init();
   mm_init();
   
   //this is what the heap looks like before any mallocs
   printBlocks();

   //You can modify this however you like.  You want to come
   //up with an example of mallocs and frees so that the mallocs
   //will return different blocks depending upon
   //the placement strategy.
   //Your first set of mallocs should use almost the entire heap
   //(and no more). 
   //After you do a bunch of mallocs, free half of those blocks
   //so you'll have a free block between every two allocated blocks.
   //That's your setup.
   //
   //This is a really simple example. You need to modify it to use
   //most of the heap (with one hole at the end for next fit.)
   //i.e., create more blocks with various sizes.
   //Note: you can give yourself easier addresses to work with by always
   //      passing to malloc a size in hex that ends with 8.  
   printf("Blocks after malloc(0x128), malloc(0x118), malloc(0x178)\n");
   bp1 = mm_malloc(0x128);
   bp2 = mm_malloc(0x118);
   bp3 = mm_malloc(0x178);
   printBlocks();

   //Now free one block so there is a free block between two allocated
   //blocks.
   mm_free(bp2);
   printf("Blocks after free(0x%x)\n", (unsigned int) bp2);
   printBlocks();

   //Now allocate a block
   bp4 = mm_malloc(0x38);
   printf("Blocks after malloc(0x38)\n");
   printBlocks();

   //firstfit will pick the very first free block that is big enough.
   //The one pointed to by bp2 is big enough.
   //The first parameter to addressCompare contains the address it should be.
   //The second parameter is the address returned by mm_malloc.
   if (whichfit == FIRSTFIT) addressCompare(bp2, bp4);

   //nextfit will pick the hole after the block pointed to
   //by bp3.  The size of that block is 0x178 + 8 = 0x180
   //This one won't work until you get next fit working.
   if (whichfit == NEXTFIT)  addressCompare(bp3 + 0x180, bp4);

   //You'll also need to test best fit
   //if (whichfit == BESTFIT) addressCompare(..., bp4);
   //
   //Now add one more malloc.  This malloc should cause next fit to loop
   //around.  That is, the hole at the bottom won't be big enough.
   //First fit and best fit should also return different values, so
   //don't make all of the holes the same size.
   //
   //Allocate the final block (bpX will be something like bp11,
   //depending upon how many blocks you allocate).
   //bpX = mm_malloc(....);
   //printf("Blocks after malloc(...)\n");
   //printBlocks();
   //
   //Remember the block that is allocated has to be different
   //for each of the three policies.
   //if (whichfit == FIRSTFIT) addressCompare(..., bpX);
   //if (whichfit == NEXTFIT)  addressCompare(..., bpX);
   //if (whichfit == BESTFIT) addressCompare(..., bpX);

   return 0;
}

/* 
 * addressCompare - Takes two addresses as input and compares them.
 *                  If the addresses don't match, an error message
 *                  is printed and the program is exited.
*/
void addressCompare(void * correct, void * returned)
{
   if (correct != returned) 
   {
      if (whichfit == FIRSTFIT) printf("First fit placement failed.\n");
      if (whichfit == NEXTFIT) printf("Next fit placement failed.\n");
      if (whichfit == BESTFIT) printf("Best fit placement failed.\n");
      printf("Should have picked: %x \n", (unsigned int) correct);
      printf("Instead chose: %x\n", (unsigned int) returned);
      exit(0);
   }
}

/*
 * parseArgs - Parses the command line arguments in order to set the 
 *             placement policy (first fit, next fit, or best fit).
 */
void parseArgs(int argc, char * argv[])
{
   if (argc > 2 && !strcmp(argv[1], "-w") && !strcmp(argv[2], "next"))
   {
      whichfit = NEXTFIT;
      printf("Implicit List Tester\n");
      printf("Using next fit placement strategy\n");
   } else if (argc > 2 && !strcmp(argv[1], "-w") && !strcmp(argv[2], "best"))
   {
      printf("Implicit List Tester\n");
      printf("Using best fit placement strategy\n");
      whichfit = BESTFIT;
   } else if (argc > 2 && !strcmp(argv[1], "-w") && !strcmp(argv[2], "first"))
   {
      printf("Implicit List Tester\n");
      printf("Using first fit placement strategy\n");
      whichfit = FIRSTFIT;
   } else if (argc != 1)
   {
      usage();
   }
}

/*
 * usage - prints usage information
 */
void usage()
{
   printf("Usage: implicitTester [-h | -w <fit>]\n");
   printf("       -w <fit> is first (default), next, or best\n"); 
   printf("       -h prints usage information\n");
   exit(0);
}

