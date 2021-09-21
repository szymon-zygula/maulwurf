#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>

#define ERR(source)\
    (perror(source),\
    fprintf(stderr, "%s:%d", __FILE__, __LINE__),\
    exit(EXIT_FAILURE))

#endif
