/* Edit translations using a subprocess.
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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "dir-list.h"
#include "error.h"
#include "progname.h"
#include "message.h"
#include "read-po.h"
#include "write-po.h"
#include "str-list.h"
#include "msgl-charset.h"
#include "xmalloc.h"
#include "system.h"
#include "findprog.h"
#include "pipe.h"
#include "wait-process.h"
#include "libgettext.h"

#define _(str) gettext (str)


/* We use a child process, and communicate through a bidirectional pipe.
   To avoid deadlocks, let the child process decide when it wants to read
   or to write, and let the parent behave accordingly.  The parent uses
   select() to know whether it must write or read.  On platforms without
   select(), we use non-blocking I/O.  (This means the parent is busy
   looping while waiting for the child.  Not good.)  */

/* On BeOS select() works only on sockets, not on normal file descriptors.  */
#ifdef __BeOS__
# undef HAVE_SELECT
#endif


/* Force output of PO file even if empty.  */
static int force_po;

/* Name of the subprogram.  */
static const char *sub_name;

/* Pathname of the subprogram.  */
static const char *sub_path;

/* Argument list for the subprogram.  */
static char **sub_argv;
static int sub_argc;

/* Long options.  */
static const struct option long_options[] =
{
  { "add-location", no_argument, &line_comment, 1 },
  { "directory", required_argument, NULL, 'D' },
  { "escape", no_argument, NULL, 'E' },
  { "force-po", no_argument, &force_po, 1 },
  { "help", no_argument, NULL, 'h' },
  { "indent", no_argument, NULL, CHAR_MAX + 1 },
  { "input", required_argument, NULL, 'i' },
  { "no-escape", no_argument, NULL, CHAR_MAX + 2 },
  { "no-location", no_argument, &line_comment, 0 },
  { "output-file", required_argument, NULL, 'o' },
  { "sort-by-file", no_argument, NULL, 'F' },
  { "sort-output", no_argument, NULL, 's' },
  { "strict", no_argument, NULL, 'S' },
  { "version", no_argument, NULL, 'V' },
  { "width", required_argument, NULL, 'w', },
  { NULL, 0, NULL, 0 }
};


/* Prototypes for local functions.  Needed to ensure compiler checking of
   function argument counts despite of K&R C function definition syntax.  */
static void usage PARAMS ((int status));
#ifdef EINTR
static inline int nonintr_close PARAMS ((int fd));
static inline ssize_t nonintr_read PARAMS ((int fd, void *buf, size_t count));
static inline ssize_t nonintr_write PARAMS ((int fd, const void *buf,
					     size_t count));
#if HAVE_SELECT
static inline int nonintr_select PARAMS ((int n, fd_set *readfds,
					  fd_set *writefds, fd_set *exceptfds,
					  struct timeval *timeout));
#endif
#endif
static void process_string PARAMS ((const char *str, size_t len,
				    char **resultp, size_t *lengthp));
static void process_message PARAMS ((message_ty *mp));
static void process_message_list PARAMS ((message_list_ty *mlp));
static msgdomain_list_ty *
       process_msgdomain_list PARAMS ((msgdomain_list_ty *mdlp));


int
main (argc, argv)
     int argc;
     char **argv;
{
  int opt;
  bool do_help;
  bool do_version;
  char *output_file;
  const char *input_file;
  msgdomain_list_ty *result;
  bool sort_by_filepos = false;
  bool sort_by_msgid = false;
  size_t i;

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

  /* Set default values for variables.  */
  do_help = false;
  do_version = false;
  output_file = NULL;
  input_file = NULL;

  /* The '+' in the options string causes option parsing to terminate when
     the first non-option, i.e. the subprogram name, is encountered.  */
  while ((opt = getopt_long (argc, argv, "+D:EFhio:sVw:", long_options, NULL))
	 != EOF)
    switch (opt)
      {
      case '\0':		/* Long option.  */
	break;

      case 'D':
	dir_list_append (optarg);
	break;

      case 'E':
	message_print_style_escape (true);
	break;

      case 'F':
	sort_by_filepos = true;
	break;

      case 'h':
	do_help = true;
	break;

      case 'i':
	if (input_file != NULL)
	  {
	    error (EXIT_SUCCESS, 0, _("at most one input file allowed"));
	    usage (EXIT_FAILURE);
	  }
	input_file = optarg;
	break;

      case 'o':
	output_file = optarg;
	break;

      case 's':
	sort_by_msgid = true;
	break;

      case 'S':
	message_print_style_uniforum ();
	break;

      case 'V':
	do_version = true;
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

      case CHAR_MAX + 1:
	message_print_style_indent ();
	break;

      case CHAR_MAX + 2:
	message_print_style_escape (false);
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

  /* Test for the subprogram name.  */
  if (optind == argc)
    error (EXIT_FAILURE, 0, _("missing filter name"));
  sub_name = argv[optind];

  /* Verify selected options.  */
  if (!line_comment && sort_by_filepos)
    error (EXIT_FAILURE, 0, _("%s and %s are mutually exclusive"),
	   "--no-location", "--sort-by-file");

  if (sort_by_msgid && sort_by_filepos)
    error (EXIT_FAILURE, 0, _("%s and %s are mutually exclusive"),
	   "--sort-output", "--sort-by-file");

  /* Build argument list for the program.  */
  sub_argc = argc - optind;
  sub_argv = (char **) xmalloc ((sub_argc + 1) * sizeof (char *));
  for (i = 0; i < sub_argc; i++)
    sub_argv[i] = argv[optind + i];
  sub_argv[i] = NULL;

  /* Extra checks for sed scripts.  */
  if (strcmp (sub_name, "sed") == 0)
    {
      if (sub_argc == 1)
	error (EXIT_FAILURE, 0,
	       _("at least one sed script must be specified"));

      /* Replace GNU sed specific options with portable sed options.  */
      for (i = 1; i < sub_argc; i++)
	{
	  if (strcmp (sub_argv[i], "--expression") == 0)
	    sub_argv[i] = "-e";
	  else if (strcmp (sub_argv[i], "--file") == 0)
	    sub_argv[i] = "-f";
	  else if (strcmp (sub_argv[i], "--quiet") == 0
		   || strcmp (sub_argv[i], "--silent") == 0)
	    sub_argv[i] = "-n";

	  if (strcmp (sub_argv[i], "-e") == 0
	      || strcmp (sub_argv[i], "-f") == 0)
	    i++;
	}
    }

  /* By default, input comes from standard input.  */
  if (input_file == NULL)
    input_file = "-";

  /* Read input file.  */
  result = read_po_file (input_file);

  /* Warn if the current locale is not suitable for this PO file.  */
  compare_po_locale_charsets (result);

  /* Attempt to locate the program.
     This is an optimization, to avoid that spawn/exec searches the PATH
     on every call.  */
  sub_path = find_in_path (sub_name);

  /* Finish argument list for the program.  */
  sub_argv[0] = (char *) sub_path;

  /* Apply the subprogram.  */
  result = process_msgdomain_list (result);

  /* Sort the results.  */
  if (sort_by_filepos)
    msgdomain_list_sort_by_filepos (result);
  else if (sort_by_msgid)
    msgdomain_list_sort_by_msgid (result);

  /* Write the merged message list out.  */
  msgdomain_list_print (result, output_file, force_po, false);

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
Usage: %s [OPTION] FILTER [FILTER-OPTION]\n\
"), program_name);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Applies a filter to all translations of a translation catalog.\n\
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
  -i, --input=INPUTFILE       input PO file\n\
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
The FILTER can be any program that reads a translation from standard input\n\
and writes a modified translation to standard output.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Useful FILTER-OPTIONs when the FILTER is 'sed':\n\
  -e, --expression=SCRIPT     add SCRIPT to the commands to be executed\n\
  -f, --file=SCRIPTFILE       add the contents of SCRIPTFILE to the commands\n\
                                to be executed\n\
  -n, --quiet, --silent       suppress automatic printing of pattern space\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Output details:\n\
      --no-escape             do not use C escapes in output (default)\n\
  -E, --escape                use C escapes in output, no extended chars\n\
      --force-po              write PO file even if empty\n\
      --indent                indented output style\n\
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
      fputs (_("Report bugs to <bug-gnu-gettext@gnu.org>.\n"),
	     stdout);
    }

  exit (status);
}


#ifdef EINTR

/* EINTR handling for close(), read(), write(), select().
   These functions can return -1/EINTR even though we don't have any
   signal handlers set up, namely when we get interrupted via SIGSTOP.  */

static inline int
nonintr_close (fd)
     int fd;
{
  int retval;

  do
    retval = close (fd);
  while (retval < 0 && errno == EINTR);

  return retval;
}
#define close nonintr_close

static inline ssize_t
nonintr_read (fd, buf, count)
     int fd;
     void *buf;
     size_t count;
{
  ssize_t retval;

  do
    retval = read (fd, buf, count);
  while (retval < 0 && errno == EINTR);

  return retval;
}
#define read nonintr_read

static inline ssize_t
nonintr_write (fd, buf, count)
     int fd;
     const void *buf;
     size_t count;
{
  ssize_t retval;

  do
    retval = write (fd, buf, count);
  while (retval < 0 && errno == EINTR);

  return retval;
}
#define write nonintr_write

# if HAVE_SELECT

static inline int
nonintr_select (n, readfds, writefds, exceptfds, timeout)
     int n;
     fd_set *readfds;
     fd_set *writefds;
     fd_set *exceptfds;
     struct timeval *timeout;
{
  int retval;

  do
    retval = select (n, readfds, writefds, exceptfds, timeout);
  while (retval < 0 && errno == EINTR);

  return retval;
}
#define select nonintr_select

# endif

#endif


/* Non-blocking I/O.  */
#ifndef O_NONBLOCK
# define O_NONBLOCK O_NDELAY
#endif
#if HAVE_SELECT
# define IS_EAGAIN(errcode) 0
#else
# ifdef EWOULDBLOCK
#  define IS_EAGAIN(errcode) ((errcode) == EAGAIN || (errcode) == EWOULDBLOCK)
# else
#  define IS_EAGAIN(errcode) ((errcode) == EAGAIN)
# endif
#endif

/* Process a string STR of size LEN bytes through the subprogram, then
   remove NUL bytes.
   Store the freshly allocated result at *RESULTP and its length at *LENGTHP.
 */
static void
process_string (str, len, resultp, lengthp)
     const char *str;
     size_t len;
     char **resultp;
     size_t *lengthp;
{
  pid_t child;
  int fd[2];
  char *result;
  size_t allocated;
  size_t length;
  int exitstatus;

  /* Open a bidirectional pipe to a subprocess.  */
  child = create_pipe_bidi (sub_name, sub_path, sub_argv, fd);
     
  /* Enable non-blocking I/O.  This permits the read() and write() calls
     to return -1/EAGAIN without blocking; this is important for polling
     if HAVE_SELECT is not defined.  It also permits the read() and write()
     calls to return after partial reads/writes; this is important if
     HAVE_SELECT is defined, because select() only says that some data
     can be read or written, not how many.  Without non-blocking I/O,
     Linux 2.2.17 and BSD systems prefer to block instead of returning
     with partial results.  */
  {
    int fcntl_flags;

    if ((fcntl_flags = fcntl (fd[1], F_GETFL, 0)) < 0
	|| fcntl (fd[1], F_SETFL, fcntl_flags | O_NONBLOCK) < 0
	|| (fcntl_flags = fcntl (fd[0], F_GETFL, 0)) < 0
	|| fcntl (fd[0], F_SETFL, fcntl_flags | O_NONBLOCK) < 0)
      error (EXIT_FAILURE, errno,
	     _("cannot set up nonblocking I/O to %s subprocess"), sub_name);
  }

  allocated = len + (len >> 2) + 1;
  result = (char *) xmalloc (allocated);
  length = 0;

  for (;;)
    {
#if HAVE_SELECT
      int n;
      fd_set readfds;
      fd_set writefds;

      FD_ZERO (&readfds);
      FD_SET (fd[0], &readfds);
      n = fd[0] + 1;
      if (str != NULL)
	{
	  FD_ZERO (&writefds);
	  FD_SET (fd[1], &writefds);
	  if (n <= fd[1])
	    n = fd[1] + 1;
	}

      n = select (n, &readfds, (str != NULL ? &writefds : NULL), NULL, NULL);
      if (n < 0)
	error (EXIT_FAILURE, errno,
	       _("communication with %s subprocess failed"), sub_name);
      if (str != NULL && FD_ISSET (fd[1], &writefds))
	goto try_write;
      if (FD_ISSET (fd[0], &readfds))
	goto try_read;
      /* How could select() return if none of the two descriptors is ready?  */
      abort ();
#endif

      /* Attempt to write.  */
#if HAVE_SELECT
    try_write:
#endif
      if (str != NULL)
	{
	  if (len > 0)
	    {
	      ssize_t nwritten = write (fd[1], str, len);
	      if (nwritten < 0 && !IS_EAGAIN (errno))
		error (EXIT_FAILURE, errno,
		       _("write to %s subprocess failed"), sub_name);
	      if (nwritten > 0)
		{
		  str += nwritten;
		  len -= nwritten;
		}
	    }
	  else
	    {
	      /* Tell the child there is nothing more the parent will send.  */
	      close (fd[1]);
	      str = NULL;
	    }
	}
#if HAVE_SELECT
      continue;
#endif

      /* Attempt to read.  */
#if HAVE_SELECT
    try_read:
#endif
      if (length == allocated)
	{
	  allocated = allocated + (allocated >> 1);
	  result = xrealloc (result, allocated);
	}
      {
	ssize_t nread = read (fd[0], result + length, allocated - length);
	if (nread < 0 && !IS_EAGAIN (errno))
	  error (EXIT_FAILURE, errno,
		 _("read from %s subprocess failed"), sub_name);
	if (nread > 0)
	  length += nread;
	if (nread == 0 && str == NULL)
	  break;
      }
#if HAVE_SELECT
      continue;
#endif
    }

  close (fd[0]);

  /* Remove zombie process from process list.  */
  exitstatus = wait_subprocess (child, sub_name);
  if (exitstatus != 0)
    error (EXIT_FAILURE, 0, _("%s subprocess terminated with exit code %d"),
	   sub_name, exitstatus);

  /* Remove NUL bytes from result.  */
  {
    char *p = result;
    char *pend = result + length;

    for (; p < pend; p++)
      if (*p == '\0')
	{
	  char *q;

	  q = p;
	  for (; p < pend; p++)
	    if (*p != '\0')
	      *q++ = *p;
	  length = q - result;
	  break;
	}
  }

  *resultp = result;
  *lengthp = length;
}


static void
process_message (mp)
     message_ty *mp;
{
  const char *msgstr = mp->msgstr;
  size_t msgstr_len = mp->msgstr_len;
  size_t nsubstrings;
  char **substrings;
  size_t total_len;
  char *total_str;
  const char *p;
  char *q;
  size_t k;

  /* Count NUL delimited substrings.  */
  for (p = msgstr, nsubstrings = 0;
       p < msgstr + msgstr_len;
       p += strlen (p) + 1, nsubstrings++);

  /* Process each NUL delimited substring separately.  */
  substrings = (char **) xmalloc (nsubstrings * sizeof (char *));
  for (p = msgstr, k = 0, total_len = 0; k < nsubstrings; k++)
    {
      char *result;
      size_t length;

      process_string (p, strlen (p), &result, &length);
      result = xrealloc (result, length + 1);
      result[length] = '\0';
      substrings[k] = result;
      total_len += length + 1;
    }

  /* Concatenate the results, including the NUL after each.  */
  total_str = (char *) xmalloc (total_len);
  for (k = 0, q = total_str; k < nsubstrings; k++)
    {
      size_t length = strlen (substrings[k]);

      memcpy (q, substrings[k], length + 1);
      free (substrings[k]);
      q += length + 1;
    }
  free (substrings);

  mp->msgstr = total_str;
  mp->msgstr_len = total_len;
}


static void
process_message_list (mlp)
     message_list_ty *mlp;
{
  size_t j;

  for (j = 0; j < mlp->nitems; j++)
    process_message (mlp->item[j]);
}


static msgdomain_list_ty *
process_msgdomain_list (mdlp)
     msgdomain_list_ty *mdlp;
{
  size_t k;

  for (k = 0; k < mdlp->nitems; k++)
    process_message_list (mdlp->item[k]->messages);

  return mdlp;
}
