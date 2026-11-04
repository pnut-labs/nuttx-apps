############################################################################
# apps/database/sqlite/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs

SQLITEDIR=$(APPDIR)/database/sqlite
NXTOOLSDIR = $(APPDIR)/tools
BUILDDIR=$(SQLITEDIR)/build

CSRCS += ${BUILDDIR}/sqlite3.c

CFLAGS += ${INCDIR_PREFIX}$(SQLITEDIR)
CFLAGS += ${DEFINE_PREFIX}_HAVE_SQLITE_CONFIG_H
CFLAGS += ${DEFINE_PREFIX}NDEBUG
CFLAGS += -Wno-unused-variable -Wno-undef -Wno-unused-function -Wno-shadow

# Leave out what the system lacks: loadable extensions need dlopen(), and
# the shell's pipes and commands popen() and system()

ifeq ($(CONFIG_LIBC_DLFCN),)
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_LOAD_EXTENSION
endif

ifeq ($(CONFIG_SYSTEM_POPEN),)
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_POPEN
endif

ifeq ($(CONFIG_SYSTEM_SYSTEM),)
CFLAGS += ${DEFINE_PREFIX}SQLITE_NOHAVE_SYSTEM
endif

# Leave out what a small system does not use: the deprecated and
# shared-cache interfaces, JSON, WAL (it needs a shared memory map of a
# file) and memory-mapped I/O; temporary tables stay in memory.  The shell
# needs sqlite3_complete().  sqlite.org warns that SQLITE_OMIT_* options
# may not work with the amalgamation: these leave the grammar alone.

ifneq ($(CONFIG_LIB_SQLITE_SMALL),)
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_DEPRECATED
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_SHARED_CACHE
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_PROGRESS_CALLBACK
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_JSON
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_WAL
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_DECLTYPE
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_TRACE
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_UTF16
ifeq ($(CONFIG_UTILS_SQLITE),)
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_COMPLETE
endif
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_GET_TABLE
CFLAGS += ${DEFINE_PREFIX}SQLITE_OMIT_TCL_VARIABLE
CFLAGS += ${DEFINE_PREFIX}SQLITE_DEFAULT_MEMSTATUS=0
CFLAGS += ${DEFINE_PREFIX}SQLITE_MAX_MMAP_SIZE=0
CFLAGS += ${DEFINE_PREFIX}SQLITE_TEMP_STORE=3
CFLAGS += ${DEFINE_PREFIX}SQLITE_DQS=0
CFLAGS += ${DEFINE_PREFIX}SQLITE_LIKE_DOESNT_MATCH_BLOBS
endif

ifneq ($(CONFIG_UTILS_SQLITE),)
PROGNAME  = sqlite3
PRIORITY  = 100
STACKSIZE = ${CONFIG_UTILS_SQLITE_STACKSIZE}
MODULE = $(CONFIG_UTILS_SQLITE)
MAINSRC = ${BUILDDIR}/shell.c
endif


# The amalgamation: generated from a git checkout in sqlite/ (which needs
# tclsh), or else sqlite.org's own, which needs nothing

SQLITE_VERSION = $(patsubst "%",%,$(CONFIG_LIB_SQLITE_VERSION))
SQLITE_YEAR    = $(patsubst "%",%,$(CONFIG_LIB_SQLITE_YEAR))
SQLITE_NUMBER  = $(shell echo $(SQLITE_VERSION) | \
                   awk -F. '{ printf "%d%02d%02d00", $$1, $$2, $$3 }')
SQLITE_AMALG   = sqlite-amalgamation-$(SQLITE_NUMBER)
SQLITE_SHA256  = $(patsubst "%",%,$(CONFIG_LIB_SQLITE_SHA256))

ifneq ($(wildcard sqlite/.git),)
${BUILDDIR}/sqlite3.c:
	$(Q) mkdir -p ${BUILDDIR} && \
	cd ${BUILDDIR} && \
	echo "Generate SQLite amalgamation version" && \
	${SQLITEDIR}/sqlite/configure 1> /dev/null && \
	make sqlite3.c shell.c -j 1> /dev/null

# shell.c too, where an earlier build generated sqlite3.c alone

${BUILDDIR}/shell.c: ${BUILDDIR}/sqlite3.c
	$(Q) cd ${BUILDDIR} && make shell.c 1> /dev/null && touch shell.c
else

# A stamp named after the version, so that another version is fetched
# again; the files are dated now, not as in the zip, so that they are
# compiled again

SQLITE_STAMP = ${BUILDDIR}/.$(SQLITE_AMALG)

${BUILDDIR}/sqlite3.c ${BUILDDIR}/shell.c: $(SQLITE_STAMP)

$(SQLITE_STAMP):
	$(if $(and $(SQLITE_YEAR),$(SQLITE_SHA256)),,$(error \
	  CONFIG_LIB_SQLITE_YEAR or _SHA256 is empty: run make olddefconfig))
	$(Q) curl -L -f --retry 3 \
	  https://www.sqlite.org/$(SQLITE_YEAR)/$(SQLITE_AMALG).zip \
	  -o $(SQLITE_AMALG).zip
	$(Q) $(NXTOOLSDIR)/check-hash.sh sha256 $(SQLITE_SHA256) \
	  $(SQLITE_AMALG).zip
	$(Q) unzip -q -o $(SQLITE_AMALG).zip
	$(Q) mkdir -p ${BUILDDIR}
	$(Q) mv $(SQLITE_AMALG)/* ${BUILDDIR}/
	$(call DELDIR, $(SQLITE_AMALG))
	$(call DELFILE, $(SQLITE_AMALG).zip)
	$(call DELFILE, ${BUILDDIR}/.sqlite-amalgamation-*)
	$(Q) touch $@
	$(Q) touch ${BUILDDIR}/sqlite3.c ${BUILDDIR}/sqlite3.h \
	  ${BUILDDIR}/sqlite3ext.h ${BUILDDIR}/shell.c
endif

context:: ${BUILDDIR}/sqlite3.c $(if $(CONFIG_UTILS_SQLITE),${BUILDDIR}/shell.c)

clean::
	$(call DELFILE, ${BUILDDIR}/*.o)

distclean::
	$(call DELDIR, ${BUILDDIR})
	$(call DELDIR, sqlite-amalgamation-*)
	$(call DELFILE, sqlite-amalgamation-*.zip)

include $(APPDIR)/Application.mk
