#ifndef TSE_H
#define TSE_H


#define RTM_DEBUG 1

#define MAX_RTM_NEST_COUNT 128  /* max number of nested txns */
#define NUM_RTM_BUFFERS 1024     /* max number of cache lines in txn */

/* base 2 log of the cache line size (6 -> 64 byte lines) */
#define TSE_LOG_CACHE_LINE_SIZE 6


typedef struct TSE_RTM_Buffer
{
    target_ulong tag; /* data tag (start of cache line) */

    /* data buffer */
    char data[1u << TSE_LOG_CACHE_LINE_SIZE];
} TSE_RTM_Buffer;


#endif

