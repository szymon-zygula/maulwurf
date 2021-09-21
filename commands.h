#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>

#include "index.h"

typedef struct command_return {
    bool should_close;
} command_result_t;

typedef struct command {
    char* name;
    command_result_t* (*handler) (char* args, indexing_data_t* data);
} command_t;

size_t get_available_commands(command_t** commands);

#endif
