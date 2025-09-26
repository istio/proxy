AC_INIT(src/circllhist.c)

node=`hostname | cut -f1 -d '.' | tr '[A-Z]' '[a-z]'`
if test -f ".configure.$node"; then
	echo "***********************************************************"
	echo "* Loading node specific configure settings for $node."
	echo "***********************************************************"
	. ./.configure.$node
fi

AC_CANONICAL_HOST

AC_ARG_ENABLE(strict,
	[AC_HELP_STRING([--enable-strict],
		[Compile with warnings treated as errors])],
	enable_strict="$enableval",
	enable_strict=no)

AC_PROG_CC
AC_C_INLINE
AC_C_BIGENDIAN
AC_PROG_CPP
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_PROG(AR, ar)
AC_PATH_PROGS(PERL, perl)
AC_PATH_PROG(NROFF, nroff)
AC_PATH_PROG(CTFCONVERT, ctfconvert)
AC_PATH_PROG(CTFMERGE, ctfmerge)

if test "x$GCC" = "xyes" ; then
	C99FLAG="-std=c99"
	CFLAGS="$CFLAGS -g $C99FLAG"
	DEPFLAGS="-MM"
else
	C99FLAG="-xc99=all"
	CFLAGS="$CFLAGS -g $C99FLAG"
	DEPFLAGS="-xM1"
fi
SHLDFLAGS="$LDFLAGS"

DOTSO=.so

if test "x$COPT" = "x" ; then
	COPT="-O5 -DNDEBUG"
fi

case $host in
*-*-darwin*)
	DOTSO=
	DOTDYLIB=dylib
	DTRACE=/usr/sbin/dtrace
	DTRACE_ENABLED=1
	PICFLAGS="-fPIC"
	CPPFLAGS="$CPPFLAGS -D_DARWIN_C_SOURCE -DMAC_OS_X_VERSION_MIN_REQUIRED=MAC_OS_X_VERSION_10_6 -Wno-deprecated-declarations"
	MODULELD="$CC -bundle -flat_namespace -undefined suppress"
	SHLD="$CC -dynamiclib -single_module -undefined dynamic_lookup -fPIC"
  DOTDYLIB=.dylib
	MODULEEXT=bundle
	;;
*-*-solaris*)
	DTRACE=/usr/sbin/dtrace
	DTRACE_ENABLED=1
	CFLAGS="$CFLAGS"
	if test "x$GCC" != "xyes" ; then
		CFLAGS="$CFLAGS -mt"
		PICFLAGS="-Kpic"
		MODULELD="$CC -G"
		SHLD="$CC -G"
	else
		PICFLAGS="-fPIC"
		MODULELD="$CC -shared"
		SHLD="$CC -shared"
		CLINKFLAGS="-shared-libgcc"
	fi
	MODULEEXT=so
	RLDFLAG="-R"
	WHOLE_ARCHIVE="-Wl,-z -Wl,allextract"
	NOWHOLE_ARCHIVE="-Wl,-z -Wl,defaultextract"
	;;
*-*-openbsd*)
	PICFLAGS="-fpic"
	LDFLAGS="$LDFLAGS -Wl,-E"
	MODULELD="$CC -shared"
	SHLD="$CC -shared"
	MODULEEXT=so
	LDFLAGS="$LDFLAGS -L/usr/local/lib"
	CFLAGS="$CFLAGS -I/usr/local/include"
	RLDFLAG="-Wl,--rpath="
	WHOLE_ARCHIVE="-Wl,--whole-archive"
	NOWHOLE_ARCHIVE="-Wl,--no-whole-archive"
	;;
*-*-linux*)
	LDFLAGS="$LDFLAGS -Wl,-E"
	PICFLAGS="-fpic"
	MODULELD="$CC -shared"
	SHLD="$CC -shared"
	MODULEEXT=so
	RLDFLAG="-Wl,--rpath="
	WHOLE_ARCHIVE="-Wl,--whole-archive"
	NOWHOLE_ARCHIVE="-Wl,--no-whole-archive"
	;;
*-*-freebsd*)
	PICFLAGS="-fpic"
	MODULELD="$CC -shared"
	LDFLAGS="$LDFLAGS -export-dynamic -L/usr/local/lib"
	CFLAGS="$CFLAGS -DCOMPAT_43TTY -I/usr/local/include"
	SHLD="$CC -shared"
	MODULEEXT=so
	RLDFLAG="-Wl,--rpath="
	;;
*)
	PICFLAGS="-fpic"
	MODULELD="$CC -shared"
	MODULEEXT=so
	RLDFLAG="-Wl,--rpath="
	;;
esac

CPPFLAGS="$CPPFLAGS -D_REENTRANT "'-I$(top_srcdir)/src'

AC_CHECK_HEADERS([alloca.h])
AC_CHECK_LIB(m, floor, ,)


AC_SUBST(DOTSO)
AC_SUBST(DOTDYLIB)
AC_SUBST(CLINKFLAGS)
AC_SUBST(COPT)
AC_SUBST(SHLD)
AC_SUBST(RLDFLAG)
AC_SUBST(DEPFLAGS)
AC_SUBST(MODULELD)
AC_SUBST(MODULEEXT)
AC_SUBST(WHOLE_ARCHIVE)
AC_SUBST(NOWHOLE_ARCHIVE)

# Checks for data types
AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(short int, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long int, 4)
AC_CHECK_SIZEOF(long long int, 8)
AC_CHECK_SIZEOF(void *, 1)

SHCFLAGS="$PICFLAGS $CFLAGS"
AC_SUBST(SHCFLAGS)
AC_SUBST(LD)
AC_SUBST(SHLDFLAGS)

SUBPREFIX="$prefix"
if test "x$prefix" = "xNONE"; then
	SUBPREFIX="$ac_default_prefix"
fi
AC_MSG_CHECKING([Resolving libexec path...])
fullpath=$libexecdir
while test "x$temppath" != "x$fullpath" ; do
	temppath=$fullpath
	fullpath=`eval echo ${fullpath}`
done
fullpath=`echo $fullpath | sed "s~NONE~$SUBPREFIX~"`

AC_MSG_CHECKING([Resolving lib path...])
fullpath=$libdir
while test "x$temppath" != "x$fullpath" ; do
	temppath=$fullpath
	fullpath=`eval echo ${fullpath}`
done
fullpath=`echo $fullpath | sed "s~NONE~$SUBPREFIX~"`
AC_MSG_RESULT([$fullpath])
LIB_DIR=${fullpath}

AC_MSG_CHECKING([Resolving sysconfdir path...])
fullpath=$sysconfdir
while test "x$temppath" != "x$fullpath" ; do
	temppath=$fullpath
	fullpath=`eval echo ${fullpath}`
done
fullpath=`echo $fullpath | sed "s~NONE~$SUBPREFIX~"`
AC_MSG_RESULT([$fullpath])
ETC_DIR=${fullpath}
AC_SUBST(ETC_DIR)

if test -n "`$CC -V 2>&1 | grep 'Sun C'`"; then
	if test "$enable_string" != "no"; then
		CFLAGS="$CFLAGS -errwarn=%all"
	fi
else
	CFLAGS="$CFLAGS -Wall"
	if test "$enable_strict" != "no"; then
		CFLAGS="$CFLAGS -Werror"
	fi
fi

AC_OUTPUT([
Makefile
src/Makefile
])
