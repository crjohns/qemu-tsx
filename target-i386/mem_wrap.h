/*
 *  Memory operation wrapper
 *
 *  Author: Christopher R. Johnson <crjohns@csail.mit.edu>
 */

#ifndef MEMWRAP_H
#define MEMWRAP_H

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
        TCGv tmp;
        int ldone;
        int ltxn;

        ltxn = gen_new_label();
        ldone = gen_new_label();

        /* need locals or we lose value after branch (very bad) */
        tmp = tcg_temp_new();

        tcg_gen_ld_tl(tmp, cpu_env, offsetof(CPUX86State, rtm_active));

        tcg_gen_brcondi_tl(TCG_COND_NE, tmp, 0, ltxn);
        /* not in txn */
        opfn(idx, t0, a0);
        tcg_gen_br(ldone);

        gen_set_label(ltxn);
        /* in txn */
        tcg_gen_movi_i32(tmp, idx); 
        if(altfn)
        {
            altfn(t0, cpu_env, tmp, a0);

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

        tcg_temp_free(tmp);
    }
    else
        opfn(idx, t0, a0);
}

#endif
