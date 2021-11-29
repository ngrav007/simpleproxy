/*
 * checkerr.c
 *
 *     Author: Nickolas Gravel (02/2021)
 *
 *     Contains functions that complete routine tasks and provide error
 *     checking. If errors have occured function prints out descriptive error
 *     message related to the function and terminates the program.
 */
#ifndef _CHECKERR_H_
#define _CHECKERR_H_

#include <stdio.h>
#include <stdlib.h>

/*  open_or_die
 *      Purpose: Open a file and do error checking, if error occurs when opening
 *               the file, print error message to stderr and exit program.
 *   Parameters: Filename and flag that indicates what mode file should be
 *               opened in (i.e. "r" = read, "w" = write)
 *      Returns: FILE pointer to opened file
 *
 *    Exception: A Checked Runtime Error occurs if an error occurs when opening
 *               a file
 */
FILE* open_or_die(const char *filename, const char *flag);


/*  check_mem_err
 *      Purpose: Check if memory was allocated, if error is encoutered error
 *               message is printed to stderr, and program is terminated
 *   Parameters: Pointer to allocated memory
 *      Returns: None
 *
 *    Exception: A Checked Runtime Error occurs if an error occurs when
 *               allocating memory
 */
void check_mem_err(void *data);

#endif
