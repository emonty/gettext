/* Creation of subprocesses, communicating via pipes.
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
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_POSIX_SPAWN
# include <spawn.h>
#else
# ifdef HAVE_VFORK_H
#  include <vfork.h>
# endif
#endif

#include "error.h"
#include "libgettext.h"

#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif

#define _(str) gettext (str)


#ifdef EINTR

/* EINTR handling for close(), open().
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

static inline int
nonintr_open (pathname, oflag, mode)
     const char *pathname;
     int oflag;
     mode_t mode;
{
  int retval;

  do
    retval = open (pathname, oflag, mode);
  while (retval < 0 && errno == EINTR);

  return retval;
}
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
create_pipe_out (progname, prog_path, prog_argv, prog_stdout, fd)
     const char *progname;
     const char *prog_path;
     char **prog_argv;
     const char *prog_stdout;
     int fd[1];
{
  int ofd[2];
#if HAVE_POSIX_SPAWN
  posix_spawn_file_actions_t actions;
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
  if ((err = posix_spawn_file_actions_init (&actions)) != 0
      || (err = posix_spawn_file_actions_adddup2 (&actions,
						  ofd[0], STDIN_FILENO)) != 0
      || (err = posix_spawn_file_actions_addclose (&actions, ofd[0])) != 0
      || (err = posix_spawn_file_actions_addclose (&actions, ofd[1])) != 0
      || (err = posix_spawn_file_actions_addopen (&actions, STDOUT_FILENO,
						  prog_stdout, O_WRONLY,
						  0)) != 0
      || (err = posix_spawnp (&child, prog_path, &actions, NULL, prog_argv,
			      environ)) != 0)
    error (EXIT_FAILURE, err, _("%s subprocess failed"), progname);
  posix_spawn_file_actions_destroy (&actions);
#else
  /* Use vfork() instead of fork() for efficiency.  */
  if ((child = vfork ()) == 0)
    {
      /* Child process code.  */
      int stdoutfd;

      if (dup2 (ofd[0], STDIN_FILENO) >= 0
	  && close (ofd[0]) >= 0
	  && close (ofd[1]) >= 0
	  && (stdoutfd = open (prog_stdout, O_WRONLY, 0)) >= 0
	  && (stdoutfd == STDOUT_FILENO
	      || (dup2 (stdoutfd, STDOUT_FILENO) >= 0
		  && close (stdoutfd) >= 0)))
	execvp (prog_path, prog_argv);
      _exit (-1);
    }
  if (child == -1)
    error (EXIT_FAILURE, errno, _("%s subprocess failed"), progname);
#endif
  close (ofd[0]);

  fd[0] = ofd[1];
  return child;
}
