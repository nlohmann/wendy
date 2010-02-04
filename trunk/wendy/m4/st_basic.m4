# serial 1
AC_DEFUN([AC_ST_BASIC],[

# store the SVN revision number
AC_CHECK_FILE([.svn],[
  AC_SUBST(VERSION_SVN, "`svnversion -n 2> /dev/null`")
  AC_DEFINE_UNQUOTED(VERSION_SVN, "${VERSION_SVN}", [The SVN revision.])],
  AC_DEFINE_UNQUOTED(VERSION_SVN, "not versioned", [The SVN revision.])
)

# look up canonical build name and write it to config.h
AC_CANONICAL_BUILD
AC_DEFINE_UNQUOTED([CONFIG_BUILDSYSTEM], ["$build"], [The platform.])


AC_PROG_SED
AC_PROG_GREP
AC_PROG_AWK

# export tools needed to determine memory consumption
AC_DEFINE_UNQUOTED(TOOL_AWK, "${AWK}", [awk])
AC_DEFINE_UNQUOTED(TOOL_GREP, "${GREP}", [grep])


# check for additional programs needed to compile
AM_MISSING_PROG(GENGETOPT, gengetopt)

AM_MISSING_PROG(HELP2MAN, help2man)


# allow the configure script to control assertions (just include config.h)
AC_HEADER_ASSERT
AH_BOTTOM([#ifdef __cplusplus
#include <cassert>
#else
#include <assert.h>
#endif])

])