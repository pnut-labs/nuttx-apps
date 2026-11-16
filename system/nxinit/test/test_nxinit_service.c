/****************************************************************************
 * apps/system/nxinit/test/test_nxinit_service.c
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
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <cmocka.h>

#include "../parser.h"
#include "../service.h"
#include "test_nxinit.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void service_manager_init(FAR struct service_manager_s *sm)
{
  list_initialize(&sm->services);
}

/* init_service_parse()/init_service_check() free failed/duplicate
 * services internally, but successfully-parsed services in "sm" are
 * left for the caller to release. This mirrors what init_service_refresh()
 * would eventually do for SVC_REMOVE-flagged entries, kept minimal here
 * since these tests never start any service (no pid, no posix_spawn).
 */

static void service_manager_free_all(FAR struct service_manager_s *sm)
{
  FAR struct service_s *s;
  FAR struct service_s *tmp;
  FAR struct service_class_s *c;
  FAR struct service_class_s *ctmp;
  int i;

  list_for_every_entry_safe(&sm->services, s, tmp, struct service_s, node)
    {
      list_for_every_entry_safe(&s->classes, c, ctmp,
                                struct service_class_s, node)
        {
          list_delete(&c->node);
          free(c);
        }

      for (i = 0; i < s->argc; i++)
        {
          free(s->argv[i]);
        }

      free(s->console);
      list_delete(&s->node);
      free(s);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_nxinit_service_duplicate_conflict
 *
 * Description:
 *   Two "service" sections sharing the same name, with no "override"
 *   option, are rejected by init_service_check() with -EEXIST.
 ****************************************************************************/

void test_nxinit_service_duplicate_conflict(FAR void **state)
{
  struct service_manager_s sm;
  struct parser_s parser =
    {
      "service", init_service_parse, init_service_check, &sm
    };

  char decl1[] = "service foo /bin/foo";
  char decl2[] = "service foo /bin/bar";

  service_manager_init(&sm);

  assert_int_equal(init_service_parse(&parser, true, decl1), 0);
  assert_int_equal(init_service_parse(&parser, true, decl2), 0);

  assert_int_equal(init_service_check(&parser), -EEXIST);

  service_manager_free_all(&sm);
}

/****************************************************************************
 * Name: test_nxinit_service_override_replaces_duplicate
 *
 * Description:
 *   When the later "service" definition carries the "override" option,
 *   init_service_check() disables/removes the earlier one instead of
 *   failing.
 ****************************************************************************/

void test_nxinit_service_override_replaces_duplicate(FAR void **state)
{
  struct service_manager_s sm;
  struct parser_s parser =
    {
      "service", init_service_parse, init_service_check, &sm
    };

  char decl1[] = "service foo /bin/foo";
  char decl2[] = "service foo /bin/bar";
  char opt_override[] = "  override";
  FAR struct service_s *s1;
  FAR struct service_s *s2;

  service_manager_init(&sm);

  assert_int_equal(init_service_parse(&parser, true, decl1), 0);
  assert_int_equal(init_service_parse(&parser, true, decl2), 0);
  assert_int_equal(init_service_parse(&parser, false, opt_override), 0);

  assert_int_equal(init_service_check(&parser), 0);

  s1 = list_first_entry(&sm.services, struct service_s, node);
  s2 = list_last_entry(&sm.services, struct service_s, node);

  /* SVC_REMOVE sets bit 31, so the raw "int" OR of the two flags is
   * negative; cast explicitly to uint32_t before comparing so
   * assert_int_equal()'s intmax_t widening does not sign-extend one
   * side and zero-extend the other.
   */

  assert_int_equal(s1->flags & (SVC_DISABLED | SVC_REMOVE),
                   (uint32_t)(SVC_DISABLED | SVC_REMOVE));
  assert_int_equal(s2->flags & (SVC_DISABLED | SVC_REMOVE), 0);

  service_manager_free_all(&sm);
}

/****************************************************************************
 * Name: test_nxinit_service_args_max_boundary
 *
 * Description:
 *   A "service" declaration with exactly
 *   CONFIG_SYSTEM_NXINIT_SERVICE_ARGS_MAX tokens is fully captured, while
 *   one more token than the limit is folded into the last slot instead
 *   of overflowing argv.
 ****************************************************************************/

void test_nxinit_service_args_max_boundary(FAR void **state)
{
  /* "service" + "<name>" + "<pathname>" already consume 3 of the
   * CONFIG_SYSTEM_NXINIT_SERVICE_ARGS_MAX tokens, leaving this many
   * "argN" tokens to reach the limit exactly. At the Kconfig lower
   * bound of 3 this is 0, i.e. "service <name> <pathname>" already
   * fills argv on its own with no room for even a single "argN"
   * token, so there is nothing meaningful left to exercise here.
   */

#define NARGS_AT_LIMIT (CONFIG_SYSTEM_NXINIT_SERVICE_ARGS_MAX - 3)

#if NARGS_AT_LIMIT < 1
  skip();
#else
  struct service_manager_s sm;
  struct parser_s parser =
    {
      "service", init_service_parse, init_service_check, &sm
    };

  char at_limit[32 + 8 * NARGS_AT_LIMIT] = "service foo1 /bin/foo";
  char over_limit[32 + 8 * (NARGS_AT_LIMIT + 1)] = "service foo2 /bin/foo";
  char last_arg[16];
  char last_two_args[32];
  FAR struct service_s *s;
  int i;

  for (i = 1; i <= NARGS_AT_LIMIT + 1; i++)
    {
      if (i <= NARGS_AT_LIMIT)
        {
          snprintf(at_limit + strlen(at_limit),
                  sizeof(at_limit) - strlen(at_limit), " arg%d", i);
        }

      snprintf(over_limit + strlen(over_limit),
              sizeof(over_limit) - strlen(over_limit), " arg%d", i);
    }

  snprintf(last_arg, sizeof(last_arg), "arg%d", NARGS_AT_LIMIT);
  snprintf(last_two_args, sizeof(last_two_args), "arg%d arg%d",
          NARGS_AT_LIMIT, NARGS_AT_LIMIT + 1);

  service_manager_init(&sm);

  /* A declaration with exactly CONFIG_SYSTEM_NXINIT_SERVICE_ARGS_MAX
   * tokens is fully captured.
   */

  assert_int_equal(init_service_parse(&parser, true, at_limit), 0);

  s = list_last_entry(&sm.services, struct service_s, node);
  assert_int_equal(s->argc, CONFIG_SYSTEM_NXINIT_SERVICE_ARGS_MAX);
  assert_string_equal(s->argv[CONFIG_SYSTEM_NXINIT_SERVICE_ARGS_MAX - 1],
                      last_arg);

  /* One token beyond the limit is folded into the last argv slot
   * together with the remaining, still-unsplit text, instead of
   * overflowing argv.
   */

  assert_int_equal(init_service_parse(&parser, true, over_limit), 0);

  s = list_last_entry(&sm.services, struct service_s, node);
  assert_int_equal(s->argc, CONFIG_SYSTEM_NXINIT_SERVICE_ARGS_MAX);
  assert_string_equal(s->argv[CONFIG_SYSTEM_NXINIT_SERVICE_ARGS_MAX - 1],
                      last_two_args);

  service_manager_free_all(&sm);
#endif

#undef NARGS_AT_LIMIT
}

/****************************************************************************
 * Name: test_nxinit_service_console_option
 *
 * Description:
 *   The "console" option flags the service with SVC_CONSOLE.  Without an
 *   argument the default console device is used (console == NULL); with an
 *   argument the device name is duplicated into the service, since the
 *   parser reuses its line buffer for the next line.
 ****************************************************************************/

void test_nxinit_service_console_option(FAR void **state)
{
  struct service_manager_s sm;
  struct parser_s parser =
    {
      "service", init_service_parse, init_service_check, &sm
    };

  char decl1[] = "service console1 /bin/sh";
  char opt_default[] = "  console";
  char decl2[] = "service console2 /bin/sh";
  char opt_device[] = "  console /dev/ttyACM0";
  char decl3[] = "service plain /bin/sh";
  FAR struct service_s *s;

  service_manager_init(&sm);

  /* "console" without an argument: flagged, default device. */

  assert_int_equal(init_service_parse(&parser, true, decl1), 0);
  assert_int_equal(init_service_parse(&parser, false, opt_default), 0);

  s = list_last_entry(&sm.services, struct service_s, node);
  assert_int_equal(s->flags & SVC_CONSOLE, SVC_CONSOLE);
  assert_null(s->console);

  /* "console <device>": flagged, device duplicated (not aliased into the
   * caller's line buffer, which is reused for the next line).
   */

  assert_int_equal(init_service_parse(&parser, true, decl2), 0);
  assert_int_equal(init_service_parse(&parser, false, opt_device), 0);

  s = list_last_entry(&sm.services, struct service_s, node);
  assert_int_equal(s->flags & SVC_CONSOLE, SVC_CONSOLE);
  assert_non_null(s->console);
  assert_string_equal(s->console, "/dev/ttyACM0");
  assert_ptr_not_equal(s->console, opt_device + 10);

  /* A service without the option keeps its stdio untouched. */

  assert_int_equal(init_service_parse(&parser, true, decl3), 0);

  s = list_last_entry(&sm.services, struct service_s, node);
  assert_int_equal(s->flags & SVC_CONSOLE, 0);
  assert_null(s->console);

  assert_int_equal(init_service_check(&parser), 0);

  service_manager_free_all(&sm);
}

/****************************************************************************
 * Name: test_nxinit_service_stop_restarting
 *
 * Description:
 *   A service waiting to restart has exited: stopping it cancels the
 *   restart and signals nothing, as the pid it had is stale (0, the idle
 *   task, if its spawn failed).
 ****************************************************************************/

void test_nxinit_service_stop_restarting(FAR void **state)
{
  struct service_manager_s sm;
  struct parser_s parser =
    {
      "service", init_service_parse, init_service_check, &sm
    };

  char decl[] = "service foo /bin/foo";
  FAR struct service_s *s;
  pid_t pid;

  service_manager_init(&sm);

  assert_int_equal(init_service_parse(&parser, true, decl), 0);
  assert_int_equal(init_service_check(&parser), 0);

  /* A pid no task has: signalling it would fail with ESRCH. */

  for (pid = 30000; kill(pid, 0) == 0; pid++)
    {
    }

  s = list_last_entry(&sm.services, struct service_s, node);
  s->flags = SVC_RESTARTING;
  s->pid   = pid;

  assert_int_equal(init_service_stop(s), 0);
  assert_int_equal(s->flags & (SVC_DISABLED | SVC_RESTARTING | SVC_RUNNING),
                   SVC_DISABLED);

  service_manager_free_all(&sm);
}

/****************************************************************************
 * Name: test_nxinit_service_find_by_pid_running
 *
 * Description:
 *   A pid names the running service that has it, not one that had it
 *   before it exited.
 ****************************************************************************/

void test_nxinit_service_find_by_pid_running(FAR void **state)
{
  struct service_manager_s sm;
  struct parser_s parser =
    {
      "service", init_service_parse, init_service_check, &sm
    };

  char decl1[] = "service old /bin/old";
  char decl2[] = "service new /bin/new";
  FAR struct service_s *s1;
  FAR struct service_s *s2;

  service_manager_init(&sm);

  assert_int_equal(init_service_parse(&parser, true, decl1), 0);
  assert_int_equal(init_service_parse(&parser, true, decl2), 0);
  assert_int_equal(init_service_check(&parser), 0);

  s1 = list_first_entry(&sm.services, struct service_s, node);
  s2 = list_last_entry(&sm.services, struct service_s, node);

  s1->flags = SVC_RESTARTING;
  s1->pid   = 7;
  s2->flags = SVC_RUNNING;
  s2->pid   = 7;

  assert_ptr_equal(init_service_find_by_pid(&sm, 7), s2);

  s2->flags = SVC_RESTARTING;
  assert_null(init_service_find_by_pid(&sm, 7));

  service_manager_free_all(&sm);
}

/****************************************************************************
 * Name: test_nxinit_service_norestart
 *
 * Description:
 *   A "norestart" service that exits, cleanly or not, is stopped and kept:
 *   neither restarted nor removed, and it can be started again.  One
 *   without the option is restarted.
 ****************************************************************************/

void test_nxinit_service_norestart(FAR void **state)
{
  struct service_manager_s sm;
  struct parser_s parser =
    {
      "service", init_service_parse, init_service_check, &sm
    };

  char decl1[] = "service app /bin/app";
  char opt[] = "  norestart";
  char decl2[] = "service plain /bin/plain";
  FAR struct service_s *app;
  FAR struct service_s *plain;
  int status;

  service_manager_init(&sm);

  assert_int_equal(init_service_parse(&parser, true, decl1), 0);
  assert_int_equal(init_service_parse(&parser, false, opt), 0);
  assert_int_equal(init_service_parse(&parser, true, decl2), 0);
  assert_int_equal(init_service_check(&parser), 0);

  app   = init_service_find_by_name(&sm, "app");
  plain = init_service_find_by_name(&sm, "plain");
  assert_non_null(app);
  assert_non_null(plain);

  for (status = 0; status < 2; status++)
    {
      app->flags |= SVC_RUNNING;
      app->pid    = 7;

      init_service_reap(app, status);
      assert_int_equal(app->flags & (SVC_RUNNING | SVC_RESTARTING |
                                     SVC_DISABLED | SVC_REMOVE),
                       SVC_DISABLED);
      assert_string_equal(init_service_state(app), "stopped");

      init_service_refresh(&sm);
      assert_ptr_equal(init_service_find_by_name(&sm, "app"), app);
    }

#ifdef CONFIG_EXAMPLES_HELLO
  /* Started again by name: a program that runs, and exits at once */

  free(app->argv[2]);
  app->argv[2] = strdup("hello");
  assert_non_null(app->argv[2]);

  status = init_service_start(app);
  assert_true(status > 0);
  assert_int_equal(app->flags & (SVC_RUNNING | SVC_DISABLED), SVC_RUNNING);

  assert_int_equal(waitpid(status, NULL, 0), status);
  init_service_reap(app, 0);
  assert_int_equal(app->flags & (SVC_RUNNING | SVC_RESTARTING |
                                 SVC_DISABLED | SVC_REMOVE),
                   SVC_DISABLED);
#endif

  plain->flags |= SVC_RUNNING;
  plain->pid    = 8;

  init_service_reap(plain, 0);
  assert_int_equal(plain->flags & (SVC_RESTARTING | SVC_DISABLED),
                   SVC_RESTARTING);

  service_manager_free_all(&sm);
}

/****************************************************************************
 * Name: test_nxinit_service_name_max
 *
 * Description:
 *   A service's name, and a class's, may be NXINIT_SERVICE_NAME_MAX - 1
 *   bytes long, kept whole; a longer one is refused.
 ****************************************************************************/

void test_nxinit_service_name_max(FAR void **state)
{
  struct service_manager_s sm;
  struct parser_s parser =
    {
      "service", init_service_parse, init_service_check, &sm
    };

  char name[NXINIT_SERVICE_NAME_MAX + 1];
  char line[NXINIT_SERVICE_NAME_MAX + 32];
  FAR struct service_class_s *c;
  FAR struct service_s *s;

  service_manager_init(&sm);

  /* The longest name */

  memset(name, 'a', sizeof(name));
  name[NXINIT_SERVICE_NAME_MAX - 1] = '\0';

  snprintf(line, sizeof(line), "service %s /bin/a", name);
  assert_int_equal(init_service_parse(&parser, true, line), 0);
  s = init_service_find_by_name(&sm, name);
  assert_non_null(s);
  assert_string_equal(s->argv[1], name);

  snprintf(line, sizeof(line), "  class %s", name);
  assert_int_equal(init_service_parse(&parser, false, line), 0);
  c = list_first_entry(&s->classes, struct service_class_s, node);
  assert_string_equal(c->name, name);

  /* One more byte: a class, then a service */

  name[NXINIT_SERVICE_NAME_MAX - 1] = 'b';
  name[NXINIT_SERVICE_NAME_MAX]     = '\0';

  snprintf(line, sizeof(line), "  class %s", name);
  assert_int_equal(init_service_parse(&parser, false, line), -ENAMETOOLONG);
  assert_ptr_equal(list_last_entry(&s->classes, struct service_class_s,
                                   node), c);

  snprintf(line, sizeof(line), "service %s /bin/b", name);
  assert_int_equal(init_service_parse(&parser, true, line), -ENAMETOOLONG);
  assert_ptr_equal(list_last_entry(&sm.services, struct service_s, node),
                   s);

  assert_int_equal(init_service_check(&parser), 0);

  service_manager_free_all(&sm);
}

