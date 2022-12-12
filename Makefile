EXTENSION = set_user
EXTVERSION = $(shell grep default_version $(EXTENSION).control | \
               sed -e "s/default_version[[:space:]]*=[[:space:]]*'\([^']*\)'/\1/")
LDFLAGS_SL += $(filter -lm, $(LIBS))
MODULES = src/set_user
PG_CONFIG = pg_config
PGFILEDESC = "set_user - similar to SET ROLE but with added logging"
REGRESS = set_user
REGRESS_OPTS = "--inputdir=test"

all: sql/$(EXTENSION)--$(EXTVERSION).sql

sql/$(EXTENSION)--$(EXTVERSION).sql: sql/set_user.sql
	cat $^ > $@

DATA = $(wildcard updates/*--*.sql) sql/$(EXTENSION)--$(EXTVERSION).sql
EXTRA_CLEAN = sql/$(EXTENSION)--$(EXTVERSION).sql

ifdef NO_PGXS
subdir = contrib/set_user
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
else
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
endif

.PHONY: install-headers uninstall-headers

install: install-headers

install-headers:
	$(MKDIR_P) "$(DESTDIR)$(includedir)"
	$(INSTALL_DATA) "src/set_user.h" "$(DESTDIR)$(includedir)"

uninstall: uninstall-headers

uninstall-headers:
	rm "$(DESTDIR)$(includedir)/set_user.h"
