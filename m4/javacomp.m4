#serial 1

# Prerequisites of javacomp.sh.
# Sets HAVE_JAVACOMP to nonempty if javacomp.sh will work.

AC_DEFUN([gt_JAVACOMP],
[
  AC_MSG_CHECKING([for Java compiler])
  AC_EGREP_CPP(yes, [
#if defined _WIN32 || defined __WIN32__ || defined __EMX__ || defined __DJGPP__
  yes
#endif
], CLASSPATH_SEPARATOR=';', CLASSPATH_SEPARATOR=':')
  HAVE_JAVACOMP=1
  if test -n "$JAVAC"; then
    ac_result="$JAVAC"
  else
    pushdef([AC_MSG_CHECKING],[:])dnl
    pushdef([AC_CHECKING],[:])dnl
    pushdef([AC_MSG_RESULT],[:])dnl
    AC_CHECK_PROG(HAVE_GCJ_IN_PATH, gcj, yes)
    AC_CHECK_PROG(HAVE_JAVAC_IN_PATH, javac, yes)
    AC_CHECK_PROG(HAVE_JIKES_IN_PATH, jikes, yes)
    popdef([AC_MSG_RESULT])dnl
    popdef([AC_CHECKING])dnl
    popdef([AC_MSG_CHECKING])dnl
    if test -n "$HAVE_GCJ_IN_PATH" \
       && gcj --version 2>/dev/null | grep '^[3-9]' >/dev/null; then
      HAVE_GCJ=1
      ac_result="gcj -C"
    else
      if test -n "$HAVE_JAVAC_IN_PATH" \
         && (javac -version >/dev/null 2>/dev/null || test $? -le 2) \
         && (if javac -help 2>&1 >/dev/null | grep at.dms.kjc.Main >/dev/null && javac -help 2>/dev/null | grep 'released.*2000' >/dev/null ; then exit 1; else exit 0; fi); then
        HAVE_JAVAC=1
        ac_result="javac"
      else
        if test -n "$HAVE_JIKES_IN_PATH" \
           && (jikes >/dev/null 2>/dev/null || test $? = 1) \
           && (
            # See if the existing CLASSPATH is sufficient to make jikes work.
            cat > conftest.java <<EOF
public class conftest {
  public static void main (String[] args) {
  }
}
EOF
            unset JAVA_HOME
            jikes conftest.java
            error=$?
            rm -f conftest.java conftest.class
            exit $?
           ); then
          HAVE_JIKES=1
          ac_result="jikes"
        else
          HAVE_JAVACOMP=
          ac_result="no"
        fi
      fi
    fi
  fi
  AC_MSG_RESULT([$ac_result])
  AC_SUBST(JAVAC)
  AC_SUBST(CLASSPATH)
  AC_SUBST(CLASSPATH_SEPARATOR)
  AC_SUBST(HAVE_GCJ)
  AC_SUBST(HAVE_JAVAC)
  AC_SUBST(HAVE_JIKES)
])
