//-----------------------------------------------------------------------------
//   hash.h
//
//   Header file for Hash Table module hash.c.
//-----------------------------------------------------------------------------

#ifndef HASH_H
#define HASH_H


#define HTMAXSIZE 1999
#define NOTFOUND  -1

struct HTentry
{
    char   *key;
    int    data;
    struct HTentry *next;
};

typedef struct HTentry *HTtable;








#endif //HASH_H
