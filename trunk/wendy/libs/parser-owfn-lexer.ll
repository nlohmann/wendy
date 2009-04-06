 // -*- C++ -*-

 /***************************************************************************** 
  * flex options 
  ****************************************************************************/

/* created lexer should be called "lex.yy.c" to make the ylwrap script work */
%option outfile="lex.yy.c"

/* plain c scanner: the prefix is our "namespace" */
%option prefix="pnapi_owfn_"

/* we read only one file */
%option noyywrap

/* yyunput not needed (fix compiler warning) */
%option nounput

/* maintain line number for error reporting */
%option yylineno


 /***************************************************************************** 
  * C declarations 
  ****************************************************************************/
%{

#include "parser.h"
#include "parser-owfn.h"

#define yystream pnapi::parser::stream
#define yylineno pnapi::parser::line
#define yytext   pnapi::parser::token
#define yyerror  pnapi::parser::error

#define yylex    pnapi::parser::owfn::lex

/* hack to read input from a C++ stream */
#define YY_INPUT(buf,result,max_size)		\
   yystream->read(buf, max_size); \
   if (yystream->bad()) \
     YY_FATAL_ERROR("input in flex scanner failed"); \
   result = yystream->gcount();

/* hack to overwrite YY_FATAL_ERROR behavior */
#define fprintf(file,fmt,msg) \
   yyerror(msg);

%}


 /*****************************************************************************
  * regular expressions 
  ****************************************************************************/

 /* a start condition to skip comments */
%s COMMENT

%%

 /* control comments */ 
"{$"                            { return LCONTROL; }
"$}"                            { return RCONTROL; }

 /* comments */
"{"                             { BEGIN(COMMENT); }
<COMMENT>"}"                    { BEGIN(INITIAL); }
<COMMENT>[^}]*                  { /* skip */ }

 /* control keywords */
MAX_UNIQUE_EVENTS               { return KEY_MAX_UNIQUE_EVENTS; }
ON_LOOP                         { return KEY_ON_LOOP; }
MAX_OCCURRENCES                 { return KEY_MAX_OCCURRENCES; }
TRUE                            { return KEY_TRUE; }
FALSE                           { return KEY_FALSE; }

 /* keywords */
SAFE                            { return KEY_SAFE; }
PLACE                           { return KEY_PLACE; }
INTERFACE                       { return KEY_INTERFACE; }
INTERNAL                        { return KEY_INTERNAL; }
INPUT                           { return KEY_INPUT; }
OUTPUT                          { return KEY_OUTPUT; }
TRANSITION                      { return KEY_TRANSITION; }
INITIALMARKING                  { return KEY_MARKING; }
FINALMARKING                    { return KEY_FINALMARKING; }
NOFINALMARKING                  { return KEY_NOFINALMARKING; }
FINALCONDITION                  { return KEY_FINALCONDITION; }
CONSUME                         { return KEY_CONSUME; }
PRODUCE                         { return KEY_PRODUCE; }
PORT                            { return KEY_PORT; }
PORTS                           { return KEY_PORTS; }
SYNCHRONOUS                     { return KEY_SYNCHRONOUS; }
SYNCHRONIZE                     { return KEY_SYNCHRONIZE; }
CONSTRAIN                       { return KEY_CONSTRAIN; }

 /* keywords for final conditions */
ALL_PLACES_EMPTY                { return KEY_ALL_PLACES_EMPTY; }
ALL_OTHER_PLACES_EMPTY          { return KEY_ALL_OTHER_PLACES_EMPTY; }
ALL_OTHER_INTERNAL_PLACES_EMPTY { return KEY_ALL_OTHER_INTERNAL_PLACES_EMPTY; }
ALL_OTHER_EXTERNAL_PLACES_EMPTY { return KEY_ALL_OTHER_EXTERNAL_PLACES_EMPTY; }
AND                             { return OP_AND; }
OR                              { return OP_OR; }
NOT                             { return OP_NOT; }
\>                              { return OP_GT; }
\<                              { return OP_LT; }
\>=                             { return OP_GE; }
\<=                             { return OP_LE; }
=                               { return OP_EQ; }
\<\>                            { return OP_NE; }
\#                              { return OP_NE; }
\(                              { return LPAR; }
\)                              { return RPAR; }

 /* other characters */
\:                              { return COLON; }
\;                              { return SEMICOLON; }
,                               { return COMMA; }

 /* identifiers */
[0-9][0-9]*                     { 
            pnapi_owfn_lval.yt_int = atoi(yytext); return NUMBER; }
"-"[0-9][0-9]*                  { 
            pnapi_owfn_lval.yt_int = atoi(yytext); return NEGATIVE_NUMBER; }
[^,;:()\t \n\r\{\}=][^,;:()\t \n\r\{\}=]* { 
            pnapi_owfn_lval.yt_string = new std::string(yytext); return IDENT; }

 /* whitespace */
[\n\r]                          { /* skip */ }
[ \t]                           { /* skip */ }

 /* anything else */
.                               { yyerror("unexpected lexical token"); }
