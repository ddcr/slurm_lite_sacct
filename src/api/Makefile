#
#  * @author         Domingos Rodrigues
#  * @modified       Tue Oct  4 22:31:58 2016
#
#  This Makefile only builds libslurm.o
#
top_srcdir = ../..
top_builddir = ../..
srcdir = .

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

CCLD = $(CC)
LD = /usr/bin/ld -m elf_x86_64
LDFLAGS =
SO_LDFLAGS =

LIBTOOL = /bin/sh $(top_builddir)/libtool
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

noinst_PROGRAMS = libslurm.o$(EXEEXT)
PROGRAMS = $(noinst_PROGRAMS)

am_libslurmhelper_la_OBJECTS = allocate.lo allocate_msg.lo \
	cancel.lo checkpoint.lo \
	complete.lo config_info.lo init_msg.lo job_info.lo \
	job_step_info.lo node_info.lo node_select_info.lo \
	partition_info.lo reservation_info.lo signal.lo \
	slurm_hostlist.lo slurm_pmi.lo step_ctx.lo step_io.lo \
	step_launch.lo pmi_server.lo submit.lo suspend.lo topo_info.lo \
	triggers.lo reconfigure.lo update_config.lo
libslurmhelper_la_OBJECTS = $(am_libslurmhelper_la_OBJECTS)

libslurm_o_LDADD = $(top_builddir)/src/common/libeio.o	\
	$(top_builddir)/src/common/libspank.o	\
	$(top_builddir)/src/common/libcommon.o

all: all-am

.SUFFIXES:
.SUFFIXES: .c .lo .o .obj

.c.o:
	$(COMPILE) -c $<

.c.lo:
	$(LTCOMPILE) -c -o $@ $<

clean-generic:

clean-libtool:
	-rm -rf .libs _libs

mostlyclean-compile:
	-rm -f *.$(OBJEXT)

mostlyclean-libtool:
	-rm -f *.lo $(pkglib_LTLIBRARIES)

mostlyclean-am: mostlyclean-compile mostlyclean-libtool

clean: clean-generic clean-libtool mostlyclean-am

all-am: $(PROGRAMS)

libslurm.o: $(libslurmhelper_la_OBJECTS) $(libslurm_o_LDADD)
	$(LINK) $(libslurmhelper_la_OBJECTS) $(libslurm_o_LDADD)

.NOEXPORT:
