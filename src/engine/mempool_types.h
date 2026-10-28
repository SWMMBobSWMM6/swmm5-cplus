// mempool_types.h -- module-local types of mempool.c, exported because they appear
// in member-function signatures of class Project.
#ifndef MEMPOOL_TYPES_H
#define MEMPOOL_TYPES_H

#define ALLOC_BLOCK_SIZE   64000       /*(62*1024)*/

typedef struct alloc_hdr_s
{
    struct alloc_hdr_s *next;   /* Next Block          */
    char               *block,  /* Start of block      */
                       *free,   /* Next free in block  */
                       *end;    /* block + block size  */
}  alloc_hdr_t;

typedef struct alloc_root_s
{
    alloc_hdr_t *first,    /* First header in pool */
                *current;  /* Current header       */
}  alloc_root_t;

#endif
