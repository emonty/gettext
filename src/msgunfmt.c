/* msgunfmt - converts binary .mo files to Uniforum style .po files
   Copyright (C) 1995-1998, 2000, 2001 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, April 1995.

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

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <stdlib.h>
#include <locale.h>

#include "hash.h"

#include "error.h"
#include "progname.h"
#include "getline.h"
#include "printf.h"
#include <system.h>

#include "gettext.h"
#include "hash-string.h"
#include "libgettext.h"
#include "message.h"
#include "write-po.h"

#define _(str) gettext (str)

#ifndef errno
extern int errno;
#endif


/* Force output of PO file even if empty.  */
static int force_po;

/* Long options.  */
static const struct option long_options[] =
{
  { "escape", no_argument, NULL, 'E' },
  { "force-po", no_argument, &force_po, 1 },
  { "help", no_argument, NULL, 'h' },
  { "indent", no_argument, NULL, 'i' },
  { "no-escape", no_argument, NULL, 'e' },
  { "output-file", required_argument, NULL, 'o' },
  { "sort-output", no_argument, NULL, 's' },
  { "strict", no_argument, NULL, 'S' },
  { "version", no_argument, NULL, 'V' },
  { "width", required_argument, NULL, 'w', },
  { NULL, 0, NULL, 0 }
};


/* This defines the byte order within the file.  It needs to be set
   appropriately once we have the file open.  */
static enum { MO_LITTLE_ENDIAN, MO_BIG_ENDIAN } endian;


/* Prototypes for local functions.  Needed to ensure compiler checking of
   function argument counts despite of K&R C function definition syntax.  */
static void usage PARAMS ((int status));
static nls_uint32 read32 PARAMS ((FILE *fp, const char *fn));
static void seek32 PARAMS ((FILE *fp, const char *fn, long offset));
static char *string32 PARAMS ((FILE *fp, const char *fn, long offset,
			       size_t *lengthp));
static message_list_ty *read_mo_file PARAMS ((message_list_ty *mlp,
					      const char *fn));


int
main (argc, argv)
     int argc;
     char **argv;
{
  int optchar;
  int do_help = 0;
  int do_version = 0;
  const char *output_file = "-";
  message_list_ty *mlp;
  msgdomain_list_ty *result;
  int sort_by_msgid = 0;

  /* Set program name for messages.  */
  set_program_name (argv[0]);
  error_print_progname = maybe_print_progname;

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  while ((optchar = getopt_long (argc, argv, "eEhio:sVw:", long_options, NULL))
	 != EOF)
    switch (optchar)
      {
      case '\0':
	/* long option */
	break;

      case 'e':
	message_print_style_escape (0);
	break;

      case 'E':
	message_print_style_escape (1);
	break;

      case 'h':
	do_help = 1;
	break;

      case 'i':
	message_print_style_indent ();
	break;

      case 'o':
	output_file = optarg;
	break;

      case 's':
	sort_by_msgid = 1;
	break;

      case 'S':
	message_print_style_uniforum ();
	break;

      case 'V':
	do_version = 1;
	break;

      case 'w':
	{
	  int value;
	  char *endp;
	  value = strtol (optarg, &endp, 10);
	  if (endp != optarg)
	    message_page_width_set (value);
	}
	break;

      default:
	usage (EXIT_FAILURE);
	break;
      }

  /* Version information is requested.  */
  if (do_version)
    {
      printf ("%s (GNU %s) %s\n", basename (program_name), PACKAGE, VERSION);
      /* xgettext: no-wrap */
      printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
"),
	      "1995-1998, 2000, 2001");
      printf (_("Written by %s.\n"), "Ulrich Drepper");
      exit (EXIT_SUCCESS);
    }

  /* Help is requested.  */
  if (do_help)
    usage (EXIT_SUCCESS);

  /* Read the given .mo file. */
  if (optind < argc)
    {
      mlp = NULL;
      do
	mlp = read_mo_file (mlp, argv[optind]);
      while (++optind < argc);
    }
  else
    mlp = read_mo_file (NULL, "-");

  result = msgdomain_list_alloc ();
  result->item[0]->messages = mlp;

  /* Sorting the list of messages.  */
  if (sort_by_msgid)
    msgdomain_list_sort_by_msgid (result);

  /* Write the resulting message list to the given .po file.  */
  msgdomain_list_print (result, output_file, force_po, 0);

  /* No problems.  */
  exit (EXIT_SUCCESS);
}


/* Display usage information and exit.  */
static void
usage (status)
     int status;
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      /* xgettext: no-wrap */
      printf (_("\
Usage: %s [OPTION] [FILE]...\n\
"), program_name);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Convert binary message catalog to Uniforum style .po file.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Input file location:\n\
  FILE ...                 input .mo files\n\
If no input file is given or if it is -, standard input is read.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Output file location:\n\
  -o, --output-file=FILE   write output to specified file\n\
The results are written to standard output if no output file is specified\n\
or if it is -.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Output details:\n\
  -e, --no-escape          do not use C escapes in output (default)\n\
  -E, --escape             use C escapes in output, no extended chars\n\
      --force-po           write PO file even if empty\n\
  -i, --indent             write indented output style\n\
      --strict             write strict uniforum style\n\
  -w, --width=NUMBER       set output page width\n\
  -s, --sort-output        generate sorted output and remove duplicates\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Informative output:\n\
  -h, --help               display this help and exit\n\
  -V, --version            output version information and exit\n\
"));
      printf ("\n");
      fputs (_("Report bugs to <bug-gnu-utils@gnu.org>.\n"),
	     stdout);
    }

  exit (status);
}


/* This function reads a 32-bit number from the file, and assembles it
   according to the current ``endian'' setting.  */
static nls_uint32
read32 (fp, fn)
     FILE *fp;
     const char *fn;
{
  int c1, c2, c3, c4;

  c1 = getc (fp);
  if (c1 == EOF)
    {
    bomb:
      if (ferror (fp))
	error (EXIT_FAILURE, errno, _("error while reading \"%s\""), fn);
      error (EXIT_FAILURE, 0, _("file \"%s\" truncated"), fn);
    }
  c2 = getc (fp);
  if (c2 == EOF)
    goto bomb;
  c3 = getc (fp);
  if (c3 == EOF)
    goto bomb;
  c4 = getc (fp);
  if (c4 == EOF)
    goto bomb;
  if (endian == MO_LITTLE_ENDIAN)
    return (((nls_uint32) c1)
	    | ((nls_uint32) c2 << 8)
	    | ((nls_uint32) c3 << 16)
	    | ((nls_uint32) c4 << 24));

  return (((nls_uint32) c1 << 24)
	  | ((nls_uint32) c2 << 16)
	  | ((nls_uint32) c3 << 8)
	  | ((nls_uint32) c4));
}


static void
seek32 (fp, fn, offset)
     FILE *fp;
     const char *fn;
     long offset;
{
  if (fseek (fp, offset, 0) < 0)
    error (EXIT_FAILURE, errno, _("seek \"%s\" offset %ld failed"),
	   fn, offset);
}


static char *
string32 (fp, fn, offset, lengthp)
     FILE *fp;
     const char *fn;
     long offset;
     size_t *lengthp;
{
  long length;
  char *buffer;
  long n;

  /* Read the string_desc structure, describing where in the file to
     find the string.  */
  seek32 (fp, fn, offset);
  length = read32 (fp, fn);
  offset = read32 (fp, fn);

  /* Allocate memory for the string to be read into.  Leave space for
     the NUL on the end.  */
  buffer = xmalloc (length + 1);

  /* Read in the string.  Complain if there is an error or it comes up
     short.  Add the NUL ourselves.  */
  seek32 (fp, fn, offset);
  n = fread (buffer, 1, length + 1, fp);
  if (n != length + 1)
    {
      if (ferror (fp))
	error (EXIT_FAILURE, errno, _("error while reading \"%s\""), fn);
      error (EXIT_FAILURE, 0, _("file \"%s\" truncated"), fn);
    }
  if (buffer[length] != '\0')
    {
      error (EXIT_FAILURE, 0,
	     _("file \"%s\" contains a not NUL terminated string"), fn);
    }

  /* Return the string to the caller.  */
  *lengthp = length + 1;
  return buffer;
}


/* This function reads an existing .mo file.  Return a message list.  */
static message_list_ty *
read_mo_file (mlp, fn)
     message_list_ty *mlp;
     const char *fn;
{
  FILE *fp;
  struct mo_file_header header;
  int j;

  if (strcmp (fn, "-") == 0 || strcmp (fn, "/dev/stdin") == 0)
    {
      fp = stdin;
      SET_BINARY (fileno (fp));
    }
  else
    {
      fp = fopen (fn, "rb");
      if (fp == NULL)
	error (EXIT_FAILURE, errno,
	       _("error while opening \"%s\" for reading"), fn);
    }

  /* We must grope the file to determine which endian it is.
     Perversity of the universe tends towards maximum, so it will
     probably not match the currently executing architecture.  */
  endian = MO_BIG_ENDIAN;
  header.magic = read32 (fp, fn);
  if (header.magic != _MAGIC)
    {
      endian = MO_LITTLE_ENDIAN;
      seek32 (fp, fn, 0L);
      header.magic = read32 (fp, fn);
      if (header.magic != _MAGIC)
	{
	unrecognised:
	  error (EXIT_FAILURE, 0, _("file \"%s\" is not in GNU .mo format"),
		 fn);
	}
    }

  /* Fill the structure describing the header.  */
  header.revision = read32 (fp, fn);
  if (header.revision != MO_REVISION_NUMBER)
    goto unrecognised;
  header.nstrings = read32 (fp, fn);
  header.orig_tab_offset = read32 (fp, fn);
  header.trans_tab_offset = read32 (fp, fn);
  header.hash_tab_size = read32 (fp, fn);
  header.hash_tab_offset = read32 (fp, fn);

  if (mlp == NULL)
    mlp = message_list_alloc ();
  for (j = 0; j < header.nstrings; ++j)
    {
      static lex_pos_ty pos = { __FILE__, __LINE__ };
      message_ty *mp;
      char *msgid;
      size_t msgid_len;
      char *msgstr;
      size_t msgstr_len;

      /* Read the msgid.  */
      msgid = string32 (fp, fn, header.orig_tab_offset + j * 8, &msgid_len);

      /* Read the msgstr.  */
      msgstr = string32 (fp, fn, header.trans_tab_offset + j * 8, &msgstr_len);

      mp = message_alloc (msgid,
			  (strlen (msgid) + 1 < msgid_len
			   ? msgid + strlen (msgid) + 1
			   : NULL),
			  msgstr, msgstr_len, &pos);
      message_list_append (mlp, mp);
    }

  if (fp != stdin)
    fclose (fp);
  return mlp;
}
