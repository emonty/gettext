/* Message list charset and locale charset handling.
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
# include "config.h"
#endif

/* Specification.  */
#include "msgl-iconv.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_ICONV
# include <iconv.h>
#endif

#include "error.h"
#include "progname.h"
#include "message.h"
#include "po-charset.h"
#include "msgl-ascii.h"
#include "xmalloc.h"
#include "strstr.h"
#include "system.h"
#include "libgettext.h"

#define _(str) gettext (str)


/* Prototypes for local functions.  Needed to ensure compiler checking of
   function argument counts despite of K&R C function definition syntax.  */
#if HAVE_ICONV
static int iconv_string PARAMS ((iconv_t cd,
				 const char *start, const char *end,
				 char **resultp, size_t *lengthp));
static const char *convert_string PARAMS ((iconv_t cd, const char *string));
static void convert_string_list PARAMS ((iconv_t cd, string_list_ty *slp));
static void convert_msgstr PARAMS ((iconv_t cd, message_ty *mp));
#endif


#if HAVE_ICONV

/* Converts an entire string from one encoding to another, using iconv.
   Return value: 0 if successful, otherwise -1 and errno set.  */
static int
iconv_string (cd, start, end, resultp, lengthp)
     iconv_t cd;
     const char *start;
     const char *end;
     char **resultp;
     size_t *lengthp;
{
#define tmpbufsize 4096
  size_t length;
  char *result;

  /* Avoid glibc-2.1 bug and Solaris 2.7 bug.  */
# if defined _LIBICONV_VERSION \
    || !((__GLIBC__ - 0 == 2 && __GLIBC_MINOR__ - 0 <= 1) || defined __sun)
  /* Set to the initial state.  */
  iconv (cd, NULL, NULL, NULL, NULL);
# endif

  /* Determine the length we need.  */
  {
    size_t count = 0;
    char tmpbuf[tmpbufsize];
    const char *inptr = start;
    size_t insize = end - start;

    while (insize > 0)
      {
	char *outptr = tmpbuf;
	size_t outsize = tmpbufsize;
	size_t res = iconv (cd,
			    (ICONV_CONST char **) &inptr, &insize,
			    &outptr, &outsize);

	if (res == (size_t)(-1))
	  {
	    if (errno == EINVAL)
	      break;
	    else
	      return -1;
	  }
# if !defined _LIBICONV_VERSION && (defined sgi || defined __sgi)
	/* Irix iconv() inserts a NUL byte if it cannot convert.  */
	else if (res > 0)
	  return -1;
# endif
	count += outptr - tmpbuf;
      }
    /* Avoid glibc-2.1 bug and Solaris 2.7 bug.  */
# if defined _LIBICONV_VERSION \
    || !((__GLIBC__ - 0 == 2 && __GLIBC_MINOR__ - 0 <= 1) || defined __sun)
    {
      char *outptr = tmpbuf;
      size_t outsize = tmpbufsize;
      size_t res = iconv (cd, NULL, NULL, &outptr, &outsize);

      if (res == (size_t)(-1))
	return -1;
      count += outptr - tmpbuf;
    }
# endif
    length = count;
  }

  *lengthp = length;
  *resultp = result = xrealloc (*resultp, length);
  if (length == 0)
    return 0;

  /* Avoid glibc-2.1 bug and Solaris 2.7 bug.  */
# if defined _LIBICONV_VERSION \
    || !((__GLIBC__ - 0 == 2 && __GLIBC_MINOR__ - 0 <= 1) || defined __sun)
  /* Return to the initial state.  */
  iconv (cd, NULL, NULL, NULL, NULL);
# endif

  /* Do the conversion for real.  */
  {
    const char *inptr = start;
    size_t insize = end - start;
    char *outptr = result;
    size_t outsize = length;

    while (insize > 0)
      {
	size_t res = iconv (cd,
			    (ICONV_CONST char **) &inptr, &insize,
			    &outptr, &outsize);

	if (res == (size_t)(-1))
	  {
	    if (errno == EINVAL)
	      break;
	    else
	      return -1;
	  }
# if !defined _LIBICONV_VERSION && (defined sgi || defined __sgi)
	/* Irix iconv() inserts a NUL byte if it cannot convert.  */
	else if (res > 0)
	  return -1;
# endif
      }
    /* Avoid glibc-2.1 bug and Solaris 2.7 bug.  */
# if defined _LIBICONV_VERSION \
    || !((__GLIBC__ - 0 == 2 && __GLIBC_MINOR__ - 0 <= 1) || defined __sun)
    {
      size_t res = iconv (cd, NULL, NULL, &outptr, &outsize);

      if (res == (size_t)(-1))
	return -1;
    }
# endif
    if (outsize != 0)
      abort ();
  }

  return 0;
#undef tmpbufsize
}

static const char *
convert_string (cd, string)
     iconv_t cd;
     const char *string;
{
  size_t len = strlen (string) + 1;
  char *result = NULL;
  size_t resultlen;

  if (iconv_string (cd, string, string + len, &result, &resultlen) == 0)
    /* Verify the result has exactly one NUL byte, at the end.  */
    if (resultlen > 0 && result[resultlen - 1] == '\0'
	&& strlen (result) == resultlen - 1)
      return result;

  error (EXIT_FAILURE, 0, _("conversion failure"));
  /* NOTREACHED */
  return NULL;
}

static void
convert_string_list (cd, slp)
     iconv_t cd;
     string_list_ty *slp;
{
  size_t i;

  if (slp != NULL)
    for (i = 0; i < slp->nitems; i++)
      slp->item[i] = convert_string (cd, slp->item[i]);
}

static void
convert_msgstr (cd, mp)
     iconv_t cd;
     message_ty *mp;
{
  char *result = NULL;
  size_t resultlen;

  if (!(mp->msgstr_len > 0 && mp->msgstr[mp->msgstr_len - 1] == '\0'))
    abort ();

  if (iconv_string (cd, mp->msgstr, mp->msgstr + mp->msgstr_len,
		    &result, &resultlen) == 0)
    /* Verify the result has a NUL byte at the end.  */
    if (resultlen > 0 && result[resultlen - 1] == '\0')
      /* Verify the result has the same number of NUL bytes.  */
      {
	const char *p;
	const char *pend;
	int nulcount1;
	int nulcount2;

	for (p = mp->msgstr, pend = p + mp->msgstr_len, nulcount1 = 0;
	     p < pend;
	     p += strlen (p) + 1, nulcount1++);
	for (p = result, pend = p + resultlen, nulcount2 = 0;
	     p < pend;
	     p += strlen (p) + 1, nulcount2++);

	if (nulcount1 == nulcount2)
	  {
	    mp->msgstr = result;
	    mp->msgstr_len = resultlen;
	    return;
	  }
      }

  error (EXIT_FAILURE, 0, _("conversion failure"));
}

#endif


void
iconv_message_list (mlp, canon_to_code)
     message_list_ty *mlp;
     const char *canon_to_code;
{
  const char *canon_from_code;
  size_t j;

  /* If the list is empty, nothing to do.  */
  if (mlp->nitems == 0)
    return;

  /* Search the header entry, and extract and replace the charset name.  */
  canon_from_code = NULL;
  for (j = 0; j < mlp->nitems; j++)
    if (mlp->item[j]->msgid[0] == '\0' && !mlp->item[j]->obsolete)
      {
	const char *header = mlp->item[j]->msgstr;

	if (header != NULL)
	  {
	    const char *charsetstr = strstr (header, "charset=");

	    if (charsetstr != NULL)
	      {
		size_t len;
		char *charset;
		const char *canon_charset;
		size_t len1, len2, len3;
		char *new_header;

		charsetstr += strlen ("charset=");
		len = strcspn (charsetstr, " \t\n");
		charset = (char *) alloca (len + 1);
		memcpy (charset, charsetstr, len);
		charset[len] = '\0';

		canon_charset = po_charset_canonicalize (charset);
		if (canon_charset == NULL)
		  error (EXIT_FAILURE, 0,
			 _("\
present charset \"%s\" is not a portable encoding name"),
			 charset);

		if (canon_from_code == NULL)
		  canon_from_code = canon_charset;
		else if (canon_from_code != canon_charset)
		  error (EXIT_FAILURE, 0,
			 _("\
two different charsets \"%s\" and \"%s\" in input file"),
			 canon_from_code, canon_charset);

		len1 = charsetstr - header;
		len2 = strlen (canon_to_code);
		len3 = (header + strlen (header)) - (charsetstr + len);
		new_header = (char *) xmalloc (len1 + len2 + len3 + 1);
		memcpy (new_header, header, len1);
		memcpy (new_header + len1, canon_to_code, len2);
		memcpy (new_header + len1 + len2, charsetstr + len, len3 + 1);
		mlp->item[j]->msgstr = new_header;
		mlp->item[j]->msgstr_len = len1 + len2 + len3 + 1;
	      }
	  }
      }
  if (canon_from_code == NULL)
    {
      if (is_ascii_message_list (mlp))
	canon_from_code = po_charset_ascii;
      else
	error (EXIT_FAILURE, 0, _("\
input file doesn't contain a header entry with a charset specification"));
    }

  /* If the two encodings are the same, nothing to do.  */
  if (canon_from_code != canon_to_code)
    {
#if HAVE_ICONV
      iconv_t cd;

      /* Avoid glibc-2.1 bug with EUC-KR.  */
# if (__GLIBC__ - 0 == 2 && __GLIBC_MINOR__ - 0 <= 1) && !defined _LIBICONV_VERSION
      if (strcmp (canon_from_code, "EUC-KR") == 0)
	cd = (iconv_t)(-1);
      else
# endif
      cd = iconv_open (canon_to_code, canon_from_code);
      if (cd == (iconv_t)(-1))
	error (EXIT_FAILURE, 0, _("\
Cannot convert from \"%s\" to \"%s\". %s relies on iconv(), \
and iconv() does not support this conversion."),
	       canon_from_code, canon_to_code, basename (program_name));

      for (j = 0; j < mlp->nitems; j++)
	{
	  message_ty *mp = mlp->item[j];

	  convert_string_list (cd, mp->comment);
	  convert_string_list (cd, mp->comment_dot);
	  convert_msgstr (cd, mp);
	}

      iconv_close (cd);
#else
	  error (EXIT_FAILURE, 0, _("\
Cannot convert from \"%s\" to \"%s\". %s relies on iconv(). \
This version was built without iconv()."),
		 canon_from_code, canon_to_code, basename (program_name));
#endif
    }
}

msgdomain_list_ty *
iconv_msgdomain_list (mdlp, to_code)
     msgdomain_list_ty *mdlp;
     const char *to_code;
{
  const char *canon_to_code;
  size_t k;

  /* Canonicalize target encoding.  */
  canon_to_code = po_charset_canonicalize (to_code);
  if (canon_to_code == NULL)
    error (EXIT_FAILURE, 0,
	   _("target charset \"%s\" is not a portable encoding name."),
	   to_code);

  for (k = 0; k < mdlp->nitems; k++)
    iconv_message_list (mdlp->item[k]->messages, canon_to_code);

  return mdlp;
}
