MODULES = set_user

EXTENSION = set_user
DATA = set_user--1.6.sql set_user--1.5--1.6.sql set_user--1.4--1.5.sql set_user--1.1--1.4.sql set_user--1.0--1.1.sql
PGFILEDESC = "set_user - similar to SET ROLE but with added logging"

REGRESS = set_user

LDFLAGS_SL += $(filter -lm, $(LIBS))

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/set_user
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

.PHONY: install-headers uninstall-headers

install: install-headers

install-headers:
	$(INSTALL_DATA) "set_user.h" $(includedir)

uninstall: uninstall-headers

uninstall-headers:
	rm "$(includedir)/set_user.h"
