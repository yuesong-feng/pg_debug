# contrib/pg_debug/Makefile

MODULE_big = pg_debug
OBJS = pg_debug.o $(WIN32RES) 
PGFILEDESC = "pg_debug - Postgresql hook example"

PG_CPPFLAGS = -I$(libpq_srcdir)
SHLIB_LINK_INTERNAL = $(libpq)

EXTENSION = pg_debug
DATA = pg_debug--1.0.sql

REGRESS = pg_debug # sql、expected 文件夹下的测试，

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
SHLIB_PREREQS = submake-libpq
subdir = contrib/pg_debug
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
