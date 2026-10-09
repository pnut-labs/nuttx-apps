/****************************************************************************
 * apps/system/nxinit/svc.c
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

/* svc: NxInit's services from the shell, through its control socket
 *
 *   svc state [<service>]
 *   svc start <service>
 *   svc stop <service>
 *   svc who <pid>
 *   svc watch
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SVC_LINE_MAX  80

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct svc_conn_s
{
  int fd;
  char buf[SVC_LINE_MAX];
  size_t len;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: svc_line
 *
 * Description:
 *   The next line NxInit sent, without its newline.
 *
 ****************************************************************************/

static int svc_line(FAR struct svc_conn_s *conn, FAR char *line,
                    size_t size)
{
  FAR char *nl;
  ssize_t n;

  while ((nl = memchr(conn->buf, '\n', conn->len)) == NULL)
    {
      if (conn->len == sizeof(conn->buf))
        {
          return -E2BIG;
        }

      n = read(conn->fd, conn->buf + conn->len,
               sizeof(conn->buf) - conn->len);
      if (n <= 0)
        {
          return n == 0 ? -ENOTCONN : -errno;
        }

      conn->len += n;
    }

  *nl = '\0';
  strlcpy(line, conn->buf, size);
  conn->len -= nl + 1 - conn->buf;
  memmove(conn->buf, nl + 1, conn->len);
  return 0;
}

static void svc_usage(void)
{
  fprintf(stderr,
          "Usage: svc state [<service>]\n"
          "       svc start|stop <service>\n"
          "       svc who <pid>\n"
          "       svc watch\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct sockaddr_un addr;
  struct svc_conn_s conn;
  char line[SVC_LINE_MAX];
  char cmd[SVC_LINE_MAX];
  bool answered = false;
  bool watch;
  int count = 0;
  int ret;
  int i;

  line[0] = '\0';

  if (argc < 2 || argc > 3)
    {
      svc_usage();
      return EXIT_FAILURE;
    }

  watch = strcmp(argv[1], "watch") == 0;
  if (watch && argc != 2)
    {
      svc_usage();
      return EXIT_FAILURE;
    }

  snprintf(cmd, sizeof(cmd), "%s%s%s\n", argv[1], argc == 3 ? " " : "",
           argc == 3 ? argv[2] : "");

  memset(&conn, 0, sizeof(conn));
  conn.fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (conn.fd < 0)
    {
      fprintf(stderr, "svc: socket: %d\n", errno);
      return EXIT_FAILURE;
    }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strlcpy(addr.sun_path, CONFIG_SYSTEM_NXINIT_CONTROL_PATH,
          sizeof(addr.sun_path));

  if (connect(conn.fd, (FAR struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      fprintf(stderr, "svc: %s: %d\n", addr.sun_path, errno);
      close(conn.fd);
      return EXIT_FAILURE;
    }

  /* The version line, or why NxInit turns the connection away; then the
   * command
   */

  ret = svc_line(&conn, line, sizeof(line));
  if (ret >= 0 && strncmp(line, "nxinit ", 7) != 0)
    {
      fprintf(stderr, "svc: %s\n", line);
      close(conn.fd);
      return EXIT_FAILURE;
    }

  if (ret >= 0 && write(conn.fd, cmd, strlen(cmd)) != (ssize_t)strlen(cmd))
    {
      ret = -errno;
    }

  /* The answer: "state" alone says how many lines follow; "watch" goes on
   * until the connection or the command is broken off
   */

  if (ret >= 0)
    {
      ret = svc_line(&conn, line, sizeof(line));
    }

  if (ret >= 0)
    {
      printf("%s\n", line);
      answered = strcmp(line, "ok") == 0 || strncmp(line, "ok ", 3) == 0;
      if (strcmp(argv[1], "state") == 0 && argc == 2 && answered)
        {
          count = atoi(line + 3);
        }

      for (i = 0; ret >= 0 && answered && (watch || i < count); i++)
        {
          ret = svc_line(&conn, line, sizeof(line));
          if (ret >= 0)
            {
              printf("%s\n", line);
              fflush(stdout);
            }
        }
    }

  close(conn.fd);

  /* A watch ends when NxInit closes the connection */

  if (ret == -ENOTCONN && !(watch && answered))
    {
      fprintf(stderr, "svc: NxInit closed the connection\n");
      return EXIT_FAILURE;
    }

  if (ret < 0 && ret != -ENOTCONN)
    {
      fprintf(stderr, "svc: %d\n", ret);
      return EXIT_FAILURE;
    }

  return answered ? EXIT_SUCCESS : EXIT_FAILURE;
}
