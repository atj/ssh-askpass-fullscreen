AC_INIT([ssh-askpass-fullscreen], [0.4])
AC_CONFIG_SRCDIR(src/${PACKAGE_NAME}.c)

AM_INIT_AUTOMAKE([-Wall -Werror foreign dist-bzip2])
AM_CONFIG_HEADER([src/config.h])

AC_PROG_CC
AC_CHECK_HEADERS([stdio.h stdlib.h string.h unistd.h])
AC_CHECK_FUNCS([memset])

PKG_CHECK_MODULES([GTK], [gtk+-2.0 >= 2.10.0],,
		  AC_MSG_ERROR([>=GTK+-2.10 is required to compile ${PACKAGE_NAME}]
))

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

AC_OUTPUT([Makefile src/Makefile])

echo
echo "Now run \`make' to compile ${PACKAGE_NAME}"
echo
