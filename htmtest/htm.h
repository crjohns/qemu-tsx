#ifndef HTM_H
#define HTM_H

//#include <stdlib.h>

#define TXA_XABORT (1 << 0)
#define TXA_RETRY  (1 << 1)
#define TXA_CONFLICT (1 << 2)
#define TXA_OVERFLOW (1 << 3)
#define TXA_BREAKPOINT (1 << 4)
#define TXA_NESTED (1 << 5)
#define TXA_ARG(val) ((val >> 24) & 0xFF)

#define _XBEGIN_STARTED 0xFFFFFFFF
#define _XBEGIN_STARTED_STR "0xFFFFFFFF"

#ifndef TSX_STORE_STATS
#define TSX_STORE_STATS 0
#endif

#if TSX_STORE_STATS

struct tsx_stat
{
    unsigned long count __attribute__((aligned(64)));
    char pad1[] __attribute__((aligned(64)));
};
static struct tsx_stat tsx_starts[NUM_THREADS] = {{0}};
static tsx_stat tsx_aborts[NUM_THREADS] = {{0}};
static tsx_stat tsx_xabort_code[NUM_THREADS][256] = {{0}};
static tsx_stat tsx_abort_reason[NUM_THREADS][64] = {{0}};
#endif



/*
 * Copyright (c) 2012,2013 Intel Corporation
 * Author: Andi Kleen
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define __rtm_force_inline __attribute__((__always_inline__)) inline

__attribute__ ((unused))
static __rtm_force_inline int xbegin(void)
{
    int ret = _XBEGIN_STARTED;
    asm volatile(".byte 0xc7,0xf8 ; .long 0" : "+a" (ret) :: "memory");
#if TSX_STORE_STATS
    if(ret == _XBEGIN_STARTED)
        tsx_starts[CPU()].count += 1;
    else
    {
        tsx_aborts[CPU()].count += 1;
        tsx_abort_reason[CPU()][ret&0x3F].count += 1;
        if(ret & TXA_XABORT)
            tsx_xabort_code[CPU()][(ret>>24) & 0xFF].count += 1;
    }
#endif
    return ret;
}

__attribute__ ((unused)) 
static void tsx_stats()
{
#if TSX_STORE_STATS
    unsigned int tx = 0;
    unsigned int txa = 0;
    unsigned int codes[256] = {0};
    unsigned int codesum = 0;
    unsigned int reasons[64] = {0};
    unsigned int reasonsum = 0;

    for(int i=0; i<NUM_THREADS; i++)
    {
        tx += tsx_starts[i].count;
        txa += tsx_aborts[i].count;
        for(int j=0; j<256; j++)
        {
            codes[j] += tsx_xabort_code[i][j].count;
            codesum += tsx_xabort_code[i][j].count;
        }
        for(int j=0; j<64; j++)
        {
            reasons[j] += tsx_abort_reason[i][j].count;
            reasonsum += tsx_abort_reason[i][j].count;
        }
    }
    
    fprintf(stderr, "Started TXN: %d  Aborted TXN: %d (%.2f%%)\n",
            tx, txa, 100.0*txa/tx);

    for(int i=0; i<64; i++)
    {
        if(reasons[i])
            fprintf(stderr, "  Abort Reason 0x%x: %6d (%.4f%%)\n", i, reasons[i], 100.0*reasons[i]/reasonsum);
    }
    for(int i=0; i<256; i++)
    {
        if(codes[i])
            fprintf(stderr, "  XABORT Code 0x%x: %6d (%.4f%%)\n", i, codes[i], 100.0*codes[i]/codesum);
    }

#else
    fprintf(stderr, "No tsx stats recorded\n");
#endif
}

__attribute__ ((unused))
static __rtm_force_inline void xend(void)
{
     asm volatile(".byte 0x0f,0x01,0xd5" ::: "memory");
}

__attribute__ ((unused))
static __rtm_force_inline void xabort(const unsigned int status)
{
    asm volatile(".byte 0xc6,0xf8,%P0" :: "i" (status) : "memory");
}

__attribute__ ((unused))
static __rtm_force_inline int xtest(void)
{
    unsigned char out;
    asm volatile(".byte 0x0f,0x01,0xd6 ; setnz %0" : "=r" (out) :: "memory");
    return out;
}

__attribute__ ((unused))
static void elock_acquire(volatile int *lock)
{
    asm volatile(
                 "1:\n\t"
                 "movl $1, %%eax\n\t"
                 "XACQUIRE LOCK xchg %%eax, %0\n\t"
                 "cmpl $0, %%eax\n\t"
                 "jz 3f\n\t"
                 "2:\n\t"
                 "cmpl $0, %0\n\t"
                 "jnz 2b\n\t"
                 "jmp 1b\n\t"
                 "3:\n\t"
                 : : "m" (*lock) : "%eax", "memory");
}

__attribute__ ((unused))
static void elock_release(volatile int *lock)
{
    asm volatile( "xor %%eax, %%eax\n\t"
                   "XRELEASE movl %%eax, %0\n\t"
                 : : "m" (*lock) : "%eax", "memory");

}

#endif
