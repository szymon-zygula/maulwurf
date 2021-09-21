#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "error.h"

#include "commands.h"
#include "file_io.h"

typedef bool (*filter_t) (file_t* file, void* data);

command_result_t* cmd_exit(char* args, indexing_data_t* data);
void stop_indexing(indexing_data_t* data);
command_result_t* cmd_exit_exclam(char* args, indexing_data_t* data);
command_result_t* cmd_index(char* args, indexing_data_t* data);
command_result_t* cmd_count(char* args, indexing_data_t* data);
size_t* count_filetypes(indexing_data_t* data);
command_result_t* cmd_largerthan(char* args, indexing_data_t* data);
bool largerthan_filter(file_t* file, void* min_size);
command_result_t* cmd_namepart(char* args, indexing_data_t* data);
bool namepart_filter(file_t* file, void* namepart);
command_result_t* cmd_owner(char* args, indexing_data_t* data);
bool owner_filter(file_t* file, void* uid);
bool ensure_args_absent(char* args, char* cmd_name);
bool ensure_args_present(char* args, char* cmd_name);
void filter_and_print_files(indexing_data_t* data, filter_t filter, void* filter_data);
size_t filter_files(
    index_t* index,
    filter_t filter,
    void* filter_data,
    bool* should_be_displayed
);
FILE* open_fileprinting_stream(size_t items);
void close_filepriting_stream(FILE* stream);
void print_files(indexing_data_t* data, bool* should_be_displayed, FILE* stream);
void print_file(file_t* file, filetype_t* filetypes, FILE* stream);

size_t get_available_commands(command_t** commands) {
    static command_t st_commands[] = {
        { "exit", cmd_exit },
        { "exit!", cmd_exit_exclam },
        { "index", cmd_index },
        { "count", cmd_count },
        { "largerthan", cmd_largerthan },
        { "namepart", cmd_namepart },
        { "owner", cmd_owner }
    };

    *commands = st_commands;
    return sizeof(st_commands) / sizeof(command_t);
}

command_result_t* cmd_exit(char* args, indexing_data_t* data
) {
    if(!ensure_args_absent(args, "exit")) return NULL;
    stop_indexing(data);

    // Unblocked, so that pthread_mutex_destroy can be safely called during resource freeing
    pthread_mutex_unlock(&data->mx_indexing_shutdown);

    static command_result_t result = { .should_close = true };
    return &result;
}

void stop_indexing(indexing_data_t* data) {
    pthread_mutex_lock(&data->mx_indexing_process);

    if(data->async_indexing_started)
        if(pthread_join(data->indexing_thread_id, NULL)) ERR("pthread_join");
}

command_result_t* cmd_exit_exclam(char* args, indexing_data_t* data
) {
    if(!ensure_args_absent(args, "exit!")) return NULL;
    pthread_mutex_unlock(&data->mx_indexing_shutdown);
    stop_indexing(data);

    static command_result_t result = { .should_close = true };
    return &result;
}

command_result_t* cmd_index(char* args, indexing_data_t* data) {
    if(!ensure_args_absent(args, "index")) return NULL;
    if(!try_to_start_async_indexing(data)) {
        fprintf(stderr, "Another indexing process is already running!\n");
    }

    return NULL;
}

command_result_t* cmd_count(char* args, indexing_data_t* data) {
    if(!ensure_args_absent(args, "count")) return NULL;
    size_t* counts = count_filetypes(data);

    printf("File type \t\t\t File count\n");
    for(size_t i = 0; i < data->filetypes_count; ++i)
        printf("%s \t\t\t %lu\n", data->filetypes[i].name, counts[i]);

    free(counts);
    return NULL;
}

size_t* count_filetypes(indexing_data_t* data) {
    size_t* counts = calloc(data->filetypes_count, sizeof(size_t));
    if(counts == NULL) ERR("calloc");

    for(size_t i = 0; i < data->index.files_count; ++i)
        counts[data->index.files[i].type] += 1;

    return counts;
}

command_result_t* cmd_largerthan(char* args, indexing_data_t* data) {
    if(!ensure_args_present(args, "largerthan")) return NULL;
    off_t min_size = atoi(args);
    filter_and_print_files(data, largerthan_filter, &min_size);
    return NULL;
}

bool largerthan_filter(file_t* file, void* min_size) {
    return file->size > *(off_t*)min_size;
}

command_result_t* cmd_namepart(char* args, indexing_data_t* data) {
    if(!ensure_args_present(args, "namepart")) return NULL;
    filter_and_print_files(data, namepart_filter, args);
    return NULL;
}

bool namepart_filter(file_t* file, void* namepart) {
    return strstr(file->name, namepart) != NULL;
}

command_result_t* cmd_owner(char* args, indexing_data_t* data) {
    if(!ensure_args_present(args, "owner")) return NULL;
    uid_t uid = atoi(args);
    filter_and_print_files(data, owner_filter, &uid);
    return NULL;
}

bool owner_filter(file_t* file, void* uid) {
    return file->owner == *(uid_t*)uid;
}

bool ensure_args_present(char* args, char* cmd_name) {
    if(args == NULL) {
        fprintf(stderr,"Command `%s` takes an argument!\n", cmd_name);
        return false;
    }

    return true;
}

bool ensure_args_absent(char* args, char* cmd_name) {
    if(args != NULL) {
        fprintf(stderr,"Command `%s` does not take any arguments!\n", cmd_name);
        return false;
    }

    return true;
}

void filter_and_print_files(indexing_data_t* data, filter_t filter, void* filter_data) {
    bool* should_be_displayed = malloc(data->index.files_count * sizeof(bool));
    size_t items = filter_files(&data->index, filter, filter_data, should_be_displayed);

    FILE* stream = open_fileprinting_stream(items);
    print_files(data, should_be_displayed, stream);
    close_filepriting_stream(stream);
    free(should_be_displayed);
}

size_t filter_files(
    index_t* index,
    filter_t filter,
    void* filter_data,
    bool* should_be_displayed
) {
    size_t items = 0;
    for(size_t i = 0; i < index->files_count; ++i) {
        should_be_displayed[i] = filter(&index->files[i], filter_data);
        items += should_be_displayed[i];
    }

    return items;
}

FILE* open_fileprinting_stream(size_t items) {
    FILE* stream = stdout;
    if(items > 3 && getenv("PAGER") != NULL)
        stream = popen(getenv("PAGER"), "w");
    return stream;
}

void close_filepriting_stream(FILE* stream) {
    if(stream != stdout)
        pclose(stream);
}

void print_files(indexing_data_t* data, bool* should_be_displayed, FILE* stream) {
    for(size_t i = 0; i < data->index.files_count; ++i)
        if(should_be_displayed[i]) print_file(&data->index.files[i], data->filetypes, stream); 
}

void print_file(file_t* file, filetype_t* filetypes, FILE* stream) {
    fprintf(stream, "Path: %s\n", file->path);
    fprintf(stream, "Size: %lu\n", file->size);
    fprintf(stream, "Type: %s\n", filetypes[file->type].name);
}
