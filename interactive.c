#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "index.h"
#include "error.h"
#include "commands.h"

#include "interactive.h"

// Should include space for final \n character
#define MAX_COMMAND_LEN 64
// MAX_COMMAND_LEN + additional character to check if too many character have not been loaded + NUL
#define COMMAND_BUFFER_LEN MAX_COMMAND_LEN + 2

void launch_interactive_console(indexing_data_t* data);
void invalid_command();
command_result_t* execute_command(
    char* command_str,
    command_t* commands,
    size_t command_count,
    indexing_data_t* data
);
command_t* get_matching_command(char* command, command_t* commands, size_t command_count);
command_result_t* parse_and_call_command(char* command_str, command_t* command, indexing_data_t* data);

void launch_interactive_console(indexing_data_t* data) {
    char command_buf[COMMAND_BUFFER_LEN];
    command_t* commands = NULL;
    size_t command_count = get_available_commands(&commands);

    for(;;) {
        print_command_prompt();
        fgets(command_buf, COMMAND_BUFFER_LEN, stdin);
        if(strlen(command_buf) == 1) continue;
        if(strlen(command_buf) > MAX_COMMAND_LEN) {
            invalid_command();
            continue;
        }

        // Delete final \n - the string now ends in \0\0, this is going to be used
        // when checking if the command is called with any arguments
        command_buf[strlen(command_buf) - 1] = '\0';

        command_result_t* result = execute_command(command_buf, commands, command_count, data);
        if(result != NULL) {
            if(result->should_close) break;
        }
    }
}

void invalid_command() {
    fprintf(stderr, "Invalid command!\n");
}

command_result_t* execute_command(
    char* command_str,
    command_t* commands,
    size_t command_count,
    indexing_data_t* data
) {
    command_t* matching_command = get_matching_command(command_str, commands, command_count);
    if(matching_command == NULL) {
        invalid_command();
        return NULL;
    }

    size_t args_start = strlen(matching_command->name) + 1;
    // Here the fact that the command ends in \0\0 is used.
    // If the condition is true, the command has been called without arguments
    return matching_command->handler(
        command_str[args_start] == '\0' ? NULL : command_str + args_start, data);
}

command_t* get_matching_command(char* command, command_t* commands, size_t command_count) {
    for(size_t i = 0; i < command_count; ++i) {
        size_t cmd_len = strlen(commands[i].name);
        if(strlen(command) < cmd_len) continue;
        if(memcmp(commands[i].name, command, cmd_len) != 0) continue;
        if(command[cmd_len] == '\0' || command[cmd_len] == ' ') return &commands[i];
    }

    return NULL;
}

void print_command_prompt() {
    printf("> ");
    fflush(stdout);
}
