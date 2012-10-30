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

    return 0;
}
