#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "error.h"

#include "file_io.h"

typedef ssize_t (*file_operator_t) (int file_descriptor, void* buffer, size_t bytes_left);

ssize_t bulk_read(int file_descriptor, char *buffer, size_t bytes_left);
ssize_t bulk_write(int file_descriptor, char *buffer, size_t bytes_left);
ssize_t generic_bulk_file_operation(
    int file_descriptor,
    char* buffer,
    size_t bytes_left,
    file_operator_t operator
);
void set_index_creation_time(char* file_name, index_t* index);

size_t read_file_signature(char* path, char* signature, size_t max_size) {
    int file_desc = open(path, O_RDONLY);
    if(file_desc < 0) ERR("open");
    ssize_t read_size = bulk_read(file_desc, signature, max_size);
    if(read_size < 0) ERR("read");
    if(close(file_desc)) ERR("close");
    return read_size;
}

void load_index_from_file(char* file_name, index_t** index) {
    errno = 0;
    int file_desc = open(file_name, O_RDONLY);
    if(file_desc < 0) {
        if(errno == ENOENT) {
            *index = NULL;
            return;
        }

        ERR("open");
    }

    bulk_read(file_desc, (char*)&(*index)->files_count, sizeof((*index)->files_count));
    (*index)->files = malloc(sizeof(file_t) * (*index)->files_count);
    bulk_read(file_desc, (char*)(*index)->files, sizeof(file_t) * (*index)->files_count);
    set_index_creation_time(file_name, *index);
    if(close(file_desc)) ERR("close");
}

void save_index_to_file(char* file_name, index_t* index) {
    int file_desc = open(file_name, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);
    if(file_desc < 0) ERR("open");
    bulk_write(file_desc, (char*)&index->files_count, sizeof(index->files_count));
    bulk_write(file_desc, (char*)index->files, sizeof(index->files[0]) * index->files_count);
    if(close(file_desc)) ERR("close");
}

ssize_t bulk_read(int file_descriptor, char *buffer, size_t bytes_left) {
    return generic_bulk_file_operation(
            file_descriptor, buffer, bytes_left, read);
}

ssize_t bulk_write(int file_descriptor, char *buffer, size_t bytes_left) {
    return generic_bulk_file_operation(
            file_descriptor, buffer, bytes_left, (file_operator_t)write);
}

ssize_t generic_bulk_file_operation(
    int file_descriptor,
    char* buffer,
    size_t bytes_left,
    file_operator_t operator
) {
    ssize_t c;
    ssize_t bytes_affected = 0;
    while(bytes_left > 0) {
        c = TEMP_FAILURE_RETRY(operator(file_descriptor, buffer, bytes_left));
        if(c < 0) return c;
        if(c == 0) return bytes_affected; // Nothing left for loading
        buffer += c;
        bytes_affected += c;
        bytes_left -= c;
    }

    return bytes_affected;
}

void set_index_creation_time(char* file_name, index_t* index) {
    struct stat filestat;
    if(lstat(file_name, &filestat)) ERR("lstat");
    index->creation_time = filestat.st_mtime;
}
