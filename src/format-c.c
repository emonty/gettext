/* C format strings.
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

#include "format.h"
#include "xmalloc.h"
#include "error.h"
#include "progname.h"
#include "libgettext.h"

#define _(str) gettext (str)

/* C format strings are described in POSIX (IEEE P1003.1 2001), section
   XSH 3 fprintf().  See also Linux fprintf(3) manual page.
   A directive
   - starts with '%' or '%m$' where m is a positive integer,
   - is optionally followed by any of the characters '#', '0', '-', ' ', '+',
     "'", each of which acts as a flag,
   - is optionally followed by a width specification: '*' (reads an argument)
     or '*m$' or a nonempty digit sequence,
   - is optionally followed by '.' and a precision specification: '*' (reads
     an argument) or '*m$' or a nonempty digit sequence,
   - is optionally followed by a size specifier, one of 'hh' 'h' 'l' 'll' 'L'
     'q' 'j' 'z' 't',
   - is finished by a specifier
       - '%', that needs no argument,
       - 'c', 'C', that need a character argument,
       - 's', 'S', that need a string argument,
       - 'i', 'd', that need a signed integer argument,
       - 'o', 'u', 'x', 'X', that need an unsigned integer argument,
       - 'e', 'E', 'f', 'F', 'g', 'G', 'a', 'A', that need a floating-point
         argument,
       - 'p', that needs a 'void *' argument,
       - 'n', that needs a pointer to integer.
   Numbered ('%m$' or '*m$') and unnumbered argument specifications cannot
   be used in the same string.
 */

enum format_arg_type
{
  FAT_NONE		= 0,
  /* Basic types */
  FAT_INTEGER		= 1,
  FAT_DOUBLE		= 2,
  FAT_CHAR		= 3,
  FAT_STRING		= 4,
  FAT_POINTER		= 5,
  FAT_COUNT_POINTER	= 6,
  /* Flags */
  FAT_UNSIGNED		= 1 << 3,
  FAT_SIZE_SHORT	= 1 << 4,
  FAT_SIZE_CHAR		= 2 << 4,
  FAT_SIZE_LONG		= 1 << 6,
  FAT_SIZE_LONGLONG	= 2 << 6,
  FAT_SIZE_INTMAX_T	= 1 << 8,
  FAT_SIZE_SIZE_T	= 1 << 9,
  FAT_SIZE_PTRDIFF_T	= 1 << 10,
  FAT_WIDE		= FAT_SIZE_LONG,
  /* Meaningful combinations of basic types and flags:
  'signed char'			= FAT_INTEGER | FAT_SIZE_CHAR,
  'unsigned char'		= FAT_INTEGER | FAT_SIZE_CHAR | FAT_UNSIGNED,
  'short'			= FAT_INTEGER | FAT_SIZE_SHORT,
  'unsigned short'		= FAT_INTEGER | FAT_SIZE_SHORT | FAT_UNSIGNED,
  'int'				= FAT_INTEGER,
  'unsigned int'		= FAT_INTEGER | FAT_UNSIGNED,
  'long int'			= FAT_INTEGER | FAT_SIZE_LONG,
  'unsigned long int'		= FAT_INTEGER | FAT_SIZE_LONG | FAT_UNSIGNED,
  'long long int'		= FAT_INTEGER | FAT_SIZE_LONGLONG,
  'unsigned long long int'	= FAT_INTEGER | FAT_SIZE_LONGLONG | FAT_UNSIGNED,
  'double'			= FAT_DOUBLE,
  'long double'			= FAT_DOUBLE | FAT_SIZE_LONGLONG,
  'char'/'int'			= FAT_CHAR,
  'wchar_t'/'wint_t'		= FAT_CHAR | FAT_SIZE_LONG,
  'const char *'		= FAT_STRING,
  'const wchar_t *'		= FAT_STRING | FAT_SIZE_LONG,
  'void *'			= FAT_POINTER,
  FAT_COUNT_SCHAR_POINTER	= FAT_COUNT_POINTER | FAT_SIZE_CHAR,
  FAT_COUNT_SHORT_POINTER	= FAT_COUNT_POINTER | FAT_SIZE_SHORT,
  FAT_COUNT_INT_POINTER		= FAT_COUNT_POINTER,
  FAT_COUNT_LONGINT_POINTER	= FAT_COUNT_POINTER | FAT_SIZE_LONG,
  FAT_COUNT_LONGLONGINT_POINTER	= FAT_COUNT_POINTER | FAT_SIZE_LONGLONG,
  */
  /* Bitmasks */
  FAT_SIZE_MASK		= (FAT_SIZE_SHORT | FAT_SIZE_CHAR \
			   | FAT_SIZE_LONG | FAT_SIZE_LONGLONG \
			   | FAT_SIZE_INTMAX_T | FAT_SIZE_SIZE_T \
			   | FAT_SIZE_PTRDIFF_T)
};

struct numbered_arg
{
  unsigned int number;
  enum format_arg_type type;
};

struct unnumbered_arg
{
  enum format_arg_type type;
};

struct spec
{
  unsigned int directives;
  unsigned int unnumbered_arg_count;
  unsigned int allocated;
  struct unnumbered_arg *unnumbered;
};

/* Locale independent test for a decimal digit.
   Argument can be  'char' or 'unsigned char'.  (Whereas the argument of
   <ctype.h> isdigit must be an 'unsigned char'.)  */
#undef isdigit
#define isdigit(c) ((unsigned int) ((c) - '0') < 10)


/* Prototypes for local functions.  Needed to ensure compiler checking of
   function argument counts despite of K&R C function definition syntax.  */
static int numbered_arg_compare PARAMS ((const void *p1, const void *p2));
static void *format_parse PARAMS ((const char *format));
static void format_free PARAMS ((void *descr));
static int format_get_number_of_directives PARAMS ((void *descr));
static bool format_check PARAMS ((const lex_pos_ty *pos,
				  void *msgid_descr, void *msgstr_descr));


static int
numbered_arg_compare (p1, p2)
     const void *p1;
     const void *p2;
{
  unsigned int n1 = ((const struct numbered_arg *) p1)->number;
  unsigned int n2 = ((const struct numbered_arg *) p2)->number;

  return (n1 > n2 ? 1 : n1 < n2 ? -1 : 0);
}

static void *
format_parse (format)
     const char *format;
{
  struct spec spec;
  unsigned int numbered_arg_count;
  struct numbered_arg *numbered;
  struct spec *result;

  spec.directives = 0;
  numbered_arg_count = 0;
  spec.unnumbered_arg_count = 0;
  spec.allocated = 0;
  numbered = NULL;
  spec.unnumbered = NULL;

  for (; *format != '\0';)
    if (*format++ == '%')
      {
	/* A directive.  */
	unsigned int number = 0;
	enum format_arg_type type;
	enum format_arg_type size;

	spec.directives++;

	if (isdigit (*format))
	  {
	    const char *f = format;
	    unsigned int m = 0;

	    do
	      {
		m = 10 * m + (*f - '0');
		f++;
	      }
	    while (isdigit (*f));

	    if (*f == '$')
	      {
		if (m == 0)
		  goto bad_format;
		number = m;
		format = ++f;
	      }
	  }

	/* Parse flags.  */
	while (*format == ' ' || *format == '+' || *format == '-'
	       || *format == '#' || *format == '0' || *format == '\'')
	  format++;

	/* Parse width.  */
	if (*format == '*')
	  {
	    unsigned int width_number = 0;

	    format++;

	    if (isdigit (*format))
	      {
		const char *f = format;
		unsigned int m = 0;

		do
		  {
		    m = 10 * m + (*f - '0');
		    f++;
		  }
		while (isdigit (*f));

		if (*f == '$')
		  {
		    if (m == 0)
		      goto bad_format;
		    width_number = m;
		    format = ++f;
		  }
	      }

	    if (width_number)
	      {
		/* Numbered argument.  */

		/* Numbered and unnumbered specifications are exclusive.  */
		if (spec.unnumbered_arg_count > 0)
		  goto bad_format;

		if (spec.allocated == numbered_arg_count)
		  {
		    spec.allocated = 2 * spec.allocated + 1;
		    numbered = (struct numbered_arg *) xrealloc (numbered, spec.allocated * sizeof (struct numbered_arg));
		  }
		numbered[numbered_arg_count].number = width_number;
		numbered[numbered_arg_count].type = FAT_INTEGER;
		numbered_arg_count++;
	      }
	    else
	      {
		/* Unnumbered argument.  */

		/* Numbered and unnumbered specifications are exclusive.  */
		if (numbered_arg_count > 0)
		  goto bad_format;

		if (spec.allocated == spec.unnumbered_arg_count)
		  {
		    spec.allocated = 2 * spec.allocated + 1;
		    spec.unnumbered = (struct unnumbered_arg *) xrealloc (spec.unnumbered, spec.allocated * sizeof (struct unnumbered_arg));
		  }
		spec.unnumbered[spec.unnumbered_arg_count].type = FAT_INTEGER;
		spec.unnumbered_arg_count++;
	      }
	  }
	else if (isdigit (*format))
	  {
	    do format++; while (isdigit (*format));
	  }

	/* Parse precision.  */
	if (*format == '.')
	  {
	    format++;

	    if (*format == '*')
	      {
		unsigned int precision_number = 0;

		format++;

		if (isdigit (*format))
		  {
		    const char *f = format;
		    unsigned int m = 0;

		    do
		      {
			m = 10 * m + (*f - '0');
			f++;
		      }
		    while (isdigit (*f));

		    if (*f == '$')
		      {
			if (m == 0)
			  goto bad_format;
			precision_number = m;
			format = ++f;
		      }
		  }

		if (precision_number)
		  {
		    /* Numbered argument.  */

		    /* Numbered and unnumbered specifications are exclusive.  */
		    if (spec.unnumbered_arg_count > 0)
		      goto bad_format;

		    if (spec.allocated == numbered_arg_count)
		      {
			spec.allocated = 2 * spec.allocated + 1;
			numbered = (struct numbered_arg *) xrealloc (numbered, spec.allocated * sizeof (struct numbered_arg));
		      }
		    numbered[numbered_arg_count].number = precision_number;
		    numbered[numbered_arg_count].type = FAT_INTEGER;
		    numbered_arg_count++;
		  }
		else
		  {
		    /* Unnumbered argument.  */

		    /* Numbered and unnumbered specifications are exclusive.  */
		    if (numbered_arg_count > 0)
		      goto bad_format;

		    if (spec.allocated == spec.unnumbered_arg_count)
		      {
			spec.allocated = 2 * spec.allocated + 1;
			spec.unnumbered = (struct unnumbered_arg *) xrealloc (spec.unnumbered, spec.allocated * sizeof (struct unnumbered_arg));
		      }
		    spec.unnumbered[spec.unnumbered_arg_count].type = FAT_INTEGER;
		    spec.unnumbered_arg_count++;
		  }
	      }
	    else if (isdigit (*format))
	      {
		do format++; while (isdigit (*format));
	      }
	  }

	/* Parse size.  */
	size = 0;
	for (;; format++)
	  {
	    if (*format == 'h')
	      {
		if (size & (FAT_SIZE_SHORT | FAT_SIZE_CHAR))
		  size = FAT_SIZE_CHAR;
		else
		  size = FAT_SIZE_SHORT;
	      }
	    else if (*format == 'l')
	      {
		if (size & (FAT_SIZE_LONG | FAT_SIZE_LONGLONG))
		  size = FAT_SIZE_LONGLONG;
		else
		  size = FAT_SIZE_LONG;
	      }
	    else if (*format == 'L')
	      size = FAT_SIZE_LONGLONG;
	    else if (*format == 'q')
	      /* Old BSD 4.4 convention.  */
	      size = FAT_SIZE_LONGLONG;
	    else if (*format == 'j')
	      size = FAT_SIZE_INTMAX_T;
	    else if (*format == 'z' || *format == 'Z')
	      /* 'z' is standardized in ISO C 99, but glibc uses 'Z' because
		 the warning facility in gcc-2.95.2 understands only 'Z'
		 (see gcc-2.95.2/gcc/c-common.c:1784).  */
	      size = FAT_SIZE_SIZE_T;
	    else if (*format == 't')
	      size = FAT_SIZE_PTRDIFF_T;
	    else
	      break;
	  }

	switch (*format)
	  {
	  case '%':
	  case 'm': /* glibc extension */
	    type = FAT_NONE;
	    break;
	  case 'c':
	    type = FAT_CHAR;
	    type |= (size & (FAT_SIZE_LONG | FAT_SIZE_LONGLONG) ? FAT_WIDE : 0);
	    break;
	  case 'C': /* obsolete */
	    type = FAT_CHAR | FAT_WIDE;
	    break;
	  case 's':
	    type = FAT_STRING;
	    type |= (size & (FAT_SIZE_LONG | FAT_SIZE_LONGLONG) ? FAT_WIDE : 0);
	    break;
	  case 'S': /* obsolete */
	    type = FAT_STRING | FAT_WIDE;
	    break;
	  case 'i': case 'd':
	    type = FAT_INTEGER;
	    type |= (size & FAT_SIZE_MASK);
	    break;
	  case 'u': case 'o': case 'x': case 'X':
	    type = FAT_INTEGER | FAT_UNSIGNED;
	    type |= (size & FAT_SIZE_MASK);
	    break;
	  case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
	  case 'a': case 'A':
	    type = FAT_DOUBLE;
	    type |= (size & FAT_SIZE_LONGLONG);
	    break;
	  case 'p':
	    type = FAT_POINTER;
	    break;
	  case 'n':
	    type = FAT_COUNT_POINTER;
	    type |= (size & FAT_SIZE_MASK);
	    break;
	  default:
	    goto bad_format;
	  }

	if (type != FAT_NONE)
	  {
	    if (number)
	      {
		/* Numbered argument.  */

		/* Numbered and unnumbered specifications are exclusive.  */
		if (spec.unnumbered_arg_count > 0)
		  goto bad_format;

		if (spec.allocated == numbered_arg_count)
		  {
		    spec.allocated = 2 * spec.allocated + 1;
		    numbered = (struct numbered_arg *) xrealloc (numbered, spec.allocated * sizeof (struct numbered_arg));
		  }
		numbered[numbered_arg_count].number = number;
		numbered[numbered_arg_count].type = type;
		numbered_arg_count++;
	      }
	    else
	      {
		/* Unnumbered argument.  */

		/* Numbered and unnumbered specifications are exclusive.  */
		if (numbered_arg_count > 0)
		  goto bad_format;

		if (spec.allocated == spec.unnumbered_arg_count)
		  {
		    spec.allocated = 2 * spec.allocated + 1;
		    spec.unnumbered = (struct unnumbered_arg *) xrealloc (spec.unnumbered, spec.allocated * sizeof (struct unnumbered_arg));
		  }
		spec.unnumbered[spec.unnumbered_arg_count].type = type;
		spec.unnumbered_arg_count++;
	      }
	  }

	format++;
      }

  /* Sort the numbered argument array, and eliminate duplicates.  */
  if (numbered_arg_count > 1)
    {
      unsigned int i, j;
      bool err;

      qsort (numbered, numbered_arg_count,
	     sizeof (struct numbered_arg), numbered_arg_compare);

      /* Remove duplicates: Copy from i to j, keeping 0 <= j <= i.  */
      err = false;
      for (i = j = 0; i < numbered_arg_count; i++)
	if (j > 0 && numbered[i].number == numbered[j-1].number)
	  {
	    enum format_arg_type type1 = numbered[i].type;
	    enum format_arg_type type2 = numbered[j-1].type;
	    enum format_arg_type type_both;

	    if (type1 == type2)
	      type_both = type1;
	    else
	      /* Incompatible types.  */
	      type_both = FAT_NONE, err = true;

	    numbered[j-1].type = type_both;
	  }
	else
	  {
	    if (j < i)
	      {
		numbered[j].number = numbered[i].number;
		numbered[j].type = numbered[i].type;
	      }
	    j++;
	  }
      numbered_arg_count = j;
      if (err)
	goto bad_format;
    }

  /* Verify that the format strings uses all arguments up to the highest
     numbered one.  */
  if (numbered_arg_count > 0)
    {
      unsigned int i;

      for (i = 0; i < numbered_arg_count; i++)
	if (numbered[i].number != i + 1)
	  goto bad_format;

      /* So now the numbered arguments array is equivalent to a sequence
	 of unnumbered arguments.  */
      spec.unnumbered_arg_count = numbered_arg_count;
      spec.allocated = spec.unnumbered_arg_count;
      spec.unnumbered = (struct unnumbered_arg *) xmalloc (spec.allocated * sizeof (struct unnumbered_arg));
      for (i = 0; i < spec.unnumbered_arg_count; i++)
        spec.unnumbered[i].type = numbered[i].type;
      free (numbered);
      numbered_arg_count = 0;
    }

  result = (struct spec *) xmalloc (sizeof (struct spec));
  *result = spec;
  return result;

 bad_format:
  if (numbered != NULL)
    free (numbered);
  if (spec.unnumbered != NULL)
    free (spec.unnumbered);
  return NULL;
}

static void
format_free (descr)
     void *descr;
{
  struct spec *spec = (struct spec *) descr;

  if (spec->unnumbered != NULL)
    free (spec->unnumbered);
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
format_check (pos, msgid_descr, msgstr_descr)
     const lex_pos_ty *pos;
     void *msgid_descr;
     void *msgstr_descr;
{
  struct spec *spec1 = (struct spec *) msgid_descr;
  struct spec *spec2 = (struct spec *) msgstr_descr;
  bool err = false;
  unsigned int i;

  /* Check the argument types are the same.  */
  if (spec1->unnumbered_arg_count != spec2->unnumbered_arg_count)
    {
      error_with_progname = false;
      error_at_line (0, 0, pos->file_name, pos->line_number,
		     _("number of format specifications in 'msgid' and 'msgstr' does not match"));
      error_with_progname = true;
      err = true;
    }
  else
    for (i = 0; i < spec1->unnumbered_arg_count; i++)
      if (spec1->unnumbered[i].type != spec2->unnumbered[i].type)
	{
	  error_with_progname = false;
	  error_at_line (0, 0, pos->file_name, pos->line_number,
			 _("format specifications in 'msgid' and 'msgstr' for argument %u are not the same"),
			 i + 1);
	  error_with_progname = true;
	  err = true;
	}

  return err;
}


struct formatstring_parser formatstring_c =
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

  printf ("(");
  for (i = 0; i < spec->unnumbered_arg_count; i++)
    {
      if (i > 0)
	printf (" ");
      if (spec->unnumbered[i].type & FAT_UNSIGNED)
	printf ("[unsigned]");
      switch (spec->unnumbered[i].type & FAT_SIZE_MASK)
	{
	case 0:
	  break;
	case FAT_SIZE_SHORT:
	  printf ("[short]");
	  break;
	case FAT_SIZE_CHAR:
	  printf ("[char]");
	  break;
	case FAT_SIZE_LONG:
	  printf ("[long]");
	  break;
	case FAT_SIZE_LONGLONG:
	  printf ("[long long]");
	  break;
	case FAT_SIZE_INTMAX_T:
	  printf ("[intmax_t]");
	  break;
	case FAT_SIZE_SIZE_T:
	  printf ("[size_t]");
	  break;
	case FAT_SIZE_PTRDIFF_T:
	  printf ("[ptrdiff_t]");
	  break;
	default:
	  abort ();
	}
      switch (spec->unnumbered[i].type & ~(FAT_UNSIGNED | FAT_SIZE_MASK))
	{
	case FAT_INTEGER:
	  printf ("i");
	  break;
	case FAT_DOUBLE:
	  printf ("f");
	  break;
	case FAT_CHAR:
	  printf ("c");
	  break;
	case FAT_STRING:
	  printf ("s");
	  break;
	case FAT_POINTER:
	  printf ("p");
	  break;
	case FAT_COUNT_POINTER:
	  printf ("n");
	  break;
	default:
	  abort ();
	}
    }
  printf (")");
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
 * compile-command: "gcc -O -g -Wall -I.. -I../lib -I../intl -DHAVE_CONFIG_H -DTEST format-c.c ../lib/libnlsut.a"
 * End:
 */

#endif /* TEST */
