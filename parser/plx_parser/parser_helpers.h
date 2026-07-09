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


#endif
