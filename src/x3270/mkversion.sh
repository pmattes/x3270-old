#! /bin/sh
# Create version.o from version.txt
#set -x

set -e

. ./version.txt
builddate=`date`
sccsdate=`date +%y/%m/%d`
user=${LOGNAME-$USER}

trap 'rm -f version.c' 0 1 2 15

cat <<EOF >version.c
char *app_defaults_version = "$adversion";
char *build = "x3270 v$version $builddate $user";
static char sccsid[] = "@(#)x3270 v$version $sccsdate $user";
EOF

cc -c version.c
