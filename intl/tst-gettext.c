/* Test of the gettext functions.
   Copyright (C) 2000 Free Software Foundation, Inc.
   Contributed by Ulrich Drepper <drepper@cygnus.com>, 2000.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


const struct
{
  const char *msgid;
  const char *msgstr;
} msgs[] =
{
#define INPUT(Str) { Str,
#define OUTPUT(Str) Str },
#include TESTSTRS_H
};

const char *catname[] =
{
  [LC_MESSAGES] = "LC_MESSAGES",
  [LC_TIME] = "LC_TIME",
  [LC_NUMERIC] = "LC_NUMERIC"
};


static int positive_gettext_test (void);
static int negative_gettext_test (void);
static int positive_dgettext_test (const char *domain);
static int positive_dcgettext_test (const char *domain, int category);
static int negative_dcgettext_test (const char *domain, int category);


int
main (int argc, char *argv[])
{
  int result = 0;

  /* This is the place where the .mo files are placed.  */
  if (argc > 1)
    {
      bindtextdomain ("existing-domain", argv[1]);
      bindtextdomain ("existing-time-domain", argv[1]);
      bindtextdomain ("non-existing-domain", argv[1]);
    }

  /* The locale the catalog is created for is "existing-category".  Now
     set the various variables in question to this value and run the
     test.  */
  setenv ("LANGUAGE", "existing-locale", 1);
  setenv ("LC_ALL", "non-existing-locale", 1);
  setenv ("LC_MESSAGES", "non-existing-locale", 1);
  setenv ("LANG", "non-existing-locale", 1);
  /* This is the name of the existing domain with a catalog for the
     LC_MESSAGES category.  */
  textdomain ("existing-domain");
  puts ("test `gettext' with LANGUAGE set");
  if (positive_gettext_test () != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  /* This is the name of a non-existing domain with a catalog for the
     LC_MESSAGES category.  We leave this value set for the `dgettext'
     and `dcgettext' tests.  */
  textdomain ("non-existing-domain");
  puts ("test `gettext' with LANGUAGE set");
  if (negative_gettext_test () != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  puts ("test `dgettext' with LANGUAGE set");
  if (positive_dgettext_test ("existing-domain") != 0)
    {
      puts ("FAILED");
      result = 1;
    }

  /* Now the same tests with LC_ALL deciding.  */
  unsetenv ("LANGUAGE");
  setenv ("LC_ALL", "existing-locale", 1);
  puts ("test `gettext' with LC_ALL set");
  /* This is the name of the existing domain with a catalog for the
     LC_MESSAGES category.  */
  textdomain ("existing-domain");
  if (positive_gettext_test () != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  /* This is the name of a non-existing domain with a catalog for the
     LC_MESSAGES category.  We leave this value set for the `dgettext'
     and `dcgettext' tests.  */
  textdomain ("non-existing-domain");
  puts ("test `gettext' with LANGUAGE set");
  if (negative_gettext_test () != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  puts ("test `dgettext' with LANGUAGE set");
  if (positive_dgettext_test ("existing-domain") != 0)
    {
      puts ("FAILED");
      result = 1;
    }

  /* Now the same tests with LC_MESSAGES deciding.  */
  unsetenv ("LC_ALL");
  setenv ("LC_MESSAGES", "existing-locale", 1);
  setenv ("LC_TIME", "existing-locale", 1);
  setenv ("LC_NUMERIC", "non-existing-locale", 1);
  puts ("test `gettext' with LC_ALL set");
  /* This is the name of the existing domain with a catalog for the
     LC_MESSAGES category.  */
  textdomain ("existing-domain");
  if (positive_gettext_test () != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  /* This is the name of a non-existing domain with a catalog for the
     LC_MESSAGES category.  We leave this value set for the `dgettext'
     and `dcgettext' tests.  */
  textdomain ("non-existing-domain");
  puts ("test `gettext' with LANGUAGE set");
  if (negative_gettext_test () != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  puts ("test `dgettext' with LANGUAGE set");
  if (positive_dgettext_test ("existing-domain") != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  puts ("test `dcgettext' with LANGUAGE set (LC_MESSAGES)");
  if (positive_dcgettext_test ("existing-domain", LC_MESSAGES) != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  /* Try a different category.  For this we also switch the domain.  */
  puts ("test `dcgettext' with LANGUAGE set (LC_TIME)");
  if (positive_dcgettext_test ("existing-time-domain", LC_TIME) != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  /* This time use a category for which there is no catalog.  */
  puts ("test `dcgettext' with LANGUAGE set (LC_NUMERIC)");
  if (negative_dcgettext_test ("existing-domain", LC_NUMERIC) != 0)
    {
      puts ("FAILED");
      result = 1;
    }

  /* Now the same tests with LANG deciding.  */
  unsetenv ("LC_MESSAGES");
  setenv ("LANG", "existing-locale", 1);
  /* This is the name of the existing domain with a catalog for the
     LC_MESSAGES category.  */
  textdomain ("existing-domain");
  puts ("test `gettext' with LC_ALL set");
  if (positive_gettext_test () != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  /* This is the name of a non-existing domain with a catalog for the
     LC_MESSAGES category.  We leave this value set for the `dgettext'
     and `dcgettext' tests.  */
  textdomain ("non-existing-domain");
  puts ("test `gettext' with LANGUAGE set");
  if (negative_gettext_test () != 0)
    {
      puts ("FAILED");
      result = 1;
    }
  puts ("test `dgettext' with LANGUAGE set");
  if (positive_dgettext_test ("existing-domain") != 0)
    {
      puts ("FAILED");
      result = 1;
    }

  return result;
}


static int
positive_gettext_test (void)
{
  size_t cnt;
  int result = 0;

  for (cnt = 0; cnt < sizeof (msgs) / sizeof (msgs[0]); ++cnt)
    {
      const char *found = gettext (msgs[cnt].msgid);

      if (found == NULL || strcmp (found, msgs[cnt].msgstr) != 0)
	{
	  /* Oops, shouldn't happen.  */
	  printf ("  gettext (\"%s\") failed, returned \"%s\"\n",
		  msgs[cnt].msgid, found);
	  result = 1;
	}
    }

  return result;
}


static int
negative_gettext_test (void)
{
  size_t cnt;
  int result = 0;

  for (cnt = 0; cnt < sizeof (msgs) / sizeof (msgs[0]); ++cnt)
    {
      const char *found = gettext (msgs[cnt].msgid);

      if (found != msgs[cnt].msgid)
	{
	  /* Oops, shouldn't happen.  */
	  printf ("  gettext (\"%s\") failed\n", msgs[cnt].msgid);
	  result = 1;
	}
    }

  return result;
}


static int
positive_dgettext_test (const char *domain)
{
  size_t cnt;
  int result = 0;

  for (cnt = 0; cnt < sizeof (msgs) / sizeof (msgs[0]); ++cnt)
    {
      const char *found = dgettext (domain, msgs[cnt].msgid);

      if (found == NULL || strcmp (found, msgs[cnt].msgstr) != 0)
	{
	  /* Oops, shouldn't happen.  */
	  printf ("  dgettext (\"%s\", \"%s\") failed, returned \"%s\"\n",
		  domain, msgs[cnt].msgid, found);
	  result = 1;
	}
    }

  return result;
}


static int
positive_dcgettext_test (const char *domain, int category)
{
  size_t cnt;
  int result = 0;

  for (cnt = 0; cnt < sizeof (msgs) / sizeof (msgs[0]); ++cnt)
    {
      const char *found = dcgettext (domain, msgs[cnt].msgid, category);

      if (found == NULL || strcmp (found, msgs[cnt].msgstr) != 0)
	{
	  /* Oops, shouldn't happen.  */
	  printf ("  dcgettext (\"%s\", \"%s\", %s) failed, returned \"%s\"\n",
		  domain, msgs[cnt].msgid, catname[category], found);
	  result = 1;
	}
    }

  return result;
}


static int
negative_dcgettext_test (const char *domain, int category)
{
  size_t cnt;
  int result = 0;

  for (cnt = 0; cnt < sizeof (msgs) / sizeof (msgs[0]); ++cnt)
    {
      const char *found = dcgettext (domain, msgs[cnt].msgid, category);

      if (found != msgs[cnt].msgid)
	{
	  /* Oops, shouldn't happen.  */
	  printf ("  dcgettext (\"%s\", \"%s\", %s) failed\n",
		  domain, msgs[cnt].msgid, catname[category]);
	  result = 1;
	}
    }

  return result;
}
