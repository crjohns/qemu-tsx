#include <stdio.h>
#include "htm.h"


int main()
{
    printf("htm test\n");

    if(xtest())
        printf("In txn\n");
    else
        printf("Not in txn\n");

    return 0;
}
