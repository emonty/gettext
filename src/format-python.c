/* Python format strings.
   Copyright (C) 2001 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "format.h"
#include "xmalloc.h"
#include "system.h"
#include "error.h"
#include "progname.h"
#include "libgettext.h"

#define _(str) gettext (str)

/* Python format strings are described in
     Python Library reference
     2. Built-in Types, Exceptions and Functions
     2.1. Built-in Types
     2.1.5. Sequence Types
     2.1.5.2. String Formatting Operations
   Any string or Unicode string can act as format string via the '%' operator,
   implemented in stringobject.c and unicodeobject.c.
   A directive
   - starts with '%'
   - is optionally followed by '(ident)' where ident is any sequence of
     characters with balanced left and right parentheses,
   - is optionally followed by any of the characters '-' (left justification),
     '+' (sign), ' ' (blank), '#' (alt), '0' (zero), each of which acts as a
     flag,
   - is optionally followed by a width specification: '*' (reads an argument)
     or a nonempty digit sequence,
   - is optionally followed by '.' and a precision specification: '*' (reads
     an argument) or a nonempty digit sequence,
   - is optionally followed by a size specifier, one of 'h' 'l' 'L'.
   - is finished by a specifier
       - '%', that needs no argument,
       - 'c', that needs a character argument,
       - 's', 'r', that need a string argument,
       - 'i', 'd', 'u', 'o', 'x', 'X', that need an integer argument,
       - 'e', 'E', 'f', 'g', 'G', that need a floating-point argument.
   Use of '(ident)' and use of unnamed argument specifications are exclusive,
   because the first requires a mapping as argument, while the second requires
   a tuple as argument.
 */

enum format_arg_type
{
  FAT_NONE,
  FAT_ANY,
  FAT_CHARACTER,
  FAT_STRING,
  FAT_INTEGER,
  FAT_FLOAT
};

struct named_arg
{
  char *name;
  enum format_arg_type type;
};

struct unnamed_arg
{
  enum format_arg_type type;
};

struct spec
{
  unsigned int directives;
  unsigned int named_arg_count;
  unsigned int unnamed_arg_count;
  unsigned int allocated;
  struct named_arg *named;
  struct unnamed_arg *unnamed;
};

/* Locale independent test for a decimal digit.
   Argument can be  'char' or 'unsigned char'.  (Whereas the argument of
   <ctype.h> isdigit must be an 'unsigned char'.)  */
#undef isdigit
#define isdigit(c) ((unsigned int) ((c) - '0') < 10)


/* Prototypes for local functions.  Needed to ensure compiler checking of
   function argument counts despite of K&R C function definition syntax.  */
static int named_arg_compare PARAMS ((const void *p1, const void *p2));
static void *format_parse PARAMS ((const char *format));
static void format_free PARAMS ((void *descr));
static int format_get_number_of_directives PARAMS ((void *descr));
static bool format_check PARAMS ((const lex_pos_ty *pos,
				  void *msgid_descr, void *msgstr_descr,
				  bool noisy));


static int
named_arg_compare (p1, p2)
     const void *p1;
     const void *p2;
{
  return strcmp (((const struct named_arg *) p1)->name,
		 ((const struct named_arg *) p2)->name);
}

static void *
format_parse (format)
     const char *format;
{
  struct spec spec;
  struct spec *result;

  spec.directives = 0;
  spec.named_arg_count = 0;
  spec.unnamed_arg_count = 0;
  spec.allocated = 0;
  spec.named = NULL;
  spec.unnamed = NULL;

  for (; *format != '\0';)
    if (*format++ == '%')
      {
	/* A directive.  */
	char *name = NULL;
	enum format_arg_type type;

	spec.directives++;

	if (*format == '(')
	  {
	    unsigned int depth;
	    const char *name_start;
	    const char *name_end;
	    size_t n;

	    name_start = ++format;
	    depth = 0;
	    for (; *format != '\0'; format++)
	      {
		if (*format == '(')
		  depth++;
		else if (*format == ')')
		  {
		    if (depth == 0)
		      break;
		    else
		      depth--;
		  }
	      }
	    if (*format == '\0')
	      goto bad_format;
	    name_end = format++;

	    n = name_end - name_start;
	    name = (char *) xmalloc (n + 1);
	    memcpy (name, name_start, n);
	    name[n] = '\0';
	  }

	while (*format == '-' || *format == '+' || *format == ' '
	       || *format == '#' || *format == '0')
	  format++;

	if (*format == '*')
	  {
	    format++;

	    /* Named and unnamed specifications are exclusive.  */
	    if (spec.named_arg_count > 0)
	      goto bad_format;

	    if (spec.allocated == spec.unnamed_arg_count)
	      {
		spec.allocated = 2 * spec.allocated + 1;
		spec.unnamed = (struct unnamed_arg *) xrealloc (spec.unnamed, spec.allocated * sizeof (struct unnamed_arg));
	      }
	    spec.unnamed[spec.unnamed_arg_count].type = FAT_INTEGER;
	    spec.unnamed_arg_count++;
	  }
	else if (isdigit (*format))
	  {
	    do format++; while (isdigit (*format));
	  }

	if (*format == '.')
	  {
	    format++;

	    if (*format == '*')
	      {
		format++;

		/* Named and unnamed specifications are exclusive.  */
		if (spec.named_arg_count > 0)
		  goto bad_format;

		if (spec.allocated == spec.unnamed_arg_count)
		  {
		    spec.allocated = 2 * spec.allocated + 1;
		    spec.unnamed = (struct unnamed_arg *) xrealloc (spec.unnamed, spec.allocated * sizeof (struct unnamed_arg));
		  }
		spec.unnamed[spec.unnamed_arg_count].type = FAT_INTEGER;
		spec.unnamed_arg_count++;
	      }
	    else if (isdigit (*format))
	      {
		do format++; while (isdigit (*format));
	      }
	  }

	if (*format == 'h' || *format == 'l' || *format == 'L')
	  format++;

	switch (*format)
	  {
	  case '%':
	    type = FAT_ANY;
	    break;
	  case 'c':
	    type = FAT_CHARACTER;
	    break;
	  case 's': case 'r':
	    type = FAT_STRING;
	    break;
	  case 'i': case 'd': case 'u': case 'o': case 'x': case 'X':
	    type = FAT_INTEGER;
	    break;
	  case 'e': case 'E': case 'f': case 'g': case 'G':
	    type = FAT_FLOAT;
	    break;
	  default:
	    goto bad_format;
	  }

	if (name != NULL)
	  {
	    /* Named argument.  */

	    /* Named and unnamed specifications are exclusive.  */
	    if (spec.unnamed_arg_count > 0)
	      goto bad_format;

	    if (spec.allocated == spec.named_arg_count)
	      {
		spec.allocated = 2 * spec.allocated + 1;
		spec.named = (struct named_arg *) xrealloc (spec.named, spec.allocated * sizeof (struct named_arg));
	      }
	    spec.named[spec.named_arg_count].name = name;
	    spec.named[spec.named_arg_count].type = type;
	    spec.named_arg_count++;
	  }
	else if (*format != '%')
	  {
	    /* Unnamed argument.  */

	    /* Named and unnamed specifications are exclusive.  */
	    if (spec.named_arg_count > 0)
	      goto bad_format;

	    if (spec.allocated == spec.unnamed_arg_count)
	      {
		spec.allocated = 2 * spec.allocated + 1;
		spec.unnamed = (struct unnamed_arg *) xrealloc (spec.unnamed, spec.allocated * sizeof (struct unnamed_arg));
	      }
	    spec.unnamed[spec.unnamed_arg_count].type = type;
	    spec.unnamed_arg_count++;
	  }

	format++;
      }

  /* Sort the named argument array, and eliminate duplicates.  */
  if (spec.named_arg_count > 1)
    {
      unsigned int i, j;
      bool err;

      qsort (spec.named, spec.named_arg_count, sizeof (struct named_arg),
	     named_arg_compare);

      /* Remove duplicates: Copy from i to j, keeping 0 <= j <= i.  */
      err = false;
      for (i = j = 0; i < spec.named_arg_count; i++)
	if (j > 0 && strcmp (spec.named[i].name, spec.named[j-1].name) == 0)
	  {
	    enum format_arg_type type1 = spec.named[i].type;
	    enum format_arg_type type2 = spec.named[j-1].type;
	    enum format_arg_type type_both;

	    if (type1 == type2 || type2 == FAT_ANY)
	      type_both = type1;
	    else if (type1 == FAT_ANY)
	      type_both = type2;
	    else
	      /* Incompatible types.  */
	      type_both = FAT_NONE, err = true;

	    spec.named[j-1].type = type_both;
	    free (spec.named[i].name);
	  }
	else
	  {
	    if (j < i)
	      {
		spec.named[j].name = spec.named[i].name;
		spec.named[j].type = spec.named[i].type;
	      }
	    j++;
	  }
      spec.named_arg_count = j;
      if (err)
	goto bad_format;
    }

  result = (struct spec *) xmalloc (sizeof (struct spec));
  *result = spec;
  return result;

 bad_format:
  if (spec.named != NULL)
    {
      unsigned int i;
      for (i = 0; i < spec.named_arg_count; i++)
	free (spec.named[i].name);
      free (spec.named);
    }
  if (spec.unnamed != NULL)
    free (spec.unnamed);
  return NULL;
}

static void
format_free (descr)
     void *descr;
{
  struct spec *spec = (struct spec *) descr;

  if (spec->named != NULL)
    {
      unsigned int i;
      for (i = 0; i < spec->named_arg_count; i++)
	free (spec->named[i].name);
      free (spec->named);
    }
  if (spec->unnamed != NULL)
    free (spec->unnamed);
  free (spec);
}

static int
format_get_number_of_directives (descr)
     void *descr;
{
  struct spec *spec = (struct spec *) descr;

  return spec->directives;
}

static bool
format_check (pos, msgid_descr, msgstr_descr, noisy)
     const lex_pos_ty *pos;
     void *msgid_descr;
     void *msgstr_descr;
     bool noisy;
{
  struct spec *spec1 = (struct spec *) msgid_descr;
  struct spec *spec2 = (struct spec *) msgstr_descr;
  bool err = false;

  if (spec1->named_arg_count > 0 && spec2->unnamed_arg_count > 0)
    {
      if (noisy)
	{
	  error_with_progname = false;
	  error_at_line (0, 0, pos->file_name, pos->line_number,
			 _("format specifications in 'msgid' expect a mapping, those in 'msgstr' expect a tuple"));
	  error_with_progname = true;
	}
      err = true;
    }
  else if (spec1->unnamed_arg_count > 0 && spec2->named_arg_count > 0)
    {
      if (noisy)
	{
	  error_with_progname = false;
	  error_at_line (0, 0, pos->file_name, pos->line_number,
			 _("format specifications in 'msgid' expect a tuple, those in 'msgstr' expect a mapping"));
	  error_with_progname = true;
	}
      err = true;
    }
  else
    {
      if (spec1->named_arg_count + spec2->named_arg_count > 0)
	{
	  unsigned int i;
	  unsigned int n = MAX (spec1->named_arg_count, spec2->named_arg_count);

	  /* Check the argument names are the same.
	     Both arrays are sorted.  We search for the first difference.  */
	  for (i = 0; i < n; i++)
	    {
	      int cmp = (i >= spec1->named_arg_count ? 1 :
			 i >= spec2->named_arg_count ? -1 :
			 strcmp (spec1->named[i].name, spec2->named[i].name));

	      if (cmp > 0)
		{
		  if (noisy)
		    {
		      error_with_progname = false;
		      error_at_line (0, 0, pos->file_name, pos->line_number,
				     _("a format specification for argument '%s' doesn't exist in 'msgid'"),
				     spec2->named[i].name);
		      error_with_progname = true;
		    }
		  err = true;
		  break;
		}
	      else if (cmp < 0)
		{
		  if (noisy)
		    {
		      error_with_progname = false;
		      error_at_line (0, 0, pos->file_name, pos->line_number,
				     _("a format specification for argument '%s' doesn't exist in 'msgstr'"),
				     spec1->named[i].name);
		      error_with_progname = true;
		    }
		  err = true;
		  break;
		}
	    }
	  /* Check the argument types are the same.  */
	  if (!err)
	    for (i = 0; i < spec2->named_arg_count; i++)
	      if (spec1->named[i].type != spec2->named[i].type)
		{
		  if (noisy)
		    {
		      error_with_progname = false;
		      error_at_line (0, 0, pos->file_name, pos->line_number,
				     _("format specifications in 'msgid' and 'msgstr' for argument '%s' are not the same"),
				     spec2->named[i].name);
		      error_with_progname = true;
		    }
		  err = true;
		  break;
		}
	}

      if (spec1->unnamed_arg_count + spec2->unnamed_arg_count > 0)
	{
	  unsigned int i;

	  /* Check the argument types are the same.  */
	  if (spec1->unnamed_arg_count != spec2->unnamed_arg_count)
	    {
	      if (noisy)
		{
		  error_with_progname = false;
		  error_at_line (0, 0, pos->file_name, pos->line_number,
				 _("number of format specifications in 'msgid' and 'msgstr' does not match"));
		  error_with_progname = true;
		}
	      err = true;
	    }
	  else
	    for (i = 0; i < spec1->unnamed_arg_count; i++)
	      if (spec1->unnamed[i].type != spec2->unnamed[i].type)
		{
		  if (noisy)
		    {
		      error_with_progname = false;
		      error_at_line (0, 0, pos->file_name, pos->line_number,
				     _("format specifications in 'msgid' and 'msgstr' for argument %u are not the same"),
				     i + 1);
		      error_with_progname = true;
		    }
		  err = true;
		}
	}
    }

  return err;
}


struct formatstring_parser formatstring_python =
{
  format_parse,
  format_free,
  format_get_number_of_directives,
  format_check
};


#ifdef TEST

/* Test program: Print the argument list specification returned by
   format_parse for strings read from standard input.  */

#include <stdio.h>
#include "getline.h"

static void
format_print (descr)
     void *descr;
{
  struct spec *spec = (struct spec *) descr;
  unsigned int i;

  if (spec == NULL)
    {
      printf ("INVALID");
      return;
    }

  if (spec->named_arg_count > 0)
    {
      if (spec->unnamed_arg_count > 0)
	abort ();

      printf ("{");
      for (i = 0; i < spec->named_arg_count; i++)
	{
	  if (i > 0)
	    printf (", ");
	  printf ("'%s':", spec->named[i].name);
	  switch (spec->named[i].type)
	    {
	    case FAT_ANY:
	      printf ("*");
	      break;
	    case FAT_CHARACTER:
	      printf ("c");
	      break;
	    case FAT_STRING:
	      printf ("s");
	      break;
	    case FAT_INTEGER:
	      printf ("i");
	      break;
	    case FAT_FLOAT:
	      printf ("f");
	      break;
	    default:
	      abort ();
	    }
	}
      printf ("}");
    }
  else
    {
      printf ("(");
      for (i = 0; i < spec->unnamed_arg_count; i++)
	{
	  if (i > 0)
	    printf (" ");
	  switch (spec->unnamed[i].type)
	    {
	    case FAT_ANY:
	      printf ("*");
	      break;
	    case FAT_CHARACTER:
	      printf ("c");
	      break;
	    case FAT_STRING:
	      printf ("s");
	      break;
	    case FAT_INTEGER:
	      printf ("i");
	      break;
	    case FAT_FLOAT:
	      printf ("f");
	      break;
	    default:
	      abort ();
	    }
	}
      printf (")");
    }
}

int
main ()
{
  for (;;)
    {
      char *line = NULL;
      size_t line_len = 0;
      void *descr;

      if (getline (&line, &line_len, stdin) < 0)
	break;

      descr = format_parse (line);

      format_print (descr);
      printf ("\n");

      free (line);
    }

  return 0;
}

/*
 * For Emacs M-x compile
 * Local Variables:
 * compile-command: "/bin/sh ../libtool --mode=link gcc -o a.out -static -O -g -Wall -I.. -I../lib -I../intl -DHAVE_CONFIG_H -DTEST format-python.c ../lib/libgettextlib.la"
 * End:
 */

#endif /* TEST */
