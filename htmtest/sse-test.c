#include <stdio.h>

int main()
{
    unsigned long long in = 3;
    unsigned long long out;

    asm volatile("movq %1, %%xmm0\n\t"
                 "movq %%xmm0, %0\n\t"
                 : "=m"(out) : "m"(in));

    printf("Output is %llu (expected 3)\n", out);

    return 0;
}
