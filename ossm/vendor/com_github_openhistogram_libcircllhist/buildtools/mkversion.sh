#!/bin/sh

STATUS=`git status 2>&1`
if [ $? -eq 0 ]; then
  echo "Building version info from git"
  HASH=`git show --format=%H | head -1`
  TSTAMP=`git show --format=%at | head -1`
  echo "    * version -> $HASH"
  SYM=`git name-rev $HASH | awk '{print $2;}' | sed -e 's/\^.*//'`
  if [ -z "`echo $SYM | grep '^tags/'`" ]; then
    SYM="branches/$SYM"
  fi
  echo "    * symbolic -> $SYM"
  BRANCH=$SYM
  VERSION="$HASH.$TSTAMP"
  if [ -n "`echo $STATUS | grep 'Changed but not updated'`" ]; then
    VERSION="$HASH.modified.$TSTAMP"
  fi
else
  BRANCH=exported
  echo "    * exported"
fi

if [ -r "$1" ]; then
  eval `cat circllhist_version.h | awk '/^#define/ { print $2"="$3;}'`
  if [ "$CIRCLLHIST_BRANCH" = "$BRANCH" -a "$CIRCLLHIST_VERSION" = "$VERSION" ]; then
    echo "    * version unchanged"
    exit
  fi
fi

cat > $1 <<EOF
#ifndef CIRCLLHIST_VERSION_H
#ifndef CIRCLLHIST_BRANCH
#define CIRCLLHIST_BRANCH "$BRANCH"
#endif
#ifndef CIRCLLHIST_VERSION
#define CIRCLLHIST_VERSION "$VERSION"
#endif

#include <stdio.h>

static inline int noit_build_version(char *buff, int len) {
  const char *start = CIRCLLHIST_BRANCH;
  if(!strncmp(start, "branches/", 9)) 
    return snprintf(buff, len, "%s.%s", start+9, CIRCLLHIST_VERSION);
  if(!strncmp(start, "tags/", 5)) 
    return snprintf(buff, len, "%s.%s", start+5, CIRCLLHIST_VERSION);
  return snprintf(buff, len, "%s.%s", CIRCLLHIST_BRANCH, CIRCLLHIST_VERSION);
}

#endif
EOF
