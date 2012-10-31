#ifndef HTM_H
#define HTM_H


static int xtest()
{
    register int ret;
    asm volatile(".byte 0x0f, 0x01, 0xd6\n\t"
                 "je not_found\n\t"
                 "movl $0x1, %0\n\t"
                 "jmp end\n\t"
                 "not_found:;\n\t"
                 "movl $0x0, %0\n\t"
                 "end:;\n\t": "=a"(ret) : );

    return ret;
}

static void elock_acquire(int *lock)
{
    asm volatile("xorl %%ecx, %%ecx\n\t"
                 "incl %%ecx\n\t"
                 "1:\n\t"
                 "xorl %%eax, %%eax\n\t"
                 ".byte 0xf2\n\t" /* gas refuses to add REPNZ on non-string op
                                     XACQUIRE prefix is the same as REPNZ 
                                     (0xf2)
                                  */
                 "LOCK cmpxchgl %%ecx, %0\n\t"
                 "jnz 1b\n\t"
                 : : "m" (*lock) : "%eax", "%ecx");
}

static void elock_release(int *lock)
{
    asm volatile("xorl %%ecx, %%ecx\n\t"
                 "1:\n\t"
                 "xorl %%eax, %%eax\n\t"
                 "incl %%eax\n\t"
                 ".byte 0xf3\n\t" /* gas refuses to add REPZ on non-string op
                                     XRELEASE prefix is the same as REPZ 
                                     (0xf3)
                                  */
                 "LOCK cmpxchgl %%ecx, %0\n\t"
                 "jnz 1b\n\t"
                 : : "m" (*lock) : "%eax", "%ecx");

}

#define XBEGIN_OP(jmp) ".byte 0xc7, 0xf8; " ".long " #jmp "\n\t"

static unsigned int xbegin()
{
    register unsigned int ret;
    asm volatile(XBEGIN_OP(2)
                 "jmp 1f\n\t" /* TXM abort skips this instruction */
                 "movl %%eax, %0\n\t" /* TXN abort, return error */
                 "jmp 2f\n\t"
                 "1:\n\t" /* In TXN, return 0 */
                 "movl $0, %0\n\t"
                 "2:\n\t"
                 : "=r"(ret) : : "%eax");

    return ret;
}

#define XEND_OP ".byte 0x0f, 0x01, 0xd5\n\t"
static void xend()
{
    asm volatile(XEND_OP);
}


/* defined as macro since xabort required encoded immediate */
#define xabort(imm) asm(".byte 0xc6, 0xf8, " #imm "\n\t"\
                        "nop;nop;nop;nop;nop;nop;nop;nop;\n\t"); /* nop slide for clean disasm */

#endif
