dnl
dnl  Configure templat for SABUL
dnl   -- Qiao (11/06/01)
dnl
AC_INIT(sabul.cpp)
AC_CONFIG_HEADER(config.h)
AC_PREREQ(0.1)

dnl
dnl  the version of SABUL
dnl
VERSION=`sed -e 's/^.*"\(.*\)";$/\1/' ${srcdir}/version.c` 
echo "configure for SABUL $VERSION"
AC_SUBST(VERSION)
PACKAGE=Sabul
AC_SUBST(PACKAGE)

dnl
dnl  check the compiler
dnl
AC_PROG_CPP
AC_PROG_INSTALL



dnl
dnl  check for system type
dnl
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED(OS_TYPE, "$host_os")                                         
dnl
dnl  for Sun, add "-lsock -lnsl"
dnl
case "$host_os" in 
   sun*) $LIBS = -lpthread -lsocket -lnsl ;;
   *) $LIBS = -lpthread ;;
esac


