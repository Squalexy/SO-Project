// compilar: gcc -c -Wall main.c -o main

#include "declarations.h"

int main()
{

    int *file_contents = read_content_from_file();

    //int time_units = file_contents[0];
    //int turn_distance = file_contents[1];
    //int n_voltas = file_contents[2];
    int n_teams = file_contents[3];
    printf("File contents [3]: %d\n", file_contents[3]);
    //int T_Avaria = file_contents[4];
    //int T_Box_min = file_contents[5];
    //int T_Box_Max = file_contents[6];
    //int fuel_capacity = file_contents[7];

    for (int i = 0; i < ARRAYSIZE; i++)
    {
        printf("%d\n", file_contents[i]);
    }

    typedef struct mem_struct
    {
        char box_state[n_teams][LINESIZE];
    } mem_struct;

    mem_struct *mem;

    // shared memory
    if ((shmid = shmget(IPC_PRIVATE, sizeof(mem_struct), IPC_CREAT | 0766)) < 0)
    {
        perror("shmget error!\n");
        exit(1);
    }

    if ((mem = (mem_struct *)shmat(shmid, NULL, 0)) == (mem_struct *)-1)
    {
        perror("shmat error!\n");
        exit(1);
    }

    if ((raceManagerPID = fork()) == 0)
    {
        printf("[%ld] Race Manager process created\n", (long)getpid());
        raceManager(n_teams);
    }
    else if (raceManagerPID == -1)
    {
        perror("Error creating Race Manager process\n");
        exit(1);
    }

    if ((malfunctionManagerPID = fork()) == 0)
    {
        printf("[%ld] Malfunction Manager process created\n", (long)getpid());
        malfunctionManager();
    }
    else if (malfunctionManagerPID == -1)
    {
        perror("Error creating Malfunction Manager process\n");
        exit(1);
    }

    // -------------------- RACE START -------------------- //

    // TODO: STUFF

    /*
    waitpid(raceManagerPID, 0, 0);
    waitpid(malfunctionManagerPID, 0, 0);

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);*/

    return 0;
}
