/* Creation of subprocesses, communicating via pipes.
   Copyright (C) 2001-2003 Free Software Foundation, Inc.
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
#include "pipe.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if defined _MSC_VER || defined __MINGW32__

/* Native Woe32 API.  */
# include <process.h>
# include "w32spawn.h"

#else

/* Unix API.  */
# ifdef HAVE_POSIX_SPAWN
#  include <spawn.h>
# else
#  ifdef HAVE_VFORK_H
#   include <vfork.h>
#  endif
# endif

#endif

#include "error.h"
#include "exit.h"
#include "gettext.h"

#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

#define _(str) gettext (str)


#ifdef EINTR

/* EINTR handling for close(), open().
   These functions can return -1/EINTR even though we don't have any
   signal handlers set up, namely when we get interrupted via SIGSTOP.  */

static inline int
nonintr_close (int fd)
{
  int retval;

  do
    retval = close (fd);
  while (retval < 0 && errno == EINTR);

  return retval;
}
#define close nonintr_close

static inline int
nonintr_open (const char *pathname, int oflag, mode_t mode)
{
  int retval;

  do
    retval = open (pathname, oflag, mode);
  while (retval < 0 && errno == EINTR);

  return retval;
}
#undef open /* avoid warning on VMS */
#define open nonintr_open

#endif


/* Open a pipe for output to a child process.
 * The child's stdout goes to a file.
 *
 *           write       system                read
 *    parent  ->   fd[0]   ->   STDIN_FILENO    ->   child
 *
 */
pid_t
create_pipe_out (const char *progname,
		 const char *prog_path, char **prog_argv,
		 const char *prog_stdout, bool null_stderr,
		 bool exit_on_error,
		 int fd[1])
{
#if defined _MSC_VER || defined __MINGW32__

  /* Native Woe32 API.
     This uses _pipe(), dup2(), and spawnv().  It could also be implemented
     using the low-level functions CreatePipe(), DuplicateHandle(),
     CreateProcess() and _open_osfhandle(); see the GNU make and GNU clisp
     source code.  */
  int ofd[2];
  int orig_stdin;
  int orig_stdout;
  int orig_stderr;
  int child;
  int nulloutfd;
  int stdoutfd;

  prog_argv = prepare_spawn (prog_argv);

  if (_pipe (ofd, 4096, O_BINARY | O_NOINHERIT) < 0)
    error (EXIT_FAILURE, errno, _("cannot create pipe"));
/* Data flow diagram:
 *
 *           write        system         read
 *    parent  ->   ofd[1]   ->   ofd[0]   ->   child
 */

  /* Save standard file handles of parent process.  */
  orig_stdin = dup_noinherit (STDIN_FILENO);
  if (prog_stdout != NULL)
    orig_stdout = dup_noinherit (STDOUT_FILENO);
  if (null_stderr)
    orig_stderr = dup_noinherit (STDERR_FILENO);
  child = -1;

  /* Create standard file handles of child process.  */
  nulloutfd = -1;
  stdoutfd = -1;
  if (dup2 (ofd[0], STDIN_FILENO) >= 0
      && (!null_stderr
	  || ((nulloutfd = open ("NUL", O_RDWR, 0)) >= 0
	      && (nulloutfd == STDERR_FILENO
		  || (dup2 (nulloutfd, STDERR_FILENO) >= 0
		      && close (nulloutfd) >= 0))))
      && (prog_stdout == NULL
	  || ((stdoutfd = open (prog_stdout, O_WRONLY, 0)) >= 0
	      && (stdoutfd == STDOUT_FILENO
		  || (dup2 (stdoutfd, STDOUT_FILENO) >= 0
		      && close (stdoutfd) >= 0)))))
    /* The child process doesn't inherit ofd[0] and ofd[1], but it
       inherits all open()ed or dup2()ed file handles (which is what
       we want in the case of STD*_FILENO) and also orig_stdin,
       orig_stdout, orig_stderr (which is not explicitly wanted but
       harmless).  */
    child = spawnvp (P_NOWAIT, prog_path, prog_argv);
  if (stdoutfd >= 0)
    close (stdoutfd);
  if (nulloutfd >= 0)
    close (nulloutfd);

  /* Restore standard file handles of parent process.  */
  if (null_stderr)
    dup2 (orig_stderr, STDERR_FILENO), close (orig_stderr);
  if (prog_stdout != NULL)
    dup2 (orig_stdout, STDOUT_FILENO), close (orig_stdout);
  dup2 (orig_stdin, STDIN_FILENO), close (orig_stdin);

  close (ofd[0]);
  if (child == -1)
    {
      if (exit_on_error)
	error (EXIT_FAILURE, errno, _("%s subprocess failed"), progname);
      else
	{
	  close (ofd[1]);
	  return -1;
	}
    }

  fd[0] = ofd[1];
  return child;

#else

  /* Unix API.  */
  int ofd[2];
#if HAVE_POSIX_SPAWN
  posix_spawn_file_actions_t actions;
  bool actions_allocated;
  int err;
  pid_t child;
#else
  int child;
#endif

  if (pipe (ofd) < 0)
    error (EXIT_FAILURE, errno, _("cannot create pipe"));
/* Data flow diagram:
 *
 *           write        system         read
 *    parent  ->   ofd[1]   ->   ofd[0]   ->   child
 */

#if HAVE_POSIX_SPAWN
  actions_allocated = false;
  if ((err = posix_spawn_file_actions_init (&actions)) != 0
      || (actions_allocated = true,
	  (err = posix_spawn_file_actions_adddup2 (&actions,
						   ofd[0], STDIN_FILENO)) != 0
	  || (err = posix_spawn_file_actions_addclose (&actions, ofd[0])) != 0
	  || (err = posix_spawn_file_actions_addclose (&actions, ofd[1])) != 0
	  || (null_stderr
	      && (err = posix_spawn_file_actions_addopen (&actions,
							  STDERR_FILENO,
							  "/dev/null", O_RDWR,
							  0))
		 != 0)
	  || (prog_stdout != NULL
	      && (err = posix_spawn_file_actions_addopen (&actions,
							  STDOUT_FILENO,
							  prog_stdout, O_WRONLY,
							  0))
		 != 0)
	  || (err = posix_spawnp (&child, prog_path, &actions, NULL, prog_argv,
				  environ)) != 0))
    {
      if (actions_allocated)
	posix_spawn_file_actions_destroy (&actions);
      if (exit_on_error)
	error (EXIT_FAILURE, err, _("%s subprocess failed"), progname);
      else
	{
	  close (ofd[0]);
	  close (ofd[1]);
	  return -1;
	}
    }
  posix_spawn_file_actions_destroy (&actions);
#else
  /* Use vfork() instead of fork() for efficiency.  */
  if ((child = vfork ()) == 0)
    {
      /* Child process code.  */
      int nulloutfd;
      int stdoutfd;

      if (dup2 (ofd[0], STDIN_FILENO) >= 0
	  && close (ofd[0]) >= 0
	  && close (ofd[1]) >= 0
	  && (!null_stderr
	      || ((nulloutfd = open ("/dev/null", O_RDWR, 0)) >= 0
		  && (nulloutfd == STDERR_FILENO
		      || (dup2 (nulloutfd, STDERR_FILENO) >= 0
			  && close (nulloutfd) >= 0))))
	  && (prog_stdout == NULL
	      || ((stdoutfd = open (prog_stdout, O_WRONLY, 0)) >= 0
		  && (stdoutfd == STDOUT_FILENO
		      || (dup2 (stdoutfd, STDOUT_FILENO) >= 0
			  && close (stdoutfd) >= 0)))))
	execvp (prog_path, prog_argv);
      _exit (127);
    }
  if (child == -1)
    {
      if (exit_on_error)
	error (EXIT_FAILURE, errno, _("%s subprocess failed"), progname);
      else
	{
	  close (ofd[0]);
	  close (ofd[1]);
	  return -1;
	}
    }
#endif
  close (ofd[0]);

  fd[0] = ofd[1];
  return child;
#endif
}
