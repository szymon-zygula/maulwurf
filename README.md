# Maulwurf
Maulwurf is a file indexer written in C.
It scans all files in a given directory and awaits queries.
The index can be saved to a file so that it is not rebuilt on every startup.
It can also be rebuilt in a separate thread while the program still accepts queries.
The index contains information about directories, JPEG files, PNG files, GZIP files and ZIP files.
File type is determined based on the magic number, not filename.
Other filetypes can be easily added in the `main` function before compilation.

## Compilation
Simply run `make` to build everything.
No dependencies except for a C compiler are required.
```
make
```

## Startup
Maulwurf can be started in the following way:
```
    maulwurf [-d indexed directory]
    [-f path to index file]
    [-t (30 =< indexing interval =< 7200)]
    If -d is omitted, MAULWURF_DIR enviroment variable has to be set.
    Then, its value is taken instead.
    If -f is omitted, the value of MAULWURF_INDEX_PATH enviroment variable is taken instead.
    If MAULWURF_INDEX_PATH is not set and -f is omitted, HOME enviroment variable has to be set.
    Then, `$HOME/.maulwurf_index` is used."
    If -t is specified, then every t seconds the index is rebuilt.

```
## Usage
The following commands are available:
- `exit` waits for any ongoing indexing to complete and exits
- `exit!` aborts any ongoing indexing and exits
- `index` starts background indexing
- `count` counts files of every filetype
- `largerthan x` prints all files larger than `x` bytes
- `namepart y` prints all files which include `y` in their name
- `owner uid` prints all files owned by a user with `uid` user id
