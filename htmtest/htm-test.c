#include <stdio.h>
#include "htm.h"


int testfn()
{
    asm volatile("nop");
    return 4;
}

int main()
{
    fprintf(stderr, "htm test\n");

    if(xtest())
        fprintf(stderr, "In txn\n");
    else
        fprintf(stderr, "Not in txn\n");

    int lock;
    elock_acquire(&lock);
    elock_release(&lock);
    printf("Did lock test\n");

    int volatile *temp = (int volatile *) malloc(sizeof(int));

    *temp = 5;


    fprintf(stderr, "Do TXN test\n");
    int reason;
    reason = xbegin();
    //fprintf(stderr, "reason is 0x%x\n", reason);
    if(reason == 0)
    {
        //if(xtest())
        //    fprintf(stderr, "In txn\n");
        //else
        //    fprintf(stderr, "Not in txn\n");
        

        // This introduces a bug with stack handling?
        // int val = testfn();
        
        *temp += 100;
        
        if(*temp == 105)
        {
            xabort(3);
        }
        else
        {
            xabort(2);
        }

        xend();
        fprintf(stderr, "TXN Ended\n");
    }
    else
    {
        fprintf(stderr, "TXN aborted, code %x\n", reason);
    }

    fprintf(stderr, "Temporary value is %x\n", *temp);

    free((void*)temp);

    return 0;
}
