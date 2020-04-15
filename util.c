#include <stdlib.h>
#include <stdio.h>

#include "util.h"

/* Read a line from a given pointer and write it into a buffer
 * Reads until it reaches a newline or EOF, but does not add to buffer
 * The buffer is malloced inside the function and needs to be freed
 *
 * @param file - file pointer to read from
 * @param buffer - pointer to store buffer in
 * @return length of read string
 *
 */
int read_line(FILE* file, char** buffer) {

    // Preallocate buffer
    *buffer = malloc(sizeof(char) * INITIAL_BUFFER);
    int size = INITIAL_BUFFER; // Maximum number of chars that can be stored
    int count = 0; // Number of characters that have been read

    int c = fgetc(file);
    while (c != EOF && c != '\n') {

        (*buffer)[count++] = c;

        // Check for buffer overflow; increase in size if necessary
        if (count == size - 1) {
            // Double buffer each time it fills up
            size = size * 2;
            *buffer = realloc(*buffer, sizeof(char) * size);
        }

        c = fgetc(file);
    }

    // Ensure buffer is null terminated
    (*buffer)[count] = 0;

    return count;

}
