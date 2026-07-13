#ifndef PLX_PARSER_HELPERS_H
#define PLX_PARSER_HELPERS_H

#include "plx_parser.h"



/*********************************************/
/*          PLXMAC PROLOG PARSING            */
/*********************************************/
int is_prolog_start(Line line);
int is_prolog_end(Line line);
char *prolog_content(Line line);



/*********************************************/
/*          PROC STATEMENTS NAMES            */
/*********************************************/
char *match_proc_start(Line line);
char *match_procentry(Line line);



/*********************************************/
/*               SIGNATURE                   */
/*********************************************/
/* Signature capture state: scan until ';' at paren depth 0, outside
 * comments and quoted strings. Comments are dropped from the signature. */
typedef struct {
    int depth;
    int inComment;
    int inString;
} SigState;

int sig_consume(StrBuf *sig, Line line, SigState *st);



/*********************************************/
/*               LABEL LOOKUP                */
/*********************************************/
typedef enum {
    FIELD_NONE = -1, /* not a labeled line */
    FIELD_NAME,
    FIELD_DESCRIPTION,
    FIELD_INPUT,
    FIELD_OUTPUT,
    FIELD_UNKNOWN /* labeled line, but the label is not recognized */
} FieldId;

FieldId parse_label(const char *content, const char **rest);



/*********************************************/
/*          COMMENT LINE HANDLING            */
/*********************************************/
char *comment_content(Line line);
int is_banner(const char *content);



/*********************************************/
/*          DOC BLOCK ACCUMULATOR            */
/*********************************************/
typedef struct {
    int active;      /* at least one recognized field collected */
    int closed;      /* end banner seen; awaiting the PROC statement */
    int prolog;      /* collected from a .plxmac Method Prolog block */
    FieldId current; /* field that continuation lines append to */
    int startLine;
    StrBuf name, description, output;
    StrList inputLines; /* input kept per line for param splitting */
} DocBlock;

void block_init(DocBlock *b);
void block_reset(DocBlock *b);
void block_append(DocBlock *b, FieldId field, const char *text);



/*********************************************/
/*        SYMBOL / MODULE CONSTRUCTION       */
/*********************************************/

/* Parameter under construction for the "Where <name> is:" input format. */
typedef struct {
    char *name;
    StrBuf desc;
} WhereParam;

char *match_where_clause(const char *line);

const char *strip_enum_marker(const char *s);

int where_param_add(
    WhereParam **arr,
    int *count,
    int *cap,
    char *name
);

void build_where_params(
    const StrList *lines,
    int firstWhere,
    Symbol *sym,
    const char *filename,
    int lineNo
);

void build_input_params(
    const StrList *lines,
    Symbol *sym,
    const char *filename,
    int lineNo
);

void block_to_symbol(
    DocBlock *b,
    Module *mod,
    const char *procName,
    const char *signature,
    int lineNo
);

void feed_doc_line(
    DocBlock *blk,
    Module *mod,
    const char *content,
    int lineNo
);


#endif