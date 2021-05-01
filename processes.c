/*
Projeto realizado por:
    Alexy de Almeida nº2019192123
    José Gonçalves nº2019223292
*/

#include "declarations.h"

void malfunctionManager(void)
{
    msg msg_to_send;
    printf("-----------------\n[%ld] Malfunction Manager waiting for race start\n------------------\n", (long)getpid());
    pthread_mutex_lock(&race->race_mutex);
    while (!race->race_started)
    {
        pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
    }
    pthread_mutex_unlock(&race->race_mutex);
    printf("---------------------\n[%ld] Malfunction Manager process working\n---------------------\n", (long)getpid());
    while (1)
    {
        sleep(config->T_Avaria * config->time_units);
        for (int i = 0; i < config->n_teams * config->max_carros; i++)
        {
            if (rand() % 101 > cars[i].reliability)
            {
                msg_to_send.car_id = cars[i].num;
                if (msgsnd(mqid, &msg_to_send, sizeof(msg), msg_to_send.car_id) < 0)
                {
                    write_logfile("ERROR SENDING MESSAGE TO MSQ");
                    exit(0);
                }
            }
        }
    }
    printf("---------------------\n[%ld] Malfunction Manager process finished\n---------------------\n", (long)getpid());
    exit(0);
}

void raceManager()
{
    int n_teams = config->n_teams;
    pid_t teams[n_teams];

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

    // ----------------------- WORK ----------------------- //
    char buffer[LINESIZE];
    int nread;
    int team_count = 0;
    fd_set read_set;

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
                // reject if race already started
                pthread_mutex_lock(&race->race_mutex);
                if (&race->race_started)
                {
                    printf("[RACE MANAGER received \"%s\" command]\nRejected, race already started!\n", buffer);
                    pthread_mutex_unlock(&race->race_mutex);
                    continue;
                }
                pthread_mutex_unlock(&race->race_mutex);

                // if from named pipe check and do command
                nread = read(fd_named_pipe, buffer, LINESIZE);
                buffer[nread - 1] = '\0';

                if (strncmp("ADDCAR", buffer, 6) == 0)
                {
                    printf("[RACE MANAGER Received \"%s\" command]\n");

                    char team_name[TEAM_NAME_SIZE];
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
                        strncpy(team_name, token, TEAM_NAME_SIZE);
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

                    // verificar se a equipa já existe
                    int i;
                    for (int i = 0; i < team_count; i++)
                    {
                        if (strcmp(team_name, team_box[i].name) == 0)
                            break;
                    }

                    // -------------------- CREATE TEAMS -------------------- //
                    // não encontrou a equipa => vai criá-la
                    if (i >= team_count)
                    {
                        strncpy(team_box[team_count].name, team_name, TEAM_NAME_SIZE);

                        pid_t teamPID;
                        if ((teamPID = fork()) == 0)
                        {
                            close(channels[team_count][0]);
                            teamManager(channels[team_count][1], team_count);
                        }
                        else if (teamPID == -1)
                        {
                            write_logfile("ERROR CREATING TEAM MANAGER PROCESS");
                            exit(1);
                        }
                        teams[team_count] = teamPID; // teams[ID_1, ID_2, ID_3,...]
                        close(channels[team_count][1]);
                        team_count++;
                    }

                    // -------------------- CREATE CAR STRUCT -------------------- //
                    // se o comando e dados forem válidos
                    // e não exceder o número de carros por equipa

                    // INICIO SINCRONIZACAO!!!
                    sem_wait(car_write);
                    for (int i = 0; i < config->n_teams * config->max_carros; i++)
                    {
                        if (cars[i].state < 1 || cars[i].state > 5)
                        {
                            strncpy(cars[i].team, team_name, TEAM_NAME_SIZE);
                            cars[i].num = car_num;
                            cars[i].combustivel = (float)config->fuel_capacity;
                            cars[i].avaria = WORKING;
                            cars[i].dist_percorrida = 0.0;
                            cars[i].voltas = 0;
                            cars[i].state = CORRIDA;
                            cars[i].speed = car_speed;
                            cars[i].consumption = car_consumption;
                            cars[i].reliability = car_reliability;
                        }
                    }
                    sem_post(car_write);
                    // FIM SINCRONIZACAO!!!

                    char car_text[LINESIZE];
                    sprintf(car_text, "NEW CAR LOADED => TEAM: %s, CAR: %d, SPEED: %d, CONSUMPTION: %.2f, RELIABILITY: %d", team_name, car_num, car_speed, car_consumption, car_reliability);
                    write_logfile(car_text);
                }
                else if (strcmp("START RACE!", buffer) == 0)
                {
                    // verificar se team_count == n_teams
                    // verificar se todas as equipas têm pelo menos um carro

                    pthread_mutex_lock(&race->race_mutex);
                    printf("[RACE MANAGER received \"%s\" command]\nRace initiated!\n", buffer);
                    race->race_started = 1;
                    pthread_cond_broadcast(&race->cv_race_started);
                    pthread_mutex_unlock(&race->race_mutex);
                }
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

    printf("---------------------\n[%ld] Race Manager process finished\n---------------------\n", (long)getpid());
    exit(0);
}

void teamManager(int pipe, int teamID)
{
    printf("---------------------\n[%ld] Team Manager #%d process working\n---------------------\n", (long)getpid(), teamID);

    // -------------------- CREATE BOX -------------------- //

    team_box[teamID].box_state = BOX_FREE;
    team_box[teamID].car_id = 0;

    // -------------------- CREATE CAR THREADS -------------------- //

    pthread_t carThreads[config->max_carros];
    int **my_cars;

    sem_wait(team_read);
    ++(config->team_read);
    if (config->team_read == 1)
        sem_wait(car_write);
    sem_post(team_read);

    // NOT FINISHED
    int car_count = 0;
    for (int i = 0; i < config->n_teams * config->max_carros; i++)
    {
        // se é um carro da equipa
        if (strcmp(cars[i].team, team_box[teamID].name) == 0)
        {
            int argv[2];
            argv[0] = cars + i;
            argv[1] = pipe;
            pthread_create(&carThreads[car_count], NULL, carThread, argv);
            my_cars[car_count++] = cars + i;
        }
    }

    sem_wait(team_read);
    --(config->team_read);
    if (config->team_read == 0)
        sem_post(car_write);
    sem_post(team_read);

    // -------------------- BOX STATE -------------------- //

    while (1)
    {
        pthread_mutex_lock(&mutex_box);

        for (int i = 0; i < len(cars); i++)
        {
            if (strcmp(cars[i].team, team_box[teamID].name) == 0 && cars[i].state == SEGURANCA && team_box[teamID].box_state == BOX_FREE)
            {
                team_box[teamID].box_state = BOX_RESERVED;
            }
        }

        while (team_box[teamID].car_id == 0)
        {
            pthread_cond_wait(&cond_box_full, &mutex_box);
        }

        team_box[teamID].box_state == BOX_FULL;
        sleep(rand() % config->T_Box_Max - rand() % config->T_Box_min + 1); // repara o carro
        sleep(2 * config->time_units);                                      // abastece
        cars[team_box[teamID].car_id].combustivel = config->fuel_capacity;
        cars[team_box[teamID].car_id].state = CORRIDA;
        team_box[teamID].box_state = BOX_FREE;
        pthread_cond_signal(&cond_box_free);
    }

    // -------------------- WAIT FOR CAR THREADS TO DIE -------------------- //

    for (int i = 0; i < car_count; i++)
    {
        pthread_join(carThreads[i], NULL);
    }

    printf("---------------------\n[%ld] Team Manager #%d process finished\n---------------------\n", (long)getpid(), teamID);
    exit(0);
}

void *carThread(void *array_infos_p)
{
    msg received_msg;

    int *array_infos = (int *)array_infos_p;

    car_struct *car = array_infos[0];
    int team;
    for (team = 0; team < config->n_teams; team++)
    {
        if (strcmp(car->team, team_box[team].name) == 0)
            break;
    }

    int pipe = array_infos[1];
    //int carID = *((int *)carID_p);

    // array_infos[0] = TeamID    array_infos[1] = carID    array_infos[2] = channels_write;

    printf("------------\n[%ld] Car #%d thread working\n------------\n", (long)getpid(), car->num);

    /* progress == how many sleeps (progresses) are needed for a turn
           we consume progress * <CONSUMPTION> liters for a turn*/
    int progress = config->turns_number / (car->speed * config->time_units);

    // -------------------- CAR RACING -------------------- //
    // block until race starts
    pthread_mutex_lock(&race->race_mutex);
    while (!race->race_started)
    {
        pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
    }
    pthread_mutex_unlock(&race->race_mutex);

    while (1)
    {

        switch (car->state)
        {

        case CORRIDA:

            // -------------------- MALFUNCTION MESSAGE FROM MSQ -------------------- //

            if (msgrcv(mqid, &received_msg, sizeof(msg), car->num, 0) < 0)
            {
                write_logfile("ERROR RECEIVING MESSAGE FROM MSQ");
                exit(0);
            }
            else
            {
                car->state = SEGURANCA;
                write(pipe, SEGURANCA, sizeof(int));
            }

            pthread_mutex_lock(&mutex_box);

            /* 1ª condição: se atingir combustível suficiente apenas para 4 voltas
               2ª condição: se faltar menos de 4 voltas para acabar a corrida, ignora*/
            if (car->combustivel >= progress * 4 * car->consumption && car->voltas + 4 < config->turns_number)
            {
                // se carro estiver a chegar ao ponto de partida, conclui a volta e entra na box
                if (config->turn_distance < car->dist_percorrida + progress && team_box[team].box_state == BOX_FREE)
                {
                    car->voltas += 1;
                    car->state = BOX;
                    team_box[team].car_id = car->num;
                    write(pipe, BOX, sizeof(int));

                    pthread_cond_signal(&cond_box_full);
                    pthread_mutex_unlock(&mutex_box);

                    pthread_mutex_lock(&mutex_box);
                    while (team_box[team].box_state != BOX_FREE)
                    {
                        pthread_cond_wait(&cond_box_full, &mutex_box);
                    }
                    car->state = CORRIDA;
                    pthread_mutex_unlock(&mutex_box);
                }
            }

            /* se atingir combustível suficiente apenas para 2 voltas e se não lhe faltar uma volta para acabar a corrida
                   condição para assegurar que o carro não vai para a box quando podia muito bem acabar a corrida */
            else if (car->combustivel >= progress * 2 * car->consumption && car->voltas + 1 < config->turns_number)
            {
                car->state = SEGURANCA;
                write(pipe, SEGURANCA, sizeof(int));
            }

            else
            {
                sleep(config->time_units * car->speed);
                car->combustivel -= car->consumption;
                car->dist_percorrida += progress;
            }

            break;

        case SEGURANCA:

            pthread_mutex_lock(&mutex_box);

            if (team_box[team].box_state == BOX_RESERVED && config->turn_distance < car->dist_percorrida + progress)
            {
                car->voltas += 1;
                car->state = BOX;
                team_box[team].car_id = car->num;
                write(pipe, BOX, sizeof(int));

                pthread_cond_signal(&cond_box_full);
                pthread_mutex_unlock(&mutex_box);

                while (team_box[team].box_state != BOX_FREE)
                {
                    pthread_cond_wait(&cond_box_free, &mutex_box);
                }
                car->state = CORRIDA;
            }

            car->speed *= 0.3;
            car->consumption = 0.4;
            sleep(config->time_units * car->speed);
            car->combustivel -= car->consumption;
            car->dist_percorrida += progress;

            break;

        case BOX:
            car->dist_percorrida = 0;
            car->speed = 0;
            car->consumption = 0;

            break;
        }

        if (car->voltas == config->turns_number)
        {
            char fim_volta[LINESIZE];
            sprintf(fim_volta, "CAR %d FINISHED THE RACE!\n", car->num);
            write_logfile(fim_volta);
            write(pipe, TERMINADO, sizeof(int));
            pthread_exit(NULL);
        }

        if (car->combustivel <= 0)
        {
            car->state = DESISTENCIA;
            char desistencia[LINESIZE];
            sprintf(desistencia, "CAR %d ABANDONED THE RACE!\n", car->num);
            write_logfile(desistencia);
            write(pipe, DESISTENCIA, sizeof(int));
            pthread_exit(NULL);
        }
    }

    printf("------------\n[%ld] Car #%d thread finished\n------------\n", (long)getpid(), car->num);
    pthread_exit(NULL);
}
