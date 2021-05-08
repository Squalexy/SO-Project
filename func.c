#include "declarations.h"

int *read_content_from_file()
{
    FILE *fptr;
    char line[LINESIZE];
    char *token = NULL;

    int *file_contents = (int *)malloc(ARRAYSIZE * sizeof(int));

    if ((fptr = fopen("config.txt", "r")) == NULL)
    {
        write_logfile("ERROR OPENING FILE");
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
                i++;
                token = strtok(NULL, ", ");
                file_contents[i] = atoi(token);
            }
            else
            {
                token = strtok(line, ", ");
                file_contents[i] = atoi(token);
            }
        }
    }

    if (file_contents[3] < 3)
    {
        // perror("Error opening file. Number of teams below 3.\n");
        write_logfile("ERROR OPENING FILE, NUMBER");
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
    signal(SIGTSTP, SIG_IGN);
    // -------------------- PRINT STATISTICS -------------------- //

    //TODO: open statistics file

    exit(0);
}

void sigint(int signum)
{
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