/*  Copyright (C) 1991, 1992, 1993, 1995, 2000 Free Software Foundation, Inc.

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

#ifndef	_PRINTF_H

#define	_PRINTF_H	1

#include <stdio.h>
#include <sys/types.h>

#if HAVE_STDDEF_H
# include <stddef.h>
#endif

#ifndef PARAMS
# if __STDC__
#  define PARAMS(args) args
# else
#  define PARAMS(args) ()
# endif
#endif

struct printf_info
{
  int prec;			/* Precision.  */
  int width;			/* Width.  */
  char spec;			/* Format letter.  */
  unsigned is_long_double:1;	/* L flag.  */
  unsigned is_short:1;		/* h flag.  */
  unsigned is_char:1;		/* hh flag.  */
  unsigned is_long:1;		/* l flag.  */
  unsigned is_longlong:1;	/* ll flag.  */
  unsigned alt:1;		/* # flag.  */
  unsigned space:1;		/* Space flag.  */
  unsigned left:1;		/* - flag.  */
  unsigned showsign:1;		/* + flag.  */
  unsigned group:1;		/* ' flag.  */
  char pad;			/* Padding character.  */
};


/* Type of a printf specifier-handler function.
   STREAM is the FILE on which to write output.
   INFO gives information about the format specification.
   Arguments can be read from ARGS.
   The function should return the number of characters written,
   or -1 for errors.  */

typedef int (*printf_function) PARAMS ((FILE * __stream,
					const struct printf_info * __info,
					const void **const __args));
typedef int (*printf_arginfo_function) PARAMS ((const struct printf_info
						*__info,
						size_t __n,
						int *__argtypes));

/* Parse FMT, and fill in N elements of ARGTYPES with the
   types needed for the conversions FMT specifies.  Returns
   the number of arguments required by FMT.

   The ARGINFO function registered with a user-defined format is passed a
   `struct printf_info' describing the format spec being parsed.  A width
   or precision of INT_MIN means a `*' was used to indicate that the
   width/precision will come from an arg.  The function should fill in the
   array it is passed with the types of the arguments it wants, and return
   the number of arguments it wants.  */

extern size_t parse_printf_format PARAMS ((const char *__fmt,
					   size_t __n,
					   int *__argtypes));

/* Codes returned by `parse_printf_format' for basic types.

   These values cover all the standard format specifications.
   Users can add new values after PA_LAST for their own types.  */

enum
{				/* C type: */
  PA_INT,			/* int */
  PA_CHAR,			/* int, cast to char */
  PA_STRING,			/* const char *, a '\0'-terminated string */
  PA_POINTER,			/* void * */
  PA_FLOAT,			/* float */
  PA_DOUBLE,			/* double */
  PA_LAST
};

/* Flag bits that can be set in a type returned by `parse_printf_format'.  */
#define	PA_FLAG_MASK		0xff00
#define	PA_FLAG_LONG_LONG	(1 << 8)
#define	PA_FLAG_LONG_DOUBLE	PA_FLAG_LONG_LONG
#define	PA_FLAG_LONG		(1 << 9)
#define	PA_FLAG_SHORT		(1 << 10)
#define	PA_FLAG_PTR		(1 << 11)
#define	PA_FLAG_CHAR		(1 << 12)


#endif /* printf.h  */
