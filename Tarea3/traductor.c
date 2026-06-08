/* traductor.c - JSON simplificado -> XML
 * Compilar: gcc -Wall -Wextra -O2 traductor.c -o traductor
 * Uso: traductor.exe fuente.txt   o   traductor.exe archivo.json
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*======================
  Lexer
======================*/
typedef enum {
    T_LBRACE, T_RBRACE, T_LBRACKET, T_RBRACKET, T_COMMA, T_COLON,
    T_STRING, T_NUMBER, T_TRUE, T_FALSE, T_NULL,
    T_EOF, T_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *lex;      /* para STRING/NUMBER guardamos el lexema */
    int line;
    int col;
} Token;

typedef struct {
    FILE *f;
    int ch;
    int line, col;
} Scanner;

static void sc_init(Scanner *sc, FILE *f){
    sc->f = f; sc->ch = fgetc(f);
    sc->line = 1; sc->col = 1;
}
static void sc_advance(Scanner *sc){
    if (sc->ch == '\n') { sc->line++; sc->col = 1; }
    else sc->col++;
    sc->ch = fgetc(sc->f);
}
static void *xmalloc(size_t n){
    void *p = malloc(n);
    if(!p){ fprintf(stderr,"Out of memory\n"); exit(1); }
    return p;
}
static void skip_ws(Scanner *sc){
    while (sc->ch==' ' || sc->ch=='\t' || sc->ch=='\r' || sc->ch=='\n') sc_advance(sc);
}
static int is_num_start(int c){ return isdigit(c); }

static Token make_simple(TokenType t, int line, int col){
    Token tk; tk.type=t; tk.lex=NULL; tk.line=line; tk.col=col; return tk;
}

/*--- STRING ---*/
static Token lex_string(Scanner *sc){
    int line = sc->line, col = sc->col;
    sc_advance(sc);
    char *buf = NULL; size_t cap=0, len=0;
    #define PUSH(c) do{ if(len+1>=cap){ cap=cap?cap*2:64; buf=realloc(buf,cap); if(!buf){fprintf(stderr,"OOM\n");exit(1);} } buf[len++]=(char)(c); }while(0)
    int closed = 0;
    while (sc->ch != EOF){
        if (sc->ch == '"'){ sc_advance(sc); closed=1; break; }
        if (sc->ch == '\\'){
            sc_advance(sc);
            if (sc->ch==EOF) break;
            int out = sc->ch;
            if (out=='n') out='\n';
            else if (out=='t') out='\t';
            else if (out=='r') out='\r';
            PUSH(out);
            sc_advance(sc);
        } else {
            PUSH(sc->ch);
            sc_advance(sc);
        }
    }
    PUSH('\0');
    Token tk;
    if (!closed){
        tk.type=T_ERROR; tk.lex=strdup("Unterminated string"); tk.line=line; tk.col=col;
    } else {
        tk.type=T_STRING; tk.lex=buf; tk.line=line; tk.col=col;
    }
    return tk;
}

/*--- NUMBER ---*/
static Token lex_number(Scanner *sc){
    int line=sc->line, col=sc->col;
    char tmp[256]; size_t n=0;
    while (isdigit(sc->ch)){ if(n<255) tmp[n++]=(char)sc->ch; sc_advance(sc); }
    if (sc->ch=='.'){ if(n<255) tmp[n++]='.'; sc_advance(sc);
        while (isdigit(sc->ch)){ if(n<255) tmp[n++]=(char)sc->ch; sc_advance(sc); }
    }
    if (sc->ch=='e' || sc->ch=='E'){
        if(n<255) tmp[n++]=(char)sc->ch; sc_advance(sc);
        if (sc->ch=='+' || sc->ch=='-'){ if(n<255) tmp[n++]=(char)sc->ch; sc_advance(sc); }
        while (isdigit(sc->ch)){ if(n<255) tmp[n++]=(char)sc->ch; sc_advance(sc); }
    }
    tmp[n]='\0';
    Token tk; tk.type=T_NUMBER; tk.lex=strdup(tmp); tk.line=line; tk.col=col; return tk;
}

/*--- PALABRAS CLAVE ---*/
static int match_kw(const char *s, const char *kw){
    for(; *s && *kw; ++s,++kw){
        if (tolower((unsigned char)*s) != tolower((unsigned char)*kw)) return 0;
    }
    return *s=='\0' && *kw=='\0';
}

/*--- TOKENIZER PRINCIPAL ---*/
static Token next_token(Scanner *sc){
    skip_ws(sc);
    int line=sc->line, col=sc->col;
    if (sc->ch==EOF) return make_simple(T_EOF, line, col);
    int c = sc->ch;
    switch (c){
        case '{': sc_advance(sc); return make_simple(T_LBRACE,line,col);
        case '}': sc_advance(sc); return make_simple(T_RBRACE,line,col);
        case '[': sc_advance(sc); return make_simple(T_LBRACKET,line,col);
        case ']': sc_advance(sc); return make_simple(T_RBRACKET,line,col);
        case ',': sc_advance(sc); return make_simple(T_COMMA,line,col);
        case ':': sc_advance(sc); return make_simple(T_COLON,line,col);
        case '"': return lex_string(sc);
        default:
            if (is_num_start(c)) return lex_number(sc);
            if (isalpha(c)){
                char buf[16]; size_t n=0;
                while (isalpha(sc->ch) && n<15){ buf[n++]=(char)sc->ch; sc_advance(sc); }
                buf[n]='\0';
                Token tk; tk.lex=strdup(buf); tk.line=line; tk.col=col;
                if (match_kw(buf,"true"))  { tk.type=T_TRUE;  return tk; }
                if (match_kw(buf,"false")) { tk.type=T_FALSE; return tk; }
                if (match_kw(buf,"null"))  { tk.type=T_NULL;  return tk; }
                tk.type=T_ERROR; return tk;
            }
            sc_advance(sc);
            Token tk; tk.type=T_ERROR; tk.lex=strdup("BadChar"); tk.line=line; tk.col=col; return tk;
    }
}

/*======================
  Parser + XML
======================*/
typedef struct {
    Scanner sc;
    Token la;      /* lookahead */
    FILE *out;
    int errors;
} Parser;

/* utilidades */
static void token_free(Token *t){ if (t->lex) free(t->lex); t->lex=NULL; }
static void advance(Parser *p){ token_free(&p->la); p->la = next_token(&p->sc); }
static const char* tname(TokenType t){
    switch(t){
        case T_LBRACE: return "'{'";
        case T_RBRACE: return "'}'";
        case T_LBRACKET: return "'['";
        case T_RBRACKET: return "']'";
        case T_COMMA: return "','";
        case T_COLON: return "':'";
        case T_STRING: return "string";
        case T_NUMBER: return "number";
        case T_TRUE:   return "true";
        case T_FALSE:  return "false";
        case T_NULL:   return "null";
        case T_EOF:    return "EOF";
        default:       return "token";
    }
}
static void report(Parser *p, const char* msg, const char* expect){
    fprintf(stderr,"[Linea %d, Col %d] %s", p->la.line, p->la.col, msg);
    if (expect) fprintf(stderr, " (esperaba %s, encontro %s)", expect, tname(p->la.type));
    fprintf(stderr,"\n");
    p->errors++;
}

/* sincronizaciones */
static void sync_until(Parser *p, int (*stop)(TokenType)){
    while (p->la.type != T_EOF && !stop(p->la.type)) advance(p);
}
static int stop_attr(TokenType t){ return t==T_COMMA || t==T_RBRACE || t==T_EOF; }
static int stop_elem(TokenType t){ return t==T_COMMA || t==T_RBRACKET || t==T_EOF; }

/* XML helpers */
static void xml_text(FILE *f, const char *s){
    for (; *s; ++s){
        if (*s=='&') fputs("&amp;", f);
        else if (*s=='<') fputs("&lt;", f);
        else if (*s=='>') fputs("&gt;", f);
        else fputc(*s, f);
    }
}
static char* dup_tag_from_string_lex(const char *str){
    char *tag = (char*)xmalloc(strlen(str)+1);
    strcpy(tag, str);
    for (char *p=tag; *p; ++p) if (*p==' ') *p='_';
    return tag;
}

/* forward decls */
static void element(Parser *p, const char *tag);
static void object(Parser *p, const char *tag);
static void array(Parser *p, const char *tag);
static void attribute(Parser *p);
static int match(Parser *p, TokenType t);

/* match con error suave */
static int match(Parser *p, TokenType t){
    if (p->la.type == t){ advance(p); return 1; }
    report(p, "Sintaxis invalida", tname(t));
    return 0;
}

/* json -> element EOF */
static void json(Parser *p){
    element(p, NULL);
    if (p->la.type != T_EOF)
        report(p, "Tokens despues del final del JSON", "EOF");
}

/* element -> object | array | string | number | true | false | null */
static void element(Parser *p, const char *tag){
    switch (p->la.type){
        case T_LBRACE: object(p, tag); break;
        case T_LBRACKET: array(p, tag); break;
        case T_STRING:
        case T_NUMBER:
        case T_TRUE:
        case T_FALSE:
        case T_NULL: {
            const char *used = tag ? tag : "value";
            fprintf(p->out, "<%s>", used);
            const char *txt =
                (p->la.type==T_STRING || p->la.type==T_NUMBER)
                ? p->la.lex
                : (p->la.type==T_TRUE ? "true" : (p->la.type==T_FALSE ? "false" : "null"));
            xml_text(p->out, txt ? txt : "");
            fprintf(p->out, "</%s>", used);
            advance(p);
            break;
        }
        default:
            report(p, "Se esperaba un elemento", "objeto/array/valor");
            sync_until(p, stop_elem);
            break;
    }
}

/* object -> { attributes-list } | {} */
static void object(Parser *p, const char *tag){
    int opened = 0;
    if (!match(p, T_LBRACE)) return;
    if (tag){ fprintf(p->out, "<%s>", tag); opened=1; }
    if (p->la.type == T_RBRACE){ advance(p); if (opened) fprintf(p->out, "</%s>", tag); return; }
    attribute(p);
    while (p->la.type == T_COMMA){ advance(p); attribute(p); }
    if (!match(p, T_RBRACE)){
        sync_until(p, stop_attr);
        if (p->la.type==T_RBRACE) advance(p);
    }
    if (opened) fprintf(p->out, "</%s>", tag);
}

/* attribute -> "name" : value */
static void attribute(Parser *p){
    if (p->la.type != T_STRING){
        report(p, "Nombre de atributo invalido", "string");
        sync_until(p, stop_attr);
        return;
    }
    char *tag = dup_tag_from_string_lex(p->la.lex ? p->la.lex : "attr");
    advance(p);
    if (!match(p, T_COLON)){ sync_until(p, stop_attr); free(tag); return; }
    element(p, tag);
    free(tag);
}

/* array -> [element-list] | [] */
static void array(Parser *p, const char *tag){
    int opened = 0;
    if (!match(p, T_LBRACKET)) return;
    if (tag){ fprintf(p->out, "<%s>", tag); opened=1; }
    if (p->la.type == T_RBRACKET){ advance(p); if (opened) fprintf(p->out, "</%s>", tag); return; }
    fprintf(p->out, "<item>");
    element(p, NULL);
    fprintf(p->out, "</item>");
    while (p->la.type == T_COMMA){
        advance(p);
        fprintf(p->out, "<item>");
        element(p, NULL);
        fprintf(p->out, "</item>");
    }
    if (!match(p, T_RBRACKET)){
        sync_until(p, stop_elem);
        if (p->la.type==T_RBRACKET) advance(p);
    }
    if (opened) fprintf(p->out, "</%s>", tag);
}

/*======================
  Main
======================*/
int main(int argc, char **argv){
    const char *inpath = NULL;

    if (argc >= 2) {
        inpath = argv[1];
    } else {
        inpath = "fuente.txt";  /* usa fuente.txt si no se pasa nada */
        fprintf(stderr, "Aviso: no se especificó archivo. Usando '%s'.\n", inpath);
    }

    FILE *in = fopen(inpath, "rb");
    if (!in){
        fprintf(stderr, "No se puede abrir '%s'.\n", inpath);
        fprintf(stderr, "Uso: %s <archivo .json o .txt>\n", argv[0]);
        return 1;
    }

    FILE *out = fopen("output.xml", "wb");
    if (!out){
        fprintf(stderr, "No se puede abrir 'output.xml' para escribir\n");
        fclose(in);
        return 1;
    }

    Parser P;
    sc_init(&P.sc, in);
    P.la = next_token(&P.sc);
    P.out = out;
    P.errors = 0;

    json(&P);

    token_free(&P.la);
    fclose(in);
    fclose(out);

    if (P.errors==0){
        printf("Traduccion completada. Revisar output.xml\n");
        return 0;
    } else {
        printf("Traduccion completada con %d error(es). Revisar output.xml (salida parcial) y la consola.\n", P.errors);
        return 2;
    }
}
