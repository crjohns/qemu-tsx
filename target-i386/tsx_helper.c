/*
 *  Intel(R) Transactional Synchronization Extension Simulation Header
 *
 *  Author: Christopher R. Johnson <crjohns@csail.mit.edu>
 */

#include "cpu.h"
#include "qemu-log.h"
#include "helper.h"

#include "tsx.h"

#if !defined(CONFIG_USER_ONLY)
#include "softmmu_exec.h"
#endif /* !defined(CONFIG_USER_ONLY) */

#include <stdio.h>

extern FILE *logfile;
extern uint64_t logcycle;

void HELPER(xtest)(CPUX86State *env)
{
    /* set zero flag if in a transaction */
    if(env->hle_active || env->rtm_active)
        cpu_load_eflags(env, 0,  (CC_C | CC_O | CC_S | CC_P | CC_A | CC_Z));
    else
        cpu_load_eflags(env, CC_Z,  (CC_C | CC_O | CC_S | CC_P | CC_A | CC_Z));

    CC_OP = CC_OP_EFLAGS;

    /*
    if(env->hle_active)
        fprintf(stderr, "HLE WAS ACTIVE IN TEST\n");
    if(env->rtm_active)
        fprintf(stderr, "RTM WAS ACTIVE IN TEST\n");

    fprintf(stderr, "In XTEST NEW ZF %d\n", env->eflags & CC_Z);
    */
}



static void print_abort_reason(char *buf, int size, target_ulong reason)
{
    snprintf(buf, size, "[%c%c%c%c%c%c]",
            (reason & TXA_XABORT)?'A':'.',
            (reason & TXA_RETRY)?'R':'.',
            (reason & TXA_CONFLICT)?'C':'.',
            (reason & TXA_OVERFLOW)?'O':'.',
            (reason & TXA_BREAKPOINT)?'B':'.',
            (reason & TXA_NESTED)?'N':'.');
}

static void txn_begin_processing(CPUX86State *env, target_ulong destpc)
{
    env->fallbackIP = destpc;
    /* need to backup more regs? */
    memcpy(env->rtm_shadow_regs, env->regs, sizeof(target_ulong)*CPU_NB_REGS);
    env->rtm_shadow_eflags = env->eflags;

    fprintf(stderr, "CPU %d starting txn (nest %d)\n", env->cpu_index, env->rtm_nest_count);
    fprintf(logfile, "XBEGIN %ld CPU %d PC 0x%lx\n", 
            logcycle, env->cpu_index, env->eip);
}

void txn_abort_processing(CPUX86State *env, uint32_t set_eax, int action)
{
    if(env->rtm_nest_count > 1)
        set_eax |= TXA_NESTED;

#if RTM_DEBUG
    fprintf(stderr, "CPU %d aborting transaction\n", env->cpu_index);
#endif
    
    /* restore old regs */
    /* XXX: Restore SIMD stuff here too */
    env->eip = env->fallbackIP;
    memcpy(env->regs, env->rtm_shadow_regs, sizeof(target_ulong)*CPU_NB_REGS);
    env->eflags = env->rtm_shadow_eflags;
    env->cc_op = CC_OP_EFLAGS;
    env->cc_src = env->eflags;

    /* reset RTM state */
    env->rtm_nest_count = 0;
    env->rtm_active = 0;

    /* free all buffers */
    env->rtm_active_buffer_count = 0;

    env->regs[R_EAX] = set_eax;

    char reasonbuf[16];
    print_abort_reason(reasonbuf, 16, set_eax);
    
    fprintf(logfile, "XABORT %ld CPU %d PC 0x%lx %s confcount %lu\n", 
            logcycle, env->cpu_index, env->eip, reasonbuf, env->rtm_conflict_count);

    if(action == ABORT_EXIT)
        cpu_loop_exit(env);
    else
        return;

}

static void txn_commit(CPUX86State *env)
{
    int i;

#if RTM_DEBUG
    fprintf(stderr, "Flushing txn on cpu %d\n", env->cpu_index);
#endif

    fprintf(logfile, "XCOMMIT %ld CPU %d PC 0x%lx lines %lu\n", 
            logcycle, env->cpu_index, env->eip, env->rtm_active_buffer_count);

    // Flush cached transaction lines
    for(i=0; i<env->rtm_active_buffer_count; i++)
    {
        int j;
        target_ulong linestart = env->rtm_buffers[i].tag << TSX_LOG_CACHE_LINE_SIZE;

        int dirty = env->rtm_buffers[i].flags & RTM_FLAG_DIRTY;
#if RTM_DEBUG
        fprintf(stderr, "\tLine %p [%c]\n", (void*)linestart, (dirty)?'D':'.');
#endif

        // Skip non-dirty lines
        if(!dirty)
            continue;

        for(j = 0; j < (1u << TSX_LOG_CACHE_LINE_SIZE); j++)
        {
            cpu_stb_data(env, linestart + j, env->rtm_buffers[i].data[j]);
        }
    }

    // Clear buffer count, disable RTM
    env->rtm_active_buffer_count = 0;
    env->rtm_active = 0;

#if RTM_DEBUG
    fprintf(stderr, "CPU %d had %lu conflicts so far\n", env->cpu_index, env->rtm_conflict_count);
#endif
    

}

void HELPER(xbegin)(CPUX86State *env, target_ulong destpc, int32_t dflag)
{
    if(env->rtm_nest_count < MAX_RTM_NEST_COUNT)
    {
        /* TODO Several exceptions missing here
           mostly have to do with non-64-bit modes.
           Ignoring this for now */

        env->rtm_nest_count += 1;

        if(!env->rtm_active)
        {
            env->rtm_active = 1;
            env->rtm_active_buffer_count = 0;
            txn_begin_processing(env, destpc);
        }
    }
    else
    {
        /* XXX is OVERFLOW appropriate here? */
        txn_abort_processing(env, TXA_OVERFLOW | TXA_NESTED, ABORT_EXIT);
    }
}

void HELPER(xend)(CPUX86State *env)
{
    if(!env->rtm_active)
    {
        // Error, invalid
        fprintf(stderr, "ERROR: XEND outside of txn\n");
        raise_exception(env, EXCP0D_GPF);
        return;
    }

    env->rtm_nest_count -= 1;

    if(env->rtm_nest_count == 0)
    {
        txn_commit(env);
    }

}

void HELPER(xabort)(CPUX86State *env, uint32_t reason)
{
    if(env->rtm_active)
        txn_abort_processing(env, TXA_XABORT | TXA_ARG(reason), ABORT_EXIT);
}


/* get the offset of an address within a cache line by extracting low bits */
#define CALC_LINE_OFFSET(addr) (addr & ((1u << TSX_LOG_CACHE_LINE_SIZE) - 1))

static TSX_RTM_Buffer *find_rtm_buf(CPUX86State *env, target_ulong a0)
{
    int i;
    target_ulong tag;

    tag = a0 >> TSX_LOG_CACHE_LINE_SIZE;
    for(i=0; i<env->rtm_active_buffer_count; i++)
    {
        if(env->rtm_buffers[i].tag == tag)
        {
            return &env->rtm_buffers[i];
        }
    }

    return NULL;
}

static TSX_RTM_Buffer *alloc_rtm_buf(CPUX86State *env)
{
    TSX_RTM_Buffer *ret;

    if(env->rtm_active_buffer_count < NUM_RTM_BUFFERS)
    {
        ret = &env->rtm_buffers[env->rtm_active_buffer_count];
        env->rtm_active_buffer_count += 1;
        return ret;
    }
    else
        return NULL;
}


/*
 * Execute body for each cache line in a cpu other than that given by "env"
 * The cache line will be in TSX_RTM_Buffer *var, and its associated cpu
 * will be in CPUX86State *current
 */
#define FOREACH_OTHER_TXN(env, current, var, body) \
{ \
    CPUX86State *current; \
    for(current = first_cpu; current != NULL; current = current->next_cpu)  \
    { \
        if(current == env) continue; \
        if(current->rtm_active) \
        { \
            int tctr; \
            for(tctr = 0; tctr < current->rtm_active_buffer_count; tctr++) \
            { \
                TSX_RTM_Buffer *var = &current->rtm_buffers[tctr]; \
                { body } \
            } \
        } \
    } \
}


/* detect if another cpu already accessed the line, and abort if they have 
 * This is essentially FCFS conflict resolution */
__attribute__((unused))
static int detect_conflict_pessimistic(CPUX86State *env, TSX_RTM_Buffer *rtmbuf)
{

    FOREACH_OTHER_TXN(env, current, ctxn, 
        if(ctxn->tag == rtmbuf->tag)
        {
#if RTM_DEBUG
            fprintf(stderr, "TXN CONFLICT: CPU %d failed because %d read line %p first\n", 
                    env->cpu_index, current->cpu_index, 
                    (void*)(rtmbuf->tag << TSX_LOG_CACHE_LINE_SIZE));
#endif
            return 1;
        })

    return 0;
}

/* detect if another cpu already wrote to a line we are reading, or
 * accessed a line we are writing 
 * This supports READ/READ
 * */
static int detect_conflict_writes(CPUX86State *env, TSX_RTM_Buffer *rtmbuf)
{

    FOREACH_OTHER_TXN(env, current, ctxn, 
        if(ctxn->tag == rtmbuf->tag)
        {
            if(rtmbuf->flags & RTM_FLAG_DIRTY)
            {
#if RTM_DEBUG
                fprintf(stderr, "TXN CONFLICT: CPU %d failed due to WA%c from %d on line %p\n", 
                        env->cpu_index, (ctxn->flags & RTM_FLAG_DIRTY)?'W':'R',
                        current->cpu_index, 
                        (void*)(rtmbuf->tag << TSX_LOG_CACHE_LINE_SIZE));
#endif
                return 1;
            }
            if(ctxn->flags & RTM_FLAG_DIRTY)
            {
#if RTM_DEBUG
                fprintf(stderr, "TXN CONFLICT: CPU %d failed due to RAW from %d on line %p\n", 
                        env->cpu_index, current->cpu_index, 
                        (void*)(rtmbuf->tag << TSX_LOG_CACHE_LINE_SIZE));
#endif
                return 1;
            }

        }
        )

    return 0;
}

#define DETECT_CONFLICT detect_conflict_writes


static TSX_RTM_Buffer *read_line_into_cache(CPUX86State *env, target_ulong a0)
{
    int i;
    target_ulong addr_base;

    TSX_RTM_Buffer *rtmbuf = alloc_rtm_buf(env);
    if(!rtmbuf)
    {
        /* Cannot alloc a new buffer, hardware buffer overflow */
        fprintf(stderr, "ERROR: HTM overflow\n");
        txn_abort_processing(env, TXA_OVERFLOW, ABORT_EXIT);
    }



    rtmbuf->tag = (a0 >> TSX_LOG_CACHE_LINE_SIZE);
    rtmbuf->flags = 0;
    addr_base = (rtmbuf->tag << TSX_LOG_CACHE_LINE_SIZE);
    
#if RTM_DEBUG
    //fprintf(stderr, "CPU %d pulling line %p into txn cache\n", env->cpu_index, (void*)addr_base);
#endif

#ifdef RTM_DATA_DEBUG
    char *line = (char*)malloc(1024);
    memset(line, 0, 1024);
    int len = 0;

    len += snprintf(line+len, 1023-len, "CPU (%d) Line %p\n", env->cpu_index, (void*)addr_base);
#endif


    for(i=0; i<(1u << TSX_LOG_CACHE_LINE_SIZE); i++)
    {
        rtmbuf->data[i] = cpu_ldub_data(env, addr_base + i);
     
#ifdef RTM_DATA_DEBUG
        len += snprintf(line+len, 1023-len, " %02x", rtmbuf->data[i] & 0xFF);
#endif
    }

#ifdef RTM_DATA_DEBUG
    printf("%s\n", line);
    free(line);
#endif

    return rtmbuf;
}

/* do all buffer reads at the byte level to deal with unaligned reads */
static void do_txn_buf_read_byte(target_ulong *out_data, CPUX86State *env, 
        target_ulong a0)
{
    target_ulong offset;
    TSX_RTM_Buffer *rtmbuf;

    offset = CALC_LINE_OFFSET(a0);

    if(!(rtmbuf = find_rtm_buf(env, a0)))
    {
        rtmbuf = read_line_into_cache(env, a0);
    }

    /* abort on a conflict caused by this read */
    if(DETECT_CONFLICT(env, rtmbuf))
    {
        env->rtm_conflict_count += 1;
        txn_abort_processing(env, TXA_RETRY | TXA_CONFLICT, ABORT_EXIT);
    }

    *out_data = rtmbuf->data[offset]; 
}


static void do_txn_buf_write_byte(uint8_t byte, CPUX86State *env, target_ulong a0)
{
    target_ulong offset;
    TSX_RTM_Buffer *rtmbuf;

    offset = CALC_LINE_OFFSET(a0);

    if(!(rtmbuf = find_rtm_buf(env, a0)))
    {
        rtmbuf = read_line_into_cache(env, a0);
    }


    /* dirty this line */
    rtmbuf->flags |= RTM_FLAG_DIRTY;

    /* abort on a conflict caused by this write */
    if(DETECT_CONFLICT(env, rtmbuf))
    {
        env->rtm_conflict_count += 1;
        txn_abort_processing(env, TXA_RETRY | TXA_CONFLICT, ABORT_EXIT);
    }
    
    /* Set the byte in the buffered cache line */
    rtmbuf->data[offset] = byte;
}

static target_ulong do_read_b(CPUX86State *env, target_ulong a0)
{
    target_ulong data;

    do_txn_buf_read_byte(&data, env, a0);

    return data & 0xff;
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

    //fprintf(stderr, "Got 0x%lx at %p (index %d)\n", data, (void*)a0, idx);

    return data;
}

target_ulong HELPER(xmem_read_s)(CPUX86State *env, int32_t idx, target_ulong a0)
{
    target_ulong value = helper_xmem_read(env, idx, a0);

    target_ulong mask;
    target_ulong sign;

    switch(idx & 3)
    {
        case 0:
            mask = 0xFF;
            sign = 0x80;
            break;
        case 1:
            mask = 0xFFFF;
            sign = 0x8000;
            break;
        case 2:
            mask = 0xFFFFFFFF;
            sign = 0x80000000;
            break;
        default:
            mask = ~0;
            sign = 0;
            break;
    }

    value &= mask;
    if(value & sign)
        value |= ~mask;

    return value;
}

void HELPER(xmem_write)(target_ulong data, CPUX86State *env, int32_t idx, target_ulong a0)
{

    switch(idx & 3)
    {
        case 0:
            do_txn_buf_write_byte(data & 0xFF, env, a0);
            break;
        case 1:
            do_txn_buf_write_byte(data & 0xFF, env, a0);
            do_txn_buf_write_byte((data >> 8) & 0xFF, env, a0+1);
            break;
        case 2:
            do_txn_buf_write_byte(data & 0xFF, env, a0);
            do_txn_buf_write_byte((data >> 8) & 0xFF, env, a0+1);
            do_txn_buf_write_byte((data >> 16) & 0xFF, env, a0+2);
            do_txn_buf_write_byte((data >> 24) & 0xFF, env, a0+3);
            break;
#ifdef TARGET_X86_64
        case 3:
            do_txn_buf_write_byte(data & 0xFF, env, a0);
            do_txn_buf_write_byte((data >> 8) & 0xFF, env, a0+1) ;
            do_txn_buf_write_byte((data >> 16) & 0xFF, env, a0+2);
            do_txn_buf_write_byte((data >> 24) & 0xFF, env, a0+3);
            do_txn_buf_write_byte((data >> 32) & 0xFF, env, a0+4);
            do_txn_buf_write_byte((data >> 40) & 0xFF, env, a0+5);
            do_txn_buf_write_byte((data >> 48) & 0xFF, env, a0+6);
            do_txn_buf_write_byte((data >> 56) & 0xFF, env, a0+7);
            break;
#endif
    }
}


void HELPER(debug_val)(target_ulong tag, target_ulong val)
{
    fprintf(stderr, "[tag %ld] Debug val is 0x%lx\n", tag, val);
}
