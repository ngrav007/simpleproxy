/*
 *     checkerr.c
 *     by Nickolas Gravel 02/2021
 *
 *      Includes function implementations defined in checkerr.h. Functions
 *      provide error checking for opening a file, and allocating memory.
 */
#include "checkerr.h"


FILE* open_or_die(const char *filename, const char *flag)
{
        FILE *fp = fopen(filename, flag);
        if (fp == NULL) {
                fprintf(stderr, "Error: failed to open %s\n", filename);
                exit(EXIT_FAILURE);
        }

        return fp;
}


void check_mem_err(void *data)
{
        if (data == NULL) {
                fprintf(stderr, "Error: memory allocation failed\n");
                exit(EXIT_FAILURE);
        }
}
