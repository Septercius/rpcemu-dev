# configure.in for the RPCemu
#

AC_PREREQ(2.50)
AC_INIT(rpcemu, 0.4)
AC_CONFIG_SRCDIR(arm.c)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)
AC_CANONICAL_HOST
AC_PROG_CC
AC_ISC_POSIX

AC_PATH_XTRA

AM_PATH_ALLEGRO(, , AC_MSG_ERROR(project requires Allegro to be installed))
AC_SUBST(topdir)
AC_OUTPUT([Makefile])
