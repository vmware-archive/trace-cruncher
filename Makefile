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
CFLAGS = -fPIC -Wall -Wextra -O2 -g $(shell python3-config --cflags)
LDFLAGS = -shared -lbfd -lrt $(shell python3-config --ldflags)
RM = rm -rf

TC_BASE_LIB = tracecruncher/libtcrunchbase.so
PY_SETUP = setup

BASE_SRCS = src/trace-obj-debug.c src/tcrunch-base.c
BASE_OBJS = $(BASE_SRCS:.c=.o)

all: $(TC_BASE_LIB) $(PY_SETUP)
	@ echo ${CYAN}Buildinging trace-cruncher:${NC};

$(PY_SETUP):
	python3 setup.py build

$(TC_BASE_LIB): $(BASE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test:
ifneq ($(UID), 0)
	$(error Unit tests must be executed with root privileges, access to the ftrace system is required)
endif
	$(warning trace-cruncher under test must be installed on the system before running these tests.)
	$(info Running unit tests:)
	cd tests && python3 -m unittest discover .

clean:
	${RM} src/npdatawrapper.c
	${RM} $(TC_BASE_LIB)
	${RM} src/*.o
	${RM} build
	bash scripts/git-snapshot/git-snapshot.sh -d -f scripts/git-snapshot/repos

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
	@ python3 $(DOCDIR)/setup.py tracecruncher.ftracepy $(UID) $(GID)
	@ python3 $(DOCDIR)/setup.py tracecruncher.ft_utils $(UID) $(GID)

clean_doc:
	@ rm -f $(DOCDIR)/*.html
