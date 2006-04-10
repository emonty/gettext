/* Temporary directories and temporary files with automatic cleanup.
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

#ifndef _CLEAN_TEMP_H
#define _CLEAN_TEMP_H

#ifdef __cplusplus
extern "C" {
#endif


/* Temporary directories and temporary files should be automatically removed
   when the program exits either normally or through a fatal signal.  We can't
   rely on the "unlink before close" idiom, because it works only on Unix and
   also - if no signal blocking is used - leaves a time window where a fatal
   signal would not clean up the temporary file.

   This module provides support for temporary directories and temporary files
   inside these temporary directories.  Temporary files without temporary
   directories are not supported here.  */

struct temp_dir
{
  /* The absolute pathname of the directory.  */
  const char * const dir_name;
  /* More fields are present here, but not public.  */
};

/* Create a temporary directory.
   PREFIX is used as a prefix for the name of the temporary directory. It
   should be short and still give an indication about the program.
   Return a fresh 'struct temp_dir' on success.  Upon error, an error message
   is shown and NULL is returned.  */
extern struct temp_dir * create_temp_dir (const char *prefix);

/* Register the given ABSOLUTE_FILE_NAME as being a file inside DIR, that
   needs to be removed before DIR can be removed.
   Should be called before the file ABSOLUTE_FILE_NAME is created.  */
extern void enqueue_temp_file (struct temp_dir *dir,
			       const char *absolute_file_name);

/* Unregister the given ABSOLUTE_FILE_NAME as being a file inside DIR, that
   needs to be removed before DIR can be removed.
   Should be called when the file ABSOLUTE_FILE_NAME could not be created.  */
extern void dequeue_temp_file (struct temp_dir *dir,
			       const char *absolute_file_name);

/* Register the given ABSOLUTE_DIR_NAME as being a subdirectory inside DIR,
   that needs to be removed before DIR can be removed.
   Should be called before the subdirectory ABSOLUTE_DIR_NAME is created.  */
extern void enqueue_temp_subdir (struct temp_dir *dir,
				 const char *absolute_dir_name);

/* Unregister the given ABSOLUTE_DIR_NAME as being a subdirectory inside DIR,
   that needs to be removed before DIR can be removed.
   Should be called when the subdirectory ABSOLUTE_DIR_NAME could not be
   created.  */
extern void dequeue_temp_subdir (struct temp_dir *dir,
				 const char *absolute_dir_name);

/* Remove the given ABSOLUTE_FILE_NAME and unregister it.  */
extern void cleanup_temp_file (struct temp_dir *dir,
			       const char *absolute_file_name);

/* Remove the given ABSOLUTE_DIR_NAME and unregister it.  */
extern void cleanup_temp_subdir (struct temp_dir *dir,
				 const char *absolute_dir_name);

/* Remove all registered files and subdirectories inside DIR.  */
extern void cleanup_temp_dir_contents (struct temp_dir *dir);

/* Remove all registered files and subdirectories inside DIR and DIR itself.
   DIR cannot be used any more after this call.  */
extern void cleanup_temp_dir (struct temp_dir *dir);


#ifdef __cplusplus
}
#endif

#endif /* _CLEAN_TEMP_H */
