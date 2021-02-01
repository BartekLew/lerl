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
    enum { STRING, FUNCTION, ARRAY, SOURCE, LIST, ITSELF, BOOLEAN, NOTHING } type;
    union {
        String      string;
        void        (*function) (List **stack, List **variables);
        StringArray array;
        Source      source;
        List        *list;
        bool        boolean;
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
void builtin_content(List **stack, List **variables);
void builtin_cut(List **stack, List **variables);
void builtin_quote(List **stack, List **variables);
void builtin_isString(List **stack, List **variables);
void builtin_doWhile(List **stack, List **variables);

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
    .type = NOTHING \
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

void pushStr(List **l, String s) {
    *l = cons((Symbol) {
                .word = s,
                .type = STRING,
                .value.string = s }, *l);
}

List *consList(List *into, List *list) {
    return cons((Symbol) {
                    .word = constString(""),
                    .type = LIST,
                    .value.list = list }, into);
}

List *consBool(bool val, List *list) {
    String name;
    if(val) name = constString("true");
    else name = constString("false");

    return cons((Symbol) {
                .word = name,
                .type = BOOLEAN,
                .value.boolean = val
           }, list);
}

List *joinLists(List *a, List *b) {
    List *start = a;
    while(a->next != NULL) {a = a->next;}
    a->next = b;
    return start;
}
    
List *reverseList(List *tgt) {
    List *last = NULL;
    while(tgt != NULL) {
        List *next = tgt->next;
        tgt->next = last;
        last = tgt;
        tgt = next;
    }

    return last;
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
    else if (s.type == ITSELF)
        printf("%.*s ", (int)s.word.len, s.word.data);
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

    StringArray arr = mk_StringArray(3);
    arr.data[0] = constString(" ");
    arr.data[1] = constString("\n");
    arr.data[2] = constString("\t");

    ans = cons( (Symbol) {
                    .word = constString("doWhile"),
                    .type = FUNCTION,
                    .value.function = &builtin_doWhile
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("string?"),
                    .type = FUNCTION,
                    .value.function = &builtin_isString
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("cut"),
                    .type = FUNCTION,
                    .value.function = &builtin_cut
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("("),
                    .type = FUNCTION,
                    .value.function = &builtin_quote
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("whitespace"),
                    .type = ARRAY,
                    .value.array = arr
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("load"),
                    .type = FUNCTION,
                    .value.function = &builtin_load
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("."),
                    .type = FUNCTION,
                    .value.function = &builtin_content
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
        } else if (val.type == NOTHING) {
            stack = cons((Symbol) {
                            .word = current,
                            .type = ITSELF
                    }, stack);
        } else {
            stack = cons(val, stack);
        }
    }

    if(stack != NULL) {
        printf("\n");
        printSymbol((Symbol) {
            .word=constString("stack"),
            .type=LIST,
            .value.list = stack});
        printf("\n");
    }

    freeList(stack);
}

void verifyArg(List **stack, const char *name) {
    if(stack == NULL || *stack == NULL) {
        fprintf(stderr, "ERROR: syntax error %s\n", name);
        exit(1);
    }
}

typedef struct Converter {
    int srcType, dstType;
    Symbol (*act)(Symbol src, List **sidestack);
} Converter;

Symbol conv_Source_String(Symbol src, List **sidestack) {
    Source val = src.value.source;
    String str = (String){.data = val.buff, .len = val.len};

    *sidestack = cons(src, *sidestack);

    return (Symbol) {
        .word = src.word,
        .type = STRING,
        .value.string = str
    };
}

Converter converters[] = (Converter[]) {
    {.srcType = SOURCE,
     .dstType = STRING,
     .act = &conv_Source_String}
};
        
Converter *findConverter(int from, int to) {
    for(uint i = 0;
        i < sizeof(converters)/sizeof(Converter);
        i++) {
        if(from == converters[i].srcType
           && to == converters[i].dstType){
            return converters+i;
        }
    }

    return NULL;
}

List *getArgs(List **stack, uint count, int types[]) {
    if(stack == NULL || *stack == NULL) {
        return NULL;
    }

    Converter *convs[count];
    for(uint i = 0; i < count; i++) {
        convs[i] = 0;
    }

    List *cur = *stack;
    for(uint i = 0; i < count; i++) {
        if(cur->val.type != types[i]
            && (convs[i] = findConverter(cur->val.type, types[i])) == NULL) {
            return NULL;
        }
        cur = cur->next;
    }

    List *sidestack = NULL;
    if(convs[0] != NULL) {
        (*stack)->val = convs[0]->act((*stack)->val, &sidestack);
    }

    cur = *stack;
    for(uint i = 1; i < count; i++) {
        cur = cur->next;
        if(convs[i] != NULL) {
            cur->val = convs[i]->act(cur->val, &sidestack);
        }
    }

    List *args = *stack;
    *stack = cur->next;

    if(sidestack != NULL) {
        List *cur = sidestack;
        for(; cur->next != NULL; cur = cur->next);
        cur->next = *stack;
        *stack = sidestack;
    }

    return args;
}

Symbol implicitMap(List **stack, List **vars,
                   void (*self) (List**, List**)) {
    Symbol s = pop(stack); 
    if(s.type == ARRAY) {
        List *ans = NULL;
        StringArray arr = s.value.array;
        for(uint i = 0; i < arr.len; i++) {
            Symbol sym = (Symbol) {
                            .word = arr.data[i],
                            .type = ITSELF
                         };
            ans = cons(sym, ans);
            self(&ans, vars);
        }
        return (Symbol) {
                 .word=constString(""),
                 .type=LIST,
                 .value.list=ans};
    } else
        return s;
}

void eval (List *body, List **stack, List **vars) {
    for(List *cur = body; cur != NULL; cur = cur->next) {
        Symbol s = find(cur->val.word, *vars);
        if(s.type != NOTHING) {
            if(s.type == FUNCTION) {
                s.value.function(stack, vars);
            } else {
                *stack = cons(s, *stack);
            }
        } else {
            *stack = cons(cur->val, *stack);
        }
    }
}

void builtin_doWhile (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]){LIST, LIST});
    if(args == NULL) {
        fprintf(stderr, "wrong arguments for doWhile(List, List)\n");
        return;
    }

    List *condition = pop(&args).value.list;
    List *body = pop(&args).value.list;
    List *ans;

    do {
        eval(body, stack, vars);
        eval(condition, stack, vars);
        ans = getArgs(stack, 1, (int[]){BOOLEAN});
        if(ans == NULL) {
            fprintf(stderr, "Wrong condition for doWhile()\n");
            printSymbol((*stack)->val);
            return;
        }
    } while (ans->val.value.boolean);
}

void builtin_content (List **stack, List **variables) {
    verifyArg(stack, ".");

    Symbol s = implicitMap(stack, variables,
                           &builtin_content);
    if(s.type == SOURCE) {
        Source src = s.value.source;
        printf("%.*s", (int)src.len, src.buff);
        close_source(src);
    }
    else if(s.type == STRING) {
        String str = s.value.string;
        printf("%.*s\n", (int)str.len, str.data);
    } else if(s.type == ITSELF) {
        String str = s.word;
        printf("%.*s\n", (int)str.len, str.data);
    }
}

void builtin_load (List **stack, List **variables) {
    verifyArg(stack, "load()");

    Symbol s = implicitMap(stack, variables, &builtin_load);
    if(s.type == LIST) {
        *stack = cons(s, *stack);
    } else if(s.type == ITSELF) {
        String str = s.word;
        char buff[str.len+1];
        buff[str.len] = 0;
        strncpy(buff, str.data, str.len);
        *stack = cons((Symbol){.word =str,
                            .type = SOURCE,
                            .value.source = load_file(buff)},
                    *stack);
    } else *stack = cons(Nothing, *stack);
}

void builtin_cut (List **stack, List **vars) {
    String      srcstr;
    StringArray seps;

    List *args = getArgs(stack, 2, (int[]){ ARRAY, STRING });
    if(args == NULL) {
        fprintf(stderr, "wrong args for cut().\n");
        return;
    }
    seps = pop(&args).value.array;
    srcstr = pop(&args).value.string;

    for(uint i = 0; i < srcstr.len; i++) {
        for(uint j = 0; j < seps.len; j++) {
            String sep = seps.data[j];
            if(sep.len > srcstr.len - i) continue;
            if(strncmp(sep.data, srcstr.data+i, sep.len) == 0) {
                String str = {.data = srcstr.data+i+sep.len,
                              .len = srcstr.len - i - sep.len };
                pushStr(stack, str);
                String s = {.data = srcstr.data,
                            .len = i};
                pushStr(stack, s);
                return;
            }
        } 
    }
}

List *varstash = NULL;
List *stackstash = NULL;

void builtin_isString(List **stack, List **vars) {
    *stack = consBool(*stack != NULL
                          && (*stack)->val.type == STRING,
                      *stack);
}

void builtin_unquote (List **stack, List **vars) {
    *vars = varstash;
    *stack = consList(stackstash, reverseList(*stack));
}

void builtin_quote (List** stack, List **vars) {
    varstash = *vars; 
    stackstash = *stack;

    *stack = NULL;
    *vars = cons((Symbol) {
/*(*/             .word = constString(")"),
                  .type = FUNCTION,
                  .value.function = &builtin_unquote},
                NULL);
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
