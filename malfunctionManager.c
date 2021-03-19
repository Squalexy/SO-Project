#include "declarations.h"

int main(int argc, char const *argv[])
{
    printf("[%ld] Malfunction Manager process working\n", (long)getpid());
    sleep(5);
    printf("[%ld] Malfunction Manager process finished\n", (long)getpid());
    exit(0);
}
