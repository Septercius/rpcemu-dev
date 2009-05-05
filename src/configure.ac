# configure.ac for the RPCemu
#

AC_PREREQ(2.50)
AC_INIT(RPCemu, 0.8.4, , rpcemu)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE(foreign)

AC_MSG_CHECKING([whether to enable debugging])
AC_ARG_ENABLE(debug,
	      AC_HELP_STRING([--enable-debug], [build debug executable]))
if test "$enable_debug" = "yes"; then
   CFLAGS="$CFLAGS -Wall -O0 -g -D_DEBUG"
   AC_MSG_RESULT([yes])
else
   CFLAGS="$CFLAGS -Wall -O2"
   AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([whether to use the dynamic recompiler])
AC_ARG_ENABLE(dynarec, 
              AC_HELP_STRING([--enable-dynarec],
                             [use dynamic recompiler (experimental)]),,
              enable_dynarec=no)
AC_MSG_RESULT($enable_dynarec)
AM_CONDITIONAL(DYNAREC, test "$enable_dynarec" = "yes")
if test "$enable_dynarec" = "yes"; then
   AC_MSG_CHECKING([for CPU])
   case "${host_cpu}" in
   	i?86)
		CPU=i386
		AC_MSG_RESULT(${host_cpu})
		;;
	x86_64)
		CPU=x86_64
		AC_MSG_RESULT(${host_cpu})
		;;
	*)
		AC_MSG_ERROR([Unsupported CPU.])
		;;
   esac
fi

AC_MSG_CHECKING([for Operating System])
case "${host}" in
    *linux*)
	    OS="linux"
	    AC_MSG_RESULT(Linux)
            AC_DEFINE(RPCEMU_LINUX, [], [OS is Linux])
	    ;;
    *mingw*)
	    OS="win"
  	    AC_MSG_RESULT(Windows)
            AC_DEFINE(RPCEMU_WIN, [], [OS is Windows])
	    ;;
    *apple*)
            OS="macosx"
            AC_MSG_RESULT([Mac OS X])
            AC_DEFINE(RPCEMU_MACOSX, [], [OS is Mac OS X])
            ;;
    *macosx*)
	    OS="macosx"
  	    AC_MSG_RESULT([Mac OS X])
            AC_DEFINE(RPCEMU_MACOSX, [], [OS is Mac OS X])
	    ;;
    *)
	    AC_MSG_ERROR([Unsupported host system.])
	    ;;
esac

AM_CONDITIONAL(OS_WIN, test "$OS" = "win")
AM_CONDITIONAL(OS_LINUX, test "$OS" = "linux")
AM_CONDITIONAL(OS_MACOSX, test "$OS" = "macosx")
AM_CONDITIONAL(CPU_X86_64, test "$CPU" = "x86_64")
AM_CONDITIONAL(CPU_I386, test "$CPU" = "i386")

AC_CONFIG_SRCDIR(arm.c)
AM_CONFIG_HEADER(config.h)
AM_PROG_CC_C_O
AC_C_BIGENDIAN(, , AC_MSG_ERROR(unknown endianness))
AC_ISC_POSIX

# Do not run test for Allegro with Win32/MinGW version, as binary builds have
# `allegro-config' missing.
# NOTE: For the following Autoconf macro to be supported, you need to extract
#       allegro.m4 from the DOS/Windows Allegro sources (the file is contained
#       in `misc') and copy it to this directory or MSYS's `/share/aclocal'.
if test "$OS" != "win"; then
   AM_PATH_ALLEGRO(, , AC_MSG_ERROR(building RPCemu requires Allegro to be installed))
fi

AC_OUTPUT([Makefile])
