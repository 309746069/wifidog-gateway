/********************************************************************\
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
 \********************************************************************/

/*
 * $Header$
 */
/**
  @file util.c
  @brief Misc utility functions
  @author Copyright (C) 2004 Philippe April <papril777@yahoo.com>
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <string.h>
#include <pthread.h>
#include <netdb.h>

#include "util.h"
#include "conf.h"
#include "debug.h"

static pthread_mutex_t ghbn_mutex = PTHREAD_MUTEX_INITIALIZER;

/* XXX Do these need to be locked ? */
static time_t last_online_time = 0;
static time_t last_offline_time = 0;

/** Fork a child and execute a shell command, the parent
 * process waits for the child to return and returns the child's exit()
 * value.
 * @return Return code of the command
 */
int
execute(char *cmd_line, int quiet)
{
    int pid,
        status,
        rc;

    const char *new_argv[4];
    new_argv[0] = "/bin/sh";
    new_argv[1] = "-c";
    new_argv[2] = cmd_line;
    new_argv[3] = NULL;

    if ((pid = fork()) < 0) {    /* fork a child process           */
        debug(LOG_ERR, "fork(): %s", strerror(errno));
        exit(1);
    } else if (pid == 0) {    /* for the child process:         */
        /* We don't want to see any errors if quiet flag is on */
        if (quiet) close(2);
        if (execvp("/bin/sh", (char *const *)new_argv) < 0) {    /* execute the command  */
            debug(LOG_ERR, "execvp(): %s", strerror(errno));
            exit(1);
        }
    } else {        /* for the parent:      */
        do {
            rc = wait(&status);
        } while (rc != pid && rc != -1);    /* wait for completion  */
    }

    return (WEXITSTATUS(status));
}

struct in_addr *
wd_gethostbyname(const char *name)
{
	struct hostent *he;
	struct in_addr *h_addr, *in_addr_temp;

	/* XXX Calling function is reponsible for free() */

	h_addr = (struct in_addr *)malloc(sizeof(struct in_addr));
	
	if (h_addr == NULL)
		return NULL;
	
	LOCK_GHBN();

	he = gethostbyname(name);

	if (he == NULL) {
		free(h_addr);
		mark_offline();
		UNLOCK_GHBN();
		return NULL;
	}

	mark_online();

	in_addr_temp = (struct in_addr *)he->h_addr_list[0];
	h_addr->s_addr = in_addr_temp->s_addr;
	
	UNLOCK_GHBN();

	return h_addr;
}

char *get_iface_ip(char *ifname) {
    struct ifreq if_data;
    struct in_addr in;
    char *ip_str;
    int sockd;
    u_int32_t ip;

    /* Create a socket */
    if ((sockd = socket (AF_INET, SOCK_PACKET, htons(0x8086))) < 0) {
        debug(LOG_ERR, "socket(): %s", strerror(errno));
        return NULL;
    }

    /* Get IP of internal interface */
    strcpy (if_data.ifr_name, ifname);

    /* Get the IP address */
    if (ioctl (sockd, SIOCGIFADDR, &if_data) < 0) {
        debug(LOG_ERR, "ioctl(): SIOCGIFADDR %s", strerror(errno));
        return NULL;
    }
    memcpy ((void *) &ip, (void *) &if_data.ifr_addr.sa_data + 2, 4);
    in.s_addr = ip;

    ip_str = (char *)inet_ntoa(in);
    return strdup(ip_str);
}

void mark_online() {
	time(&last_online_time);
}

void mark_offline() {
	time(&last_offline_time);
}

int is_online() {
	if (last_online_time == 0 || (last_offline_time - last_online_time) >= (config_get_config()->checkinterval * 2) ) {
		/* We're probably offline */
		return (0);
	}
	else {
		/* We're probably online */
		return (1);
	}
}

