#include <stdio.h>
#include "htm.h"


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

        xabort(2);
        xend();
    }
    else
    {
        fprintf(stderr, "TXN aborted, code %x\n", reason);
        return 0;
    }

    fprintf(stderr, "TXN Ended\n");

    return 0;
}
