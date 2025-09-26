AC_INIT([tclap],[1.2.5])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR([examples/test1.cpp])
AM_CONFIG_HEADER(config/config.h)
AM_INIT_AUTOMAKE
AC_PROG_CXX
AC_CXX_HAVE_SSTREAM
AC_CXX_HAVE_STRSTREAM
AC_CXX_HAVE_LONG_LONG
AC_CHECK_PROG(DOT,dot,YES,NO)
AC_PROG_RANLIB
AC_PROG_INSTALL
BB_ENABLE_DOXYGEN

HAVE_WARN_EFFECTIVE_CXX
CXXFLAGS="$CXXFLAGS $WARN_EFFECTIVE_CXX"

AM_CONDITIONAL([HAVE_GNU_COMPILERS], [test x$ac_cv_cxx_compiler_gnu = xyes])

AC_CONFIG_FILES([ Makefile \
	   tclap.pc \
           examples/Makefile \
		   include/Makefile \
		   include/tclap/Makefile \
		   config/Makefile \
		   docs/Makefile \
		   docs/Doxyfile  \
		   msc/Makefile  \
		   msc/examples/Makefile  \
		   tests/Makefile])
AC_CONFIG_COMMANDS([default],[\
		   chmod a+x $ac_top_srcdir/tests/*.sh],[])
AC_OUTPUT
