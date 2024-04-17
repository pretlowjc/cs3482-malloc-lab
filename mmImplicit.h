#include <stdio.h>
#define FIRSTFIT 1
#define NEXTFIT 2
#define BESTFIT 3

extern int mm_init(void);
extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);
extern void *mm_realloc(void *ptr, size_t size);
extern int mm_check();
extern void printBlocks();
extern int whichfit;
