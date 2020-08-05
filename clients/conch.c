#include <stdio.h>

#define CONCH_CLIENT_IMPLEMENTATION
#include "conch.h"

int main(int argc, char **argv) 
{
    char key[256];
    conch_result result = conch_lease_key("foo", key, sizeof(key));
    if(result != CONCH_SUCCESS)
    {
        printf("Failed to lease a key from the local conch server\n");
    }
    else 
    {
        printf("Successfully leased the key: %s\n", key);
    }

    printf("Press enter to quit\n");
    getc(stdin);
    printf("Done\n");
    return 0;
}
