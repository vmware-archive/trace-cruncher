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

all:
	@ echo ${CYAN}Buildinging trace-cruncher:${NC};
	python3 setup.py build

clean:
	rm -f src/npdatawrapper.c
	rm -rf build

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

clean_doc:
	@ rm -f $(DOCDIR)/*.html
