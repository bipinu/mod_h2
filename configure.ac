# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

AC_PREREQ([2.69])
AC_INIT([mod_http2], [2.0.32], [stefan@eissing.org])

LT_PREREQ([2.2.6])
LT_INIT()
dnl See versioning rule:
dnl  http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
AC_SUBST(LT_CURRENT, 11)
AC_SUBST(LT_REVISION, 0)
AC_SUBST(LT_AGE, 6)

major=`echo $PACKAGE_VERSION |cut -d. -f1 | sed -e "s/[^0-9]//g"`
minor=`echo $PACKAGE_VERSION |cut -d. -f2 | sed -e "s/[^0-9]//g"`
patch=`echo $PACKAGE_VERSION |cut -d. -f3 | cut -d- -f1 | sed -e "s/[^0-9]//g"`

PACKAGE_VERSION_NUM=`printf "0x%02x%02x%02x" "$major" "$minor" "$patch"`

AC_SUBST(PACKAGE_VERSION_NUM)

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([subdir-objects no-define])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_SRCDIR([mod_http2/h2_config.c])
AC_CONFIG_HEADERS([mod_http2/config.h])


AC_ARG_ENABLE([werror],
    [AS_HELP_STRING([--enable-werror],
                    [Turn on compile time warnings])],
    [werror=$enableval], [werror=no])

AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
                    [Turn on debug checks])],
    [debug=$enableval], [debug=no])

AC_ARG_WITH([apxs], [AS_HELP_STRING([--with-apxs],
    [Use APXS executable [default=check]])],
    [request_apxs=$withval], [request_apxs=check])

AC_ARG_ENABLE([poll-streams],
    [AS_HELP_STRING([--disable-poll-streams],
                    [Disable polling of h2 streams, even if available.])],
    [poll_streams=$enableval], [poll_streams=yes])

AC_ARG_WITH([openssl], [AS_HELP_STRING([--with-openssl], [Use openssl from this prefix])],
    [request_openssl=$withval], [request_openssl=check])

# Checks for programs.
AC_PROG_CC

# extern, we need to find where the apxs is. which then
# can tell us the various directories we need.
#
if test x"$request_apxs" = "xcheck"; then
    AC_PATH_PROG([APXS], [apxs])
    if test "x${APXS}" = "x"; then
        AC_MSG_ERROR("no APXS installation found")
    fi
else
    APXS="$request_apxs"
    AC_MSG_NOTICE("using APXS=$APXS as configured")
fi

AC_SUBST(APR)

prefix="$($APXS -q prefix)"
prefix="${prefix:-/usr}"
AC_SUBST(prefix)

if test x"$request_openssl" != "xcheck"; then
    LDFLAGS="$LDFLAGS -L$request_openssl/lib";
    CFLAGS="$CFLAGS -I$request_openssl/include";
    CPPFLAGS="$CPPFLAGS -I$request_openssl/include";
fi

exec_prefix="$($APXS -q exec_prefix)"
AC_SUBST(exec_prefix)
APR_BINDIR="$($APXS -q APR_BINDIR)"
AC_SUBST(APR_BINDIR)
sbindir="$($APXS -q sbindir)"
AC_SUBST(sbindir)
progname="$($APXS -q progname)"
AC_SUBST(progname)
libdir="$($APXS -q libdir)"
AC_SUBST(libdir)
SYSCONF_DIR="$($APXS -q sysconfdir)"
AC_SUBST(SYSCONF_DIR)
LIBEXEC_DIR="$($APXS -q LIBEXECDIR)"
AC_SUBST(LIBEXEC_DIR)

LDFLAGS="-L$libdir $LDFLAGS"
CPPFLAGS="-I$($APXS -q includedir) -I$($APXS -q APR_INCLUDEDIR) $($APXS -q EXTRA_CPPFLAGS) $CPPFLAGS"

HTTPD_VERSION="$($APXS -q HTTPD_VERSION)"
AC_SUBST(HTTPD_VERSION)

HTTPD="$sbindir/httpd"
if test -x "$HTTPD"; then
  :  # all fine
else
  HTTPD="$sbindir/apache2"
  if test -x "$HTTPD"; then
    :  # all fine
  else
    HTTPD=""
    AC_PATH_PROG([HTTPD], [httpd])
    if test -x "$HTTPD"; then
      :  # ok
    else
      HTTPD=""
      AC_PATH_PROG([HTTPD], [apache2])
      if test -x "$HTTPD"; then
        :  # ok
      else
        AC_MSG_ERROR([httpd/apache2 not in PATH])
      fi
    fi
  fi
fi
AC_SUBST(HTTPD)

if test -x "$HTTPD"; then
    DSO_MODULES="$($HTTPD -t -D DUMP_MODULES | fgrep '(shared)'| sed 's/_module.*//g'|tr -d \\n)"
    AC_SUBST(DSO_MODULES)
    STATIC_MODULES="$($HTTPD -t -D DUMP_MODULES | fgrep '(static)'| sed 's/_module.*//g'|tr -d \\n)"
    AC_SUBST(STATIC_MODULES)
else
    AC_MSG_WARN("httpd/apache2 not found in '$BINDIR', test suite will not work!")
    HTTPD=""
fi
AC_SUBST(LOAD_LOG_CONFIG)
AC_SUBST(LOAD_LOGIO)
AC_SUBST(LOAD_UNIXD)
AC_SUBST(LOAD_VERSION)
AC_SUBST(LOAD_WATCHDOG)

export BUILD_SUBDIRS="mod_http2"
AC_SUBST(BUILD_SUBDIRS)


AC_CHECK_HEADERS([ \
  arpa/inet.h \
  fcntl.h \
  inttypes.h \
  limits.h \
  netdb.h \
  netinet/in.h \
  pwd.h \
  stddef.h \
  stdint.h \
  stdlib.h \
  string.h \
  sys/socket.h \
  sys/time.h \
  syslog.h \
  time.h \
  unistd.h \
])

# We need nghttp2
PKG_CHECK_MODULES([LIBNGHTTP2], [libnghttp2 >= 1.2.0],
                  [have_nghttp2=yes], [have_nghttp2=no])
if test "x${have_nghttp2}" = "xno"; then
  AC_MSG_ERROR($LIBNGHTTP2_PKG_ERRORS)
fi

CFLAGS="$LIBNGHTTP2_CFLAGS $CFLAGS"
LIBS="$LIBNGHTTP2_LIBS $LIBS"

AC_CHECK_HEADERS([nghttp2/nghttp2.h])
# nghttp2 >= 1.3.0: access to stream weights
AC_CHECK_FUNCS([nghttp2_stream_get_weight],
        [CPPFLAGS="$CPPFLAGS -DH2_NG2_STREAM_API"], [])
# nghttp2 >= 1.5.0: changing stream priorities
AC_CHECK_FUNCS([nghttp2_session_change_stream_priority],
        [CPPFLAGS="$CPPFLAGS -DH2_NG2_CHANGE_PRIO"], [])
# nghttp2 >= 1.14.0: invalid header callback
AC_CHECK_FUNCS([nghttp2_session_callbacks_set_on_invalid_header_callback],
        [CPPFLAGS="$CPPFLAGS -DH2_NG2_INVALID_HEADER_CB"], [])
dnl # nghttp2 >= 1.15.0: get/set stream window sizes
AC_CHECK_FUNCS([nghttp2_session_get_stream_local_window_size],
        [CPPFLAGS="$CPPFLAGS -DH2_NG2_LOCAL_WIN_SIZE"], [])
AC_CHECK_FUNCS([nghttp2_option_set_no_closed_streams],
        [CPPFLAGS="$CPPFLAGS -DH2_NG2_NO_CLOSED_STREAMS"], [])
dnl # nghttp2 >= 1.50.0: rfc9113 leading/trailing whitespec strictness
AC_CHECK_FUNCS([nghttp2_option_set_no_rfc9113_leading_and_trailing_ws_validation],
        [CPPFLAGS="$CPPFLAGS -DH2_NG2_RFC9113_STRICTNESS"], [])

AC_PATH_PROG([NGHTTP], [nghttp], [], [$APR_BINDIR$PATH_SEPARATOR$PATH])
if test "x${NGHTTP}" = "x"; then
    if test -x "${prefix}/bin/nghttp"; then
        NGHTTP="${prefix}/bin/nghttp"
    else
        AC_MSG_WARN("no nghttp found, test coverage will be incomplete")
    fi
fi
AC_SUBST(NGHTTP)

AC_PATH_PROG([H2LOAD], [h2load], [], [$APR_BINDIR$PATH_SEPARATOR$PATH])
if test "x${H2LOAD}" = "x"; then
    if test -x "${prefix}/bin/h2load"; then
        H2LOAD="${prefix}/bin/h2load"
    else
        AC_MSG_WARN("no h2load found, test coverage will be incomplete")
    fi
fi
AC_SUBST(H2LOAD)



AC_CHECK_LIB([crypto], [SHA256_Init], [
CPPFLAGS="$CPPFLAGS -DH2_OPENSSL"
LIBS="$LIBS -lcrypto"],
[AC_MSG_ERROR("libcrypto not found")])

# Checks for header files.
AC_CHECK_HEADERS([ \
    assert.h \
    stddef.h \
])

DEBUG_CFLAGS=""
if test "x$debug" != "xno"; then
    DEBUG_CFLAGS="-DAP_DEBUG"
fi
AC_SUBST(DEBUG_CFLAGS)

WERROR_CFLAGS=""
if test "x$werror" != "xno"; then
    AX_CHECK_COMPILE_FLAG([-Wall], [WERROR_CFLAGS="$WERROR_CFLAGS -Wall"])
    #AX_CHECK_COMPILE_FLAG([-Wconversion], [WERROR_CFLAGS="$WERROR_CFLAGS -Wconversion"])
    AX_CHECK_COMPILE_FLAG([-Wextra], [WERROR_CFLAGS="$WERROR_CFLAGS -Wextra"])
    AX_CHECK_COMPILE_FLAG([-Werror], [WERROR_CFLAGS="$WERROR_CFLAGS -Werror"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-prototypes], [WERROR_CFLAGS="$WERROR_CFLAGS -Wmissing-prototypes"])
    AX_CHECK_COMPILE_FLAG([-Wstrict-prototypes], [WERROR_CFLAGS="$WERROR_CFLAGS -Wstrict-prototypes"])
    AX_CHECK_COMPILE_FLAG([-Wmissing-declarations], [WERROR_CFLAGS="$WERROR_CFLAGS -Wmissing-declarations"])
    AX_CHECK_COMPILE_FLAG([-Wpointer-arith], [WERROR_CFLAGS="$WERROR_CFLAGS -Wpointer-arith"])
    # we like to use such things...
    #AX_CHECK_COMPILE_FLAG([-Wdeclaration-after-statement], [WERROR_CFLAGS="$WERROR_CFLAGS -Wdeclaration-after-statement"])
    AX_CHECK_COMPILE_FLAG([-Wformat-security], [WERROR_CFLAGS="$WERROR_CFLAGS -Wformat-security"])
    AX_CHECK_COMPILE_FLAG([-Wwrite-strings], [WERROR_CFLAGS="$WERROR_CFLAGS -Wwrite-strings"])
    AX_CHECK_COMPILE_FLAG([-Wshadow], [WERROR_CFLAGS="$WERROR_CFLAGS -Wshadow"])
    AX_CHECK_COMPILE_FLAG([-Winline], [WERROR_CFLAGS="$WERROR_CFLAGS -Winline"])
    AX_CHECK_COMPILE_FLAG([-Wnested-externs], [WERROR_CFLAGS="$WERROR_CFLAGS -Wnested-externs"])
    AX_CHECK_COMPILE_FLAG([-Wfloat-equal], [WERROR_CFLAGS="$WERROR_CFLAGS -Wfloat-equal"])
    AX_CHECK_COMPILE_FLAG([-Wundef], [WERROR_CFLAGS="$WERROR_CFLAGS -Wundef"])
    AX_CHECK_COMPILE_FLAG([-Wendif-labels], [WERROR_CFLAGS="$WERROR_CFLAGS -Wendif-labels"])
    AX_CHECK_COMPILE_FLAG([-Wempty-body], [WERROR_CFLAGS="$WERROR_CFLAGS -Wempty-body"])
# does not make sense with APR as it prefers char* to void*
#    AX_CHECK_COMPILE_FLAG([-Wcast-align], [WERROR_CFLAGS="$WERROR_CFLAGS -Wcast-align"])
    AX_CHECK_COMPILE_FLAG([-Wvla], [WERROR_CFLAGS="$WERROR_CFLAGS -Wvla"])
    AX_CHECK_COMPILE_FLAG([-Wunreachable-code], [WERROR_CFLAGS="$WERROR_CFLAGS -Wunreachable-code"])
    AX_CHECK_COMPILE_FLAG([-Waddress], [WERROR_CFLAGS="$WERROR_CFLAGS -Waddress"])
    AX_CHECK_COMPILE_FLAG([-Wattributes], [WERROR_CFLAGS="$WERROR_CFLAGS -Wattributes"])
    AX_CHECK_COMPILE_FLAG([-Wdiv-by-zero], [WERROR_CFLAGS="$WERROR_CFLAGS -Wdiv-by-zero"])
    AX_CHECK_COMPILE_FLAG([-Wshorten-64-to-32], [WERROR_CFLAGS="$WERROR_CFLAGS -Wshorten-64-to-32"])
    AX_CHECK_COMPILE_FLAG([-Wsign-compare], [WERROR_CFLAGS="$WERROR_CFLAGS -Wsign-compare"])
    AX_CHECK_COMPILE_FLAG([-Wno-unused-parameter], [WERROR_CFLAGS="$WERROR_CFLAGS -Wno-unused-parameter"])
    #AX_CHECK_COMPILE_FLAG([-Wno-shadow], [WERROR_CFLAGS="$WERROR_CFLAGS -Wno-shadow"])
    #AX_CHECK_COMPILE_FLAG([-Wno-undef], [WERROR_CFLAGS="$WERROR_CFLAGS -Wno-undef"])

    AX_CHECK_COMPILE_FLAG(["-std=c89"], [WERROR_CFLAGS="$WERROR_CFLAGS -std=c89"])
    AX_CHECK_COMPILE_FLAG([-Wdeclaration-after-statement], [WERROR_CFLAGS="$WERROR_CFLAGS -Wdeclaration-after-statement"])

# gcc warnings, considered useful as in https://kristerw.blogspot.de/2017/09/useful-gcc-warning-options-not-enabled.html
#
    #AX_CHECK_COMPILE_FLAG(["-Wduplicated-cond"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wduplicated-cond"])
    #AX_CHECK_COMPILE_FLAG(["-Wduplicated-branches"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wduplicated-branches"])
    #AX_CHECK_COMPILE_FLAG(["-Wlogical-op"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wlogical-op"])
    #AX_CHECK_COMPILE_FLAG(["-Wrestrict"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wrestrict"])
    AX_CHECK_COMPILE_FLAG(["-Wnull-dereference"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wnull-dereference"])
    #AX_CHECK_COMPILE_FLAG(["-Wuseless-cast"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wuseless-cast"])
    #AX_CHECK_COMPILE_FLAG(["-Wjump-misses-init"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wjump-misses-init"])
    AX_CHECK_COMPILE_FLAG(["-Wdouble-promotion"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wdouble-promotion"])
    AX_CHECK_COMPILE_FLAG(["-Wshadow"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wshadow"])
    AX_CHECK_COMPILE_FLAG(["-Wformat=2"], [WERROR_CFLAGS="$WERROR_CFLAGS -Wformat=2"])

    # This is used by the APR_OFFSET macro
    AX_CHECK_COMPILE_FLAG([-Wno-null-pointer-subtraction], [WERROR_CFLAGS="$WERROR_CFLAGS -Wno-null-pointer-subtraction"])
    AX_CHECK_COMPILE_FLAG([-Wno-null-dereference], [WERROR_CFLAGS="$WERROR_CFLAGS -Wno-null-dereference"])
fi
AC_SUBST(WERROR_CFLAGS)

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([memmove memset strcasecmp strchr])

AC_ARG_WITH([serverdir], [AS_HELP_STRING([--with-serverdir],
    [Use serverdir directory for setup [default=gen/apache]])],
    [request_serverdir=$withval], [request_serverdir=$PWD/test/gen/apache])

AC_ARG_WITH([curl], [AS_HELP_STRING([--with-curl], [Use curl from this prefix])],
    [request_curl=$withval], [request_curl=check])

TEST_DIR="$PWD/test"
AC_SUBST(TEST_DIR)

GEN_DIR="$PWD/test/gen"
AC_SUBST(GEN_DIR)

SERVER_DIR="$request_serverdir"
AC_SUBST(SERVER_DIR)

HTTP_PORT=42001
AC_SUBST(HTTP_PORT)

HTTPS_PORT=42002
AC_SUBST(HTTPS_PORT)

HTTPD_USER="${USER:-www-data}"
AC_SUBST(HTTPD_USER)

MODULE_SRC="$PWD/mod_http2"
AC_SUBST(MODULE_SRC)

# we need curl for testing
#
curl_check="curl"
CURL_BIN="-"
if test "x$request_curl" != "xcheck"; then
    curl_check="$request_curl/bin/curl"
    test -x $curl_check && CURL_BIN=$curl_check
else
    AC_PATH_PROG([CURL_BIN],["$curl_check"], "-")
    if test "x$CURL_BIN" = "x-"; then
        curl_check="${prefix}/bin/curl"
        test -x $curl_check && CURL_BIN=$curl_check
    fi
fi

if test "x$CURL_BIN" = "x-"; then
    CURL_MSG="$curl_check was not found. Tests will not work."
    AC_MSG_WARN([$CURL_MSG])
else
    CURL_MSG="$CURL_BIN"
    AC_SUBST(CURL_BIN)

    curl_h2=$( $CURL_BIN -V | fgrep 'HTTP2' )
    if test "x$curl_h2" = "x"; then
        CURL_MSG="$CURL_BIN does not support HTTP/2. Tests will not work."
        AC_MSG_WARN([$CURL_MSG])
    fi
fi

# Do we have a pkg-config?
AC_ARG_VAR([PKGCONFIG], [pkg-config executable])
AC_PATH_PROG([PKGCONFIG], [pkg-config])


# do we poll streams, when available, or disable this (e.g. for test runs)
if test "$poll_streams" = "no"; then
CPPFLAGS="$CPPFLAGS -DH2_NO_PIPES"
fi


AC_CONFIG_FILES([
    Makefile
    mod_http2/Makefile
    mod_http2/h2_version.h
    test/Makefile
    test/pyhttpd/config.ini
    test/clients/Makefile
])

AC_OUTPUT

AC_MSG_NOTICE([summary of build options:

    Version:        ${VERSION} shared $LT_CURRENT:$LT_REVISION:$LT_AGE
    Host type:      ${host}
    Install prefix: ${prefix}
    APXS:           ${APXS}
    HTTPD-VERSION:  ${HTTPD_VERSION}
    C compiler:     ${CC}
    CFLAGS:         ${CFLAGS}
    WARNCFLAGS:     ${WARNCFLAGS}
    LDFLAGS:        ${LDFLAGS}
    LIBS:           ${LIBS}
    CPPFLAGS:       ${CPPFLAGS}
    curl            ${CURL_MSG}
    Libnghttp2:     ${LIBNGHTTP2_VERSION} (CFLAGS='${LIBNGHTTP2_CFLAGS}' LIBS='${LIBNGHTTP2_LIBS}')
    nghttp          ${NGHTTP:--}
    h2load          ${H2LOAD:--}
    httpd           ${HTTPD:--}
])
