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
    @file auth.h
    @brief Authentication handling thread
    @author Copyright (C) 2004 Alexandre Carmel-Veilleux <acv@acv.ca>
*/

#ifndef _AUTH_H_
#define _AUTH_H_

/**
 * @brief Authentication codes returned by auth server.
 *
 * Authentication result codes returned by auth_server_request() corresponding
 * to result code from the central server itself.
 */
typedef enum {
    AUTH_ERROR = -1,
    AUTH_DENIED = 0,
    AUTH_ALLOWED = 1,
    AUTH_VALIDATION = 5,
    AUTH_VALIDATION_FAILED = 6,
    AUTH_LOCKED = 254
} t_authcode;

/**
 * Structure returned by auth_server_request()
 */
typedef struct _t_authresponse {
    int authcode;
} t_authresponse;

void thread_authenticate_client(void *arg);
void thread_client_timeout_check(void *arg);

#endif
