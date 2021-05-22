#include "declarations.h"

int *read_content_from_file()
{
    FILE *fptr;
    char line[LINESIZE];
    char *token = NULL;

    int *file_contents = malloc(ARRAYSIZE * sizeof(int));

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
                if (file_contents[i] < 0)
                {
                    write_logfile("ERROR READING CONFIG FILE. A NEGATIVE NUMBER WAS INSERTED");
                    exit(1);
                }
                i++;
                token = strtok(NULL, ", ");
                file_contents[i] = atoi(token);
                if (file_contents[i] < 0)
                {
                    write_logfile("ERROR READING CONFIG FILE. A NEGATIVE NUMBER WAS INSERTED");
                    exit(1);
                }
            }
            else
            {
                token = strtok(line, ", ");
                file_contents[i] = atoi(token);
                if (file_contents[i] < 0)
                {
                    write_logfile("ERROR READING CONFIG FILE. A NEGATIVE NUMBER WAS INSERETED");
                    exit(1);
                }
            }
        }
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
    fprintf(log_file, "%02d:%02d:%02d  %s\n", hours, minutes, seconds, text_to_write);
    fflush(stdout);
    fflush(log_file);
    sem_post(writing);
}

void sigtstp(int signum)
{
    write_logfile("SIGNAL SIGSTP RECEIVED");
    print_statistics();
}

void sigint_simulator(int signo)
{
    signal(SIGINT, SIG_IGN);
    write_logfile("SIGNAL SIGINT RECEIVED");

    pthread_mutex_lock(&race->race_mutex);
    race->end_sim = 1;
    pthread_cond_broadcast(&race->cv_race_started);
    pthread_mutex_unlock(&race->race_mutex);

    waitpid(malfunctionManagerPID, 0, 0);
    waitpid(raceManagerPID, 0, 0);

    fclose(log_file);

    // close named semaphores
    sem_close(writing);
    sem_unlink("WRITING");

    // destroy all semaphores and CVs
    pthread_mutexattr_destroy(&attrmutex);
    pthread_condattr_destroy(&attrcondv);

    pthread_cond_destroy(&race->cv_race_started);
    pthread_cond_destroy(&race->cv_allow_start);
    pthread_cond_destroy(&race->cv_allow_teams);
    pthread_cond_destroy(&race->cv_waiting);

    pthread_mutex_destroy(&race->race_mutex);
    pthread_mutex_destroy(&classif_mutex);

    // remove MSQ
    msgctl(mqid, IPC_RMID, 0);

    unlink(PIPE_NAME);

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);

    exit(0);
}

void sigint_race(int signo)
{
    int team_count = 0;
    signal(SIGINT, SIG_IGN);
    write_logfile("RACE MANAGER CLEANING");

    // WAIT FOR TEAMS
    pthread_mutex_lock(&race->race_mutex);
    race->end_sim = 1;
    team_count = race->team_count;
    while (race->waiting != team_count)
    {
        pthread_cond_wait(&race->cv_waiting, &race->race_mutex);
    }
    pthread_cond_broadcast(&race->cv_allow_teams);
    pthread_mutex_unlock(&race->race_mutex);

    for (int i = 0; i < team_count; i++)
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
    write_logfile("MALFUNCTION MANAGER CLEANING");
    printf("[%ld] Malfunction Manager process finished\n", (long)getpid());
    exit(0);
}

void sigint_team(int signo)
{
    signal(SIGINT, SIG_IGN);
    write_logfile("TEAM MANAGER CLEANING");

    close(channels[teamID][1]);
    free(carThreads);

    pthread_cond_destroy(&cond_box_free);
    pthread_cond_destroy(&cond_box_full);
    pthread_mutex_destroy(&mutex_box);

    printf("[%ld] Team Manager #%d process finished\n", (long)getpid(), teamID);
    exit(0);
}

car_struct *sort_classif()
{
    car_struct *classifs = calloc(race->car_count, sizeof(car_struct));
    car_struct temp;

    for (int i = 0; i < race->car_count; ++i)
    {
        classifs[i] = cars[i];
    }

    for (int i = 0; i < race->car_count; ++i)
    {
        for (int j = i + 1; j < race->car_count; ++j)
        {
            if (classifs[i].voltas == config->turns_number)
            {
                continue;
            }
            else if ((classifs[i].voltas < classifs[j].voltas) || (classifs[i].voltas == classifs[j].voltas && classifs[i].dist_percorrida < classifs[j].dist_percorrida) || classifs[i].classificacao < classifs[j].classificacao)
            {
                temp = classifs[i];
                classifs[i] = classifs[j];
                classifs[j] = temp;
            }
        }
    }

    return classifs;
}

void print_statistics()
{
    car_struct *final_classification = sort_classif();

    printf("\n*******************************************************\n");
    printf("\n::: TOP 5 :::\n");

    for (int i = 0; i < 5; i++)
    {
        if (&final_classification[i] == NULL)
            break;
        printf("%d : Car %d\n", i + 1, final_classification[i].num);
    }

    printf("\n::: LAST PLACE :::\n");
    printf("%d : Car %d\n", race->car_count, final_classification[race->car_count - 1].num);

    printf("\n::: MALFUNCTIONS :::\n%d\n", race->n_avarias);
    printf("\n::: REFUELLINGS :::\n%d\n", race->n_abastecimentos);

    printf("\n*******************************************************\n");
    free(final_classification);
}

bool check_int(char *variable)
{
    for (int i = 0; variable[i] != '\0'; i++)
    {
        if (variable[i] >= '0' && variable[i] <= '9')
        {
            continue;
        }
        else
        {
            return false;
        }
    }
    return true;
}

int check_commands(int car_num, int car_speed, int car_reliability, int converted, float car_consumption, char **fields, char *team_name, int team_count)
{
    
    return 0;
}

int update_or_create_team(int team_count, char *team_name)
{
    int i;
    for (i = 0; i < team_count; i++)
    {
        if (strcmp(team_name, all_teams[i].name) == 0)
        {
            if (all_teams[i].number_of_cars == config->max_carros)
            {
                return 1;
            }
            else
            {
                all_teams[i].number_of_cars++;
                break;
            }
        }
    }

    if (i >= team_count)
    {
        strncpy(all_teams[team_count].name, team_name, TEAM_NAME_SIZE);
        all_teams[team_count].number_of_cars = 0;

        pid_t teamPID;
        if ((teamPID = fork()) == 0)
        {
            close(channels[team_count][0]);
            teamID = team_count;
            teamManager();
        }
        else if (teamPID == -1)
        {
            write_logfile("ERROR CREATING TEAM MANAGER PROCESS");
            exit(1);
        }
        teamsPID[team_count] = teamPID; // teams[ID_1, ID_2, ID_3,...]
        close(channels[team_count][1]);
        pthread_mutex_lock(&race->race_mutex);
        race->team_count = ++team_count;
        pthread_mutex_unlock(&race->race_mutex);
    }
    return 0;
}

void create_car_struct(char *team_name, int car_num, int car_speed, int car_consumption, int car_reliability)
{
    int car_i;
    pthread_mutex_lock(&race->race_mutex);
    car_i = (race->car_count)++;
    pthread_mutex_unlock(&race->race_mutex);

    strncpy(cars[car_i].team, team_name, TEAM_NAME_SIZE);
    cars[car_i].num = car_num;
    //cars[car_i].combustivel = (float)config->fuel_capacity;
    //cars[car_i].avaria = WORKING;
    //cars[car_i].dist_percorrida = 0.0;
    //cars[car_i].voltas = 0;
    //cars[car_i].state = CORRIDA;
    cars[car_i].speed = car_speed;
    cars[car_i].consumption = car_consumption;
    cars[car_i].reliability = car_reliability;
    //cars[car_i].n_stops_box = 0;
    //cars[car_i].classificacao = 0;

    //printf("Car %d struct: TEAM %s, AVARIA: %d, COMBUSTÍVEL: %f, d_PERCORRIDA: %f, VOLTAS: %d, STATE: %d, SPEED: %d, CONSUMPTION: %f, RELIABILITY: %d\n", cars[car_i].num, cars[car_i].team, cars[car_i].avaria, cars[car_i].combustivel, cars[car_i].dist_percorrida, cars[car_i].voltas, cars[car_i].state, cars[car_i].speed, cars[car_i].consumption, cars[car_i].reliability);

    char car_text[LINESIZE];
    sprintf(car_text, "NEW CAR LOADED => TEAM: %s, CAR: %d, SPEED: %d, CONSUMPTION: %.2f, RELIABILITY: %d", team_name, car_num, car_speed, car_consumption, car_reliability);
    write_logfile(car_text);
}