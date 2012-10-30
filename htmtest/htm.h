#ifndef HTM_H
#define HTM_H


static inline int xtest()
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

#endif
