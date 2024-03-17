dnl Autoconf settings for npapi-vlc

AC_COPYRIGHT([Copyright 2002-2019 VLC authors and VideoLAN])

AC_INIT(npapi-vlc, 4.0.0)
VERSION_MAJOR="4"
AC_SUBST(VERSION_MAJOR)
VERSION_MINOR="0"
AC_SUBST(VERSION_MINOR)
VERSION_REVISION="0"
AC_SUBST(VERSION_REVISION)
VERSION_EXTRA=""
AC_SUBST(VERSION_EXTRA)
dnl Win32 need s a numerical version_extra.
case $( echo ${VERSION_EXTRA}|wc -m ) in
       "1") VERSION_EXTRA_RC="0";;
       "2") VERSION_EXTRA_RC=$( echo ${VERSION_EXTRA}|tr "abcdefghi" "123456789") ;;
       *) if test "${VERSION_EXTRA}" = "-git"; then VERSION_EXTRA_RC="666"; else VERSION_EXTRA_RC="99"; fi
esac
AC_SUBST(VERSION_EXTRA_RC)
COPYRIGHT_YEARS="2002-2019"
AC_SUBST(COPYRIGHT_YEARS)
COPYRIGHT_MESSAGE="Copyright © ${COPYRIGHT_YEARS} VLC authors and VideoLAN"
AC_SUBST(COPYRIGHT_MESSAGE)

AC_CONFIG_AUX_DIR(autotools)
AC_CONFIG_MACRO_DIR(m4)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PRESERVE_HELP_ORDER

AM_INIT_AUTOMAKE(tar-ustar color-tests)
AC_CONFIG_HEADERS([config.h])

# Disable with "./configure --disable-silent-rules" or "make V=1"
AM_SILENT_RULES([yes])

dnl
dnl Check for tools
dnl
AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS
AC_DEFINE([_FORTIFY_SOURCE], 2, [Define to '2' to get glibc warnings.])
AC_PROG_CXX
AC_PROG_OBJCXX
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

dnl override platform specific check for dependent libraries
dnl otherwise libtool linking of shared libraries will
dnl fail on anything other than pass_all.
AC_CACHE_VAL(lt_cv_deplibs_check_method,
    [lt_cv_deplibs_check_method=pass_all])

AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG
AC_CHECK_TOOL(DLLTOOL, dlltool, :)

dnl Check for compiler properties
AC_C_CONST

WIDLFLAGS=""
dnl
dnl  Check the operating system
dnl
case "${host_os}" in
  darwin*)
    SYS=darwin

    dnl Force gcc "-arch" flag
    ARCH_flag=""
    case "${host}" in
      i*86*)
        ARCH_flag="-arch i386"
      ;;
      ppc64*)
        ARCH_flag="-arch ppc64"
      ;;
      ppc*)
        ARCH_flag="-arch ppc"
      ;;
      x86_64*)
        ARCH_flag="-arch x86_64"
      ;;
    esac

    CFLAGS="${CFLAGS} -fobjc-exceptions -D_INTL_REDIRECT_MACROS ${ARCH_flag}"
    CXXFLAGS="${CXXFLAGS} -D_INTL_REDIRECT_MACROS ${ARCH_flag} -stdlib=libc++"
    OBJCXXFLAGS="${OBJCXXFLAGS} -fobjc-exceptions -D_INTL_REDIRECT_MACROS ${ARCH_flag} -stdlib=libc++ -std=c++11"
    CPPFLAGS="${CPPFLAGS} ${ARCH_flag}"
    LDFLAGS="${LDFLAGS} -Wl,-headerpad_max_install_names ${ARCH_flag}"
    ;;
  *mingw32*)
    SYS=mingw32
    AC_CHECK_TOOL(WINDRES, windres, :)
    AC_TRY_COMPILE([#include <windows.h>], [
      #if _WIN32_WINNT >= 0x0501
      #error new enough by default
      #endif
    ], [
      AC_DEFINE([_WIN32_WINNT], 0x0501, [Define to '0x0501' for Windows XP APIs.])
    ])
    AC_DEFINE([_WIN32_IE], 0x0600, [Define to '0x0600' for IE6.0 APIs.])
    AC_DEFINE([_WIN32], 1, [Define to 1 if targetting Windows.])
    CC="$CC -static-libgcc"
    CXX="$CXX -static-libgcc -static-libstdc++"
    ;;
esac
AM_CONDITIONAL(HAVE_DARWIN, test "${SYS}" = "darwin")
AM_CONDITIONAL(HAVE_WIN32, test "${SYS}" = "mingw32")

LT_INIT

dnl
dnl Check widl/midl tools for win32
AS_IF([test "${SYS}" = "mingw32"],[
    AC_CHECK_PROGS(MIDL, [midl], no)
    AC_CHECK_TOOL(WIDL, [widl], no)

    case "${host}" in
        amd64*|x86_64*|aarch64*)
            WIDLFLAGS=" -m64"
        ;;
        *)
            WIDLFLAGS=" -m32"
        ;;
    esac
    AC_LANG_PUSH(C++)
    AC_CHECK_HEADER(ole2.h,,[AC_MSG_ERROR([required OLE header ole2.h is missing from your system])])
    AC_CHECK_HEADER(olectl.h,,[AC_MSG_ERROR([required OLE header olectl.h is missing from your system])])
    AC_CHECK_HEADERS(objsafe.h,,,[#include <ole2.h>])

    HAS_WIDL_DASH_L=no
    AS_IF([test "${WIDL}" != "no"],[
        AC_MSG_CHECKING([widl uses -L])
        AS_IF([$WIDL $WIDLFLAGS -L. -t /dev/null 2>/dev/null],[
            AC_MSG_RESULT([yes])
            HAS_WIDL_DASH_L=yes
        ],[
            AC_MSG_RESULT([no])
        ])
    ])

    AC_MSG_CHECKING([for stdole2.tld presence])
    AS_IF([test "${WIDL}" != "no"],[
      ac_ext=idl
      ac_objext=tlb
      ac_compile='$WIDL $WIDLFLAGS -t conftest.$ac_ext -o conftest.$ac_objext'
      cat <<_ACEOF >conftest.idl
import "ocidl.idl";
[[
  uuid(DF2BBE39-40A8-433b-A279-073F48DA94B6),
]]
library AXVLC
{
  importlib("stdole2.tlb");
}
_ACEOF
        AS_IF([ac_fn_cxx_try_compile "$LINENO"],[
            AC_MSG_RESULT([yes])
            has_stdole2_tlb=yes
        ],[
            AC_MSG_RESULT([no])
            has_stdole2_tlb=no
        ])
    ],[
        AC_MSG_RESULT([no])
        has_stdole2_tlb=no
    ])

    AC_LANG_POP(C++)
],[
    MIDL="no"
    WIDL="no"
    has_stdole2_tlb="no"
])

AC_SUBST(WIDLFLAGS)
AM_CONDITIONAL(HAS_MIDL_COMPILER, test "${MIDL}" != "no")
AM_CONDITIONAL(HAS_WIDL_COMPILER, test "${WIDL}" != "no")
AM_CONDITIONAL(HAS_STDOLE2_TLB, test "${has_stdole2_tlb}" = "yes")
AM_CONDITIONAL(HAS_WIDL_DASH_L, test "${HAS_WIDL_DASH_L}" = "yes")

dnl
dnl Buggy glibc prevention. Purposedly not cached.
dnl See sourceware.org bugs 5058 and 5443.
dnl Ubuntu alone has 20 bug numbers for this...
dnl
AC_MSG_CHECKING(for buggy GNU/libc versions)
AC_PREPROC_IFELSE([AC_LANG_SOURCE([
#include <limits.h>
#if defined (__GLIBC__) && (__GLIBC__ == 2) \
  && (__GLIBC_MINOR__ >= 5) && (__GLIBC_MINOR__ <= 7)
# error GNU/libc with dcgettext killer bug!
#endif
])], [
  AC_MSG_RESULT([not present])
], [
  AC_MSG_RESULT([found])
  AC_MSG_ERROR([Buggy GNU/libc (version 2.5 - 2.7) present. VLC would crash; there is no viable
work-around for this. Check with your distribution vendor on how to update the
glibc run-time.])
])

dnl
dnl libraries checks
AC_PATH_XTRA
PKG_CHECK_MODULES([LIBVLC], [libvlc >= 4.0.0], [
    LIBVLC_PREFIX=`pkg-config --variable=prefix libvlc`
    AC_SUBST(LIBVLC_PREFIX)])


dnl
dnl ActiveX
AC_ARG_ENABLE(activex, AS_HELP_STRING([--enable-activex], [ActiveX plugin [default=yes (on Windows)]]))
AM_CONDITIONAL(BUILD_ACTIVEX, test "${SYS}" = "mingw32" -a "${enable_activex}" != "no")


dnl
dnl final flags for ActiveX
AM_COND_IF([HAVE_WIN32], [
  ACTIVEX_CPPFLAGS="${CPPFLAGS} -DUNICODE -D_UNICODE -D_MIDL_USE_GUIDDEF_"
  ACTIVEX_CXXFLAGS="${CXXFLAGS} -fno-exceptions"

  AC_ARG_VAR([ACTIVEX_LIBS], [linker flags for ActiveX])
  ACTIVEX_LIBS="${ACTIVEX_LIBS} -lole32 -loleaut32 -luuid -lshlwapi -lgdi32"
])

AC_CONFIG_FILES([
  Makefile
  share/Makefile
  common/Makefile
  activex/Makefile
])

AM_COND_IF([HAVE_WIN32], [
  AC_CONFIG_FILES([
    activex/axvlc.inf
    activex/axvlc_rc.rc
  ])
])
AC_OUTPUT
