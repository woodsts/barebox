# SPDX-License-Identifier: GPL-2.0
# scripts/dtc makefile

hostprogs-always-$(CONFIG_DTC)		+= dtc fdtget

dtc-objs	:= dtc.o flattree.o fstree.o data.o livetree.o treesource.o \
		   srcpos.o checks.o util.o
dtc-objs	+= dtc-lexer.lex.o dtc-parser.tab.o

libfdt-objs	:= fdt.o fdt_ro.o fdt_strerror.o fdt_wip.o fdt_overlay.o
libfdt-objs	+= fdt_empty_tree.o fdt_rw.o fdt_sw.o
libfdt-objs	:= $(libfdt-objs:%.o=libfdt/%.o)

fdtget-objs    += fdtget.o $(libfdt-objs) util.o

# Source files need to get at the userspace version of libfdt_env.h to compile
HOST_EXTRACFLAGS += -I$(src)/libfdt

HOST_EXTRACFLAGS += -DNO_YAML

# Generated files need one more search path to include headers in source tree
HOSTCFLAGS_dtc-lexer.lex.o := -I$(src)
HOSTCFLAGS_dtc-parser.tab.o := -I$(src)

# dependencies on generated files need to be listed explicitly
$(obj)/dtc-lexer.lex.o: $(obj)/dtc-parser.tab.h
