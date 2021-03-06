/*****************************************************************************\
 Wendy -- Calculating Operating Guidelines

 Copyright (C) 2009  Niels Lohmann <niels.lohmann@uni-rostock.de>

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


%token KEY_PLACES KEY_TRANSITIONS KEY_SYNCHRONOUS COMMA SEMICOLON NAME

%defines "syntax_cover.h"
%name-prefix "cover_"
%output "syntax_cover.cc"

%{
#include <vector>
#include <string>
#include "Cover.h"

/* Type of read labels:
 * 0 - places
 * 1 - transition
 * 2 - synchronous labels
 */
unsigned short labelType;

/// places to cover
std::vector<std::string> cover_placeNames;

/// transitions to cover
std::vector<std::string> cover_transitionNames;

extern int cover_lex();
extern int cover_error(const char *);
%}

%union {
  char* str;
}

%type <str> NAME

%%

cover:
  places transitions synchronous
  {
    Cover::initialize(cover_placeNames, cover_transitionNames);
    cover_placeNames.clear();
    cover_transitionNames.clear();
  }
;

places:
  /* empty */
| KEY_PLACES { labelType = 0; } names SEMICOLON
;

transitions:
  /* empty */
| KEY_TRANSITIONS { labelType = 1; } names SEMICOLON
;

synchronous:
  /* empty */
| KEY_SYNCHRONOUS { labelType = 2; } names SEMICOLON
;

names:
  /* empty */
| NAME
  {
    switch(labelType) {
        case 0: cover_placeNames.push_back($1); break;
        case 1: cover_transitionNames.push_back($1); break;
        case 2: Cover::synchronousLabels.push_back($1); break;
        default: /* ignore */ ;
    }
    free($1);
  }
| names COMMA NAME
  {
    switch(labelType) {
        case 0: cover_placeNames.push_back($3); break;
        case 1: cover_transitionNames.push_back($3); break;
        case 2: Cover::synchronousLabels.push_back($3); break;
        default: /* ignore */ ;
    }
    free($3);
  }
;
