# contrib/pg_fix/Makefile

PGFILEDESC = "pg_fix - fix relation page"
PGAPPICON = win32

PROGRAM = pg_fix
OBJS	= pg_fix.o

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_fix
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
