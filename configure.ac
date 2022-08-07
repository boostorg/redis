AC_PREREQ([2.69])
AC_INIT([Aedis], [0.3.0], [mzimbres@gmail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR(include/aedis.hpp)
AM_INIT_AUTOMAKE([-Wall foreign])
AC_LANG(C++)

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

# This check has been stolen from Asio
AC_MSG_CHECKING([whether coroutines are enabled])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM(
    [[#if defined(__clang__)]]
    [[# if (__cplusplus >= 201703) && (__cpp_coroutines >= 201703)]]
    [[#  if __has_include(<experimental/coroutine>)]]
    [[#   define AEDIS_HAS_CO_AWAIT 1]]
    [[#  endif]]
    [[# endif]]
    [[#elif defined(__GNUC__)]]
    [[# if (__cplusplus >= 201709) && (__cpp_impl_coroutine >= 201902)]]
    [[#  if __has_include(<coroutine>)]]
    [[#   define AEDIS_HAS_CO_AWAIT 1]]
    [[#  endif]]
    [[# endif]]
    [[#endif]]
    [[#ifndef AEDIS_HAS_CO_AWAIT]]
    [[# error coroutines not available]]
    [[#endif]])],
  [AC_MSG_RESULT([yes])
    HAVE_COROUTINES=yes;],
  [AC_MSG_RESULT([no])
    HAVE_COROUTINES=no;])

AM_CONDITIONAL(HAVE_COROUTINES,test x$HAVE_COROUTINES = xyes)

AC_CONFIG_FILES([Makefile include/Makefile doc/Doxyfile])
AC_OUTPUT
