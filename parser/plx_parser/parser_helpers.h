#ifndef PLX_PARSER_HELPERS_H
#define PLX_PARSER_HELPERS_H

#include "plx_parser.h"



/*********************************************/
/*          PLXMAC PROLOG PARSING            */
/*********************************************/
int is_prolog_start(const char *line);
int is_prolog_end(const char *line);
char *prolog_content(const char *line);



/*********************************************/
/*          PROC STATEMENTS NAMES            */
/*********************************************/
char *match_proc_start(const char *line);
char *match_procentry(const char *line);



/*********************************************/
/*               SIGNATURE                   */
/*********************************************/
int sig_consume(StrBuf *sig, const char *line, SigState *st);



/*********************************************/
/*               LABEL LOOKUP                */
/*********************************************/
FieldId parse_label(const char *content, const char **rest);



/*********************************************/
/*          COMMENT LINE HANDLING            */
/*********************************************/
char *comment_content(const char *line);
int is_banner(const char *content);



/*********************************************/
/*          DOC BLOCK ACCUMULATOR            */
/*********************************************/
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