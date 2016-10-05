#
#  * @author         Domingos Rodrigues
#  * @modified       Thu Sep 22 21:08:19 2016
#
top_srcdir = ../..
top_builddir = ../..
srcdir = .
CC = gcc
CCDEPMODE = depmode=none
CFLAGS = -DNUMA_VERSION1_COMPATIBILITY -O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 \
	-fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -mtune=generic \
	-pthread -fno-gcse
CMD_LDFLAGS =
CPP = gcc -E
CPPFLAGS =

DEFS = -DHAVE_CONFIG_H
DEFAULT_INCLUDES = -I. -I$(top_builddir) -I$(top_builddir)/include
INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/src/common

CCLD = $(CC)
LD = /usr/bin/ld -m elf_x86_64
LDFLAGS =

COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)

# DDCR: DRY-MODE
LIBTOOL = /bin/sh $(top_builddir)/libtool

LTCOMPILE = $(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
	--mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(CPPFLAGS) $(CFLAGS)
LINK = $(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
	--mode=link $(CCLD) $(CFLAGS) \
	$(LDFLAGS) -o $@

EXEEXT =
OBJEXT = o
LIBS =

MYSQL_CFLAGS = -I/usr/include/mysql  -g -pipe -Wp,-D_FORTIFY_SOURCE=2 -fexceptions \
	-fstack-protector --param=ssp-buffer-size=4 -m64 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 \
	-D_LARGEFILE_SOURCE -fno-strict-aliasing -fwrapv -fPIC -DUNIV_LINUX
MYSQL_LIBS = -rdynamic -L/usr/lib64/mysql -lmysqlclient_r -lz \
	-lpthread -lcrypt -lnsl -lm -lpthread -lssl -lcrypto
LIB_LDFLAGS =

MYSQL_LIB = libslurm_mysql.la
libslurm_mysql_la_SOURCES = mysql_common.c mysql_common.h
libslurm_mysql_la_LIBADD = $(MYSQL_LIBS)
libslurm_mysql_la_LDFLAGS = $(LIB_LDFLAGS)
libslurm_mysql_la_CFLAGS = $(MYSQL_CFLAGS)

am_libslurm_mysql_la_OBJECTS = libslurm_mysql_la-mysql_common.lo
libslurm_mysql_la_OBJECTS = $(am_libslurm_mysql_la_OBJECTS)
libslurm_mysql_la_DEPENDENCIES =

libslurm_mysql_la_LINK = $(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) \
	$(LIBTOOLFLAGS) --mode=link $(CCLD) \
	$(libslurm_mysql_la_CFLAGS) $(CFLAGS) \
	$(libslurm_mysql_la_LDFLAGS) $(LDFLAGS) -o $@
am_libslurm_mysql_la_rpath =

.SUFFIXES:
.SUFFIXES: .c .lo .o .obj

libslurm_mysql.la: $(libslurm_mysql_la_OBJECTS) $(libslurm_mysql_la_DEPENDENCIES)
	$(libslurm_mysql_la_LINK) $(am_libslurm_mysql_la_rpath) $(libslurm_mysql_la_OBJECTS) \
	$(libslurm_mysql_la_LIBADD) $(LIBS)

.c.o:
	$(COMPILE) -c $<

.c.lo:
	$(LTCOMPILE) -c -o $@ $<

libslurm_mysql_la-mysql_common.lo: mysql_common.c
	$(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) \
	$(LIBTOOLFLAGS) --mode=compile $(CC) $(DEFS) \
	$(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) \
	$(libslurm_mysql_la_CFLAGS) $(CFLAGS) \
	-c -o libslurm_mysql_la-mysql_common.lo \
	`test -f 'mysql_common.c' || echo '$(srcdir)/'`mysql_common.c

noinst_LTLIBRARIES = $(MYSQL_LIB)

clean-noinstLTLIBRARIES:
	-test -z "$(noinst_LTLIBRARIES)" || rm -f $(noinst_LTLIBRARIES)

clean-libtool:
	-rm -rf .libs _libs

mostlyclean-generic:

mostlyclean-compile:
	-rm -f *.$(OBJEXT)

mostlyclean-libtool:
	-rm -f *.lo

mostlyclean-am: mostlyclean-compile mostlyclean-generic \
	mostlyclean-libtool

clean-generic:

clean: clean-am

clean-am: clean-generic clean-libtool \
	mostlyclean-am clean-noinstLTLIBRARIES
