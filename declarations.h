#ifndef declarations_h
#define declarations_h

// ------------------ includes ------------------ //

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>

// ------------------ defines ------------------ //

#define ARRAYSIZE 9
#define LINESIZE 200
#define TEAM_NAME_SIZE 64

// box states
#define BOX_FREE 0
#define BOX_FULL 1
#define BOX_RESERVED 2

// car states
#define BOX 1
#define CORRIDA 2
#define SEGURANCA 3
#define DESISTENCIA 4
#define TERMINADO 5

#define WORKING 0
#define MALFUNCTION 1

// named pipe
#define PIPE_NAME "np_race_manager"

// ------------------ variables ------------------ //

char log_text[LINESIZE];

int shmid;
int mqid;
pid_t raceManagerPID, malfunctionManagerPID;
sem_t *writing;
char *mem;

pthread_mutexattr_t attrmutex;
pthread_condattr_t attrcondv;

pthread_mutex_t mutex_box;
pthread_cond_t cond_box_free;
pthread_cond_t cond_box_full;

// log file
FILE *log_file;

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

typedef struct race_state_struct
{
    // race state
    int race_started;
    int threads_created;
    int car_count;
    int team_count;
    int waiting;
    int end_sim;
    pthread_mutex_t race_mutex;
    pthread_cond_t cv_race_started;
    pthread_cond_t cv_allow_start;
    pthread_cond_t cv_allow_teams;
    pthread_cond_t cv_waiting;

    // statistics
    int n_avarias;
    int n_abastecimentos;
    int classificacao;
} race_state;

typedef struct car_struct_
{
    char team[TEAM_NAME_SIZE];
    int num;
    int avaria;
    float combustivel;
    float dist_percorrida;
    int voltas;
    int state;
    float speed;
    float consumption;
    int reliability;

    // statistics
    int n_stops_box;
    int classificacao;
} car_struct;

typedef struct team_struct_
{
    char name[TEAM_NAME_SIZE];
    // box
    int car_id;
    int box_state;
    int reserved_count;
    int finished;
    int number_of_cars;
    // mutex semaphores
    //pthread_mutex_t mutex_box;
    //pthread_mutex_t mutex_car_state_box;
    // condition variables
    //pthread_cond_t cond_box_full;
    //pthread_cond_t cond_box_free;
} team_struct;

config_struct *config;
race_state *race;
car_struct *cars;
team_struct *all_teams;

// ------------------ other structures ------------------ //

typedef struct
{
    long car_id;
} msg;

// ------------------ processes ------------------ //

void malfunctionManager(void);
void raceManager();
void *carThread(void *carID_p);
void teamManager();

// ------------------ functions ------------------ //
void write_logfile(char *text_to_write);
void print_content_from_file(int *file_contents);
int *read_content_from_file();
void sigtstp(int signum);
void sigint(int signum);
void clean_resources();
void sigint_simulator(int signo);
void sigint_race(int signo);
void sigint_malfunction(int signo);
void sigint_team(int signo);
bool check_int(char *variable);
int check_commands(int car_num, int car_speed, int car_reliability, int converted, float car_consumption, char **fields, char *team_name, int team_count);
int update_or_create_team(int team_count, char* team_name);
void create_car_struct(char *team_name, int car_num, int car_speed, int car_consumption, int car_reliability);

// other
pid_t *teamsPID;
int fd_named_pipe, count, teamID;
int **channels;
pthread_t *carThreads;
int *car_IDs;
car_struct *sort_classif();
void print_statistics();

sigset_t set_all, set_allow;

#endif
