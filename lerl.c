#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

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

typedef struct Symbol Symbol;
typedef struct SymbolArray SymbolArray;
struct Symbol {
    String  word;
    enum { STRING, FUNCTION } type;
    union {
        String string;
        void   (*function) (StringArray before, StringArray after,
                            SymbolArray variables);
    } value;
};
ArrayOf(Symbol)

void fun_load (StringArray before, StringArray after,
               SymbolArray variables) {
    printf("before: ");
    for(uint i = 0; i < before.len; i++) {
        String *s = &(before.data[i]);
        printf ("%.*s ", (int)s->len, s->data);
    }
    printf("\n");

    printf("after: ");
    for(uint i = 0; i < after.len; i++) {
        String *s = &(after.data[i]);
        printf ("%.*s ", (int)s->len, s->data);
    }
    printf("\n");
}

SymbolArray initial_global_symtab (void) {
    static Symbol symbols[] = {
        { .word = {.len = 4, .data = "load"},
          .type = FUNCTION,
          .value.function = &fun_load }
    };
    return (SymbolArray) {
        .len = sizeof(symbols)/sizeof(Symbol),
        .data = symbols
    };
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

    SymbolArray globalsym = initial_global_symtab();
    for(uint i = 0; i < symbols.len; i++) {
        String current = symbols.data[i];
        for(uint j = 0; j < globalsym.len; j++) {
            Symbol global = globalsym.data[j];
            if (global.word.len == current.len
                && strncmp(global.word.data, current.data,
                           current.len) == 0) {
                if(global.type == FUNCTION) {
                    global.value.function((StringArray) {
                                        .len = i,
                                        .data = symbols.data
                                    },
                                    (StringArray) {
                                        .len = symbols.len - i -1,
                                        .data = symbols.data + i + 1
                                    },
                                    globalsym
                    );
                } else {
                    printf("Unknown symbol: %.*s\n", (int)current.len, current.data);
                }
            }
        }
    }

    printf("\n***\n");

    for(uint i = 0; i < sources.len; i++) {
        close_source(sources.data[i]);
    }
    free_SourceArray(sources);

    return 0;
}
