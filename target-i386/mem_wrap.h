#ifndef MEMWRAP_H
#define MEMWRAP_H

/* Here are some hacks for dealing with temporary registers
 
   For Haswell emulation, we need to check if we are in a
   transaction before doing a memory access (which is the point
   of the wrapper function).

   This test requires a branch to test if transactional mode
   is active, however, QEMU does not maintain the state
   of temporary registers through branches. A local temporary
   is required for that, but the temps (e.g. cpu_T[0]) are
   not locals.

   The result is that register assignments for those temporaries are 
   unexpectedly cleared for the microcode before the load.

   These macros preserve the original values of temporaries 
   across the internal branch.

*/



#define DEF_LTEMPS() \
    TCGv local_T[2], local_A0; \
    TCGv local_T3, local_tmp0, local_tmp4, local_tmp5; \
    TCGv_ptr local_ptr0, local_ptr1; \
    TCGv_i32 local_tmp2_i32, local_tmp3_i32; \
    TCGv_i64 local_tmp1_i64;

#define LOCALIZE_TEMPS() \
    local_T[0] = tcg_temp_local_new(); \
    tcg_gen_mov_tl(local_T[0], cpu_T[0]); \
    local_T[1] = tcg_temp_local_new(); \
    tcg_gen_mov_tl(local_T[1], cpu_T[1]); \
    local_A0 = tcg_temp_local_new(); \
    tcg_gen_mov_tl(local_A0, cpu_A0); \
    local_T3 = tcg_temp_local_new(); \
    tcg_gen_mov_tl(local_T3, cpu_T3); \
    local_tmp0 = tcg_temp_local_new(); \
    tcg_gen_mov_tl(local_tmp0, cpu_tmp0); \
    local_tmp4 = tcg_temp_local_new(); \
    tcg_gen_mov_tl(local_tmp4, cpu_tmp4); \
    local_tmp5 = tcg_temp_local_new(); \
    tcg_gen_mov_tl(local_tmp5, cpu_tmp5); \
    local_ptr0 = tcg_temp_local_new(); \
    tcg_gen_mov_tl(local_ptr0, cpu_ptr0); \
    local_ptr1 = tcg_temp_local_new(); \
    tcg_gen_mov_tl(local_ptr1, cpu_ptr1); \
    local_tmp2_i32 = tcg_temp_local_new(); \
    tcg_gen_mov_i32(local_tmp2_i32, cpu_tmp2_i32); \
    local_tmp3_i32 = tcg_temp_local_new(); \
    tcg_gen_mov_i32(local_tmp3_i32, cpu_tmp3_i32); \
    local_tmp1_i64 = tcg_temp_local_new(); \
    tcg_gen_mov_i64(local_tmp1_i64, cpu_tmp1_i64);

#define RESTORE_TEMPS() \
    tcg_gen_mov_tl(cpu_T[0], local_T[0]); \
    tcg_gen_mov_tl(cpu_T[1], local_T[1]); \
    tcg_gen_mov_tl(cpu_A0, local_A0); \
    tcg_gen_mov_tl(cpu_T3, local_T3); \
    tcg_gen_mov_tl(cpu_tmp0, local_tmp0); \
    tcg_gen_mov_tl(cpu_tmp4, local_tmp4); \
    tcg_gen_mov_tl(cpu_tmp5, local_tmp5); \
    tcg_gen_mov_tl(cpu_ptr0, local_ptr0); \
    tcg_gen_mov_tl(cpu_ptr1, local_ptr1); \
    tcg_gen_mov_tl(cpu_tmp2_i32, local_tmp2_i32); \
    tcg_gen_mov_tl(cpu_tmp3_i32, local_tmp3_i32); \
    tcg_gen_mov_tl(cpu_tmp1_i64, local_tmp1_i64); \
    tcg_temp_free(local_T[0]); \
    tcg_temp_free(local_T[1]); \
    tcg_temp_free(local_A0); \
    tcg_temp_free(local_T3); \
    tcg_temp_free(local_tmp0); \
    tcg_temp_free(local_tmp4); \
    tcg_temp_free(local_tmp5); \
    tcg_temp_free(local_ptr0); \
    tcg_temp_free(local_ptr1); \
    tcg_temp_free(local_tmp2_i32); \
    tcg_temp_free(local_tmp3_i32); \
    tcg_temp_free(local_tmp1_i64);



#define wrap_read_v(idx, t0, a0) \
    wrap_memop_v(idx, t0, a0, gen_op_ld_v, gen_helper_xmem_read)

#define wrap_write_v(idx, t0, a0) \
    wrap_memop_v(idx, t0, a0, gen_op_st_v, gen_helper_xmem_write)

static void wrap_memop_v(int idx, TCGv t0, TCGv a0, 
        void (*opfn)(int, TCGv, TCGv),
        void (*altfn)(TCGv, TCGv_ptr, TCGv, TCGv))
{
    if(cpu_single_env->cpuid_7_0_ebx_features & 
                (CPUID_7_0_EBX_RTM | CPUID_7_0_EBX_HLE))
    {
        int ldone;
        int ltxn;

        TCGv t0_l, a0_l;
        TCGv tmp;
        DEF_LTEMPS();

        ltxn = gen_new_label();
        ldone = gen_new_label();

        /* need locals or we lose value after branch (very bad) */
        t0_l = tcg_temp_local_new();
        a0_l = tcg_temp_local_new();
        tmp = tcg_temp_new();

        LOCALIZE_TEMPS(); /* don't clobber temps */

        tcg_gen_mov_tl(t0_l, t0);
        tcg_gen_mov_tl(a0_l, a0);

        tcg_gen_ld_tl(tmp, cpu_env, offsetof(CPUX86State, rtm_active));

        tcg_gen_brcondi_tl(TCG_COND_NE, tmp, 0, ltxn);
        /* not in txn */
        opfn(idx, t0_l, a0_l);
        tcg_gen_br(ldone);

        gen_set_label(ltxn);
        /* in txn */
        tcg_gen_movi_i32(tmp, idx); 
        if(altfn)
        {
            altfn(t0_l, cpu_env, tmp, a0_l);

#ifdef DEBUG_READWRITE
            if(altfn != gen_helper_xmem_write)
            {
                TCGv tmp2;


                tmp2 = tcg_temp_new();
                opfn(idx, tmp2, a0_l);

                tcg_gen_movi_tl(tmp, 2);
                gen_helper_debug_val(tmp, a0_l);

                tcg_gen_movi_tl(tmp, 0);
                gen_helper_debug_val(tmp, t0_l);
                tcg_gen_movi_tl(tmp, 1);
                gen_helper_debug_val(tmp, tmp2);
                tcg_temp_free(tmp2);
            }
            else
            {
                tcg_gen_movi_tl(tmp, 2);
                gen_helper_debug_val(tmp, a0_l);
                tcg_gen_movi_tl(tmp, 3);
                gen_helper_debug_val(tmp, t0_l);
            }
#endif
        }

        gen_set_label(ldone);
        /* done */

        RESTORE_TEMPS(); /* restore temps to local values */

        tcg_gen_mov_tl(t0, t0_l); /* copy local back to input temp */
        tcg_gen_mov_tl(a0, a0_l);
        
        tcg_temp_free(tmp);
        tcg_temp_free(t0_l);
        tcg_temp_free(a0_l);
    }
    else
        opfn(idx, t0, a0);
}

#endif
