AC_INIT([Wendy], 3.4-unreleased, wendy@service-technology.org, wendy, http://service-technology.org/wendy)

AC_MSG_NOTICE([__        __             _       ])
AC_MSG_NOTICE([\ \      / /__ _ __   __| |_   _ ])
AC_MSG_NOTICE([ \ \ /\ / / _ \ '_ \ / _` | | | |])
AC_MSG_NOTICE([  \ V  V /  __/ | | | (_| | |_| |])
AC_MSG_NOTICE([   \_/\_/ \___|_| |_|\__,_|\__, |])
AC_MSG_NOTICE([                           |___/ ])
AC_MSG_NOTICE([synthesizing partners for services])
AC_MSG_NOTICE([${PACKAGE_URL}])
AC_MSG_NOTICE([version ${PACKAGE_VERSION}])
AC_MSG_NOTICE()

AC_CONFIG_SRCDIR([src/main.cc])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_AUX_DIR([build-aux])

# use the service-technology.org macros
AC_ST_BASIC
AC_ST_LOLA([1.14])
AC_ST_AUTOTEST
AC_ST_PNAPI([4.02])
AC_ST_VERBOSE

# needed to integrate "silence" utility
AC_DEFINE_UNQUOTED([UTIL_FOLDER], ["`pwd`/utils"], [The path to the utils folder.])

# search for optional tools needed by "make check"
AC_PATH_PROG(CNF2BITS,    [wendyFormula2bits], not found)
AC_PATH_PROG(FIONA,       [fiona], not found)
AC_PATH_PROG(MARLENE,     [marlene], not found)
AC_PATH_PROG(MIA,         [mia], not found)
AC_PATH_PROG(PETRI,       [petri], not found)
AC_PATH_PROG(VALGRIND,    [valgrind], not found)
AC_PATH_PROG(WENDY2FIONA, [wendy2fiona], not found)

# check for required functions and die if they are not found
AC_CHECK_FUNCS([popen pclose],,AC_MSG_ERROR([cannot use required functions]))

# FreeBSD does not know log2()
AC_CHECK_FUNCS([log2])

# check and set exact unsigned types
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF([void*])

# write files
AC_CONFIG_FILES([
	Makefile libs/pnapi/Makefile src/Makefile src/wendy.conf doc/Makefile
	doc/Doxyfile doc/Doxydetails tests/Makefile tests/package.m4])
AC_CONFIG_FILES([tests/cover.sh], [chmod +x tests/cover.sh])
AC_OUTPUT
