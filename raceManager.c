#include "declarations.h"

int main(int argc, char const *argv[])
{
    printf("[%ld] Race Manager process working\n", (long)getpid());
    sleep(5);
    printf("[%ld] Race Manager process finished\n", (long)getpid());
    exit(0);
}
