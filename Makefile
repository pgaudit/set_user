MODULES = set_user

EXTENSION = set_user
DATA = set_user--3.0.sql set_user--2.0--3.0.sql set_user--1.6--2.0.sql set_user--1.5--1.6.sql set_user--1.4--1.5.sql set_user--1.1--1.4.sql set_user--1.0--1.1.sql set_user--4.0.0rc1--4.0.0.sql set_user--4.0.0.sql set_user--3.0--4.0.0.sql
PGFILEDESC = "set_user - similar to SET ROLE but with added logging"

REGRESS = set_user

LDFLAGS_SL += $(filter -lm, $(LIBS))

ifdef NO_PGXS
subdir = contrib/set_user
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
else
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
endif

.PHONY: install-headers uninstall-headers

install: install-headers

install-headers:
	$(MKDIR_P) "$(DESTDIR)$(includedir)"
	$(INSTALL_DATA) "set_user.h" "$(DESTDIR)$(includedir)"

uninstall: uninstall-headers

uninstall-headers:
	rm "$(DESTDIR)$(includedir)/set_user.h"
