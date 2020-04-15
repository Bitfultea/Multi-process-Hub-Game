#include <stdio.h>

#define INITIAL_BUFFER 80

/* Read a line from a given pointer and write it into a buffer
 * Reads until it reaches a newline or EOF, but does not add to buffer
 * The buffer is malloced inside the function and needs to be freed
 *
 * @param file - file pointer to read from
 * @param buffer - pointer to store buffer in
 * @return length of read string
 *
 */
int read_line(FILE* file, char** buffer);
