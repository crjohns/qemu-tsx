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
    env->cc_op = CC_OP_EFLAGS;


}
