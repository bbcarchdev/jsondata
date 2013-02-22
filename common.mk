CFLAGS=-Wall -Werror -D_LARGEFILE64_SOURCE
LDFLAGS=-lc -lm

ifneq ($(shell $(CC) -v 2>&1 | grep -i clang),)
CFLAGS+=-Qunused-arguments
else
CFLAGS+=-pthread
LDFLAGS+=-pthread
endif

ifneq ($(COVER),)
CFLAGS+=-fprofile-arcs -ftest-coverage
LDFLAGS+=-lgcov
endif

ifneq ($(PROFILE),)
CFLAGS+=-pg
LDFLAGS+=-pg
endif

ifneq ($(DEBUG),)
CFLAGS+=-g2
else
CFLAGS+=-O3
endif

