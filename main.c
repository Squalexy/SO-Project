/*
Projeto realizado por:
    Alexy de Almeida nº2019192123
    José Gonçalves nº2019223292
*/

#include "declarations.h"

int main()
{
    // -------------------- CREATE SEMAPHORES -------------------- //

    sem_unlink("WRITING");
    writing = sem_open("WRITING", O_CREAT | O_EXCL, 0700, 1);

    // variable conditions
    pthread_cond_t car_state = PTHREAD_COND_INITIALIZER;

    // -------------------- RESET LOG FILE -------------------- //

    // fclose(fopen("log.txt", "w"));

    // -------------------- CREATE SHARED MEMORY -------------------- //

    char *mem;

    if ((shmid = shmget(IPC_PRIVATE, sizeof(config_struct) + sizeof(car_struct) * config->max_carros * config->n_teams + sizeof(team_box_struct) * config->n_teams, IPC_CREAT | 0766)) < 0)
    {
        perror("shmget error!\n");
        exit(1);
    }

    if ((mem = (char *)shmat(shmid, NULL, 0)) == (char *)-1)
    {
        perror("shmat error!\n");
        exit(1);
    }

    config = (config_struct *)mem;
    race = (race_state *)(mem + sizeof(config_struct));
    cars = (car_struct *)(mem + sizeof(config_struct) + sizeof(race_state));
    team_box = (team_box_struct *)(mem + sizeof(config_struct) + sizeof(race_state) + config->max_carros * config->n_teams * sizeof(car_struct));

    // -------------------- READ CONTENT FROM LOG FILE AND CREATE LOG FILE STRUCT -------------------- //

    int *file_contents = read_content_from_file();
    config->time_units = file_contents[0];
    config->turn_distance = file_contents[1];
    config->turns_number = file_contents[2];
    config->n_teams = file_contents[3];
    config->max_carros = file_contents[4];
    config->T_Avaria = file_contents[5];
    config->T_Box_min = file_contents[6];
    config->T_Box_Max = file_contents[7];
    config->fuel_capacity = file_contents[8];

    // -------------------- PRINT CONTENT FROM LOG FILE -------------------- //

    print_content_from_file(file_contents);

    // -------------------- CREATE MUTEX SEMAPHORES -------------------- //

    mutex_box = PTHREAD_MUTEX_INITIALIZER;
    mutex_car_state_box = PTHREAD_MUTEX_INITIALIZER;

    // -------------------- CONDITION VARIABLES -------------------- //

    pthread_mutexattr_t attrmutex;
    pthread_condattr_t attrcondv;

    cond_box_full = PTHREAD_COND_INITIALIZER;
    cond_box_free = PTHREAD_COND_INITIALIZER;

    /* Initialize attribute of race_mutex. */
    pthread_mutexattr_init(&attrmutex);
    pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);

    /* Initialize attribute of condition variable. */
    pthread_condattr_init(&attrcondv);
    pthread_condattr_setpshared(&attrcondv, PTHREAD_PROCESS_SHARED);

    /* Initialize race_mutex. */
    pthread_mutex_init(&race->race_mutex, &attrmutex);

    /* Initialize condition variables. */
    pthread_cond_init(&race->cv_race_started, &attrcondv);
    pthread_cond_init(&race->cv_allow_pipe, &attrcondv);
    pthread_cond_init(&race->cv_allow_teams, &attrcondv);

    race->race_started = 0;
    race->teams_reading = 0;
    race->car_count = 0;

    // -------------------- CREATE NAMED PIPE -------------------- //

    unlink(PIPE_NAME);
    if ((mkfifo(PIPE_NAME, O_CREAT | O_EXCL | 0600) < 0) && (errno != EEXIST))
    {
        perror("Cannot create pipe!\n");
        exit(1);
    }

    // -------------------- CREATE MESSAGE QUEUE -------------------- //

    mqid = mssget(IPC_PRIVATE, IPC_CREAT | 0777);
    if (mqid < 0)
    {
        perror("Creating message queue");
        exit(0);
    }

    // -------------------- SIMULATOR START -------------------- //

    if ((raceManagerPID = fork()) == 0)
    {
        // printf("[%ld] Race Manager process created\n", (long)getpid());
        write_logfile("SIMULATOR STARTING");
        raceManager();
    }
    else if (raceManagerPID == -1)
    {
        // perror("Error creating Race Manager process\n");
        write_logfile("ERROR CREATING RACE MANAGER PROCESS");
        exit(1);
    }

    fptr = fopen("log.txt", "w");
    if (fptr == NULL)
    {
        perror("Error opening log file.\n");
        exit(1);
    }

    // -------------------- MALFUNCTION MANAGER START -------------------- //

    if ((malfunctionManagerPID = fork()) == 0)
    {
        malfunctionManager();
    }
    else if (malfunctionManagerPID == -1)
    {
        // perror("Error creating Malfunction Manager process\n");
        write_logfile("ERROR CREATING MALFUNCTION MANAGER PROCESS");
        exit(1);
    }

    // -------------------- SIMULATOR END -------------------- //

    waitpid(raceManagerPID, 0, 0);
    waitpid(malfunctionManagerPID, 0, 0);

    write_logfile("SIMULATOR CLOSING");

    // close named pipe
    unlink(PIPE_NAME);

    // clean up semaphores
    sem_close(writing);
    sem_unlink("WRITING");

    pthread_cond_destroy(&car_state);
    pthread_cond_destroy(&race->cv_race_started);
    pthread_cond_destroy(&race->cv_allow_pipe);
    pthread_cond_destroy(&race->cv_allow_teams);
    pthread_mutex_destroy(&race->race_mutex);
    pthread_mutexattr_destroy(&attrmutex);
    pthread_condattr_destroy(&attrcondv);
    //pthread_exit(NULL);

    fclose(fptr);

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
