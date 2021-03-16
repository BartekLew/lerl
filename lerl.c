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
    if(src.fd == -1) return;
    
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
    enum { STRING, INT, CHAR, BUILTIN, FUNCTION, ARRAY, SOURCE, LIST, ITSELF, BOOLEAN, NOTHING, ANY } type;
    union {
        String      string;
        void        (*builtin) (List **stack, List **variables);
        StringArray array;
        Source      source;
        List        *list;
        bool        boolean;
        char        character;
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
void builtin_len (List **stack, List **vars);
void builtin_moveArg (List **stack, List **vars);
void builtin_assign (List **stack, List **vars);
void builtin_clone (List **stack, List **vars);
void builtin_plus (List **stack, List **vars);
void builtin_minus (List **stack, List **vars);
void builtin_lt (List **stack, List **vars);
void builtin_gt (List **stack, List **vars);
void builtin_lte (List **stack, List **vars);
void builtin_gte (List **stack, List **vars);
void builtin_and (List **stack, List **vars);
void builtin_or (List **stack, List **vars);
void builtin_substr (List **stack, List **vars);
void builtin_in (List **stack, List **vars);
void builtin_exit (List **stack, List **vars);

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

List *consChar(char val, List *list) {
    return cons((Symbol) {
                .word = constString(""),
                .type = CHAR,
                .value.character = val}, list);
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

List *consString(String str, List *tail) {
    return cons((Symbol) {
                .word = str,
                .type = STRING,
                .value.string = str }, tail);
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
        printf("( ");
        for(List *l = s.value.list; l != NULL; l = l->next) {
            printSymbol(l->val);
        }
        printf(")");
    } else if (s.type == CHAR) {
        printf("'%c' ", s.value.character);
    } else if (s.type == INT) {
        printf("%d ", s.value.integer);
    }
    else
        printf ("%.*s ", (int)s.word.len, s.word.data);
}

void printList (List *l) {
    printSymbol((Symbol) {.word = constString(""),
                          .type = LIST,
                          .value.list = l });
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
                    .word = constString("in"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_in
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
                    .word = constString("substr"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_substr
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("="),
                    .type = BUILTIN,
                    .value.builtin = &builtin_eq
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("&"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_and
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("or"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_or
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("+"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_plus
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("-"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_minus
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("<"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_lt
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString(">"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_gt
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("<="),
                    .type = BUILTIN,
                    .value.builtin = &builtin_lte
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString(">="),
                    .type = BUILTIN,
                    .value.builtin = &builtin_gte
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("exit"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_exit
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
                    .word = constString("assign"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_assign
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("len"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_len
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString(">>|"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_moveArg
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("clone"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_clone
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
                    .type = CHAR,
                    .value.character = '\n'
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("#space"),
                    .type = CHAR,
                    .value.character = ' '
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("#tab"),
                    .type = CHAR,
                    .value.character = '\t'
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("#paropn"),
                    .type = INT,
                    .value.integer = 40
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("#parcls"),
                    .type = INT,
                    .value.integer = ')'
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
                    .type = ARRAY,
                    .value.array = mkStringArray(argc, argv)
                }, ans);
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

    if(src.len == 0) return Nothing;

    if(src.data[0] == '-') {
        positive = false;
         i++;
    }

    if(src.len - i == 0) return Nothing;

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
    if(s.type == ITSELF) {
        if(s.word.len == 2
                && s.word.data[0] == '#') {
            return (Symbol) {
                .word = s.word,
                    .type = INT,
                    .value.integer = (int) s.word.data[1] };
        }
        Symbol sn = strToInt(s.word);
        if(sn.type == INT) {
            return sn;
        }
    } 
    return s;
}

void eval (List *body, List **stack, List **vars);

void evalSym (Symbol insym, List **stack, List **vars) {
    if(stringEq(insym.word, Nothing.word)) {
        *stack = cons(Nothing, *stack);
        return;
    }

    Symbol s = find(insym.word, *vars);
    if(s.type != NOTHING) {
        if(s.type == BUILTIN) {
            s.value.builtin(stack, vars);
        } else if(s.type == FUNCTION) {
            eval(s.value.list, stack, vars);
        } else {
            *stack = cons(s, *stack);
        }
    } else {
        *stack = cons(specialSym(insym), *stack);
    }
}

void eval (List *body, List **stack, List **vars) {
    List *oldvars = *vars;
    for(List *cur = body; cur != NULL; cur = cur->next) {
        evalSym(cur->val, stack, vars);
    }
    if(*vars != oldvars) {
        List *cur = *vars;
        while(cur->next != oldvars) {
            cur = cur->next;
        }
        cur->next = NULL;
        freeList(*vars);
        *vars = oldvars;
    }
}

void run_source(Source root, List **vars) {
    List *stack = NULL;

    uint symbols_count = count_symbols(root);

    StringArray symbols = mk_StringArray(symbols_count);
    load_symbols (root, symbols, 0);

    for(uint i = 0; i < symbols.len; i++) {
        String current = symbols.data[i];
        
        if(stringEq(current, Nothing.word)) {
            stack = cons(Nothing, stack);
            continue;
        }
        
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

Symbol conv_char_int (Symbol src, List **sidestack) {
    return (Symbol) {
        .word = src.word,
        .type = INT,
        .value.integer = (int)src.value.character
    };
}

Converter converters[] = (Converter[]) {
    {.srcType = SOURCE,
     .dstType = STRING,
     .act = &conv_Source_String},
    {.srcType = CHAR,
     .dstType = INT,
     .act = &conv_char_int}
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
    } else if(a.type == CHAR) {
        return a.value.character == b.value.character;
    } else if (a.type == STRING) {
        return stringEq(a.value.string, b.value.string);
    } else if (a.type == ITSELF) {
        return stringEq(a.word, b.word);
    } else if (a.type == NOTHING) {
        return true;
    } else {
        fprintf(stderr, "TODO: symbolEq for type %d.\n", a.type);
        return false;
    }
}

void builtin_exit (List **stack, List **vars) {
    List *args = getArgs(stack, 1, (int[]) { INT });
    argsOrWarn(args);

    int exitCode = pop(&args).value.integer;
    exit(exitCode);
}

void builtin_in (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { LIST, ANY });
    argsOrWarn(args);

    List *options = pop(&args).value.list;
    Symbol ref = pop(&args);

    for(List *opt = options; opt != NULL; opt = opt->next) {
        Symbol sym = find(opt->val.word, *vars);
        if(sym.type == NOTHING) sym = opt->val;

        if(symbolEq(sym, ref)) {
            *stack = consBool(true,
                              cons(ref, *stack));
            return;
        }
    }

    *stack = consBool(false,
                      cons(ref, *stack));
}

void builtin_or (List **stack, List **vars) {
    List *args = getArgs(stack, 1, (int[]) { LIST });
    if(args != NULL) {
        List *tests = pop(&args).value.list;
        List *bools = NULL;

        for(List *cur = tests; cur != NULL; cur = cur->next) {
            evalSym(cur->val, stack, vars);
            if(stack != NULL && (*stack)->val.type == BOOLEAN) {
                List *stacktail = (*stack)->next;
                (*stack)->next = bools;
                bools = *stack;
                *stack = stacktail;
            }
        }

        while(bools != NULL) {
            if(pop(&bools).value.boolean == true) {
                freeList(bools);
                *stack = consBool(true, *stack);
                return;
            }
        }

        *stack = consBool(false, *stack);

        return;
    }
    args = getArgs(stack,  2, (int[]) { BOOLEAN, BOOLEAN });
    argsOrWarn(args);

    bool a = pop(&args).value.boolean;
    bool b = pop(&args).value.boolean;

    *stack = consBool(a || b, *stack);
}

void builtin_and (List **stack, List **vars) {
    List *args = getArgs(stack, 1, (int[]) { LIST });
    if(args != NULL) {
        List *tests = pop(&args).value.list;
        List *bools = NULL;

        for(List *cur = tests; cur != NULL; cur = cur->next) {
            evalSym(cur->val, stack, vars);
            if(stack != NULL && (*stack)->val.type == BOOLEAN) {
                List *stacktail = (*stack)->next;
                (*stack)->next = bools;
                bools = *stack;
                *stack = stacktail;
            }
        }

        while(bools != NULL) {
            if(pop(&bools).value.boolean != true) {
                freeList(bools);
                *stack = consBool(false, *stack);
                return;
            }
        }

        *stack = consBool(true, *stack);

        return;
    }
    args = getArgs(stack,  2, (int[]) { BOOLEAN, BOOLEAN });
    argsOrWarn(args);

    bool a = pop(&args).value.boolean;
    bool b = pop(&args).value.boolean;

    *stack = consBool(a && b, *stack);
}

void builtin_lt (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int a = pop(&args).value.integer;
    int b = args->val.value.integer;

    args->next = *stack;
    *stack = args;

    *stack = consBool(a > b, *stack);

    // > is used to make it more intuitve, as stack
    // is reverse to order in the code.
}

void builtin_lte (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int a = pop(&args).value.integer;
    int b = args->val.value.integer;

    args->next = *stack;
    *stack = args;

    *stack = consBool(a >= b, *stack);

    // > is used to make it more intuitve, as stack
    // is reverse to order in the code.
}

void builtin_gt (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int a = pop(&args).value.integer;
    int b = args->val.value.integer;

    args->next = *stack;
    *stack = args;

    *stack = consBool(a < b, *stack);

    // < is used to make it more intuitve, as stack
    // is reverse to order in the code.
}

void builtin_gte (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int a = pop(&args).value.integer;
    int b = args->val.value.integer;

    args->next = *stack;
    *stack = args;

    *stack = consBool(a <= b, *stack);

    // < is used to make it more intuitve, as stack
    // is reverse to order in the code.
}

void builtin_plus (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    *stack = consInt(pop(&args).value.integer
                        + pop(&args).value.integer,
                     *stack);
}

void builtin_minus (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int b = pop(&args).value.integer;
    int a = pop(&args).value.integer;
    *stack = consInt(a - b, *stack);
}

void builtin_clone (List **stack, List **vars) {
    if(*stack != NULL) {
        *stack = cons((*stack)->val, *stack);
    }
}

void builtin_assign (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { ITSELF, ANY });
    argsOrWarn(args);

    Symbol name = pop(&args);
    Symbol val = pop(&args);


    *vars = cons((Symbol) {
                    .word = name.word,
                    .type = val.type,
                    .value = val.value }, *vars);
}

void evalListExpr(List *listExpr) {
    String opn = constString("("),
           cls = constString(")");   
    List *trace = NULL;
    List *cur = listExpr;
    List *last = NULL;
    while(cur != NULL) {
        if(stringEq(cur->val.word, opn)) {
            trace = consList(trace, cur);
        } else if (stringEq(cur->val.word, cls)) {
            if(trace == NULL) {
                freeList(listExpr->next);
                if(listExpr->val.type == LIST)
                    freeList(listExpr->val.value.list);
                listExpr->val = Nothing;
                return;
            }
            pop(&cur);
            last->next = NULL;
            List *begining = pop(&trace).value.list;
            begining->val.word = constString("");
            begining->val.type = LIST;
            begining->val.value.list = begining->next;
            begining->next = cur;
            last = NULL;
            continue;
        }

        last = cur;
        cur = cur->next;           
    }
}

bool evalCondexpr (Symbol expr, List **stack, List **vars) {
    if(stack == NULL) return false;

    if(expr.type == LIST) {
        eval(expr.value.list, stack, vars);
        Symbol ret = pop(stack);
        if(ret.type != BOOLEAN) {
            fprintf(stderr, "evalCondexpr() wrong return value.\n");
            return false;
        }
        return ret.value.boolean;
    } else {
        Symbol ref = find(expr.word, *vars);
        if(ref.type == NOTHING)
            return symbolEq(expr, (*stack)->val);
        return symbolEq(ref, (*stack)->val);
    }
}

void builtin_match (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { LIST, ANY });
    if(args == NULL)
        return;

    List *rules = pop(&args).value.list;
    evalListExpr(rules);
    args->next = *stack;
    *stack = args;

    while(rules != NULL && rules->next != NULL) {
        if(evalCondexpr(rules->val, stack, vars)) {
            if(rules->next->val.word.len == 0
                && rules->next->val.type == LIST) {
                eval(rules->next->val.value.list,
                                   stack, vars);
            } else {
                *stack = cons(rules->next->val,
                              *stack);
            }
            freeList(rules);
            return;
        }
        List *nextcur = rules->next->next;
        rules->next->next = NULL;
        freeList(rules);
        rules = nextcur;
    }

    if(rules != NULL){
        if(rules->val.word.len == 0
            && rules->val.type == LIST) {
            eval(pop(&rules).value.list, stack, vars);
        } else {
            *stack = cons(pop(&rules), *stack);
        }
    }
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

    *stack = consBool(symbolEq(a, b), *stack);
}

void builtin_at (List **stack, List **vars) {
    List *args = getArgs(stack, 2, (int[]) { INT, STRING });
    if(args == NULL) {
        List *args = getArgs(stack, 2, (int[]) { INT, ARRAY });
        argsOrWarn(args);

        int idx = pop(&args).value.integer;
        StringArray sar = args->val.value.array;
        args->next = *stack;

        if(idx >= sar.len || idx < 0)
            *stack = cons(Nothing, args);
        else
            *stack = consString(sar.data[idx], args);

        return;
    }

    int idx = pop(&args).value.integer;
    String src = args->val.value.string;
    args->next = *stack;

    if(idx >= src.len || idx < 0)
        *stack = cons(Nothing, args);
    else 
        *stack = consChar(src.data[idx], args);
}

void builtin_len (List **stack, List **vars) {
    List *args = getArgs(stack, 1, (int[]) { STRING });
    argsOrWarn(args);

    String s = args->val.value.string;
    args->next = *stack;
    *stack = consInt(s.len, args);
}

void builtin_moveArg (List **stack, List **vars) {
    List *args = getArgs(stack, 1, (int[]) { INT });
    argsOrWarn(args);

    int n = pop(&args).value.integer;
    uint i = 0;
    List *cur = *stack;
    while(i < n-1 && cur != NULL) {
        cur = cur->next;
        i++;
    }

    if(i != n - 1 || cur == NULL || cur->next == NULL) {
        *stack = cons(Nothing, *stack);
    } else {
        List *mem = cur->next;
        cur->next = mem->next;
        mem->next = *stack;
        *stack = mem;
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
    List *args = getArgs(stack, 2, (int[]) { INT, ANY });
    if(args != NULL) {
        int distance = pop(&args).value.integer;
        Symbol val = pop(&args);

        List *cur = *stack;
        for(int i = 1; i < distance; i++)
            cur = cur->next;

        if(cur->next->val.type == LIST) {
            Symbol *s = &(cur->next->val);
            s->value.list = cons(val, s->value.list);
        } else
           cur->next = consList(cur->next,
                                cons(val, NULL));
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

    for(int i = from; i <= to; i++) {
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
        printSymbol(s);
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
    } else if(s.type == CHAR) {
        printf("%c", s.value.character);
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
    } else if (s.type == STRING) {
        String str = s.word;
        char buff[str.len+1];
        buff[str.len] = 0;
        strncpy(buff, str.data, str.len);
        *stack = cons((Symbol){.word = str,
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

void builtin_substr(List **stack, List **vars) {
    List *args = getArgs(stack, 3, (int[]) { INT, INT, STRING });
    argsOrWarn(args);

    int end = pop(&args).value.integer;
    int start = pop(&args).value.integer;

    String str = args->val.value.string;

    args->next = *stack;
    *stack = consString(
        (String){.data = str.data+start,
                 .len = end - start },
        args);
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

extern char _binary_lerl_lrc_start;
extern char _binary_lerl_lrc_end;

int main(int argc, const char **argv) {
    List *globalsym = initial_global_symtab(argc-1, argv+1);
    run_source((Source) {
                 .name = "(builtin init)",
                 .buff = &_binary_lerl_lrc_start,
                 .len = &_binary_lerl_lrc_end - &_binary_lerl_lrc_start,
                 .fd = -1} , &globalsym);
    freeList(globalsym);

    return 0;
}
