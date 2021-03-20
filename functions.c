#include "declarations.h"
#include <pthread.h>

int *read_content_from_file()
{
    FILE *fptr;
    char line[LINESIZE];
    char *token = NULL;

    int *file_contents = (int *)malloc(ARRAYSIZE * sizeof(int));

    if ((fptr = fopen("config.txt", "r")) == NULL)
    {
        printf("Error! opening file\n");
        exit(1);
    }

    for (int i = 0; i < ARRAYSIZE; i++)
    {
        if (fgets(line, LINESIZE, fptr) != NULL)
        {
            if (strchr(line, ',') != NULL)
            {
                token = strtok(line, ",");
                file_contents[i] = atoi(token);
                i++;
                token = strtok(NULL, "");
                file_contents[i] = atoi(token);
            }
            else
                file_contents[i] = atoi(line);
        }
    }

    if (file_contents[3] < 3)
    {
        perror("Error opening file. Number of teams below 3.\n");
        exit(1);
    }

    fclose(fptr);
    return file_contents;
}

void malfunctionManager(void)
{
    printf("[%ld] Malfunction Manager process working\n", (long)getpid());
    sleep(5);
    printf("[%ld] Malfunction Manager process finished\n", (long)getpid());
    exit(0);
}

void raceManager(int n_teams)
{
    printf("[%ld] Race Manager process working\n", (long)getpid());

    pid_t teams[n_teams];
    for (int i = 0; i < n_teams; i++)
    {
        pid_t teamPID;
        if ((teamPID = fork()) == 0)
        {
            teamManager(i);
        }
        else if (teamPID == -1)
        {
            perror("Error creating Team Manager process\n");
            exit(1);
        }
        teams[i] = teamPID;
    }

    // wait for teams
    for (int i = 0; i < n_teams; i++)
    {
        waitpid(teams[i], 0, 0);
    }

    printf("[%ld] Race Manager process finished\n", (long)getpid());
    exit(0);
}

void *carThread(void *carID_p)
{
    int carID = *((int *)carID_p);
    printf("[%ld] Car #%d thread working\n", (long)getpid(), carID);
    sleep(5);
    printf("[%ld] Car #%d thread finished\n", (long)getpid(), carID);
    pthread_exit(0);
}

void teamManager(int teamID)
{
    printf("[%ld] Team Manager #%d process working\n", (long)getpid(), teamID);
    // TODO: get num cars from ipc
    int numCar = 2;
    pthread_t carThreads[numCar];
    int carID[numCar];

    // create car threads
    for (int i = 0; i < numCar; i++)
    {
        carID[i] = i;
        pthread_create(&carThreads[i], NULL, carThread, &carID[i]);
        printf("[%ld] Car #%d thread created\n", (long)getpid(), carID[i]);
    }

    // join car threads
    for (int i = 0; i < numCar; i++)
    {
        pthread_join(carThreads[i], NULL);
    }

    printf("[%ld] Team Manager #%d process finished\n", (long)getpid(), teamID);
    exit(0);
}