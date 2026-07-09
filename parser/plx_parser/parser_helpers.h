#ifndef PLX_PARSER_HELPERS_H
#define PLX_PARSER_HELPERS_H

#include "plx_parser.h"



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
