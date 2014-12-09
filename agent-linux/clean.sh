#! /bin/sh

set -x

true \
&& rm -f aclocal.m4 \
&& rm -f -r autom4te.cache \
&& rm -f collectps-*.tar.bz2 \
&& rm -f collectps-*.tar.gz \
&& rm -f compile \
&& rm -f config.guess \
&& rm -f config.log \
&& rm -f config.status \
&& rm -f config.sub \
&& rm -f configure \
&& rm -f depcomp \
&& rm -f install-sh \
&& rm -f -r libltdl \
&& rm -f libtool \
&& rm -f ltmain.sh \
&& rm -f Makefile \
&& rm -f Makefile.in \
&& rm -f missing \
&& rm -f INSTALL \
&& rm -f conf/Makefile \
&& rm -f conf/Makefile.in \
&& rm -f -r src/.deps \
&& rm -f -r src/.libs \
&& rm -f src/*.o \
&& rm -f src/*.la \
&& rm -f src/*.lo \
&& rm -f src/config.h \
&& rm -f src/config.h.in \
&& rm -f src/config.h.in~ \
&& rm -f src/Makefile \
&& rm -f src/Makefile.in \
&& rm -f src/stamp-h1 \
&& rm -f src/stamp-h1.in \
&& rm -f src/*.pb-c.c \
&& rm -f src/*.pb-c.h \
&& rm -f src/collectps_agent \
&& rm -f src/Makefile.in
