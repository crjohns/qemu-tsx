#ifndef HTM_H
#define HTM_H


static int xtest()
{
    int ret;
    asm volatile(".byte 0x0f, 0x01, 0xd6\n\t"
                 "jne not_found\n\t"
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

#endif
