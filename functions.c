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
    while (1)
    {
        sleep(config->T_Avaria * config->time_units);
        for (int i = 0; i < config->n_teams * config->max_carros; i++)
        {
            if (rand() % 100 > cars[i].reliability)
            {
                // TODO: comunica avaria por message queue
            }
        }
    }
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
                    char team_name;
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
                        {
                            write_logfile(err_log_str);
                            continue;
                        }
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
                        team_name = token[0];
                    }
                    else
                    {
                        write_logfile(err_log_str);
                        continue;
                    }

                    token = strtok(fields[1], " ");
                    if (strcmp(token, "CAR:") == 0)
                    {
                        token = strtok(NULL, " ");
                        car_num = atoi(token);
                        // TODO: check for errors and invalid values
                    }
                    else
                    {
                        write_logfile(err_log_str);
                        continue;
                    }

                    token = strtok(fields[2], " ");
                    if (strcmp(token, "SPEED:") == 0)
                    {
                        token = strtok(NULL, " ");
                        car_speed = atoi(token);
                        // TODO: check for errors and invalid values
                    }
                    else
                    {
                        write_logfile(err_log_str);
                        continue;
                    }

                    token = strtok(fields[3], " ");
                    if (strcmp(token, "CONSUMPTION:") == 0)
                    {
                        token = strtok(NULL, " ");
                        car_consumption = (float)atof(token);
                        // TODO: check for errors and invalid values
                    }
                    else
                    {
                        write_logfile(err_log_str);
                        continue;
                    }

                    token = strtok(fields[4], " ");
                    if (strcmp(token, "RELIABILITY:") == 0)
                    {
                        token = strtok(NULL, " ");
                        car_reliability = atoi(token);
                        // TODO: check for errors and invalid values
                    }
                    else
                    {
                        write_logfile(err_log_str);
                        continue;
                    }

                    // -------------------- CREATE CAR STRUCT -------------------- //
                    // se o comando e dados forem válidos
                    // e não exceder o número de carros por equipa

                    // INICIO SINCRONIZACAO!!!
                    for (int i = 0; i < config->n_teams * config->max_carros; i++)
                    {
                        if (cars[i].state < 1 || cars[i].state > 5)
                        {
                            cars[i].team = team_name;
                            cars[i].num = car_num;
                            cars[i].combustivel = (float)config->fuel_capacity;
                            cars[i].dist_percorrida = 0.0;
                            cars[i].voltas = 0;
                            cars[i].state = CORRIDA;
                            cars[i].speed = car_speed;
                            cars[i].consumption = car_consumption;
                            cars[i].reliability = car_reliability;
                        }
                    }
                    // FIM SINCRONIZACAO!!!

                    char car_text[LINESIZE];
                    sprintf(car_text, "NEW CAR LOADED => TEAM: %c, CAR: %d, SPEED: %d, CONSUMPTION: %.2f, RELIABILITY: %d", team_name, car_num, car_speed, car_consumption, car_reliability);
                    write_logfile(car_text);
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

    // corrida acaba e limpa os recursos
    clean_resources(fd_named_pipe, channels);

    printf("[%ld] Race Manager process finished\n", (long)getpid());
    exit(0);
}

void teamManager(int pipe, int teamID)
{
    printf("[%ld] Team Manager #%d process working\n", (long)getpid(), teamID);

    // -------------------- CREATE BOX -------------------- //

    team_box[teamID].box_state = BOX_FREE;

    // -------------------- CREATE CAR THREADS -------------------- //

    pthread_t carThreads[config->max_carros];
    int **my_cars;

    // quando a corrida começa
    int car_count = 0;
    for (int i = 0; i < config->n_teams * config->max_carros; i++)
    {
        // se é um carro da equipa
        if (cars[i].team == 65 + teamID)
        {
            int argv[2];
            argv[0] = cars + i;
            argv[1] = pipe;
            pthread_create(&carThreads[car_count], NULL, carThread, argv);
            my_cars[car_count++] = cars + i;
        }
    }

    // -------------------- BOX STATE -------------------- //

    //TODO: se box livre e carro precisar de ir à box, repara o carro
    while (1)
    {
        /* for (int i = 0; i < len(carros da equipa); i++){
            
            if (carros[i].state = SEGURANCA){
                box_state = RESERVADO;
            }

            if (carros[i].state = BOX and box_state = FREE){
                box_state = FULL;
                sleep(rand % T_box_max - rand % T_box_min);
                sleep(2 * config->time_units);
                carros[i].combustivel = config->fuel_capacity;
                carros[i].state = CORRIDA;
                box_state = FREE;


        }
        */
    }

    //TODO: reparar o carro: sleep de (T_box_min a T_box_max) + sleep de (2 time units)

    // -------------------- WAIT FOR CAR THREADS TO DIE -------------------- //

    for (int i = 0; i < car_count; i++)
    {
        pthread_join(carThreads[i], NULL);
    }

    printf("[%ld] Team Manager #%d process finished\n", (long)getpid(), teamID);
    exit(0);
}

void *carThread(void *array_infos_p)
{
    int *array_infos = (int *)array_infos_p;

    car_struct *car = array_infos[0];
    int team = car->team - 65;
    int speed = car->speed;
    float consumption = car->consumption;
    int reliability = car->reliability;

    int pipe = array_infos[1];
    //int carID = *((int *)carID_p);

    // array_infos[0] = TeamID
    // array_infos[1] = carID
    // array_infos[2] = channels_write;

    printf("[%ld] Car #%d thread working\n", (long)getpid(), car->num);

    // progress == how many sleeps (progresses) are needed for a turn
    // we consume progress * <CONSUMPTION> liters for a turn
    int progress = config->turns_number / (car->speed * config->time_units);

    while (1)
    {

        // -------------------- CAR RACING -------------------- //
        switch (car->state)
        {
        case CORRIDA:

            // se atingir combustível suficiente apenas para 4 voltas
            if (progress * config->turns_number * consumption * car->combustivel >= progress * 4)
            {
                // se faltar 1 progress para acabar a volta e a box estiver free, o carro conclui a volta e entra na box
                if (config->turn_distance < car->dist_percorrida + progress && team_box[team].box_state == BOX_FREE)
                {
                    car->voltas += 1;
                    car->state = BOX;
                    team_box[team].box_state = BOX_FULL;
                    team_box[team].car = *cars;
                    write(pipe, BOX, sizeof(int));
                    continue;
                }
            }

            // se atingir combustível suficiente apenas para 2 voltas
            else if (car->combustivel >= progress * 2 * consumption)
            {
                car->state = SEGURANCA;
                write(pipe, SEGURANCA, sizeof(int));
                continue;
            }

            else
            {
                sleep(config->time_units * speed);
                car->combustivel -= consumption;
                car->dist_percorrida += progress;
                continue;
            }

        case SEGURANCA:

            speed *= 0.3;
            consumption = 0.4;
            sleep(config->time_units * speed);
            car->combustivel -= consumption;
            car->dist_percorrida += progress;
            continue;

        case BOX:
            car->dist_percorrida = 0;
            cars->speed = 0;
            cars->consumption = 0;
            continue;

        case DESISTENCIA:
            car->combustivel = 0;
            printf("[%ld] Car #%d thread finished\n", (long)getpid(), car->num);
            pthread_exit(NULL);

        case TERMINADO:
            printf("[%ld] Car #%d thread finished\n", (long)getpid(), car->num);
            pthread_exit(NULL);
        }
    }

    // -------------------- NOTIFICAR POR UNNAMED PIPE -------------------- //
    // TODO: notificar por UNNAMED PIPE:
    // 1) alteração de estado
    // 2) se terminou a corrida (desistência/ganhou/chegou à meta)

    printf("[%ld] Car #%d thread finished\n", (long)getpid(), car->num);
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