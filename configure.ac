dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])

AC_INIT([cafe-system-monitor],
	[1.24.1],
        [https://cafe-desktop.org/])
AC_CONFIG_SRCDIR(configure.ac)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz foreign check-news])
AM_SILENT_RULES([yes])

AM_MAINTAINER_MODE([enable])
CAFE_MAINTAINER_MODE_DEFINES
CAFE_DEBUG_CHECK([no])

# Check For programs
AC_PROG_CC
AC_PROG_CXX
AC_LANG([C++])
AC_LANG_COMPILER_REQUIRE
AX_CXX_COMPILE_STDCXX_11([],[mandatory])
PKG_PROG_PKG_CONFIG([0.19])

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

# Package dependencies
GLIB_REQUIRED=2.56.0
LIBGTOP_REQUIRED=2.37.2
GIOMM_REQUIRED=2.26.0
GLIBMM_REQUIRED=2.22
CTK_REQUIRED=3.22.0
CTKMM_REQUIRED=3.8.1
LIBWNCK_REQUIRED=3.0.0
LIBXML_REQUIRED=2.0
RSVG_REQUIRED=2.35
SYSTEMD_REQUIRED=44

PKG_CHECK_MODULES(GMODULE,gmodule-2.0,[GMODULE_ADD="gmodule-2.0"],[GMODULE_ADD=""])
PKG_CHECK_MODULES(PROCMAN,$GMODULE_ADD glib-2.0 >= $GLIB_REQUIRED libgtop-2.0 >= $LIBGTOP_REQUIRED libwnck-3.0 >= $LIBWNCK_REQUIRED ctk+-3.0 >= $CTK_REQUIRED ctkmm-3.0 >= $CTKMM_REQUIRED libxml-2.0 >= $LIBXML_REQUIRED librsvg-2.0 >= $RSVG_REQUIRED glibmm-2.4 >= $GLIBMM_REQUIRED giomm-2.4 >= $GIOMM_REQUIRED)

PKG_CHECK_MODULES(TOOLS, glib-2.0 >= $GLIB_REQUIRED)

have_systemd=no
AC_ARG_ENABLE(systemd, AS_HELP_STRING([--disable-systemd], [disable systemd support]),,enable_systemd=no)
if test "x$enable_systemd" != "xno"; then
	PKG_CHECK_MODULES(SYSTEMD, [libsystemd], [have_systemd=yes],
	                  [PKG_CHECK_MODULES(SYSTEMD, [libsystemd-login >= $SYSTEMD_REQUIRED],
	                  [have_systemd=yes])])
	if test "x$have_systemd" = xno; then
		AC_MSG_ERROR([*** systemd support requested but libraries not found])
	else
		AC_DEFINE(HAVE_SYSTEMD, 1, [Define if systemd is available])
	fi
fi

AM_CONDITIONAL(HAVE_SYSTEMD, [test "$have_systemd" = "yes"])

# Compiler warnings
CAFE_COMPILE_WARNINGS([yes])
CAFE_CXX_WARNINGS([yes])

dnl CXXFLAGS="-fvisibility=hidden -fvisibility-inlines-hidden $CXXFLAGS"
dnl CXXFLAGS="-fvisibility-inlines-hidden $CXXFLAGS"

GLIB_GSETTINGS

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=cafe-system-monitor
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [The gettext catalog name])

# Documentation
YELP_HELP_INIT

AC_CONFIG_FILES([
Makefile
src/Makefile
src/org.cafe.system-monitor.gschema.xml
pixmaps/Makefile
po/Makefile.in
help/Makefile
tools/Makefile
cafe-system-monitor.desktop.in
])

AC_OUTPUT

echo "

Configuration:

        Source code location:   ${srcdir}
        C Compiler:             ${CC}
        CFLAGS:                 ${CFLAGS}
        WARN_CFLAGS:            ${WARN_CFLAGS}
        C++ Compiler:           ${CXX}
        CXXFLAGS:               ${CXXFLAGS}
        WARN_CXXFLAGS:          ${WARN_CXXFLAGS}
        Maintainer mode:        ${USE_MAINTAINER_MODE}
        Systemd support:        ${have_systemd}
"
