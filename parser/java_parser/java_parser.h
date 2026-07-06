/*
  java_parser.h is the data that represents a parsed Java language file 
  and the two functions to produce and free one. Implemented int java_parser.c.

  Module is comprised of Symbol[] which are its methods and constructors and Param[] which are the paramaters of the methods and constructors. 
*/

#ifndef JAVA_PARSER_H
#define JAVA_PARSER_H

#include <stddef.h>
#include <stdint.h>

// One parameter of a method
typedef struct {
    char *name;  // the parameter name
    char *description; // text from @param, NULL if undocumented 
} Param;

// One method or constructor 
typedef struct {
    char  *name; // the method or constructor name
    char  *signature; // combination of the method or constuctors name and parameter list
    char  *brief; // short summary from the comment
    char  *returns; // text from @return
    char  *diagram; // AI mode diagram
    char  *notes; //text from @throws general notes etc.
    Param *params; // the parameters
    size_t param_count; // number of the parameters
    uint32_t line; // 1-based line of the declaration
} Symbol;


//The file and the methods or constructors in it 
typedef struct {
    char *filename; // the name of the file 
    Symbol *symbols; // the methods
    size_t count;  // number of methods
    size_t cap; // capacity of the method or constructor array array
} Module;



/*
  Read the text of the Java file and returns a module listing the methods or constructors in it.
  The file is read and the text is passed as 'src' and the lenght as 'len' without the disk being touched.
  
  The return module makes copies of everything so you can free 'src' as it returns.
  
  The memory can be released by calling module_free()

*/
Module java_parse(const char *path, const char *src, size_t len);

// Free a module and its contents
void module_free(Module *m);

#endif