/* Converts Uniforum style .po files to binary .mo files
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

#include "dir-list.h"
#include "error.h"
#include "progname.h"
#include "xerror.h"
#include "getline.h"
#include "printf.h"
#include <system.h>

#include "gettext.h"
#include "hash-string.h"
#include "libgettext.h"
#include "message.h"
#include "po.h"

#define _(str) gettext (str)

#ifndef errno
extern int errno;
#endif

#define SIZEOF(a) (sizeof(a) / sizeof(a[0]))

/* Define the data structure which we need to represent the data to
   be written out.  */
struct id_str_pair
{
  char *id;
  size_t id_len;
  char *id_plural;
  size_t id_plural_len;
  char *str;
  size_t str_len;
};

/* Contains information about the definition of one translation.  */
struct hashtable_entry
{
  char *msgid_plural;
  char *msgstr;
  size_t msgstr_len;
  lex_pos_ty pos;
};

/* This structure defines a derived class of the po_ty class.  (See
   po.h for an explanation.)  */
typedef struct msgfmt_class_ty msgfmt_class_ty;
struct msgfmt_class_ty
{
  /* inherited instance variables, etc */
  PO_BASE_TY

  bool is_fuzzy;
  enum is_c_format is_c_format;
  enum is_wrap do_wrap;

  bool has_header_entry;
};

/* Alignment of strings in resulting .mo file.  */
static size_t alignment;

/* Contains exit status for case in which no premature exit occurs.  */
static int exit_status;

/* If true include even fuzzy translations in output file.  */
static bool include_all = false;

/* Nonzero if no hash table in .mo is wanted.  */
static int no_hash_table;

/* Specifies name of the output file.  */
static const char *output_file_name;

/* We may have more than one input file.  Domains with same names in
   different files have to merged.  So we need a list of tables for
   each output file.  */
struct msg_domain
{
  /* Table for mapping message IDs to message strings.  */
  hash_table symbol_tab;
  /* Name of domain these ID/String pairs are part of.  */
  const char *domain_name;
  /* Output file name.  */
  const char *file_name;
  /* Link to the next domain.  */
  struct msg_domain *next;
};
static struct msg_domain *domain_list;
static struct msg_domain *current_domain;

/* If not zero list duplicate message identifiers.  */
static int verbose_level;

/* If true check strings according to format string rules for the
   language.  */
static bool do_check = false;

/* Counters for statistics on translations for the processed files.  */
static int msgs_translated;
static int msgs_untranslated;
static int msgs_fuzzy;

/* If not zero print statistics about translation at the end.  */
static int do_statistics;

/* Long options.  */
static const struct option long_options[] =
{
  { "alignment", required_argument, NULL, 'a' },
  { "check", no_argument, NULL, 'c' },
  { "directory", required_argument, NULL, 'D' },
  { "help", no_argument, NULL, 'h' },
  { "no-hash", no_argument, &no_hash_table, 1 },
  { "output-file", required_argument, NULL, 'o' },
  { "statistics", no_argument, &do_statistics, 1 },
  { "strict", no_argument, NULL, 'S' },
  { "use-fuzzy", no_argument, NULL, 'f' },
  { "verbose", no_argument, NULL, 'v' },
  { "version", no_argument, NULL, 'V' },
  { NULL, 0, NULL, 0 }
};


#ifndef roundup
# if defined __GNUC__ && __GNUC__ >= 2
#  define roundup(x, y) ({typeof(x) _x = (x); typeof(y) _y = (y); \
			  ((_x + _y - 1) / _y) * _y; })
# else
#  define roundup(x, y) ((((x)+((y)-1))/(y))*(y))
# endif	/* GNU CC2  */
#endif /* roundup  */

/* Prototypes for local functions.  Needed to ensure compiler checking of
   function argument counts despite of K&R C function definition syntax.  */
static void usage PARAMS ((int status))
#if defined __GNUC__ && ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
	__attribute__ ((noreturn))
#endif
;
static void grammar PARAMS ((char *filename));
static void format_constructor PARAMS ((po_ty *that));
static void format_directive_domain PARAMS ((po_ty *pop, char *name));
static void format_directive_message PARAMS ((po_ty *pop, char *msgid,
					      lex_pos_ty *msgid_pos,
					      char *msgid_plural,
					      char *msgstr, size_t msgstr_len,
					      lex_pos_ty *msgstr_pos,
					      bool obsolete));
static void format_comment_special PARAMS ((po_ty *pop, const char *s));
static void format_debrief PARAMS ((po_ty *));
static struct msg_domain *new_domain PARAMS ((const char *name,
					      const char *file_name));
static int compare_id PARAMS ((const void *pval1, const void *pval2));
static void write_table PARAMS ((FILE *output_file, hash_table *tab));
static void check_pair PARAMS ((const char *msgid, const lex_pos_ty *msgid_pos,
				const char *msgid_plural,
				const char *msgstr, size_t msgstr_len,
				const lex_pos_ty *msgstr_pos, int is_format));
static const char *add_mo_suffix PARAMS ((const char *));


int
main (argc, argv)
     int argc;
     char *argv[];
{
  int opt;
  bool do_help = false;
  bool do_version = false;
  bool strict_uniforum = false;
  struct msg_domain *domain;

  /* Set default value for global variables.  */
  alignment = DEFAULT_OUTPUT_ALIGNMENT;

  /* Set program name for messages.  */
  set_program_name (argv[0]);
  error_print_progname = maybe_print_progname;
  error_one_per_line = 1;
  exit_status = EXIT_SUCCESS;

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  while ((opt = getopt_long (argc, argv, "a:cD:fho:vV", long_options, NULL))
	 != EOF)
    switch (opt)
      {
      case '\0':		/* Long option.  */
	break;
      case 'a':
	{
	  char *endp;
	  size_t new_align = strtoul (optarg, &endp, 0);

	  if (endp != optarg)
	    alignment = new_align;
	}
	break;
      case 'c':
	do_check = true;
	break;
      case 'D':
	dir_list_append (optarg);
	break;
      case 'f':
	include_all = true;
	break;
      case 'h':
	do_help = true;
	break;
      case 'o':
	output_file_name = optarg;
	break;
      case 'S':
	strict_uniforum = true;
	break;
      case 'v':
	++verbose_level;
	break;
      case 'V':
	do_version = true;
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

  /* Test whether we have a .po file name as argument.  */
  if (optind >= argc)
    {
      error (EXIT_SUCCESS, 0, _("no input file given"));
      usage (EXIT_FAILURE);
    }

  /* The -o option determines the name of the domain and therefore
     the output file.  */
  if (output_file_name != NULL)
    current_domain =
      new_domain (output_file_name,
		  strict_uniforum ? add_mo_suffix (output_file_name)
				  : output_file_name);

  /* Prepare PO file reader.  We need to see the comments because inexact
     translations must be reported.  */
  po_lex_pass_comments (true);

  /* Now write out all domains.  */
  /* Process all given .po files.  */
  while (argc > optind)
    {
      /* Remember that we currently have not specified any domain.  This
	 is of course not true when we saw the -o option.  */
      if (output_file_name == NULL)
	current_domain = NULL;

      /* And process the input file.  */
      grammar (argv[optind]);

      ++optind;
    }

  for (domain = domain_list; domain != NULL; domain = domain->next)
    {
      FILE *output_file;

      /* If no entry for this domain don't even create the file.  */
      if (domain->symbol_tab.filled != 0)
	{
	  if (strcmp (domain->domain_name, "-") == 0)
	    {
	      output_file = stdout;
	      SET_BINARY (fileno (output_file));
	    }
	  else
	    {
	      const char *fname = domain->file_name;

	      output_file = fopen (fname, "wb");
	      if (output_file == NULL)
		{
		  error (0, errno,
			 _("error while opening \"%s\" for writing"), fname);
		  exit_status = EXIT_FAILURE;
		}
	    }

	  if (output_file != NULL)
	    {
	      write_table (output_file, &domain->symbol_tab);
	      if (output_file != stdout)
		fclose (output_file);
	    }
	}
    }

  /* Print statistics if requested.  */
  if (verbose_level > 0 || do_statistics)
    {
      fprintf (stderr,
	       ngettext ("%d translated message", "%d translated messages",
			 msgs_translated),
	       msgs_translated);
      if (msgs_fuzzy > 0)
	fprintf (stderr,
		 ngettext (", %d fuzzy translation", ", %d fuzzy translations",
			   msgs_fuzzy),
		 msgs_fuzzy);
      if (msgs_untranslated > 0)
	fprintf (stderr,
		 ngettext (", %d untranslated message",
			   ", %d untranslated messages",
			   msgs_untranslated),
		 msgs_untranslated);
      fputs (".\n", stderr);
    }

  exit (exit_status);
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
Usage: %s [OPTION] filename.po ...\n\
"), program_name);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Generate binary message catalog from textual translation description.\n\
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
  filename.po ...             input files\n\
  -D, --directory=DIRECTORY   add DIRECTORY to list for input files search\n\
If input file is -, standard input is read.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Output file location:\n\
  -o, --output-file=FILE      write output to specified file\n\
      --strict                enable strict Uniforum mode\n\
If output file is -, output is written to standard output.\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Input file interpretation:\n\
  -c, --check                 perform language dependent checks on strings\n\
  -f, --use-fuzzy             use fuzzy entries in output\n\
"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Output details:\n\
  -a, --alignment=NUMBER      align strings to NUMBER bytes (default: %d)\n\
      --no-hash               binary file will not include the hash table\n\
"), DEFAULT_OUTPUT_ALIGNMENT);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Informative output:\n\
  -h, --help                  display this help and exit\n\
  -V, --version               output version information and exit\n\
      --statistics            print statistics about translations\n\
  -v, --verbose               list input file anomalies\n\
Giving the -v option more than once increases the verbosity level.\n\
"));
      printf ("\n");
      fputs (_("Report bugs to <bug-gnu-gettext@gnu.org>.\n"), stdout);
    }

  exit (status);
}


static struct msg_domain *
new_domain (name, file_name)
     const char *name;
     const char *file_name;
{
  struct msg_domain **p_dom = &domain_list;

  while (*p_dom != NULL && strcmp (name, (*p_dom)->domain_name) != 0)
    p_dom = &(*p_dom)->next;

  if (*p_dom == NULL)
    {
      struct msg_domain *domain;

      domain = (struct msg_domain *) xmalloc (sizeof (struct msg_domain));
      if (init_hash (&domain->symbol_tab, 100) != 0)
	error (EXIT_FAILURE, errno, _("while creating hash table"));
      domain->domain_name = name;
      domain->file_name = file_name;
      domain->next = NULL;
      *p_dom = domain;
    }

  return *p_dom;
}


/* Prepare for first message.  */
static void
format_constructor (that)
     po_ty *that;
{
  msgfmt_class_ty *this = (msgfmt_class_ty *) that;

  this->is_fuzzy = false;
  this->is_c_format = undecided;
  this->do_wrap = undecided;
  this->has_header_entry = false;
}


/* Some checks after whole file is read.  */
static void
format_debrief (that)
     po_ty *that;
{
  msgfmt_class_ty *this = (msgfmt_class_ty *) that;

  /* Test whether header entry was found.
     FIXME: Should do this even if not in verbose mode, because the
     consequences are not harmless.  But it breaks the test suite.  */
  if (verbose_level > 0 && !this->has_header_entry)
    {
      multiline_error (xasprintf ("%s: ", gram_pos.file_name),
		       xasprintf (_("\
warning: PO file header missing, fuzzy, or invalid\n")));
      multiline_error (NULL,
		       xasprintf (_("\
warning: charset conversion will not work\n")));
    }
}


/* Process `domain' directive from .po file.  */
static void
format_directive_domain (pop, name)
     po_ty *pop;
     char *name;
{
  /* If no output file was given, we change it with each `domain'
     directive.  */
  if (output_file_name == NULL)
    {
      size_t correct;

      correct = strcspn (name, INVALID_PATH_CHAR);
      if (name[correct] != '\0')
	{
	  exit_status = EXIT_FAILURE;
	  if (correct == 0)
	    {
	      error (0, 0, _("\
domain name \"%s\" not suitable as file name"), name);
	      return;
	    }
	  else
	    error (0, 0, _("\
domain name \"%s\" not suitable as file name: will use prefix"), name);
	  name[correct] = '\0';
	}

      /* Set new domain.  */
      current_domain = new_domain (name, add_mo_suffix (name));
    }
  else
    {
      if (verbose_level > 0)
	/* We don't change the exit status here because this is really
	   only an information.  */
	error (0, 0, _("`domain %s' directive ignored"), name);

      /* NAME was allocated in po-gram.y but is not used anywhere.  */
      free (name);
    }
}


/* Process `msgid'/`msgstr' pair from .po file.  */
static void
format_directive_message (that, msgid_string, msgid_pos, msgid_plural,
			  msgstr_string, msgstr_len, msgstr_pos, obsolete)
     po_ty *that;
     char *msgid_string;
     lex_pos_ty *msgid_pos;
     char *msgid_plural;
     char *msgstr_string;
     size_t msgstr_len;
     lex_pos_ty *msgstr_pos;
     bool obsolete;
{
  msgfmt_class_ty *this = (msgfmt_class_ty *) that;
  struct hashtable_entry *entry;

  /* Don't emit untranslated entries.  Also don't emit fuzzy entries, unless
     --use-fuzzy was specified.  But ignore fuzziness of the header entry.  */
  if (msgstr_string[0] == '\0'
      || (!include_all && this->is_fuzzy && msgid_string[0] != '\0'))
    {
      if (verbose_level > 1)
	{
	  /* We don't change the exit status here because this is really
	     only an information.  */
	  error_with_progname = false;
	  error_at_line (0, 0, msgstr_pos->file_name, msgstr_pos->line_number,
			 (msgstr_string[0] == '\0'
			  ? _("empty `msgstr' entry ignored")
			  : _("fuzzy `msgstr' entry ignored")));
	  error_with_progname = true;
	}

      /* Increment counter for fuzzy/untranslated messages.  */
      if (msgstr_string[0] == '\0')
	++msgs_untranslated;
      else
	++msgs_fuzzy;

      /* Free strings allocated in po-gram.y.  */
      free (msgstr_string);
    }
  else
    {
      /* Test for header entry.  */
      if (msgid_string[0] == '\0')
	{
	  this->has_header_entry = true;

	  /* Do some more tests on test contents of the header entry.  */
	  if (verbose_level > 0)
	    {
	      static const char *required_fields[] =
	      {
		"Project-Id-Version", "PO-Revision-Date",
		"Last-Translator", "Language-Team", "MIME-Version",
		"Content-Type", "Content-Transfer-Encoding"
	      };
	      static const char *default_values[] =
	      {
		"PACKAGE VERSION", "YEAR-MO-DA", "FULL NAME", "LANGUAGE",
		NULL, "text/plain; charset=CHARSET", "ENCODING"
	      };
	      const size_t nfields = SIZEOF (required_fields);
	      int initial = -1;
	      int cnt;

	      for (cnt = 0; cnt < nfields; ++cnt)
		{
		  char *endp = strstr (msgstr_string, required_fields[cnt]);

		  if (endp == NULL)
		    error (0, 0, _("headerfield `%s' missing in header"),
			   required_fields[cnt]);
		  else if (endp != msgstr_string && endp[-1] != '\n')
		    error (0, 0, _("\
header field `%s' should start at beginning of line"),
			   required_fields[cnt]);
		  else if (default_values[cnt] != NULL
			   && strncmp (default_values[cnt],
				       endp + strlen (required_fields[cnt]) + 2,
				       strlen (default_values[cnt])) == 0)
		    {
		      if (initial != -1)
			{
			  error (0, 0, _("\
some header fields still have the initial default value"));
			  initial = -1;
			  break;
			}
		      else
			initial = cnt;
		    }
		}

	      if (initial != -1)
		error (0, 0, _("field `%s' still has initial default value"),
		       required_fields[initial]);
	    }
	}
      else
	/* We don't count the header entry in the statistic so place the
	   counter incrementation here.  */
	if (this->is_fuzzy)
	  ++msgs_fuzzy;
	else
	  ++msgs_translated;

      /* We found a valid pair of msgid/msgstr.
	 Construct struct to describe msgstr definition.  */
      entry = (struct hashtable_entry *) xmalloc (sizeof (*entry));

      entry->msgid_plural = msgid_plural;
      entry->msgstr = msgstr_string;
      entry->msgstr_len = msgstr_len;
      entry->pos = *msgstr_pos;

      /* Do some more checks on both strings.  */
      check_pair (msgid_string, msgid_pos, msgid_plural,
		  msgstr_string, msgstr_len, msgstr_pos,
		  do_check && possible_c_format_p (this->is_c_format));

      /* Check whether already a domain is specified.  If not use default
	 domain.  */
      if (current_domain == NULL)
	current_domain = new_domain (MESSAGE_DOMAIN_DEFAULT,
				     add_mo_suffix (MESSAGE_DOMAIN_DEFAULT));

      /* We insert the ID/string pair into the hashing table.  But we have
	 to take care for duplicates.  */
      if (insert_entry (&current_domain->symbol_tab, msgid_string,
			strlen (msgid_string) + 1, entry))
	{
	  /* We don't need the just constructed entry.  */
	  free (entry);

	  if (verbose_level > 0)
	    {
	      /* We give a fatal error about this, but only if the
		 translations are different.  Tell the user the old
		 definition for reference.  */
	      find_entry (&current_domain->symbol_tab, msgid_string,
			  strlen (msgid_string) + 1, (void **) &entry);
	      if (msgstr_len != entry->msgstr_len
		  || memcmp (msgstr_string, entry->msgstr, msgstr_len) != 0)
		{
		  po_gram_error_at_line (msgid_pos, _("\
duplicate message definition"));
		  po_gram_error_at_line (&entry->pos, _("\
...this is the location of the first definition"));

		  /* FIXME Should this be always a reason for an
		     exit status != 0?  */
		  exit_status = EXIT_FAILURE;
		}
	    }

	  /* We don't need the just constructed entries'
	     parameter string (allocated in po-gram.y).  */
	  free (msgstr_string);
	}
  }

  /* We do not need the msgid string in any case.  */
  free (msgid_string);

  /* Prepare for next message.  */
  this->is_fuzzy = false;
  this->is_c_format = undecided;
  this->do_wrap = undecided;
}


/* Test for `#, fuzzy' comments and warn.  */
static void
format_comment_special (that, s)
     po_ty *that;
     const char *s;
{
  msgfmt_class_ty *this = (msgfmt_class_ty *) that;

  if (strstr (s, "fuzzy") != NULL)
    {
      static bool warned = false;

      if (!include_all && verbose_level > 1 && !warned)
	{
	  warned = true;
	  error (0, 0, _("\
%s: warning: source file contains fuzzy translation"),
		 gram_pos.file_name);
	}

      this->is_fuzzy = true;
    }
  this->is_c_format = parse_c_format_description_string (s);
  this->do_wrap = parse_c_width_description_string (s);
}


static int
compare_id (pval1, pval2)
     const void *pval1;
     const void *pval2;
{
  return strcmp (((struct id_str_pair *) pval1)->id,
		 ((struct id_str_pair *) pval2)->id);
}


static void
write_table (output_file, tab)
     FILE *output_file;
     hash_table *tab;
{
  static char null = '\0';
  /* This should be explained:
     Each string has an associate hashing value V, computed by a fixed
     function.  To locate the string we use open addressing with double
     hashing.  The first index will be V % M, where M is the size of the
     hashing table.  If no entry is found, iterating with a second,
     independent hashing function takes place.  This second value will
     be 1 + V % (M - 2).
     The approximate number of probes will be

       for unsuccessful search:  (1 - N / M) ^ -1
       for successful search:    - (N / M) ^ -1 * ln (1 - N / M)

     where N is the number of keys.

     If we now choose M to be the next prime bigger than 4 / 3 * N,
     we get the values
			 4   and   1.85  resp.
     Because unsuccesful searches are unlikely this is a good value.
     Formulas: [Knuth, The Art of Computer Programming, Volume 3,
		Sorting and Searching, 1973, Addison Wesley]  */
  nls_uint32 hash_tab_size = no_hash_table ? 0
					   : next_prime ((tab->filled * 4)
							 / 3);
  nls_uint32 *hash_tab;

  /* Header of the .mo file to be written.  */
  struct mo_file_header header;
  struct id_str_pair *msg_arr;
  void *ptr;
  size_t cnt;
  const void *id;
  size_t id_len;
  struct hashtable_entry *entry;
  struct string_desc sd;

  /* Fill the structure describing the header.  */
  header.magic = _MAGIC;		/* Magic number.  */
  header.revision = MO_REVISION_NUMBER;	/* Revision number of file format.  */
  header.nstrings = tab->filled;	/* Number of strings.  */
  header.orig_tab_offset = sizeof (header);
			/* Offset of table for original string offsets.  */
  header.trans_tab_offset = sizeof (header)
			    + tab->filled * sizeof (struct string_desc);
			/* Offset of table for translation string offsets.  */
  header.hash_tab_size = hash_tab_size;	/* Size of used hashing table.  */
  header.hash_tab_offset =
	no_hash_table ? 0 : sizeof (header)
			    + 2 * (tab->filled * sizeof (struct string_desc));
			/* Offset of hashing table.  */

  /* Write the header out.  */
  fwrite (&header, sizeof (header), 1, output_file);

  /* Allocate table for the all elements of the hashing table.  */
  msg_arr = (struct id_str_pair *) alloca (tab->filled * sizeof (msg_arr[0]));

  /* Read values from hashing table into array.  */
  for (cnt = 0, ptr = NULL;
       iterate_table (tab, &ptr, &id, &id_len, (void **) &entry) >= 0;
       ++cnt)
    {
      msg_arr[cnt].id = (char *) id;
      msg_arr[cnt].id_len = id_len;
      msg_arr[cnt].id_plural = entry->msgid_plural;
      msg_arr[cnt].id_plural_len =
	(entry->msgid_plural != NULL ? strlen (entry->msgid_plural) + 1 : 0);
      msg_arr[cnt].str = entry->msgstr;
      msg_arr[cnt].str_len = entry->msgstr_len;
    }

  /* Sort the table according to original string.  */
  qsort (msg_arr, tab->filled, sizeof (msg_arr[0]), compare_id);

  /* Set offset to first byte after all the tables.  */
  sd.offset = roundup (sizeof (header)
		       + tab->filled * sizeof (sd)
		       + tab->filled * sizeof (sd)
		       + hash_tab_size * sizeof (nls_uint32),
		       alignment);

  /* Write out length and starting offset for all original strings.  */
  for (cnt = 0; cnt < tab->filled; ++cnt)
    {
      /* Subtract 1 because of the terminating NUL.  */
      sd.length = msg_arr[cnt].id_len + msg_arr[cnt].id_plural_len - 1;
      fwrite (&sd, sizeof (sd), 1, output_file);
      sd.offset += roundup (sd.length + 1, alignment);
    }

  /* Write out length and starting offset for all translation strings.  */
  for (cnt = 0; cnt < tab->filled; ++cnt)
    {
      /* Subtract 1 because of the terminating NUL.  */
      sd.length = msg_arr[cnt].str_len - 1;
      fwrite (&sd, sizeof (sd), 1, output_file);
      sd.offset += roundup (sd.length + 1, alignment);
    }

  /* Skip this part when no hash table is needed.  */
  if (!no_hash_table)
    {
      /* Allocate room for the hashing table to be written out.  */
      hash_tab = (nls_uint32 *) alloca (hash_tab_size * sizeof (nls_uint32));
      memset (hash_tab, '\0', hash_tab_size * sizeof (nls_uint32));

      /* Insert all value in the hash table, following the algorithm described
	 above.  */
      for (cnt = 0; cnt < tab->filled; ++cnt)
	{
	  nls_uint32 hash_val = hash_string (msg_arr[cnt].id);
	  nls_uint32 idx = hash_val % hash_tab_size;

	  if (hash_tab[idx] != 0)
	    {
	      /* We need the second hashing function.  */
	      nls_uint32 c = 1 + (hash_val % (hash_tab_size - 2));

	      do
		if (idx >= hash_tab_size - c)
		  idx -= hash_tab_size - c;
		else
		  idx += c;
	      while (hash_tab[idx] != 0);
	    }

	  hash_tab[idx] = cnt + 1;
	}

      /* Write the hash table out.  */
      fwrite (hash_tab, sizeof (nls_uint32), hash_tab_size, output_file);
    }

  /* Write bytes to make first string to be aligned.  */
  cnt = sizeof (header) + 2 * tab->filled * sizeof (sd)
	+ hash_tab_size * sizeof (nls_uint32);
  fwrite (&null, 1, roundup (cnt, alignment) - cnt, output_file);

  /* Now write the original strings.  */
  for (cnt = 0; cnt < tab->filled; ++cnt)
    {
      size_t len = msg_arr[cnt].id_len + msg_arr[cnt].id_plural_len;

      fwrite (msg_arr[cnt].id, msg_arr[cnt].id_len, 1, output_file);
      if (msg_arr[cnt].id_plural_len > 0)
	fwrite (msg_arr[cnt].id_plural, msg_arr[cnt].id_plural_len, 1,
		output_file);
      fwrite (&null, 1, roundup (len, alignment) - len, output_file);
    }

  /* Now write the translation strings.  */
  for (cnt = 0; cnt < tab->filled; ++cnt)
    {
      size_t len = msg_arr[cnt].str_len;

      fwrite (msg_arr[cnt].str, len, 1, output_file);
      fwrite (&null, 1, roundup (len, alignment) - len, output_file);

      free (msg_arr[cnt].str);
    }

  /* Hashing table is not used anmore.  */
  delete_hash (tab);
}


static void
check_pair (msgid, msgid_pos, msgid_plural, msgstr, msgstr_len, msgstr_pos,
	    is_format)
     const char *msgid;
     const lex_pos_ty *msgid_pos;
     const char *msgid_plural;
     const char *msgstr;
     size_t msgstr_len;
     const lex_pos_ty *msgstr_pos;
     int is_format;
{
  int has_newline;
  unsigned int i;
  const char *p;
  size_t nidfmts, nstrfmts;

  /* If the msgid string is empty we have the special entry reserved for
     information about the translation.  */
  if (msgid[0] == '\0')
    return;

  /* Test 1: check whether all or none of the strings begin with a '\n'.  */
  has_newline = (msgid[0] == '\n');
#define TEST_NEWLINE(p) (p[0] == '\n')
  if (msgid_plural != NULL)
    {
      if (TEST_NEWLINE(msgid_plural) != has_newline)
	{
	  error_with_progname = false;
	  error_at_line (0, 0, msgid_pos->file_name, msgid_pos->line_number,
			 _("\
`msgid' and `msgid_plural' entries do not both begin with '\\n'"));
	  error_with_progname = true;
	  exit_status = EXIT_FAILURE;
	}
      for (p = msgstr, i = 0; p < msgstr + msgstr_len; p += strlen (p) + 1, i++)
	if (TEST_NEWLINE(p) != has_newline)
	  {
	    error_with_progname = false;
	    error_at_line (0, 0, msgid_pos->file_name, msgid_pos->line_number,
			   _("\
`msgid' and `msgstr[%u]' entries do not both begin with '\\n'"), i);
	    error_with_progname = true;
	    exit_status = EXIT_FAILURE;
	  }
    }
  else
    {
      if (TEST_NEWLINE(msgstr) != has_newline)
	{
	  error_with_progname = false;
	  error_at_line (0, 0, msgid_pos->file_name, msgid_pos->line_number,
			 _("\
`msgid' and `msgstr' entries do not both begin with '\\n'"));
	  error_with_progname = true;
	  exit_status = EXIT_FAILURE;
	}
    }
#undef TEST_NEWLINE

  /* Test 2: check whether all or none of the strings end with a '\n'.  */
  has_newline = (msgid[strlen (msgid) - 1] == '\n');
#define TEST_NEWLINE(p) (p[0] != '\0' && p[strlen (p) - 1] == '\n')
  if (msgid_plural != NULL)
    {
      if (TEST_NEWLINE(msgid_plural) != has_newline)
	{
	  error_with_progname = false;
	  error_at_line (0, 0, msgid_pos->file_name, msgid_pos->line_number,
			 _("\
`msgid' and `msgid_plural' entries do not both end with '\\n'"));
	  error_with_progname = true;
	  exit_status = EXIT_FAILURE;
	}
      for (p = msgstr, i = 0; p < msgstr + msgstr_len; p += strlen (p) + 1, i++)
	if (TEST_NEWLINE(p) != has_newline)
	  {
	    error_with_progname = false;
	    error_at_line (0, 0, msgid_pos->file_name, msgid_pos->line_number,
			   _("\
`msgid' and `msgstr[%u]' entries do not both end with '\\n'"), i);
	    error_with_progname = true;
	    exit_status = EXIT_FAILURE;
	  }
    }
  else
    {
      if (TEST_NEWLINE(msgstr) != has_newline)
	{
	  error_with_progname = false;
	  error_at_line (0, 0, msgid_pos->file_name, msgid_pos->line_number,
			 _("\
`msgid' and `msgstr' entries do not both end with '\\n'"));
	  error_with_progname = true;
	  exit_status = EXIT_FAILURE;
	}
    }
#undef TEST_NEWLINE

  if (is_format != 0 && msgid_plural == NULL)
    {
      /* Test 3: check whether both formats strings contain the same
	 number of format specifications.  */
      nidfmts = parse_printf_format (msgid, 0, NULL);
      nstrfmts = parse_printf_format (msgstr, 0, NULL);
      if (nidfmts != nstrfmts)
	{
	  error_with_progname = false;
	  error_at_line (0, 0, msgid_pos->file_name, msgid_pos->line_number,
			 _("\
number of format specifications in `msgid' and `msgstr' does not match"));
	  error_with_progname = true;
	  exit_status = EXIT_FAILURE;
	}
      else
	{
	  int *id_args = (int *) alloca (nidfmts * sizeof (int));
	  int *str_args = (int *) alloca (nstrfmts * sizeof (int));
	  size_t cnt;

	  (void) parse_printf_format (msgid, nidfmts, id_args);
	  (void) parse_printf_format (msgstr, nstrfmts, str_args);

	  for (cnt = 0; cnt < nidfmts; ++cnt)
	    if (id_args[cnt] != str_args[cnt])
	      {
		error_with_progname = false;
		error_at_line (0, 0, msgid_pos->file_name,
			       msgid_pos->line_number, _("\
format specifications for argument %lu are not the same"),
			       (unsigned long) (cnt + 1));
		error_with_progname = true;
		exit_status = EXIT_FAILURE;
	      }
	}
    }
}


/* So that the one parser can be used for multiple programs, and also
   use good data hiding and encapsulation practices, an object
   oriented approach has been taken.  An object instance is allocated,
   and all actions resulting from the parse will be through
   invocations of method functions of that object.  */

static po_method_ty format_methods =
{
  sizeof (msgfmt_class_ty),
  format_constructor, /* constructor */
  NULL, /* destructor */
  format_directive_domain,
  format_directive_message,
  NULL, /* parse_brief */
  format_debrief, /* parse_debrief */
  NULL, /* comment */
  NULL, /* comment_dot */
  NULL, /* comment_filepos */
  format_comment_special /* comment */
};


/* Read .po file FILENAME and store translation pairs.  */
static void
grammar (filename)
     char *filename;
{
  po_ty *pop;

  pop = po_alloc (&format_methods);
  po_scan (pop, filename);
  po_free (pop);
}


static const char *
add_mo_suffix (fname)
     const char *fname;
{
  size_t len;
  char *result;

  len = strlen (fname);
  if (len > 3 && memcmp (fname + len - 3, ".mo", 3) == 0)
    return fname;
  if (len > 4 && memcmp (fname + len - 4, ".gmo", 4) == 0)
    return fname;
  result = (char *) xmalloc (len + 4);
  stpcpy (stpcpy (result, fname), ".mo");
  return result;
}
