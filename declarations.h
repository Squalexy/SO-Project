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
#define PIPE_NAME "np_race_manager"

// ------------------ variables ------------------ //

char log_text[LINESIZE];

int shmid;
pid_t raceManagerPID, malfunctionManagerPID;
sem_t *writing;

// ------------------ structures of shared memory ------------------ //

typedef struct config_struct_
{
    int time_units;
    int turn_distance;
    int turns_number;
    int n_teams;
    int max_carros;
    int T_Avaria;
    int T_Box_min;
    int T_Box_Max;
    int fuel_capacity;
} config_struct;

typedef struct car_struct_
{
    int id_team;
    float combustivel;
    float dist_percorrida;
    int voltas;
    char *state;
} car_struct;

typedef struct team_box_struct_
{
    int box_state;
    car_struct car;
} team_box_struct;

config_struct *config;
car_struct *cars;
team_box_struct *team_box;

// ------------------ functions ------------------ //

int *read_content_from_file();
void malfunctionManager(void);
void raceManager(int n_teams);
void *carThread(void *carID_p);
void teamManager(int channels_write, int teamID);
void write_logfile(char *text_to_write);
void print_content_from_file(int *file_contents);

#endif