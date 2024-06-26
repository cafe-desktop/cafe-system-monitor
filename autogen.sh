#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="cafe-system-monitor"

(test -f $srcdir/configure.ac) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

which cafe-autogen || {
    echo "You need to install cafe-common"
    exit 1
}

which yelp-build || {
    echo "You need to install yelp-tools"
    exit 1
}

USE_COMMON_DOC_BUILD=yes

. cafe-autogen

