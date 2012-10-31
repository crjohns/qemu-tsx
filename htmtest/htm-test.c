#include <stdio.h>
#include "htm.h"


int main()
{
    printf("htm test\n");

    if(xtest())
        printf("In txn\n");
    else
        printf("Not in txn\n");

    int lock;
    elock_acquire(&lock);
    elock_release(&lock);
    printf("Did lock test\n");


    int reason;
    if((reason = xbegin()) == 0)
    {
        if(xtest())
            printf("In txn\n");
        else
            printf("Not in txn\n");

        xabort(2);
        xend();
    }
    else
    {
        printf("TXN aborted");
        return 0;
    }

    printf("TXN Ended");

    return 0;
}
