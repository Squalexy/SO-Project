/*
Projeto realizado por:
    Alexy de Almeida nº2019192123
    José Gonçalves nº2019223292
*/

#include "declarations.h"

int main()
{

    // -------------------- RESET LOG FILE -------------------- //

    fclose(fopen("log.txt", "w"));

    // -------------------- READ CONTENT FROM LOG FILE -------------------- //

    int *file_contents = read_content_from_file();
    int time_units = file_contents[0];
    int turn_distance = file_contents[1];
    int turns_number = file_contents[2];
    int n_teams = file_contents[3];
    int max_carros = file_contents[4];
    int T_Avaria = file_contents[5];
    int T_Box_min = file_contents[6];
    int T_Box_Max = file_contents[7];
    int fuel_capacity = file_contents[8];

    // -------------------- PRINT CONTENT FROM LOG FILE -------------------- //

    print_content_from_file(file_contents);

    // -------------------- CREATE SEMAPHORES -------------------- //

    sem_unlink("WRITING");
    writing = sem_open("WRITING", O_CREAT | O_EXCL, 0700, 1);

    // -------------------- CREATE SHARED MEMORY -------------------- //

    char *mem;

    if ((shmid = shmget(IPC_PRIVATE, sizeof(config_struct) + sizeof(car_struct) * max_carros * n_teams + sizeof(team_box_struct) * n_teams, IPC_CREAT | 0766)) < 0)
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
    cars = (car_struct *)(mem + sizeof(config_struct));
    team_box = (team_box_struct *)(mem + sizeof(config_struct) + max_carros * n_teams * sizeof(car_struct));

    // -------------------- CREATE NAMED PIPE -------------------- //

    if ((mkfifo(PIPE_NAME, O_CREAT | O_EXCL | 0755) < 0) && (errno != EEXIST))
    {
        perror("Cannot create pipe!\n");
        exit(1);
    }

    // -------------------- OPEN NAMED PIPE FOR WRITING -------------------- //

    int fd;
    if ((fd = open(PIPE_NAME, O_WRONLY)) < 0)
    {
        perror("Cannot open pipe for writing\n");
        exit(1);
    }

    // redirect stdin to the input of the named pipe
    dup2(fd, fileno(stdin));
    close(fd);

    // -------------------- SIMULATOR START -------------------- //

    if ((raceManagerPID = fork()) == 0)
    {
        // printf("[%ld] Race Manager process created\n", (long)getpid());
        write_logfile("SIMULATOR STARTING");
        raceManager(n_teams);
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
        malfunctionManager();
    }
    else if (malfunctionManagerPID == -1)
    {
        // perror("Error creating Malfunction Manager process\n");
        write_logfile("ERROR CREATING MALFUNCTION MANAGER PROCESS");
        exit(1);
    }

    // -------------------- SIMULATOR END -------------------- //

    waitpid(raceManagerPID, 0, 0);
    waitpid(malfunctionManagerPID, 0, 0);

    write_logfile("SIMULATOR CLOSING");

    sem_close(writing);
    sem_unlink("WRITING");

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
