// compilar: gcc -c -Wall main.c -o main

#include "declarations.h"

int main()
{

    int *file_contents = read_content_from_file();

    /*
    int uni_tempo_segundo = file_contents[0];
    int dist_volta = file_contents[1];
    int n_voltas = file_contents[2];
    int n_equipas = file_contents[3];
    int T_Avaria = file_contents[4];
    int T_Box_min = file_contents[5];
    int T_Box_Max = file_contents[6];
    int capacidade_combustivel = file_contents[7];
*/
    for (int i = 0; i < ARRAYSIZE; ++i)
    {
        printf("%d\n", file_contents[i]);
    }

    return 0;
}