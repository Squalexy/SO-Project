/*
Projeto realizado por:
    Alexy de Almeida nº2019192123
    José Gonçalves nº2019223292
*/

#include "declarations.h"
#include <string.h>

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
    fprintf(fptr, "%02d:%02d:%02d  %s\n", hours, minutes, seconds, text_to_write);
    sem_post(writing);
}

void malfunctionManager(void)
{
    printf("[%ld] Malfunction Manager process working\n", (long)getpid());
    //while (1)
    //{
    sleep(config->T_Avaria * config->time_units);
    // TODO: aceder a fiabilidade do carro e com base nessa fiabilidade cria uma avaria
    // for (int i = 0; i < team_box_struct)
    // TODO: criar avaria
    //}
    printf("[%ld] Malfunction Manager process finished\n", (long)getpid());
    exit(0);
}

void raceManager()
{
    int n_teams = config->n_teams;
    pid_t teams[n_teams];
    fd_set read_set;

    // -------------------- CREATE UNNAMED PIPES -------------------- //

    int channels[n_teams][2];
    for (int i = 0; i < n_teams; i++)
    {
        pipe(channels[i]);
    }

    // --------------- OPEN NAMED PIPE FOR READ-WRITE --------------- //

    int fd_named_pipe;
    if ((fd_named_pipe = open(PIPE_NAME, O_RDWR)) < 0)
    {
        perror("Cannot open pipe for read-write\n");
        exit(1);
    }

    // -------------------- CREATE TEAMS -------------------- //

    for (int i = 0; i < n_teams; i++)
    {
        pid_t teamPID;
        if ((teamPID = fork()) == 0)
        {
            close(channels[i][0]);
            teamManager(channels[i][1], i);
        }
        else if (teamPID == -1)
        {
            write_logfile("ERROR CREATING TEAM MANAGER PROCESS");
            exit(1);
        }
        teams[i] = teamPID; // teams[ID_1, ID_2, ID_3,...]
        close(channels[i][1]);
    }

    // ----------------------- WORK ----------------------- //
    char buffer[LINESIZE];
    int nread;

    printf("[%ld] Race Manager process working\n", (long)getpid());
    while (1)
    {
        FD_ZERO(&read_set);
        for (int i = 0; i < n_teams; i++)
        {
            FD_SET(channels[i][0], &read_set);
        }
        FD_SET(fd_named_pipe, &read_set);

        // SELECT named pipe and unnamed pipes
        if (select(fd_named_pipe + 1, &read_set, NULL, NULL, NULL) > 0)
        {
            for (int i = 0; i < n_teams; i++)
            {
                if (FD_ISSET(channels[i][0], &read_set))
                {
                    // if from unnamed pipe do something else about the car state
                    nread = read(channels[i][0], buffer, LINESIZE);
                    buffer[nread - 1] = '\0';
                    printf("Received from team %d's unnamed pipe: \"%s\"\n", i, buffer);
                }
            }

            if (FD_ISSET(fd_named_pipe, &read_set))
            {
                // if from named pipe check and do command
                nread = read(fd_named_pipe, buffer, LINESIZE);
                buffer[nread - 1] = '\0';

                if (strncmp("ADDCAR", buffer, 6) == 0)
                {
                    printf("[RACE MANAGER Received \"%s\" command]\n");
                    /*
                    char *token = NULL;
                    char *inner_token = NULL;
                    token = strtok(buffer, ", ");
                    inner_token = strtok(token, " ");
                    */
                    /*
                    char team_name;
                    int car_num, car_speed, car_reliability, read;
                    float car_consumption;
                    read = sscanf(buffer, "ADDCAR TEAM: %c, CAR: %d, SPEED: %d, CONSUMPTION: %f, RELIABILITY: %d", &team_name, &car_num, &car_speed, &car_consumption, &car_reliability);
                    if (read == EOF || read != 5){
                        char *err_log_str = "WRONG COMMAND => ";
                        strcat(buffer, err_log_str);
                        write_logfile(err_log_str);
                    }
                    */
                    char team_name[50];
                    int car_num, car_speed, car_reliability, read;
                    float car_consumption;

                    char *token = NULL;

                    // comando separado por vírgula (cada campo)
                    char fields[5][64];

                    char *err_log_str = "WRONG COMMAND => ";
                    strcat(buffer, err_log_str);

                    token = strtok(buffer + 7, ", "); // para tirar o ADDCAR
                    for (int i = 0; i < 5; i++)       // 5 campos
                    {
                        if (token == NULL)
                            write_logfile(err_log_str);
                        strncpy(fields[i], token, 64);
                        token = strtok(NULL, ", ");
                    }

                    for (int i = 0; i < 5; i++)
                    {
                        printf("%s\n", fields[i]);
                    }

                    token = strtok(fields[0], " ");
                    if (strcmp(token, "TEAM:") == 0)
                    {
                        token = strtok(NULL, " ");
                        strcpy(team_name, token);
                    }
                    else
                        write_logfile(err_log_str);

                    token = strtok(fields[1], " ");
                    if (strcmp(token, "CAR:") == 0)
                    {
                        token = strtok(NULL, " ");
                        car_num = atoi(token);
                        // TODO: check for errors and invalid values
                    }
                    else
                        write_logfile(err_log_str);

                    token = strtok(fields[2], " ");
                    if (strcmp(token, "SPEED:") == 0)
                    {
                        token = strtok(NULL, " ");
                        car_speed = atoi(token);
                        // TODO: check for errors and invalid values
                    }
                    else
                        write_logfile(err_log_str);

                    token = strtok(fields[3], " ");
                    if (strcmp(token, "CONSUMPTION:") == 0)
                    {
                        token = strtok(NULL, " ");
                        car_consumption = (float)atof(token);
                        // TODO: check for errors and invalid values
                    }
                    else
                        write_logfile(err_log_str);

                    token = strtok(fields[4], " ");
                    if (strcmp(token, "RELIABILITY:") == 0)
                    {
                        token = strtok(NULL, " ");
                        car_reliability = atoi(token);
                        // TODO: check for errors and invalid values
                    }
                    else
                        write_logfile(err_log_str);
                }
                else if (strcmp("START RACE!", buffer) == 0)
                    printf("[RACE MANAGER received \"%s\" command]\nRace initiated!\n", buffer);
                else
                    printf("[RACE MANAGER received unknown command]: %s \n", buffer);
            }
        }
    }

    // -------------------- WAIT FOR TEAMS -------------------- //

    for (int i = 0; i < n_teams; i++)
    {
        waitpid(teams[i], NULL, 0);
    }

    clean_resources(fd_named_pipe, channels);

    printf("[%ld] Race Manager process finished\n", (long)getpid());
    exit(0);
}

void teamManager(int channels_write, int teamID)
{
    printf("[%ld] Team Manager #%d process working\n", (long)getpid(), teamID);

    // -------------------- CREATE BOX -------------------- //

    team_box[teamID].box_state = BOX_FREE;

    // TOOO: numCar a ser alterado por named pipe!
    int numCar = 2;

    int carID[config->max_carros];
    pthread_t carThreads[config->max_carros];

    if (numCar > config->max_carros)
    {
        write_logfile("NUMBER OF CARS ABOVE THE MAXIMUM ALLOWED");
        exit(1);
    }

    // -------------------- CREATE CAR THREADS -------------------- //

    for (int i = 0; i < numCar; i++)
    {
        carID[i] = (teamID * numCar) + i;

        // create ids to put as car struct info
        int array_ids[2];
        array_ids[0] = teamID;
        array_ids[1] = carID;

        pthread_create(&carThreads[i], NULL, carThread, array_ids);
        //printf("[%ld] Car #%d thread created\n", (long)getpid(), carID[i]);

        char car_text[LINESIZE];

        sprintf(car_text, "NEW CAR LOADED => TEAM %d, CAR: %02d", teamID, carID[i]);
        write_logfile(car_text);
    }

    // -------------------- MANAGE WRITING PIPES -------------------- //
    // enviar informação do estado do carro para o Race Manager

    while (1)
    {
        // if estado qualquer cena
        write(channels_write, <estado>, sizeof(int));
    }

    // -------------------- BOX STATE -------------------- //

    //TODO: aceder ao estado da box
    /*
    if
    */

    //TODO: se box livre e carro precisar de ir à box, repara o carro
    //TODO: reparar o carro: sleep de (T_box_min a T_box_max) + sleep de (2 time units)

    // -------------------- WAIT FOR CAR THREADS TO DIE -------------------- //

    for (int i = 0; i < numCar; i++)
    {
        pthread_join(carThreads[i], NULL);
    }

    printf("[%ld] Team Manager #%d process finished\n", (long)getpid(), teamID);
    exit(0);
}

void *carThread(void *array_ids_p)
{
    int *array_ids = (int *)array_ids_p;
    //int carID = *((int *)carID_p);

    // array_ids[0] = TeamID
    // array_ids[1] = carID

    printf("[%ld] Car #%d thread working\n", (long)getpid(), array_ids[1]);

    // -------------------- CREATE CAR STRUCT -------------------- //
    cars[array_ids[1]].id_team = array_ids[0];
    cars[array_ids[1]].combustivel = (float)config->fuel_capacity;
    cars[array_ids[1]].dist_percorrida = 0;
    cars[array_ids[1]].voltas = 0;
    cars[array_ids[1]].state = CORRIDA;

    cars[array_ids[1]].speed = <SPEED>;
    cars[array_ids[1]].consumption = <CONSUMPTION>;
    cars[array_ids[1]].reliability = <RELIABILITY>;

    // progress == how many sleeps (progresses) are needed for a turn
    // we consume progress * <CONSUMPTION> liters for a turn
    int progress = config->turns_number / (cars[array_ids[1]].speed * config->time_units);

    while (1)
    {

        // -------------------- CAR RACING -------------------- //
        switch (*cars[array_ids[1]].state)
        {
        case CORRIDA:

            // se atingir combustível suficiente apenas para 4 voltas
            if (progress * config->turns_number * < CONSUMPTION >> cars[array_ids[1]].combustivel >= progress * 4 * <CONSUMPTION>)
            {
                // se faltar 1 progress para acabar a volta e a box estiver free, o carro conclui a volta e entra na box
                if (config->turn_distance < cars[array_ids[1]].dist_percorrida + progress && team_box[cars[array_ids[1]].id_team].box_state = BOX_FREE)
                {
                    cars[array_ids[1]].voltas += 1;
                    // meter o carro na box
                }
            }

            else if (cars[array_ids[1]].combustivel >= progress * 2 * <CONSUMPTION>)
            {
                *cars[array_ids[1]].state = SEGURANCA;
                break;
            }

            else
            {
                sleep(config->time_units * <SPEED>);
                cars[array_ids[1]].combustivel -= <CONSUMPTION>;
                break;
            }

        case SEGURANCA:
            sleep(config->time_units * (<SPEED> * 0.3));
            cars[array_ids[1]].combustivel -= <CONSUMPTION> * 0.4;
            break;

        case BOX:
            cars[array_ids[1]].dist_percorrida = 0;
            break;

        case DESISTENCIA:
            cars[array_ids[1]].combustivel = 0;
            printf("[%ld] Car #%d thread finished\n", (long)getpid(), cars[array_ids[1]]);
            pthread_exit(NULL);
            break;

        case TERMINADO:
            printf("[%ld] Car #%d thread finished\n", (long)getpid(), cars[array_ids[1]]);
            pthread_exit(NULL);
            break;
        }
    }

    // -------------------- NOTIFICAR POR UNNAMED PIPE -------------------- //
    // TODO: notificar por UNNAMED PIPE:
    // 1) alteração de estado
    // 2) se terminou a corrida (desistência/ganhou/chegou à meta)

    printf("[%ld] Car #%d thread finished\n", (long)getpid(), cars[array_ids[1]]);
    pthread_exit(NULL);
}

void clean_resources(int fd_named_pipe, int **channels)
{ // cleans all resources including closing of files
    close(fd_named_pipe);
    for (int i = 0; i < config->n_teams; i++)
    {
        close(channels[i][0]);
        close(channels[i][1]);
    }
    unlink(PIPE_NAME);
}

void sigtstp(int signum)
{
    // -------------------- PRINT STATISTICS -------------------- //

    //TODO: open statistics file

    exit(0);
}

void sigint(int signum)
{
    signal(SIGINT, SIG_IGN);
    for (int i = 0; i < config->n_teams * config->max_carros; i++)
    {

        // carros na box terminam
        if (cars[i].state == BOX)
        {
            cars[i].state = TERMINADO;
        }

        // TODO: carros em corrida continuam
    }
}