top_srcdir = ../../../..
top_builddir = ../../../..
srcdir = .
libdir = $(realpath $(top_builddir))/lib64
pkglibdir = $(libdir)/slurm

NORMAL_INSTALL = :
NORMAL_UNINSTALL = :

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
pkglib_LTLIBRARIES = jobcomp_filetxt.la
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

LIB_LDFLAGS =

# Text file job completion logging plugin.
jobcomp_filetxt_la_SOURCES = jobcomp_filetxt.c \
			filetxt_jobcomp_process.c filetxt_jobcomp_process.h

jobcomp_filetxt_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)
jobcomp_filetxt_la_CFLAGS =

am_jobcomp_filetxt_la_OBJECTS = \
	jobcomp_filetxt_la-jobcomp_filetxt.lo \
	jobcomp_filetxt_la-filetxt_jobcomp_process.lo
jobcomp_filetxt_la_OBJECTS = $(am_jobcomp_filetxt_la_OBJECTS)
jobcomp_filetxt_la_LINK = $(LIBTOOL) --tag=CC \
	$(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=link $(CCLD) \
	$(jobcomp_filetxt_la_CFLAGS) $(CFLAGS) \
	$(jobcomp_filetxt_la_LDFLAGS) $(LDFLAGS) -o $@
am_jobcomp_filetxt_la_rpath = -rpath \
	$(pkglibdir)
am__strip_dir = `echo $$p | sed -e 's|^.*/||'`;

.SUFFIXES:
.SUFFIXES: .c .lo .o .obj
.PHONY: clean install

jobcomp_filetxt.la: $(jobcomp_filetxt_la_OBJECTS) \
	$(jobcomp_filetxt_la_DEPENDENCIES)
	$(jobcomp_filetxt_la_LINK) \
	$(am_jobcomp_filetxt_la_rpath) \
	$(jobcomp_filetxt_la_OBJECTS) \
	$(jobcomp_filetxt_la_LIBADD) $(LIBS)

.c.o:
	$(COMPILE) -c $<

.c.lo:
	$(LTCOMPILE) -c -o $@ $<

jobcomp_filetxt_la-jobcomp_filetxt.lo: jobcomp_filetxt.c
	$(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
	--mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(jobcomp_filetxt_la_CFLAGS) $(CFLAGS) \
	-c -o jobcomp_filetxt_la-jobcomp_filetxt.lo \
	`test -f 'jobcomp_filetxt.c' || echo '$(srcdir)/'`jobcomp_filetxt.c

jobcomp_filetxt_la-filetxt_jobcomp_process.lo: filetxt_jobcomp_process.c
	$(LIBTOOL) --tag=CC $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) \
	--mode=compile $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(AM_CPPFLAGS) $(CPPFLAGS) $(jobcomp_filetxt_la_CFLAGS) $(CFLAGS) \
	-c -o jobcomp_filetxt_la-filetxt_jobcomp_process.lo \
	`test -f 'filetxt_jobcomp_process.c' || echo '$(srcdir)/'`filetxt_jobcomp_process.c

clean-generic:

clean-libtool:
	-rm -rf .libs _libs

mostlyclean-compile:
	-rm -f *.$(OBJEXT)

mostlyclean-libtool:
	-rm -f *.lo $(pkglib_LTLIBRARIES)

mostlyclean-am: mostlyclean-compile mostlyclean-libtool

clean: clean-generic clean-libtool mostlyclean-am uninstall

all: all-am

all-am: Makefile $(LTLIBRARIES) install

install: install-am
uninstall: uninstall-am

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
uninstall-pkglibLTLIBRARIES:
		@$(NORMAL_UNINSTALL)
		@list='$(pkglib_LTLIBRARIES)'; for p in $$list; do \
		  p=$(am__strip_dir) \
		  echo " $(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=uninstall rm -f '$(DESTDIR)$(pkglibdir)/$$p'"; \
		  $(LIBTOOL) $(AM_LIBTOOLFLAGS) $(LIBTOOLFLAGS) --mode=uninstall rm -f "$(DESTDIR)$(pkglibdir)/$$p"; \
		done
uninstall-am: uninstall-pkglibLTLIBRARIES
