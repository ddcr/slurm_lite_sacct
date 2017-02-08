DIRS = src
BUILDDIRS = $(DIRS:%=build-%)
INSTALLDIRS = $(DIRS:%=install-%)
CLEANDIRS = $(DIRS:%=clean-%)

CC = gcc
CCDEPMODE = depmode=none
CFLAGS = -DNUMA_VERSION1_COMPATIBILITY -O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 \
	-fexceptions -fstack-protector --param=ssp-buffer-size=4 -m64 -mtune=generic \
	-pthread -fno-gcse
CPPFLAGS =
CMD_LDFLAGS =
CPP = gcc -E
CXX = g++
CXXCPP = g++ -E
CXXDEPMODE = depmode=none
CXXFLAGS = -O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fexceptions -fstack-protector \
	--param=ssp-buffer-size=4 -m64 -mtune=generic

DEFS = -DHAVE_CONFIG_H
DEFAULT_INCLUDES = -I/usr/include/mysql -I.
INCLUDES = -I./include -I.

CCLD = $(CC)
LD = /usr/bin/ld -m elf_x86_64
LDFLAGS =

COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)
sacct_LDFLAGS = -export-dynamic $(CMD_LDFLAGS)
sacct_LINK = $(CCLD) $(CFLAGS) $(sacct_LDFLAGS) \
	$(LDFLAGS) -o $@
sacct_LDADD = compat_pwdgrp.$(OBJEXT) src/api/libslurm.o -ldl

EXEEXT = .exe
OBJEXT = o
LIBS =

.SUFFIXES:
.SUFFIXES: .c .lo .o .obj

sacct_DEPENDENCIES = compat_pwdgrp.$(OBJEXT) src/api/libslurm.o
sacct_OBJECTS = sacct.$(OBJEXT) process.$(OBJEXT) print.$(OBJEXT) \
	options.$(OBJEXT)

noinst_HEADERS = sacct.c
HEADERS = $(noinst_HEADERS)

bin_PROGRAMS = sacct$(EXEEXT)
PROGRAMS = $(bin_PROGRAMS)

all: $(PROGRAMS)

$(DIRS): $(BUILDDIRS)

$(BUILDDIRS):
	$(MAKE) -C $(@:build-%=%)

sacct$(EXEEXT): config.h $(BUILDDIRS) $(sacct_OBJECTS) $(sacct_DEPENDENCIES)
	@rm -f sacct$(EXEEXT)
	$(sacct_LINK) $(sacct_OBJECTS) $(sacct_LDADD) $(LIBS)

plugin: NEW=1
plugin: config.h
	$(MAKE) -C src/plugins/jobcomp/filetxt clean
	$(MAKE) -C src/plugins/jobcomp/filetxt install
	$(MAKE) -C src/plugins/accounting_storage/mysql clean
	$(MAKE) -C src/plugins/accounting_storage/mysql install

test_bits: test_bits.c
	$(CC) -o test_bits.x test_bits.c

test_memory_issues: config.h test_memory_issues.o src/api/libslurm.o
	@rm -f $@$(EXEEXT)
	$(CCLD) $(CFLAGS) $(LDFLAGS) -o $@$(EXEEXT) test_memory_issues.o \
	src/api/libslurm.o -ldl

test_memory_issues.c.o:
	$(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) -c $@

test_compat_pwdgrp: config.h test_compat_pwdgrp.o compat_pwdgrp.o src/api/libslurm.o
	@rm -f $@$(EXEEXT)
	$(CCLD) $(CFLAGS) $(LDFLAGS) -o $@$(EXEEXT) test_compat_pwdgrp.o compat_pwdgrp.o \
	src/api/libslurm.o -ldl

test_compat_pwdgrp.c.o:
	$(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) -c $@

compat_pwdgrp.c.o:
	$(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) -c $@

# force rebuilding of config.h even if it exists
config.h: .FORCE
.FORCE:
config.h: config.h.in
	if test "$(NEW)" == "1"; then \
		sed 's/\/\* \#undef NEWQUERY \*\//#define NEWQUERY 1/' config.h.in \
		> config.h || echo "Failed to recreate config.h";\
	else \
		cp config.h.in config.h; \
	fi;

.c.o:
	$(COMPILE) -c $<

clean: $(CLEANDIRS)
	@rm -f $(sacct_OBJECTS)
$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

.PHONY: subdirs $(DIRS)
.PHONY: subdirs $(BUILDDIRS)
.PHONY: subdirs $(INSTALLDIRS)
.PHONY: subdirs $(CLEANDIRS)
.PHONY: all clean
.PHONY: .FORCE
