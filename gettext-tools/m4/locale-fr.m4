# locale-fr.m4 serial 1 (gettext-0.13)
dnl Copyright (C) 2003 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

dnl Determine the name of a french locale with traditional encoding.
AC_DEFUN([gt_LOCALE_FR],
[
  AC_CACHE_CHECK([for a traditional french locale], gt_cv_locale_fr, [
changequote(,)dnl
    cat <<EOF > conftest.$ac_ext
#include <locale.h>
#include <time.h>
struct tm t;
char buf[16];
int main () {
  /* Check whether the given locale name is recognized by the system.  */
  if (setlocale (LC_ALL, "") == NULL) return 1;
  /* Check whether in the abbreviation of the second month, the second
     character (should be U+00E9: LATIN SMALL LETTER E WITH ACUTE) is only
     one byte long. This excludes the UTF-8 encoding.  */
  t.tm_year = 1975 - 1900; t.tm_mon = 2 - 1; t.tm_mday = 4;
  if (strftime (buf, sizeof (buf), "%b", &t) < 3 || buf[2] != 'v') return 1;
  return 0;
}
EOF
changequote([,])dnl
    if AC_TRY_EVAL([ac_link]) && test -s conftest$ac_exeext; then
      # Test for the usual locale name.
      if (LC_ALL=fr_FR ./conftest; exit) 2>/dev/null; then
        gt_cv_locale_fr=fr_FR
      else
        # Test for the locale name with explicit encoding suffix.
        if (LC_ALL=fr_FR.ISO-8859-1 ./conftest; exit) 2>/dev/null; then
          gt_cv_locale_fr=fr_FR.ISO-8859-1
        else
          # Test for the AIX, OSF/1, FreeBSD, NetBSD locale name.
          if (LC_ALL=fr_FR.ISO8859-1 ./conftest; exit) 2>/dev/null; then
            gt_cv_locale_fr=fr_FR.ISO8859-1
          else
            # Test for the HP-UX locale name.
            if (LC_ALL=fr_FR.iso88591 ./conftest; exit) 2>/dev/null; then
              gt_cv_locale_fr=fr_FR.iso88591
            else
              # Test for the Solaris 7 locale name.
              if (LC_ALL=fr ./conftest; exit) 2>/dev/null; then
                gt_cv_locale_fr=fr
              else
                # Special test for NetBSD 1.6.
                if test -f /usr/share/locale/fr_FR.ISO8859-1/LC_CTYPE; then
                  gt_cv_locale_fr=fr_FR.ISO8859-1
                else
                  # None found.
                  gt_cv_locale_fr=fr_FR
                fi
              fi
            fi
          fi
        fi
      fi
    fi
    rm -fr conftest*
  ])
  LOCALE_FR=$gt_cv_locale_fr
  AC_SUBST([LOCALE_FR])
])
