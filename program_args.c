#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "error.h"

#include "program_args.h"

#define DEFAULT_INDEX_FILENAME ".maulwurf_index"
#define MIN_INDEXING_INTERVAL 30
#define MAX_INDEXING_INTERVAL 7200

void parse_program_args(int argc, char** argv, program_args_t* program_args);
char* get_default_dir_path();
char* get_default_index_path(bool* should_be_freed);
char* get_fallback_index_path();
bool are_args_correct(program_args_t* program_args);
void usage(char* program_path);

void get_program_args(int argc, char** argv, program_args_t* program_args) {
    parse_program_args(argc, argv, program_args);
    if(program_args->dir_path == NULL) program_args->dir_path = get_default_dir_path(argv[0]);
    program_args->should_free_index_path = false;
    if(program_args->index_path == NULL)
        program_args->index_path =
            get_default_index_path(&program_args->should_free_index_path);

    if(!are_args_correct(program_args))
        usage(argv[0]);
}

void parse_program_args(int argc, char** argv, program_args_t* program_args) {
    program_args->indexing_interval = NO_INTERVAL_INDEXING;
    program_args->dir_path = NULL;
    program_args->index_path = NULL;
    int opt;
    while((opt = getopt(argc, argv, "d:f:t:")) != -1) {
        switch(opt) {
            case 'd':
                program_args->dir_path = optarg;
                break;
            case 'f':
                program_args->index_path = optarg;
                break;
            case 't':
                program_args->indexing_interval = atoi(optarg);
                break;
            case '?':
                usage(argv[0]);
                break;
        }
    }

    if(argc > optind) usage(argv[0]);
}

char* get_default_dir_path() {
    char* dir_env = getenv("MAULWURF_DIR");
    return dir_env;
}

char* get_default_index_path(bool* should_be_freed) {
    char* index_path = getenv("MAULWURF_INDEX_PATH");
    if(index_path != NULL) {
        should_be_freed = false;
        return index_path;
    }

    *should_be_freed = true;
    return get_fallback_index_path();
}

char* get_fallback_index_path() {
    char* home = getenv("HOME");
    if(home == NULL) return NULL;
    size_t home_len = strlen(home);
    char* index_path = malloc(home_len + 1 + sizeof(DEFAULT_INDEX_FILENAME));
    if(index_path == NULL) ERR("malloc");
    memcpy(index_path, home, home_len);
    index_path[home_len] = '/';
    strcpy(index_path + home_len + 1, DEFAULT_INDEX_FILENAME);
    return index_path;
}

bool are_args_correct(program_args_t* program_args) {
    bool is_indexing_interval_within_range =
        program_args->indexing_interval <= MAX_INDEXING_INTERVAL &&
        program_args->indexing_interval >= MIN_INDEXING_INTERVAL;

    return
        (is_indexing_interval_within_range ||
        program_args->indexing_interval == NO_INTERVAL_INDEXING) &&
        program_args->dir_path != NULL &&
        program_args->index_path != NULL;
}

void usage(char* program_path) {
    fprintf(stderr,
        "Invalid use of %s!\nUsage: "
        "%s [-d indexing directory] "
        "[-f path to index file] "
        "[-t 30 =< indexing interval =< 7200]\n"
        "If -d is omitted, MAULWURF_DIR enviroment variable has to be set."
        "Then, its value is taken instead.\n"
        "If -f is omitted, the value of MAULWURF_INDEX_PATH enviroment variable is taken instead. "
        "If MAULWURF_INDEX_PATH is not set and -f is omitted, HOME enviroment variable has to be set."
        "Then, `$HOME/.maulwurf_index` is used."
        "\n",
        program_path, program_path
    );
    exit(EXIT_FAILURE);
}
