#include "declarations.h"

int *read_content_from_file()
{
    FILE *fptr;
    char line[LINESIZE];
    char *token = NULL;

    int *file_contents = (int *)malloc(ARRAYSIZE * sizeof(int));

    if ((fptr = fopen("config.txt", "r")) == NULL)
    {
        write_logfile("ERROR OPENING CONFIG FILE");
        exit(1);
    }

    for (int i = 0; i < ARRAYSIZE; i++)
    {
        if (fgets(line, LINESIZE, fptr) != NULL)
        {
            if (strchr(line, ',') != NULL)
            {
                token = strtok(line, ", ");
                file_contents[i] = atoi(token);
                if (token < 0)
                {
                    write_logfile("ERROR READING CONFIG FILE. A NEGATIVE NUMBER WAS INSERETED");
                    exit(1);
                }
                i++;
                token = strtok(NULL, ", ");
                file_contents[i] = atoi(token);
                if (token < 0)
                {
                    write_logfile("ERROR READING CONFIG FILE. A NEGATIVE NUMBER WAS INSERETED");
                    exit(1);
                }
            }
            else
            {
                token = strtok(line, ", ");
                file_contents[i] = atoi(token);
                if (token < 0)
                {
                    write_logfile("ERROR READING CONFIG FILE. A NEGATIVE NUMBER WAS INSERETED");
                    exit(1);
                }
            }
        }
    }

    if ((sizeof(file_contents) / sizeof(int)) > 8)
    {
        write_logfile("ERROR READING CONFIG FILE. TOO MANY ARGUMENTS");
        exit(1);
    }

    if (file_contents[3] < 3)
    {
        write_logfile("ERROR OPENING CONFIG FILE, NUMBER OF TEAMS BELOW 3");
        exit(1);
    }

    fclose(fptr);
    return file_contents;
}

void print_content_from_file(int *file_contents)
{
    printf("--- CONTENT FROM FILE ---\n");
    for (int i = 0; i < ARRAYSIZE; i++)
    {
        if (i == 0)
            printf("TIME UNITS : %d\n", file_contents[i]);
        else if (i == 1)
            printf("TURN DISTANCE : %d\n", file_contents[i]);
        else if (i == 2)
            printf("TURNS NUMBER : %d\n", file_contents[i]);
        else if (i == 3)
            printf("TEAMS NUMBER : %d\n", file_contents[i]);
        else if (i == 4)
            printf("CARS LIMIT : %d\n", file_contents[i]);
        else if (i == 5)
            printf("T_AVARIA : %d\n", file_contents[i]);
        else if (i == 6)
            printf("T_BOX_MIN : %d\n", file_contents[i]);
        else if (i == 7)
            printf("T_BOX_MAX : %d\n", file_contents[i]);
        else if (i == 8)
            printf("FUEL CAPACITY : %d\n", file_contents[i]);
    }
    printf("\n");
}

void write_logfile(char *text_to_write)
{
    int hours, minutes, seconds;
    time_t now = time(NULL);

    struct tm *local = localtime(&now);
    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;

    sem_wait(writing);
    printf("%02d:%02d:%02d  %s\n", hours, minutes, seconds, text_to_write);
    fprintf(log, "%02d:%02d:%02d  %s\n", hours, minutes, seconds, text_to_write);
    sem_post(writing);
}

void sigtstp(int signum)
{
    write_logfile("SIGNAL SIGSTP RECEIVED");
    signal(SIGTSTP, SIG_IGN);
    // -------------------- PRINT STATISTICS -------------------- //
    printf("\n*******************************************************\n");
    //TODO: top 5, ordem decrescente
    //TODO: carro em último lugar

    printf("Total de avarias: %d", race->n_avarias);
    printf("Total de abastecimentos: %d", race->n_abastecimentos);

    printf("\n*******************************************************\n");
    exit(0);
}

void sigint(int signum)
{
    write_logfile("SIGNAL SIGINT RECEIVED");
    signal(SIGINT, SIG_IGN);

    // 1) Wait for cars to get to the line AND cars in boxes
    for (int i = 0; i < config->n_teams * config->max_carros; i++)
    {
        // carros na box terminam
        if (cars[i].state == BOX)
        {
            cars[i].state = TERMINADO;
        }
    }

    // 2) Print statistics

    // 3) Clean resources
    clean_resources();
}

void clean_resources()
{
    // log file
    fclose(log);

    // close named semaphores
    sem_close(writing);
    sem_unlink("WRITING");

    // destroy all semaphores and CVs
    pthread_cond_destroy(&race->cv_race_started);
    pthread_cond_destroy(&race->cv_allow_start);
    pthread_cond_destroy(&race->cv_allow_teams);
    pthread_mutex_destroy(&race->race_mutex);
    pthread_mutexattr_destroy(&attrmutex);
    pthread_condattr_destroy(&attrcondv);

    for (int i = 0; i < all_teams; i++)
    {
        pthread_mutex_destroy(&all_teams[i].mutex_box);
        pthread_mutex_destroy(&all_teams[i].mutex_car_state_box);
        pthread_cond_destroy(&all_teams[i].cond_box_free);
        pthread_cond_destroy(&all_teams[i].cond_box_full);
    }

    // Guarantees that every process receives a SIGTERM , to kill them
    kill(0, SIGTERM);
    while (wait(NULL) != -1)
        ;

    // remove MSQ
    msgctl(mqid, IPC_RMID, 0);

    // shared memory
    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);

    exit(0);
}

void sigint_simulator(int signo)
{
    signal(SIGINT, SIG_IGN);
    write_logfile("SIGNAL SIGINT RECEIVED");

    waitpid(raceManagerPID, 0, 0);
    waitpid(malfunctionManagerPID, 0, 0);

    fclose(log);

    // close named semaphores
    sem_close(writing);
    sem_unlink("WRITING");

    // destroy all semaphores and CVs
    pthread_cond_destroy(&race->cv_race_started);
    printf("cv_race_started CLOSED [%d]\n", errno);
    pthread_cond_destroy(&race->cv_allow_start);
    printf("cv_allow_start CLOSED [%d]\n", errno);
    pthread_cond_destroy(&race->cv_allow_teams);
    printf("cv_allow_teams CLOSED [%d]\n", errno);
    pthread_mutex_destroy(&race->race_mutex);
    printf("race_mutex CLOSED [%d]\n", errno);
    pthread_mutexattr_destroy(&attrmutex);
    printf("attrmutex CLOSED [%d]\n", errno);
    pthread_condattr_destroy(&attrcondv);
    printf("attrcondv CLOSED [%d]\n", errno);

    // remove MSQ
    msgctl(mqid, IPC_RMID, 0);

    unlink(PIPE_NAME);

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);

    exit(0);
}

void sigint_race(int signo)
{
    signal(SIGINT, SIG_IGN);
    write_logfile("RACE MANAGER CLEANING");
    
    pthread_mutex_lock(&race->race_mutex);
    race->race_started = 0;
    pthread_cond_broadcast(&race->cv_race_started);
    pthread_mutex_unlock(&race->race_mutex);

    // WAIT FOR TEAMS
    for (int i = 0; i < config->n_teams; i++)
    {
        waitpid(teamsPID[i], NULL, 0);
        close(channels[i][0]);
        free(channels[i]);
    }
    free(teamsPID);
    free(channels);

    close(fd_named_pipe);

    printf("[%ld] Race Manager process finished\n", (long)getpid());
    
    exit(0);
}

void sigint_malfunction(int signo)
{
    signal(SIGINT, SIG_IGN);
    write_logfile("MALFUNCTION MANAGER FINISHED");
    exit(0);
}

void sigint_team(int signo)
{
    signal(SIGINT, SIG_IGN);
    write_logfile("TEAM MANAGER CLEANING");

    // WAIT FOR CAR THREADS TO DIE
    for (int i = 0; i < count; i++)
    {
        pthread_join(carThreads[i], NULL);
    }
    close(channels[teamID][1]);
    free(carThreads);

    pthread_mutex_destroy(&all_teams[teamID].mutex_box);
    pthread_mutex_destroy(&all_teams[teamID].mutex_car_state_box);
    pthread_cond_destroy(&all_teams[teamID].cond_box_free);
    //pthread_cond_destroy(&all_teams[teamID].cond_box_full);
    //printf("cond box full CLOSED\n");

    printf("[%ld] Team Manager #%d process finished\n", (long)getpid(), teamID);
    exit(0);
}
