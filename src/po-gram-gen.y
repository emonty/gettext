/* GNU gettext - internationalization aids
   Copyright (C) 1995, 1996, 1998, 2000, 2001 Free Software Foundation, Inc.

   This file was written by Peter Miller <pmiller@agso.gov.au>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

%{
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include "po-lex.h"
#include "po-gram.h"
#include "error.h"
#include "system.h"
#include "libgettext.h"
#include "po.h"

#define _(str) gettext (str)

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in the same program.  Note that these are only
   the variables produced by yacc.  If other parser generators (bison,
   byacc, etc) produce additional global names that conflict at link time,
   then those parser generators need to be fixed instead of adding those
   names to this list. */

#define yymaxdepth po_gram_maxdepth
#define yyparse po_gram_parse
#define yylex   po_gram_lex
#define yyerror po_gram_error
#define yylval  po_gram_lval
#define yychar  po_gram_char
#define yydebug po_gram_debug
#define yypact  po_gram_pact
#define yyr1    po_gram_r1
#define yyr2    po_gram_r2
#define yydef   po_gram_def
#define yychk   po_gram_chk
#define yypgo   po_gram_pgo
#define yyact   po_gram_act
#define yyexca  po_gram_exca
#define yyerrflag po_gram_errflag
#define yynerrs po_gram_nerrs
#define yyps    po_gram_ps
#define yypv    po_gram_pv
#define yys     po_gram_s
#define yy_yys  po_gram_yys
#define yystate po_gram_state
#define yytmp   po_gram_tmp
#define yyv     po_gram_v
#define yy_yyv  po_gram_yyv
#define yyval   po_gram_val
#define yylloc  po_gram_lloc
#define yyreds  po_gram_reds          /* With YYDEBUG defined */
#define yytoks  po_gram_toks          /* With YYDEBUG defined */
#define yylhs   po_gram_yylhs
#define yylen   po_gram_yylen
#define yydefred po_gram_yydefred
#define yydgoto po_gram_yydgoto
#define yysindex po_gram_yysindex
#define yyrindex po_gram_yyrindex
#define yygindex po_gram_yygindex
#define yytable  po_gram_yytable
#define yycheck  po_gram_yycheck

static long plural_counter;
%}

%token	COMMENT
%token	DOMAIN
%token	JUNK
%token	MSGID
%token	MSGID_PLURAL
%token	MSGSTR
%token	NAME
%token	'[' ']'
%token	NUMBER
%token	STRING

%union
{
  char *string;
  long number;
  lex_pos_ty pos;
  struct msgstr_def rhs;
}

%type <string> STRING COMMENT string_list msgid_pluralform
%type <number> NUMBER
%type <pos> msgid msgstr
%type <rhs> pluralform pluralform_list

%right MSGSTR

%%

msgfmt
	: /* empty */
	| msgfmt comment
	| msgfmt domain
	| msgfmt message
	| msgfmt error
	;

domain
	: DOMAIN STRING
		{
		   po_callback_domain ($2);
		}
	;

message
	: msgid string_list msgstr string_list
		{
		  po_callback_message ($2, &$1, NULL,
				       $4, strlen ($4) + 1, &$3);
		}
	| msgid string_list msgid_pluralform pluralform_list
		{
		  po_callback_message ($2, &$1, $3,
				       $4.msgstr, $4.msgstr_len, &$4.pos);
		}
	| msgid string_list msgid_pluralform
		{
		  po_gram_error_at_line (&$1, _("missing `msgstr[]' section"));
		  free ($2);
		  free ($3);
		}
	| msgid string_list pluralform_list
		{
		  po_gram_error_at_line (&$1, _("missing `msgid_plural' section"));
		  free ($2);
		  free ($3.msgstr);
		}
	| msgid string_list
		{
		  po_gram_error_at_line (&$1, _("missing `msgstr' section"));
		  free ($2);
		}
	;

msgid_pluralform
	: MSGID_PLURAL string_list
		{
		  plural_counter = 0;
		  $$ = $2;
		}
	;

pluralform_list
	: pluralform
		{
		  $$ = $1;
		}
	| pluralform_list pluralform
		{
		  $$.msgstr = (char *) xmalloc ($1.msgstr_len + $2.msgstr_len);
		  memcpy ($$.msgstr, $1.msgstr, $1.msgstr_len);
		  memcpy ($$.msgstr + $1.msgstr_len, $2.msgstr, $2.msgstr_len);
		  $$.msgstr_len = $1.msgstr_len + $2.msgstr_len;
		  $$.pos = $1.pos;
		  free ($1.msgstr);
		  free ($2.msgstr);
		}
	;

pluralform
	: msgstr '[' NUMBER ']' string_list
		{
		  if ($3 != plural_counter)
		    {
		      if (plural_counter == 0)
			po_gram_error_at_line (&$1, _("first plural form has nonzero index"));
		      else
			po_gram_error_at_line (&$1, _("plural form has wrong index"));
		    }
		  plural_counter++;
		  $$.msgstr = $5;
		  $$.msgstr_len = strlen ($5) + 1;
		  $$.pos = $1;
		}
	;

msgid
	: MSGID
		{
		  $$ = gram_pos;
		}
	;

msgstr
	: MSGSTR
		{
		  $$ = gram_pos;
		}
	;

string_list
	: STRING
		{
		  $$ = $1;
		}
	| string_list STRING
		{
		  size_t len1;
		  size_t len2;

		  len1 = strlen ($1);
		  len2 = strlen ($2);
		  $$ = (char *) xmalloc (len1 + len2 + 1);
		  stpcpy (stpcpy ($$, $1), $2);
		  free ($1);
		  free ($2);
		}
	;

comment
	: COMMENT
		{
		  po_callback_comment ($1);
		}
	;
