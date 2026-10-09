/****************************************************************************
 * apps/system/nxinit/control.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __APPS_SYSTEM_NXINIT_CONTROL_H
#define __APPS_SYSTEM_NXINIT_CONTROL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>

#include "init.h"
#include "parser.h"
#include "service.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The control socket (CONFIG_SYSTEM_NXINIT_CONTROL): a local stream socket
 * speaking a text protocol.  A connection first receives the version line,
 * "nxinit 1"; then every line it sends is a command, and gets one answer:
 *
 *   state            ok <n>, then n lines: <service> <state> <pid>
 *   state <service>  ok <service> <state> <pid>
 *   watch            ok; then a line for every change of state, as long as
 *                    the connection is open: event <service> <state> <pid>
 *   who <pid>        ok <service>: the service whose task that is
 *   start <service>  ok, also when it runs already
 *   stop <service>   ok
 *
 * A command that fails is answered "error <errno> <text>".  A state is
 * ready, restarting, stopping or stopped; the pid is 0 when the service
 * is not running.
 *
 * On a connection that watches, an event line may come at any time, also
 * between a command and its answer (a "stop" sends "stopping" first); the
 * n lines after "ok <n>" come together.  Lines end in a newline, which may
 * come after a carriage return, and are shorter than 80 characters: a
 * longer command is answered "error ... line too long", and the connection
 * closed.  NxInit never waits on a client: one that does not read what it
 * is sent is closed, and one more than CONFIG_SYSTEM_NXINIT_CONTROL_CLIENTS
 * is answered "error ... too many clients" in place of the version line,
 * and closed.
 */

#define NXINIT_CONTROL_VERSION   1

#ifdef CONFIG_SYSTEM_NXINIT_CONTROL
#  define NXINIT_CONTROL_POLLERS (1 + CONFIG_SYSTEM_NXINIT_CONTROL_CLIENTS)
#else
#  define NXINIT_CONTROL_POLLERS 0
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Where a command's answer goes, line by line, without the newline */

typedef CODE int (*init_control_emit_t)(FAR void *arg,
                                        FAR const char *line);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_SYSTEM_NXINIT_CONTROL

/****************************************************************************
 * Name: init_control_init, init_control_handle, init_control_deinit
 *
 * Description:
 *   The control socket's pollers: the first one initialized listens, the
 *   others carry a connection each.
 *
 ****************************************************************************/

int  init_control_init(FAR struct init_poller_s *ctx);
void init_control_handle(FAR struct init_poller_s *ctx);
void init_control_deinit(FAR struct init_poller_s *ctx);

/****************************************************************************
 * Name: init_control_changed
 *
 * Description:
 *   A service's state has changed: tell those who watch.
 *
 ****************************************************************************/

void init_control_changed(FAR struct service_s *service);

/****************************************************************************
 * Name: init_control_execute
 *
 * Description:
 *   Run one command and emit its answer.
 *
 * Input Parameters:
 *   sm     - The services.
 *   line   - The command, without its newline; changed.
 *   emit   - Called with each line of the answer.
 *   arg    - Passed to emit.
 *   watchp - Set when the command asks to watch.
 *
 * Returned Value:
 *   Zero (OK) when the answer was emitted, whatever it says; emit's
 *   failure otherwise.
 *
 ****************************************************************************/

int init_control_execute(FAR struct service_manager_s *sm, FAR char *line,
                         init_control_emit_t emit, FAR void *arg,
                         FAR bool *watchp);

#else
#  define init_control_changed(service)
#endif

#endif /* __APPS_SYSTEM_NXINIT_CONTROL_H */
