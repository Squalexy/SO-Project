/*
Projeto realizado por:
    Alexy de Almeida nº2019192123
    José Gonçalves nº2019223292
*/

#include "declarations.h"

int main(){
    // -------------------- CREATE NAMED SEMAPHORES -------------------- //

    sem_unlink("WRITING");
    writing = sem_open("WRITING", O_CREAT | O_EXCL, 0700, 1);

    // open log file
    log = fopen("log.txt", "w");
    if (log == NULL)
    {
        perror("Error opening log file.\n");
        exit(1);
    }

    // -------------------- READ CONTENT FROM CONFIG FILE -------------------- //
    int *file_contents = read_content_from_file();

    // -------------------- RESET LOG FILE -------------------- //

    fclose(fopen("log.txt", "w"));

    // -------------------- CREATE SHARED MEMORY -------------------- //


    if ((shmid = shmget(IPC_PRIVATE, sizeof(config_struct) + sizeof(race_state) + (sizeof(team_struct) *  file_contents[3]) + (sizeof(car_struct) * file_contents[4] *  file_contents[3]), IPC_CREAT | 0766)) < 0)
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
    all_teams = (team_struct *)(mem + sizeof(config_struct) + sizeof(race_state));
    cars = (car_struct *)(mem + sizeof(config_struct) + sizeof(race_state) + (sizeof(team_struct) * file_contents[3]));

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
    pthread_cond_init(&race->cv_allow_start, &attrcondv);
    pthread_cond_init(&race->cv_allow_teams, &attrcondv);

    race->race_started = 0;
    race->threads_created = -1;
    race->car_count = 0;

    // -------------------- CREATE LOG FILE STRUCT -------------------- //

    config->time_units = file_contents[0];
    config->turn_distance = file_contents[1];
    config->turns_number = file_contents[2];
    config->n_teams = file_contents[3];
    config->max_carros = file_contents[4];
    config->T_Avaria = file_contents[5];
    config->T_Box_min = file_contents[6];
    config->T_Box_Max = file_contents[7];
    config->fuel_capacity = file_contents[8];
    
    // PRINT CONTENT FROM LOG FILE
    print_content_from_file(file_contents);

    // -------------------- CREATE NAMED PIPE -------------------- //

    unlink(PIPE_NAME);
    if ((mkfifo(PIPE_NAME, O_CREAT | O_EXCL | 0600) < 0) && (errno != EEXIST))
    {
        perror("Cannot create pipe!\n");
        exit(1);
    }

    // -------------------- CREATE MESSAGE QUEUE -------------------- //

    mqid = msgget(IPC_PRIVATE, IPC_CREAT | 0777);
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
        signal(SIGINT, sigint_race);
        raceManager();
    }
    else if (raceManagerPID == -1)
    {
        // perror("Error creating Race Manager process\n");
        write_logfile("ERROR CREATING RACE MANAGER PROCESS");
        exit(1);
    }

    // -------------------- MALFUNCTION MANAGER START -------------------- //

    if ((malfunctionManagerPID = fork()) == 0)
    {
        signal(SIGINT, sigint_malfunction);
        malfunctionManager();
    }
    else if (malfunctionManagerPID == -1)
    {
        // perror("Error creating Malfunction Manager process\n");
        write_logfile("ERROR CREATING MALFUNCTION MANAGER PROCESS");
        exit(1);
    }

    // -------------------- CAPTURE SIGNALS -------------------- //
    signal(SIGINT, sigint_simulator);
    signal(SIGTSTP, sigtstp);

    // -------------------- SIMULATOR END -------------------- //
    
    waitpid(raceManagerPID, 0, 0);
    waitpid(malfunctionManagerPID, 0, 0);

    write_logfile("SIMULATOR CLOSING");

    //clean_resources();


    return 0;
}
