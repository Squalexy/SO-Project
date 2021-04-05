/*
Projeto realizado por:
    Alexy de Almeida nº2019192123
    José Gonçalves nº2019223292
*/

#include "declarations.h"
int max_carros;

int main()
{

    // -------------------- RESET LOG FILE -------------------- //

    fclose(fopen("log.txt", "w"));

    // -------------------- READ CONTENT FROM LOG FILE -------------------- //

    int *file_contents = read_content_from_file();

    //int time_units = file_contents[0];
    //int turn_distance = file_contents[1];
    //int turns_number = file_contents[2];
    int n_teams = file_contents[3];
    max_carros = file_contents[4];
    //int T_Avaria = file_contents[5];
    //int T_Box_min = file_contents[6];
    //int T_Box_Max = file_contents[7];
    //int fuel_capacity = file_contents[8];

    // -------------------- PRINT CONTENT FROM LOG FILE -------------------- //

    print_content_from_file(file_contents);

    // -------------------- CREATE SEMAPHORES -------------------- //

    sem_unlink("WRITING");
    writing = sem_open("WRITING", O_CREAT | O_EXCL, 0700, 1);

    // -------------------- CREATE SHARED MEMORY -------------------- //

    typedef struct mem_struct
    {
        char box_state[n_teams][LINESIZE];
    } mem_struct;

    mem_struct *mem;

    if ((shmid = shmget(IPC_PRIVATE, sizeof(mem_struct), IPC_CREAT | 0766)) < 0)
    {
        perror("shmget error!\n");
        exit(1);
    }

    if ((mem = (mem_struct *)shmat(shmid, NULL, 0)) == (mem_struct *)-1)
    {
        perror("shmat error!\n");
        exit(1);
    }

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
