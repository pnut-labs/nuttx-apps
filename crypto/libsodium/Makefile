############################################################################
# apps/crypto/libsodium/Makefile
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

LIBSODIUM_VERSION = $(patsubst "%",%,$(strip $(CONFIG_LIBSODIUM_VERSION)))
LIBSODIUM_URL ?= "https://github.com/jedisct1/libsodium/releases/download/$(LIBSODIUM_VERSION)-RELEASE"

# Unquoted: as a make target, a name in quotes is never found, so the
# archive was downloaded again at every build, over any copy already there

LIBSODIUM_TARGZ = libsodium-$(LIBSODIUM_VERSION).tar.gz

# The release's archive is checked before it is unpacked: its SHA-256 for
# the version the Kconfig names by default; another version needs its own,
# given as LIBSODIUM_SHA256

ifeq ($(LIBSODIUM_VERSION),1.0.18)
LIBSODIUM_SHA256 ?= 6f504490b342a4f8a4c4a02fc9b866cbef8622d5df4e5452b46be121e46636c1
endif

LIBSODIUM_UNPACKNAME = libsodium
UNPACK ?= tar -xaf

LIBSODIUM_UNPACKLIBDIR = $(LIBSODIUM_UNPACKNAME)$(DELIM)src$(DELIM)$(LIBSODIUM_UNPACKNAME)
LIBSODIUM_UNPACKTESTDIR = $(LIBSODIUM_UNPACKNAME)$(DELIM)test$(DELIM)default

CSRCS += $(shell if [ -d "$(LIBSODIUM_UNPACKLIBDIR)" ]; then find "$(LIBSODIUM_UNPACKLIBDIR)" -name "*.c"; fi)
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/crypto/libsodium/libsodium/src/libsodium/include/sodium
CFLAGS += -DDEV_MODE -DCONFIGURED=1
CFLAGS += -Wno-unused-function -Wno-undef -Wno-unused-variable -Wno-deprecated-declarations \
          -Wno-shadow

ifneq ($(CONFIG_LIBSODIUM_TEST),)
MODULE = $(CONFIG_LIBSODIUM_TEST)
MAINSRC = $(wildcard $(LIBSODIUM_UNPACKTESTDIR)$(DELIM)*.c)
PROGNAME = $(patsubst $(LIBSODIUM_UNPACKTESTDIR)$(DELIM)%.c,%,$(MAINSRC))
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/crypto/libsodium/libsodium/test/quirks
PRIORITY = $(CONFIG_LIBSODIUM_TEST_PRIORITY)
STACKSIZE = $(CONFIG_LIBSODIUM_TEST_STACKSIZE)
endif

$(LIBSODIUM_TARGZ):
	@echo "Downloading: $(LIBSODIUM_URL)/$(LIBSODIUM_TARGZ)"
	$(Q) $(call DOWNLOAD,$(LIBSODIUM_URL),$(LIBSODIUM_TARGZ))

$(LIBSODIUM_UNPACKNAME): $(LIBSODIUM_TARGZ)
	$(Q) if [ -z "$(LIBSODIUM_SHA256)" ]; then \
	       echo "libsodium $(LIBSODIUM_VERSION): no SHA-256 to check it" \
	            "against: give LIBSODIUM_SHA256" >&2; \
	       exit 1; \
	     fi
	$(Q) $(APPDIR)/tools/check-hash.sh sha256 $(LIBSODIUM_SHA256) $(LIBSODIUM_TARGZ)
	@echo "Unpacking: $(LIBSODIUM_TARGZ) -> $(LIBSODIUM_UNPACKNAME)"
	$(call DELDIR, $(LIBSODIUM_UNPACKNAME))
	$(call DELDIR, $(LIBSODIUM_UNPACKNAME)-$(LIBSODIUM_VERSION))
	$(Q) $(UNPACK) $(LIBSODIUM_TARGZ)
	$(Q) mv $(LIBSODIUM_UNPACKNAME)-$(LIBSODIUM_VERSION) $(LIBSODIUM_UNPACKNAME)
	$(Q) echo "Patching $(LIBSODIUM_UNPACKNAME)"
	$(Q) patch -p1 -d $(LIBSODIUM_UNPACKNAME) < 0001-fix-multiple-definition-bug-in-libsodium-test.patch
	$(Q) patch -p1 -d $(LIBSODIUM_UNPACKNAME) < 0002-fix-cannot-find-file-sodium-version.h.patch
	$(Q) touch $(LIBSODIUM_UNPACKNAME)

ifeq ($(wildcard $(LIBSODIUM_UNPACKNAME)/.git),)
context:: $(LIBSODIUM_UNPACKNAME)

distclean::
	$(Q) rm -rf $(LIBSODIUM_UNPACKNAME)
endif

include $(APPDIR)/Application.mk
