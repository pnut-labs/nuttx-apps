# ##############################################################################
# apps/crypto/libsodium/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

if(CONFIG_LIBSODIUM)

  # ############################################################################
  # Config and Fetch libsodium lib
  # ############################################################################
  set(LIBSODIUM_DIR ${CMAKE_CURRENT_LIST_DIR}/libsodium)

  if(NOT EXISTS ${LIBSODIUM_DIR})
    set(LIBSODIUM_URL https://github.com/jedisct1/libsodium/archive)

    # The tag's archive is checked: its SHA-256 for the version the Kconfig
    # names by default; another version needs its own, given as
    # LIBSODIUM_ZIP_SHA256

    if(NOT LIBSODIUM_ZIP_SHA256 AND CONFIG_LIBSODIUM_VERSION STREQUAL "1.0.18")
      set(LIBSODIUM_ZIP_SHA256
          e530b577c233346a784bf8a141b36ee8a4437bc716022024f94c492c7f0860ab)
    endif()
    if(NOT LIBSODIUM_ZIP_SHA256)
      message(
        FATAL_ERROR
          "libsodium ${CONFIG_LIBSODIUM_VERSION}: no SHA-256 to check it against: give LIBSODIUM_ZIP_SHA256"
      )
    endif()

    FetchContent_Declare(
      libsodium_fetch
      URL ${LIBSODIUM_URL}/${CONFIG_LIBSODIUM_VERSION}.zip
      URL_HASH SHA256=${LIBSODIUM_ZIP_SHA256}
      SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/libsodium BINARY_DIR
      ${NUTTX_BINARY_DIR}/apps/crypto/libsodium/libsodium
      PATCH_COMMAND
        patch -p1 -d ${CMAKE_CURRENT_LIST_DIR}/libsodium <
        ${CMAKE_CURRENT_LIST_DIR}/0001-fix-multiple-definition-bug-in-libsodium-test.patch
        && patch -p1 -d ${CMAKE_CURRENT_LIST_DIR}/libsodium <
        ${CMAKE_CURRENT_LIST_DIR}/0002-fix-cannot-find-file-sodium-version.h.patch
      DOWNLOAD_NO_PROGRESS true
      TIMEOUT 30)

    FetchContent_GetProperties(libsodium_fetch)

    if(NOT libsodium_fetch_POPULATED)
      FetchContent_Populate(libsodium_fetch)
    endif()
  endif()

  # ############################################################################
  # Flags
  # ############################################################################

  set(CFLAGS
      -DDEV_MODE
      -DCONFIGURED=1
      -Wno-unused-function
      -Wno-undef
      -Wno-unused-variable
      -Wno-deprecated-declarations
      -Wno-shadow)

  # ############################################################################
  # Sources
  # ############################################################################

  file(GLOB_RECURSE CSRCS ${LIBSODIUM_DIR}/src/libsodium/*.c)

  # ############################################################################
  # Include Directory
  # ############################################################################

  set(INCDIR ${LIBSODIUM_DIR}/src/libsodium/include
             ${LIBSODIUM_DIR}/src/libsodium/include/sodium)

  # ############################################################################
  # Library Configuration
  # ############################################################################

  nuttx_add_library(libsodium STATIC)

  target_sources(libsodium PRIVATE ${CSRCS})
  target_include_directories(libsodium PRIVATE ${INCDIR})
  target_compile_options(libsodium PRIVATE ${CFLAGS})

  # ############################################################################
  # Applications Configuration
  # ############################################################################

  if(CONFIG_LIBSODIUM_TEST)
    file(GLOB TEST_CSRCS ${LIBSODIUM_DIR}/test/default/*.c)
    list(APPEND INCDIR ${LIBSODIUM_DIR}/test/quirks)

    foreach(TEST_CSRC ${TEST_CSRCS})
      get_filename_component(TEST_PROG ${TEST_CSRC} NAME_WLE)
      nuttx_add_application(
        MODULE
        ${CONFIG_LIBSODIUM_TEST}
        NAME
        ${TEST_PROG}
        STACKSIZE
        ${CONFIG_LIBSODIUM_TEST_STACKSIZE}
        PRIORITY
        ${CONFIG_LIBSODIUM_TEST_PRIORITY}
        SRCS
        ${TEST_CSRC}
        COMPILE_FLAGS
        ${CFLAGS}
        INCLUDE_DIRECTORIES
        ${INCDIR}
        DEPENDS
        libsodium)
    endforeach()
  endif()
endif()
