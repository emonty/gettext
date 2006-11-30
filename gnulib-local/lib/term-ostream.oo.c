/* Output stream for attributed text, producing ANSI escape sequences.
   Copyright (C) 2006 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2006.

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
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

#include <config.h>

/* Specification.  */
#include "term-ostream.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "exit.h"
#include "fatal-signal.h"
#include "full-write.h"
#include "sigprocmask.h"
#include "xalloc.h"
#include "xsize.h"
#include "gettext.h"

#define _(str) gettext (str)

#define SIZEOF(a) (sizeof(a) / sizeof(a[0]))


/* Including <curses.h> or <term.h> is dangerous, because it also declares
   a lot of junk, such as variables PC, UP, and other.  */

#ifdef __cplusplus
extern "C" {
#endif

/* Gets the capability information for terminal type TYPE.
   Returns 1 if successful, 0 if TYPE is unknown, -1 on other error.  */
extern int tgetent (char *bp, const char *type);

/* Retrieves the value of a numerical capability.
   Returns -1 if it is not available.  */
extern int tgetnum (const char *id);

/* Retrieves the value of a boolean capability.
   Returns 1 if it available, 0 otherwise.  */
extern int tgetflag (const char *id);

/* Retrieves the value of a string capability.
   Returns NULL if it is not available.
   Also, if AREA != NULL, stores it at *AREA and advances *AREA.  */
extern const char * tgetstr (const char *id, char **area);

/* Instantiates a string capability with format strings.
   The return value is statically allocated and must not be freed.  */
extern char * tparm (const char *str, ...);

/* Retrieves a string that causes cursor positioning to (column, row).
   This function is necessary because the string returned by tgetstr ("cm")
   is in a special format.  */
extern const char * tgoto (const char *cm, int column, int row);

/* Retrieves the value of a string capability.
   OUTCHARFUN is called in turn for each 'char' of the result.
   This function is necessary because string capabilities can contain
   padding commands.  */
extern void tputs (const char *cp, int affcnt, int (*outcharfun) (int));

/* The ncurses functions for color handling (see ncurses/base/lib_color.c)
   are overkill: Most terminal emulators support only a fixed, small number
   of colors.  */

#ifdef __cplusplus
}
#endif


/* ANSI C and ISO C99 6.7.2.1.(4) forbid use of bit fields for types other
   than 'int' or 'unsigned int'.
   On the other hand, C++ forbids conversion between enum types and integer
   types without an explicit cast.  */
#ifdef __cplusplus
# define BITFIELD_TYPE(orig_type,integer_type) orig_type
#else
# define BITFIELD_TYPE(orig_type,integer_type) integer_type
#endif

/* Attributes that can be set on a character.  */
typedef struct
{
  BITFIELD_TYPE(term_color_t,     signed int)   color     : 4;
  BITFIELD_TYPE(term_color_t,     signed int)   bgcolor   : 4;
  BITFIELD_TYPE(term_weight_t,    unsigned int) weight    : 1;
  BITFIELD_TYPE(term_posture_t,   unsigned int) posture   : 1;
  BITFIELD_TYPE(term_underline_t, unsigned int) underline : 1;
} attributes_t;

struct term_ostream : struct ostream
{
fields:
  /* The file descriptor used for output.  Note that ncurses termcap emulation
     uses the baud rate information from file descriptor 1 (stdout) if it is
     a tty, or from file descriptor 2 (stderr) otherwise.  */
  int fd;
  char *filename;
  /* Values from the terminal type's terminfo/termcap description.
     See terminfo(5) for details.  */
				/* terminfo  termcap */
  int max_colors;		/* colors    Co */
  int no_color_video;		/* ncv       NC */
  char *set_a_foreground;	/* setaf     AF */
  char *set_foreground;		/* setf      Sf */
  char *set_a_background;	/* setab     AB */
  char *set_background;		/* setb      Sb */
  char *orig_pair;		/* op        op */
  char *enter_bold_mode;	/* bold      md */
  char *enter_italics_mode;	/* sitm      ZH */
  char *exit_italics_mode;	/* ritm      ZR */
  char *enter_underline_mode;	/* smul      us */
  char *exit_underline_mode;	/* rmul      ue */
  char *exit_attribute_mode;	/* sgr0      me */
  /* Inferred values.  */
  bool supports_foreground;
  bool supports_background;
  bool supports_weight;
  bool supports_posture;
  bool supports_underline;
  /* Variable state.  */
  char *buffer;			/* Buffer for the current line.  */
  attributes_t *attrbuffer;	/* Buffer for the simplified attributes; same
				   length as buffer.  */
  size_t buflen;		/* Number of bytes stored so far.  */
  size_t allocated;		/* Allocated size of the buffer.  */
  attributes_t curr_attr;	/* Current attributes.  */
  attributes_t simp_attr;	/* Simplified current attributes.  */
};

/* Simplify attributes, according to the terminal's capabilities.  */
static attributes_t
simplify_attributes (term_ostream_t stream, attributes_t attr)
{
  if ((attr.color != COLOR_DEFAULT || attr.bgcolor != COLOR_DEFAULT)
      && stream->no_color_video > 0)
    {
      /* When colors and attributes can not be represented simultaneously,
	 we give preference to the color.  */
      if (stream->no_color_video & 2)
	/* Colors conflict with underlining.  */
	attr.underline = UNDERLINE_OFF;
      if (stream->no_color_video & 32)
	/* Colors conflict with bold weight.  */
	attr.weight = WEIGHT_NORMAL;
    }
  if (!stream->supports_foreground)
    attr.color = COLOR_DEFAULT;
  if (!stream->supports_background)
    attr.bgcolor = COLOR_DEFAULT;
  if (!stream->supports_weight)
    attr.weight = WEIGHT_DEFAULT;
  if (!stream->supports_posture)
    attr.posture = POSTURE_DEFAULT;
  if (!stream->supports_underline)
    attr.underline = UNDERLINE_DEFAULT;
  return attr;
}

/* While a line is being output, we need to be careful to restore the
   terminal's settings in case of a fatal signal or an exit() call.  */

/* File descriptor to which out_char shall output escape sequences.  */
static int out_fd = -1;

/* Filename of out_fd.  */
static const char *out_filename;

/* Output a single char to out_fd.  Ignore errors.  */
static int
out_char_unchecked (int c)
{
  char bytes[1];

  bytes[0] = (char)c;
  full_write (out_fd, bytes, 1);
  return 0;
}

/* State that informs the exit handler what to do.  */
static const char *restore_colors;
static const char *restore_weight;
static const char *restore_posture;
static const char *restore_underline;

/* The exit handler.  */
static void
restore (void)
{
  /* Only do something while some output was interrupted.  */
  if (out_fd >= 0)
    {
      if (restore_colors != NULL)
	tputs (restore_colors, 1, out_char_unchecked);
      if (restore_weight != NULL)
	tputs (restore_weight, 1, out_char_unchecked);
      if (restore_posture != NULL)
	tputs (restore_posture, 1, out_char_unchecked);
      if (restore_underline != NULL)
	tputs (restore_underline, 1, out_char_unchecked);
    }
}

/* The list of signals whose default behaviour is to stop the program.  */
static int stopping_signals[] =
  {
#ifdef SIGTSTP
    SIGTSTP,
#endif
#ifdef SIGTTIN
    SIGTTIN,
#endif
#ifdef SIGTTOU
    SIGTTOU,
#endif
    0
  };

#define num_stopping_signals (SIZEOF (stopping_signals) - 1)

static sigset_t stopping_signal_set;

static void
init_stopping_signal_set ()
{
  static bool stopping_signal_set_initialized = false;
  if (!stopping_signal_set_initialized)
    {
      size_t i;

      sigemptyset (&stopping_signal_set);
      for (i = 0; i < num_stopping_signals; i++)
	sigaddset (&stopping_signal_set, stopping_signals[i]);

      stopping_signal_set_initialized = true;
    }
}

/* Temporarily delay the stopping signals.  */
void
block_stopping_signals ()
{
  init_stopping_signal_set ();
  sigprocmask (SIG_BLOCK, &stopping_signal_set, NULL);
}

/* Stop delaying the stopping signals.  */
void
unblock_stopping_signals ()
{
  init_stopping_signal_set ();
  sigprocmask (SIG_UNBLOCK, &stopping_signal_set, NULL);
}

/* Compare two sets of attributes for equality.  */
static inline bool
equal_attributes (attributes_t attr1, attributes_t attr2)
{
  return (attr1.color == attr2.color
	  && attr1.bgcolor == attr2.bgcolor
	  && attr1.weight == attr2.weight
	  && attr1.posture == attr2.posture
	  && attr1.underline == attr2.underline);
}

/* Convert a color in RGB encoding to BGR encoding.  */
static inline int
color_bgr (term_color_t color)
{
  return ((color & 4) >> 2) | (color & 2) | ((color & 1) << 2);
}

/* Output a single char to out_fd.  */
static int
out_char (int c)
{
  char bytes[1];

  bytes[0] = (char)c;
  /* We have to write directly to the file descriptor, not to a buffer with
     the same destination, because of the padding and sleeping that tputs()
     does.  */
  if (full_write (out_fd, bytes, 1) < 1)
    error (EXIT_FAILURE, errno, _("error writing to %s"), out_filename);
  return 0;
}

/* Output escape sequences to switch from OLD_ATTR to NEW_ATTR.  */
static void
out_attr_change (term_ostream_t stream,
		 attributes_t old_attr, attributes_t new_attr)
{
  bool cleared_attributes;

  /* We don't know the default colors of the terminal.  The only way to switch
     back to a default color is to use stream->orig_pair.  */
  if ((new_attr.color == COLOR_DEFAULT && old_attr.color != COLOR_DEFAULT)
      || (new_attr.bgcolor == COLOR_DEFAULT && old_attr.bgcolor != COLOR_DEFAULT))
    {
      assert (stream->supports_foreground || stream->supports_background);
      tputs (stream->orig_pair, 1, out_char);
      old_attr.color = COLOR_DEFAULT;
      old_attr.bgcolor = COLOR_DEFAULT;
    }
  if (new_attr.color != old_attr.color)
    {
      assert (stream->supports_foreground);
      assert (new_attr.color != COLOR_DEFAULT);
      if (stream->set_a_foreground != NULL)
	tputs (tparm (stream->set_a_foreground, new_attr.color), 1, out_char);
      else
	tputs (tparm (stream->set_foreground, color_bgr (new_attr.color)),
	       1, out_char);
    }
  if (new_attr.bgcolor != old_attr.bgcolor)
    {
      assert (stream->supports_background);
      assert (new_attr.bgcolor != COLOR_DEFAULT);
      if (stream->set_a_background != NULL)
	tputs (tparm (stream->set_a_background, new_attr.bgcolor), 1, out_char);
      else
	tputs (tparm (stream->set_background, color_bgr (new_attr.bgcolor)),
	       1, out_char);
    }

  cleared_attributes = false;
  if (new_attr.weight != old_attr.weight)
    {
      assert (stream->supports_weight);
      if (new_attr.weight == WEIGHT_BOLD)
	tputs (stream->enter_bold_mode, 1, out_char);
      else
	{
	  /* The simplest way to clear bold mode is exit_attribute_mode.
	     With xterm, you can also do it with "Esc [ 0 m", but this escape
	     sequence is not contained in the terminfo description.  */
	  tputs (stream->exit_attribute_mode, 1, out_char);
	  /* We don't know whether exit_attribute_mode clears also the
	     italics or underline mode.  */
	  cleared_attributes = true;
	}
    }
  if (new_attr.posture != old_attr.posture
      || (cleared_attributes && new_attr.posture != POSTURE_DEFAULT))
    {
      assert (stream->supports_posture);
      if (new_attr.posture == POSTURE_ITALIC)
	tputs (stream->enter_italics_mode, 1, out_char);
      else
	{
	  if (stream->exit_italics_mode != NULL)
	    tputs (stream->exit_italics_mode, 1, out_char);
	  else if (!cleared_attributes)
	    tputs (stream->exit_attribute_mode, 1, out_char);
	}
    }
  if (new_attr.underline != old_attr.underline
      || (cleared_attributes && new_attr.underline != UNDERLINE_DEFAULT))
    {
      assert (stream->supports_underline);
      if (new_attr.underline == UNDERLINE_ON)
	tputs (stream->enter_underline_mode, 1, out_char);
      else
	{
	  if (stream->exit_underline_mode != NULL)
	    tputs (stream->exit_underline_mode, 1, out_char);
	  else if (!cleared_attributes)
	    tputs (stream->exit_attribute_mode, 1, out_char);
	}
    }
}

/* Output the buffered line atomically.
   The terminal is assumed to have the default state (regarding colors and
   attributes) before this call.  It is left in default state after this
   call (regardless of stream->curr_attr).  */
static void
output_buffer (term_ostream_t stream)
{
  attributes_t default_attr;
  attributes_t attr;
  const char *cp;
  const attributes_t *ap;
  size_t len;
  size_t n;

  default_attr.color = COLOR_DEFAULT;
  default_attr.bgcolor = COLOR_DEFAULT;
  default_attr.weight = WEIGHT_DEFAULT;
  default_attr.posture = POSTURE_DEFAULT;
  default_attr.underline = UNDERLINE_DEFAULT;

  attr = default_attr;

  cp = stream->buffer;
  ap = stream->attrbuffer;
  len = stream->buflen;

  /* See how much we can output without blocking signals.  */
  for (n = 0; n < len && equal_attributes (ap[n], attr); n++)
    ;
  if (n > 0)
    {
      if (full_write (stream->fd, cp, n) < n)
	error (EXIT_FAILURE, errno, _("error writing to %s"), stream->filename);
      cp += n;
      ap += n;
      len -= n;
    }
  if (len > 0)
    {
      /* Block fatal signals, so that a SIGINT or similar doesn't interrupt
	 us without the possibility of restoring the terminal's state.  */
      block_fatal_signals ();
      /* Likewise for SIGTSTP etc.  */
      block_stopping_signals ();

      /* Enable the exit handler for restoring the terminal's state.  */
      restore_colors =
	(stream->supports_foreground || stream->supports_background
	 ? stream->orig_pair
	 : NULL);
      restore_weight =
	(stream->supports_weight ? stream->exit_attribute_mode : NULL);
      restore_posture =
	(stream->supports_posture
	 ? (stream->exit_italics_mode != NULL
	    ? stream->exit_italics_mode
	    : stream->exit_attribute_mode)
	 : NULL);
      restore_underline =
	(stream->supports_underline
	 ? (stream->exit_underline_mode != NULL
	    ? stream->exit_underline_mode
	    : stream->exit_attribute_mode)
	 : NULL);
      out_fd = stream->fd;
      out_filename = stream->filename;

      while (len > 0)
	{
	  /* Activate the attributes in *ap.  */
	  out_attr_change (stream, attr, *ap);
	  attr = *ap;
	  /* See how many characters we can output without further attribute
	     changes.  */
	  for (n = 1; n < len && equal_attributes (ap[n], attr); n++)
	    ;
	  if (full_write (stream->fd, cp, n) < n)
	    error (EXIT_FAILURE, errno, _("error writing to %s"),
		   stream->filename);
	  cp += n;
	  ap += n;
	  len -= n;
	}

      /* Switch back to the default attributes.  */
      out_attr_change (stream, attr, default_attr);

      /* Disable the exit handler.  */
      out_fd = -1;
      out_filename = NULL;

      /* Unblock fatal and stopping signals.  */
      unblock_stopping_signals ();
      unblock_fatal_signals ();
    }
  stream->buflen = 0;
}

/* Implementation of ostream_t methods.  */

static void
term_ostream::write_mem (term_ostream_t stream, const void *data, size_t len)
{
  const char *cp = (const char *) data;
  while (len > 0)
    {
      /* Look for the next newline.  */
      const char *newline = (const char *) memchr (cp, '\n', len);
      size_t n = (newline != NULL ? newline - cp : len);

      /* Copy n bytes into the buffer.  */
      if (n > stream->allocated - stream->buflen)
	{
	  size_t new_allocated =
	    xmax (xsum (stream->buflen, n),
		  xsum (stream->allocated, stream->allocated));
	  if (size_overflow_p (new_allocated))
	    error (EXIT_FAILURE, 0,
		   _("%s: too much output, buffer size overflow"),
		   "term_ostream");
	  stream->buffer = (char *) xrealloc (stream->buffer, new_allocated);
	  stream->attrbuffer =
	    (attributes_t *)
	    xrealloc (stream->attrbuffer,
		      new_allocated * sizeof (attributes_t));
	  stream->allocated = new_allocated;
	}
      memcpy (stream->buffer + stream->buflen, cp, n);
      {
	attributes_t attr = stream->simp_attr;
	attributes_t *ap = stream->attrbuffer + stream->buflen;
	attributes_t *ap_end = ap + n;
	for (; ap < ap_end; ap++)
	  *ap = attr;
      }
      stream->buflen += n;

      if (newline != NULL)
	{
	  output_buffer (stream);
	  if (full_write (stream->fd, "\n", 1) < 1)
	    error (EXIT_FAILURE, errno, _("error writing to %s"),
		   stream->filename);
	  cp += n + 1; /* cp = newline + 1; */
	  len -= n + 1;
	}
      else
	break;
    }
}

static void
term_ostream::flush (term_ostream_t stream)
{
  output_buffer (stream);
}

static void
term_ostream::free (term_ostream_t stream)
{
  term_ostream_flush (stream);
  free (stream->filename);
  if (stream->set_a_foreground != NULL)
    free (stream->set_a_foreground);
  if (stream->set_foreground != NULL)
    free (stream->set_foreground);
  if (stream->set_a_background != NULL)
    free (stream->set_a_background);
  if (stream->set_background != NULL)
    free (stream->set_background);
  if (stream->orig_pair != NULL)
    free (stream->orig_pair);
  if (stream->enter_bold_mode != NULL)
    free (stream->enter_bold_mode);
  if (stream->enter_italics_mode != NULL)
    free (stream->enter_italics_mode);
  if (stream->exit_italics_mode != NULL)
    free (stream->exit_italics_mode);
  if (stream->enter_underline_mode != NULL)
    free (stream->enter_underline_mode);
  if (stream->exit_underline_mode != NULL)
    free (stream->exit_underline_mode);
  if (stream->exit_attribute_mode != NULL)
    free (stream->exit_attribute_mode);
  free (stream->buffer);
  free (stream);
}

/* Implementation of term_ostream_t methods.  */

term_color_t
term_ostream::get_color (term_ostream_t stream)
{
  return stream->curr_attr.color;
}

void
term_ostream::set_color (term_ostream_t stream, term_color_t color)
{
  stream->curr_attr.color = color;
  stream->simp_attr = simplify_attributes (stream, stream->curr_attr);
}

term_color_t
term_ostream::get_bgcolor (term_ostream_t stream)
{
  return stream->curr_attr.bgcolor;
}

void
term_ostream::set_bgcolor (term_ostream_t stream, term_color_t color)
{
  stream->curr_attr.bgcolor = color;
  stream->simp_attr = simplify_attributes (stream, stream->curr_attr);
}

term_weight_t
term_ostream::get_weight (term_ostream_t stream)
{
  return stream->curr_attr.weight;
}

void
term_ostream::set_weight (term_ostream_t stream, term_weight_t weight)
{
  stream->curr_attr.weight = weight;
  stream->simp_attr = simplify_attributes (stream, stream->curr_attr);
}

term_posture_t
term_ostream::get_posture (term_ostream_t stream)
{
  return stream->curr_attr.posture;
}

void
term_ostream::set_posture (term_ostream_t stream, term_posture_t posture)
{
  stream->curr_attr.posture = posture;
  stream->simp_attr = simplify_attributes (stream, stream->curr_attr);
}

term_underline_t
term_ostream::get_underline (term_ostream_t stream)
{
  return stream->curr_attr.underline;
}

void
term_ostream::set_underline (term_ostream_t stream, term_underline_t underline)
{
  stream->curr_attr.underline = underline;
  stream->simp_attr = simplify_attributes (stream, stream->curr_attr);
}

/* Constructor.  */

static inline char *
xstrdup0 (const char *str)
{
  return (str != NULL ? xstrdup (str) : NULL);
}

term_ostream_t
term_ostream_create (int fd, const char *filename)
{
  term_ostream_t stream = XMALLOC (struct term_ostream_representation);
  const char *term;

  stream->base.vtable = &term_ostream_vtable;
  stream->fd = fd;
  stream->filename = xstrdup (filename);

  /* Defaults.  */
  stream->max_colors = -1;
  stream->no_color_video = -1;
  stream->set_a_foreground = NULL;
  stream->set_foreground = NULL;
  stream->set_a_background = NULL;
  stream->set_background = NULL;
  stream->orig_pair = NULL;
  stream->enter_bold_mode = NULL;
  stream->enter_italics_mode = NULL;
  stream->exit_italics_mode = NULL;
  stream->enter_underline_mode = NULL;
  stream->exit_underline_mode = NULL;
  stream->exit_attribute_mode = NULL;

  /* Retrieve the terminal type.  */
  term = getenv ("TERM");
  if (term != NULL && term[0] != '\0')
    {
      struct { char buf[1024]; char canary[4]; } termcapbuf;
      int retval;
  
      /* Call tgetent, being defensive against buffer overflow.  */
      memcpy (termcapbuf.canary, "CnRy", 4);
      retval = tgetent (termcapbuf.buf, term);
      if (memcmp (termcapbuf.canary, "CnRy", 4) != 0)
	/* Buffer overflow!  */
	abort ();

      /* Retrieve particular values depending on the terminal type.  */
      stream->max_colors = tgetnum ("Co");
      stream->no_color_video = tgetnum ("nc");
      stream->set_a_foreground = xstrdup0 (tgetstr ("AF", NULL));
      stream->set_foreground = xstrdup0 (tgetstr ("Sf", NULL));
      stream->set_a_background = xstrdup0 (tgetstr ("AB", NULL));
      stream->set_background = xstrdup0 (tgetstr ("Sb", NULL));
      stream->orig_pair = xstrdup0 (tgetstr ("op", NULL));
      stream->enter_bold_mode = xstrdup0 (tgetstr ("md", NULL));
      stream->enter_italics_mode = xstrdup0 (tgetstr ("ZH", NULL));
      stream->exit_italics_mode = xstrdup0 (tgetstr ("ZR", NULL));
      stream->enter_underline_mode = xstrdup0 (tgetstr ("us", NULL));
      stream->exit_underline_mode = xstrdup0 (tgetstr ("ue", NULL));
      stream->exit_attribute_mode = xstrdup0 (tgetstr ("me", NULL));
    }

  /* Infer the capabilities.  */
  stream->supports_foreground =
    (stream->max_colors >= 8
     && (stream->set_a_foreground != NULL || stream->set_foreground != NULL)
     && stream->orig_pair != NULL);
  stream->supports_background =
    (stream->max_colors >= 8
     && (stream->set_a_background != NULL || stream->set_background != NULL)
     && stream->orig_pair != NULL);
  stream->supports_weight =
    (stream->enter_bold_mode != NULL && stream->exit_attribute_mode != NULL);
  stream->supports_posture =
    (stream->enter_italics_mode != NULL
     && (stream->exit_italics_mode != NULL
	 || stream->exit_attribute_mode != NULL));
  stream->supports_underline =
    (stream->enter_underline_mode != NULL
     && (stream->exit_underline_mode != NULL
	 || stream->exit_attribute_mode != NULL));

  /* Initialize the buffer.  */
  stream->allocated = 120;
  stream->buffer = XNMALLOC (stream->allocated, char);
  stream->attrbuffer = XNMALLOC (stream->allocated, attributes_t);
  stream->buflen = 0;

  /* Initialize the current attributes.  */
  stream->curr_attr.color = COLOR_DEFAULT;
  stream->curr_attr.bgcolor = COLOR_DEFAULT;
  stream->curr_attr.weight = WEIGHT_DEFAULT;
  stream->curr_attr.posture = POSTURE_DEFAULT;
  stream->curr_attr.underline = UNDERLINE_DEFAULT;
  stream->simp_attr = simplify_attributes (stream, stream->curr_attr);

  /* Register an exit handler.  */
  {
    static bool registered = false;
    if (!registered)
      {
	atexit (restore);
	registered = true;
      }
  }

  return stream;
}
