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

/* $Header$ */
/** @internal
  @file fw_iptables.c
  @brief Firewall iptables functions
  @author Copyright (C) 2004 Philippe April <papril777@yahoo.com>
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"

#include "conf.h"
#include "fw_iptables.h"
#include "firewall.h"
#include "debug.h"
#include "util.h"
#include "client_list.h"

static int iptables_do_command(char *format, ...);
static char *iptables_compile(char *, t_firewall_rule *);
static void iptables_load_ruleset(char *, char *);

extern pthread_mutex_t	client_list_mutex;
extern pthread_mutex_t	config_mutex;

/**
Used to supress the error output of the firewall during destruction */ 
static int fw_quiet = 0;

/** @internal */
static int
iptables_do_command(char *format, ...)
{
    va_list vlist;
    char *fmt_cmd,
        *cmd;
    int rc;

    va_start(vlist, format);
    vasprintf(&fmt_cmd, format, vlist);
    asprintf(&cmd, "iptables %s", fmt_cmd);
    debug(LOG_DEBUG, "Executing command: %s", cmd);
	
    rc = execute(cmd, fw_quiet);

    free(fmt_cmd);
    free(cmd);

    return rc;
}

/**
 * @internal
 * Compiles a struct definition of a firewall rule into a valid iptables
 * command.
 * @arg chain Chain that the command will be (-A)ppended to.
 * @arg rule Definition of a rule into a struct, from conf.c.
 */
static char *
iptables_compile(char *chain, t_firewall_rule *rule)
{
    char	command[MAX_BUF],
    		*mode;
    
    memset(command, 0, MAX_BUF);
    
    if (rule->block_allow == 1) {
        mode = strdup("ACCEPT");
    } else {
        mode = strdup("REJECT");
    }
    
    snprintf(command, sizeof(command),  "-t filter -A %s ", chain);
    if (rule->mask != NULL) {
        snprintf((command + strlen(command)), (sizeof(command) - 
                strlen(command)), "-d %s ", rule->mask);
    }
    if (rule->protocol != NULL) {
        snprintf((command + strlen(command)), (sizeof(command) -
                strlen(command)), "-p %s ", rule->protocol);
    }
    if (rule->port != NULL) {
        snprintf((command + strlen(command)), (sizeof(command) -
                strlen(command)), "--dport %s ", rule->port);
    }
    snprintf((command + strlen(command)), (sizeof(command) - 
            strlen(command)), "-j %s", mode);
    
    free(mode);

    /* XXX The buffer command, an automatic variable, will get cleaned
     * off of the stack when we return, so we strdup() it. */
    return(strdup(command));
}

/**
 * @internal
 * Load all the rules in a rule set.
 * @arg ruleset Name of the ruleset
 * @arg chain IPTables chain the rules go into
 */
static void
iptables_load_ruleset(char *ruleset, char *chain)
{
	t_firewall_rule		*rules;
	char			*cmd;

	debug(LOG_DEBUG, "Load ruleset %s into chain %s", ruleset, chain);
	
	for (rules = get_ruleset(ruleset); rules != NULL; rules = rules->next) {
		cmd = iptables_compile(chain, rules);
		debug(LOG_DEBUG, "Loading rule \"%s\" into %s", cmd, chain);
		iptables_do_command(cmd);
		free(cmd);
	}

	debug(LOG_DEBUG, "Ruleset %s loaded into %s", ruleset, chain);
}

void
iptables_fw_clear_authservers(void)
{
    iptables_do_command("-t filter -F " TABLE_WIFIDOG_AUTHSERVERS);
}

void
iptables_fw_set_authservers(void)
{
    s_config *config;
    t_auth_serv *auth_server;
   
    config = config_get_config();
    
    LOCK_CONFIG();
    
    for (auth_server = config->auth_servers; auth_server != NULL; auth_server = auth_server->next) {
	    if (auth_server->last_ip == NULL || strcmp(auth_server->last_ip, "0.0.0.0") == 0) {
	        iptables_do_command("-t filter -A " TABLE_WIFIDOG_AUTHSERVERS " -d %s -j ACCEPT", auth_server->authserv_hostname);
	    } else {
	        iptables_do_command("-t filter -A " TABLE_WIFIDOG_AUTHSERVERS " -d %s -j ACCEPT", auth_server->last_ip);
	    }
    }

    UNLOCK_CONFIG();
}

/** Initialize the firewall rules
 */
int
iptables_fw_init(void)
{
    s_config *config;
	 char * gw_interface = NULL;
	 char * external_interface = NULL;
	 int gw_port = 0;
   
    fw_quiet = 0;

    config = config_get_config();
	 LOCK_CONFIG();
	 if (config->gw_interface)
		 gw_interface = strdup(config->gw_interface);
	 if (config->external_interface)
		 external_interface = strdup(config->external_interface);
	 gw_port = config->gw_port;
	 UNLOCK_CONFIG();
    
	 /*
	  *
	  * Everything in the MANGLE table
	  *
	  */

	 		/* Create new chains */
			iptables_do_command("-t mangle -N " TABLE_WIFIDOG_OUTGOING);
			iptables_do_command("-t mangle -N " TABLE_WIFIDOG_INCOMING);

			/* Assign links and rules to these new chains */
			iptables_do_command("-t mangle -I PREROUTING 1 -i %s -j " TABLE_WIFIDOG_OUTGOING, gw_interface);

			if (external_interface) {
				iptables_do_command("-t mangle -I FORWARD 1 -i %s -j " TABLE_WIFIDOG_INCOMING, external_interface);
			}
			else {
				iptables_do_command("-t mangle -I FORWARD 1 -j " TABLE_WIFIDOG_INCOMING);
			}


	 /*
	  *
	  * Everything in the NAT table
	  *
	  */

	 		/* Create new chains */
			iptables_do_command("-t nat -N " TABLE_WIFIDOG_WIFI_TO_INTERNET);
			iptables_do_command("-t nat -N " TABLE_WIFIDOG_UNKNOWN);

			/* Assign links and rules to these new chains */
			if (external_interface) {
				iptables_do_command("-t nat -I PREROUTING 1 -i %s -j " TABLE_WIFIDOG_WIFI_TO_INTERNET, gw_interface);
			}
			else {
				iptables_do_command("-t nat -I PREROUTING 1 -j " TABLE_WIFIDOG_WIFI_TO_INTERNET);
			}

			iptables_do_command("-t nat -A " TABLE_WIFIDOG_WIFI_TO_INTERNET " -m mark --mark 0x%u -j RETURN", FW_MARK_KNOWN);
			iptables_do_command("-t nat -A " TABLE_WIFIDOG_WIFI_TO_INTERNET " -m mark --mark 0x%u -j RETURN", FW_MARK_PROBATION);
			iptables_do_command("-t nat -A " TABLE_WIFIDOG_WIFI_TO_INTERNET " -j " TABLE_WIFIDOG_UNKNOWN);
			iptables_do_command("-t nat -A " TABLE_WIFIDOG_UNKNOWN " -p tcp --dport 80 -j REDIRECT --to-ports %d", gw_port);


	 /*
	  *
	  * Everything in the FILTER table
	  *
	  */

	 		/* Create new chains */
			iptables_do_command("-t filter -N " TABLE_WIFIDOG_WIFI_TO_GW);
			iptables_do_command("-t filter -N " TABLE_WIFIDOG_WIFI_TO_INTERNET);
			iptables_do_command("-t filter -N " TABLE_WIFIDOG_AUTHSERVERS);
			iptables_do_command("-t filter -N " TABLE_WIFIDOG_LOCKED);
			iptables_do_command("-t filter -N " TABLE_WIFIDOG_GLOBAL);
			iptables_do_command("-t filter -N " TABLE_WIFIDOG_VALIDATE);
			iptables_do_command("-t filter -N " TABLE_WIFIDOG_KNOWN);
			iptables_do_command("-t filter -N " TABLE_WIFIDOG_UNKNOWN);

			/* Assign links and rules to these new chains */
			iptables_do_command("-t filter -I INPUT 1 -i %s -j " TABLE_WIFIDOG_WIFI_TO_GW, gw_interface);

			if (external_interface) {
				iptables_do_command("-t filter -I FORWARD 1 -i %s -j " TABLE_WIFIDOG_WIFI_TO_INTERNET, gw_interface);
			}
			else {
				iptables_do_command("-t filter -I FORWARD 1 -j " TABLE_WIFIDOG_WIFI_TO_INTERNET);
			}

			iptables_do_command("-t filter -A " TABLE_WIFIDOG_WIFI_TO_INTERNET " -j " TABLE_WIFIDOG_AUTHSERVERS);
			iptables_fw_set_authservers();

			iptables_do_command("-t filter -A " TABLE_WIFIDOG_WIFI_TO_INTERNET " -m mark --mark 0x%u -j " TABLE_WIFIDOG_LOCKED, FW_MARK_LOCKED);
			iptables_load_ruleset("locked-users", TABLE_WIFIDOG_LOCKED);

			iptables_do_command("-t filter -A " TABLE_WIFIDOG_WIFI_TO_INTERNET " -j " TABLE_WIFIDOG_GLOBAL);
			iptables_load_ruleset("global", TABLE_WIFIDOG_GLOBAL);

			iptables_do_command("-t filter -A " TABLE_WIFIDOG_WIFI_TO_INTERNET " -m mark --mark 0x%u -j " TABLE_WIFIDOG_VALIDATE, FW_MARK_PROBATION);
			iptables_load_ruleset("validating-users", TABLE_WIFIDOG_VALIDATE);

			iptables_do_command("-t filter -A " TABLE_WIFIDOG_WIFI_TO_INTERNET " -m mark --mark 0x%u -j " TABLE_WIFIDOG_KNOWN, FW_MARK_KNOWN);
			iptables_load_ruleset("known-users", TABLE_WIFIDOG_KNOWN);
    
			iptables_do_command("-t filter -A " TABLE_WIFIDOG_WIFI_TO_INTERNET " -j " TABLE_WIFIDOG_UNKNOWN);
			iptables_load_ruleset("unknown-users", TABLE_WIFIDOG_UNKNOWN);
			iptables_do_command("-t filter -A " TABLE_WIFIDOG_UNKNOWN " -j REJECT --reject-with icmp-port-unreachable");

	if (gw_interface)
		free(gw_interface);
	if (external_interface)
		free(external_interface);

    return 1;
}

/** Remove the firewall rules
 * This is used when we do a clean shutdown of WiFiDog and when it starts to make
 * sure there are no rules left over
 */
int
iptables_fw_destroy(void)
{
    fw_quiet = 1;

	 /*
	  *
	  * Everything in the MANGLE table
	  *
	  */
	 iptables_fw_destroy_mention("mangle", "PREROUTING", TABLE_WIFIDOG_OUTGOING);
	 iptables_fw_destroy_mention("mangle", "FORWARD", TABLE_WIFIDOG_INCOMING);
    iptables_do_command("-t mangle -F " TABLE_WIFIDOG_OUTGOING);
    iptables_do_command("-t mangle -F " TABLE_WIFIDOG_INCOMING);
    iptables_do_command("-t mangle -X " TABLE_WIFIDOG_OUTGOING);
    iptables_do_command("-t mangle -X " TABLE_WIFIDOG_INCOMING);

	 /*
	  *
	  * Everything in the NAT table
	  *
	  */
	 iptables_fw_destroy_mention("nat", "PREROUTING", TABLE_WIFIDOG_WIFI_TO_INTERNET);
    iptables_do_command("-t nat -F " TABLE_WIFIDOG_WIFI_TO_INTERNET);
    iptables_do_command("-t nat -F " TABLE_WIFIDOG_UNKNOWN);
    iptables_do_command("-t nat -X " TABLE_WIFIDOG_WIFI_TO_INTERNET);
    iptables_do_command("-t nat -X " TABLE_WIFIDOG_UNKNOWN);

	 /*
	  *
	  * Everything in the FILTER table
	  *
	  */
	 iptables_fw_destroy_mention("filter", "INPUT", TABLE_WIFIDOG_WIFI_TO_GW);
	 iptables_fw_destroy_mention("filter", "FORWARD", TABLE_WIFIDOG_WIFI_TO_INTERNET);
	 iptables_do_command("-t filter -F " TABLE_WIFIDOG_WIFI_TO_GW);
	 iptables_do_command("-t filter -F " TABLE_WIFIDOG_WIFI_TO_INTERNET);
	 iptables_do_command("-t filter -F " TABLE_WIFIDOG_AUTHSERVERS);
	 iptables_do_command("-t filter -F " TABLE_WIFIDOG_LOCKED);
	 iptables_do_command("-t filter -F " TABLE_WIFIDOG_GLOBAL);
	 iptables_do_command("-t filter -F " TABLE_WIFIDOG_VALIDATE);
	 iptables_do_command("-t filter -F " TABLE_WIFIDOG_KNOWN);
	 iptables_do_command("-t filter -F " TABLE_WIFIDOG_UNKNOWN);
	 iptables_do_command("-t filter -X " TABLE_WIFIDOG_WIFI_TO_GW);
	 iptables_do_command("-t filter -X " TABLE_WIFIDOG_WIFI_TO_INTERNET);
	 iptables_do_command("-t filter -X " TABLE_WIFIDOG_AUTHSERVERS);
	 iptables_do_command("-t filter -X " TABLE_WIFIDOG_LOCKED);
	 iptables_do_command("-t filter -X " TABLE_WIFIDOG_GLOBAL);
	 iptables_do_command("-t filter -X " TABLE_WIFIDOG_VALIDATE);
	 iptables_do_command("-t filter -X " TABLE_WIFIDOG_KNOWN);
	 iptables_do_command("-t filter -X " TABLE_WIFIDOG_UNKNOWN);

    return 1;
}

/*
 * Helper for iptables_fw_destroy
 * @param table The table to search
 * @param chain The chain in that table to search
 * @param mention A word to find and delete in rules in the given table+chain
 */
int
iptables_fw_destroy_mention(
		char * table,
		char * chain,
		char * mention
) {
	FILE *p = NULL;
	char *command = NULL;
	char *command2 = NULL;
	char line[MAX_BUF];
	char rulenum[10];
	int deleted = 0;

	asprintf(&command, "iptables -t %s -L %s -n --line-numbers -v", table, chain);

	if ((p = popen(command, "r"))) {
		/* Skip first 2 lines */
		while (!feof(p) && fgetc(p) != '\n');
		while (!feof(p) && fgetc(p) != '\n');
		/* Loop over entries */
		while (fgets(line, sizeof(line), p)) {
			/* Look for mention */
			if (strstr(line, mention)) {
				/* Found mention - Get the rule number into rulenum*/
				if (sscanf(line, "%9[0-9]", rulenum) == 1) {
					/* Delete the rule: */
					asprintf(&command2, "-t %s -D %s %s", table, chain, rulenum);
					iptables_do_command(command2);
					free(command2);
					deleted = 1;
					/* Do not keep looping - the captures rulenums will no longer be accurate */
					break;
				}
			}
		}
		pclose(p);
	}

	free(command);

	if (deleted) {
		/* Recurse just in case there are more in the same table+chain */
		iptables_fw_destroy_mention(table, chain, mention);
	}

	return (deleted);
}

/** Set the firewall access for a specific client */
int
iptables_fw_access(fw_access_t type, char *ip, char *mac, int tag)
{
    int rc;

    fw_quiet = 0;

    switch(type) {
        case FW_ACCESS_ALLOW:
            iptables_do_command("-t filter -A " TABLE_WIFIDOG_WIFI_TO_GW " -s %s -j ACCEPT", ip);
            iptables_do_command("-t mangle -A " TABLE_WIFIDOG_OUTGOING " -s %s -m mac --mac-source %s -j MARK --set-mark %d", ip, mac, tag);
            rc = iptables_do_command("-t mangle -A " TABLE_WIFIDOG_INCOMING " -d %s -j ACCEPT", ip);
            break;
        case FW_ACCESS_DENY:
            iptables_do_command("-t filter -D " TABLE_WIFIDOG_WIFI_TO_GW " -s %s -j ACCEPT", ip);
            iptables_do_command("-t mangle -D " TABLE_WIFIDOG_OUTGOING " -s %s -m mac --mac-source %s -j MARK --set-mark %d", ip, mac, tag);
            rc = iptables_do_command("-t mangle -D " TABLE_WIFIDOG_INCOMING " -d %s -j ACCEPT", ip);
            break;
        default:
            rc = -1;
            break;
    }

    return rc;
}

/** Update the counters of all the clients in the client list */
int
iptables_fw_counters_update(void)
{
    FILE *output;
    char *script,
        ip[16],
        rc;
    unsigned long int counter;
    t_client *p1;
	 struct in_addr tempaddr;

    /* Look for outgoing traffic */
    asprintf(&script, "%s %s", "iptables", "-v -n -x -t mangle -L " TABLE_WIFIDOG_OUTGOING);
    if (!(output = popen(script, "r"))) {
        debug(LOG_ERR, "popen(): %s", strerror(errno));
        return -1;
    }
    free(script);

    /* skip the first two lines */
    while (('\n' != fgetc(output)) && !feof(output))
        ;
    while (('\n' != fgetc(output)) && !feof(output))
        ;
    while (output && !(feof(output))) {
        rc = fscanf(output, "%*s %lu %*s %*s %*s %*s %*s %15[0-9.] %*s %*s %*s %*s %*s 0x%*u", &counter, ip);
        if (2 == rc && EOF != rc) {
			  /* Sanity*/
			  if (!inet_aton(ip, &tempaddr)) {
				  debug(LOG_WARNING, "I was supposed to read an IP address but instead got [%s] - ignoring it", ip);
				  continue;
			  }
            debug(LOG_DEBUG, "Outgoing %s Bytes=%ld", ip, counter);
	    LOCK_CLIENT_LIST();
            if ((p1 = client_list_find_by_ip(ip))) {
                if (p1->counters.outgoing < counter) {
                    p1->counters.outgoing = counter;
                    p1->counters.last_updated = time(NULL);
                    debug(LOG_DEBUG, "%s - Updated outgoing counter to %ld bytes from outgoing chain", ip, counter);
                }
            } else {
                debug(LOG_ERR, "Could not find %s in client list", ip);
            }
	    UNLOCK_CLIENT_LIST();
        }
    }
    pclose(output);

    /* Look for wifi-to-firewall traffic */
    asprintf(&script, "%s %s", "iptables", "-v -n -x -t filter -L " TABLE_WIFIDOG_WIFI_TO_GW);
    if (!(output = popen(script, "r"))) {
        debug(LOG_ERR, "popen(): %s", strerror(errno));
        return -1;
    }
    free(script);

    /* skip the first two lines */
    while (('\n' != fgetc(output)) && !feof(output))
        ;
    while (('\n' != fgetc(output)) && !feof(output))
        ;
    while (output && !(feof(output))) {
        rc = fscanf(output, "%*s %lu %*s %*s %*s %*s %*s %15[0-9.] %*s", &counter, ip);
        if (2 == rc && EOF != rc) {
			  /* Sanity*/
			  if (!inet_aton(ip, &tempaddr)) {
				  debug(LOG_WARNING, "I was supposed to read an IP address but instead got [%s] - ignoring it", ip);
				  continue;
			  }
            debug(LOG_DEBUG, "WIFI2FW %s Bytes=%ld", ip, counter);
	    LOCK_CLIENT_LIST();
            if ((p1 = client_list_find_by_ip(ip))) {
                if (p1->counters.togateway < counter) {
                    p1->counters.togateway = counter;
                    p1->counters.last_updated = time(NULL);
                    debug(LOG_DEBUG, "%s - Updated togateway counter to %ld bytes from wifi2fw chain", ip, counter);
                }
            } else {
                debug(LOG_ERR, "Could not find %s in client list", ip);
            }
	    UNLOCK_CLIENT_LIST();
        }
    }
    pclose(output);

    /* Look for incoming traffic */
    asprintf(&script, "%s %s", "iptables", "-v -n -x -t mangle -L " TABLE_WIFIDOG_INCOMING);
    if (!(output = popen(script, "r"))) {
        debug(LOG_ERR, "popen(): %s", strerror(errno));
        return -1;
    }
    free(script);

    /* skip the first two lines */
    while (('\n' != fgetc(output)) && !feof(output))
        ;
    while (('\n' != fgetc(output)) && !feof(output))
        ;
    while (output && !(feof(output))) {
        rc = fscanf(output, "%*s %lu %*s %*s %*s %*s %*s %*s %15[0-9.]", &counter, ip);
        if (2 == rc && EOF != rc) {
			  /* Sanity*/
			  if (!inet_aton(ip, &tempaddr)) {
				  debug(LOG_WARNING, "I was supposed to read an IP address but instead got [%s] - ignoring it", ip);
				  continue;
			  }
            debug(LOG_DEBUG, "Incoming %s Bytes=%ld", ip, counter);
	    LOCK_CLIENT_LIST();
            if ((p1 = client_list_find_by_ip(ip))) {
                if (p1->counters.incoming < counter) {
                    p1->counters.incoming = counter;
                    p1->counters.last_updated = time(NULL);
                    debug(LOG_DEBUG, "%s - Updated counter to %ld bytes", ip, counter);
                }
            } else {
                debug(LOG_ERR, "Could not find %s in client list", ip);
            }
	    UNLOCK_CLIENT_LIST();
        }
    }
    pclose(output);

    return 1;
}
