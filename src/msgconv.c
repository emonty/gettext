/* Converts a translation catalog to a different character encoding.
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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include "dir-list.h"
#include "error.h"
#include "progname.h"
#include "message.h"
#include "read-po.h"
#include "write-po.h"
#include "msgl-iconv.h"
#include "system.h"
#include "libgettext.h"

#define _(str) gettext (str)

extern const char * locale_charset PARAMS ((void));


/* Force output of PO file even if empty.  */
static int force_po;

/* Target encoding.  */
static const char *to_code;

/* Long options.  */
static const struct option long_options[] =
{
  { "add-location", no_argument, &line_comment, 1 },
  { "directory", required_argument, NULL, 'D' },
  { "escape", no_argument, NULL, 'E' },
  { "force-po", no_argument, &force_po, 1 },
  { "help", no_argument, NULL, 'h' },
  { "indent", no_argument, NULL, 'i' },
  { "no-escape", no_argument, NULL, 'e' },
  { "no-location", no_argument, &line_comment, 0 },
  { "output-file", required_argument, NULL, 'o' },
  { "sort-by-file", no_argument, NULL, 'F' },
  { "sort-output", no_argument, NULL, 's' },
  { "strict", no_argument, NULL, 'S' },
  { "to-code", required_argument, NULL, 't' },
  { "version", no_argument, NULL, 'V' },
  { "width", required_argument, NULL, 'w', },
  { NULL, 0, NULL, 0 }
};


/* Prototypes for local functions.  */
static void usage PARAMS ((int status));


int
main (argc, argv)
     int argc;
     char **argv;
{
  int opt;
  int do_help;
  int do_version;
  char *output_file;
  const char *input_file;
  msgdomain_list_ty *result;
  int sort_by_filepos = 0;
  int sort_by_msgid = 0;

  /* Set program name for messages.  */
  program_name = argv[0];
  error_print_progname = maybe_print_progname;

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  /* Set default values for variables.  */
  do_help = 0;
  do_version = 0;
  output_file = NULL;
  input_file = NULL;

  while ((opt = getopt_long (argc, argv, "D:eEFhio:st:Vw:", long_options, NULL))
	 != EOF)
    switch (opt)
      {
      case '\0':		/* Long option.  */
	break;

      case 'D':
	dir_list_append (optarg);
	break;

      case 'e':
	message_print_style_escape (0);
	break;

      case 'E':
	message_print_style_escape (1);
	break;

      case 'F':
	sort_by_filepos = 1;
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

      case 't':
	to_code = optarg;
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
	      "2001");
      printf (_("Written by %s.\n"), "Bruno Haible");
      exit (EXIT_SUCCESS);
    }

  /* Help is requested.  */
  if (do_help)
    usage (EXIT_SUCCESS);

  /* Test whether we have an .po file name as argument.  */
  if (optind == argc)
    input_file = "-";
  else if (optind + 1 == argc)
    input_file = argv[optind];
  else
    {
      error (EXIT_SUCCESS, 0, _("at most one input file allowed"));
      usage (EXIT_FAILURE);
    }

  if (sort_by_msgid && sort_by_filepos)
    error (EXIT_FAILURE, 0, _("%s and %s are mutually exclusive"),
	   "--sort-output", "--sort-by-file");

  /* Default for target encoding is current locale's encoding.  */
  if (to_code == NULL)
    to_code = locale_charset ();

  /* Read input file and convert.  */
  result = iconv_msgdomain_list (read_po_file (input_file), to_code);

  /* Sort the results.  */
  if (sort_by_filepos)
    msgdomain_list_sort_by_filepos (result);
  else if (sort_by_msgid)
    msgdomain_list_sort_by_msgid (result);

  /* Write the merged message list out.  */
  msgdomain_list_print (result, output_file, force_po, 0);

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
Usage: %s [OPTION] [INPUTFILE]\n\
"), program_name);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Converts a translation catalog to a different character encoding.\n\
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
  INPUTFILE                   input PO file\n\
  -D, --directory=DIRECTORY   add DIRECTORY to list for input files search\n\
If no input file is given or if it is -, standard input is read.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Output file location:\n\
  -o, --output-file=FILE      write output to specified file\n\
The results are written to standard output if no output file is specified\n\
or if it is -.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Conversion target:\n\
  -t, --to-code=NAME          encoding for output\n\
The default encoding is the current locale's encoding.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Output details:\n\
  -e, --no-escape             do not use C escapes in output (default)\n\
  -E, --escape                use C escapes in output, no extended chars\n\
      --force-po              write PO file even if empty\n\
  -i, --indent                indented output style\n\
      --no-location           suppress '#: filename:line' lines\n\
      --add-location          preserve '#: filename:line' lines (default)\n\
      --strict                strict Uniforum output style\n\
  -w, --width=NUMBER          set output page width\n\
  -s, --sort-output           generate sorted output and remove duplicates\n\
  -F, --sort-by-file          sort output by file location\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Informative output:\n\
  -h, --help                  display this help and exit\n\
  -V, --version               output version information and exit\n\
"));
      printf ("\n");
      fputs (_("Report bugs to <bug-gnu-utils@gnu.org>.\n"),
	     stdout);
    }

  exit (status);
}
