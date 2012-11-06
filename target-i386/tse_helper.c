/*
 *  Intel(R) Transactional Synchronization Extension Simulation
 *
 *  Copyright (c) 2012 Christopher R. Johnson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */



#include "cpu.h"
#include "qemu-log.h"
#include "helper.h"

#include "tse.h"

#if !defined(CONFIG_USER_ONLY)
#include "softmmu_exec.h"
#endif /* !defined(CONFIG_USER_ONLY) */

#include <stdio.h>

void HELPER(xtest)(CPUX86State *env)
{
    /* set zero flag if in a transaction */
    if(env->hle_active || env->rtm_active)
        env->eflags &= ~CC_Z;
    else
        env->eflags |= CC_Z;

    /* clear other flags */
    env->eflags &= (~(CC_C | CC_O | CC_S | CC_P | CC_A));


    /* lazy evaluation */
    env->cc_src = env->eflags;
    env->cc_op = CC_OP_EFLAGS;
}


#define TXA_XABORT (1 << 0)
#define TXA_RETRY  (1 << 1)
#define TXA_CONFLICT (1 << 2)
#define TXA_OVERFLOW (1 << 3)
#define TXA_BREAKPOINT (1 << 4)
#define TXA_NESTED (1 << 5)
#define TXA_ARG(arg) ((arg & 0xFF) << 24)

static void txn_begin_processing(CPUX86State *env, target_ulong destpc)
{
    env->fallbackIP = destpc;
    /* need to backup more regs? */
    memcpy(env->rtm_shadow_regs, env->regs, sizeof(target_ulong)*CPU_NB_REGS);
    env->rtm_shadow_eflags = env->eflags;
}

static void txn_abort_processing(CPUX86State *env, uint32_t set_eax)
{
    if(env->rtm_nest_count > 1)
        set_eax |= TXA_NESTED;

    env->eip = env->fallbackIP;
    memcpy(env->regs, env->rtm_shadow_regs, sizeof(target_ulong)*CPU_NB_REGS);
    env->eflags = env->rtm_shadow_eflags;
    env->cc_op = CC_OP_EFLAGS;
    env->cc_src = env->eflags;

    env->rtm_nest_count = 0;
    env->rtm_active = 0;

    env->regs[R_EAX] = set_eax;
}

void HELPER(xbegin)(CPUX86State *env, target_ulong destpc, int32_t dflag)
{
    if(env->rtm_nest_count < MAX_RTM_NEST_COUNT)
    {
        /* TODO Several exceptions missing here
           mostly have to do with non-64-bit modes.
           Ignoring this for now */

        env->rtm_nest_count += 1;
        env->rtm_active = 1;
        env->rtm_active_buffer_count = 0;

        txn_begin_processing(env, destpc);
    }
    else
    {
        /* XXX is OVERFLOW appropriate here? */
        txn_abort_processing(env, TXA_OVERFLOW | TXA_NESTED);
    }
}

void HELPER(xend)(CPUX86State *env)
{
}

void HELPER(xabort)(CPUX86State *env, uint32_t reason)
{
    /* XXX should be NOP if not active... */
    txn_abort_processing(env, TXA_XABORT | TXA_ARG(reason));
}


/* get the offset of an address within a cache line by extracting low bits */
#define CALC_LINE_OFFSET(addr) (addr & ((1u << TSE_LOG_CACHE_LINE_SIZE) - 1))

static TSE_RTM_Buffer *find_rtm_buf(CPUX86State *env, target_ulong a0)
{
    int i;
    target_ulong tag;

    tag = a0 >> TSE_LOG_CACHE_LINE_SIZE;
    for(i=0; i<env->rtm_active_buffer_count; i++)
    {
        if(env->rtm_buffers[i].tag == tag)
        {
            return &env->rtm_buffers[i];
        }
    }

    return NULL;
}

static TSE_RTM_Buffer *alloc_rtm_buf(CPUX86State *env)
{
    TSE_RTM_Buffer *ret;

    if(env->rtm_active_buffer_count < NUM_RTM_BUFFERS)
    {
        ret = &env->rtm_buffers[env->rtm_active_buffer_count];
        env->rtm_active_buffer_count += 1;
        return ret;
    }
    else
        return NULL;
}

/* do all buffer reads at the byte level to dead with unaligned reads */
static int do_txn_buf_read_byte(target_ulong *out_data, CPUX86State *env, 
        target_ulong a0)
{
    target_ulong offset;
    TSE_RTM_Buffer *rtmbuf;

    offset = CALC_LINE_OFFSET(a0);

    if((rtmbuf = find_rtm_buf(env, a0)))
    {
        *out_data = rtmbuf->data[offset]; 
        return 1;
    }
    else
        return 0;

}

static void do_txn_buf_write_byte(uint8_t byte, CPUX86State *env, target_ulong a0)
{
    target_ulong offset;
    TSE_RTM_Buffer *rtmbuf;

    offset = CALC_LINE_OFFSET(a0);

    if(!(rtmbuf = find_rtm_buf(env, a0)))
    {
        /* This txn did not touch that byte yet, 
           pull the line into a buffer and modify it */
        int i;
        target_ulong addr_base;

        rtmbuf = alloc_rtm_buf(env);
        if(!rtmbuf)
        {
            /* Cannot alloc a new buffer, hardware buffer overflow */
            txn_abort_processing(env, TXA_OVERFLOW);
            return;
        }

        rtmbuf->tag = (a0 >> TSE_LOG_CACHE_LINE_SIZE);
        addr_base = (rtmbuf->tag << TSE_LOG_CACHE_LINE_SIZE);

        for(i=0; i<(1u << TSE_LOG_CACHE_LINE_SIZE); i++)
        {
            rtmbuf->data[i] = cpu_ldub_data(env, addr_base + i);
        }
    }
    
    /* Set the byte in the buffered cache line */
    rtmbuf->data[offset] = byte;
    return;


}

static target_ulong do_read_b(CPUX86State *env, target_ulong a0)
{
    target_ulong data;

    if(!do_txn_buf_read_byte(&data, env, a0))
    {
        data = cpu_ldub_data(env, a0);
        do_txn_buf_write_byte(data & 0xFF, env, a0);
    }

    return data;
}

static target_ulong do_read_w(CPUX86State *env, target_ulong a0)
{
    target_ulong data;

    data = do_read_b(env, a0);
    data |= (do_read_b(env, a0+1) << 8);

    return data;
}

static target_ulong do_read_l(CPUX86State *env, target_ulong a0)
{
    target_ulong data;

    data = do_read_w(env, a0);
    data |= (do_read_w(env, a0+2) << 16);
    return data;
}

static target_ulong do_read_q(CPUX86State *env, target_ulong a0)
{
    target_ulong data = 0x0000DEADBEEF0000lu;

#ifdef TARGET_X86_64
    data = do_read_l(env, a0);
    data |= (do_read_l(env, a0+4) << 32);
#endif

    return data;
}


target_ulong HELPER(xmem_read)(CPUX86State *env, int32_t idx, target_ulong a0)
{

    target_ulong data;

    switch(idx & 3)
    {
        case 0:
            data = do_read_b(env, a0);
            break;
        case 1:
            data = do_read_w(env, a0);
            break;
        case 2:
            data = do_read_l(env, a0);
            break;
        case 3:
#ifdef TARGET_X86_64
            data = do_read_q(env, a0);
#endif
            break;
    }

    fprintf(stderr, "Got 0x%lx at %p (index %d)\n", data, (void*)a0, idx);

    return data;
}

target_ulong HELPER(xmem_write)(CPUX86State *env, int32_t idx, target_ulong a0)
{

    return 0u;
}


void HELPER(debug_val)(target_ulong val)
{
    fprintf(stderr, "Debug val is 0x%lx\n", val);
}
