#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdbool.h>

typedef unsigned int uint;

typedef struct String {
    const char    *data;
    size_t        len; 
} String;

bool stringEq (String a, String b) {
    if(a.len != b.len) {return false;}
    return strncmp(a.data, b.data, a.len) == 0;   
}

String mkString (const char *data) {
    return (String) {
        .len = strlen(data),
        .data = data
    };
}

#define constString(STRING) \
    ((String){.data = STRING, .len = sizeof(STRING)-1})


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
typedef struct List List;
struct Symbol {
    String  word;
    enum { STRING, FUNCTION, ARRAY, SOURCE, LIST, ITSELF } type;
    union {
        String      string;
        void        (*function) (List **stack, List **variables);
        StringArray array;
        Source      source;
        List        *list;
    } value;
};
ArrayOf(Symbol)

StringArray mkStringArray(size_t size, const char **vals) {
    StringArray ans = (StringArray) {
        .len = size,
        .data = malloc(size * sizeof(String))
    };

    for(uint i = 0; i < size; i++) {
        ans.data[i] = (String) {
            .len = strlen(vals[i]),
            .data = vals[i] 
        };
    }

    return ans;
}

void printStringArray(String name, StringArray arr) {
    printf("ARRAY %.*s: ", (int)name.len, name.data);
    for(uint i = 0; i < arr.len; i++) {
        String s = arr.data[i];
        printf("%.*s ", (int)s.len, s.data);
    }
    printf("\n");
}

void builtin_load(List **stack, List **variables);

typedef struct List {
    Symbol      val;
    struct List *next;
} List;

List *cons(Symbol value, List *before) {
    List *ans = malloc(sizeof(List));
    *ans = (List) {
        .val = value,
        .next = before
    };
    return ans;
}

void freeList (List *l) {
    if(l == NULL) return;

    if(l->val.type == LIST) {
        freeList(l->val.value.list);
    }

    if(l->val.type == SOURCE) {
        close_source(l->val.value.source);
    }

    if(l->next != NULL) {
        freeList(l->next);
    }

    free(l);
}

#define Nothing (Symbol) { \
    .word = constString("nothing"), \
    .type = ITSELF \
} \

#define listSymbol(NAME, VAL) \
    ((Symbol) { \
        .word = constString(NAME), \
        .type = LIST, \
        .value.list = VAL \
    }) \

#define sourceSymbol(NAME, VAL) \
    ((Symbol) { \
        .word = constString(NAME), \
        .type = SOURCE, \
        .value.source = VAL \
    }) \

#define stringSymbol(NAME, VAL) \
    ((Symbol) { \
        .word = constString(NAME), \
        .type = STRING, \
        .value.string = VAL \
    }) \

Symbol pop(List **l) {
    if(l == NULL)
        return Nothing;

    Symbol s = (*l)->val;

    List *old = *l;
    *l = (*l)->next;
    free(old);

    return s;    
}

Symbol find(String name, List *list) {
    for(List *l = list; l != NULL; l = l->next) {
        if(stringEq(name, l->val.word))
            return l->val;
    }

    return Nothing;
}

void printSymbol (Symbol s) {
    if(s.type == ARRAY) printStringArray(s.word, s.value.array);    
    else if (s.type == STRING)
        printf("%.*s ", (int)s.value.string.len, s.value.string.data);
    else if (s.type == SOURCE)
        printf("SOURCE %.*s ", (int)s.word.len, s.word.data);
    else if (s.type == LIST) {
        printf("(");
        for(List *l = s.value.list; l != NULL; l = l->next) {
            printSymbol(l->val);
        }
        printf(")");
    }
    else
        printf ("%.*s ", (int)s.word.len, s.word.data);
}

List *initial_global_symtab (int argc, const char **argv) {
    List *ans = NULL;
    ans = cons( (Symbol) {
                    .word = constString("load"),
                    .type = FUNCTION,
                    .value.function = &builtin_load
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("args"),
                    .type = ARRAY
                }, ans);
    ans->val.value.array = mkStringArray(argc, argv);

    return ans;
}


void run_source(const char *filename, List **vars) {
    List *stack = NULL;

    Source root = load_file(filename);
    List *sources = cons(sourceSymbol(filename, root), NULL);
    *vars = cons(listSymbol("sources", sources), *vars);

    uint symbols_count = count_symbols(root);

    StringArray symbols = mk_StringArray(symbols_count);
    load_symbols (root, symbols, 0);

    for(uint i = 0; i < symbols.len; i++) {
        String current = symbols.data[i];
        Symbol val = find(current, *vars);

        if(val.type == FUNCTION) {
            val.value.function(&stack, vars);
        } else if (val.type == ARRAY) {
            stack = cons(val, stack);
        }
    }

    printSymbol((Symbol) {
        .word=constString("stack"),
        .type=LIST,
        .value.list = stack});
    printf("\n");

    freeList(stack);
}

void builtin_load (List **stack, List **variables) {
    if(stack == NULL || *stack == NULL) {
        fprintf(stderr, "ERROR: syntax error load(source)\n");
        exit(1);
    }

    Symbol s = pop(stack);
    if(s.type == ARRAY) {
        StringArray arr = s.value.array;
        for(uint i = 0; i < arr.len; i++) {
            String s = arr.data[i];
            char buff[s.len+1];
            buff[s.len] = 0;
            strncpy(buff, s.data, s.len);
            *stack = cons((Symbol){.word =s,
                                   .type = SOURCE,
                                   .value.source = load_file(buff)},
                          *stack);
        }
    } else *stack = cons(Nothing, *stack);
}

int main(int argc, const char **argv) {
    if(argc == 1) {
        printf("USAGE:\n    %s <source> [args...]\n\n",
               argv[0]
        );

        return 1;
    }

    List *globalsym = initial_global_symtab(argc-2, argv+2);
    run_source(argv[1], &globalsym);
    freeList(globalsym);

    return 0;
}
