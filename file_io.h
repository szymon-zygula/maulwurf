#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdlib.h>

#include "index.h"

size_t read_file_signature(char* path, char* signature, size_t max_size);
void load_index_from_file(char* file_name, index_t** index);
void save_index_to_file(char* file_name, index_t* index);

#endif
