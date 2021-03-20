// ------------------ includes ------------------ //

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <pthread.h>

// ------------------ defines ------------------ //

#define ARRAYSIZE 8
#define LINESIZE 100

// ------------------ structures ------------------ //

// ------------------ variables ------------------ //

int shmid;
pid_t raceManagerPID, malfunctionManagerPID;

// ------------------ functions ------------------ //

int *read_content_from_file();
void malfunctionManager(void);
void raceManager(int n_teams);
void *carThread(void *carID_p);
void teamManager(int teamID);