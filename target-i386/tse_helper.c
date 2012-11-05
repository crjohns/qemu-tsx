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


target_ulong HELPER(xmem_read)(CPUX86State *env, int32_t idx, target_ulong a0)
{

    target_ulong data;

    switch(idx & 3)
    {
        case 0:
            data = cpu_ldub_data(env, a0);
            break;
        case 1:
            data = cpu_lduw_data(env, a0);
            break;
        case 2:
            data = cpu_ldl_data(env, a0);
            break;
        case 3:
#ifdef TARGET_X86_64
            data = cpu_ldq_data(env, a0);
#endif
            break;
    }


    return data;
}


void HELPER(debug_val)(target_ulong val)
{
    fprintf(stderr, "Debug val is 0x%lx\n", val);
}
