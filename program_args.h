#ifndef PROGRAM_ARGS_H
#define PROGRAM_ARGS_H

#include <stdbool.h>

#define NO_INTERVAL_INDEXING -1

typedef struct program_args {
    int indexing_interval;
    char* dir_path;
    char* index_path;
    bool should_free_index_path;
} program_args_t;

void get_program_args(int argc, char** argv, program_args_t* program_args);

#endif
