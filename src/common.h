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
/** @file common.h
    @brief Common constants and other bits
    @author Copyright (C) 2004 Philippe April <papril777@yahoo.com>
    @todo This file is evil, it should only contain constants, not includes.  Makes it very hard to find out what other files a code section uses.
*/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#include <pthread.h>

#include "config.h"

#include "httpd.h"

#include "gateway.h"
#include "conf.h"
#include "commandline.h"
#include "debug.h"
#include "userclasses.h"
#include "firewall.h"
#include "http.h"
#include "centralserver.h"
#include "auth.h"

#define MAX_BUF 4096

#define SCRIPT_FWINIT       "fw.init"
#define SCRIPT_FWACCESS     "fw.access"
#define SCRIPT_FWDESTROY    "fw.destroy"
#define SCRIPT_FWCOUNTERS   "fw.counters"

#endif /* _COMMON_H_ */
