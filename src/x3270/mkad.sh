#! /bin/sh
# Translate an app-defaults file to fallback definitions.

# delete all comments
# change '"' and '#' to '\"' and '\\#'
# change '\n' to '\\n'
# change all lines to "...",
# remove trailing '",' from lines ending in '\'
# remove all '"' before whitespace
# compress all spaces and tabs
sed \
    -e '/^!/d'				\
    -e 's_"_\\&_g'			\
    -e 's_#_\\\\&_g'			\
    -e 's_\\n_\\\\n_g'			\
    -e 's_^[^/].*_"&",_'		\
    -e 's_\\",$_\\_'			\
    -e 's_^"\([ 	]\)_\1_'	\
    -e 's/[ 	][ 	]*/ /g'		\
    $*
