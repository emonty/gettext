/* xmalloc.c -- malloc with out of memory checking
   Copyright (C) 1990-1996, 2000, 2001 Free Software Foundation, Inc.

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

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* Specification.  */
#include "xmalloc.h"

#include <stdlib.h>

#include "error.h"
#include "gettext.h"

#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif

#define _(str) gettext (str)


/* Prototypes for local functions.  Needed to ensure compiler checking of
   function argument counts despite of K&R C function definition syntax.  */
static void *fixup_null_alloc PARAMS ((size_t n));


/* Exit value when the requested amount of memory is not available.
   The caller may set it to some other value.  */
int xmalloc_exit_failure = EXIT_FAILURE;

static void *
fixup_null_alloc (n)
     size_t n;
{
  void *p;

  p = 0;
  if (n == 0)
    p = malloc ((size_t) 1);
  if (p == NULL)
    error (xmalloc_exit_failure, 0, _("memory exhausted"));
  return p;
}

/* Allocate N bytes of memory dynamically, with error checking.  */

void *
xmalloc (n)
     size_t n;
{
  void *p;

  p = malloc (n);
  if (p == NULL)
    p = fixup_null_alloc (n);
  return p;
}

/* Allocate memory for N elements of S bytes, with error checking.  */

void *
xcalloc (n, s)
     size_t n, s;
{
  void *p;

  p = calloc (n, s);
  if (p == NULL)
    p = fixup_null_alloc (n);
  return p;
}

/* Change the size of an allocated block of memory P to N bytes,
   with error checking.
   If P is NULL, run xmalloc.  */

void *
xrealloc (p, n)
     void *p;
     size_t n;
{
  if (p == NULL)
    return xmalloc (n);
  p = realloc (p, n);
  if (p == NULL)
    p = fixup_null_alloc (n);
  return p;
}
