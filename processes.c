/*
    UNIVERSIDADE DE COIMBRA - FACULDADE DE CIÊNCIAS E TECNOLOGIA
    SISTEMAS OPERATIVOS
    
    PROJECT BY:
    ALEXY DE ALMEIDA Nº2019192123
    JOSÉ GONÇALVAS Nºº2019223292
*/

#include "declarations.h"

void malfunctionManager(void)
{
    int signo;
    msg msg_to_send;
    char malfunction[LINESIZE];

    sigprocmask(SIG_UNBLOCK, &set_allow, NULL);
    signal(SIGINT, sigint_malfunction);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);

    while (1)
    {
        strcpy(malfunction, "");
        sprintf(malfunction, "[%ld] MALFUNCTION MANAGER PROCESS WAITING FOR RACE START\n", (long)getpid());
        write_logfile(malfunction);

        sigprocmask(SIG_BLOCK, &set_allow, NULL);
        pthread_mutex_lock(&race->race_mutex);
        while (race->race_started != 1)
        {
            pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
            if (race->end_sim == 1)
            {
                pthread_mutex_unlock(&race->race_mutex);
                sigprocmask(SIG_UNBLOCK, &set_allow, NULL);
                sigwait(&set_allow, &signo);
            }
        }
        pthread_mutex_unlock(&race->race_mutex);
        sigprocmask(SIG_UNBLOCK, &set_allow, NULL);

        strcpy(malfunction, "");
        sprintf(malfunction, "[%ld] MALFUNCTION MANAGER PROCESS WORKING\n", (long)getpid());
        write_logfile(malfunction);

        while (1)
        {
            pthread_mutex_lock(&race->race_mutex);
            if (race->race_started == 0)
            {
                pthread_mutex_unlock(&race->race_mutex);
                break;
            }
            pthread_mutex_unlock(&race->race_mutex);

            usleep(config->T_Avaria * (1.0 / (float)config->time_units) * 1000000);
            for (int i = 0; i < race->car_count; i++)
            {
                if (rand() % 101 > cars[i].reliability)
                {
                    msg_to_send.car_id = cars[i].num;
                    if (msgsnd(mqid, &msg_to_send, sizeof(msg_to_send) - sizeof(long), 0) < 0)
                    {
                        write_logfile("ERROR SENDING MESSAGE TO MSQ\n");
                        exit(0);
                    }
                    strcpy(malfunction, "");
                    sprintf(malfunction, "SENDING MALFUNCTION TO CAR %d\n", cars[i].num);
                    race->n_avarias += 1;
                    write_logfile(malfunction);
                }
            }
        }

        strcpy(malfunction, "");
        sprintf(malfunction, "[%ld] MALFUNCTION MANAGER PROCESS FINISHED\n", (long)getpid());
        write_logfile(malfunction);
    }
}

void raceManager()
{
    char racing[LINESIZE];
    int n_teams = config->n_teams;
    teamsPID = (pid_t *)malloc(sizeof(pid_t) * n_teams);
    channels = (int **)malloc(sizeof(int *) * n_teams);

    sigprocmask(SIG_UNBLOCK, &set_allow, NULL);
    signal(SIGINT, sigint_race);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);

    // -------------------- CREATE UNNAMED PIPES -------------------- //

    for (int i = 0; i < n_teams; i++)
    {
        channels[i] = (int *)malloc(sizeof(int) * 2);
        pipe(channels[i]);
    }

    // --------------- OPEN NAMED PIPE FOR READ-WRITE --------------- //

    if ((fd_named_pipe = open(PIPE_NAME, O_RDWR)) < 0)
    {
        write_logfile("CANNOT OPEN PIPE FOR READ-WRITE\n");
        exit(1);
    }

    // ----------------------- WORK ----------------------- //
    char buffer[BUFFERSIZE];
    int nread;
    int team_count = 0, finished = 0;
    fd_set read_set;

    strcpy(racing, "");
    sprintf(racing, "[%ld] RACE MANAGER PROCESS WORKING\n", (long)getpid());
    write_logfile(racing);
    while (1)
    {
        FD_ZERO(&read_set);
        for (int i = 0; i < n_teams; i++)
        {
            FD_SET(channels[i][0], &read_set);
        }
        FD_SET(fd_named_pipe, &read_set);

        // SELECT NAMED PIPE AND UNNAMED PIPES
        if (select(fd_named_pipe + 1, &read_set, NULL, NULL, NULL) > 0)
        {
            for (int i = 0; i < n_teams; i++)
            {
                if (FD_ISSET(channels[i][0], &read_set))
                {
                    // IF FROM UNNAMED PIPE DO SOMETHING ELSE ABOUT THE CAR STATE
                    int state = 0;
                    nread = read(channels[i][0], &state, sizeof(int));
                    if (nread > 0)
                        strcpy(racing, "");
                    sprintf(racing, "RECEIVED FROM TEAM %d'S UNNAMED PIPE: \"%d\"\n", i, state);
                    write_logfile(racing);

                    if (nread > 0 && (state == TERMINADO || state == DESISTENCIA))
                        finished++;

                    pthread_mutex_lock(&race->race_mutex);
                    if (finished == race->car_count)
                    {
                        race->race_started = 0;
                        race->threads_created = -1;

                        finished = 0;
                        write_logfile("RACE ENDED");
                        pthread_cond_broadcast(&race->cv_race_started);
                        pthread_mutex_unlock(&race->race_mutex);
                        sigprocmask(SIG_UNBLOCK, &set_allow, NULL);
                    }
                    else
                    {
                        pthread_mutex_unlock(&race->race_mutex);
                    }
                }
            }

            if (FD_ISSET(fd_named_pipe, &read_set))
            {

                // IF FROM NAMED PIPE, CHECK AND DO COMMAND
                nread = read(fd_named_pipe, buffer, BUFFERSIZE);
                buffer[nread - 1] = '\0';

                // REJECT IF RACE ALREADY STARTED
                pthread_mutex_lock(&race->race_mutex);
                if (race->race_started == 1)
                {
                    strcpy(racing, "");
                    sprintf(racing, "RACE MANAGER RECEIVED \"%s\" COMMAND --- REJECTED, RACE ALREADY STARTED!\n", buffer);
                    write_logfile(racing);
                    pthread_mutex_unlock(&race->race_mutex);
                    continue;
                }
                pthread_mutex_unlock(&race->race_mutex);

                if (strncmp("ADDCAR", buffer, 6) == 0)
                {

                    strcpy(racing, "");
                    sprintf(racing, "RACE MANAGER RECEIVED \"%s\" COMMAND --- ACCEPTED\n", buffer);
                    write_logfile(racing);
                    char team_name[TEAM_NAME_SIZE];
                    char *token = NULL;
                    char fields[10][64]; // COMMAND SEPARATED BY COMMAS AND SPACES (EACH FIELD)

                    int car_num = 0, car_speed = 0, car_reliability = 0, converted = 0;
                    float car_consumption = 0;

                    token = strtok(buffer + 7, ", "); // RETRIEVE "ADDCAR"
                    for (int i = 0; i < 10; i++)
                    {
                        strncpy(fields[i], token, 64);
                        token = strtok(NULL, ", ");
                    }

                    // VERIFIES IF INTEGERS ARE VALID AND NOT NEGATIVE
                    if (check_int(fields[3]) == false || check_int(fields[5]) == false || check_int(fields[9]) == false)
                    {
                        write_logfile("ERROR READING NAMED PIPE - INVALID INTEGER");
                        continue;
                    }

                    // VERIFIES IF FLOAT IS VALID AND NOT NEGATIVE
                    double temp;
                    if (sscanf(fields[7], "%lf", &temp) == 0 || !strcmp(fields[7], "-"))
                    {
                        write_logfile("ERROR READING NAMED PIPE - INVALID FLOAT");
                        continue;
                    }

                    // READS COMMANDS AND NUMBERS FROM NAMED PIPE AND CHECKS IF THEY ARE VALID
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

                    if (converted != 5)
                    {
                        write_logfile("ERROR READING NAMED PIPE - INVALID FIELDS, MUST USE CORRECT COMMANDS AND INPUTS\n");
                        continue;
                    }

                    for (int i = 0; i < race->car_count; i++)
                    {
                        if (car_num == cars[i].num)
                        {
                            write_logfile("ERROR READING NAMED PIPE - CAR ALREADY EXISTS\n");
                            continue;
                        }
                    }

                    // VERIFIES IF A TEAM EXISTS; IF IT DOESN'T, CREATES A NEW ONE
                    int i;
                    for (i = 0; i < team_count; i++)
                    {
                        if (strcmp(team_name, all_teams[i].name) == 0)
                        {
                            if (all_teams[i].number_of_cars == config->max_carros)
                            {
                                continue;
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

                    // IF NUMBER OF CARS HASN'T EXCEED MAXIMUM ALLOWED AND COMMANDS ARE VALID, CREATES CAR STRUCT
                    int car_i;
                    pthread_mutex_lock(&race->race_mutex);
                    car_i = (race->car_count)++;
                    pthread_mutex_unlock(&race->race_mutex);

                    strncpy(cars[car_i].team, team_name, TEAM_NAME_SIZE);
                    cars[car_i].num = car_num;
                    cars[car_i].speed = car_speed;
                    cars[car_i].consumption = car_consumption;
                    cars[car_i].reliability = car_reliability;

                    strcpy(racing, "");
                    sprintf(racing, "NEW CAR LOADED => TEAM: %s, CAR: %d, SPEED: %d, CONSUMPTION: %.2f, RELIABILITY: %d", team_name, car_num, car_speed, car_consumption, car_reliability);
                    write_logfile(racing);
                }
                else if (strcmp("START RACE!", buffer) == 0)
                {
                    // VERIFIES IF ALL TEAMS ARE PRESENT
                    if (team_count != n_teams)
                    {
                        write_logfile("CANNOT START, NOT ENOUGH TEAMS");
                        continue;
                    }
                    sigprocmask(SIG_BLOCK, &set_allow, NULL);

                    // BEGINS RACE WHEN ALL TEAM MANAGERS FINISHED CREATING CAR THREADS
                    pthread_mutex_lock(&race->race_mutex);
                    race->n_avarias = 0;
                    race->n_abastecimentos = 0;
                    race->classificacao = 1;

                    race->threads_created = 0;
                    pthread_cond_broadcast(&race->cv_allow_teams);

                    while (race->threads_created != race->car_count)
                        pthread_cond_wait(&race->cv_allow_start, &race->race_mutex);

                    race->race_started = 1;
                    pthread_cond_broadcast(&race->cv_race_started);
                    pthread_mutex_unlock(&race->race_mutex);

                    strcpy(racing, "");
                    sprintf(racing, "RACE MANAGER RECEIVED \"%s\" COMMAND --- RACE INITIATED!\n", buffer);
                    write_logfile(racing);
                }
                else
                {
                    strcpy(racing, "");
                    sprintf(racing, "RACE MANAGER RECEIVED UNKNOWN COMMAND: %s \n", buffer);
                    write_logfile(racing);
                }
            }
        }
    }
}

void teamManager()
{
    char car_racing[LINESIZE];
    int signo;
    car_IDs = (int *)malloc(sizeof(int) * config->max_carros);
    carThreads = (pthread_t *)malloc(sizeof(pthread_t) * config->max_carros);

    char teaming[LINESIZE];

    sigprocmask(SIG_UNBLOCK, &set_allow, NULL);
    signal(SIGINT, sigint_team);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);

    //* -------------------- CREATE MUTEX SEMAPHORES -------------------- //
    mutex_box = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    //* -------------------- CREATE CVs -------------------- //
    cond_box_full = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    cond_box_free = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

    // BLOCKS ALL SIGNALS
    sigprocmask(SIG_BLOCK, &set_allow, NULL);
    while (1)
    {

        strcpy(teaming, "");
        sprintf(teaming, "[%ld] TEAM MANAGER #%d PROCESS WORKING\n", (long)getpid(), teamID);
        write_logfile(teaming);

        //* -------------------- CREATE BOX -------------------- //
        all_teams[teamID].box_state = BOX_FREE;
        all_teams[teamID].car_id = -1;
        all_teams[teamID].reserved_count = 0;
        all_teams[teamID].finished = 0;

        //* -------------------- CREATE CAR THREADS -------------------- //
        int cars_length = 0;
        count = 0;

        pthread_mutex_lock(&race->race_mutex);
        ++(race->waiting);
        pthread_cond_signal(&race->cv_waiting);

        while (race->threads_created < 0)
        {
            pthread_cond_wait(&race->cv_allow_teams, &race->race_mutex);
            if (race->end_sim == 1)
            {
                pthread_mutex_unlock(&race->race_mutex);
                sigprocmask(SIG_UNBLOCK, &set_allow, NULL);
                sigwait(&set_allow, &signo);
            }
        }

        --(race->waiting);
        cars_length = race->car_count;
        pthread_mutex_unlock(&race->race_mutex);

        for (int i = 0; i < cars_length; i++)
        {
            // IF CARS BELONG TO THE TEAM
            if (strcmp(cars[i].team, all_teams[teamID].name) == 0)
            {
                car_IDs[count] = i;
                pthread_create(&carThreads[count], NULL, carThread, &car_IDs[count]);
                count++;
            }
        }

        //! BLOCK UNTIL RACE STARTS
        pthread_mutex_lock(&race->race_mutex);
        while (race->race_started != 1)
        {
            pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
        }
        pthread_mutex_unlock(&race->race_mutex);

        //* -------------------- BOX STATE -------------------- //

        float time_units = 1.0 / (float)config->time_units;
        car_struct *car = NULL;

#ifdef DEBUG
        //? printf("** TEAM %s BOX IS WORKING **\n", all_teams[teamID].name);
        fflush(stdout);
#endif

        while (1)
        {

#ifdef DEBUG
            //? printf("** TEAM %s BOX IS WAITING TO RECEIVE CARS **\n", all_teams[teamID].name);
            fflush(stdout);
#endif

            // IF BOX IS IN <BOX_FREE> STATE AND THERE'S A CAR IN <SEGURANCA> STATE, BOX STATE BECOMES <RESERVED>
            pthread_mutex_lock(&mutex_box);
            while (all_teams[teamID].car_id < 0 && all_teams[teamID].finished < count)
            {
                // WAITS FOR A CAR TO ENTER THE BOX
                pthread_cond_wait(&cond_box_full, &mutex_box);
                if (all_teams[teamID].reserved_count > 0 && all_teams[teamID].box_state == BOX_FREE)
                {
                    all_teams[teamID].box_state = BOX_RESERVED;
                }
            }

            if (all_teams[teamID].finished == count)
            {
                pthread_mutex_unlock(&mutex_box);
                break;
            }

            all_teams[teamID].box_state = BOX_FULL;
            car = cars + (all_teams[teamID].car_id);
            pthread_mutex_unlock(&mutex_box);
            race->n_abastecimentos += 1;

#ifdef DEBUG
            //? printf("** TEAM %s BOX IS REPAIRING CAR %d **\n", all_teams[teamID].name, car->num);
            fflush(stdout);
#endif

            // REPAIRING CAR
            if (car->avaria == MALFUNCTION)
            {
                usleep(((rand() % config->T_Box_Max) - (rand() % config->T_Box_min) + 1) * time_units * 1000000);
                car->avaria = WORKING;
            }

            // FUELING CAR
            usleep(2 * time_units * 1000000);
            car->combustivel = (float)config->fuel_capacity;

            pthread_mutex_lock(&mutex_box);
            all_teams[teamID].car_id = -1;

            if (all_teams[teamID].reserved_count > 0)
                all_teams[teamID].box_state = BOX_RESERVED;
            else
                all_teams[teamID].box_state = BOX_FREE;

            // SIGNAL CARS THAT THE BOX IS FREE
            pthread_cond_signal(&cond_box_free);
            pthread_mutex_unlock(&mutex_box);

            // BOX IS FREE

            strcpy(car_racing, "");
            sprintf(car_racing, "TEAM %s BOX REPAIRED CAR %d\n", car->team, car->num);
            write_logfile(car_racing);

            fflush(stdout);
        }

        pthread_mutex_lock(&race->race_mutex);
        while (race->race_started != 0)
        {
            pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
        }
        pthread_mutex_unlock(&race->race_mutex);

        for (int i = 0; i < count; i++)
        {
            pthread_join(carThreads[i], NULL);
        }
    }
}

void *carThread(void *id)
{
    char fim_volta[LINESIZE], desistencia[LINESIZE], mud_estado[LINESIZE];
    char car_racing[LINESIZE];
    int bytes_received;
    msg received_msg;

    int car_id = *((int *)id);
    car_struct *car = cars + car_id;
    int team = teamID;

    pthread_sigmask(SIG_BLOCK, &set_allow, NULL);

    car->avaria = WORKING;
    car->combustivel = (float)config->fuel_capacity;
    car->dist_percorrida = 0;
    car->voltas = 0;
    car->state = CORRIDA;
    car->n_stops_box = 0;
    car->classificacao = 0;

    int pipe = channels[teamID][1];

    float time_units = 1.0 / (float)config->time_units;
    float speed = car->speed;
    float consumption = car->consumption;

    // STATE BEFORE ENTERING THE TEAM BOX
    int prev_state = 0;

    strcpy(car_racing, "");
    sprintf(car_racing, "[%ld] CAR #%d THREAD WORKING\n", (long)getpid(), car->num);
    write_logfile(car_racing);

    //* -------------------- CAR RACING -------------------- //

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

        if (car->state != BOX && car->state != DESISTENCIA && car->state != TERMINADO)
        {
            if (car->dist_percorrida >= config->turn_distance)
            {
                car->voltas += 1;
                car->dist_percorrida = 0 + (car->dist_percorrida - config->turn_distance);

                strcpy(car_racing, "");
                sprintf(car_racing, "CAR #%d COMPLETED A TURN! --- NUMBER OF TURNS: %d", car->num, car->voltas);
                write_logfile(car_racing);

                pthread_mutex_lock(&race->race_mutex);
                if (race->end_sim == 1)
                {
                    car->state = TERMINADO;
                }
                pthread_mutex_unlock(&race->race_mutex);
            }

            if (car->voltas == config->turns_number)
            {
                car->state = TERMINADO;
            }

            if (car->combustivel <= 0 && car->state != TERMINADO)
            {
                car->state = DESISTENCIA;
            }
        }

        // -------------------- MALFUNCTION MESSAGE FROM MSQ -------------------- //
        if ((bytes_received = msgrcv(mqid, &received_msg, sizeof(received_msg) - sizeof(long), car->num, IPC_NOWAIT)) < 0 && errno != ENOMSG)
        {
            write_logfile("ERROR RECEIVING MESSAGE FROM MSQ");
            exit(0);
        }
        else if (bytes_received >= 0 && (car->state == SEGURANCA || car->state == CORRIDA))
        {
            //* CHANGES CAR STATE

            pthread_mutex_lock(&mutex_box);
            ++(all_teams[teamID].reserved_count);
            pthread_cond_signal(&cond_box_full);
            pthread_mutex_unlock(&mutex_box);

            car->state = SEGURANCA;
            car->avaria = MALFUNCTION;
            write(pipe, &car->state, sizeof(int));
            strcpy(mud_estado, "");
            sprintf(mud_estado, "NEW MALFUNCTION IN CAR #%d RECEIVED\n", car->num);
            write_logfile(mud_estado);
        }

        switch (car->state)
        {
        case CORRIDA:

            // 1ST CONDITION: IF THERE'S ONLY ENOUGH FUEL FOR 2 TURNS
            // 2ND CONDITION: IF HE CAN'T FINISH THE RACE IN 2 TURNS

            if (car->combustivel <= ((2 * config->turn_distance * consumption) / speed) && car->voltas + 2 < config->turns_number)
            {
                //* CHANGES CAR STATE
                pthread_mutex_lock(&mutex_box);
                ++(all_teams[teamID].reserved_count);
                pthread_cond_signal(&cond_box_full);
                pthread_mutex_unlock(&mutex_box);

                car->state = SEGURANCA;
                write(pipe, &car->state, sizeof(int));
                strcpy(mud_estado, "");
                sprintf(mud_estado, "CAR #%d CHANGED STATE <<CORRIDA>> TO STATE <<SEGURANCA>>\n", car->num);
                write_logfile(mud_estado);
            }

            // 1ST CONDITION: IF THERE'S ONLY ENOUGH FUEL FOR 4 TURNS
            // 2ND CONDITION: IF HE CAN'T FINISH THE RACE IN 4 TURNS

            else if (car->combustivel <= ((4 * config->turn_distance * consumption) / speed) && car->voltas + 4 < config->turns_number)
            {
                // IF CAR IS NEAR THE BOX AND BOX STATE IS <BOX_FREE>, ENTERS THE BOX AND FINISHES ONE TURN
                if (config->turn_distance < car->dist_percorrida + speed)
                {
                    // if the simulator is going to close, end lap instead of going to box
                    pthread_mutex_lock(&race->race_mutex);
                    if (race->end_sim == 1)
                    {
                        car->state = TERMINADO;
                        pthread_mutex_unlock(&race->race_mutex);
                        break;
                    }
                    pthread_mutex_unlock(&race->race_mutex);

                    // verificar se a box esta livre
                    pthread_mutex_lock(&mutex_box);
                    if (all_teams[team].box_state == BOX_FREE)
                    {
                        all_teams[team].car_id = car_id;
                        pthread_cond_signal(&cond_box_full);
                        pthread_mutex_unlock(&mutex_box);

#ifdef DEBUG
                        //? printf("** CAR %d SIGNALED TEAM BOX WITH CV **\n", car->num);
                        fflush(stdout);
#endif

                        //* CHANGES CAR STATE
                        prev_state = car->state;
                        car->state = BOX;
                        car->n_stops_box += 1;
                        write(pipe, &car->state, sizeof(int));

                        strcpy(mud_estado, "");
                        sprintf(mud_estado, "CAR #%d CHANGED STATE <<CORRIDA>> TO STATE <<BOX>>\n", car->num);
                        write_logfile(mud_estado);
                    }
                    else
                    {
                        pthread_mutex_unlock(&mutex_box);
                    }
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
            // IF BOX STATE IS <BOX_RESERVED> AND CAR IS NEAR THE BOX, TRIES TO ENTER THE BOX
            if (config->turn_distance <= car->dist_percorrida + speed && car->voltas + 1 < config->turns_number)
            {
                // if the simulator is going to close, end lap instead of going to box
                pthread_mutex_lock(&race->race_mutex);
                if (race->end_sim == 1)
                {
                    car->state = TERMINADO;
                    pthread_mutex_unlock(&race->race_mutex);
                    break;
                }
                pthread_mutex_unlock(&race->race_mutex);

                // verificar se a box esta livre
                pthread_mutex_lock(&mutex_box);
                if (all_teams[team].box_state != BOX_FULL)
                {
                    all_teams[team].car_id = car_id;
                    pthread_cond_signal(&cond_box_full);
                    pthread_mutex_unlock(&mutex_box);

#ifdef DEBUG
                    //? printf("** CAR %d SIGNALED TEAM BOX WITH CV **\n", car->num);
                    fflush(stdout);
#endif

                    //* CHANGES CAR STATE
                    prev_state = car->state;
                    car->state = BOX;
                    car->n_stops_box += 1;
                    write(pipe, &car->state, sizeof(int));

                    strcpy(mud_estado, "");
                    sprintf(mud_estado, "CAR #%d CHANGED STATE <<SEGURANCA>> TO STATE <<BOX>>\n", car->num);
                    write_logfile(mud_estado);
                }
                else
                {
                    pthread_mutex_unlock(&mutex_box);
                }
            }

            else
            {
                speed = car->speed * 0.3;
                consumption = car->consumption * 0.4;
                usleep(time_units * 1000000);
                car->combustivel -= consumption;
                car->dist_percorrida += speed;
            }

            break;

        case BOX:

#ifdef DEBUG
            //? printf("** CAR #%d IS ENTERING TEAM %s BOX **\n", car->num, car->team);
            fflush(stdout);
#endif

            car->dist_percorrida = 0;
            speed = 0;
            consumption = 0;

#ifdef DEBUG
            //? printf("** CAR #%d SIGNALED THE BOX WITH CV **\n", car->num);
            fflush(stdout);
#endif
            pthread_mutex_lock(&mutex_box);
            if (prev_state == SEGURANCA)
                --(all_teams[teamID].reserved_count);
            while (all_teams[team].car_id >= 0)
            {
                pthread_cond_wait(&cond_box_free, &mutex_box);
            }
            pthread_mutex_unlock(&mutex_box);

#ifdef DEBUG
            //? printf("** CAR %d IS OUT OF THE BOX **\n", car->num);
            fflush(stdout);
#endif

            //* CHANGES CAR STATE
            // if the simulator is going to close, end race instead of continuing
            pthread_mutex_lock(&race->race_mutex);
            if (race->end_sim == 1)
            {
                car->state = TERMINADO;
                pthread_mutex_unlock(&race->race_mutex);
                break;
            }
            pthread_mutex_unlock(&race->race_mutex);

            car->state = CORRIDA;

            strcpy(mud_estado, "");
            sprintf(mud_estado, "CAR %d CHANGED STATE <<BOX>> TO STATE <<CORRIDA>>\n", car->num);
            write_logfile(mud_estado);

            speed = car->speed;
            car->voltas += 1;
            car->dist_percorrida = 0;
            consumption = car->consumption;

            strcpy(car_racing, "");
            sprintf(car_racing, "CAR #%d COMPLETED A TURN! --- NUMBER OF TURNS: %d", car->num, car->voltas);
            write_logfile(car_racing);

            break;

        case TERMINADO:
            sprintf(fim_volta, "CAR %d FINISHED THE RACE!\n", car->num);
            write_logfile(fim_volta);
            write(pipe, &car->state, sizeof(int));

            pthread_mutex_lock(&mutex_box);
            ++(all_teams[teamID].finished);
            pthread_cond_signal(&cond_box_full);
            pthread_mutex_unlock(&mutex_box);

            pthread_mutex_lock(&race->race_mutex);
            car->classificacao = race->classificacao;
            race->classificacao += 1;
            while (race->race_started != 0)
            {
                pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
            }
            pthread_mutex_unlock(&race->race_mutex);

            pthread_exit(NULL);
            break;

        case DESISTENCIA:
            sprintf(desistencia, "CAR %d ABANDONED THE RACE!\n", car->num);
            write_logfile(desistencia);
            write(pipe, &car->state, sizeof(int));

            pthread_mutex_lock(&mutex_box);
            ++(all_teams[teamID].finished);
            pthread_cond_signal(&cond_box_full);
            pthread_mutex_unlock(&mutex_box);

            pthread_mutex_lock(&race->race_mutex);
            while (race->race_started != 0)
            {
                pthread_cond_wait(&race->cv_race_started, &race->race_mutex);
            }
            pthread_mutex_unlock(&race->race_mutex);

            pthread_exit(NULL);
            break;
        }
    }
}
