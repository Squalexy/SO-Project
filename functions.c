#include "declarations.h"

int *read_content_from_file()
{
    FILE *fptr;
    char line[LINESIZE];
    char *token = NULL;

    int *file_contents = (int *)malloc(ARRAYSIZE * sizeof(int));

    if ((fptr = fopen("config.txt", "r")) == NULL)
    {
        // write_logfile("ERROR OPENING FILE");
        exit(1);
    }

    for (int i = 0; i < ARRAYSIZE; i++)
    {
        if (fgets(line, LINESIZE, fptr) != NULL)
        {
            if (strchr(line, ',') != NULL)
            {
                token = strtok(line, ",");
                file_contents[i] = atoi(token);
                i++;
                token = strtok(NULL, "");
                file_contents[i] = atoi(token);
            }
            else
                file_contents[i] = atoi(line);
        }
    }

    if (file_contents[3] < 3)
    {
        // perror("Error opening file. Number of teams below 3.\n");
        // write_logfile("ERROR OPENING FILE, NUMBER");
        exit(1);
    }

    fclose(fptr);
    return file_contents;
}

void print_content_from_file(int **file_contents)
{
    printf("--- CONTENT FROM FILE ---\n");
    for (int i = 0; i < ARRAYSIZE; i++)
    {
        switch (i)
        {
        case 0:
            printf("TIME UNITS : %d\n", *file_contents[i]);
        case 1:
            printf("TURN DISTANCE : %d\n", *file_contents[i]);
        case 2:
            printf("TURNS NUMBER : %d\n", *file_contents[i]);
        case 3:
            printf("TEAMS NUMBER : %d\n", *file_contents[i]);
        case 4:
            printf("CARS LIMIT : %d\n", *file_contents[i]);
        case 5:
            printf("T_AVARIA : %d\n", *file_contents[i]);
        case 6:
            printf("T_BOX_MIN : %d\n", *file_contents[i]);
        case 7:
            printf("T_BOX_MAX : %d\n", *file_contents[i]);
        case 8:
            printf("FUEL CAPACITY : %d\n", *file_contents[i]);
        }
    }
    printf("\n");
}

void write_logfile(char *text_to_write)
{
    FILE *fptr;
    fptr = fopen("log.txt", "a");
    if (fptr == NULL)
    {
        perror("Error opening file.\n");
        exit(1);
    }

    int hours, minutes, seconds;
    time_t now = time(NULL);

    struct tm *local = localtime(&now);
    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    printf("%02d:%02d:%02d  %s\n", hours, minutes, seconds, text_to_write);
    fprintf(fptr, "%02d:%02d:%02d  %s\n", hours, minutes, seconds, text_to_write);

    fclose(fptr);
}

void malfunctionManager(void)
{
    printf("[%ld] Malfunction Manager process working\n", (long)getpid());
    sleep(5);
    printf("[%ld] Malfunction Manager process finished\n", (long)getpid());
    exit(0);
}

void raceManager(int n_teams)
{
    printf("[%ld] Race Manager process working\n", (long)getpid());

    pid_t teams[n_teams];
    for (int i = 0; i < n_teams; i++)
    {
        pid_t teamPID;
        if ((teamPID = fork()) == 0)
        {
            teamManager(i);
        }
        else if (teamPID == -1)
        {
            // perror("Error creating Team Manager process\n");
            char err_team[] = "ERROR CREATING TEAM MANAGER PROCESS";
            write_logfile(err_team);
            exit(1);
        }
        teams[i] = teamPID; // teams[ID_1, ID_2, ID_3,...]
    }

    // wait for teams
    for (int i = 0; i < n_teams; i++)
    {
        waitpid(teams[i], NULL, 0);
    }

    printf("[%ld] Race Manager process finished\n", (long)getpid());
    exit(0);
}

void teamManager(int teamID)
{
    printf("[%ld] Team Manager #%d process working\n", (long)getpid(), teamID);

    int numCar = 2;
    int carID[max_carros];
    pthread_t carThreads[max_carros];

    if (numCar > max_carros)
    {
        char err_cars[] = "NUMBER OF CARS ABOVE THE MAXIMUM ALLOWED";
        write_logfile(err_cars);
        exit(1);
    }

    // create car threads
    for (int i = 0; i < numCar; i++)
    {
        carID[i] = (teamID * numCar) + i;
        pthread_create(&carThreads[i], NULL, carThread, &carID[i]);
        //printf("[%ld] Car #%d thread created\n", (long)getpid(), carID[i]);

        char car_text[LINESIZE];
        sprintf(car_text, "NEW CAR LOADED => TEAM %d, CAR: %02d", teamID, carID[i]);
        write_logfile(car_text);
    }

    // wait for threads to die
    for (int i = 0; i < numCar; i++)
    {
        pthread_join(carThreads[i], NULL);
    }

    printf("[%ld] Team Manager #%d process finished\n", (long)getpid(), teamID);
    exit(0);
}

void *carThread(void *carID_p)
{
    int carID = *((int *)carID_p);

    printf("[%ld] Car #%d thread working\n", (long)getpid(), carID);
    sleep(5);
    printf("[%ld] Car #%d thread finished\n", (long)getpid(), carID);
    pthread_exit(NULL);
}

// TODO: - nº do carro em cada equipa é igual
// TODO: - são criadas threads carro com o mesmo ID, em cada equipa
