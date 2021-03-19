#include "declarations.h"

void initRaceManager(void)
{
    char *argv[] = {"./raceManager", NULL};
    execv("./raceManager", argv);

    // only returns if error
    perror("Race Manager execv error!\n");
    exit(1);
}

void initMalfunctionManager(void)
{
    char *argv[] = {"./malfunctionManager", NULL};
    execv("./malfunctionManager", argv);

    // only returns if error
    perror("Malfunction Manager execv error!\n");
    exit(1);
}

int main(int argc, char const *argv[])
{
    pid_t raceManagerPID, malfunctionManagerPID;

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
        initRaceManager();
    }
    else if (raceManagerPID == -1)
    {
        perror("Error creating Race Manager process\n");
        exit(1);
    }

    if ((malfunctionManagerPID = fork()) == 0)
    {
        printf("[%ld] Malfunction Manager process created\n", (long)getpid());
        initMalfunctionManager();
    }
    else if (malfunctionManagerPID == -1)
    {
        perror("Error creating Malfunction Manager process\n");
        exit(1);
    }

    // TODO: STUFF

    waitpid(raceManagerPID, 0, 0);
    waitpid(malfunctionManagerPID, 0, 0);

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}
