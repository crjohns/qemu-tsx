/*
 *  Intel(R) Transactional Synchronization Extension Simulation Header
 *
 *  Author: Christopher R. Johnson <crjohns@csail.mit.edu>
 */

#ifndef TSX_H
#define TSX_H


#define RTM_DEBUG 1

//#define DEBUG_SINGLESTEP /* enable singlestepping */
#define DEFAULT_SINGLESTEPS 10  /* Number of times to single step 
                                     after leaving HTM code */
#define INTFF_SINGLESTEPS 0xFFFFFFFF /* Number of times to single step when yielding
                               with int $0xFF; */

#define MAX_RTM_NEST_COUNT 128  /* max number of nested txns */
#define NUM_RTM_BUFFERS 1024     /* max number of cache lines in txn */

/* base 2 log of the cache line size (6 -> 64 byte lines) */
#define TSX_LOG_CACHE_LINE_SIZE 6



#define TXA_XABORT (1 << 0)
#define TXA_RETRY  (1 << 1)
#define TXA_CONFLICT (1 << 2)
#define TXA_OVERFLOW (1 << 3)
#define TXA_BREAKPOINT (1 << 4)
#define TXA_NESTED (1 << 5)
#define TXA_ARG(arg) ((arg & 0xFF) << 24)




typedef struct TSX_RTM_Buffer
{
    target_ulong tag; /* data tag (start of cache line) */

#define RTM_FLAG_DIRTY 1
#define RTM_FLAG_ACTIVE 2
    target_ulong flags;

    /* data buffer */
    char data[1u << TSX_LOG_CACHE_LINE_SIZE];
} TSX_RTM_Buffer;


typedef struct CPUX86State CPUX86State;

#define ABORT_RETURN 0
#define ABORT_EXIT 1
void txn_abort_processing(CPUX86State *env, uint32_t set_eax, int action);

#endif

