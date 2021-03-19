#include "declarations.h"

int *read_content_from_file()
{
    FILE *fptr;
    char line[LINESIZE];
    char *token = NULL;

    int *file_contents = (int *)malloc(ARRAYSIZE * sizeof(int));

    if ((fptr = fopen("config.txt", "r")) == NULL)
    {
        printf("Error! opening file");
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

    fclose(fptr);
    return file_contents;
}