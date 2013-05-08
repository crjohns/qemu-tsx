#ifndef TXN_CACHE_H
#define TXN_CACHE_H

#include "cpu.h"
#include "tsx.h"

#define TSX_CACHE_WAYS 8
#define TSX_CACHE_SETS 64

/* get the offset of an address within a cache line by extracting low bits */
#define CALC_LINE_OFFSET(addr) (addr & ((1u << TSX_LOG_CACHE_LINE_SIZE) - 1))

typedef struct TSXCache
{
    target_ulong alloced_lines;
    target_ulong ways;
    target_ulong sets;

    TSX_RTM_Buffer *buffer;

} TSXCache;

void clear_rtm_cache(CPUX86State *env);

void create_rtm_cache(CPUX86State *env, target_ulong ways, target_ulong sets);

/* return the set number for the tag in this cache
 * just mods for now, but we can do more
 * complicated hashing here
 */
#define TAG_HASH(cache, tag) (tag % cache->sets)

void delete_rtm_cache(CPUX86State *env);

/* find an active buffer with the correct tag */
TSX_RTM_Buffer *find_rtm_buf(CPUX86State *env, target_ulong a0);

/* allocate a buffer and mark it active */
TSX_RTM_Buffer *alloc_rtm_buf(CPUX86State *env, target_ulong a0);


/*
 * Execute body for each cache line in a cpu other than that given by "env"
 * The cache line will be in TSX_RTM_Buffer *var, and its associated cpu
 * will be in CPUX86State *current
 */
#define FOREACH_OTHER_TXN(env, tag_in, current, var, body) \
{ \
    CPUX86State *current; \
    for(current = first_cpu; current != NULL; current = current->next_cpu)  \
    { \
        if(current == env) continue; \
        TSXCache *cache = current->tsx_cache; \
        if(current->rtm_active) \
        { \
            target_ulong baseIndex = TAG_HASH(cache, tag_in); \
            int i; \
            for(i = 0; i < cache->ways; i++) \
            { \
                TSX_RTM_Buffer *var = &cache->buffer[baseIndex*cache->ways+i]; \
                if(var->flags & RTM_FLAG_ACTIVE) \
                { body } \
            } \
        } \
    } \
}



#endif
