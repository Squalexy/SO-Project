// ------------------ includes ------------------ //

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

// ------------------ defines ------------------ //

#define ARRAYSIZE 8
#define LINESIZE 100

// ------------------ structures ------------------ //

typedef struct mem_struct
{
    int placeholder;
} mem_struct;

// ------------------ variables ------------------ //
int shmid;
mem_struct *mem;

// ------------------ functions ------------------ //

int *read_content_from_file();