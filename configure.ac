AC_PREREQ([2.69])
AC_INIT([Aedis], [0.0.1], [mzimbres@gmail.com])
AC_CONFIG_MACRO_DIR([m4])
#AC_CONFIG_SRCDIR([src/aedis.cpp])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall foreign])

# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB
AM_PROG_AR

AX_BOOST_BASE([1.78],, AC_MSG_ERROR[Boost not found])

AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT64_T
AC_CHECK_TYPES([ptrdiff_t])

AC_MSG_CHECKING([whether C++20 is enabled])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#if __cplusplus < 202002L]]
    [[#error C++20 not available]]
    [[#endif]])],
  [AC_MSG_RESULT([yes])
    HAVE_CXX20=yes;],
  [AC_MSG_RESULT([no])
    HAVE_CXX20=no;])

AM_CONDITIONAL(HAVE_CXX20,test x$HAVE_CXX20 = xyes)
AC_CONFIG_FILES([Makefile doc/Doxyfile])
AC_OUTPUT
