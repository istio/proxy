#!/bin/sh
set -e

if [ -x /usr/bin/gsed ]; then
    SED=gsed
else
    SED=sed
fi
AWK=awk
if [ -x /usr/bin/ggrep ]; then
    GREP=ggrep
else
    GREP=grep
fi
if [ -x /usr/bin/gegrep ]; then
    EGREP=gegrep
else
    EGREP=egrep
fi

cat |\
  $GREP -v -F '/* FFI_SKIP */' |\
  $GREP -v "^$" |\
  $SED 's|//.*$||' |\
  $EGREP -v "#if|#endif|#define|#include|SSIZE_T" |\
  $SED 's/u_int/uint/g' |\
  $SED 's/API_EXPORT(\([^\)]*\))/\1/g' |\
  $AWK '/typedef struct histogram histogram_t;/ {print "typedef long int ssize_t;"} /./{print}'
