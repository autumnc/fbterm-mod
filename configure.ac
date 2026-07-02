#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([fbterm-mod], [1.20])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/fbterm.cpp])
AC_CONFIG_HEADERS([config.h])

AC_SUBST([RELEASE_DATE], "June 2026")

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG

AC_ARG_ENABLE(gpm, 
              AC_HELP_STRING([--disable-gpm], [disable gpm mouse support [[default=auto]]]), 
              [GPM="$enableval"],
              [GPM=auto])
              
AC_ARG_ENABLE(vesa, 
              AC_HELP_STRING([--disable-vesa], [disable VESA video card support [[default=auto]]]), 
              [VESA="$enableval"],
              [VESA=auto])

AC_ARG_ENABLE(epoll, 
              AC_HELP_STRING([--disable-epoll], [do not use epoll system call [[default=auto]]]), 
              [EPOLL="$enableval"],
              [EPOLL=auto])

AC_ARG_ENABLE(signalfd,
              AC_HELP_STRING([--disable-signalfd], [do not use signalfd system call [[default=auto]]]),
              [SIGNALFD="$enableval"],
              [SIGNALFD=auto])

AC_ARG_ENABLE(neon,
              AC_HELP_STRING([--enable-neon], [enable ARM NEON SIMD optimizations [[default=auto]]]),
              [NEON="$enableval"],
              [NEON=auto])

AC_ARG_ENABLE(rga,
              AC_HELP_STRING([--enable-rga], [enable Rockchip RGA hardware acceleration [[default=auto]]]),
              [RGA="$enableval"],
              [RGA=auto])

# ARM NEON detection
HAVE_ARM_NEON=no
if test x"$NEON" != xno; then
  saved_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -march=armv7-a -mfpu=neon"
  AC_MSG_CHECKING([whether ARM NEON intrinsics compile])
  AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([[#include <arm_neon.h>]],
      [[uint8x8_t v; (void)v; return 0;]])],
    [AC_MSG_RESULT([yes])
     HAVE_ARM_NEON=yes
     NEON_CFLAGS="-march=armv7-a -mfpu=neon"],
    [AC_MSG_RESULT([no])
     if test x"$NEON" = xyes; then
       AC_MSG_ERROR([--enable-neon requested but NEON intrinsics not supported])
     fi])
  CXXFLAGS="$saved_CXXFLAGS"
fi
AC_SUBST([NEON_CFLAGS])

                                          
# Checks for libraries.
AC_CHECK_LIB([util], [forkpty])
PKG_CHECK_MODULES([FT2], [freetype2])
PKG_CHECK_MODULES([FC], [fontconfig])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h langinfo.h locale.h stddef.h stdlib.h string.h termios.h unistd.h \
				  sys/ioctl.h sys/socket.h sys/time.h linux/fb.h linux/kd.h linux/keyboard.h linux/input.h],
				  [], [AC_MSG_ERROR(required header file is missing!)])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_STRUCT_TM

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MMAP
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_CHECK_FUNCS([memset memmove munmap nl_langinfo setenv setlocale socket strcasecmp strchr strstr strtol],
				[], [AC_MSG_ERROR(required lib function is missing!)])

GPM_WARN=
if test x"$GPM" != xno; then
	AC_CHECK_HEADERS([gpm.h])
	if test x"$GPM" = xauto; then
		GPM="$ac_cv_header_gpm_h"
		[[ "$GPM" = "no" ]] && GPM_WARN=yes 
	elif test x"$ac_cv_header_gpm_h" != xyes; then
		AC_MSG_ERROR(can't find gpm.h required by gpm mouse support!)
	fi
fi

VESA_WARN=
if test x"$VESA" != xno; then
	AC_CHECK_HEADERS([libx86.h])
	if test x"$VESA" = xauto; then
		VESA="$ac_cv_header_libx86_h"
		[[ "$VESA" = "no" ]] && VESA_WARN=yes
	elif test x"$ac_cv_header_libx86_h" != xyes; then
		AC_MSG_ERROR(can't find libx86.h required by VESA video card support!)
	fi
fi

if test x"$EPOLL" = xauto -a x"$cross_compiling" = xno; then
  AC_RUN_IFELSE(
    AC_LANG_PROGRAM([[#include <sys/epoll.h>]],
      [[if (epoll_create(10) >= 0) return 0; return 1;]]),
    [EPOLL=yes]
  )
fi

if test x"$SIGNALFD" = xauto -a x"$cross_compiling" = xno; then
  AC_RUN_IFELSE(
    AC_LANG_PROGRAM([[#include <sys/signalfd.h>]],
      [[sigset_t mask; if (signalfd(-1, &mask, 0) >= 0) return 0; return 1;]]),
    [SIGNALFD=yes]
  )
fi

RGA_WARN=
if test x"$RGA" != xno; then
	AC_CHECK_HEADERS([rga/im2d.h rga/RgaApi.h rga/RockchipRga.h])
	AC_CHECK_LIB([rga], [imcopy_t], [have_im2d=yes], [have_im2d=no])
	AC_CHECK_LIB([rga], [c_RkRgaBlit], [have_old_rga=yes], [have_old_rga=no])

	if test x"$have_im2d" = xyes -a x"$ac_cv_header_rga_im2d_h" = xyes; then
		RGA_MODE=im2d
	elif test x"$have_old_rga" = xyes; then
		if test x"$ac_cv_header_rga_RockchipRga_h" = xyes; then
			RGA_MODE=old_rga_rockchip
		elif test x"$ac_cv_header_rga_RgaApi_h" = xyes; then
			RGA_MODE=old_rga_api
		else
			RGA_MODE=none
		fi
	else
		RGA_MODE=none
	fi

	if test x"$RGA" = xauto; then
		if test x"$RGA_MODE" != xnone; then
			RGA=yes
		else
			RGA=no
			RGA_WARN=yes
		fi
	elif test x"$RGA" = xyes; then
		if test x"$RGA_MODE" = xnone; then
			AC_MSG_ERROR(can't find usable librga (need im2d or RgaApi)! install librga from https://github.com/airockchip/librga)
		fi
	fi
fi

AM_CONDITIONAL(VESA, test x"$VESA" = xyes)
AM_CONDITIONAL(HAVE_ARM_NEON, test x"$HAVE_ARM_NEON" = xyes)
AM_CONDITIONAL(ENABLE_RGA, test x"$RGA" = xyes)

RGA_LIBS=
[[ "$RGA" = "yes" ]] && RGA_LIBS="-lrga"
AC_SUBST([RGA_LIBS], ["$RGA_LIBS"])
AC_SUBST([RGA_MODE], ["$RGA_MODE"])

x86_LIBS=
[[ "$VESA" = "yes" ]] && x86_LIBS="-lx86"
AC_SUBST([X86_LIBS], ["$x86_LIBS"])

AH_TEMPLATE([ENABLE_GPM])
AH_TEMPLATE([ENABLE_VESA])
AH_TEMPLATE([HAVE_EPOLL])
AH_TEMPLATE([HAVE_ARM_NEON])
AH_TEMPLATE([ENABLE_RGA])
AH_TEMPLATE([RGA_USE_IM2D])
AH_TEMPLATE([RGA_USE_OLD_API])
AH_TEMPLATE([HAVE_SIGNALFD])

[[ "$GPM" = "yes" ]] && AC_DEFINE([ENABLE_GPM])
[[ "$VESA" = "yes" ]] && AC_DEFINE([ENABLE_VESA])
[[ "$EPOLL" = "yes" ]] && AC_DEFINE([HAVE_EPOLL])
[[ "$SIGNALFD" = "yes" ]] && AC_DEFINE([HAVE_SIGNALFD])
[[ "$HAVE_ARM_NEON" = "yes" ]] && AC_DEFINE([HAVE_ARM_NEON])
[[ "$RGA" = "yes" ]] && AC_DEFINE([ENABLE_RGA])
if test x"$RGA_MODE" = xim2d; then
	AC_DEFINE([RGA_USE_IM2D])
elif test x"$RGA" = xyes; then
	AC_DEFINE([RGA_USE_OLD_API])
fi

AC_CONFIG_FILES([Makefile src/Makefile src/lib/Makefile im/Makefile terminfo/Makefile doc/Makefile doc/fbterm.1])
AC_CONFIG_LINKS([im/input_key.h:src/input_key.h im/immessage.h:src/immessage.h])
AC_OUTPUT

[[ "$GPM_WARN" = "yes" ]] && AC_MSG_WARN([gpm.h doesn't exist! gpm mouse support will be disabled!])
[[ "$RGA_WARN" = "yes" ]] && AC_MSG_WARN([rga/im2d.h doesn't exist! RGA hardware acceleration will be disabled!])
[[ "$VESA_WARN" = "yes" ]] && AC_MSG_WARN([libx86.h doesn't exist! VESA support will be disabled!]) || exit 0
