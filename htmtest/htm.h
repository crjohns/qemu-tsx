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


__attribute__ ((unused))
static int xtest()
{
    register int ret;
    asm volatile("xor %%eax, %%eax\n\t"
                 ".byte 0x0f, 0x01, 0xd6\n\t"
                 "setne %%al\n\t"
                 "mov %%eax, %0\n\t": "=r"(ret) : : "%eax", "memory");

    return ret;
}

__attribute__ ((unused))
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

__attribute__ ((unused))
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

__attribute__ ((unused))
static unsigned int xbegin()
{
    register unsigned int ret;
//    unsigned long long int v1, v2, v3, v4, v5;
    asm volatile(XBEGIN_OP(2)
                 "jmp 1f\n\t" /* TXN abort skips this instruction */
                 "movl %%eax, %0\n\t" /* TXN abort, return error */
                 "jmp 2f\n\t"
                 "1:\n\t" /* In TXN, return 0 */
                 "movl $0, %0\n\t"
                 "2:\n\t"
                 : "=r"(ret) : : "%eax");

    asm volatile("":::"memory");

#if 0
    fprintf(stderr, "ret is 0x%x\n", ret);


    asm volatile("mov %%rsp, %0\n\t"
                 "mov 0x40(%%rsp), %%rax\n\t"
                 "mov %%rax, %1\n\t"
                 "mov 0x48(%%rsp), %%rax\n\t"
                 "mov %%rax, %2\n\t"
                 "mov 0x50(%%rsp), %%rax\n\t"
                 "mov %%rax, %3\n\t"
                 "call 1f\n\t"
                 "1: pop %%rax\n\t"
                 "mov %%rax, %4\n\t"
                 : "=m"(v1),"=m"(v2),"=m"(v3),"=m"(v4),"=m"(v5) : : "%rax");

    fprintf(stderr, "[ip %x] rsp %llx, s0 %llx, s1 %llx, s2 %llx\n", v5, v1, v2, v3, v4);
#endif

    return ret;
}

#define XEND_OP ".byte 0x0f, 0x01, 0xd5\n\t"
__attribute__ ((unused))
static void xend()
{
    asm volatile(XEND_OP);
    asm volatile("":::"memory");
}


/* defined as macro since xabort required encoded immediate */
#define xabort(imm) { asm volatile(".byte 0xc6, 0xf8, " #imm "\n\t"\
                        "nop;nop;nop;nop;nop;nop;nop;nop;\n\t"); } /* nop slide for clean disasm */

#endif
