#! /bin/sh
# Translate an app-defaults file to a resource definition usable
# by XrmGetStringDatabase
#
# res_string.sh app-defaults-file

# change '"' and '#' to '\"' and '\\#'
# change '\n' to '\\n'
# remove '!' comments
# add a newline at the end of all non-continuation lines
# compress blanks and tabs to single blanks
# add quotes front and back
sed \
    -e '/^!/d'			\
    -e 's_"_\\&_g'		\
    -e 's_#_\\\\&_g'		\
    -e 's_\\n_\\\\n_g'		\
    -e 's/[^\\]$/&\\n\\/'	\
    -e 's/[ 	][ 	]*/ /g'	\
    -e '$s/\\$/"/'		\
    $* | \
sed '1s/^/"/'
