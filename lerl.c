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

bool dbg = false;

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
        uint    refs; \
        size_t  len; \
    } TYPE ## Array; \
    \
    TYPE ## Array \
    mk_ ## TYPE ## Array (uint len) { \
        return (TYPE ## Array) { \
            .data = malloc (sizeof(TYPE) * len), \
            .refs = 1, \
            .len = len \
        }; \
    } \
    \
    void free_ ## TYPE ## Array (TYPE ## Array tgt) {\
        if(--tgt.refs == 0) \
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
typedef struct RunEnv RunEnv;

struct Symbol {
    String  word;
    enum { STRING, INT, CHAR, BUILTIN, FUNCTION, ARRAY, SOURCE, LIST, ITSELF, BOOLEAN, SCOPE, NOTHING, ANY } type;
    union {
        String      string;
        void        (*builtin) (RunEnv *env);
        StringArray array;
        Source      source;
        List        *list;
        bool        boolean;
        char        character;
        int         integer;
    } value;
};
ArrayOf(Symbol)

struct RunEnv {
    List *stack, *globals, *scopeStack;
};

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

void builtin_load(RunEnv *env);
void builtin_content(RunEnv *env);
void builtin_cut(RunEnv *env);
void builtin_quote(RunEnv *env);
void builtin_isString(RunEnv *env);
void builtin_doWhile(RunEnv *env);
void builtin_whileDo(RunEnv *env);
void builtin_defun(RunEnv *env);
void builtin_stash(RunEnv *env);
void builtin_reverse(RunEnv *env);
void builtin_drop (RunEnv *env);
void builtin_dropOne (RunEnv *env);
void builtin_at (RunEnv *env);
void builtin_eq (RunEnv *env);
void builtin_neq (RunEnv *env);
void builtin_if (RunEnv *env);
void builtin_match (RunEnv *env);
void builtin_doCounting (RunEnv *env);
void builtin_len (RunEnv *env);
void builtin_moveArg (RunEnv *env);
void builtin_assign (RunEnv *env);
void builtin_clone (RunEnv *env);
void builtin_plus (RunEnv *env);
void builtin_minus (RunEnv *env);
void builtin_mul (RunEnv *env);
void builtin_lt (RunEnv *env);
void builtin_gt (RunEnv *env);
void builtin_lte (RunEnv *env);
void builtin_gte (RunEnv *env);
void builtin_and (RunEnv *env);
void builtin_not (RunEnv *env);
void builtin_or (RunEnv *env);
void builtin_substr (RunEnv *env);
void builtin_in (RunEnv *env);
void builtin_exit (RunEnv *env);
void builtin_dbgon (RunEnv *env);
void builtin_dbgoff (RunEnv *env);
void builtin_eval (RunEnv *env);
void builtin_toInt (RunEnv *env);
void builtin_toSym (RunEnv *env);
void builtin_toStr (RunEnv *env);
void builtin_lst (RunEnv *env);
void builtin_pop (RunEnv *env);
void builtin_isEmpty (RunEnv *env);
void printSymbol (Symbol s);

typedef struct List {
    Symbol      val;
    uint        refs;
    struct List *next;
} List;

List *cons(Symbol value, List *before) {
    List *ans = malloc(sizeof(List));
    *ans = (List) {
        .val = value,
        .refs = 1,
        .next = before
    };

    #ifdef DEBUG_MEM
    printf("cons %p: ", ans);
    printSymbol(value);
    printf("\n");
    #endif 

    return ans;
}

void freeList (List *l) {
    if(l == NULL) return;

    #ifdef DEBUG_MEM
    printf("freeing %p (%u refs): ", l, l->refs);
    printSymbol(l->val);
    printf("\n");
    #endif

    if(l->refs-- > 1) return;

    if(l->val.type == LIST) {
        freeList(l->val.value.list);
    }

    if(l->val.type == ARRAY) {
        free_StringArray(l->val.value.array);
    }
    if(l->val.type == SOURCE) {
        close_source(l->val.value.source);
    }

    if(l->next != NULL) {
        freeList(l->next);
    }

    free(l);
}

List *cloneListUntil(List *l, List *last) {
    if(l == NULL) return NULL;

    List *ans = cons(l->val, NULL);
    List *wcur = ans;
    for(List *cur = l->next; cur != NULL && cur != last; cur = cur->next) {
        wcur->next = cons(cur->val, NULL);
        wcur = wcur->next;
    }

    wcur->next = NULL;
    return ans;
}

List *joinLists(List *a, List *b) {
    if(a == NULL) return b;

    List *cur = a;
    while(cur->next != NULL) cur=cur->next;
    cur->next = b;
    b->refs++;
    return a;
}

List *splitList(List **l, uint index) {
    if(index == 0) return NULL;

    List *cur = *l;
    bool needs_clone = false;
    for(uint i = 1; i < index; i++) {
        needs_clone |= (cur->refs > 1);
        cur = cur->next;
    }
    needs_clone |= (cur->refs > 1);

    List *ans = *l;
    if(needs_clone) {
        ans = cloneListUntil(*l, cur);
        freeList(*l);
    }

    *l = cur->next;
    return joinLists(ans, cur->next);
}

Symbol refsym(Symbol s) {
    if(s.type == LIST && s.value.list != NULL) {
        s.value.list->refs++;
    } else if (s.type == ARRAY) {
        s.value.array.refs++;
    }

    return s;
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

    #ifdef DEBUG_MEM
    printf("popping %p: ", *l);
    printSymbol((*l)->val);
    printf("\n");
    #endif

    Symbol s = (*l)->val;

    List *old = *l;
    *l = (*l)->next;
    if(old->refs-- == 1)
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

List *reversedList(List *src) {
    if(src == NULL) return NULL;

    List *ans = cons(src->val, NULL);
    while(src->next != NULL) {
        src = src->next;
        ans = cons(src->val, ans);
    }

    return ans;
}

Symbol find(String name, List *list) {
    for(List *l = list; l != NULL; l = l->next) {
        if(stringEq(name, l->val.word))
            return l->val;
    }

    return Nothing;
}

Symbol findVar(RunEnv *env, String name) {
    if(env->scopeStack != NULL) {
        Symbol sym = find(name, env->scopeStack->val.value.list);
        if(sym.type != NOTHING) return sym;
    }

    return find(name, env->globals);
}

void printSymbol (Symbol s) {
    if(s.type == ARRAY) printStringArray(s.word, s.value.array);    
    else if (s.type == STRING)
        printf("\"%.*s\" ", (int)s.value.string.len, s.value.string.data);
    else if (s.type == ITSELF)
        printf("%.*s ", (int)s.word.len, s.word.data);
    else if (s.type == SOURCE)
        printf("SOURCE %.*s ", (int)s.word.len, s.word.data);
    else if (s.type == LIST || s.type == FUNCTION || s.type == SCOPE) {
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
                    .word = constString("lst"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_lst
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("pop"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_pop
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("next"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_pop
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("doWhile"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_doWhile
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("whileDo"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_whileDo
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
                    .word = constString("empty?"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_isEmpty
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("substr"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_substr
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("!@"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_eval
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("="),
                    .type = BUILTIN,
                    .value.builtin = &builtin_eq
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("!="),
                    .type = BUILTIN,
                    .value.builtin = &builtin_neq
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("&"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_and
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("not"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_not
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
                    .word = constString("*"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_mul
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
                    .word = constString("+dbg"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_dbgon
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString("-dbg"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_dbgoff
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString(">int"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_toInt
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString(">sym"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_toSym
                }, ans);
    ans = cons( (Symbol) {
                    .word = constString(">str"),
                    .type = BUILTIN,
                    .value.builtin = &builtin_toStr
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
        } else if (s.word.data[0] == '\'') {
            return (Symbol) {
                .word = {.data = s.word.data + 1,
                         .len = s.word.len - 1},
                .type = ITSELF};
        }
        Symbol sn = strToInt(s.word);
        if(sn.type == INT) {
            return sn;
        }
    } 
    return s;
}

void eval (Symbol body, RunEnv *env);

void evalSym (Symbol insym, RunEnv *env) {
    if(dbg) {
        printf("eval: ");
        printSymbol(insym);
        if(env->stack) {
            putc(' ', stdout);
            printSymbol(env->stack->val);
        }
        putc('\n', stdout);
    }

    if(stringEq(insym.word, Nothing.word)) {
        env->stack = cons(Nothing, env->stack);
        return;
    }

    if(insym.type != ITSELF) {
        env->stack = cons(insym, env->stack);
        return;
    }

    Symbol s = findVar(env, insym.word);
    if(s.type != NOTHING) {
        if(s.type == BUILTIN) {
            s.value.builtin(env);
        } else if(s.type == FUNCTION) {
            eval(s, env);
        } else {
            env->stack = cons(refsym(s), env->stack);
        }
    } else {
        env->stack = cons(specialSym(insym), env->stack);
    }

    if(dbg && env->stack) {
        printf(" >> ");
        printSymbol(env->stack->val);
        putc('\n', stdout);
    }
}

void eval (Symbol body, RunEnv *env) {
    env->scopeStack = cons((Symbol) {
                             .word = (body.word.len > 0)
                                                ?body.word
                                                :constString("(eval)"),
                             .type = SCOPE,
                             .value.list = (body.word.len > 0)
                                                ?NULL
                                                :env->scopeStack->val.value.list },
                            env->scopeStack);

    for(List *cur = body.value.list; cur != NULL; cur = cur->next) {
        evalSym(cur->val, env);
    }

    pop(&(env->scopeStack));
}

void run_source(Source root, List **vars) {
    RunEnv env = { .stack = NULL,
                   .globals = *vars,
                   .scopeStack = consList(NULL, NULL) };

    uint symbols_count = count_symbols(root);

    StringArray symbols = mk_StringArray(symbols_count);
    load_symbols (root, symbols, 0);

    for(uint i = 0; i < symbols.len; i++) {
        String current = symbols.data[i];
        
        if(stringEq(current, Nothing.word)) {
            env.stack = cons(Nothing, env.stack);
            continue;
        }
        
        Symbol val = findVar(&env, current);

        if(val.type == BUILTIN) {
            val.value.builtin(&env);
        } else if (val.type == FUNCTION) {
            eval(val, &env);
        } else if (val.type == NOTHING) {
            env.stack = cons(specialSym((Symbol){
                                    .word = current,
                                    .type = ITSELF}),
                         env.stack);
        } else {
            env.stack = cons(refsym(val), env.stack);
        }

        *vars = env.globals;
    }

    if(env.stack != NULL) {
        printf("\n");
        printSymbol((Symbol) {
            .word=constString("stack"),
            .type=LIST,
            .value.list = env.stack});
        printf("\n");
    }

    free(symbols.data);
    freeList(env.stack);
}

void verifyArg(List *stack, const char *name) {
    if(stack == NULL) {
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

List *getArgs(RunEnv *env, uint count, int types[]) {
    if(&(env->stack) == NULL || env->stack == NULL) {
        return NULL;
    }

    Converter *convs[count];
    for(uint i = 0; i < count; i++) {
        convs[i] = 0;
    }

    List *cur = env->stack;
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
        (env->stack)->val = convs[0]->act((env->stack)->val, &sidestack);
    }

    cur = env->stack;
    for(uint i = 1; i < count; i++) {
        cur = cur->next;
        if(convs[i] != NULL) {
            cur->val = convs[i]->act(cur->val, &sidestack);
        }
    }

    List *args = env->stack;
    env->stack = cur->next;

    if(sidestack != NULL) {
        List *cur = sidestack;
        for(; cur->next != NULL; cur = cur->next);
        cur->next = env->stack;
        env->stack = sidestack;
    }

    return args;
}

Symbol implicitMap(RunEnv *env, void (*self) (RunEnv *)) {
    Symbol s = pop(&(env->stack)); 
    if(s.type == ARRAY) {
        List *ans = NULL;
        StringArray arr = s.value.array;
        for(uint i = 0; i < arr.len; i++) {
            Symbol sym = (Symbol) {
                            .word = arr.data[i],
                            .type = ITSELF
                         };
            ans = cons(sym, ans);
            RunEnv inenv = {.stack = ans,
                            .globals = env->globals,
                            .scopeStack = env->scopeStack };
            self(&inenv);

            free_StringArray(s.value.array);
        }
        return (Symbol) {
                 .word=constString(""),
                 .type=LIST,
                 .value.list=ans};
    } else if(s.type == LIST) {
        List *ans = NULL;
        for(List *cur = s.value.list; cur != NULL; cur=cur->next) {
            ans = cons(cur->val, ans);
            RunEnv inenv = {.stack = ans,
                            .globals = env->globals,
                            .scopeStack = env->scopeStack};
            self(&inenv);
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
        exit(1); \
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

void builtin_isEmpty (RunEnv *env) {
    if(env->stack == NULL || env->stack->val.type != LIST) {
        fprintf(stderr, "builtin_empty?: wrong arg\n");
        return;
    }

    env->stack = consBool(env->stack->val.value.list == NULL, env->stack);
}
void builtin_pop (RunEnv *env) {
    if(env->stack == NULL || env->stack->val.type != LIST) {
        fprintf(stderr, "builtin_pop: wrong arg\n");
        return;
    }

    List **lptr = &(env->stack->val.value.list);
    if(*lptr == NULL) {
        env->stack = cons(Nothing, env->stack);
        return;
    }

    if((*lptr)->refs == 1) {
        List *tail = (*lptr)->next;
        (*lptr)->next = env->stack;
        env->stack = *lptr;
        *lptr = tail;
    } else
        env->stack = cons(pop(lptr), env->stack);
}

void builtin_lst (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]){ INT });
    argsOrWarn(args);

    int n = pop(&args).value.integer;
    List *cur = env->stack;
    for(uint i = 1; i < n && cur != NULL; i++, cur = cur->next);
    if(cur == NULL) {
        fprintf(stderr, "builtin_lst: insufficient args.\n");
        return;
    }

    List *newstack = cur->next;
    cur->next = NULL;
    env->stack = consList(newstack, env->stack);
}

void builtin_toInt (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]){ STRING });
    argsOrWarn(args);

    env->stack = cons(strToInt(pop(&args).value.string), env->stack);
}

void builtin_toSym (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]){ STRING });
    argsOrWarn(args);

    String str = pop(&args).value.string;
    env->stack = cons ((Symbol) { .word = str,
                              .type = ITSELF },
                   env->stack);
}

void builtin_toStr (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]){ ITSELF });
    argsOrWarn(args);

    Symbol sym = pop(&args);
    sym.type = STRING;
    sym.value.string = sym.word;

    env->stack = cons (sym, env->stack);
}

void builtin_eval (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]){ LIST });
    argsOrWarn(args);

    Symbol sym = pop(&args);
    eval(sym, env);
    freeList(sym.value.list);
}

void builtin_dbgon (RunEnv *env) {
    dbg = true;
}

void builtin_dbgoff (RunEnv *env) {
    dbg = false;
    printf("Stack:\n");
    printList(env->stack);
    printf("\n*************\n");
}

void builtin_exit (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]) { INT });
    argsOrWarn(args);

    int exitCode = pop(&args).value.integer;
    exit(exitCode);
}

void builtin_in (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { LIST, ANY });
    argsOrWarn(args);

    List *options = pop(&args).value.list;
    Symbol ref = pop(&args);

    for(List *opt = options; opt != NULL; opt = opt->next) {
        Symbol sym = findVar(env, opt->val.word);
        if(sym.type == NOTHING) sym = opt->val;

        if(symbolEq(sym, ref)) {
            env->stack = consBool(true,
                              cons(ref, env->stack));
            freeList(options);
            return;
        }
    }

    freeList(options);
    env->stack = consBool(false,
                      cons(ref, env->stack));
}

void builtin_or (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]) { LIST });
    if(args != NULL) {
        List *tests = pop(&args).value.list;
        List *bools = NULL;

        for(List *cur = tests; cur != NULL; cur = cur->next) {
            evalSym(cur->val, env);
            if(env->stack != NULL && env->stack->val.type == BOOLEAN) {
                List *stacktail = env->stack->next;
                env->stack->next = bools;
                bools = env->stack;
                env->stack = stacktail;
            }
        }

        freeList(tests);

        while(bools != NULL) {
            if(pop(&bools).value.boolean == true) {
                freeList(bools);
                env->stack = consBool(true, env->stack);
                return;
            }
        }

        env->stack = consBool(false, env->stack);

        return;
    }
    args = getArgs(env,  2, (int[]) { BOOLEAN, BOOLEAN });
    argsOrWarn(args);

    bool a = pop(&args).value.boolean;
    bool b = pop(&args).value.boolean;

    env->stack = consBool(a || b, env->stack);
}

void builtin_not (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]) { BOOLEAN });
    env->stack = consBool(!(pop(&args).value.boolean), env->stack);
}

void builtin_and (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]) { LIST });
    if(args != NULL) {
        List *tests = pop(&args).value.list;
        List *bools = NULL;

        for(List *cur = tests; cur != NULL; cur = cur->next) {
            evalSym(cur->val, env);
            if(env->stack != NULL && env->stack->val.type == BOOLEAN) {
                List *stacktail = env->stack->next;
                env->stack->next = bools;
                bools = env->stack;
                env->stack = stacktail;
            }
        }

        freeList(tests);

        while(bools != NULL) {
            if(pop(&bools).value.boolean != true) {
                freeList(bools);
                env->stack = consBool(false, env->stack);
                return;
            }
        }

        env->stack = consBool(true, env->stack);

        return;
    }
    args = getArgs(env,  2, (int[]) { BOOLEAN, BOOLEAN });
    argsOrWarn(args);

    bool a = pop(&args).value.boolean;
    bool b = pop(&args).value.boolean;

    env->stack = consBool(a && b, env->stack);
}

void builtin_lt (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int a = pop(&args).value.integer;
    int b = args->val.value.integer;

    args->next = env->stack;
    env->stack = args;

    env->stack = consBool(a > b, env->stack);

    // > is used to make it more intuitve, as stack
    // is reverse to order in the code.
}

void builtin_lte (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int a = pop(&args).value.integer;
    int b = args->val.value.integer;

    args->next = env->stack;
    env->stack = args;

    env->stack = consBool(a >= b, env->stack);

    // > is used to make it more intuitve, as stack
    // is reverse to order in the code.
}

void builtin_gt (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int a = pop(&args).value.integer;
    int b = args->val.value.integer;

    args->next = env->stack;
    env->stack = args;

    env->stack = consBool(a < b, env->stack);

    // < is used to make it more intuitve, as stack
    // is reverse to order in the code.
}

void builtin_gte (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int a = pop(&args).value.integer;
    int b = args->val.value.integer;

    args->next = env->stack;
    env->stack = args;

    env->stack = consBool(a <= b, env->stack);

    // < is used to make it more intuitve, as stack
    // is reverse to order in the code.
}

void builtin_plus (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    env->stack = consInt(pop(&args).value.integer
                        + pop(&args).value.integer,
                     env->stack);
}

void builtin_minus (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    int b = pop(&args).value.integer;
    int a = pop(&args).value.integer;
    env->stack = consInt(a - b, env->stack);
}

void builtin_mul (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { INT, INT });
    argsOrWarn(args);

    env->stack = consInt(pop(&args).value.integer
                        * pop(&args).value.integer,
                     env->stack);
}

void builtin_clone (RunEnv *env) {
    if(env->stack != NULL) {
        env->stack = cons((env->stack)->val, env->stack);
    }
}

void builtin_assign (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { ITSELF, ANY });
    argsOrWarn(args);

    Symbol name = pop(&args);
    Symbol val = pop(&args);


    env->scopeStack->val.value.list
        = cons((Symbol) {
                .word = name.word,
                .type = val.type,
                .value = val.value }, 
            env->scopeStack->val.value.list);
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

bool evalCondexpr (Symbol expr, RunEnv *env) {
    if(env->stack == NULL) return false;

    if(expr.type == LIST) {
        eval(expr, env);
        Symbol ret = pop(&(env->stack));
        if(ret.type != BOOLEAN) {
            fprintf(stderr, "evalCondexpr() wrong return value.\n");
            return false;
        }
        return ret.value.boolean;
    } else {
        Symbol ref = findVar(env, expr.word);
        if(ref.type == NOTHING)
            return symbolEq(expr, (env->stack)->val);
        return symbolEq(ref, (env->stack)->val);
    }
}

void builtin_match (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { LIST, ANY });
    if(args == NULL)
        return;

    List *rules = pop(&args).value.list;
    evalListExpr(rules);
    args->next = env->stack;
    env->stack = args;

    while(rules != NULL && rules->next != NULL) {
        if(evalCondexpr(rules->val, env)) {
            if(rules->next->val.word.len == 0
                && rules->next->val.type == LIST) {
                eval(rules->next->val, env);
            } else {
                env->stack = cons(rules->next->val, env->stack);
            }
            return;
        }
        List *nextcur = rules->next->next;
        rules->next->next = NULL;
        rules = nextcur;
    }

    if(rules != NULL){
        // TODO: Why len == 0??
        if(rules->val.word.len == 0
            && rules->val.type == LIST) {
            Symbol body = pop(&rules);
            eval(body, env);
            freeList(body.value.list);
        } else {
            env->stack = cons(pop(&rules), env->stack);
        }
    }
    else
        env->stack = cons(Nothing, env->stack);
}

void builtin_if (RunEnv *env) {
    List *args = getArgs(env, 3, (int[]) { LIST, LIST, BOOLEAN });
    if(args != NULL) {
        Symbol ifb = pop(&args);
        Symbol elseb = pop(&args);
        bool which = pop(&args).value.boolean;

        if(which)
            eval(ifb, env);
        else
            eval(elseb, env);

        freeList(ifb.value.list);
        freeList(elseb.value.list);
    } else {
        args = getArgs(env, 2, (int[]) { LIST, BOOLEAN });
        argsOrWarn(args);
        Symbol ifb = pop(&args);
        bool which = pop(&args).value.boolean;
        
        if(which) {
            eval(ifb, env);
        }

        freeList(ifb.value.list);
    }
}

void builtin_eq (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { ANY, ANY });
    argsOrWarn(args);

    Symbol a = args->val;
    Symbol b = args->next->val;

    args->next->next = env->stack;
    env->stack = args->next;
    args->next = NULL;
    freeList(args);

    env->stack = consBool(symbolEq(a, b), env->stack);
}

void builtin_neq (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { ANY, ANY });
    argsOrWarn(args);

    Symbol a = args->val;
    Symbol b = args->next->val;

    args->next->next = env->stack;
    env->stack = args->next;
    args->next = NULL;
    freeList(args);

    env->stack = consBool(!symbolEq(a, b), env->stack);
}

void builtin_at (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { INT, STRING });
    if(args == NULL) {
        List *args = getArgs(env, 2, (int[]) { INT, ARRAY });
        argsOrWarn(args);

        int idx = pop(&args).value.integer;
        StringArray sar = args->val.value.array;
        args->next = env->stack;

        if(idx >= sar.len || idx < 0)
            env->stack = cons(Nothing, args);
        else
            env->stack = consString(sar.data[idx], args);

        return;
    }

    int idx = pop(&args).value.integer;
    String src = args->val.value.string;
    args->next = env->stack;

    if(idx >= src.len || idx < 0)
        env->stack = cons(Nothing, args);
    else 
        env->stack = consChar(src.data[idx], args);
}

void builtin_len (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]) { STRING });
    argsOrWarn(args);

    String s = args->val.value.string;
    args->next = env->stack;
    env->stack = consInt(s.len, args);
}

void builtin_moveArg (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]) { INT });
    argsOrWarn(args);

    int n = pop(&args).value.integer;
    uint i = 0;
    List *cur = env->stack;
    while(i < n-1 && cur != NULL) {
        cur = cur->next;
        i++;
    }

    if(i != n - 1 || cur == NULL || cur->next == NULL) {
        env->stack = cons(Nothing, env->stack);
    } else {
        List *mem = cur->next;
        cur->next = mem->next;
        mem->next = env->stack;
        env->stack = mem;
    }
}

void builtin_drop (RunEnv *env) {
    freeList(env->stack);
    env->stack = NULL;
}

void builtin_dropOne (RunEnv *env) {
    Symbol s = pop(&(env->stack));
    if(s.type == LIST)
        freeList(s.value.list);
}

void builtin_stash (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]) { INT, ANY });
    argsOrWarn(args);
    int distance = pop(&args).value.integer;
    Symbol val = pop(&args);

    List *cur = env->stack;
    for(int i = 1; i < distance; i++)
         cur = cur->next;

    if(cur == NULL) {
        fprintf(stderr, "builtin_stash: to short stack for len=%d\n",
                distance);
        return;
    }

    if(cur->next != NULL && cur->next->val.type == LIST) {
         Symbol *s = &(cur->next->val);
         s->value.list = cons(val, s->value.list);
    } else
         cur->next = consList(cur->next,
                              cons(val, NULL));
}

void builtin_defun (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]){ITSELF, LIST});
    if(args == NULL) {
        fprintf(stderr, "wrong arguments for fn(Itself, List)\n");
        return;
    }

    Symbol sym = pop(&args);
    args->val.type = FUNCTION;
    args->val.word = sym.word;
    args->next = env->globals;
    env->globals = args;
}

void builtin_reverse (RunEnv *env) {
    List *args = getArgs(env, 1, (int[]){LIST});
    if(args != NULL) {
        List *oldList = args->val.value.list;
        args->val.value.list = reversedList(oldList);

        args->next = env->stack;
        env->stack = args;

        freeList(oldList);
    }
}

void builtin_doCounting (RunEnv *env) {
    List *args = getArgs(env, 3, (int[]){INT, INT, LIST});
    argsOrWarn(args);

    int to = pop(&args).value.integer;
    int from = pop(&args).value.integer;
    Symbol commands = pop(&args);

    for(int i = from; i <= to; i++) {
        env->stack = consInt(i, env->stack);
        eval(commands, env);
    }

    freeList(commands.value.list);
}
    
void builtin_doWhile (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]){LIST, LIST});
    if(args == NULL) {
        fprintf(stderr, "wrong arguments for doWhile(List, List)\n");
        return;
    }

    Symbol condition = pop(&args);
    Symbol body = pop(&args);
    List *ans;

    do {
        eval(body, env);
        eval(condition, env);
        ans = getArgs(env, 1, (int[]){BOOLEAN});
        if(ans == NULL) {
            fprintf(stderr, "Wrong condition for doWhile()\n");
            printSymbol((env->stack)->val);

            freeList(body.value.list);
            freeList(condition.value.list);
            return;
        }
    } while (ans->val.value.boolean);

    freeList(body.value.list);
    freeList(condition.value.list);
}

void builtin_whileDo (RunEnv *env) {
    List *args = getArgs(env, 2, (int[]){LIST, LIST});
    if(args == NULL) {
        fprintf(stderr, "wrong arguments for whileDo(List, List)\n");
        return;
    }

    Symbol condition = pop(&args);
    Symbol body = pop(&args);
 
    while (true) {
        eval(condition, env);
        List *ans = getArgs(env, 1, (int[]){BOOLEAN});
        if(ans == NULL) {
            fprintf(stderr, "Wrong condition for whileDo()\n");
            printSymbol(env->stack->val);

            freeList(body.value.list);
            freeList(condition.value.list);
            return;
        }
        if(!pop(&ans).value.boolean) break;

        eval(body, env);
    }

    freeList(body.value.list);
    freeList(condition.value.list);
}

void builtin_content (RunEnv *env) {
    verifyArg(env->stack, ".");

    Symbol s = pop(&(env->stack));

    const char *opar = "( ", *cpar = " )";

    if(s.type == LIST) {
        printSymbol(s);
        freeList(s.value.list);
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
        free_StringArray(arr);
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

void builtin_load (RunEnv *env) {
    verifyArg(env->stack, "load()");

    Symbol s = implicitMap(env, &builtin_load);
    if(s.type == LIST) {
        env->stack = cons(s, env->stack);
    } else if(s.type == ITSELF) {
        String str = s.word;
        char buff[str.len+1];
        buff[str.len] = 0;
        strncpy(buff, str.data, str.len);
        env->stack = cons((Symbol){.word =str,
                            .type = SOURCE,
                            .value.source = load_file(buff)},
                    env->stack);
    } else if (s.type == STRING) {
        String str = s.word;
        char buff[str.len+1];
        buff[str.len] = 0;
        strncpy(buff, str.data, str.len);
        env->stack = cons((Symbol){.word = str,
                               .type = SOURCE,
                               .value.source = load_file(buff)},
                      env->stack);
    } else env->stack = cons(Nothing, env->stack);
}

void builtin_cut (RunEnv *env) {
    String      srcstr;
    StringArray seps;

    List *args = getArgs(env, 2, (int[]){ ARRAY, STRING });
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
                pushStr(&(env->stack), str);
                String s = {.data = srcstr.data,
                            .len = i};
                pushStr(&(env->stack), s);
                free_StringArray(seps);
                return;
            }
        } 
    }

    free_StringArray(seps);
    env->stack = cons((Symbol) {
                .word = constString(""),
                .type = NOTHING
            }, env->stack);
    pushStr(&(env->stack), srcstr);
}

void builtin_substr(RunEnv *env) {
    List *args = getArgs(env, 3, (int[]) { INT, INT, STRING });
    argsOrWarn(args);

    int end = pop(&args).value.integer;
    int start = pop(&args).value.integer;

    String str = args->val.value.string;

    args->next = env->stack;
    env->stack = consString(
        (String){.data = str.data+start,
                 .len = end - start },
        args);
}

List *varstash = NULL;
List *scopestash = NULL;
List *stackstash = NULL;
uint quot_depth = 0;

void builtin_isString(RunEnv *env) {
    if(env->stack == NULL) {
        consBool(false, env->stack);
        return;
    }
    int type = (env->stack)->val.type;
    if(type == NOTHING) {
        pop(&(env->stack));
        env->stack = consBool(false, env->stack);
    } else if (type == STRING) {
        env->stack = consBool(true, env->stack);
    } else {
        env->stack = consBool(false, env->stack);
    }
}

void builtin_unquote (RunEnv *env) {
    if(--quot_depth == 0) {
        env->globals = varstash;
        env->scopeStack = scopestash;
        env->stack = consList(stackstash, reverseList(env->stack));
    } else {
        env->stack = cons((Symbol) {
  /*(*/         .word = constString(")"), 
                .type = ITSELF},
                env->stack);
    }
}

void builtin_nested_quote (RunEnv *env) {
    quot_depth++;
    env->stack = cons((Symbol) {
                .word = constString("("), //)
                .type = ITSELF},
                env->stack);
}

void builtin_quote (RunEnv *env) {
    varstash = env->globals; 
    scopestash = env->scopeStack;
    stackstash = env->stack;
    quot_depth++;

    env->stack = NULL;
    env->scopeStack = NULL;
    env->globals = cons((Symbol) {
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
