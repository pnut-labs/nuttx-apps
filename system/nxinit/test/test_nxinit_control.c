/****************************************************************************
 * apps/system/nxinit/test/test_nxinit_control.c
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
#include <nuttx/list.h>

#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include "../control.h"
#include "../parser.h"
#include "../service.h"
#include "test_nxinit.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Two services, "a" running as task 7 and "b" stopped, and the answer to
 * a command
 */

struct control_fixture_s
{
  struct service_manager_s sm;
  char out[256];
  size_t len;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int control_capture(FAR void *arg, FAR const char *line)
{
  FAR struct control_fixture_s *f = arg;

  f->len += snprintf(f->out + f->len, sizeof(f->out) - f->len, "%s\n",
                     line);
  return 0;
}

static void control_setup(FAR struct control_fixture_s *f)
{
  struct parser_s parser =
    {
      "service", init_service_parse, init_service_check, &f->sm
    };

  char decl_a[] = "service a /bin/a";
  char decl_b[] = "service b /bin/b";
  FAR struct service_s *a;

  memset(f, 0, sizeof(*f));
  list_initialize(&f->sm.services);

  assert_int_equal(init_service_parse(&parser, true, decl_a), 0);
  assert_int_equal(init_service_parse(&parser, true, decl_b), 0);
  assert_int_equal(init_service_check(&parser), 0);

  a = init_service_find_by_name(&f->sm, "a");
  assert_non_null(a);
  a->flags |= SVC_RUNNING;
  a->pid    = 7;
}

static void control_teardown(FAR struct control_fixture_s *f)
{
  FAR struct service_s *s;
  FAR struct service_s *tmp;
  int i;

  list_for_every_entry_safe(&f->sm.services, s, tmp, struct service_s,
                            node)
    {
      for (i = 0; i < s->argc; i++)
        {
          free(s->argv[i]);
        }

      free(s->console);
      list_delete(&s->node);
      free(s);
    }
}

/* Run a command; its answer is in f->out */

static bool control_run(FAR struct control_fixture_s *f,
                        FAR const char *command)
{
  char line[64];
  bool watch;

  strlcpy(line, command, sizeof(line));
  f->len    = 0;
  f->out[0] = '\0';
  assert_int_equal(init_control_execute(&f->sm, line, control_capture, f,
                                        &watch), 0);
  return watch;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_nxinit_control_state
 *
 * Description:
 *   "state" lists every service with its state and task; "state <name>"
 *   one; an unknown name is an error.
 ****************************************************************************/

void test_nxinit_control_state(FAR void **state)
{
  struct control_fixture_s f;

  control_setup(&f);

  control_run(&f, "state");
  assert_string_equal(f.out, "ok 2\na ready 7\nb stopped 0\n");

  control_run(&f, "state b");
  assert_string_equal(f.out, "ok b stopped 0\n");

  control_run(&f, "state c");
  assert_string_equal(f.out, "error 2 no such service\n");

  control_teardown(&f);
}

/****************************************************************************
 * Name: test_nxinit_control_who
 *
 * Description:
 *   "who <pid>" names the service whose task that is; a task that is no
 *   service's is an error.
 ****************************************************************************/

void test_nxinit_control_who(FAR void **state)
{
  struct control_fixture_s f;

  control_setup(&f);

  control_run(&f, "who 7");
  assert_string_equal(f.out, "ok a\n");

  control_run(&f, "who 99");
  assert_string_equal(f.out, "error 3 no such task\n");

  control_run(&f, "who 7x");
  assert_string_equal(f.out, "error 22 not a task\n");

  control_teardown(&f);
}

/****************************************************************************
 * Name: test_nxinit_control_commands
 *
 * Description:
 *   "watch" asks for the changes; a service that is not running cannot be
 *   stopped; what is not a command is an error.
 ****************************************************************************/

void test_nxinit_control_commands(FAR void **state)
{
  struct control_fixture_s f;

  control_setup(&f);

  assert_true(control_run(&f, "watch"));
  assert_string_equal(f.out, "ok\n");

  assert_false(control_run(&f, "stop b"));
  assert_int_equal(strncmp(f.out, "error 3 ", 8), 0);

  control_run(&f, "reboot now");
  assert_string_equal(f.out, "error 22 unknown command\n");

  control_run(&f, "");
  assert_string_equal(f.out, "error 22 no command\n");

  control_teardown(&f);
}
