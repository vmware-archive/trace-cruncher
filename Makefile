#
# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2019 VMware Inc, Yordan Karadzhov (VMware) <y.karadz@gmail.com>
#

UID := $(shell id -u)
GID := $(shell id -g)

CYAN	:= '\e[36m'
PURPLE	:= '\e[35m'
NC	:= '\e[0m'

DOCDIR = ./docs

CC = gcc
CFLAGS = -fPIC -Wall -Wextra -O2 -g
LDFLAGS = -shared -lbfd -lrt
RM = rm -rf

TC_BASE_LIB = tracecruncher/libtcrunchbase.so
PY_SETUP = setup

BASE_SRCS = src/trace-obj-debug.c
BASE_OBJS = $(BASE_SRCS:.c=.o)

all: $(TC_BASE_LIB) $(PY_SETUP)
	@ echo ${CYAN}Buildinging trace-cruncher:${NC};

$(PY_SETUP):
	python3 setup.py build

$(TC_BASE_LIB): $(BASE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	${RM} src/npdatawrapper.c
	${RM} $(TC_BASE_LIB)
	${RM} src/*.o
	${RM} build
	bash scripts/date-snapshot/date-snapshot.sh -d -f scripts/date-snapshot/repos

install:
	@ echo ${CYAN}Installing trace-cruncher:${NC};
	python3 setup.py install --record install_manifest.txt

uninstall:
	@ if [ $(UID) -ne 0 ]; then \
	echo ${PURPLE}Permission denied${NC} 1>&2; \
	else \
	echo ${CYAN}Uninstalling trace-cruncher:${NC}; \
	xargs rm -v < install_manifest.txt; \
	rm -rfv dist tracecruncher.egg-info; \
	rm -fv install_manifest.txt; \
	fi

doc:
	@ echo ${CYAN}Buildinging trace-cruncher documentation:${NC};
	@ python3 $(DOCDIR)/setup.py builtins
	@ sudo python3 $(DOCDIR)/setup.py tracecruncher.ftracepy $(UID) $(GID)
	@ sudo python3 $(DOCDIR)/setup.py tracecruncher.ft_utils $(UID) $(GID)

clean_doc:
	@ rm -f $(DOCDIR)/*.html
