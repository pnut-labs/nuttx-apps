/****************************************************************************
 * apps/include/netutils/pppd.h
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

#ifndef __APPS_INCLUDE_NETUTILS_PPPD_H
#define __APPS_INCLUDE_NETUTILS_PPPD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <time.h>

#include <net/if.h>
#include <netinet/in.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/* Required configuration settings:
 *
 *   CONFIG_NETUTILS_PPPD_PAP - PPPD PAP authentication support
 *     Default: n
 */

#define TTYNAMSIZ               16
#define PAP_USERNAME_SIZE       16
#define PAP_PASSWORD_SIZE       16

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* What pppd() tells its caller about the link (pppd_settings_s.notify) */

enum pppd_event_e
{
  PPPD_LINK_UP,                    /* IPCP is done: the interface is up */
  PPPD_LINK_DOWN                   /* The link is down: lost, renegotiated
                                    * by the peer, or stopped.  Unless
                                    * stopped, pppd() connects again, and
                                    * dials only with a connect script */
};

/* The link's addresses, as IPCP gave them; a name server the peer did not
 * give is 0.0.0.0
 */

struct pppd_link_s
{
  char ifname[IFNAMSIZ];           /* The interface, as "ppp0" */
  struct in_addr local;            /* Its address */
  struct in_addr peer;             /* The peer's */
  struct in_addr dns[2];           /* The name servers */
};

struct pppd_settings_s
{
  /* Serial Interface */

  char ttyname[TTYNAMSIZ];

#ifdef CONFIG_NETUTILS_PPPD_PAP
  /* PAP Authentication Settings */

  char pap_username[PAP_USERNAME_SIZE];
  char pap_password[PAP_PASSWORD_SIZE];
#endif /* CONFIG_NETUTILS_PPPD_PAP */

  /* Chat Scripts.  With no connect script, pppd() neither hangs the line
   * up nor dials it when it connects (only a stop ends the link with a
   * Terminate-Request): the caller has connected the line, and connects
   * it again once told the link is down (notify)
   */

  FAR const char *connect_script;
  FAR const char *disconnect_script;

  /* If true, chat does not copy the scripts and the modem's answers to
   * stderr: a console that no one reads blocks the write, and pppd with it
   */

  bool quiet;

  /* If not NULL: once *stop is true, pppd() ends the link, takes the
   * interface down, closes the tty and returns 0.  It looks at least once
   * a second, and at the end of each chat script.
   */

  FAR volatile bool *stop;

  /* If not NULL: called on pppd()'s thread when the link comes up (with its
   * addresses) and when it goes down (link NULL; enum pppd_event_e says
   * when).  link is valid only during the call: copy what is needed.
   * pppd() then leaves the name servers and the default route to the
   * caller, instead of giving them to the resolver and the routing table.
   */

  CODE void (*notify)(FAR void *arg, enum pppd_event_e event,
                      FAR const struct pppd_link_s *link);
  FAR void *arg;
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Functions Definitions
 ****************************************************************************/

/****************************************************************************
 * Name: pppd
 *
 * Description:
 *   Create an pppd connection
 *
 * Input Parameters:
 *    pppd_settings, setting struct for the ppp connection
 *
 * Returned Value:
 *   Returns termination state, blocking as long as the connection is up;
 *   0 once stopped through pppd_settings->stop
 *
 ****************************************************************************/

int pppd(const struct pppd_settings_s *ppp_settings);

#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif /* __APPS_INCLUDE_NETUTILS_PPPD_H */
