#include <pthread.h>
#include <stdio.h>
#include "htm.h"

#define READ_IN_TXN 1

void *reader(void *arg)
{
    int *iarg = (int*)arg;
    for(int i=0; i<100000; i++)
    {
        asm volatile("":::"memory");
#if READ_IN_TXN
        *iarg += 1;
#else
        if(*iarg -= 1)
            return NULL;
#endif
    }

    return NULL;
}


int temp = 0;

int main(int argc, char **argv)
{
    pthread_t thr;
    pthread_create(&thr, NULL, reader, &temp);

    if(xbegin() == 0)
    {
        for(int i=0; i<100000; i++)
        {
            asm volatile("":::"memory");
#if READ_IN_TXN
            if(temp == -1 == 0)
                return 1;
#else
            temp += 1;
#endif
        }

        xend();
    }
    else
    {
        fprintf(stderr, "Aborted\n");
        return 0;
    }

    fprintf(stderr, "Successful\n");

    return 0;
}


