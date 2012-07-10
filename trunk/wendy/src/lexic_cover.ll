/*****************************************************************************\
 Wendy -- Synthesizing Partners for Services

 Copyright (c) 2009 Niels Lohmann, Christian Sura, and Daniela Weinberg

 Wendy is free software: you can redistribute it and/or modify it under the
 terms of the GNU Affero General Public License as published by the Free
 Software Foundation, either version 3 of the License, or (at your option)
 any later version.

 Wendy is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
 more details.

 You should have received a copy of the GNU Affero General Public License
 along with Wendy.  If not, see <http://www.gnu.org/licenses/>. 
\*****************************************************************************/


%option noyywrap
%option yylineno
%option nodefault
%option nounput
%option outfile="lex.yy.c"
%option prefix="cover_"

%{
#include <cstring>
#include "syntax_cover.hh"
#include "verbose.h"


void cover_error(const char *);
%}

/* comments */
%s COMMENT

name      [^,;:()\t \n\{\}]+

%%

  /* comments */
"{"             { BEGIN(COMMENT);                       }
<COMMENT>"}"    { BEGIN(INITIAL);                       }
<COMMENT>[^}]*  { /* skip */                            }

"PLACES"        { return KEY_PLACES;                    }
"TRANSITIONS"   { return KEY_TRANSITIONS;               }
"SYNCHRONOUS"   { return KEY_SYNCHRONOUS;               }
","             { return COMMA;                         }
";"             { return SEMICOLON;                     }

{name}          { cover_lval.str = strdup(cover_text); return NAME; }
[ \t\r\n]*      { /* skip */                            }

.               { cover_error("lexical error");         }

%%

__attribute__((noreturn)) void cover_error(const char *msg) 
{
  status("%d: error near '%s': %s", cover_lineno, cover_text, msg);
  abort(15, "error while parsing the cover file");
}

