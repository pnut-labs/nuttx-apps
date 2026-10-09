/****************************************************************************
 * apps/system/nxinit/control.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "control.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The longest command, and the longest line of an answer */

#define CONTROL_LINE_MAX  80

#define CONTROL_CLIENTS   CONFIG_SYSTEM_NXINIT_CONTROL_CLIENTS

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct control_client_s
{
  char in[CONTROL_LINE_MAX];      /* A command arriving */
  size_t len;
  pid_t pid;                      /* The caller, or -1 when not known */
  bool watching;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* NxInit runs once: the listening poller, and one for each connection */

static FAR struct init_poller_s *g_listener;
static FAR struct init_poller_s *g_clients[CONTROL_CLIENTS];
static struct control_client_s g_client_state[CONTROL_CLIENTS];
static int g_nclients;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: control_close
 ****************************************************************************/

static void control_close(FAR struct init_poller_s *ctx)
{
  FAR struct control_client_s *client = ctx->priv;

  if (ctx->pfd->fd >= 0)
    {
      close(ctx->pfd->fd);
      ctx->pfd->fd = -1;
    }

  client->len      = 0;
  client->pid      = -1;
  client->watching = false;
}

/****************************************************************************
 * Name: control_emit
 *
 * Description:
 *   Send a line to a connection.  One that cannot take it at once is too
 *   slow to keep: NxInit never waits on a client.
 *
 ****************************************************************************/

static int control_emit(FAR void *arg, FAR const char *line)
{
  FAR struct init_poller_s *ctx = arg;
  char buf[CONTROL_LINE_MAX + 1];
  ssize_t n;
  int len;

  /* Closed by an earlier line of the same answer */

  if (ctx->pfd->fd < 0)
    {
      return -EBADF;
    }

  len = snprintf(buf, sizeof(buf), "%s\n", line);
  if (len < 0 || len >= (int)sizeof(buf))
    {
      return -E2BIG;
    }

  /* The connection was accepted non-blocking, which is what keeps this
   * from waiting: NuttX's local stream sockets ignore MSG_DONTWAIT
   */

  n = send(ctx->pfd->fd, buf, len, MSG_DONTWAIT);
  if (n != len)
    {
      init_warn("Control client too slow: closed");
      control_close(ctx);
      return -EAGAIN;
    }

  return 0;
}

/****************************************************************************
 * Name: control_error
 ****************************************************************************/

static int control_error(init_control_emit_t emit, FAR void *arg,
                         int error, FAR const char *text)
{
  char line[CONTROL_LINE_MAX];

  snprintf(line, sizeof(line), "error %d %s", error, text);
  return emit(arg, line);
}

/****************************************************************************
 * Name: control_state_line
 ****************************************************************************/

static void control_state_line(FAR struct service_s *service,
                               FAR const char *prefix, FAR char *line)
{
  snprintf(line, CONTROL_LINE_MAX, "%s%s %s %d", prefix, service->argv[1],
           init_service_state(service),
           (service->flags & SVC_RUNNING) != 0 ? (int)service->pid : 0);
}

/****************************************************************************
 * Name: control_accept
 *
 * Description:
 *   A connection is waiting: give it a free poller, or turn it away.
 *
 ****************************************************************************/

static void control_accept(FAR struct init_poller_s *ctx)
{
  char line[CONTROL_LINE_MAX];
  int fd;
  int i;

  fd = accept4(ctx->pfd->fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (fd < 0)
    {
      if (errno != EAGAIN && errno != EINTR)
        {
          init_err("Control accept %d", errno);
        }

      return;
    }

  for (i = 0; i < g_nclients; i++)
    {
      if (g_clients[i]->pfd->fd < 0)
        {
#ifdef CONFIG_NET_LOCAL_SCM
          FAR struct control_client_s *client = g_clients[i]->priv;
          struct ucred cred;
          socklen_t len = sizeof(cred);

          if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0)
            {
              client->pid = cred.pid;
            }
#endif

          g_clients[i]->pfd->fd = fd;
          snprintf(line, sizeof(line), "nxinit %d", NXINIT_CONTROL_VERSION);
          control_emit(g_clients[i], line);
          return;
        }
    }

  /* Told why, as far as it takes it at once, then closed */

  init_warn("Control: as many clients as it takes");
  snprintf(line, sizeof(line), "error %d too many clients\n", EBUSY);
  send(fd, line, strlen(line), MSG_DONTWAIT);
  close(fd);
}

/****************************************************************************
 * Name: control_read
 *
 * Description:
 *   Read what a connection sent, and run every whole command.
 *
 ****************************************************************************/

static void control_read(FAR struct init_poller_s *ctx)
{
  FAR struct control_client_s *client = ctx->priv;
  FAR char *nl;
  ssize_t n;
  bool watch;

  n = recv(ctx->pfd->fd, client->in + client->len,
           sizeof(client->in) - client->len, MSG_DONTWAIT);
  if (n <= 0)
    {
      if (n == 0 || (errno != EAGAIN && errno != EINTR))
        {
          control_close(ctx);
        }

      return;
    }

  client->len += n;

  while (ctx->pfd->fd >= 0 &&
         (nl = memchr(client->in, '\n', client->len)) != NULL)
    {
      *nl = '\0';
      if (nl > client->in && *(nl - 1) == '\r')
        {
          *(nl - 1) = '\0';
        }

      watch = false;
      init_control_execute(ctx->sm, client->pid, client->in, control_emit,
                           ctx, &watch);
      if (ctx->pfd->fd < 0)
        {
          return;
        }

      client->watching |= watch;

      client->len -= nl + 1 - client->in;
      memmove(client->in, nl + 1, client->len);
    }

  /* A command longer than a line can be is not one */

  if (ctx->pfd->fd >= 0 && client->len == sizeof(client->in))
    {
      control_error(control_emit, ctx, E2BIG, "line too long");
      control_close(ctx);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int init_control_init(FAR struct init_poller_s *ctx)
{
  struct sockaddr_un addr;
  int fd;

  ctx->pfd->events  = POLLIN;
  ctx->pfd->revents = 0;
  ctx->pfd->fd      = -1;

  /* The pollers after the first carry the connections */

  if (g_listener != NULL)
    {
      DEBUGASSERT(g_nclients < CONTROL_CLIENTS);
      ctx->priv               = &g_client_state[g_nclients];
      g_clients[g_nclients++] = ctx;
      control_close(ctx);
      return 0;
    }

  /* The first poller listens.  When the socket cannot be set up, NxInit
   * goes on without it: neither this poller nor the connections' pollers
   * get a descriptor, and poll() passes them over
   */

  g_listener = ctx;

  fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0)
    {
      init_err("Control socket %d", errno);
      return 0;
    }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strlcpy(addr.sun_path, CONFIG_SYSTEM_NXINIT_CONTROL_PATH,
          sizeof(addr.sun_path));
  unlink(addr.sun_path);

  if (bind(fd, (FAR struct sockaddr *)&addr, sizeof(addr)) < 0 ||
      listen(fd, CONTROL_CLIENTS) < 0)
    {
      init_err("Control socket %s: %d", addr.sun_path, errno);
      close(fd);
      return 0;
    }

  ctx->pfd->fd = fd;
  return 0;
}

void init_control_handle(FAR struct init_poller_s *ctx)
{
  if (ctx == g_listener)
    {
      control_accept(ctx);
    }
  else if ((ctx->pfd->revents & (POLLHUP | POLLERR)) != 0 &&
           (ctx->pfd->revents & POLLIN) == 0)
    {
      control_close(ctx);
    }
  else
    {
      control_read(ctx);
    }
}

void init_control_deinit(FAR struct init_poller_s *ctx)
{
  if (ctx == g_listener)
    {
      if (ctx->pfd->fd >= 0)
        {
          unlink(CONFIG_SYSTEM_NXINIT_CONTROL_PATH);
        }

      g_listener = NULL;
    }

  if (ctx->pfd->fd >= 0)
    {
      close(ctx->pfd->fd);
      ctx->pfd->fd = -1;
    }
}

void init_control_changed(FAR struct service_s *service)
{
  char line[CONTROL_LINE_MAX];
  int i;

  control_state_line(service, "event ", line);

  for (i = 0; i < g_nclients; i++)
    {
      FAR struct control_client_s *client = g_clients[i]->priv;

      if (g_clients[i]->pfd->fd >= 0 && client->watching)
        {
          control_emit(g_clients[i], line);
        }
    }
}

int init_control_execute(FAR struct service_manager_s *sm, pid_t caller,
                         FAR char *line, init_control_emit_t emit,
                         FAR void *arg, FAR bool *watchp)
{
  FAR struct service_s *service;
  char answer[CONTROL_LINE_MAX];
  FAR char *argv[3];
  FAR char *end;
  long pid;
  int argc;
  int ret;
  int n;

  *watchp = false;

  argc = init_parse_arguments(line, false, nitems(argv), argv);
  if (argc <= 0)
    {
      return control_error(emit, arg, EINVAL, "no command");
    }

  if (strcmp(argv[0], "state") == 0 && argc == 1)
    {
      n = 0;
      list_for_every_entry(&sm->services, service, struct service_s, node)
        {
          n++;
        }

      snprintf(answer, sizeof(answer), "ok %d", n);
      ret = emit(arg, answer);
      list_for_every_entry(&sm->services, service, struct service_s, node)
        {
          if (ret < 0)
            {
              break;
            }

          control_state_line(service, "", answer);
          ret = emit(arg, answer);
        }

      return ret;
    }

  if (strcmp(argv[0], "watch") == 0 && argc == 1)
    {
      *watchp = true;
      return emit(arg, "ok");
    }

  /* Only a service's own task says that it is ready */

  if (strcmp(argv[0], "ready") == 0 && argc == 1)
    {
      if (caller <= 0)
        {
          return control_error(emit, arg, EPERM, "caller unknown");
        }

      service = init_service_find_by_pid(sm, caller);
      ret = service != NULL ? init_service_ready(service) : -ESRCH;
      if (ret == -EINVAL)
        {
          return control_error(emit, arg, EINVAL, "not a notify service");
        }
      else if (ret < 0)
        {
          return control_error(emit, arg, ESRCH, "not a running service");
        }

      return emit(arg, "ok");
    }

  if (argc != 2 ||
      (strcmp(argv[0], "who") != 0 && strcmp(argv[0], "state") != 0 &&
       strcmp(argv[0], "start") != 0 && strcmp(argv[0], "stop") != 0))
    {
      return control_error(emit, arg, EINVAL, "unknown command");
    }

  if (strcmp(argv[0], "who") == 0)
    {
      pid = strtol(argv[1], &end, 10);
      if (*end != '\0' || pid <= 0)
        {
          return control_error(emit, arg, EINVAL, "not a task");
        }

      service = init_service_find_by_pid(sm, (pid_t)pid);
      if (service == NULL || (service->flags & SVC_RUNNING) == 0)
        {
          return control_error(emit, arg, ESRCH, "no such task");
        }

      snprintf(answer, sizeof(answer), "ok %s", service->argv[1]);
      return emit(arg, answer);
    }

  service = init_service_find_by_name(sm, argv[1]);
  if (service == NULL)
    {
      return control_error(emit, arg, ENOENT, "no such service");
    }

  if (strcmp(argv[0], "state") == 0)
    {
      control_state_line(service, "ok ", answer);
      return emit(arg, answer);
    }

  if (strcmp(argv[0], "start") == 0)
    {
      ret = init_service_start(service);
    }
  else
    {
      ret = init_service_stop(service);
    }

  if (ret < 0)
    {
      return control_error(emit, arg, -ret, strerror(-ret));
    }

  return emit(arg, "ok");
}
