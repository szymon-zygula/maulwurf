#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include "error.h"
#include "file_io.h"
#include "interactive.h"

#include "index.h"

#define STARTING_INDEX_SIZE 16

typedef struct partial_indexing_data {
    index_t index;
    filetype_t* filetypes;
    size_t filetypes_count;
    pthread_mutex_t* mx_indexing_shutdown;
} partial_indexing_data_t;

size_t get_max_signature_len(filetype_t* filetypes, size_t filetypes_count);
bool try_to_add_next_dir_entry(
    char* dir_path,
    partial_indexing_data_t* indexing_data,
    DIR* dir,
    size_t* files_buf_size,
    size_t max_signature_len
);
bool should_stop_indexing(pthread_mutex_t* mx_indexing_shutdown);
char* get_file_path(char* dir_path, char* filename);
void reallocate_files_buffer(index_t* index, size_t* files_buf_size);
void load_dir_to_index(
    char* path,
    partial_indexing_data_t* indexing_data,
    size_t* files_buf_size,
    size_t max_signature_len
);
bool add_next_file_if_matches(
    partial_indexing_data_t* indexing_data,
    char* path,
    char* name,
    size_t max_signature_len
);
bool try_to_fill_in_stat_data(
    file_t* file,
    char* path,
    filetype_t* filetypes,
    size_t filetypes_count,
    size_t max_signature_len
);
void fill_in_name_data(file_t* file, char* name);
void fill_in_path_data(file_t* file, char* path);
size_t get_regular_filetype(
    char* path,
    filetype_t* filetypes,
    size_t filetypes_count,
    size_t max_signature_len
);
bool does_match_any_signature(filetype_t* filetype, char* signature, size_t signature_len);
void swap_indices(char* index_path,
    index_t* old_index,
    index_t* new_index,
    pthread_mutex_t* mx_index
);

index_t create_index(
    char *dir_path,
    filetype_t* filetypes,
    size_t filetypes_count,
    pthread_mutex_t* mx_indexing_shutdown
) {
    size_t files_buf_size = STARTING_INDEX_SIZE;
    partial_indexing_data_t indexing_data = {
        .index = {
            .files_count = 0,
            // Buffer will grow as more files are found
            .files = malloc(files_buf_size * sizeof(file_t))
        },
        .filetypes = filetypes,
        .filetypes_count = filetypes_count,
        .mx_indexing_shutdown = mx_indexing_shutdown
    };

    size_t max_signature_len = get_max_signature_len(filetypes, filetypes_count);

    load_dir_to_index(dir_path, &indexing_data, &files_buf_size, max_signature_len);
    // Buffer is truncated to the area where files are kept
    indexing_data.index.files =
        realloc(indexing_data.index.files, indexing_data.index.files_count * sizeof(file_t));
    if(indexing_data.index.files_count != 0 && indexing_data.index.files == NULL) ERR("realloc");
    indexing_data.index.creation_time = time(NULL);
    if(indexing_data.index.creation_time == -1) ERR("time");
    return indexing_data.index;
}

// Recursively adds next directory to the index being built
void load_dir_to_index(
    char* dir_path,
    partial_indexing_data_t* indexing_data,
    size_t* files_buf_size,
    size_t max_signature_len
) {
    DIR* dir = opendir(dir_path);
    if(dir == NULL) ERR("opendir");

    while(try_to_add_next_dir_entry(
            dir_path, indexing_data, dir, files_buf_size, max_signature_len));

    if(closedir(dir)) ERR("closedir");
}

size_t get_max_signature_len(filetype_t* filetypes, size_t filetypes_count) {
    size_t max_len = 0;
    for(size_t i = 0; i < filetypes_count; ++i) {
        for(size_t j = 0; j < filetypes[i].magic_number_count; ++j) {
            magic_number_t mn = filetypes[i].magic_numbers[j];
            if(mn.signature_len > max_len) max_len = mn.signature_len;
        }
    }

    return max_len;
}

// Tries to add next directory to the index, returns true if succeds, false if there isn't
// anything left to do
bool try_to_add_next_dir_entry(
    char* dir_path,
    partial_indexing_data_t* indexing_data,
    DIR* dir,
    size_t* files_buf_size,
    size_t max_signature_len
) {
    if(should_stop_indexing(indexing_data->mx_indexing_shutdown)) return false;
    errno = 0;
    struct dirent* dir_entry = readdir(dir);
    if(errno) ERR("readdir");
    if(dir_entry == NULL) return false;

    if(strcmp("..", dir_entry->d_name) == 0 || strcmp(".", dir_entry->d_name) == 0) return true;
    char* file_path = get_file_path(dir_path, dir_entry->d_name);
    if(indexing_data->index.files_count == *files_buf_size)
        reallocate_files_buffer(&indexing_data->index, files_buf_size);

    file_t* current_file = &indexing_data->index.files[indexing_data->index.files_count];
    bool has_been_added =
        add_next_file_if_matches(indexing_data, file_path, dir_entry->d_name, max_signature_len);
    if(has_been_added && current_file->type == FILETYPE_DIRECTORY)
        load_dir_to_index(file_path, indexing_data, files_buf_size, max_signature_len);

    free(file_path);
    return true;
}

bool should_stop_indexing(pthread_mutex_t* mx_indexing_shutdown) {
    if(mx_indexing_shutdown != NULL && pthread_mutex_trylock(mx_indexing_shutdown) == 0) {
        pthread_mutex_unlock(mx_indexing_shutdown);
        return true;
    };

    return false;
}

// Joins directory path with file path
char* get_file_path(char* dir_path, char* filename) {
    size_t dir_path_len = strlen(dir_path) ;
    char* file_path = malloc(dir_path_len + strlen(filename) + 2);
    if(file_path == NULL) ERR("malloc");

    memcpy(file_path, dir_path, dir_path_len);
    file_path[dir_path_len] = '/';
    strcpy(file_path + dir_path_len + 1, filename);
    return file_path;
}

void reallocate_files_buffer(index_t* index, size_t* files_buf_size) {
    *files_buf_size *= 2;
    index->files = realloc(index->files, *files_buf_size * sizeof(file_t));
    if(index->files == NULL) ERR("realloc");
}

// Adds the next file to the index if its type is allowed
// Returns true if the file has been added, false otherwise
bool add_next_file_if_matches(
    partial_indexing_data_t* indexing_data,
    char* path,
    char* name,
    size_t max_signature_len
) {
    file_t* file = &indexing_data->index.files[indexing_data->index.files_count];
    if(!try_to_fill_in_stat_data(
        file,
        path,
        indexing_data->filetypes,
        indexing_data->filetypes_count,
        max_signature_len
    ))
        return false;
    fill_in_name_data(file, name);
    fill_in_path_data(file, path);
    indexing_data->index.files_count += 1;
    return true;
}

// Tries to fill in information about the file based on lstat.
// Returns true if succeds, false otherwise
bool try_to_fill_in_stat_data(
    file_t* file,
    char* path,
    filetype_t* filetypes,
    size_t filetypes_count,
    size_t max_signature_len
) {
    struct stat filestat;
    if(lstat(path, &filestat)) ERR("lstat");

    if(S_ISDIR(filestat.st_mode)) file->type = FILETYPE_DIRECTORY;
    else {
        if(!S_ISREG(filestat.st_mode)) return false;

        file->type = get_regular_filetype(path, filetypes, filetypes_count, max_signature_len);
        if(file->type == FILETYPE_INVALID) return false;
    }

    file->owner = filestat.st_uid;
    file->size = filestat.st_size;

    return true;
}

void fill_in_name_data(file_t* file, char* name) {
    if(strlen(name) > MAX_FILENAME_LEN) {
        fprintf(
            stderr,
            "A file with name longer than allowed (%lu) has been detected\n",
            MAX_FILENAME_LEN
        );
        memcpy(file->name, name, MAX_FILENAME_LEN);
        file->name[MAX_FILENAME_LEN] = '\0';
    }
    else {
        strcpy(file->name, name);
    }
}

void fill_in_path_data(file_t* file, char* path) {
    char* absolute_path = realpath(path, NULL);
    if(absolute_path == NULL) ERR("realpath");
    if(strlen(absolute_path) > MAX_FILEPATH_LEN) {
        fprintf(
            stderr,
            "A file with absolute path longer than allowed (%lu) has been detected\n",
            MAX_FILEPATH_LEN
        );
        absolute_path[MAX_FILEPATH_LEN] = '\0';
    }

    strcpy(file->path, absolute_path);
    free(absolute_path);
}

// Returns the index of the file type in the `filetypes` array
size_t get_regular_filetype(
    char* path,
    filetype_t* filetypes,
    size_t filetypes_count,
    size_t max_signature_len
) {
    char* signature = malloc(max_signature_len);
    if(signature == NULL) ERR("malloc");
    size_t signature_len = read_file_signature(path, signature, max_signature_len);

    size_t filetype = FILETYPE_INVALID;
    for(size_t i = 1; i < filetypes_count; ++i) {
        if(does_match_any_signature(&filetypes[i], signature, signature_len)) {
            filetype = i;
            break;
        }
    }

    free(signature);
    return filetype;
}

bool does_match_any_signature(filetype_t* filetype, char* signature, size_t signature_len) {
    for(size_t i = 0; i < filetype->magic_number_count; ++i) {
        magic_number_t mn = filetype->magic_numbers[i];
        if(signature_len < mn.signature_len) continue; 
        if(memcmp(signature, mn.signature, mn.signature_len) == 0) return true;
    }

    return false;
}

void* async_update_index(void* void_args) {
    indexing_data_t* data = void_args;
    index_t new_index = create_index(
        data->dir_path,
        data->filetypes,
        data->filetypes_count,
        &data->mx_indexing_shutdown
    );
    if(should_stop_indexing(&data->mx_indexing_shutdown)) {
        destroy_index(&new_index);
        pthread_mutex_unlock(&data->mx_indexing_process);
        return NULL;
    }
    swap_indices(data->index_path, &data->index, &new_index, &data->mx_index);
    pthread_mutex_unlock(&data->mx_indexing_process);
    printf("Indexing has been completed.\n");
    print_command_prompt();
    return NULL;
}

void swap_indices(char* index_path,
    index_t* old_index,
    index_t* new_index,
    pthread_mutex_t* mx_index
) {
    save_index_to_file(index_path, new_index);
    pthread_mutex_lock(mx_index);
    destroy_index(old_index);
    *old_index = *new_index;
    pthread_mutex_unlock(mx_index);
}

void* async_update_index_periodically(void* void_args) {
    pthread_setcancelstate(PTHREAD_CANCEL_DEFERRED, NULL);
    periodic_indexing_args_t* args = void_args;
    time_t current_time, time_difference;
    for(;;) {
        current_time = time(NULL);
        if(current_time == -1) ERR("time");
        pthread_mutex_lock(&args->indexing_data->mx_index);
        time_difference = current_time - args->indexing_data->index.creation_time;
        pthread_mutex_unlock(&args->indexing_data->mx_index);
        if(time_difference >= args->indexing_interval) {
            try_to_start_async_indexing(args->indexing_data);
            sleep(args->indexing_interval);
        }
        else sleep(args->indexing_interval - time_difference);
    }
    return NULL;
}

bool try_to_start_async_indexing(indexing_data_t* indexing_data) {
    if(!pthread_mutex_trylock(&indexing_data->mx_indexing_process)) {
        if(indexing_data->async_indexing_started) {
            if(pthread_join(indexing_data->indexing_thread_id, NULL)) ERR("pthread_join");
        }
        else
            indexing_data->async_indexing_started = true;

        if(pthread_create(
            &indexing_data->indexing_thread_id,
            NULL,
            async_update_index,
            (void*)indexing_data)
        )
            ERR("pthread");

        return true;
    };

    return false;
}

void destroy_index(index_t* index) {
    index->files_count = 0;
    free(index->files);
    index->files = NULL;
}
