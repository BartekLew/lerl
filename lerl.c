#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

typedef unsigned int uint;

typedef struct String {
    char    *ptr;
    size_t  len; 
} String;

typedef struct Source {
    const char   *name;
    const char   *buff;
    size_t       len;
    int          fd;
} Source;

typedef struct SourceArray {
    Source  *data;
    size_t  len;
} SourceArray;

SourceArray srcarr (uint len) {
    return (SourceArray) {
        .data = malloc (sizeof(Source) * len),
        .len = len
    };
}

Source load_file (const char *file_name) {
    struct stat details;
    Source retval;
    if(stat(file_name, &details) != 0
       || (retval.fd = open(file_name, O_RDONLY)) < 0
       || (retval.buff = mmap(NULL, details.st_size, PROT_READ,
                              MAP_SHARED, retval.fd, 0)) == MAP_FAILED) {
        fprintf(stderr, "%s: can't open\n", file_name);
        exit(1);
    }
    
    retval.name = file_name;
    retval.len = details.st_size;
    return retval;
}

void close_source (Source src) {
    munmap((void*)src.buff, src.len);
    close(src.fd);
}
    
int main(int argc, char **argv) {
    SourceArray sources = srcarr(argc - 1);
    for(uint i = 0; i < sources.len; i++) {
        sources.data[i] = load_file(argv[i+1]);
    }      

    for(uint i = 0; i < sources.len; i++) {
        printf("source: %s (size=%lu)\n\n%s\n***\n\n",
               sources.data[i].name,
               sources.data[i].len,
               sources.data[i].buff);
        close_source(sources.data[i]);
    }
    free(sources.data);

    return 0;
}
