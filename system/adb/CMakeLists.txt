# ##############################################################################
# apps/system/adb/CMakeLists.txt
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

if(CONFIG_SYSTEM_ADBD)

  # One commit, never master, and its archive checked: another version
  # comes with its own hash
  if(NOT ADBD_VERSION)
    set(ADBD_VERSION "85cc7a90101b2ae76ee4112ba285141c8fba5012")
  endif()

  # A tree fetched before, of another version or with other patches, is
  # never used: it is removed by hand.  The stamp names the version and the
  # patches' hash.
  file(READ ${CMAKE_CURRENT_LIST_DIR}/0001-microadb-check-key-signatures.patch
       ADBD_PATCH1)
  file(READ ${CMAKE_CURRENT_LIST_DIR}/0002-microadb-stop-on-sigterm.patch
       ADBD_PATCH2)
  string(SHA256 ADBD_PATCHES_HASH "${ADBD_PATCH1}${ADBD_PATCH2}")
  string(SUBSTRING ${ADBD_PATCHES_HASH} 0 12 ADBD_PATCHES_HASH)
  set(ADBD_STAMP
      ${CMAKE_CURRENT_LIST_DIR}/microADB/.pinned-${ADBD_VERSION}-${ADBD_PATCHES_HASH}
  )
  if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/microADB AND NOT EXISTS ${ADBD_STAMP})
    message(
      FATAL_ERROR "system/adb/microADB is not the pinned version with these "
                  "patches: remove it to fetch ${ADBD_VERSION}")
  endif()

  if(NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/microADB)

    if(NOT ADBD_URL)
      set(ADBD_URL "https://github.com/spiriou/microADB/archive")
    endif()

    if(NOT ADBD_SHA256)
      set(ADBD_SHA256
          "25a52167d72eb181d0991e5b3ff866c67d8a026f5ba0fdda1d9b14ebd1d04b61")
    endif()

    FetchContent_Declare(
      microADB
      URL "${ADBD_URL}/${ADBD_VERSION}.zip" URL_HASH SHA256=${ADBD_SHA256}
          SOURCE_DIR
          ${CMAKE_CURRENT_LIST_DIR}/microADB BINARY_DIR
          ${NUTTX_BINARY_DIR}/apps/system/microADB/microADB
      DOWNLOAD_NO_PROGRESS true
      TIMEOUT 30
      PATCH_COMMAND
        patch -p1 -i
        ${CMAKE_CURRENT_LIST_DIR}/0001-microadb-check-key-signatures.patch &&
        patch -p1 -i
        ${CMAKE_CURRENT_LIST_DIR}/0002-microadb-stop-on-sigterm.patch)

    FetchContent_GetProperties(microADB)

    if(NOT microADB_POPULATED)
      FetchContent_Populate(microADB)
      file(TOUCH ${ADBD_STAMP})
    endif()
  endif()

  set(CSRCS adb_main.c adb_banner.c microADB/adb_client.c microADB/adb_frame.c
            microADB/hal/hal_uv.c microADB/hal/hal_uv_packet.c)

  if(CONFIG_ADBD_TCP_SERVER)
    list(APPEND CSRCS microADB/hal/hal_uv_client_tcp.c)
  endif()

  if(CONFIG_ADBD_USB_SERVER)
    list(APPEND CSRCS microADB/hal/hal_uv_client_usb.c)
  endif()

  if(CONFIG_ADBD_QEMU_SERVER)
    list(APPEND CSRCS microADB/hal/hal_uv_client_qemu.c)
  endif()

  if(CONFIG_ADBD_AUTHENTICATION)
    list(APPEND CSRCS adb_auth.c)
  endif()

  if(CONFIG_ADBD_FILE_SERVICE)
    list(APPEND CSRCS microADB/file_sync_service.c)
  endif()

  if(CONFIG_ADBD_LOGCAT_SERVICE)
    list(APPEND CSRCS logcat_service.c)
  endif()

  if(CONFIG_ADBD_SHELL_SERVICE)
    list(APPEND CSRCS microADB/hal/shell_service_uv.c)
  endif()

  if(CONFIG_ADBD_SOCKET_SERVICE)
    list(APPEND CSRCS microADB/tcp_service.c microADB/hal/hal_uv_socket.c)
  endif()

  nuttx_add_application(
    MODULE
    ${CONFIG_SYSTEM_ADBD}
    NAME
    ${CONFIG_ADBD_PROGNAME}
    SRCS
    ${CSRCS}
    INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_LIST_DIR}/microADB
    STACKSIZE
    ${CONFIG_ADBD_STACKSIZE}
    PRIORITY
    ${CONFIG_ADBD_PRIORITY})

endif()
