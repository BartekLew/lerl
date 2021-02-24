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
    enum { STRING, INT, BUILTIN, FUNCTION, ARRAY, SOURCE, LIST, ITSELF, BOOLEAN, NOTHING, ANY } type;
    union {
        String      string;
        void        (*builtin) (List **stack, List **variables);
        StringArray array;
        Source      source;
        List        *list;
        bool        boolean;
        int         integer;
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
void builtin_defun(List **stack, List **variables);
void builtin_stash(List **stack, List **variables);
void builtin_reverse(List **stack, List **variables);
void builtin_drop (List **stack, List **vars);
void builtin_dropOne (List **stack, List **vars);
void builtin_at (List **stack, List **vars);
void builtin_eq (List **stack, List **vars);
void builtin_if (List **stack, List **vars);
void builtin_match (List **stack, List **vars);
void builtin_doCounting (List **stack, List **vars);


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

List *consInt(int val, List *list) {
    return cons((Symbol) {
                .word = constString(""),
                .type = INT,
                .value.integer = val}, list);
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
    else if (s.type == LIST || s.type == FUNCTION) {
        printf("(");
        for(List *l = s.value.list; l != NULL; l = l->next) {
            printSymbol(l->val);
        }
        printf(")");
    } else if (s.type == INT) {
        printf("%d ", s.value.integer);
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
                    .word = constString("stash"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_stash
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("reverse"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_reverse
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("?"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_if
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("doWhile"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_doWhile
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("doCounting"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_doCounting
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("string?"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_isString
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("="),
                    .type = BUILTIN,
                    .value.builtin = &builtin_eq
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("cut"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_cut
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("match"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_match
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("@"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_at
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("("),
                    .type = BUILTIN,
                    .value.builtin = &builtin_quote
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("whitespace"),
                    .type = ARRAY,
                    .value.array = arr
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("#nl"),
                    .type = STRING,
                    .value.string = constString("\n")
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("#paropn"),
                    .type = INT,
                    .value.integer = 40
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("load"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_load
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("."),
                    .type = BUILTIN,
                    .value.builtin = &builtin_content
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString(";"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_drop
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString(";1"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_dropOne
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("args"),
                    .type = ARRAY
                }, ans);
    ans->val.value.array = mkStringArray(argc, argv);

    ans = cons( (Symbol) {
                    .word = constString("fn"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_defun
                }, ans);

    return ans;
}

Symbol strToInt(String src) {
    uint i = 0;
    bool positive = true;
    int val = 0;
    if(src.data[0] == '-') {
        positive = false;
         i++;
    }

    for(; i < src.len; i++) {
        char c = src.data[i];
        if(c < '0' || c > '9') {
            return Nothing;
        }
        val = val*10 + c - '0';
    }

    return (Symbol) {
            .word = src,
            .type = INT,
            .value.integer = (positive)?val:-val
    };
}

Symbol specialSym(Symbol s) {
    Symbol sn = strToInt(s.word);
    if(sn.type == INT) {
        return sn;
    } else {
        return s;
    }
}

void eval (List *body, List **stack, List **vars) {
    for(List *cur = body; cur != NULL; cur = cur->next) {
        Symbol s = find(cur->val.word, *vars);
        if(s.type != NOTHING) {
            if(s.type == BUILTIN) {
                s.value.builtin(stack, vars);
            } else if(s.type == FUNCTION) {
                eval(s.value.list, stack, vars);
            } else {
                *stack = cons(s, *stack);
            }
        } else {
            *stack = cons(specialSym(cur->val), *stack);
        }
    }
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

        if(val.type == BUILTIN) {
            val.value.builtin(&stack, vars);
        } else if (val.type == FUNCTION) {
            eval(val.value.list, &stack, vars);
        } else if (val.type == NOTHING) {
            stack = cons(specialSym((Symbol){
                                    .word = current,
                                    .type = ITSELF}),
                         stack);
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
        if(cur == NULL) return NULL;

        if(types[i] != ANY
            && cur->val.type != types[i]
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
    } else if(s.type == LIST) {
        List *ans = NULL;
        for(List *cur = s.value.list; cur != NULL; cur=cur->next) {
            ans = cons(cur->val, ans);
            self(&ans, vars);
        }
        return (Symbol) {
                 .word=constString(""),
                 .type=LIST,
                 .value.list=ans};
    } else
        return s;
}

#define argsOrWarn(ARGS) \
    if(ARGS == NULL) { \
        fprintf(stderr, "%s: wrong argument list\n", \
                __FUNCTION__); \
        return; \
    }

bool symbolEq (Symbol a, Symbol b) {
    if(a.type != b.type) return false;
    if(a.type == INT) {
        return a.value.integer == b.value.integer;
    } else if (a.type == STRING) {
        return stringEq(a.value.string, b.value.string);
    } else {
        fprintf(stderr, "TODO: symbolEq for type %d.\n", a.type);
        return false;
    }
}

void builtin_match (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { LIST, ANY });
    if(args == NULL)
        return;

    List *rules = pop(&args).value.list;
    args->next = *stack;
    *stack = args;

    while(rules->next != NULL) {
        Symbol ref = find(rules->val.word, *vars);
        if(symbolEq(ref, (*stack)->val)) {
            *stack = cons(rules->next->val,
                          *stack);
            freeList(rules);
            return;
        }
        List *nextcur = rules->next->next;
        rules->next->next = NULL;
        freeList(rules);
        rules = nextcur;
    }

    if(rules != NULL)
        *stack = cons(rules->val, *stack);
    else
        *stack = cons(Nothing, *stack);
}

void builtin_if (List **stack, List **vars) {
    List *ifb = NULL, *elseb = NULL;
    bool which;

    List *args = getArgs(stack, 3, (int[]) { LIST, LIST, BOOLEAN });
    if(args != NULL) {
        ifb = pop(&args).value.list;
        elseb = pop(&args).value.list;
        which = pop(&args).value.boolean;
    } else {
        args = getArgs(stack, 2, (int[]) { LIST, BOOLEAN });
        argsOrWarn(args);
        ifb = pop(&args).value.list;
        which = pop(&args).value.boolean;
    }

    if(which) {
        eval(ifb, stack, vars);
    } else if(elseb != NULL) {
        eval(elseb, stack, vars);
    }
    
}

void builtin_eq (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { ANY, ANY });
    argsOrWarn(args);

    Symbol a = args->val;
    Symbol b = args->next->val;

    args->next->next = *stack;
    *stack = args->next;
    args->next = NULL;
    freeList(args);

    if(a.type != b.type) {
        *stack = consBool(false, *stack);       
    } else if (a.type == INT) {
        *stack = consBool(a.value.integer == b.value.integer,
                          *stack);
    } else {
        *stack = cons(Nothing, *stack);
    }
    
}

void builtin_at (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { INT, STRING });
    argsOrWarn(args);

    int idx = pop(&args).value.integer;
    String src = args->val.value.string;
    args->next = *stack;

    if(idx >= src.len || idx < 0) {
        *stack = cons(Nothing, args);
    } else {
        *stack = consInt((int)src.data[idx], args);
    }
}

void builtin_drop (List **stack, List **vars) {
    freeList(*stack);
    *stack = NULL;
}

void builtin_dropOne (List **stack, List **vars) {
    pop(stack);
}

void builtin_stash (List **stack, List **vars) {
    List *args = getArgs(stack, 3, (int[]){ ANY, ANY, LIST });
    if(args != NULL) {
        List *rest = args->next;
        List *acc = rest->next;

        List **acclist = &(acc->val.value.list);
        args->next = *acclist;
        *acclist = args;

        acc->next = *stack;
        rest->next = acc;
        *stack = rest;

        return;
    }
    args = getArgs(stack, 2, (int[]) { ANY, ANY });
    if(args != NULL) {
        List *rest = args->next;
        args->next = NULL;

        rest->next = consList(*stack,args);
        *stack = rest;
    }
}

void builtin_defun (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]){ITSELF, LIST});
    if(args == NULL) {
        fprintf(stderr, "wrong arguments for fn(Itself, List)\n");
        return;
    }

    Symbol sym = pop(&args);
    args->val.type = FUNCTION;
    args->val.word = sym.word;
    args->next = *vars;
    *vars = args;
}

void builtin_reverse (List **stack, List **vars) {
    List *args = getArgs(stack, 1, (int[]){LIST});
    if(args != NULL) {
        args->val.value.list = reverseList(args->val.value.list);
        args->next = *stack;
        *stack = args;
    }
}

void builtin_doCounting (List **stack, List **vars) {
    List *args = getArgs(stack, 3, (int[]){INT, INT, LIST});
    argsOrWarn(args);

    int to = pop(&args).value.integer;
    int from = pop(&args).value.integer;
    List *commands = pop(&args).value.list;

    for(int i = 0; i <= to; i++) {
        *stack = consInt(i, *stack);
        eval(commands, stack, vars);
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

    Symbol s = pop(stack);

    const char *opar = "( ", *cpar = " )";

    if(s.type == LIST) {
        List *cur = s.value.list;
        fputs(opar, stdout);
        while(cur->next != NULL) {
            builtin_content(&cur, variables);
            putc(' ', stdout);
        }
        builtin_content(&cur, variables); //(
        fputs(cpar, stdout);
    } else if(s.type == ARRAY) {
        StringArray arr = s.value.array;
        fputs(opar, stdout);
        uint i = 0;
        for(; i < arr.len-1; i++) {
            printf("%.*s ", (int)arr.data[i].len,
                            arr.data[i].data);
        }
        printf("%.*s", (int)arr.data[i].len, arr.data[i].data);
        fputs(cpar,stdout);
    } else if(s.type == SOURCE) {
        Source src = s.value.source;
        printf("%.*s", (int)src.len, src.buff);
        close_source(src);
    }
    else if(s.type == STRING) {
        String str = s.value.string;
        printf("%.*s", (int)str.len, str.data);
    } else if(s.type == ITSELF) {
        String str = s.word;
        printf("%.*s", (int)str.len, str.data);
    } else if(s.type == INT) {
        int n = s.value.integer;
        printf("%d", n);
    } else if(s.type == BOOLEAN) {
        bool b = s.value.boolean;
        printf("%s", b?"true":"false");
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
    *stack = cons((Symbol) {
                .word = constString(""),
                .type = NOTHING
            }, *stack);
    pushStr(stack, srcstr);
}

List *varstash = NULL;
List *stackstash = NULL;
uint quot_depth = 0;

void builtin_isString(List **stack, List **vars) {
    if(*stack == NULL) {
        consBool(false, *stack);
        return;
    }
    int type = (*stack)->val.type;
    if(type == NOTHING) {
        pop(stack);
        *stack = consBool(false, *stack);
    } else if (type == STRING) {
        *stack = consBool(true, *stack);
    } else {
        *stack = consBool(false, *stack);
    }
}

void builtin_unquote (List **stack, List **vars) {
    if(--quot_depth == 0) {
        *vars = varstash;
        *stack = consList(stackstash, reverseList(*stack));
    } else {
        *stack = cons((Symbol) {
  /*(*/         .word = constString(")"), 
                .type = ITSELF},
                *stack);
    }
}

void builtin_nested_quote (List **stack, List **vars) {
    quot_depth++;
    *stack = cons((Symbol) {
                .word = constString("("), //)
                .type = ITSELF},
                *stack);
}

void builtin_quote (List** stack, List **vars) {
    varstash = *vars; 
    stackstash = *stack;
    quot_depth++;

    *stack = NULL;
    *vars = cons((Symbol) {
/*(*/             .word = constString(")"),
                  .type = BUILTIN,
                  .value.builtin = &builtin_unquote},
                cons((Symbol) {
                     .word = constString("("), //)
                     .type = BUILTIN,
                     .value.builtin = &builtin_nested_quote},
                     NULL));
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
