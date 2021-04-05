#ifndef declarations_h
#define declarations_h

// ------------------ includes ------------------ //

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>

// ------------------ defines ------------------ //

#define ARRAYSIZE 9
#define LINESIZE 100
#define BOX_FREE 0
#define BOX_FULL 1
#define BOX_RESERVED 2

// ------------------ structures ------------------ //

// ------------------ variables ------------------ //

char log_text[LINESIZE];
extern int max_carros;

int shmid;
pid_t raceManagerPID, malfunctionManagerPID;
sem_t *writing;

// ------------------ functions ------------------ //

int *read_content_from_file();
void malfunctionManager(void);
void raceManager(int n_teams);
void *carThread(void *carID_p);
void teamManager(int teamID);
void write_logfile(char *text_to_write);
void print_content_from_file(int *file_contents);

#endif