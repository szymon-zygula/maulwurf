#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include "index.h"
#include "interactive.h"
#include "error.h"
#include "file_io.h"
#include "program_args.h"

filetype_t get_available_filetypes();
void initialize_mutexes(indexing_data_t* indexing_data);
void cleanup(
    indexing_data_t* indexing_data,
    program_args_t* program_args,
    pthread_t periodic_indexing_thread_id
);
void initialize_index(indexing_data_t* indexing_data);
pthread_t initialize_periodic_indexing_thread(
    indexing_data_t* indexing_data,
    program_args_t* program_args,
    periodic_indexing_args_t* periodic_indexing_args
);

int main(int argc, char** argv) {
    program_args_t program_args;
    get_program_args(argc, argv, &program_args);
    filetype_t filetypes[] = {
        { .name = "Directory", .magic_number_count = 0 },
        FILETYPE("JPEG Image", MAGIC_NUMBER("\xff\xd8\xff")),
        FILETYPE("PNG Image", MAGIC_NUMBER("\x89\x50\x4e\x47\x0d\x0a\x1a\x0a")),
        FILETYPE("GZIP archive", MAGIC_NUMBER("\x1f\x8b")),
        FILETYPE("ZIP archive",
                MAGIC_NUMBER("\x50\x4B\x03\x04"),
                MAGIC_NUMBER("\x50\x4B\x05\x06"),
                MAGIC_NUMBER("\x50\x4B\x07\x08"))
    };

    indexing_data_t indexing_data = {
        .filetypes = filetypes,
        .filetypes_count = sizeof(filetypes) / sizeof(filetype_t),
        .dir_path = program_args.dir_path,
        .index_path = program_args.index_path,
        .async_indexing_started = false
    };
    initialize_mutexes(&indexing_data);
    initialize_index(&indexing_data);

    periodic_indexing_args_t periodic_indexing_args;
    pthread_t periodic_indexing_thread_id =
        initialize_periodic_indexing_thread(&indexing_data, &program_args, &periodic_indexing_args);
    launch_interactive_console(&indexing_data);
    cleanup(&indexing_data, &program_args, periodic_indexing_thread_id);

    return EXIT_SUCCESS;
}

void initialize_mutexes(indexing_data_t* indexing_data) {
    // index access mutex
    if(pthread_mutex_init(&indexing_data->mx_index, NULL)) ERR("pthread_mutex_init");
    // mutex ensuring that only one index can be built at any given moment
    if(pthread_mutex_init(&indexing_data->mx_indexing_process, NULL)) ERR("pthread_mutex_init");
    // mutex forcing the end of indexing
    if(pthread_mutex_init(&indexing_data->mx_indexing_shutdown, NULL)) ERR("pthread_mutex_init");
    pthread_mutex_lock(&indexing_data->mx_indexing_shutdown);
}

void initialize_index(indexing_data_t* indexing_data) {
    index_t* index = &indexing_data->index;
    load_index_from_file(indexing_data->index_path, &index);
    if(index == NULL) {
        indexing_data->index = create_index(
            indexing_data->dir_path,
            indexing_data->filetypes,
            indexing_data->filetypes_count,
            &indexing_data->mx_indexing_shutdown
        );
        save_index_to_file(indexing_data->index_path, &indexing_data->index);
    }
}

void cleanup(
    indexing_data_t* indexing_data,
    program_args_t* program_args,
    pthread_t periodic_indexing_thread_id
) {
    if(program_args->indexing_interval != NO_INTERVAL_INDEXING) {
        pthread_cancel(periodic_indexing_thread_id);
        if(pthread_join(periodic_indexing_thread_id, NULL)) ERR("pthread_join");
    }

    pthread_mutex_destroy(&indexing_data->mx_indexing_shutdown);
    pthread_mutex_destroy(&indexing_data->mx_index);
    pthread_mutex_unlock(&indexing_data->mx_indexing_process);
    pthread_mutex_destroy(&indexing_data->mx_indexing_process);

    destroy_index(&indexing_data->index);
    if(program_args->should_free_index_path)
        free(program_args->index_path);
}

pthread_t initialize_periodic_indexing_thread(
    indexing_data_t* indexing_data,
    program_args_t* program_args,
    periodic_indexing_args_t* periodic_indexing_args
) {
    pthread_t periodic_indexing_thread_id;
    if(program_args->indexing_interval != NO_INTERVAL_INDEXING) {
        periodic_indexing_args->indexing_data = indexing_data;
        periodic_indexing_args->indexing_interval = program_args->indexing_interval;
        pthread_create(
            &periodic_indexing_thread_id,
            NULL,
            async_update_index_periodically,
            periodic_indexing_args
        );
    }

    return periodic_indexing_thread_id;
}
