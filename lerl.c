#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

typedef unsigned int uint;

typedef struct String {
    const char    *data;
    size_t        len; 
} String;

typedef struct Source {
    const char   *name;
    const char   *buff;
    size_t       len;
    int          fd;
} Source;

#define ArrayOf(TYPE) \
    typedef struct TYPE ## Array { \
        TYPE    *data; \
        size_t  len; \
    } TYPE ## Array; \
    \
    TYPE ## Array \
    mk_ ## TYPE ## Array (uint len) { \
        return (TYPE ## Array) { \
            .data = malloc (sizeof(TYPE) * len), \
            .len = len \
        }; \
    } \
    \
    void free_ ## TYPE ## Array (TYPE ## Array tgt) {\
        free(tgt.data); \
    }

ArrayOf(Source)
ArrayOf(String)

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

uint count_symbols (Source src) {
    uint n = 0;
    uint mod = 0;
    for(uint i = 0; i < src.len; i++) {
        if(src.buff[i] == ' '
        || src.buff[i] == '\t'
        || src.buff[i] == '\n') {
            if(mod == 1)
                mod = 0;
        } else {
            if(mod == 0) {
                n++;
                mod = 1;
            }
        }
    }

    return n;
}

off_t load_symbols (Source src, StringArray arr, off_t offset) {
    uint mod = 1;
    uint symstart = 0;
    for (uint i = 0; i < src.len; i++) {
        if(src.buff[i] == ' '
        || src.buff[i] == '\t'
        || src.buff[i] == '\n') {
            if(mod == 1) {
                mod = 0;
                arr.data[offset++] = (String) {
                    .data =  src.buff + symstart,
                    .len = i - symstart
                };
            }
        } else {
            if(mod == 0) {
                symstart = i;
                mod = 1;
            }
        }
    }

    if(mod == 1) {
        arr.data[offset++] = (String) {
            .data = src.buff + symstart,
            .len = src.len - symstart
        };
    }

    return offset;
}

int main(int argc, char **argv) {
    SourceArray sources = mk_SourceArray(argc - 1);
    uint symbols_count = 0;
    for(uint i = 0; i < sources.len; i++) {
        sources.data[i] = load_file(argv[i+1]);
        symbols_count += count_symbols(sources.data[i]);
    }      

    StringArray symbols = mk_StringArray(symbols_count);
    off_t symarr_off = 0;
    for(uint i = 0; i < sources.len; i++) {
        symarr_off = load_symbols (
            sources.data[i], symbols, symarr_off
        );
    }

    printf ("%u symbols: ", symbols_count);
    for (uint i = 0; i < symbols_count; i++) {
        printf("%.*s ",  symbols.data[i].len,
                         symbols.data[i].data);
    }

    printf("\n***\n");

    for(uint i = 0; i < sources.len; i++) {
        printf("source: %s (size=%lu/%u symbols)\n\n%s\n***\n\n",
               sources.data[i].name,
               sources.data[i].len,
               count_symbols(sources.data[i]),
               sources.data[i].buff);
        close_source(sources.data[i]);
    }
    free_SourceArray(sources);

    return 0;
}
