/*
 * This program is used to test the explicit lists code.
 * You can use it/modify it however you like.
 */
#include <string.h>
#include <stdlib.h>
#include "mmExplicit.h"
#include "memlib.h"

void parseArgs(int argc, char * argv[]);
void addressCompare(void * correct, void * returned);
void usage();

int main(int argc, char * argv[])
{
   void *bp1, *bp2, *bp3, *bp4;

   parseArgs(argc, argv);
   //You need these.  Don't delete these calls.
   mem_init();
   mm_init();

   //You can modify this however you need for testing
   //and debugging.
   //For example, after you've implemented a particular
   //coalesce case, you should modify this so that it tests
   //that particular case.
   printf("Blocks after mm_init:\n");
   printBlocks();
   printFreeList();
   bp1 = mm_malloc(0x7f8);
   printf("Blocks after mm_malloc(0x7f8):\n");
   printBlocks();
   printFreeList();
   bp2 = mm_malloc(0x3f8);
   printf("Blocks after mm_malloc(0x3f8):\n");
   printBlocks();
   printFreeList();
   bp3 = mm_malloc(0x7f8);
   printf("Blocks after mm_malloc(0x7f8):\n");
   printBlocks();
   printFreeList();
   mm_free(bp1);
   mm_free(bp2);
   printBlocks();
   printFreeList();   //bp1 and bp2 blocks should be coalesced
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
   if ((argc == 1) || 
       (argc > 2 && !strcmp(argv[1], "-w") && !strcmp(argv[2], "first")))
   {
      printf("Explicit List Tester\n");
      printf("Using first fit placement strategy\n");
      whichfit = FIRSTFIT;
   }
   else if (argc > 2 && !strcmp(argv[1], "-w") && !strcmp(argv[2], "next"))
   {
      whichfit = NEXTFIT;
      printf("Explicit List Tester\n");
      printf("Using next fit placement strategy\n");
   } else if (argc > 2 && !strcmp(argv[1], "-w") && !strcmp(argv[2], "best"))
   {
      printf("Explicit List Tester\n");
      printf("Using best fit placement strategy\n");
      whichfit = BESTFIT;
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
   printf("Usage: explicitTester [-h | -w <fit>]\n");
   printf("       -w <fit> is first (default), next, or best\n");
   printf("       -h prints usage information\n");
   exit(0);
}
