#include "tsx_cache.h"


void clear_rtm_cache(CPUX86State *env)
{
    TSXCache *cache = env->tsx_cache;
    cache->alloced_lines = 0;
    memset(cache->buffer, 0, sizeof(TSX_RTM_Buffer)*cache->ways*cache->sets);
}

void create_rtm_cache(CPUX86State *env, target_ulong ways, target_ulong sets)
{
    env->tsx_cache = (TSXCache*) malloc(sizeof(TSXCache));

    env->tsx_cache->ways = ways;
    env->tsx_cache->sets = sets;

    env->tsx_cache->buffer = (TSX_RTM_Buffer*) malloc(
                                        sizeof(TSX_RTM_Buffer)*ways*sets);
    clear_rtm_cache(env);
    
}

void delete_rtm_cache(CPUX86State *env)
{
    free(env->tsx_cache->buffer);
    free(env->tsx_cache);
}

TSX_RTM_Buffer *find_rtm_buf(CPUX86State *env, target_ulong a0)
{
    int i;
    target_ulong tag;
    TSXCache *cache = env->tsx_cache;
    target_ulong baseIndex;

    tag = a0 >> TSX_LOG_CACHE_LINE_SIZE;
    baseIndex = TAG_HASH(cache, tag);

    for(i=0; i<cache->ways; i++)
    {
        TSX_RTM_Buffer *buf = &cache->buffer[baseIndex*cache->ways+i];
        if(buf->tag == tag && buf->flags & RTM_FLAG_ACTIVE)
        {
            return buf;
        }
    }

    return NULL;
}

TSX_RTM_Buffer *alloc_rtm_buf(CPUX86State *env, target_ulong a0)
{
    target_ulong baseIndex;
    target_ulong tag;
    int i;
    TSXCache *cache = env->tsx_cache;

    tag = a0 >> TSX_LOG_CACHE_LINE_SIZE;
    baseIndex = TAG_HASH(cache, tag);


    for(i=0; i<cache->ways; i++)
    {
        TSX_RTM_Buffer *buf = &cache->buffer[baseIndex*cache->ways+i];
        if((buf->flags & RTM_FLAG_ACTIVE) == 0)
        {
            buf->flags = RTM_FLAG_ACTIVE;
            cache->alloced_lines += 1;
            return buf;
        }
    }

    return NULL;
}
