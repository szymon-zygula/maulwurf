#ifndef INDEX_H
#define INDEX_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct magic_number {
    char* signature;
    size_t signature_len;
} magic_number_t;

#define MAGIC_NUMBER(_signature) {\
    .signature = _signature,\
    .signature_len = sizeof(_signature) - 1\
}

#define MAX_MAGIC_NUMBERS 4LU
typedef struct filetype {
    magic_number_t magic_numbers[MAX_MAGIC_NUMBERS];
    size_t magic_number_count;
    char* name;
} filetype_t;

// This makro needs at least MAX_MAGIC_NUMBERS * (number of fields in magic_number) + 2
// arguments (not including `...`)
#define TENTH_ARG(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10
#define COUNT_MAGIC_NUMBERS(...) TENTH_ARG(__VA_ARGS__, 4, 4, 3, 3, 2, 2, 1, 1)
#define FILETYPE(_name, ...) {\
    .name = _name,\
    .magic_numbers = {__VA_ARGS__},\
    .magic_number_count = COUNT_MAGIC_NUMBERS(__VA_ARGS__)\
}

#define FILETYPE_DIRECTORY 0LU
#define FILETYPE_INVALID -1LU

#define MAX_FILENAME_LEN 256LU
#define MAX_FILEPATH_LEN 1024LU

typedef struct file {
    char name[MAX_FILENAME_LEN + 1];
    char path[MAX_FILEPATH_LEN + 1];
    off_t size;
    uid_t owner;
    size_t type;
} file_t;

typedef struct index {
    file_t* files;
    time_t creation_time;
    size_t files_count;
} index_t;

// Structure containing all data which could be necessary during index operations
typedef struct indexing_data {
    index_t index;
    filetype_t* filetypes;
    size_t filetypes_count;
    char* dir_path;
    char* index_path;
    pthread_mutex_t mx_index;
    pthread_mutex_t mx_indexing_process;
    pthread_mutex_t mx_indexing_shutdown;
    pthread_t indexing_thread_id;
    bool async_indexing_started;
} indexing_data_t;

index_t create_index(
    char *dir_path,
    filetype_t* filetypes,
    size_t filetypes_count,
    pthread_mutex_t* mx_indexing_shutdown
);

// `async_update_index_periodically` function argument
typedef struct periodic_indexing_args {
    int indexing_interval;
    indexing_data_t* indexing_data;
} periodic_indexing_args_t;

void* async_update_index(void* void_args);
void* async_update_index_periodically(void* void_args);
bool try_to_start_async_indexing(indexing_data_t* indexing_data);
void destroy_index(index_t* index);

#endif
