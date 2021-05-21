/*
    Projeto realizado por:
    Alexy de Almeida nº2019192123
    José Gonçalves nº2019223292
*/

#include "declarations.h"

void malfunctionManager(void)
{
    msg msg_to_send;
    while (1)
    {
        printf("-----------------\n[%ld] Malfunction Manager waiting for race start\n------------------\n", (long)getpid());
        pthread_mutex_lock(&race->race_mutex);
        while (race->race_started != 1)
        {
            pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
        }
        pthread_mutex_unlock(&race->race_mutex);

        printf("---------------------\n[%ld] Malfunction Manager process working\n---------------------\n", (long)getpid());

        signal(SIGTSTP, SIG_IGN);
        signal(SIGINT, SIG_IGN);

        while (1)
        {
            pthread_mutex_lock(&race->race_mutex);
            if (race->race_started == 0)
            {
                pthread_mutex_unlock(&race->race_mutex);
                break;
            }
            pthread_mutex_unlock(&race->race_mutex);

            usleep(config->T_Avaria * ((float)1 / (float)config->time_units) * 1000000);
            for (int i = 0; i < race->car_count; i++)
            {
                if (rand() % 101 > cars[i].reliability && (cars[i].state == CORRIDA || cars[i].state == SEGURANCA))
                {
                    msg_to_send.car_id = cars[i].num;
                    if (msgsnd(mqid, &msg_to_send, sizeof(msg), 0) < 0)
                    {
                        write_logfile("ERROR SENDING MESSAGE TO MSQ");
                        exit(0);
                    }

                    char malfunction[LINESIZE];
                    sprintf(malfunction, "SENDING MALFUNCTION TO CAR %d", cars[i].num);
                    race->n_avarias += 1;
                    write_logfile(malfunction);
                }
            }
        }
        printf("---------------------\n[%ld] Malfunction Manager process finished\n---------------------\n", (long)getpid());
    }
    exit(0);
}

void raceManager()
{
    int n_teams = config->n_teams;
    pid_t teams[n_teams];
    race->n_cars_racing = 0;

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
    int team_count = 0, finished = 0;
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
                    int state = 0;
                    nread = read(channels[i][0], &state, sizeof(int));
                    if (nread > 0)
                        printf("Received from team %d's unnamed pipe: \"%d\"\n", i, state);

                    if (state == TERMINADO || state == DESISTENCIA)
                        finished++;

                    printf("FINISHED: %d\n", finished);

                    if (finished == n_teams)
                    {
                        pthread_mutex_lock(&race->race_mutex);
                        race->race_started = 0;
                        race->threads_created = -1;
                        finished = 0;
                        pthread_cond_broadcast(&race->cv_race_started);
                        pthread_mutex_unlock(&race->race_mutex);
                    }
                }
            }

            if (FD_ISSET(fd_named_pipe, &read_set))
            {
                // if from named pipe check and do command
                nread = read(fd_named_pipe, buffer, LINESIZE);
                buffer[nread - 1] = '\0';

                // reject if race already started
                pthread_mutex_lock(&race->race_mutex);
                if (race->race_started == 1)
                {
                    printf("[RACE MANAGER received \"%s\" command]\nRejected, race already started!\n", buffer);
                    pthread_mutex_unlock(&race->race_mutex);
                    continue;
                }
                pthread_mutex_unlock(&race->race_mutex);

                if (strncmp("ADDCAR", buffer, 6) == 0)
                {
                    printf("[RACE MANAGER Received \"%s\" command]\n", buffer);

                    char team_name[TEAM_NAME_SIZE];
                    char err_log_str[LINESIZE] = "WRONG COMMAND => ";
                    char *token = NULL;
                    char fields[10][64]; // comando separado por vírgula e espaço (cada campo)

                    int car_num, car_speed, car_reliability, converted = 0;
                    float car_consumption;

                    strcat(err_log_str, buffer);

                    token = strtok(buffer + 7, ", "); // para tirar o ADDCAR
                    for (int i = 0; i < 10; i++)
                    {
                        strncpy(fields[i], token, 64);
                        token = strtok(NULL, ", ");
                    }

                    if (strcmp(fields[0], "TEAM:") == 0)
                    {
                        strncpy(team_name, fields[1], TEAM_NAME_SIZE);
                        converted += 1;
                    }

                    if (strcmp(fields[2], "CAR:") == 0)
                    {
                        car_num = atoi(fields[3]);
                        converted += 1;
                    }

                    if (strcmp(fields[4], "SPEED:") == 0)
                    {
                        car_speed = atoi(fields[5]);
                        converted += 1;
                    }

                    if (strcmp(fields[6], "CONSUMPTION:") == 0)
                    {
                        car_consumption = (float)atof(fields[7]);
                        converted += 1;
                    }

                    if (strcmp(fields[8], "RELIABILITY:") == 0)
                    {
                        car_reliability = atoi(fields[9]);
                        converted += 1;
                    }

                    // nem todos os campos foram convertidos corretamente
                    if (converted != 5)
                    {
                        write_logfile(err_log_str);
                        continue;
                    }

                    // verificar se a equipa já existe
                    int i;
                    for (i = 0; i < team_count; i++)
                    {
                        if (strcmp(team_name, all_teams[i].name) == 0)
                            break;
                    }

                    // -------------------- CREATE TEAMS -------------------- //
                    // não encontrou a equipa => vai criá-la
                    if (i >= team_count)
                    {
                        strncpy(all_teams[team_count].name, team_name, TEAM_NAME_SIZE);

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

                    int car_i;
                    pthread_mutex_lock(&race->race_mutex);
                    car_i = (race->car_count)++;
                    pthread_mutex_unlock(&race->race_mutex);

                    printf("Car_i: %d\n", car_i);

                    strncpy(cars[car_i].team, team_name, TEAM_NAME_SIZE);
                    cars[car_i].num = car_num;
                    cars[car_i].combustivel = (float)config->fuel_capacity;
                    cars[car_i].avaria = WORKING;
                    cars[car_i].dist_percorrida = 0.0;
                    cars[car_i].voltas = 0;
                    cars[car_i].state = CORRIDA;
                    cars[car_i].speed = car_speed;
                    cars[car_i].consumption = car_consumption;
                    cars[car_i].reliability = car_reliability;
                    cars[car_i].n_stops_box = 0;
                    cars[car_i].classificacao = 0;

                    //strncpy(cars[car_i].team, team_name, TEAM_NAME_SIZE);
                    // printf("Car %d struct: TEAM %s, AVARIA: %d, COMBUSTÍVEL: %f, d_PERCORRIDA: %f, VOLTAS: %d, STATE: %d, SPEED: %f, CONSUMPTION: %f, RELIABILITY: %d\n", cars[car_i].num, cars[car_i].team, cars[car_i].avaria, cars[car_i].combustivel, cars[car_i].dist_percorrida, cars[car_i].voltas, cars[car_i].state, cars[car_i].speed, cars[car_i].consumption, cars[car_i].reliability);

                    char car_text[LINESIZE];
                    sprintf(car_text, "NEW CAR LOADED => TEAM: %s, CAR: %d, SPEED: %d, CONSUMPTION: %.2f, RELIABILITY: %d", team_name, car_num, car_speed, car_consumption, car_reliability);
                    write_logfile(car_text);
                }
                else if (strcmp("START RACE!", buffer) == 0)
                {
                    // verificar se todas as equipas estão presentes
                    if (team_count != n_teams)
                    {
                        write_logfile("CANNOT START, NOT ENOUGH TEAMS");
                        continue;
                    }

                    // iniciar a corrida quando todos os team managers acabarem de criar as car threads
                    pthread_mutex_lock(&race->race_mutex);
                    race->threads_created = 0;
                    pthread_cond_broadcast(&race->cv_allow_teams);

                    while (race->threads_created != race->car_count)
                        pthread_cond_wait(&race->cv_allow_start, &race->race_mutex);

                    race->race_started = 1;
                    pthread_cond_broadcast(&race->cv_race_started);
                    pthread_mutex_unlock(&race->race_mutex);

                    printf("[RACE MANAGER received \"%s\" command]\nRace initiated!\n", buffer);
                }
                else
                    printf("[RACE MANAGER received unknown command]: %s \n", buffer);
            }
        }
    }

    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    // -------------------- WAIT FOR TEAMS -------------------- //

    for (int i = 0; i < n_teams; i++)
    {
        waitpid(teams[i], NULL, 0);
    }

    close(fd_named_pipe);

    printf("---------------------\n[%ld] Race Manager process finished\n---------------------\n", (long)getpid());
    exit(0);
}

void teamManager(int pipe, int teamID)
{
    // -------------------- CREATE BOX -------------------- //

    all_teams[teamID].box_state = BOX_FREE;
    all_teams[teamID].car_id = -1;

    // -------------------- CREATE MUTEX SEMAPHORES -------------------- //

    all_teams[teamID].mutex_box = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    all_teams[teamID].mutex_car_state_box = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    // -------------------- CREATE CVs -------------------- //
    all_teams[teamID].cond_box_full = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    all_teams[teamID].cond_box_free = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    signal(SIGTSTP, SIG_IGN);

    while (1)
    {
        printf("---------------------\n[%ld] Team Manager #%d process working\n---------------------\n", (long)getpid(), teamID);
        // -------------------- CREATE CAR THREADS -------------------- //

        pthread_t carThreads[config->max_carros];
        int cars_length, count = 0;

        pthread_mutex_lock(&race->race_mutex);
        while (race->threads_created < 0)
            pthread_cond_wait(&race->cv_allow_teams, &race->race_mutex);
        cars_length = race->car_count;
        pthread_mutex_unlock(&race->race_mutex);

        for (int i = 0; i < cars_length; i++)
        {
            // se é um carro da equipa
            if (strcmp(cars[i].team, all_teams[teamID].name) == 0)
            {
                int argv[2];
                argv[0] = i;
                argv[1] = pipe;

                race->n_cars_racing += 1;
                pthread_create(&carThreads[count], NULL, carThread, argv);
            }
        }

        // block until race starts
        pthread_mutex_lock(&race->race_mutex);
        while (race->race_started != 1)
        {
            pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
        }
        pthread_mutex_unlock(&race->race_mutex);

        // -------------------- BOX STATE -------------------- //

        float time_units = (float)1 / (float)config->time_units;

        while (1)
        {
            pthread_mutex_lock(&race->race_mutex);
            if (race->race_started == 0)
            {
                printf("TEAM %s BOX IS CLOSING...\n", all_teams[teamID].name);
                pthread_mutex_unlock(&race->race_mutex);
                break;
            }
            pthread_mutex_unlock(&race->race_mutex);

            pthread_mutex_lock(&all_teams[teamID].mutex_box);
            for (int i = 0; i < count; i++)
            {
                if (strcmp(cars[i].team, all_teams[teamID].name) == 0 && cars[i].state == SEGURANCA && all_teams[teamID].box_state == BOX_FREE)
                {
                    all_teams[teamID].box_state = BOX_RESERVED;
                    break;
                }
            }
            pthread_mutex_unlock(&all_teams[teamID].mutex_box);

            // wait for a car to enter the box
            printf("TEAM %s BOX IS WAITING TO RECEIVE CARS!\n", all_teams[teamID].name);
            printf("TEAM %s BOX STATE IS %d\n", all_teams[teamID].name, all_teams[teamID].box_state);

            pthread_mutex_lock(&all_teams[teamID].mutex_box);
            while (all_teams[teamID].car_id < 0)
            {
                pthread_cond_wait(&all_teams[teamID].cond_box_full, &all_teams[teamID].mutex_box);
            }
            pthread_mutex_unlock(&all_teams[teamID].mutex_box);

            all_teams[teamID].box_state = BOX_FULL;
            race -> n_abastecimentos += 1;
            printf("\nTEAM %s BOX IS REPAIRING CAR %d\n", all_teams[teamID].name, all_teams[teamID].car_id);

            if (cars[all_teams[teamID].car_id].avaria == MALFUNCTION)
            {
                usleep((rand() % config->T_Box_Max - rand() % config->T_Box_min + 1) * time_units * 1000000); // repair car
                cars[all_teams[teamID].car_id].avaria = WORKING;
            }
            usleep(2 * time_units * 1000000); // fuel
            cars[all_teams[teamID].car_id].combustivel = config->fuel_capacity;

            // box is free
            printf("\nBOX FROM TEAM %s HAS REPAIRED CAR %d.\n", all_teams[teamID].name, all_teams[teamID].car_id);

            pthread_mutex_lock(&all_teams[teamID].mutex_box);
            all_teams[teamID].car_id = -1;
            all_teams[teamID].box_state = BOX_FREE;
            // signal the cars the box is free
            pthread_cond_signal(&all_teams[teamID].cond_box_free);
            pthread_mutex_unlock(&all_teams[teamID].mutex_box);
        }

        // -------------------- WAIT FOR CAR THREADS TO DIE -------------------- //

        for (int i = 0; i < count; i++)
        {
            pthread_join(carThreads[i], NULL);
        }

        printf("---------------------\n[%ld] Team Manager #%d process finished\n---------------------\n", (long)getpid(), teamID);
    }
    exit(0);
}

void *carThread(void *array_infos_p)
{
    char fim_volta[LINESIZE], desistencia[LINESIZE], mud_estado[LINESIZE];
    int bytes_received;
    msg received_msg;
    int *array_infos = (int *)array_infos_p;
    int car_id = array_infos[0];
    car_struct *car = cars + car_id;
    int team;
    for (team = 0; team < config->n_teams; team++)
    {
        if (strcmp(car->team, all_teams[team].name) == 0)
            break;
    }

    int pipe = array_infos[1];

    float time_units = (float)1 / (float)config->time_units;
    float speed = car->speed;
    float consumption = car->consumption;

    // array_infos[0] = car index    array_infos[1] = channels_write;

    printf("------------\n[%ld] Car #%d thread working\n------------\n", (long)getpid(), car->num);

    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    // -------------------- CAR RACING -------------------- //
    //! BLOCKS UNTIL RACE STARTS
    pthread_mutex_lock(&race->race_mutex);
    ++(race->threads_created);
    pthread_cond_signal(&race->cv_allow_start);
    while (race->race_started != 1)
    {
        pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
    }
    pthread_mutex_unlock(&race->race_mutex);

    while (1)
    {

        // printf("Car %d struct: TEAM %s, AVARIA: %d, COMBUSTÍVEL: %f, d_PERCORRIDA: %f, VOLTAS: %d, STATE: %d, SPEED: %f, CONSUMPTION: %f, RELIABILITY: %d\n", car->num, car->team, car->avaria, car->combustivel, car->dist_percorrida, car->voltas, car->state, speed, consumption, car->reliability);

        if (car->dist_percorrida >= config->turn_distance)
        {
            car->voltas += 1;
            car->dist_percorrida = 0 + (car->dist_percorrida - config->turn_distance);
            printf("\nCar %d completed a turn!\nNumber of turns: %d\n\n", car->num, car->voltas);
        }

        if (car->voltas == config->turns_number)
        {
            car->state = TERMINADO;
        }

        if (car->combustivel <= 0 && car->state != TERMINADO)
        {
            car->state = DESISTENCIA;
        }

        switch (car->state)
        {

        case CORRIDA:
            // -------------------- MALFUNCTION MESSAGE FROM MSQ -------------------- //
            if ((bytes_received = msgrcv(mqid, &received_msg, sizeof(msg), car->num, IPC_NOWAIT)) < 0 && errno != ENOMSG)
            {
                write_logfile("ERROR RECEIVING MESSAGE FROM MSQ");
                exit(0);
            }

            else if (bytes_received > 0)
            {
                //* MUDA DE ESTADO
                car->state = SEGURANCA;
                car->avaria = MALFUNCTION;
                write(pipe, &car->state, sizeof(int));
                strcpy(mud_estado, "");
                sprintf(mud_estado, "NEW MALFUNCTION IN CAR %d AND CHANGED STATE <<CORRIDA>> TO STATE <<SEGURANCA>>\n", car->num);
                write_logfile(mud_estado);
            }

            /* se atingir combustível suficiente apenas para 2 voltas e se não lhe faltar uma volta para acabar a corrida
                    condição para assegurar que o carro não vai para a box quando podia muito bem acabar a corrida */
            //if (car->combustivel <= ((2 * turn_dist * consumption) / speed))

            if (car->combustivel <= ((2 * config->turn_distance * consumption) / speed) && car->voltas + 2 < config->turns_number)
            {
                //* MUDA DE ESTADO
                car->state = SEGURANCA;
                write(pipe, &car->state, sizeof(int));
                strcpy(mud_estado, "");
                sprintf(mud_estado, "CAR %d CHANGED STATE <<CORRIDA>> TO STATE <<SEGURANCA>>\n", car->num);
                write_logfile(mud_estado);
            }

            /* 1ª condição: se atingir combustível suficiente apenas para 4 voltas
                2ª condição: se faltar menos de 4 voltas para acabar a corrida, ignora*/
            else if (car->combustivel <= ((4 * config->turn_distance * consumption) / speed) && car->voltas + 4 < config->turns_number)
            {
                // se carro estiver a chegar ao ponto de partida, entra na box e conclui a volta
                if (config->turn_distance < car->dist_percorrida + speed && all_teams[team].box_state == BOX_FREE)
                {

                    //* MUDA DE ESTADO
                    pthread_mutex_lock(&all_teams[team].mutex_car_state_box);
                    car->state = BOX;
                    strcpy(mud_estado, "");
                    sprintf(mud_estado, "CAR %d CHANGED STATE <<CORRIDA>> TO STATE <<BOX>>\n", car->num);
                    write_logfile(mud_estado);

                    printf("**CAR %d IS ENTERING TEAM %s BOX**\n", car->num, car->team);
                    all_teams[team].car_id = car->num;
                    car->n_stops_box += 1;
                    write(pipe, &car->state, sizeof(int));

                    pthread_cond_signal(&all_teams[team].cond_box_full);
                    pthread_mutex_unlock(&all_teams[team].mutex_car_state_box);

                    pthread_mutex_lock(&all_teams[team].mutex_box);
                    while (all_teams[team].box_state != BOX_FREE)
                    {
                        pthread_cond_wait(&all_teams[team].cond_box_free, &all_teams[team].mutex_box);
                    }
                    pthread_mutex_unlock(&all_teams[team].mutex_box);

                    //* MUDA DE ESTADO
                    car->state = CORRIDA;
                    strcpy(mud_estado, "");
                    sprintf(mud_estado, "CAR %d CHANGED STATE <<BOX>> TO STATE <<CORRIDA>>\n", car->num);
                    write_logfile(mud_estado);

                    car->voltas += 1;
                    car->dist_percorrida = 0;
                    consumption = car->consumption;
                    printf("\n**Car %d completed a turn!\nNumber of turns: %d**\n\n", car->num, car->voltas);
                }
            }

            else
            {
                usleep(time_units * 1000000);
                car->combustivel -= consumption;
                car->dist_percorrida += speed;
            }

            break;

        case SEGURANCA:
            // se box estiver reservada e carro[SEGURANCA] estiver a aproximar-se da meta, entra na box
            if (all_teams[team].box_state != BOX_FULL && config->turn_distance <= car->dist_percorrida + speed)
            {
                //* MUDA DE ESTADO
                pthread_mutex_lock(&all_teams[team].mutex_car_state_box);
                car->state = BOX;
                strcpy(mud_estado, "");
                sprintf(mud_estado, "CAR %d CHANGED STATE <<CORRIDA>> TO STATE <<BOX>>\n", car->num);
                write_logfile(mud_estado);
                all_teams[team].car_id = car->num;
                write(pipe, &car->state, sizeof(int));
                pthread_cond_signal(&all_teams[team].cond_box_full);
                pthread_mutex_unlock(&all_teams[team].mutex_car_state_box);


                pthread_mutex_lock(&all_teams[team].mutex_box);
                while (all_teams[team].box_state != BOX_FREE)
                {
                    pthread_cond_wait(&all_teams[team].cond_box_free, &all_teams[team].mutex_box);
                }
                pthread_mutex_unlock(&all_teams[team].mutex_box);

                //* MUDA DE ESTADO
                car->state = CORRIDA;
                strcpy(mud_estado, "");
                sprintf(mud_estado, "CAR %d CHANGED STATE <<BOX>> TO STATE <<CORRIDA>>\n", car->num);
                write_logfile(mud_estado);

                speed = car->speed;
                car->voltas += 1;
                car->dist_percorrida = 0;
                consumption = car->consumption;
                printf("\n**CAR %d COMPLETED A TURN!\nNUMBER OF TURNS: %d**\n\n", car->num, car->voltas);
            }

            else
            {
                speed = cars->speed * 0.3;
                consumption = car->consumption * 0.4;
                usleep(time_units * 1000000);
                car->combustivel -= consumption;
                car->dist_percorrida += speed;
            }

            break;

        case BOX:
            car->dist_percorrida = 0;
            speed = 0;
            consumption = 0;

            break;

        case TERMINADO:
            race->n_cars_racing -= 1;
            sprintf(fim_volta, "CAR %d FINISHED THE RACE!\n", car->num);
            write_logfile(fim_volta);
            write(pipe, &car->state, sizeof(int));

            if(race->n_cars_racing == 0){
                printf("\n\n\n*******\nEND OF RACE\n*******");
            }

            pthread_mutex_lock(&classif_mutex);
            car->classificacao = race->classificacao;
            race->classificacao += 1;
            pthread_mutex_unlock(&classif_mutex);

            pthread_exit(NULL);

            break;

        case DESISTENCIA:
            race->n_cars_racing -= 1;
            sprintf(desistencia, "CAR %d ABANDONED THE RACE!\n", car->num);
            write_logfile(desistencia);
            write(pipe, &car->state, sizeof(int));
            pthread_exit(NULL);

            break;
        }
    }
}
