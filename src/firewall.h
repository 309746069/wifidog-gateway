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
/** @file firewall.h
    @brief Firewall update functions
    @author Copyright (C) 2004 Philippe April <papril777@yahoo.com>
*/

#ifndef _FIREWALL_H_
#define _FIREWALL_H_

typedef struct {
    void *next;
    char ip[16];
    char mac[18];
    char token[33];
    int active; /* boolean */
    long int counter;
} t_node;

int fw_init(void);
int fw_destroy(void);
int fw_allow(char *ip, char *mac, int profile);
int fw_deny(char *ip, char *mac, int profile);
void fw_counter(void);
int execute(char **argv);
char *arp_get(char *req_ip);

void node_init(void);
t_node *node_add(char *ip, char *mac, char *token, long int counter,
		 int active);
t_node *node_find_by_ip(char *ip);
t_node *node_find_by_token(char *token);
void node_delete(t_node *node);
void free_node(t_node *node);

#endif /* _FIREWALL_H_ */
