top_srcdir = ../../../..
top_builddir = ../../../..
srcdir = .
libdir = $(realpath $(top_builddir))/lib64
pkglibdir = $(libdir)/slurm

NORMAL_INSTALL = :

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
pkglib_LTLIBRARIES = accounting_storage_mysql.la

pkglibLTLIBRARIES_INSTALL = $(INSTALL)
LTLIBRARIES = $(pkglib_LTLIBRARIES)

INSTALL = /usr/bin/install -c
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_PROGRAM = ${INSTALL}
INSTALL_SCRIPT = ${INSTALL}
INSTALL_STRIP_PROGRAM = $(install_sh) -c -s

CCLD = $(CC)
LD = /usr/bin/ld -m elf_x86_64
LDFLAGS =
SO_LDFLAGS =
PLUGIN_FLAGS = -module -avoid-version --export-dynamic

LIBTOOL = /bin/sh $(top_builddir)/libtool
MKDIR_P = /bin/mkdir -p
COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
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

# Mysql storage plugin.
accounting_storage_mysql_la_SOURCES = accounting_storage_mysql.c \
	mysql_jobacct_process.c mysql_jobacct_process.h \
	mysql_rollup.c mysql_rollup.h
accounting_storage_mysql_la_LIBADD = \
	$(top_builddir)/src/database/libslurm_mysql.la $(MYSQL_LIBS)
accounting_storage_mysql_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)
accounting_storage_mysql_la_CFLAGS = $(MYSQL_CFLAGS)

am_accounting_storage_mysql_la_OBJECTS = \
	accounting_storage_mysql_la-accounting_storage_mysql.lo \
	accounting_storage_mysql_la-mysql_jobacct_process.lo \
	accounting_storage_mysql_la-mysql_rollup.lo
accounting_storage_mysql_la_OBJECTS = $(am_accounting_storage_mysql_la_OBJECTS)
accounting_storage_mysql_la_DEPENDENCIES = $(top_builddir)/src/database/libslurm_mysql.la
accounting_storage_mysql_la_LINK = $(LIBTOOL) --tag=CC \
	$(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link $(CCLD) \
	$(accounting_storage_mysql_la_CFLAGS) $(CFLAGS) \
	$(accounting_storage_mysql_la_LDFLAGS) $(LDFLAGS) -o $@
am_accounting_storage_mysql_la_rpath = -rpath \
	$(pkglibdir)
am__strip_dir = `echo $$p | sed -e 's|^.*/||'`;

.SUFFIXES:
.SUFFIXES: .c .lo .o .obj
.PHONY: clean install

accounting_storage_mysql.la: $(accounting_storage_mysql_la_OBJECTS) \
	$(accounting_storage_mysql_la_DEPENDENCIES)
	$(accounting_storage_mysql_la_LINK) \
	$(am_accounting_storage_mysql_la_rpath) \
	$(accounting_storage_mysql_la_OBJECTS) \
	$(accounting_storage_mysql_la_LIBADD) $(LIBS)

.c.o:
	$(COMPILE) -c $<

.c.lo:
	$(LTCOMPILE) -c -o $@ $<

accounting_storage_mysql_la-accounting_storage_mysql.lo: accounting_storage_mysql.c
	$(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
	--mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(accounting_storage_mysql_la_CFLAGS) $(CFLAGS) \
	-c -o accounting_storage_mysql_la-accounting_storage_mysql.lo \
	`test -f 'accounting_storage_mysql.c' || echo '$(srcdir)/'`accounting_storage_mysql.c

accounting_storage_mysql_la-mysql_jobacct_process.lo: mysql_jobacct_process.c
	$(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
	--mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(accounting_storage_mysql_la_CFLAGS) $(CFLAGS) \
	-c -o accounting_storage_mysql_la-mysql_jobacct_process.lo \
	`test -f 'mysql_jobacct_process.c' || echo '$(srcdir)/'`mysql_jobacct_process.c

accounting_storage_mysql_la-mysql_rollup.lo: mysql_rollup.c
	$(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
	--mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(accounting_storage_mysql_la_CFLAGS) $(CFLAGS) \
	-c -o accounting_storage_mysql_la-mysql_rollup.lo \
	`test -f 'mysql_rollup.c' || echo '$(srcdir)/'`mysql_rollup.c

clean-generic:

clean-libtool:
	-rm -rf .libs _libs

mostlyclean-compile:
	-rm -f *.$(OBJEXT)

mostlyclean-libtool:
	-rm -f *.lo $(pkglib_LTLIBRARIES)

mostlyclean-am: mostlyclean-compile mostlyclean-libtool

clean: clean-generic clean-libtool mostlyclean-am

all: all-am

all-am: Makefile $(LTLIBRARIES) install

install: install-am
install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am
install-exec-am: install-pkglibLTLIBRARIES
install-pkglibLTLIBRARIES: $(pkglib_LTLIBRARIES)
		@$(NORMAL_INSTALL)
		test -z "$(pkglibdir)" || $(MKDIR_P) "$(DESTDIR)$(pkglibdir)"
		@list='$(pkglib_LTLIBRARIES)'; for p in $$list; do \
		  if test -f $$p; then \
		    f=$(am__strip_dir) \
		    echo " $(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(pkglibLTLIBRARIES_INSTALL) $(INSTALL_STRIP_FLAG) '$$p' '$(DESTDIR)$(pkglibdir)/$$f'"; \
		    $(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=install $(pkglibLTLIBRARIES_INSTALL) $(INSTALL_STRIP_FLAG) "$$p" "$(DESTDIR)$(pkglibdir)/$$f"; \
		  else :; fi; \
		done
